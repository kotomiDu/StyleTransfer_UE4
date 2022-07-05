#define OVSTSPATIALUPSCALING_API DLLEXPORT
#include "StyleTransferViewExtension.h"

#include "StyleTransferSpartialUpscaler.h"

#include "GenericPlatform/GenericPlatformDriver.h"
#if PLATFORM_WINDOWS
#include "Windows/WindowsPlatformMisc.h"
#endif

static TAutoConsoleVariable<int32> CVarTransferEnabled(
	TEXT("r.OVST.Enabled"),
	0,
	TEXT("Set Openvino Style transfer enabled. 0:Disable 1:CPU 2:GPU"),
	ECVF_RenderThreadSafe);

void StyleTransferViewExtension::OnCreate()
{
	starFrames = 0;
	lastMode = 0;
	isIntel = false;
#if PLATFORM_WINDOWS
	FGPUDriverInfo GPUDriverInfo = FPlatformMisc::GetGPUDriverInfo(GRHIAdapterName);
	if (GPUDriverInfo.IsIntel())
	{
		isIntel = true;
	}
#endif
}

void StyleTransferViewExtension::BeginRenderViewFamily(FSceneViewFamily& InViewFamily)
{
	if (InViewFamily.GetFeatureLevel() >= ERHIFeatureLevel::SM5 && CVarTransferEnabled.GetValueOnAnyThread() == 2)
	{
		FStyleTransferSpatialUpscalerData* viewData = new FStyleTransferSpatialUpscalerData();
		viewData->isIntel = isIntel;
		InViewFamily.SetSecondarySpatialUpscalerInterface(new StyleTransferSpatialUpscaler(TSharedPtr<FStyleTransferSpatialUpscalerData>(viewData)));
	}
}

void StyleTransferViewExtension::PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily)
{
	int mode = CVarTransferEnabled.GetValueOnAnyThread();
	if (mode == 2)
	{
		if (lastMode != mode)
		{
			starFrames = 0;
		}
		else
		{
			starFrames++;
		}
		const StyleTransferSpatialUpscaler* upscaler = static_cast<const StyleTransferSpatialUpscaler*>(InViewFamily.GetSecondarySpatialUpscalerInterface());
		upscaler->SetStartFrames(starFrames);
	}
	lastMode = mode;
}