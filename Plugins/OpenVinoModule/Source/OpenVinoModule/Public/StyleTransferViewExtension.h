#pragma once

#include "SceneViewExtension.h"

class StyleTransferViewExtension final : public FSceneViewExtensionBase
{
public:
	StyleTransferViewExtension(const FAutoRegister& AutoRegister) : FSceneViewExtensionBase(AutoRegister) {}

	// ISceneViewExtension interface
	void SetupViewFamily(FSceneViewFamily& InViewFamily) override {}
	void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override {}
	void PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily) override {}

	void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override;
	void PreRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView) override;
};