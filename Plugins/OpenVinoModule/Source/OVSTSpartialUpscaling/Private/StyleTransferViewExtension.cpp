#include "StyleTransferSpartialUpscaler.h"

#define OVSTSPATIALUPSCALING_API DLLEXPORT
#include "StyleTransferViewExtension.h"

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
	// Get device here
	void* device = RHICmdList.GetNativeDevice();


	
}