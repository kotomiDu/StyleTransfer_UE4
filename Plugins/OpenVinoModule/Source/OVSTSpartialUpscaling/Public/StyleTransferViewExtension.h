#pragma once

#include "SceneViewExtension.h"

struct FStyleTransferSpatialUpscalerData;
class StyleTransferViewExtension final : public FSceneViewExtensionBase
{
public:
	StyleTransferViewExtension(const FAutoRegister& AutoRegister) : FSceneViewExtensionBase(AutoRegister) {
		OnCreate();
	}

	// ISceneViewExtension interface
	void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override {}
	void SetupViewFamily(FSceneViewFamily& InViewFamily) override {}
	void PreRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView) override {}

	OVSTSPATIALUPSCALING_API void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override;
	OVSTSPATIALUPSCALING_API void PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily) override;

private:
	OVSTSPATIALUPSCALING_API void OnCreate();
	bool isIntel;
	bool starFrames;
	int32 lastMode;
};