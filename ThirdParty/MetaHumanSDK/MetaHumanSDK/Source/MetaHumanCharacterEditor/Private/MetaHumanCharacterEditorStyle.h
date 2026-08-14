// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"

/**
 * Slate style set for the MetaHuman Character Editor
 */
class FMetaHumanCharacterEditorStyle : public FSlateStyleSet
{
public:

	static const FMetaHumanCharacterEditorStyle& Get();

	static void Register();
	static void Unregister();

private:
	FMetaHumanCharacterEditorStyle();

	struct FSkinAccentRegionStyleParams
	{
		FName Property;
		FVector2D BrushSize;
		FName Image;
	};

	/** Helper function to set the style of a skin accent region */
	void SetSkinAccentRegionStyle(const FSkinAccentRegionStyleParams& InParams);
};