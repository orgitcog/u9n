// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Fonts/FontCache.h"
#include "Text3DTypes.h"

/** Contains text line metric with sufficient information to fetch and transform each character. */
struct FShapedGlyphLine
{
	/** Get's the offset from the previous character, accounting for kerning and word spacing. */
	float GetWidthAdvance(const int32 InIndex, const float InKerning, const float InWordSpacing) const;

	/** The corresponding shaped glyph for each character in this line of text. */
	TArray<FShapedGlyphEntry> GlyphsToRender;

	/** Stored result of line width */
	float Width = 0.0f;

	/** Glyph advance on this line */
	TArray<float> GlyphAdvances;

	/** Direction of the text in the line */
	TextBiDi::ETextDirection TextDirection = TextBiDi::ETextDirection::LeftToRight;
};
