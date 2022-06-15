#include "StyleTransferSpartialUpscaler.h"
#include "RHIStaticStates.h"
#include "RHIDefinitions.h"
#include "RHI.h"
#include "RenderResource.h"
#include "Engine/TextureRenderTarget2D.h"

#include "Windows/AllowWindowsPlatformTypes.h"
#include <d3d11.h>
#include "Windows/HideWindowsPlatformTypes.h"



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
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), 64);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), 1);
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

StyleTransferSpatialUpscaler::StyleTransferSpatialUpscaler()
{	
}

StyleTransferSpatialUpscaler::~StyleTransferSpatialUpscaler()
{
}

ISpatialUpscaler* StyleTransferSpatialUpscaler::Fork_GameThread(const class FSceneViewFamily& ViewFamily) const
{
	// the object we return here will get deleted by UE4 when the scene view tears down, so we need to instantiate a new one every frame.
	return new StyleTransferSpatialUpscaler();
}

FScreenPassTexture StyleTransferSpatialUpscaler::AddPasses(FRDGBuilder& GraphBuilder, const FViewInfo& View, const FInputs& PassInputs) const
{
	RDG_GPU_STAT_SCOPE(GraphBuilder, StyleTransferPass);
	check(PassInputs.SceneColor.IsValid());

	FString RHIName = GDynamicRHI->GetName();
	FScreenPassRenderTarget Output = PassInputs.OverrideOutput;
	check(Output.IsValid());

	// add pass here
#if 1	// openvino pass
	FScreenPassTextureViewport OutputViewport = FScreenPassTextureViewport(Output.Texture);
	FParametersOCL* PassParameters = GraphBuilder.AllocParameters<FParametersOCL>();
	PassParameters->OVST.InputTexture = PassInputs.SceneColor.Texture;
	PassParameters->OVST.Width = OutputViewport.Rect.Width();
	PassParameters->OVST.Height = OutputViewport.Rect.Height();
	PassParameters->OutputTexture = Output.Texture;

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
				
			});
	}
#endif
#else	// test PS/CS pass
	const bool bOutputSupportsUAV = (Output.Texture->Desc.Flags & TexCreate_UAV) == TexCreate_UAV;
	if (!bOutputSupportsUAV)
	{	// vs-ps
		FOVSTPS::FParameters* PassParameters = GraphBuilder.AllocParameters<FOVSTPS::FParameters>();

		// set pass inputs
		PassParameters->OVST.InputTexture = PassInputs.SceneColor.Texture;
		PassParameters->RenderTargets[0] = FRenderTargetBinding(Output.Texture, ERenderTargetLoadAction::ENoAction);
		FScreenPassTextureViewport OutputViewport = FScreenPassTextureViewport(Output.Texture);

		// grab shaders
		bool useReserve = false;
		FOVSTPS::FPermutationDomain PSPermutationVector;
		PSPermutationVector.Set<OVST_UseReserve>(useReserve);

		TShaderMapRef<FOVSTPS> PixelShader(View.ShaderMap, PSPermutationVector);

		AddDrawScreenPass(GraphBuilder,
			RDG_EVENT_NAME("Openvino Styletransfer Pass, use reserve=%d (PS)"
				, ((useReserve) ? 1 : 0)),
			View, OutputViewport, OutputViewport,
			PixelShader, PassParameters,
			EScreenPassDrawFlags::None
		);
	}
	else
	{	// cs
		FOVSTCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FOVSTCS::FParameters>();

		// set pass inputs
		PassParameters->OVST.InputTexture = PassInputs.SceneColor.Texture;
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
#endif
	
	FScreenPassTexture FinalOutput = Output;
	return MoveTemp(FinalOutput);
}