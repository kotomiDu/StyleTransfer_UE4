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

void StyleTransferViewExtension::PreRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView)
{
	if (CVarTransferEnabled.GetValueOnAnyThread() == 2)
	{
		// Get device here
		void* device = RHICmdList.GetNativeDevice();
		FString RHIName = GDynamicRHI->GetName();
#if PLATFORM_WINDOWS
		// Set ocl device
		if (RHIName == TEXT("D3D11"))
		{
			// Set device here for ocl

		}
#endif
	}
}