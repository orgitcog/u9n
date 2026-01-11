// Copyright Epic Games, Inc. All Rights Reserved.

#include "AudioMeterStyle.h"
#include "Styling/StyleDefaults.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AudioMeterStyle)

FAudioMeterStyle::FAudioMeterStyle()
	: MeterSize(FVector2D(250.0f, 25.0f))
	, MeterPadding(FVector2D(10.0f, 5.0f))
	, MeterValuePadding(3.0f)
	, PeakValueWidth(2.0f)
	, ValueRangeDb(FVector2D(-160, 10))
	, bShowScale(true)
	, bScaleSide(true)
	, ScaleHashOffset(5.0f)
	, ScaleHashWidth(1.0f)
	, ScaleHashHeight(10.0f)
	, DecibelsPerHash(10)
	, Font(FStyleDefaults::GetFontInfo(5))
{
}

void FAudioMeterStyle::GetResources(TArray< const FSlateBrush* >& OutBrushes) const
{
	OutBrushes.Add(&MeterValueImage);
	OutBrushes.Add(&MeterBackgroundImage);
	OutBrushes.Add(&MeterPeakImage);
}

const FName FAudioMeterStyle::TypeName(TEXT("FAudioMeterStyle"));

const FAudioMeterStyle& FAudioMeterStyle::GetDefault()
{
	static FAudioMeterStyle Default;
	return Default;
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
FAudioMeterStyle& FAudioMeterStyle::SetFont(const FName& InFontName, uint16 InSize) { Font = FSlateFontInfo(InFontName, InSize); return *this; }
FAudioMeterStyle& FAudioMeterStyle::SetFont(const FString& InFontName, uint16 InSize) { Font = FSlateFontInfo(*InFontName, InSize); return *this; }
FAudioMeterStyle& FAudioMeterStyle::SetFont(const WIDECHAR* InFontName, uint16 InSize) { Font = FSlateFontInfo(InFontName, InSize); return *this; }
FAudioMeterStyle& FAudioMeterStyle::SetFont(const ANSICHAR* InFontName, uint16 InSize) { Font = FSlateFontInfo(InFontName, InSize); return *this; }
FAudioMeterStyle& FAudioMeterStyle::SetFontName(const FName& InFontName) { Font = FSlateFontInfo(InFontName, Font.Size); return *this; }
FAudioMeterStyle& FAudioMeterStyle::SetFontName(const FString& InFontName) { Font = FSlateFontInfo(InFontName, Font.Size); return *this; }
FAudioMeterStyle& FAudioMeterStyle::SetFontName(const WIDECHAR* InFontName) { Font = FSlateFontInfo(InFontName, Font.Size); return *this; }
FAudioMeterStyle& FAudioMeterStyle::SetFontName(const ANSICHAR* InFontName) { Font = FSlateFontInfo(InFontName, Font.Size); return *this; }
PRAGMA_ENABLE_DEPRECATION_WARNINGS