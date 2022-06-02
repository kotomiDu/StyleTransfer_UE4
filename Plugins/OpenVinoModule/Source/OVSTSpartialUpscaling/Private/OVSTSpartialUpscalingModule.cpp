// Copyright Epic Games, Inc. All Rights Reserved.

#include "OVSTSpartialUpscalingModule.h"
#include "Engine/Engine.h"

#include "CoreMinimal.h"
#include "Interfaces/IPluginManager.h"

void FOVSTSpartialUpscalingModule::StartupModule()
{
	FString PluginShaderDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("OpenVinoModule"))->GetBaseDir(), TEXT("Shaders"));
	AddShaderSourceDirectoryMapping(TEXT("/Plugin/OpenVinoModule"), PluginShaderDir);
}

void FOVSTSpartialUpscalingModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FOVSTSpartialUpscalingModule, OVSTSpartialUpscaling)