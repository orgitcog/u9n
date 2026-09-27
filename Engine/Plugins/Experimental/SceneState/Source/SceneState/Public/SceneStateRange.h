// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include <limits>
#include "SceneStateRange.generated.h"

USTRUCT()
struct FSceneStateRange
{
	GENERATED_BODY()

	static constexpr uint16 InvalidIndex = std::numeric_limits<uint16>::max();

	uint16 GetLastIndex() const
	{
		checkSlow(IsValid());
		return Index + Count - 1;
	}

	bool IsValid() const
	{
		return Count > 0 && Index <= InvalidIndex - Count;
	}

	UPROPERTY()
	uint16 Index = FSceneStateRange::InvalidIndex;

	UPROPERTY()
	uint16 Count = 0;
};
