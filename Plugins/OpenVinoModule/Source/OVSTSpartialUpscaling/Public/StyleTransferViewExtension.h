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
	void PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily) override {}

	OVSTSPATIALUPSCALING_API void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override;
	OVSTSPATIALUPSCALING_API void PreRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView) override;

private:
	OVSTSPATIALUPSCALING_API void OnCreate();
	bool isIntel;
};