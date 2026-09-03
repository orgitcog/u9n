// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanCalibrationLib.h"
#include "TitanVersion.h"

#include "Modules/ModuleManager.h"

#include "Misc/EngineVersion.h"

static const FEngineVersion TitanLibVersionAsEngineVersion = FEngineVersion { MHCALIB_TITAN_MAJOR_VERSION, MHCALIB_TITAN_MINOR_VERSION, MHCALIB_TITAN_PATCH_VERSION, 0, TEXT("") };

FString FMetaHumanCalibrationLibModule::GetVersion()
{
	return TitanLibVersionAsEngineVersion.ToString(EVersionComponent::Patch);
}

IMPLEMENT_MODULE(FMetaHumanCalibrationLibModule, MetaHumanCalibrationLib)