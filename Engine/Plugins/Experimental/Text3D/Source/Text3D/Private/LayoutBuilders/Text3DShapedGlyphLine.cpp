// Copyright Epic Games, Inc. All Rights Reserved.

#include "LayoutBuilders/Text3DShapedGlyphLine.h"

float FShapedGlyphLine::GetWidthAdvance(const int32 InIndex, const float InKerning, const float InWordSpacing) const
{
	check(InIndex >= 0 && InIndex < GlyphsToRender.Num());

	const FShapedGlyphEntry& Glyph = GlyphsToRender[InIndex];
	float Advance = Glyph.XOffset + Glyph.XAdvance;

	const bool bFirstCharacter = InIndex == 0;
	const bool bLastCharacter = InIndex == GlyphsToRender.Num() - 1;
	const bool bSkipCharacter = TextDirection == TextBiDi::ETextDirection::RightToLeft ? bLastCharacter : bFirstCharacter;

	if (!bSkipCharacter)
	{
		// @note: as per FSlateElementBatcher::BuildShapedTextSequence, per Glyph Kerning isn't used
		Advance += InKerning;

		if (!Glyph.bIsVisible)
		{
			Advance += InWordSpacing;
		}
	}

	return Advance;
}
