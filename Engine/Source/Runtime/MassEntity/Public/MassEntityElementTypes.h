// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassEntityElementTypes.generated.h"

// This is the base class for all lightweight fragments
USTRUCT()
struct FMassFragment
{
	GENERATED_BODY()
};

// This is the base class for types that will only be tested for presence/absence, i.e. Tags.
// Subclasses should never contain any member properties.
USTRUCT()
struct FMassTag
{
	GENERATED_BODY()
};

USTRUCT()
struct FMassChunkFragment
{
	GENERATED_BODY()
};

USTRUCT()
struct FMassSharedFragment
{
	GENERATED_BODY()
};

USTRUCT()
struct FMassConstSharedFragment
{
	GENERATED_BODY()
};

namespace UE::Mass
{
	template<typename T>
	bool IsA(const UStruct* /*Struct*/)
	{
		return false;
	}

	template<>
	inline bool IsA<FMassFragment>(const UStruct* Struct)
	{
		return Struct && Struct->IsChildOf(FMassFragment::StaticStruct());
	}

	template<>
	inline bool IsA<FMassTag>(const UStruct* Struct)
	{
		return Struct && Struct->IsChildOf(FMassTag::StaticStruct());
	}

	template<>
	inline bool IsA<FMassChunkFragment>(const UStruct* Struct)
	{
		return Struct && Struct->IsChildOf(FMassChunkFragment::StaticStruct());
	}

	template<>
	inline bool IsA<FMassSharedFragment>(const UStruct* Struct)
	{
		return Struct && Struct->IsChildOf(FMassSharedFragment::StaticStruct());
	}

	template<>
	inline bool IsA<FMassConstSharedFragment>(const UStruct* Struct)
	{
		return Struct && Struct->IsChildOf(FMassConstSharedFragment::StaticStruct());
	}
}
