// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Text3DShapedGlyphLine.h"

class FFreeTypeFace;
struct FShapedGlyphLine;

/** Contains text metrics with sufficient information to fetch and transform each line. */
struct FText3DShapedGlyphText
{
	FText3DShapedGlyphText();

	void Reset();
	void CalculateWidth();

	/** Individual character kerning */
	TArray<float> Kernings;

	/** General tracking for all characters */
	float Tracking;

	/** Spacing between words */
	float WordSpacing;

	/** Maximum width allowed */
	float MaxWidth;

	/** Whether to wrap words */
	bool bWrap;

	/** Lines calculated based on current text */
	TArray<FShapedGlyphLine> Lines;

	/** Glyph index to font face */
	TMap<uint32, const FFreeTypeFace*> GlyphIndexToFontFace;
};
