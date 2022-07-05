#include "StyleTransferSpartialUpscaler.h"
#include "RHIStaticStates.h"
#include "RHIDefinitions.h"
#include "RHI.h"
#include "RenderResource.h"
#include "Engine/TextureRenderTarget2D.h"

#include "Windows/AllowWindowsPlatformTypes.h"
#include <d3d11.h>
#include "Windows/HideWindowsPlatformTypes.h"
#include "ThirdParty\OpenVinoWrapper\OpenVinoWrapper.h"

#define TEST_PASS_ROUTE	(0)

DECLARE_GPU_STAT(StyleTransferPass)

// permutation domains
class OVST_UseReserve : SHADER_PERMUTATION_BOOL("ENABLE_RESERVE");

BEGIN_SHADER_PARAMETER_STRUCT(FOVSTPassParameters, )
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, ColorSampler)
	SHADER_PARAMETER(FVector2D, OutExtent)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FParametersOCL, )
	SHADER_PARAMETER_STRUCT_INCLUDE(FOVSTPassParameters, OVST)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, OutputTexture)
END_SHADER_PARAMETER_STRUCT()

///
/// OVST COMPUTE SHADER
///
class FOVSTCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FOVSTCS);
	SHADER_USE_PARAMETER_STRUCT(FOVSTCS, FGlobalShader);

	using FPermutationDomain = TShaderPermutationDomain<OVST_UseReserve>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FOVSTPassParameters, OVST)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, OutputTexture)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);

		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), 16);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), 16);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEZ"), 1);
		OutEnvironment.SetDefine(TEXT("COMPUTE_SHADER"), 1);
	}
};

IMPLEMENT_GLOBAL_SHADER(FOVSTCS, "/Plugin/OpenVinoModule/Private/PostProcessOVST.usf", "MainCS", SF_Compute);

///
/// OVST PIXEL SHADER
///
class FOVSTPS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FOVSTPS);
	SHADER_USE_PARAMETER_STRUCT(FOVSTPS, FGlobalShader);

	using FPermutationDomain = TShaderPermutationDomain<OVST_UseReserve>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FOVSTPassParameters, OVST)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);

		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
	}
};

IMPLEMENT_GLOBAL_SHADER(FOVSTPS, "/Plugin/OpenVinoModule/Private/PostProcessOVST.usf", "MainPS", SF_Pixel);

StyleTransferSpatialUpscaler::StyleTransferSpatialUpscaler(TSharedPtr<FStyleTransferSpatialUpscalerData> InViewData)
	:ViewData(InViewData)
{	
}

StyleTransferSpatialUpscaler::~StyleTransferSpatialUpscaler()
{
}

ISpatialUpscaler* StyleTransferSpatialUpscaler::Fork_GameThread(const class FSceneViewFamily& ViewFamily) const
{
	// the object we return here will get deleted by UE4 when the scene view tears down, so we need to instantiate a new one every frame.
	return new StyleTransferSpatialUpscaler(ViewData);
}

