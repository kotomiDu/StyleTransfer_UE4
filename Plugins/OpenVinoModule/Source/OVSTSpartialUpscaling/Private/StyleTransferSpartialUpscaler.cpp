#include "StyleTransferSpartialUpscaler.h"

DECLARE_GPU_STAT(StyleTransferPass)

// permutation domains
class OVST_UseReserve : SHADER_PERMUTATION_BOOL("ENABLE_RESERVE");

BEGIN_SHADER_PARAMETER_STRUCT(FOVSTPassParameters, )
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputTexture)
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
/// RCAS PIXEL SHADER
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

	FScreenPassRenderTarget Output = PassInputs.OverrideOutput;
	check(Output.IsValid());

	// add pass here
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

	FScreenPassTexture FinalOutput = Output;
	return MoveTemp(FinalOutput);
}