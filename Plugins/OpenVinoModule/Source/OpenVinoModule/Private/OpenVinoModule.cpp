// Copyright Epic Games, Inc. All Rights Reserved.

#include "OpenVinoModule.h"
#include "Engine/Engine.h"

#define OVSTSPATIALUPSCALING_API DLLIMPORT
#include "StyleTransferViewExtension.h"

IMPLEMENT_MODULE(FOpenVinoModuleModule, OpenVinoModule)

#define LOCTEXT_NAMESPACE "FOpenVinoModuleModule"

void FOpenVinoModuleModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	PStyleTransferViewExtension = FSceneViewExtensions::NewExtension<StyleTransferViewExtension>();
}

void FOpenVinoModuleModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
	PStyleTransferViewExtension = nullptr;
}

#undef LOCTEXT_NAMESPACE