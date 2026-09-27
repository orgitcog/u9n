// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/ObjectMacros.h"

#include "MetaHumanBodyType.h"

#include "MetaHumanMigrationInfo.generated.h"

USTRUCT()
struct FMetaHumanTextureSynthesisMigrationInfo
{
	GENERATED_BODY()

	UPROPERTY()
	bool bSkinUnlocked = true;

	UPROPERTY()
	float LowFrequency = 0.5f;

	UPROPERTY()
	float HighFrequency = 0.5f;

	UPROPERTY()
	float Contrast = 0.5f;

	UPROPERTY()
	FVector2f SkinToneUI = FVector2f{ ForceInit };

	UPROPERTY()
	FLinearColor SkinTone = FLinearColor{ ForceInit };
};

USTRUCT()
struct FMetaHumanFreckesMigrationInfo
{
	GENERATED_BODY()

	UPROPERTY()
	FName Option;

	UPROPERTY()
	float Density = 0.5f;

	UPROPERTY()
	float Saturation = 0.5f;

	UPROPERTY()
	float Strength = 0.5f;

	UPROPERTY()
	float ToneShift = 0.5f;
};

USTRUCT()
struct FMetaHumanAccentRegionMigrationInfo
{
	GENERATED_BODY()

	UPROPERTY()
	float Redness = 0.5f;

	UPROPERTY()
	float Saturation = 0.5f;

	UPROPERTY()
	float Lightness = 0.5f;
};

USTRUCT()
struct FMetaHumanAccentRegionsMigrationInfo
{
	GENERATED_BODY()

	UPROPERTY()
	FMetaHumanAccentRegionMigrationInfo Scalp;

	UPROPERTY()
	FMetaHumanAccentRegionMigrationInfo Forehead;

	UPROPERTY()
	FMetaHumanAccentRegionMigrationInfo Nose;

	UPROPERTY()
	FMetaHumanAccentRegionMigrationInfo UnderEye;

	UPROPERTY()
	FMetaHumanAccentRegionMigrationInfo Cheeks;

	UPROPERTY()
	FMetaHumanAccentRegionMigrationInfo Lips;

	UPROPERTY()
	FMetaHumanAccentRegionMigrationInfo Chin;

	UPROPERTY()
	FMetaHumanAccentRegionMigrationInfo Ears;
};

USTRUCT()
struct FMetaHumanIrisMigrationInfo
{
	GENERATED_BODY()

	UPROPERTY()
	FName Option;

	// TODO: Need to map these to FLinearColor
	UPROPERTY()
	FVector2D Color1UI = FVector2D(0.8f, 0.3f);

	UPROPERTY()
	FLinearColor Color1 = FLinearColor{ ForceInit };

	UPROPERTY()
	FVector2D Color2UI = FVector2D(0.5f, 0.3f);

	UPROPERTY()
	FLinearColor Color2 = FLinearColor{ ForceInit };

	UPROPERTY()
	bool bUseRadialBlend = true;

	UPROPERTY()
	float ColorBalance = 0.5f;

	UPROPERTY()
	float ColorBalanceSmoothness = 0.5f;

	UPROPERTY()
	float Size = 0.5f;

	UPROPERTY()
	float Saturation = 0.5f;

	UPROPERTY()
	float LimbusDarkAmount = 0.5f;
};

USTRUCT()
struct FMetaHumanScleraMigrationInfo
{
	GENERATED_BODY()

	UPROPERTY()
	FVector2D TintUI = FVector2D{ 0.2f, 0.2f };

	UPROPERTY()
	FLinearColor Tint = FLinearColor{ ForceInit };

	UPROPERTY()
	float Brightness = 0.5f;

	UPROPERTY()
	float Rotation = 0.5f;

	UPROPERTY()
	float Vascularity = 0.5f;

	UPROPERTY()
	float VeinsRotation = 0.5f;
};

USTRUCT()
struct FMetaHumanEyeMigrationInfo
{
	GENERATED_BODY()

