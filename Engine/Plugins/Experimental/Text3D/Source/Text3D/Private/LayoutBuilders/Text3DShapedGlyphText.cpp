// Copyright Epic Games, Inc. All Rights Reserved.

#include "LayoutBuilders/Text3DShapedGlyphText.h"

#include "Misc/EnumerateRange.h"

FText3DShapedGlyphText::FText3DShapedGlyphText()
{
	Reset();
}

void FText3DShapedGlyphText::Reset()
{
	Kernings.Reset();
	Tracking = 0.f;
	WordSpacing = 0.0f;
	bWrap = false;
	Lines.Reset();
	GlyphIndexToFontFace.Reset();
}

void FText3DShapedGlyphText::CalculateWidth()
{
	TArray<FShapedGlyphLine> NewLines;
    NewLines.Reserve(Lines.Num());

    int32 CharacterIndex = 0;
    for (TConstEnumerateRef<FShapedGlyphLine> GlyphLine : EnumerateRange(Lines))
    {
        FShapedGlyphLine* CurrentLine = &NewLines.AddDefaulted_GetRef();
        CurrentLine->TextDirection = GlyphLine->TextDirection;

        const bool bIsRTL = (GlyphLine->TextDirection == TextBiDi::ETextDirection::RightToLeft);

        TArray<float> CurrentAdvances;
        TArray<FShapedGlyphEntry> CurrentWord;
        float LineWidth = 0.0f;
        float CurrentWordLength = 0.0f;

        const int32 GlyphCount = GlyphLine->GlyphsToRender.Num();

		for (TConstEnumerateRef<FShapedGlyphEntry> CurrentGlyph : EnumerateRange(GlyphLine->GlyphsToRender))
        {
    		const bool bVisibleGlyph = CurrentGlyph->HasValidGlyph() && CurrentGlyph->bIsVisible;
			const int32 Index = bIsRTL ? Kernings.Num() - 1 - CharacterIndex : CharacterIndex;
    		const float CurrentKerning = bVisibleGlyph ? Kernings[Index] : 0.f;
    		const float GlyphAdv = GlyphLine->GetWidthAdvance(CurrentGlyph.GetIndex(), Tracking + CurrentKerning, WordSpacing);

            // If we're at the end the line or at whitespace
            const bool bWordBreak = !bVisibleGlyph || CurrentGlyph.GetIndex() == GlyphCount - 1;

    		if (bWrap                               // when we're wrapping
				&& bWordBreak                       // and at a word break
				&& LineWidth > MaxWidth             // and the current line is longer than the max
				&& CurrentWordLength != LineWidth)  // and the line is not just a single word that we can't break
    		{
    			CurrentLine->Width = LineWidth - CurrentWordLength;
    			CurrentLine = &NewLines.AddDefaulted_GetRef();
    			CurrentLine->TextDirection = GlyphLine->TextDirection;
    			LineWidth = CurrentWordLength;
    		}

			LineWidth += GlyphAdv;
			CurrentWordLength += GlyphAdv;

			if (bIsRTL)
			{
				CurrentWord.Insert(*CurrentGlyph, 0);
				CurrentAdvances.Insert(GlyphAdv, 0);
			}
			else
			{
				CurrentWord.Add(*CurrentGlyph);
				CurrentAdvances.Add(GlyphAdv);
			}

            if (bWordBreak)
            {
        		if (bIsRTL)
        		{
        			CurrentLine->GlyphsToRender.Insert(CurrentWord, 0);
        			CurrentLine->GlyphAdvances.Insert(CurrentAdvances, 0);
        		}
	            else
	            {
            		CurrentLine->GlyphsToRender.Append(CurrentWord);
            		CurrentLine->GlyphAdvances.Append(CurrentAdvances);
	            }

                CurrentWordLength = 0.0f;
                CurrentWord.Reset();
                CurrentAdvances.Reset();
            }

    		if (bVisibleGlyph)
    		{
    			CharacterIndex++;
    		}
        }

        CurrentLine->Width = LineWidth;
    }

    Lines = NewLines;
}