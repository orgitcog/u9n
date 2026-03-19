// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanAudioBaseLiveLinkSubjectSettings.h"



void UMetaHumanAudioBaseLiveLinkSubjectSettings::Setup()
{
	Super::Setup();

	// No calibration, smoothing or head translation required.
	// Calibration and head translations are no-ops unless configured,
	// but we do need to explicitly set smoothing parameters to null.
	Parameters = nullptr;
}