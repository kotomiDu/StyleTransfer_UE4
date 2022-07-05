#pragma once

#include "PostProcess/PostProcessUpscale.h"

struct FStyleTransferSpatialUpscalerData
{	
	bool isIntel;
	int startFrames;
	bool initialized = false;
	FScreenPassTexture ConvertTexture[6];
	FString Names[6];
};

class StyleTransferSpatialUpscaler final : public ISpatialUpscaler
{
public:
	StyleTransferSpatialUpscaler(TSharedPtr<FStyleTransferSpatialUpscalerData> InViewData);
	virtual ~StyleTransferSpatialUpscaler();

	// ISpatialUpscaler interface
	const TCHAR* GetDebugName() const override { return TEXT("StyleTransferSpatialUpscaler"); }

	void SetStartFrames(int frames) const { ViewData->startFrames = frames; }

	ISpatialUpscaler* Fork_GameThread(const class FSceneViewFamily& ViewFamily) const override;
	FScreenPassTexture AddPasses(FRDGBuilder& GraphBuilder, const FViewInfo& View, const FInputs& PassInputs) const override;

private:
	TSharedPtr<FStyleTransferSpatialUpscalerData> ViewData;
};