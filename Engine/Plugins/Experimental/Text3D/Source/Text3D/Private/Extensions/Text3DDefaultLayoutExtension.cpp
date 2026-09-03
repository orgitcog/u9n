// Copyright Epic Games, Inc. All Rights Reserved.

#include "Extensions/Text3DDefaultLayoutExtension.h"

#include "Algo/Accumulate.h"
#include "Algo/Count.h"
#include "Characters/Text3DCharacterBase.h"
#include "Engine/Font.h"
#include "Extensions/Text3DCharacterExtensionBase.h"
#include "Extensions/Text3DGeometryExtensionBase.h"
#include "Framework/Text/PlainTextLayoutMarshaller.h"
#include "LayoutBuilders/Text3DLayout.h"
#include "LayoutBuilders/Text3DLayoutShaper.h"
#include "LayoutBuilders/Text3DShapedGlyphText.h"
#include "Logs/Text3DLogs.h"
#include "Misc/EnumerateRange.h"
#include "Styling/StyleDefaults.h"
#include "Styling/SlateTypes.h"
#include "Subsystems/Text3DEngineSubsystem.h"
#include "Text3DComponent.h"

#if WITH_FREETYPE
#include "Fonts/SlateTextShaper.h"
#endif

FName UText3DDefaultLayoutExtension::GetUseMaxWidthPropertyName()
{
	return GET_MEMBER_NAME_CHECKED(UText3DDefaultLayoutExtension, bUseMaxWidth);
}

FName UText3DDefaultLayoutExtension::GetUseMaxHeightPropertyName()
{
	return GET_MEMBER_NAME_CHECKED(UText3DDefaultLayoutExtension, bUseMaxHeight);
}

FName UText3DDefaultLayoutExtension::GetMaxHeightPropertyName()
{
	return GET_MEMBER_NAME_CHECKED(UText3DDefaultLayoutExtension, MaxHeight);
}

FName UText3DDefaultLayoutExtension::GetMaxWidthPropertyName()
{
	return GET_MEMBER_NAME_CHECKED(UText3DDefaultLayoutExtension, MaxWidth);
}

FName UText3DDefaultLayoutExtension::GetScaleProportionallyPropertyName()
{
	return GET_MEMBER_NAME_CHECKED(UText3DDefaultLayoutExtension, bScaleProportionally);
}

void UText3DDefaultLayoutExtension::SetTracking(const float Value)
{
	if (FMath::IsNearlyEqual(Tracking, Value))
	{
		return;
	}
	
	Tracking = Value;
	OnLayoutOptionsChanged();
}

void UText3DDefaultLayoutExtension::SetLineSpacing(const float Value)
{
	if (FMath::IsNearlyEqual(LineSpacing, Value))
	{
		return;
	}
	
	LineSpacing = Value;
	OnLayoutOptionsChanged();
}

void UText3DDefaultLayoutExtension::SetWordSpacing(const float Value)
{
	if (FMath::IsNearlyEqual(WordSpacing, Value))
	{
		return;
	}

	WordSpacing = Value;
	OnLayoutOptionsChanged();
}

void UText3DDefaultLayoutExtension::SetHorizontalAlignment(const EText3DHorizontalTextAlignment Value)
{
	if (HorizontalAlignment == Value)
	{
		return;
	}

	HorizontalAlignment = Value;
	OnLayoutOptionsChanged();
}

void UText3DDefaultLayoutExtension::SetVerticalAlignment(const EText3DVerticalTextAlignment Value)
{
	if (VerticalAlignment == Value)
	{
		return;
	}
	
	VerticalAlignment = Value;
	OnLayoutOptionsChanged();
}

void UText3DDefaultLayoutExtension::SetUseMaxWidth(const bool Value)
{
	if (bUseMaxWidth == Value)
	{
		return;
	}

	bUseMaxWidth = Value;
	OnLayoutOptionsChanged();
}

void UText3DDefaultLayoutExtension::SetMaxWidth(const float Value)
{
	const float NewValue = FMath::Max(1.0f, Value);
	if (FMath::IsNearlyEqual(MaxWidth, NewValue))
	{
		return;
	}
	
	MaxWidth = NewValue;
	OnLayoutOptionsChanged();
}

void UText3DDefaultLayoutExtension::SetMaxWidthBehavior(const EText3DMaxWidthHandling Value)
{
	if (MaxWidthBehavior == Value)
	{
		return;
	}

	MaxWidthBehavior = Value;
	OnLayoutOptionsChanged();
}

void UText3DDefaultLayoutExtension::SetUseMaxHeight(const bool Value)
{
	if (bUseMaxHeight == Value)
	{
		return;
	}
	
	bUseMaxHeight = Value;
	OnLayoutOptionsChanged();
}

