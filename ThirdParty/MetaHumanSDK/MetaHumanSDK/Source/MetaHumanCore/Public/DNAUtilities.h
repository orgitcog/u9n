// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

enum class EDNARigCompatiblityFlags
{
	None	= 0,

	Joint	= 1 << 0,
	Mesh	= 1 << 1,
	LOD		= 1 << 2,

	All = Joint | Mesh | LOD
};

ENUM_CLASS_FLAGS(EDNARigCompatiblityFlags);

class IDNAReader;

class METAHUMANCORE_API FDNAUtilities
{
public:
	// Checks if provided DNA readers share the same rig definition
	static bool CheckCompatibility(IDNAReader* InDnaReaderA, IDNAReader* InDnaReaderB, EDNARigCompatiblityFlags InCompareFlags = EDNARigCompatiblityFlags::All);

	// Checks if provided DNA readers share the same rig definition. Also outputs compatibility message
	static bool CheckCompatibility(IDNAReader* InDnaReaderA, IDNAReader* InDnaReaderB, EDNARigCompatiblityFlags InCompareFlags, FString& OutCompatibilityMsg);
};
