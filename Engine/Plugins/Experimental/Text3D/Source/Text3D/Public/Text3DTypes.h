// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/EnumClassFlags.h"
#include "Text3DTypes.generated.h"

/** Enumerate Text3D update flags based on their priority */
enum class EText3DRendererFlags : uint8
{
	None,
	/** Update whole geometry for text */
	Geometry = 1 << 0,
	/** Update layout for characters (transform) */
	Layout = 1 << 1,
	/** Update materials slots */
	Material = 1 << 2,
	/** Update visibility/lighting properties */
	Visibility = 1 << 3,
	/** Update everything */
	All = Geometry | Layout | Visibility | Material
};

UENUM()
enum class EText3DMaterialStyle : uint8 
{
	Invalid UMETA(Hidden),
	Solid,
	Gradient,
	Texture,
	Custom
};

UENUM()
enum class EText3DMaterialBlendMode : uint8
{
	Invalid UMETA(Hidden),
	Opaque,
	Translucent
};

UENUM()
enum class EText3DFontStyleFlags : uint8
{
	None = 0,
	Monospace = 1 << 0,
	Bold = 1 << 1,
	Italic = 1 << 2,
};

UENUM()
enum class EText3DBevelType : uint8
{
	Linear,
	HalfCircle,
	Convex,
	Concave,
	OneStep,
	TwoSteps,
	Engraved
};

enum class EText3DGroupType : uint8
{
	Front = 0,
	Bevel = 1,
	Extrude = 2,
	Back = 3,

	TypeCount = 4
};

UENUM()
enum class EText3DVerticalTextAlignment : uint8
{
	FirstLine		UMETA(DisplayName = "First Line"),
	Top				UMETA(DisplayName = "Top"),
	Center			UMETA(DisplayName = "Center"),
	Bottom			UMETA(DisplayName = "Bottom"),
};

UENUM()
enum class EText3DHorizontalTextAlignment : uint8
{
	Left			UMETA(DisplayName = "Left"),
	Center			UMETA(DisplayName = "Center"),
	Right			UMETA(DisplayName = "Right"),
};

UENUM()
enum class EText3DMaxWidthHandling : uint8
{
	/** Scales the text to meet the max width */
	Scale			UMETA(DisplayName = "Scale"),
	/** First wraps the text (if possible) and then scales to meet the max width */
	WrapAndScale	UMETA(DisplayName = "Wrap and Scale"),
};

UENUM()
enum class EText3DCharacterEffectOrder : uint8
{
	Normal			UMETA(DisplayName = "Left To Right"),
	FromCenter		UMETA(DisplayName = "From Center"),
	ToCenter		UMETA(DisplayName = "To Center"),
	Opposite		UMETA(DisplayName = "Right To Left"),
};

struct FText3DWordStatistics
{
	/** Actual range taking into account whitespaces */
	FTextRange ActualRange;

	/** Render range not taking into account whitespaces */
	FTextRange RenderRange;
};

struct FText3DStatistics
{
	TArray<FText3DWordStatistics> Words;
	int32 WhiteSpaces;
};

struct FText3DFontFamily
{
	void AddFontFace(const FString& InFontFaceName, const FString& InFontFacePath)
	{
		if (FontFacePaths.Contains(InFontFaceName))
		{
			return;
		}

		FontFacePaths.Add(InFontFaceName, InFontFacePath);
	}

	/** Family these font faces belong to */
	FString FontFamilyName;

	/** Map of each font face with name -> path */
	TMap<FString, FString> FontFacePaths;
};

/** Used to identify a specific material type */
USTRUCT()
struct FText3DMaterialKey
{
	GENERATED_BODY()

	FText3DMaterialKey() = default;
	explicit FText3DMaterialKey(EText3DMaterialBlendMode InBlend, bool bInIsUnlit)
		: BlendMode(InBlend)
		, bIsUnlit(bInIsUnlit)
	{}
		
	bool operator==(const FText3DMaterialKey& Other) const
	{
		return BlendMode == Other.BlendMode
			&& bIsUnlit == Other.bIsUnlit;
	}

	bool operator!=(const FText3DMaterialKey& Other) const
	{
		return !(*this == Other);
	}
	
	friend uint32 GetTypeHash(const FText3DMaterialKey& InMaterialSettings)
	{
		return HashCombineFast(GetTypeHash(InMaterialSettings.BlendMode), GetTypeHash(InMaterialSettings.bIsUnlit));
	}

	UPROPERTY()
	EText3DMaterialBlendMode BlendMode = EText3DMaterialBlendMode::Invalid;

	UPROPERTY()
	bool bIsUnlit = false;
};

USTRUCT()
struct FText3DMaterialGroupKey
{
	GENERATED_BODY()

	FText3DMaterialGroupKey() = default;
	explicit FText3DMaterialGroupKey(FText3DMaterialKey InKey, EText3DGroupType InGroup, EText3DMaterialStyle InStyle)
		: Key(InKey)
		, Group(InGroup)
		, Style(InStyle)
	{}

	bool operator==(const FText3DMaterialGroupKey& Other) const
	{
		return Key == Other.Key
			&& Group == Other.Group
			&& Style == Other.Style;
	}

	bool operator!=(const FText3DMaterialGroupKey& Other) const
	{
		return !(*this == Other);
	}

	friend uint32 GetTypeHash(const FText3DMaterialGroupKey& InMaterialSettings)
	{
		return HashCombineFast(HashCombineFast(GetTypeHash(InMaterialSettings.Key), GetTypeHash(InMaterialSettings.Group)), GetTypeHash(InMaterialSettings.Style));
	}

	UPROPERTY()
	FText3DMaterialKey Key;

	EText3DGroupType Group = EText3DGroupType::Front;

	UPROPERTY()
	EText3DMaterialStyle Style = EText3DMaterialStyle::Invalid;
};

struct FText3DTypeFaceMetrics
{
	float FontHeight = 0.f;
	/** Height above baseline */
	float FontAscender = 0.f;
	/** Height below baseline */
	float FontDescender = 0.f;
};

namespace UE::Text3D::Metrics
{
	/** Original value to match size across different Text3D version */
	static constexpr float FontSize = 64;
	/** Value used to match size across Text3D implementation */
	static constexpr float SlateFontSize = 48;
	/** Scale used to transform freetype face result to get normalized values */
	static constexpr float FontSizeInverse = 1.0f / FontSize;
	/** DPI used to match slate */
	static constexpr float FontDPI = 96.f;
}