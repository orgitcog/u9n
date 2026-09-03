// Copyright Epic Games, Inc. All Rights Reserved.


#include "MetaHumanCoreTechLibVersion.h"
#include "Misc/EngineVersion.h"

#include "TitanVersion.h"


// Use FEngineVersion parsing features
static const FEngineVersion TitanLibVersionAsEngineVersion = FEngineVersion{ MHCTL_TITAN_MAJOR_VERSION, MHCTL_TITAN_MINOR_VERSION, MHCTL_TITAN_PATCH_VERSION, 0, TEXT("") };

FString FMetaHumanCoreTechLibVersion::GetMetaHumanCoreTechLibVersionString()
{
	return TitanLibVersionAsEngineVersion.ToString(EVersionComponent::Patch);
}

