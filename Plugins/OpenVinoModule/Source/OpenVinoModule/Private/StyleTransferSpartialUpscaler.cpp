#include "StyleTransferSpartialUpscaler.h"

DECLARE_GPU_STAT(StyleTransferPass)

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

	FScreenPassTexture FinalOutput = PassInputs.SceneColor;
	return MoveTemp(FinalOutput);
}