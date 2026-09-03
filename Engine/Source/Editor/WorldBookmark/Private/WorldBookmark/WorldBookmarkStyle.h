// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateStyle.h"

class FWorldBookmarkStyle final : public FSlateStyleSet
{
public:
	FWorldBookmarkStyle();
	virtual ~FWorldBookmarkStyle();

	static FWorldBookmarkStyle& Get();
};
