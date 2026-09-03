// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanEditorSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaHumanEditorSettings)


UMetaHumanEditorSettings::UMetaHumanEditorSettings()
{
	SampleCount = 2;
	MaximumResolution = 8192;
	bForceSerialIngestion = false;
	bShowDevelopersContent = false;
	bShowOtherDevelopersContent = false;
	bLoadTrackersOnStartup = true;
	bTrainSolversFastLowMemory_DEPRECATED = false;
}

void UMetaHumanEditorSettings::PostEditChangeProperty(struct FPropertyChangedEvent& InPropertyChangeEvent)
{
	Super::PostEditChangeProperty(InPropertyChangeEvent);

	OnSettingsChanged.Broadcast();
}