void UText3DDefaultLayoutExtension::SetMaxHeight(const float Value)
{
	const float NewValue = FMath::Max(1.0f, Value);
	if (FMath::IsNearlyEqual(MaxHeight, NewValue))
	{
		return;
	}

	MaxHeight = NewValue;
	OnLayoutOptionsChanged();
}

void UText3DDefaultLayoutExtension::SetScaleProportionally(const bool Value)
{
	if (bScaleProportionally == Value)
	{
		return;
	}

	bScaleProportionally = Value;
	OnLayoutOptionsChanged();
}

float UText3DDefaultLayoutExtension::GetTextHeight() const
{
	return ShapedText->Lines.Num() * TypeFaceMetrics.FontHeight + (ShapedText->Lines.Num() - 1) * LineSpacing;
}

FVector UText3DDefaultLayoutExtension::GetTextScale() const
{
	return TextScale;
}

EText3DExtensionResult UText3DDefaultLayoutExtension::PreRendererUpdate(EText3DRendererFlags InFlag)
{
	if (InFlag != EText3DRendererFlags::Geometry && InFlag != EText3DRendererFlags::Layout)
	{
		return EText3DExtensionResult::Active;
	}

	UText3DComponent* Text3DComponent = GetText3DComponent();
	
	if (!ShapedText.IsValid())
	{
		ShapedText = MakeShared<FText3DShapedGlyphText>();
	}

	if (!TextLayout.IsValid())
	{
		TextLayout = MakeShared<FText3DLayout>();
	}

	if (!TextLayoutMarshaller.IsValid())
	{
		TextLayoutMarshaller = FPlainTextLayoutMarshaller::Create();
	}

	if (!FontStyle.IsValid())
	{
		FontStyle = MakeShared<FTextBlockStyle>();
	}

	if (EnumHasAnyFlags(Text3DComponent->GetUpdateFlags(), EText3DRendererFlags::Geometry))
	{
		UFont* Font = Text3DComponent->GetFont();
		FSlateFontInfo FontInfo(Font, UE::Text3D::Metrics::SlateFontSize);
		FontInfo.CompositeFont = Font->GetCompositeFont() ? MakeShared<FCompositeFont>(*Text3DComponent->GetFont()->GetCompositeFont()) : FStyleDefaults::GetFontInfo().CompositeFont;
		FontInfo.TypefaceFontName = Text3DComponent->GetTypeface();
		FontStyle->SetFont(FontInfo);
	}

	ShapedText->Reset();
	FText3DLayoutShaper::Get()->ShapeBidirectionalText(*FontStyle, Text3DComponent->GetFormattedText().ToString(), TextLayout, TextLayoutMarshaller, ShapedText->Lines);

	const int32 CharacterCount = Algo::TransformAccumulate(ShapedText->Lines, [](const FShapedGlyphLine& InLine)
	{
		return Algo::CountIf(InLine.GlyphsToRender, [](const FShapedGlyphEntry& InGlyph)
		{
			return InGlyph.HasValidGlyph() && InGlyph.bIsVisible;
		});
	},0);

	if (EnumHasAnyFlags(Text3DComponent->GetUpdateFlags(), EText3DRendererFlags::Geometry))
	{
		UText3DCharacterExtensionBase* CharacterExtension = Text3DComponent->GetCharacterExtension();
		CharacterExtension->AllocateCharacters(CharacterCount);
	}

	ShapedText->GlyphIndexToFontFace.Reserve(CharacterCount);
	ShapedText->Kernings.SetNum(CharacterCount);

	UText3DGeometryExtensionBase* GeometryExtension = Text3DComponent->GetGeometryExtension();
	uint16 CharacterIndex = 0;

#if WITH_FREETYPE
	FT_Face FontFace = nullptr;
	for (TConstEnumerateRef<FShapedGlyphLine> ShapedLine : EnumerateRange(ShapedText->Lines))
	{
		const bool bIsRTL = (ShapedLine->TextDirection == TextBiDi::ETextDirection::RightToLeft);

		for (TConstEnumerateRef<FShapedGlyphEntry> GlyphEntry : EnumerateRange(ShapedLine->GlyphsToRender))
		{
			if (!GlyphEntry->FontFaceData.IsValid())
			{
				// Add as nullptr if not already in map
				if (!ShapedText->GlyphIndexToFontFace.Contains(GlyphEntry->GlyphIndex))
				{
					ShapedText->GlyphIndexToFontFace.Add(GlyphEntry->GlyphIndex, nullptr);
				}

				continue;
			}

			if (const TSharedPtr<FFreeTypeFace> FontFacePtr = GlyphEntry->FontFaceData->FontFace.Pin())
			{
				ShapedText->GlyphIndexToFontFace.FindOrAdd(GlyphEntry->GlyphIndex, FontFacePtr.Get());
				FontFace = FontFacePtr->GetFace();
			}

			if (GlyphEntry->HasValidGlyph() && GlyphEntry->bIsVisible)
			{
				const uint16 Index = bIsRTL ? CharacterCount - 1 - CharacterIndex : CharacterIndex;
				UText3DCharacterBase* Character = Text3DComponent->GetCharacter(Index);
				ShapedText->Kernings[CharacterIndex] = Character->GetCharacterKerning();
				CharacterIndex++;
			}
		}
	}

	if (EnumHasAnyFlags(Text3DComponent->GetUpdateFlags(), EText3DRendererFlags::Geometry))
	{
		FTypefaceFontData* TypefaceFontData = GeometryExtension->GetTypefaceFontData();
		if (!TypefaceFontData)
		{
			return EText3DExtensionResult::Failed;
		}

		TypefaceFontData->SetTypeFace(FontFace);

		if (FontFace)
		{
			TypeFaceMetrics.FontHeight = FontFace->size->metrics.height * UE::Text3D::Metrics::FontSizeInverse;
			TypeFaceMetrics.FontAscender = FontFace->size->metrics.ascender * UE::Text3D::Metrics::FontSizeInverse;
			TypeFaceMetrics.FontDescender = FontFace->size->metrics.descender * UE::Text3D::Metrics::FontSizeInverse;
		}
	}
#else
	UE_LOG(LogText3D, Warning, TEXT("FreeType is not available, cannot proceed without it"))
	return EText3DExtensionResult::Failed;
#endif

	ShapedText->Tracking = Tracking;
	ShapedText->WordSpacing = WordSpacing;
	ShapedText->MaxWidth = MaxWidth;
	ShapedText->bWrap = bUseMaxWidth && MaxWidthBehavior == EText3DMaxWidthHandling::WrapAndScale;
	ShapedText->CalculateWidth();
	CalculateTextScale();

	CharacterIndex = 0;
	for (TConstEnumerateRef<FShapedGlyphLine> ShapedLine : EnumerateRange(ShapedText->Lines))
	{
		FVector Location = GetLineLocation(ShapedLine.GetIndex());
		const bool bIsRTL = (ShapedLine->TextDirection == TextBiDi::ETextDirection::RightToLeft);

		for (TConstEnumerateRef<FShapedGlyphEntry> ShapedGlyph : EnumerateRange(ShapedLine->GlyphsToRender))
		{
			if (ShapedGlyph->HasValidGlyph() && ShapedGlyph->bIsVisible)
			{
				const uint16 Index = bIsRTL ? CharacterCount - 1 - CharacterIndex : CharacterIndex;
				UText3DCharacterBase* Character = Text3DComponent->GetCharacter(Index);

				if (EnumHasAnyFlags(Text3DComponent->GetUpdateFlags(), EText3DRendererFlags::Geometry))
				{
					Character->SetGlyphIndex(ShapedGlyph->GlyphIndex);
					GeometryExtension->FindOrLoadGlyphMesh(Character);
				}

				constexpr bool bReset = true;
				FTransform& CharacterTransform = Character->GetTransform(bReset);

				const FVector GlyphSize = Character->GetMeshBounds().GetSize();
				FVector Shift = FVector::ZeroVector;
				switch (GeometryExtension->GetGlyphHAlignment())
				{
				case EText3DHorizontalTextAlignment::Left:
					Shift = FVector::ZeroVector;
					break;
				case EText3DHorizontalTextAlignment::Center:
					Shift.Y = (GlyphSize.Y / 2);
					break;
				case EText3DHorizontalTextAlignment::Right:
					Shift.Y = GlyphSize.Y;
					break;
				}

				if (ShapedGlyph.GetIndex() != 0)
				{
					Shift.Y += Tracking + ShapedText->Kernings[CharacterIndex];
				}

				CharacterTransform.Accumulate(FTransform(Location + Shift));
				CharacterIndex++;
			}

			Location.Y += ShapedLine->GlyphAdvances[ShapedGlyph.GetIndex()];
		}
	}

	return EText3DExtensionResult::Finished;
}

