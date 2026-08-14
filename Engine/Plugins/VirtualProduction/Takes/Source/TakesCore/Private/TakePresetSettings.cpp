// Copyright Epic Games, Inc. All Rights Reserved.

#include "TakePresetSettings.h"

#include "LevelSequence.h"

UTakePresetSettings::UTakePresetSettings()
	: TargetRecordClass(ULevelSequence::StaticClass())
{}

UTakePresetSettings* UTakePresetSettings::Get()
{
	return GetMutableDefault<UTakePresetSettings>();
}

UClass* UTakePresetSettings::GetTargetRecordClass() const
{
	return TargetRecordClass.TargetRecordClass ? TargetRecordClass.TargetRecordClass.Get() : ULevelSequence::StaticClass();
}

#if WITH_EDITOR
void UTakePresetSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UObject::PostEditChangeProperty(PropertyChangedEvent);
	SaveConfig();
	OnSettingsChangedDelegate.Broadcast();
}
#endif
