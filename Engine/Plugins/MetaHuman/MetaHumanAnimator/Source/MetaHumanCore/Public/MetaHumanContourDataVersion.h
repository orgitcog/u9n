// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/UnrealString.h"

/** Version for the contour data internal functionality (not necessarily matching UE version) */
struct METAHUMANCORE_API FMetaHumanContourDataVersion
{
	enum class ECompatibilityResult : uint8
	{
		NoUpgrade = 0,		// Nothing to change
		NeedsUpgrade,		// Not expected to be compatible
		RecommendUpgrade,	// Compatible but upgrade is recommended
		AutoUpgrade,		// Minor change that could be automatically resolved
	};

	/** Returns the Mesh Tracker module version on an FEngineVersion struct */
	static FString GetContourDataVersionString();

	/** Check for compatibility for a list of versions, returning the most severe upgrade requirement as OutResult */
	static bool CheckVersionCompatibility(const TArray<FString>& InVersionStringList, ECompatibilityResult& OutResult);

	/** Config file name for contour data */
	const static FString ConfigFileName;
};
