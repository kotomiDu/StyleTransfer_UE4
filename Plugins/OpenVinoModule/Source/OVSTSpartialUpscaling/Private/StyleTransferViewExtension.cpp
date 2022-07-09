#define OVSTSPATIALUPSCALING_API DLLEXPORT
#include "StyleTransferViewExtension.h"

#include "StyleTransferSpartialUpscaler.h"

#include "GenericPlatform/GenericPlatformDriver.h"
#if PLATFORM_WINDOWS
#include "Windows/WindowsPlatformMisc.h"
#endif

#if WITH_EDITOR
#include "Editor.h"
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
	bool isRunning = true;
#if WITH_EDITOR
	if (GIsEditor && !GEditor->PlayWorld && !GIsPlayInEditorWorld)
	{
		isRunning = false;
	}
#endif
	if (isRunning && InViewFamily.GetFeatureLevel() >= ERHIFeatureLevel::SM5 && CVarTransferEnabled.GetValueOnAnyThread() == 2)
	{
		FStyleTransferSpatialUpscalerData* viewData = new FStyleTransferSpatialUpscalerData();
		viewData->isIntel = isIntel;
		InViewFamily.SetSecondarySpatialUpscalerInterface(new StyleTransferSpatialUpscaler(TSharedPtr<FStyleTransferSpatialUpscalerData>(viewData)));
	}
}
