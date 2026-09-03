// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"

class FNavigationToolStyle final : public FSlateStyleSet
{
public:
	static FNavigationToolStyle& Get()
	{
		static FNavigationToolStyle Instance;
		return Instance;
	}

	FNavigationToolStyle();
	virtual ~FNavigationToolStyle() override;
};