FScreenPassTexture StyleTransferSpatialUpscaler::AddPasses(FRDGBuilder& GraphBuilder, const FViewInfo& View, const FInputs& PassInputs) const
{
	RDG_GPU_STAT_SCOPE(GraphBuilder, StyleTransferPass);
	check(PassInputs.SceneColor.IsValid());

	FString RHIName = GDynamicRHI->GetName();
	FScreenPassRenderTarget Output = PassInputs.OverrideOutput;
	check(Output.IsValid());

	int oclWidth, oclHeight;
	check(OpenVINO_GetCurrentSTsize(&oclWidth, &oclHeight));

	// create resource
	if (ViewData->initialized)
	{
		if (ViewData->ConvertTexture[0].ViewRect.Width() != oclWidth ||
			ViewData->ConvertTexture[0].ViewRect.Height() != oclHeight)
		{
			ViewData->initialized = false;
		}
	}
	if (!ViewData->initialized)
	{
		FRDGTextureDesc TextureDesc = PassInputs.SceneColor.Texture->Desc;
		TextureDesc.Extent.X = oclWidth;
		TextureDesc.Extent.Y = oclHeight;
		TextureDesc.Format = PF_R8G8B8A8;
		TextureDesc.Flags = TexCreate_ShaderResource | TexCreate_UAV | TexCreate_RenderTargetable;
		for (int i = 0; i < 6; i++)
		{
			ViewData->Names[i] = FString::Format(TEXT("OVST-Convert-{0}"), { FString::FromInt(i) });
			ViewData->ConvertTexture[i].Texture = GraphBuilder.CreateTexture(TextureDesc, *ViewData->Names[i], ERDGTextureFlags::MultiFrame);
			ViewData->ConvertTexture[i].ViewRect = FIntRect(0, 0, oclWidth, oclHeight);
		}
		ViewData->initialized = true;
	}
	int packIndex = GFrameNumber;
	int ovstIndex = GFrameNumber - 2;
	int outputIndex = (packIndex % 3) * 2 + 0;

	// input for openvino
	FRHISamplerState* BilinearClampSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	{
		const bool bStyleInputSupportsUAV = (ViewData->ConvertTexture[outputIndex].Texture->Desc.Flags & TexCreate_UAV) == TexCreate_UAV;
		if (!bStyleInputSupportsUAV)
		{	// vs-ps
			FOVSTPS::FParameters* PassParameters = GraphBuilder.AllocParameters<FOVSTPS::FParameters>();

			// set pass inputs
			PassParameters->OVST.InputTexture = PassInputs.SceneColor.Texture;
			PassParameters->OVST.ColorSampler = BilinearClampSampler;
			PassParameters->OVST.OutExtent.X = oclWidth;
			PassParameters->OVST.OutExtent.Y = oclHeight;
			PassParameters->RenderTargets[0] = FRenderTargetBinding(ViewData->ConvertTexture[outputIndex].Texture, ERenderTargetLoadAction::ENoAction);
			FScreenPassTextureViewport InputViewport = FScreenPassTextureViewport(PassInputs.SceneColor.Texture);
			FScreenPassTextureViewport OutputViewport = FScreenPassTextureViewport(ViewData->ConvertTexture[outputIndex].Texture);

			// grab shaders
			bool useReserve = false;
			FOVSTPS::FPermutationDomain PSPermutationVector;
			PSPermutationVector.Set<OVST_UseReserve>(useReserve);

			TShaderMapRef<FOVSTPS> PixelShader(View.ShaderMap, PSPermutationVector);

			AddDrawScreenPass(GraphBuilder,
				RDG_EVENT_NAME("Openvino Styletransfer Pass, use reserve=%d (PS)"
					, ((useReserve) ? 1 : 0)),
				View, OutputViewport, InputViewport,
				PixelShader, PassParameters,
				EScreenPassDrawFlags::None
			);
		}
		else
		{	// cs
			FOVSTCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FOVSTCS::FParameters>();

			// set pass inputs
			PassParameters->OVST.InputTexture = PassInputs.SceneColor.Texture;
			PassParameters->OVST.ColorSampler = BilinearClampSampler;
			PassParameters->OVST.OutExtent.X = oclWidth;
			PassParameters->OVST.OutExtent.Y = oclHeight;
			PassParameters->OutputTexture = GraphBuilder.CreateUAV(ViewData->ConvertTexture[outputIndex].Texture);
			FScreenPassTextureViewport OutputViewport = FScreenPassTextureViewport(ViewData->ConvertTexture[outputIndex].Texture);

			// grab shaders
			bool useReserve = false;
			FOVSTCS::FPermutationDomain CSPermutationVector;
			CSPermutationVector.Set<OVST_UseReserve>(useReserve);

			TShaderMapRef<FOVSTCS> ComputeShaderOVSTPass(View.ShaderMap, CSPermutationVector);

			FComputeShaderUtils::AddPass(GraphBuilder,
				RDG_EVENT_NAME("Openvino Styletransfer Pass, use reserve=%d (CSS)"
					, ((useReserve) ? 1 : 0)),
				ComputeShaderOVSTPass, PassParameters,
				FComputeShaderUtils::GetGroupCount(OutputViewport.Rect.Size(), 16)
			);
		}
	}

	// openvino pass here
	if(ovstIndex >= 0 && ViewData->startFrames >= 2)
	{
		int index = (ovstIndex % 3) * 2;
		FScreenPassTextureViewport OutputViewport = FScreenPassTextureViewport(ViewData->ConvertTexture[index + 1].Texture);
		FParametersOCL* PassParameters = GraphBuilder.AllocParameters<FParametersOCL>();
		PassParameters->OVST.InputTexture = ViewData->ConvertTexture[index].Texture;
		PassParameters->OVST.OutExtent.X = OutputViewport.Rect.Width();
		PassParameters->OVST.OutExtent.Y = OutputViewport.Rect.Height();
		PassParameters->OutputTexture = ViewData->ConvertTexture[index + 1].Texture;

#if PLATFORM_WINDOWS
#if TEST_PASS_ROUTE
		if (RHIName == TEXT("D3D11"))
#else
		if (ViewData->isIntel && RHIName == TEXT("D3D11"))
#endif
		{
			GraphBuilder.AddPass(
				RDG_EVENT_NAME("OpenVinoStyleTransfer"),
				PassParameters,
				ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
				[PassParameters](FRHICommandList& RHICmdList)
				{
#if !TEST_PASS_ROUTE
					// process opencl here
					ID3D11Texture2D* inputTex = static_cast<ID3D11Texture2D*>(PassParameters->OVST.InputTexture->GetRHI()->GetTexture2D()->GetNativeResource());
					ID3D11Texture2D* outputTex = static_cast<ID3D11Texture2D*>(PassParameters->OutputTexture->GetRHI()->GetTexture2D()->GetNativeResource());
					int width = PassParameters->OVST.OutExtent.X;
					int height = PassParameters->OVST.OutExtent.Y;
					// call open vino pass here ...
					OpenVino_Infer_FromDXData(inputTex, outputTex, width, height, 0);
#endif
				});
#if !TEST_PASS_ROUTE
			outputIndex = index + 1;
#endif
		}
#endif
	}

	// output for final
	const bool bOutputSupportsUAV = (Output.Texture->Desc.Flags & TexCreate_UAV) == TexCreate_UAV;
	if (!bOutputSupportsUAV)
	{	// vs-ps
		FOVSTPS::FParameters* PassParameters = GraphBuilder.AllocParameters<FOVSTPS::FParameters>();
		FScreenPassTextureViewport InputViewport = FScreenPassTextureViewport(ViewData->ConvertTexture[outputIndex].Texture);
		FScreenPassTextureViewport OutputViewport = FScreenPassTextureViewport(Output.Texture);

		// set pass inputs
		PassParameters->OVST.InputTexture = ViewData->ConvertTexture[outputIndex].Texture;
		PassParameters->OVST.ColorSampler = BilinearClampSampler;
		PassParameters->OVST.OutExtent.X = OutputViewport.Rect.Width();
		PassParameters->OVST.OutExtent.Y = OutputViewport.Rect.Height();
		PassParameters->RenderTargets[0] = FRenderTargetBinding(Output.Texture, ERenderTargetLoadAction::ENoAction);

		// grab shaders
		bool useReserve = false;
		FOVSTPS::FPermutationDomain PSPermutationVector;
		PSPermutationVector.Set<OVST_UseReserve>(useReserve);

		TShaderMapRef<FOVSTPS> PixelShader(View.ShaderMap, PSPermutationVector);

		AddDrawScreenPass(GraphBuilder,
			RDG_EVENT_NAME("Openvino Styletransfer Pass, use reserve=%d (PS)"
				, ((useReserve) ? 1 : 0)),
			View, OutputViewport, InputViewport,
			PixelShader, PassParameters,
			EScreenPassDrawFlags::None
		);
	}
	else
	{	// cs
		FOVSTCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FOVSTCS::FParameters>();
		FScreenPassTextureViewport OutputViewport = FScreenPassTextureViewport(Output.Texture);

		// set pass inputs
		PassParameters->OVST.InputTexture = ViewData->ConvertTexture[outputIndex].Texture;
		PassParameters->OVST.ColorSampler = BilinearClampSampler;
		PassParameters->OVST.OutExtent.X = OutputViewport.Rect.Width();
		PassParameters->OVST.OutExtent.Y = OutputViewport.Rect.Height();
		PassParameters->OutputTexture = GraphBuilder.CreateUAV(Output.Texture);
		
		// grab shaders
		bool useReserve = false;
		FOVSTCS::FPermutationDomain CSPermutationVector;
		CSPermutationVector.Set<OVST_UseReserve>(useReserve);

		TShaderMapRef<FOVSTCS> ComputeShaderOVSTPass(View.ShaderMap, CSPermutationVector);

		FComputeShaderUtils::AddPass(GraphBuilder,
			RDG_EVENT_NAME("Openvino Styletransfer Pass, use reserve=%d (CSS)"
				, ((useReserve) ? 1 : 0)),
			ComputeShaderOVSTPass, PassParameters,
			FComputeShaderUtils::GetGroupCount(OutputViewport.Rect.Size(), 16)
		);
	}
	
	FScreenPassTexture FinalOutput = Output;
	return MoveTemp(FinalOutput);
}