EText3DExtensionResult UText3DDefaultLayoutExtension::PostRendererUpdate(EText3DRendererFlags InFlag)
{
	return EText3DExtensionResult::Active;
}

void UText3DDefaultLayoutExtension::CalculateTextScale()
{
	FVector Scale(1.0f, 1.0f, 1.0f);

	float TextMaxWidth = 0.0f;
	for (const FShapedGlyphLine& ShapedLine : ShapedText->Lines)
	{
		TextMaxWidth = FMath::Max(TextMaxWidth, ShapedLine.Width);
	}

	if (bUseMaxWidth && TextMaxWidth > MaxWidth && TextMaxWidth > 0.0f)
	{
		Scale.Y *= MaxWidth / TextMaxWidth;
		if (bScaleProportionally)
		{
			Scale.Z = Scale.Y;
		}
	}

	const float TotalHeight = GetTextHeight();
	if (bUseMaxHeight && TotalHeight > MaxHeight && TotalHeight > 0.0f)
	{
		Scale.Z *= MaxHeight / TotalHeight;
		if (bScaleProportionally)
		{
			Scale.Y = Scale.Z;
		}
	}

	if (bScaleProportionally)
	{
		Scale.X = Scale.Y;
	}

	TextScale = Scale;
}

FVector UText3DDefaultLayoutExtension::GetLineLocation(int32 LineIndex) const
{
	float HorizontalOffset = 0.0f;
	float VerticalOffset = 0.0f;
	if (LineIndex < 0 || LineIndex >= ShapedText->Lines.Num())
	{
		return FVector::ZeroVector;
	}

	const FShapedGlyphLine& ShapedLine = ShapedText->Lines[LineIndex];

	if (HorizontalAlignment == EText3DHorizontalTextAlignment::Center)
	{
		HorizontalOffset = -ShapedLine.Width * 0.5f;
	}
	else if (HorizontalAlignment == EText3DHorizontalTextAlignment::Right)
	{
		HorizontalOffset = -ShapedLine.Width;
	}

	const float TotalHeight = GetTextHeight();
	if (VerticalAlignment != EText3DVerticalTextAlignment::FirstLine)
	{
		// First align it to Top
		VerticalOffset -= TypeFaceMetrics.FontAscender;

		if (VerticalAlignment == EText3DVerticalTextAlignment::Center)
		{
			VerticalOffset += TotalHeight * 0.5f;
		}
		else if (VerticalAlignment == EText3DVerticalTextAlignment::Bottom)
		{
			VerticalOffset += TotalHeight + TypeFaceMetrics.FontDescender;
		}
	}

	VerticalOffset -= LineIndex * (TypeFaceMetrics.FontHeight + LineSpacing);

	return FVector(0.0f, HorizontalOffset, VerticalOffset);
}

