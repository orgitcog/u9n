// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"

class AVALANCHEEDITORCORE_API FAvaEditorCoreStyle final : public FSlateStyleSet
{
public:
	static FAvaEditorCoreStyle& Get()
	{
		static FAvaEditorCoreStyle Instance;
		return Instance;
	}

	FAvaEditorCoreStyle();
	virtual ~FAvaEditorCoreStyle() override;
};
