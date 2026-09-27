// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "ColorManagement/ColorSpace.h"
#include "Engine/TextureDefines.h"
#include "IMediaOptions.h"

#include "ImgMediaSourceColorSettings.generated.h"

// TODO: EMediaSourceEncoding, FMediaSourceColorSettings & FNativeMediaSourceColorSettings should be moved up to BaseMediaSource.h and unified with UCaptureCardMediaSource settings.

/** List of source encodings that can be converted to linear. (Integer values match the ETextureSourceEncoding values in TextureDefines.h */
UENUM()
enum class EMediaSourceEncoding : uint8
{
	MSE_None	= 0 UMETA(DisplayName = "None", ToolTip = "The source encoding is not overridden."),
	MSE_Linear	= 1 UMETA(DisplayName = "Linear", ToolTip = "The source encoding is considered linear."),
	MSE_sRGB	= 2 UMETA(DisplayName = "sRGB", ToolTip = "sRGB source encoding to be linearized"),
	MSE_ST2084	= 3 UMETA(DisplayName = "ST 2084/PQ", ToolTip = "SMPTE ST 2084/PQ source encoding to be linearized"),
	MSE_SLog3	= 12 UMETA(DisplayName = "SLog3", ToolTip = "Sony SLog3 source encoding to be linearized"),

	MSE_MAX,
};

/* Manual definition of media source color space & encoding. */
USTRUCT(BlueprintType)
struct FMediaSourceColorSettings
{
	GENERATED_USTRUCT_BODY()

	FMediaSourceColorSettings()
		: EncodingOverride(EMediaSourceEncoding::MSE_None)
		, ColorSpaceOverride(ETextureColorSpace::TCS_None)
		, RedChromaticityCoordinate(FVector2D::ZeroVector)
		, GreenChromaticityCoordinate(FVector2D::ZeroVector)
		, BlueChromaticityCoordinate(FVector2D::ZeroVector)
		, WhiteChromaticityCoordinate(FVector2D::ZeroVector)
		, ChromaticAdaptationMethod(static_cast<ETextureChromaticAdaptationMethod>(UE::Color::DEFAULT_CHROMATIC_ADAPTATION_METHOD))
	{}

	/** Source encoding of the media. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ColorManagement)
	EMediaSourceEncoding EncodingOverride;

	/** Source color space of the media. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ColorManagement)
	ETextureColorSpace ColorSpaceOverride;

	/** Red chromaticity coordinate of the source color space. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ColorManagement, meta = (EditCondition = "ColorSpaceOverride == ETextureColorSpace::TCS_Custom"))
	FVector2D RedChromaticityCoordinate;

	/** Green chromaticity coordinate of the source color space. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ColorManagement, meta = (EditCondition = "ColorSpaceOverride == ETextureColorSpace::TCS_Custom"))
	FVector2D GreenChromaticityCoordinate;

	/** Blue chromaticity coordinate of the source color space. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ColorManagement, meta = (EditCondition = "ColorSpaceOverride == ETextureColorSpace::TCS_Custom"))
	FVector2D BlueChromaticityCoordinate;

	/** White chromaticity coordinate of the source color space. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ColorManagement, meta = (EditCondition = "ColorSpaceOverride == ETextureColorSpace::TCS_Custom"))
	FVector2D WhiteChromaticityCoordinate;

	/** Chromatic adaption method applied if the source white point differs from the working color space white point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ColorManagement, meta = (EditCondition = "ColorSpaceOverride != ETextureColorSpace::TCS_None"))
	ETextureChromaticAdaptationMethod ChromaticAdaptationMethod;

#if WITH_EDITOR
	/** Update the chromaticity coordinates member variables based on the color space choice (unless custom). */
	IMGMEDIA_API void UpdateColorSpaceChromaticities();
#endif
};

/* Engine-native color source settings container for media option. */
struct FNativeMediaSourceColorSettings : public IMediaOptions::FDataContainer
{
	/** Constructor */
	FNativeMediaSourceColorSettings();

	/** Updates the native settings from user-controlled settings. */
	void Update(const FMediaSourceColorSettings& InSettings);

	/** Color encoding override getter. */
	UE::Color::EEncoding GetEncodingOverride() const { return EncodingOverride; }

	/** Color space override getter. */
	const UE::Color::FColorSpace& GetColorSpaceOverride(const UE::Color::FColorSpace& InDefaultColorSpace) const;

	/** Chromatic adaptation getter. */
	UE::Color::EChromaticAdaptationMethod GetChromaticAdaptationMethod() const { return ChromaticAdaptationMethod; }

private:
	/** Manual source encoding override. */
	std::atomic<UE::Color::EEncoding> EncodingOverride;

	/** Manual source color space override. */
	TOptional<UE::Color::FColorSpace> ColorSpaceOverride;

	/** Chromatic adapation to be used on manual source color space override. */
	std::atomic<UE::Color::EChromaticAdaptationMethod> ChromaticAdaptationMethod;

	/** Protects color space override variable. */
	mutable FCriticalSection ColorSpaceCriticalSection;
};