#if WITH_EDITOR
void UText3DDefaultLayoutExtension::PostEditChangeProperty(FPropertyChangedEvent& InEvent)
{
	Super::PostEditChangeProperty(InEvent);

	static const TSet<FName> LayoutPropertyNames
	{
		GET_MEMBER_NAME_CHECKED(UText3DDefaultLayoutExtension, Tracking),
		GET_MEMBER_NAME_CHECKED(UText3DDefaultLayoutExtension, LineSpacing),
		GET_MEMBER_NAME_CHECKED(UText3DDefaultLayoutExtension, WordSpacing),
		GET_MEMBER_NAME_CHECKED(UText3DDefaultLayoutExtension, HorizontalAlignment),
		GET_MEMBER_NAME_CHECKED(UText3DDefaultLayoutExtension, VerticalAlignment),
		GET_MEMBER_NAME_CHECKED(UText3DDefaultLayoutExtension, bUseMaxWidth),
		GET_MEMBER_NAME_CHECKED(UText3DDefaultLayoutExtension, MaxWidth),
		GET_MEMBER_NAME_CHECKED(UText3DDefaultLayoutExtension, bUseMaxHeight),
		GET_MEMBER_NAME_CHECKED(UText3DDefaultLayoutExtension, MaxHeight),
		GET_MEMBER_NAME_CHECKED(UText3DDefaultLayoutExtension, bScaleProportionally),
	};

	if (LayoutPropertyNames.Contains(InEvent.GetMemberPropertyName()))
	{
		OnLayoutOptionsChanged();
	}
}
#endif

void UText3DDefaultLayoutExtension::OnLayoutOptionsChanged()
{
	MaxWidth = FMath::Max(1.0f, MaxWidth);
	MaxHeight = FMath::Max(1.0f, MaxHeight);

	EText3DRendererFlags Flags = EText3DRendererFlags::Material;
	EnumAddFlags(Flags, EText3DRendererFlags::Layout);

	if (MaxWidthBehavior == EText3DMaxWidthHandling::WrapAndScale && ShapedText.IsValid())
	{
		for (const TConstEnumerateRef<FShapedGlyphLine> ShapedLine : EnumerateRange(ShapedText->Lines))
		{
			if (ShapedLine->TextDirection == TextBiDi::ETextDirection::RightToLeft)
			{
				EnumAddFlags(Flags, EText3DRendererFlags::Geometry);
				break;
			}
		}
	}

	RequestUpdate(Flags);
}
