// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImgMediaSourceColorSettings.h"

#if WITH_EDITOR
void FMediaSourceColorSettings::UpdateColorSpaceChromaticities()
{
	if (ColorSpaceOverride != ETextureColorSpace::TCS_Custom)
	{
		UE::Color::FColorSpace Chromaticities(static_cast<UE::Color::EColorSpace>(ColorSpaceOverride));
		Chromaticities.GetChromaticities(RedChromaticityCoordinate, GreenChromaticityCoordinate, BlueChromaticityCoordinate, WhiteChromaticityCoordinate);
	}
}
#endif // WITH_EDITOR

FNativeMediaSourceColorSettings::FNativeMediaSourceColorSettings()
	: EncodingOverride(UE::Color::EEncoding::None)
	, ColorSpaceOverride()
	, ChromaticAdaptationMethod(UE::Color::EChromaticAdaptationMethod::Bradford)
{}

void FNativeMediaSourceColorSettings::Update(const FMediaSourceColorSettings& InSettings)
{
	{
		FScopeLock Lock(&ColorSpaceCriticalSection);

		if (InSettings.ColorSpaceOverride == ETextureColorSpace::TCS_Custom)
		{
			ColorSpaceOverride = UE::Color::FColorSpace(
				InSettings.RedChromaticityCoordinate,
				InSettings.GreenChromaticityCoordinate,
				InSettings.BlueChromaticityCoordinate,
				InSettings.WhiteChromaticityCoordinate
			);
		}
		else if (InSettings.ColorSpaceOverride != ETextureColorSpace::TCS_None)
		{
			ColorSpaceOverride = UE::Color::FColorSpace(static_cast<UE::Color::EColorSpace>(InSettings.ColorSpaceOverride));
		}
		else
		{
			ColorSpaceOverride.Reset();
		}
	}

	EncodingOverride = static_cast<UE::Color::EEncoding>(InSettings.EncodingOverride);
	ChromaticAdaptationMethod = static_cast<UE::Color::EChromaticAdaptationMethod>(InSettings.ChromaticAdaptationMethod);
}

const UE::Color::FColorSpace& FNativeMediaSourceColorSettings::GetColorSpaceOverride(const UE::Color::FColorSpace& InDefaultColorSpace) const
{
	FScopeLock Lock(&ColorSpaceCriticalSection);
	
	if (ColorSpaceOverride.IsSet())
	{
		return ColorSpaceOverride.GetValue();
	}

	return InDefaultColorSpace;
}

