// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Settings/EditorLoadingSavingSettings.h"

namespace UE::MetaHuman
{
// Note: This function is duplicated in the MetaHumanCaptureSource module.
// The intention is to consolidate these functions in future.
TArray<FAutoReimportDirectoryConfig> DepthGeneratorUpdateAutoReimportExclusion(
	const FString& InSourceDirectory,
	const FString& InWildcard,
	const TArray<FAutoReimportDirectoryConfig>& InDirectoryConfigs
);

// Note: This function is duplicated in the MetaHumanCaptureSource module.
// The intention is to consolidate these functions in future.
bool DepthGeneratorDirectoryConfigsAreDifferent(
	const TArray<FAutoReimportDirectoryConfig>& Lhs,
	const TArray<FAutoReimportDirectoryConfig>& Rhs
);
} // namespace UE::MetaHuman
