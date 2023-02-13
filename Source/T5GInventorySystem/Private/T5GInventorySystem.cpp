// Copyright Epic Games, Inc. All Rights Reserved.

#include "T5GInventorySystem.h"

#define LOCTEXT_NAMESPACE "FT5GInventorySystemModule"

void FT5GInventorySystemModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
}

void FT5GInventorySystemModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FT5GInventorySystemModule, T5GInventorySystem)