	UPROPERTY()
	FMetaHumanIrisMigrationInfo Iris;

	UPROPERTY()
	FMetaHumanScleraMigrationInfo Sclera;
};

USTRUCT()
struct FMetaHumanTeethMigrationInfo
{
	GENERATED_BODY()

	UPROPERTY()
	FVector2D TeethColorUI = FVector2D(0.35f);

	UPROPERTY()
	FLinearColor TeethColor = FLinearColor{ ForceInit };

	UPROPERTY()
	FVector2D GumColorUI = FVector2D(0.35f);

	UPROPERTY()
	FLinearColor GumColor = FLinearColor{ ForceInit };

	UPROPERTY()
	FVector2D PlaqueColorUI = FVector2D(0.35f);

	UPROPERTY()
	FLinearColor PlaqueColor = FLinearColor{ ForceInit };

	UPROPERTY()
	float PlaqueAmount = 0.12f;
};

USTRUCT()
struct FMetaHumanFoundationMakeupMigrationInfo
{
	GENERATED_BODY()

	UPROPERTY()
	bool bApplyFoundation = false;

	UPROPERTY()
	FLinearColor Color = FLinearColor{ ForceInit };

	UPROPERTY()
	float Intensity = 0.39f;

	UPROPERTY()
	float Roughness = 0.25f;

	UPROPERTY()
	float Concealer = 0.57f;
};

USTRUCT()
struct FMetaHumanEyesMakeupMigrationInfo
{
	GENERATED_BODY()

	UPROPERTY()
	FName Option;

	UPROPERTY()
	FLinearColor PrimaryColor = FLinearColor{ ForceInit };

	UPROPERTY()
	FLinearColor SecondaryColor = FLinearColor{ ForceInit };

	UPROPERTY()
	float Roughness = 0.5f;

	UPROPERTY()
	float Transparency = 0.6f;

	UPROPERTY()
	float Metalness = 0.0f;
};

USTRUCT()
struct FMetaHumanBlusherMakeupMigrationInfo
{
	GENERATED_BODY()

	UPROPERTY()
	FName Option;

	UPROPERTY()
	FLinearColor Color = FLinearColor{ ForceInit };

	UPROPERTY()
	float Intensity = 0.4f;

	UPROPERTY()
	float Roughness = 0.25f;
};

USTRUCT()
struct FMetaHumanLipsMakeupMigrationInfo
{
	GENERATED_BODY()

	UPROPERTY()
	FName Option;

	UPROPERTY()
	FLinearColor Color = FLinearColor{ ForceInit };

	UPROPERTY()
	float Roughness = 0.5f;

	UPROPERTY()
	float Transparency = 0.85f;
};

USTRUCT()
struct FMetaHumanMakeupMigrationInfo
{
	GENERATED_BODY()

public:

	UPROPERTY()
	FMetaHumanFoundationMakeupMigrationInfo Foundation;

	UPROPERTY()
	FMetaHumanEyesMakeupMigrationInfo Eyes;

	UPROPERTY()
	FMetaHumanBlusherMakeupMigrationInfo Blusher;

	UPROPERTY()
	FMetaHumanLipsMakeupMigrationInfo Lips;
};

USTRUCT()
struct FMetaHumanFaceMigrationInfo
{
	GENERATED_BODY()

	UPROPERTY()
	FMetaHumanFreckesMigrationInfo Freckles;

	UPROPERTY()
	FMetaHumanAccentRegionsMigrationInfo Accents;

	UPROPERTY()
	FMetaHumanEyeMigrationInfo LeftEye;

	UPROPERTY()
	FMetaHumanEyeMigrationInfo RightEye;

	UPROPERTY()
	FMetaHumanTeethMigrationInfo Teeth;

	UPROPERTY()
	FMetaHumanMakeupMigrationInfo Makeup;
};

USTRUCT()
struct FMetaHumanGroomMigrationInfo
{
	GENERATED_BODY()

