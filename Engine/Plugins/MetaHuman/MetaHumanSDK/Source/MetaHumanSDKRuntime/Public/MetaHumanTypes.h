// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "Misc/EnumRange.h"
#include "MetaHumanTypes.generated.h"

// Common data types used in various parts of the MetaHuman SDK


/**
 * This enum defines the Texture objects stored in a MetaHumanCharacter by indexing the SynthesizedFaceTextures array.
 * The values match the names of the material slots for the MetaHumanCharacter Editable preview material, and
 * correspond to the expected outputs of the TS model in the TitanLib module.
 */
UENUM()
enum class EFaceTextureType : uint8
{
	Basecolor = 0				UMETA(DisplayName = "Base Color"),
	Basecolor_Animated_CM1		UMETA(DisplayName = "Base Color Animated CM1"),
	Basecolor_Animated_CM2		UMETA(DisplayName = "Base Color Animated CM2"),
	Basecolor_Animated_CM3		UMETA(DisplayName = "Base Color Animated CM3"),

	Normal,
	Normal_Animated_WM1		UMETA(DisplayName = "Normal Animated WM1"),
	Normal_Animated_WM2		UMETA(DisplayName = "Normal Animated WM2"),
	Normal_Animated_WM3		UMETA(DisplayName = "Normal Animated WM3"),

	Cavity,

	Count UMETA(Hidden)
};
ENUM_RANGE_BY_COUNT(EFaceTextureType, EFaceTextureType::Count);

UENUM()
enum class EBodyTextureType : uint8
{
	Body_Basecolor = 0				UMETA(DisplayName = "Body Base Color"),
	Body_Normal						UMETA(DisplayName = "Body Normal"),
	Body_Cavity						UMETA(DisplayName = "Body Cavity"),
	Body_Underwear_Basecolor		UMETA(DisplayName = "Body Underwear Base Color"),
	Body_Underwear_Normal			UMETA(DisplayName = "Body Underwear Normal"),
	Body_Underwear_Mask				UMETA(DisplayName = "Body Underwear Mask"),
	Chest_Basecolor					UMETA(DisplayName = "Chest Base Color"),
	Chest_Normal					UMETA(DisplayName = "Chest Normal"),
	Chest_Cavity					UMETA(DisplayName = "Chest Cavity"),
	Chest_Underwear_Basecolor		UMETA(DisplayName = "Chest Underwear Base Color"),
	Chest_Underwear_Normal			UMETA(DisplayName = "Chest Underwear Normal"),

	Count UMETA(Hidden)
};ENUM_RANGE_BY_COUNT(EBodyTextureType, EBodyTextureType::Count);

UENUM()
enum class EMetaHumanQualityLevel : uint8
{
	Low,
	Medium,
	High,
	Cinematic,

	Count UMETA(Hidden)
};
ENUM_RANGE_BY_COUNT(EMetaHumanQualityLevel, EMetaHumanQualityLevel::Count);