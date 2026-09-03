// Copyright Epic Games, Inc. All Rights Reserved.

#include "CineAssemblyTakeRecorderSettings.h"

void UCineAssemblyTakeRecorderSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UObject::PostEditChangeProperty(PropertyChangedEvent);
	SaveConfig();
}
