// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanCharacterPaletteProjectSettings.h"
#include "MetaHumanTypes.h"

UMetaHumanCharacterPaletteProjectSettings::UMetaHumanCharacterPaletteProjectSettings()
{
	// Add the slots for all possible legacy pipelines to be configured in the project settings
	DefaultCharacterLegacyPipelines.Add(EMetaHumanQualityLevel::Cinematic);
	DefaultCharacterLegacyPipelines.Add(EMetaHumanQualityLevel::High);
	DefaultCharacterLegacyPipelines.Add(EMetaHumanQualityLevel::Medium);
	DefaultCharacterLegacyPipelines.Add(EMetaHumanQualityLevel::Low);

	DefaultCharacterUEFNPipelines.Add(EMetaHumanQualityLevel::High);
	DefaultCharacterUEFNPipelines.Add(EMetaHumanQualityLevel::Medium);
	DefaultCharacterUEFNPipelines.Add(EMetaHumanQualityLevel::Low);
}