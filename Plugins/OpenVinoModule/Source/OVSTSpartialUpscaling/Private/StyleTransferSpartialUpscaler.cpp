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



DECLARE_GPU_STAT(StyleTransferPass)

// permutation domains
class OVST_UseReserve : SHADER_PERMUTATION_BOOL("ENABLE_RESERVE");

BEGIN_SHADER_PARAMETER_STRUCT(FOVSTPassParameters, )
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputTexture)
	SHADER_PARAMETER(int, Width)
	SHADER_PARAMETER(int, Height)
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
	FScreenPassTextureViewport TotalOutputViewport = FScreenPassTextureViewport(Output.Texture);
	check(Output.IsValid());

	// create resource
	if (ViewData->initialized)
	{
		if (ViewData->ConvertTexture[0].ViewRect.Width() != TotalOutputViewport.Rect.Width() ||
			ViewData->ConvertTexture[0].ViewRect.Height() != TotalOutputViewport.Rect.Height())
		{
			ViewData->initialized = false;
		}
	}
	if (!ViewData->initialized)
	{
		FRDGTextureDesc TextureDesc = PassInputs.SceneColor.Texture->Desc;
		TextureDesc.Extent.X = TotalOutputViewport.Rect.Width();
		TextureDesc.Extent.Y = TotalOutputViewport.Rect.Height();
		TextureDesc.Format = PF_R8G8B8A8;
		TextureDesc.Flags = TexCreate_ShaderResource | TexCreate_UAV | TexCreate_RenderTargetable;
		for (int i = 0; i < 2; i++)
		{
			FString name = FString::Format(TEXT("OVST-Convert-{0}"), { FString::FromInt(i) });
			ViewData->ConvertTexture[i].Texture = GraphBuilder.CreateTexture(TextureDesc, name.GetCharArray().GetData(), ERDGTextureFlags::MultiFrame);
			ViewData->ConvertTexture[i].ViewRect = FIntRect(0, 0, TotalOutputViewport.Rect.Width(), TotalOutputViewport.Rect.Height());
		}
		ViewData->initialized = true;
	}

	// input for openvino
	for (int i = 0; i < 2; i++)
	{
		const bool bStyleInputSupportsUAV = (ViewData->ConvertTexture[i].Texture->Desc.Flags & TexCreate_UAV) == TexCreate_UAV;
		if (!bStyleInputSupportsUAV)
		{	// vs-ps
			FOVSTPS::FParameters* PassParameters = GraphBuilder.AllocParameters<FOVSTPS::FParameters>();

			// set pass inputs
			PassParameters->OVST.InputTexture = PassInputs.SceneColor.Texture;
			PassParameters->RenderTargets[0] = FRenderTargetBinding(ViewData->ConvertTexture[i].Texture, ERenderTargetLoadAction::ENoAction);
			FScreenPassTextureViewport InputViewport = FScreenPassTextureViewport(PassInputs.SceneColor.Texture);
			FScreenPassTextureViewport OutputViewport = FScreenPassTextureViewport(ViewData->ConvertTexture[i].Texture);

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
			PassParameters->OutputTexture = GraphBuilder.CreateUAV(ViewData->ConvertTexture[i].Texture);
			FScreenPassTextureViewport OutputViewport = FScreenPassTextureViewport(ViewData->ConvertTexture[i].Texture);

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
	{
		FScreenPassTextureViewport OutputViewport = FScreenPassTextureViewport(ViewData->ConvertTexture[1].Texture);
		FParametersOCL* PassParameters = GraphBuilder.AllocParameters<FParametersOCL>();
		PassParameters->OVST.InputTexture = ViewData->ConvertTexture[0].Texture;
		PassParameters->OVST.Width = OutputViewport.Rect.Width();
		PassParameters->OVST.Height = OutputViewport.Rect.Height();
		PassParameters->OutputTexture = ViewData->ConvertTexture[1].Texture;

#if PLATFORM_WINDOWS
		if (RHIName == TEXT("D3D11"))
		{
			GraphBuilder.AddPass(
				RDG_EVENT_NAME("OpenVinoStyleTransfer"),
				PassParameters,
				ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
				[PassParameters](FRHICommandList& RHICmdList)
				{
					// process opencl here
					ID3D11Texture2D* inputTex = static_cast<ID3D11Texture2D*>(PassParameters->OVST.InputTexture->GetRHI()->GetTexture2D()->GetNativeResource());
					ID3D11Texture2D* outputTex = static_cast<ID3D11Texture2D*>(PassParameters->OutputTexture->GetRHI()->GetTexture2D()->GetNativeResource());
					int width = PassParameters->OVST.Width;
					int height = PassParameters->OVST.Height;
					// call open vino pass here ...
					OpenVino_Infer_FromDXData(inputTex, outputTex, width, height, 0);

				});
		}
#endif
	}

	// output for final
	const bool bOutputSupportsUAV = (Output.Texture->Desc.Flags & TexCreate_UAV) == TexCreate_UAV;
	if (!bOutputSupportsUAV)
	{	// vs-ps
		FOVSTPS::FParameters* PassParameters = GraphBuilder.AllocParameters<FOVSTPS::FParameters>();

		// set pass inputs
		PassParameters->OVST.InputTexture = ViewData->ConvertTexture[1].Texture;
		PassParameters->RenderTargets[0] = FRenderTargetBinding(Output.Texture, ERenderTargetLoadAction::ENoAction);
		FScreenPassTextureViewport InputViewport = FScreenPassTextureViewport(ViewData->ConvertTexture[1].Texture);
		FScreenPassTextureViewport OutputViewport = FScreenPassTextureViewport(Output.Texture);

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
		PassParameters->OVST.InputTexture = ViewData->ConvertTexture[1].Texture;
		PassParameters->OutputTexture = GraphBuilder.CreateUAV(Output.Texture);
		FScreenPassTextureViewport OutputViewport = FScreenPassTextureViewport(Output.Texture);

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