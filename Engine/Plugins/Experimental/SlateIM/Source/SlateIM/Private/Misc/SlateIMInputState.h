// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "InputCoreTypes.h"

enum class ESlateIMKeyState : uint8
{
	Idle,
	Pressed,
	Held,
	Released,
};

struct FSlateIMInputState
{
	TMap<FKey, ESlateIMKeyState> KeyStateMap;
	TMap<FKey, float> AnalogValueMap;
};
