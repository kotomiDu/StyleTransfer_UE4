#pragma once

#include "PostProcess/PostProcessUpscale.h"

struct FStyleTransferSpatialUpscalerData
{	
	bool initialized = false;
	FScreenPassTexture ConvertTexture[2];
};

class StyleTransferSpatialUpscaler final : public ISpatialUpscaler
{
public:
	StyleTransferSpatialUpscaler(TSharedPtr<FStyleTransferSpatialUpscalerData> InViewData);
	virtual ~StyleTransferSpatialUpscaler();

	// ISpatialUpscaler interface
	const TCHAR* GetDebugName() const override { return TEXT("StyleTransferSpatialUpscaler"); }

	ISpatialUpscaler* Fork_GameThread(const class FSceneViewFamily& ViewFamily) const override;
	FScreenPassTexture AddPasses(FRDGBuilder& GraphBuilder, const FViewInfo& View, const FInputs& PassInputs) const override;

private:
	TSharedPtr<FStyleTransferSpatialUpscalerData> ViewData;
};