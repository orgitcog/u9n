// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Settings/EditorLoadingSavingSettings.h"

namespace UE::MetaHuman
{
// Note: This function is duplicated in the MetaHumanDepthGenerator module.
// The intention is to consolidate these functions in future.
TArray<FAutoReimportDirectoryConfig> CaptureSourceUpdateAutoReimportExclusion(
	const FString& InSourceDirectory,
	const FString& InWildcard,
	const TArray<FAutoReimportDirectoryConfig>& InDirectoryConfigs
);

// Note: This function is duplicated in the MetaHumanDepthGenerator module.
// The intention is to consolidate these functions in future.
bool CaptureSourceDirectoryConfigsAreDifferent(
	const TArray<FAutoReimportDirectoryConfig>& Lhs,
	const TArray<FAutoReimportDirectoryConfig>& Rhs
);
} // namespace UE::MetaHuman
