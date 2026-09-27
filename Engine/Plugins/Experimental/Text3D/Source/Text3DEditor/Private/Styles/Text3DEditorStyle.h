// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"

class FText3DEditorStyle : public FSlateStyleSet
{
public:
	FText3DEditorStyle();
	
	virtual ~FText3DEditorStyle() override;
	
	static FText3DEditorStyle& Get()
	{
		static FText3DEditorStyle StyleSet;
		return StyleSet;
	}
};