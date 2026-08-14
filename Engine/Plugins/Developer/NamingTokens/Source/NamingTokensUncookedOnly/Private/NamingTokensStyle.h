// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"

/** Slate style set for Naming Tokens */
class FNamingTokensStyle : public FSlateStyleSet
{
public:
	static FName StyleName;

	/** Access the singleton instance for this style set */
	static FNamingTokensStyle& Get();

private:
	FNamingTokensStyle();
	~FNamingTokensStyle();
};
