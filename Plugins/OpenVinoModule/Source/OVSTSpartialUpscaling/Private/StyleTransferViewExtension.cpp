#define OVSTSPATIALUPSCALING_API DLLEXPORT
#include "StyleTransferViewExtension.h"

#include "StyleTransferSpartialUpscaler.h"

static TAutoConsoleVariable<int32> CVarTransferEnabled(
	TEXT("r.OVST.Enabled"),
	0,
	TEXT("Set Openvino Style transfer enabled. 0:Disable 1:CPU 2:GPU"),
	ECVF_RenderThreadSafe);

void StyleTransferViewExtension::BeginRenderViewFamily(FSceneViewFamily& InViewFamily)
{
	if (InViewFamily.GetFeatureLevel() >= ERHIFeatureLevel::SM5 && CVarTransferEnabled.GetValueOnAnyThread() == 2)
	{
		InViewFamily.SetSecondarySpatialUpscalerInterface(new StyleTransferSpatialUpscaler());
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