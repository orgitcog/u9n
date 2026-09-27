// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaHumanViewportModes.generated.h"

UENUM()
enum class EABImageViewMode
{
	// Ensure any change is reflected in the material shader

	A = 0, // Single view modes
	B,

	ABSplit, // Multi view modes
	ABSide,

	Current, // Special modes for querying per-view parameter state
	Any
};

UENUM()
enum class EABImageNavigationMode
{
	ThreeD = 0,
	TwoD
};

UENUM()
enum class EABImageMouseSide
{
	NotApplicable = 0,
	A,
	B
};
