// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"

class FMetaHumanPerformanceStyle
	: public FSlateStyleSet
{
public:
	static FMetaHumanPerformanceStyle& Get();

	static void Register();
	static void Unregister();

private:
	FMetaHumanPerformanceStyle();
};