	UPROPERTY()
	FName Option;

	UPROPERTY()
	float Length = 1.0f;

	UPROPERTY()
	bool bUseCustomProperties = false;

	UPROPERTY()
	FVector2D MelaninAndRedness = FVector2D::ZeroVector;

	UPROPERTY()
	float Roughness = 0.25f;

	UPROPERTY()
	float Whiteness = 0.0f;

	UPROPERTY()
	float Lightness = 0.5f;

	UPROPERTY()
	FName PresetOption;

	UPROPERTY()
	bool bUseDyeColor = false;

	UPROPERTY()
	FLinearColor DyeColor = FLinearColor(0.32156899571418762f, 0.0f, 0.69019597768783569f, 0.0f);

	UPROPERTY()
	bool bUseRegions = false;

	UPROPERTY()
	FVector2D RegionsUV = FVector2D(0.2819f, 0.25f);

	UPROPERTY()
	bool bUseRegionsColor = false;

	UPROPERTY()
	FLinearColor RegionsColor = FLinearColor::White;

	UPROPERTY()
	FName RegionsPresetOption;

	UPROPERTY()
	bool bUseOmbre = false;

	UPROPERTY()
	FVector2D OmbreUV = FVector2D(0.2819f, 0.25f);

	UPROPERTY()
	bool bUseOmbreColor = false;

	UPROPERTY()
	FLinearColor OmbreColor = FLinearColor::White;

	UPROPERTY()
	float OmbreShift = 0.5f;

	UPROPERTY()
	float OmbreContrast = 0.0f;

	UPROPERTY()
	float OmbreIntensity = 1.0f;

	UPROPERTY()
	FName OmbrePresetOption;

	UPROPERTY()
	bool bUseHighlights = false;

	UPROPERTY()
	FVector2D HighlightsUV = FVector2D(0.2819f, 0.25f);

	UPROPERTY()
	bool bUseHighlightsColor = false;

	UPROPERTY()
	FLinearColor HighlightsColor = FLinearColor::White;

	UPROPERTY()
	float HighlightsBlending = 0.5f;

	UPROPERTY()
	float HighlightsIntensity = 0.5f;

	UPROPERTY()
	int32 HighlightsVariation = 0;

	UPROPERTY()
	FName HighlightsPresetOption;

	UPROPERTY()
	float EyebrowsShading = 1.0f;

	UPROPERTY()
	float EyebrowsMicroblading = 1.0f;

	UPROPERTY()
	bool bUseMaskModulation = true;

	UPROPERTY()
	FName MaskModulationOption;
};

USTRUCT()
struct FMetaHumanGroomsMigrationInfo
{
	GENERATED_BODY()

	UPROPERTY()
	FMetaHumanGroomMigrationInfo Hair;

	UPROPERTY()
	FMetaHumanGroomMigrationInfo Eyebrows;

	UPROPERTY()
	FMetaHumanGroomMigrationInfo Eyelashes;

	UPROPERTY()
	FMetaHumanGroomMigrationInfo Beard;

	UPROPERTY()
	FMetaHumanGroomMigrationInfo Mustache;

	UPROPERTY()
	FMetaHumanGroomMigrationInfo Peachfuzz;
};

USTRUCT()
struct FMetaHumanBodyMigrationInfo
{
	GENERATED_BODY()

	UPROPERTY()
	EMetaHumanBodyType Type = EMetaHumanBodyType::f_med_nrw;
};

USTRUCT()
struct FMetaHumanMigrationInfo
{
	GENERATED_BODY()

	UPROPERTY()
	FMetaHumanBodyMigrationInfo Body;

	UPROPERTY()
	FMetaHumanFaceMigrationInfo Face;

	UPROPERTY()
	FMetaHumanTextureSynthesisMigrationInfo TextureSynthesis;

	UPROPERTY()
	FMetaHumanGroomsMigrationInfo Grooms;
};