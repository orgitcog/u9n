// Copyright Epic Games, Inc. All Rights Reserved.

#include "LayoutBuilders/Text3DLayoutShaper.h"

#include "Brushes/SlateNoResource.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Fonts/FontCache.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Text/PlainTextLayoutMarshaller.h"
#include "Framework/Text/ShapedTextCache.h"
#include "Framework/Text/SlateTextRun.h"
#include "Framework/Text/TextLayout.h"
#include "Internationalization/Text.h"
#include "Text3DShapedGlyphLine.h"

TSharedPtr<FText3DLayoutShaper> FText3DLayoutShaper::Get()
{
	static TSharedPtr<FText3DLayoutShaper> Instance = MakeShared<FText3DLayoutShaper>(FPrivateToken{});
	return Instance;
}

void FText3DLayoutShaper::ShapeBidirectionalText(
	const FTextBlockStyle& Style,
	const FString& Text,
	const TSharedPtr<FTextLayout>& TextLayout,
	const TSharedPtr<ITextLayoutMarshaller>& TextMarshaller,
	TArray<FShapedGlyphLine>& OutShapedLines)
{
	// @todo: Restore when dependency on SlateApplication is removed. Currently this means text meshes won't be created on the server.
	// @see: UE-211843
	if (!FSlateApplication::IsInitialized())
	{
		return;
	}

	TextLayout->ClearLines();
	TextLayout->ClearLineHighlights();
	TextLayout->ClearRunRenderers();
	
	TextMarshaller->SetText(Text, *TextLayout);
	TextMarshaller->ClearDirty();

	TextLayout->UpdateLayout();

	TArray<FTextLayout::FLineView> LineViews = TextLayout->GetLineViews();

	// @note: mimics FSlateTextLayout::OnPaint
	for (const FTextLayout::FLineView& Line : LineViews)
	{
		FShapedGlyphLine& ShapedLine = OutShapedLines.AddDefaulted_GetRef();
		ShapedLine.TextDirection = Line.TextBaseDirection;

		for (const TSharedRef<ILayoutBlock>& Block : Line.Blocks)
		{
			FString BlockText;
			Block->GetRun()->AppendTextTo(BlockText);

			TSharedRef<const FString> BlockTextRef = MakeShared<const FString>(BlockText);
			
			const FTextRange BlockRange = Block->GetTextRange();
			const FLayoutBlockTextContext BlockTextContext = Block->GetTextContext();

			const FShapedGlyphSequenceRef ShapedGlyphSequence = ShapedTextCacheUtil::GetShapedTextSubSequence(
				BlockTextContext.ShapedTextCache,
				FCachedShapedTextKey(Line.Range, 1.0f, BlockTextContext, Style.Font),
				BlockRange,
				**BlockTextRef,
				BlockTextContext.TextDirection
			);

			ShapedLine.GlyphsToRender.Append(ShapedGlyphSequence->GetGlyphsToRender());
		}

		if (ShapedLine.TextDirection == TextBiDi::ETextDirection::RightToLeft)
		{
			Algo::Reverse(ShapedLine.GlyphsToRender);
		}
	}
}
