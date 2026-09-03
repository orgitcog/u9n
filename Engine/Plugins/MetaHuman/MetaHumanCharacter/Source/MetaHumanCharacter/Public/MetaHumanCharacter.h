// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "Animation/AnimSequence.h"
#include "Animation/PoseAsset.h"
#include "Serialization/EditorBulkData.h"
#include "Misc/EnumRange.h"
#include "ImageCore.h"
#include "MetaHumanCharacterPipeline.h"
#include "MetaHumanTypes.h"
#include "Misc/ObjectThumbnail.h"
#include "MetaHumanCharacter.generated.h"

class UMetaHumanCollection;
class UMetaHumanCollectionPipeline;

/**
* The rigging state of the Character
*/
enum class EMetaHumanCharacterRigState : uint8
{
	Unrigged = 0,
	RigPending,
	Rigged
};

/**
 * Configures single section of the wardrobe asset view.
 */
USTRUCT()
struct METAHUMANCHARACTER_API FMetaHumanCharacterAssetsSection
{
	GENERATED_BODY()

	/** Long package directory name where to look for the assets */
	UPROPERTY(EditAnywhere, Category = "Section", meta = (LongPackageName))
	FDirectoryPath ContentDirectoryToMonitor;

	/** Palette slot to target when the asset from this section is added. */
	UPROPERTY(EditAnywhere, Category = "Section")
	FName SlotName;

	/** Specifies the list of classes to look for in the given directory */
	UPROPERTY(EditAnywhere, Category = "Section")
	TArray<TSubclassOf<UObject>> ClassesToFilter;

	/** True if this section should be considered a pure virtual folder */
	UPROPERTY()
	bool bPureVirtual = false;

	bool operator==(const FMetaHumanCharacterAssetsSection& Other) const
	{
		return 
			ContentDirectoryToMonitor.Path == Other.ContentDirectoryToMonitor.Path && 
			SlotName == Other.SlotName &&
			ClassesToFilter == Other.ClassesToFilter;
	}
};

USTRUCT()
struct METAHUMANCHARACTER_API FMetaHumanCharacterWardrobeIndividualAssets
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Section")
	TArray<TSoftObjectPtr<class UMetaHumanWardrobeItem>> Items;
};

USTRUCT()
struct METAHUMANCHARACTER_API FMetaHumanCharacterIndividualAssets
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Section")
	TArray<TSoftObjectPtr<UMetaHumanCharacter>> Characters;
};

UENUM()
enum class EMetaHumanCharacterTemplateType : uint8
{
	MetaHuman,
};

USTRUCT()
struct FMetaHumanCharacterFaceEvaluationSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Face", meta = (UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "1"))
	float GlobalDelta = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Face", meta = (UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "1"))
	float HighFrequencyDelta = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Face", meta = (UIMin = "0.8", UIMax = "1.3", ClampMin = "0.8", ClampMax = "1.3"))
	float HeadScale = 1.0f;

	bool operator==(const FMetaHumanCharacterFaceEvaluationSettings& InOther) const
	{
		return GlobalDelta == InOther.GlobalDelta &&
			HighFrequencyDelta == InOther.HighFrequencyDelta &&
			HeadScale == InOther.HeadScale;
	}

	bool operator!=(const FMetaHumanCharacterFaceEvaluationSettings& InOther) const
	{
		return !(*this == InOther);
	}
};


USTRUCT()
struct FMetaHumanCharacterSkinProperties
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Skin", meta = (UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "1"))
	float U = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Skin", meta = (UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "1"))
	float V = 0.5f;

	UPROPERTY(VisibleAnywhere, Category = "Skin")
	FVector3f BodyBias = {74.f, 28.f, 15.f};

	UPROPERTY(VisibleAnywhere, Category = "Skin")
	FVector3f BodyGain= {30.f, 10.f, 5.f};

	UPROPERTY(EditAnywhere, Category = "Skin")
	bool bShowTopUnderwear = true;

	UPROPERTY(EditAnywhere, Category = "Skin", meta = (UIMin = "0", UIMax = "8", ClampMin = "0", ClampMax = "8"))
	int32 BodyTextureIndex = 0;

	UPROPERTY(EditAnywhere, Category = "Skin", meta = (UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "1"))
	int32 FaceTextureIndex = 0;

	UPROPERTY(EditAnywhere, Category = "Skin")
	float Roughness = 1.06f;

	bool operator==(const FMetaHumanCharacterSkinProperties& InOther) const
	{
		return U == InOther.U &&
			V == InOther.V &&
			BodyTextureIndex == InOther.BodyTextureIndex &&
			FaceTextureIndex == InOther.FaceTextureIndex &&
			Roughness == InOther.Roughness;
	}

	bool operator!=(const FMetaHumanCharacterSkinProperties& InOther) const
	{
		return !(*this == InOther);
	}
};

UENUM()
enum class EMetaHumanCharacterFrecklesMask : uint8
{
	None,
	Type1,
	Type2,
	Type3,

	Count UMETA(Hidden)
};
ENUM_RANGE_BY_COUNT(EMetaHumanCharacterFrecklesMask, EMetaHumanCharacterFrecklesMask::Count);

USTRUCT()
struct FMetaHumanCharacterFrecklesProperties
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Freckles", meta = (UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "1"))
	float Density = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Freckles", meta = (UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "1"))
	float Strength = 0.2f;

	UPROPERTY(EditAnywhere, Category = "Freckles", meta = (UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "1"))
	float Saturation = 0.6f;

	UPROPERTY(EditAnywhere, Category = "Freckles", meta = (UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "1"))
	float ToneShift = 0.65f;

	UPROPERTY(EditAnywhere, Category = "Freckles")
	EMetaHumanCharacterFrecklesMask Mask = EMetaHumanCharacterFrecklesMask::None;
};

USTRUCT()
struct FMetaHumanCharacterAccentRegionProperties
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Accents", meta = (UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "1"))
	float Redness = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Accents", meta = (UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "1"))
	float Saturation = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Accents", meta = (UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "1"))
	float Lightness = 0.5f;
};

USTRUCT()
struct FMetaHumanCharacterAccentRegions
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Accents")
	FMetaHumanCharacterAccentRegionProperties Scalp;

	UPROPERTY(EditAnywhere, Category = "Accents")
	FMetaHumanCharacterAccentRegionProperties Forehead;

	UPROPERTY(EditAnywhere, Category = "Accents")
	FMetaHumanCharacterAccentRegionProperties Nose;

	UPROPERTY(EditAnywhere, Category = "Accents")
	FMetaHumanCharacterAccentRegionProperties UnderEye;

	UPROPERTY(EditAnywhere, Category = "Accents")
	FMetaHumanCharacterAccentRegionProperties Cheeks;

	UPROPERTY(EditAnywhere, Category = "Accents")
	FMetaHumanCharacterAccentRegionProperties Lips;

	UPROPERTY(EditAnywhere, Category = "Accents")
	FMetaHumanCharacterAccentRegionProperties Chin;

	UPROPERTY(EditAnywhere, Category = "Accents")
	FMetaHumanCharacterAccentRegionProperties Ears;
};

UENUM()
enum class EMetaHumanCharacterSkinPreviewMaterial : uint8
{
	Default		UMETA(DisplayName = "Topology"),
	Editable	UMETA(DisplayName = "Skin"),
	Clay		UMETA(DisplayName = "Clay"),
	Count UMETA(Hidden)
};
ENUM_RANGE_BY_COUNT(EMetaHumanCharacterSkinPreviewMaterial, EMetaHumanCharacterSkinPreviewMaterial::Count);

/**
 * Struct used to serialize information about a synthesized texture
 */
USTRUCT()
struct FMetaHumanCharacterTextureInfo
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "Textures")
	int32 SizeX = 0;

	UPROPERTY(VisibleAnywhere, Category = "Textures")
	int32 SizeY = 0;

	UPROPERTY(VisibleAnywhere, Category = "Textures")
	int32 NumSlices = 0;

	UPROPERTY(VisibleAnywhere, Category = "Textures")
	uint8 Format = ERawImageFormat::BGRA8;

	UPROPERTY(VisibleAnywhere, Category = "Textures")
	uint8 GammaSpace = (uint8) EGammaSpace::sRGB;

	void Init(const FImageInfo& InImageInfo)
	{
		SizeX = InImageInfo.SizeX;
		SizeY = InImageInfo.SizeY;
		NumSlices = InImageInfo.NumSlices;
		Format = InImageInfo.Format;
		GammaSpace = (uint8) InImageInfo.GammaSpace;
	}

	FImage GetBlankImage() const
	{
		FImage Result;
		Result.Init(ToImageInfo());

		return Result;
	}

	FImageInfo ToImageInfo() const
	{
		return FImageInfo(SizeX, SizeY, NumSlices, (ERawImageFormat::Type) Format, (EGammaSpace) GammaSpace);
	}
};

/**
 * Struct that hard references to all possible textures used in the skin material.
 * This is also used as a utility to pass around skin textures sets
 */
USTRUCT()
struct METAHUMANCHARACTER_API FMetaHumanCharacterSkinTextureSet
{
	GENERATED_BODY()

	UPROPERTY()
	TMap<EFaceTextureType, TObjectPtr<class UTexture2D>> Face;

	UPROPERTY()
	TMap<EBodyTextureType, TObjectPtr<class UTexture2D>> Body;

	/**
	 * Appends another texture set to this one.
	 * Replaces or adds any new textures from InOther
	 */
	void Append(const FMetaHumanCharacterSkinTextureSet& InOther);
};

/**
 * Struct used to hold soft references to a skin texture set. This is
 * used to store override textures in the MetaHuman Character object
 * which are not loaded by default.
 */
USTRUCT()
struct METAHUMANCHARACTER_API FMetaHumanCharacterSkinTextureSoftSet
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Face")
	TMap<EFaceTextureType, TSoftObjectPtr<class UTexture2D>> Face;

	UPROPERTY(EditAnywhere, Category = "Body")
	TMap<EBodyTextureType, TSoftObjectPtr<class UTexture2D>> Body;

	/**
	 * Load the textures and returns a texture set
	 */
	FMetaHumanCharacterSkinTextureSet LoadTextureSet() const;
};

UENUM()
enum class EMetaHumanCharacterEyelashesType : uint8
{
	None,
	Sparse,
	ShortFine,
	Thin,
	SlightCurl,
	LongCurl,
	ThickCurl,

	Count UMETA(Hidden)
};
ENUM_RANGE_BY_COUNT(EMetaHumanCharacterEyelashesType, EMetaHumanCharacterEyelashesType::Count);

USTRUCT()
struct FMetaHumanCharacterEyelashesProperties
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Eyelashes")
	EMetaHumanCharacterEyelashesType Type = EMetaHumanCharacterEyelashesType::None;

	UPROPERTY(EditAnywhere, Category = "Eyelashes", meta = (HideAlphaChannel))
	FLinearColor DyeColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, Category = "Eyelashes", meta = (UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "1"))
	float Melanin = 0.3f;

	UPROPERTY(EditAnywhere, Category = "Eyelashes", meta = (UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "1"))
	float Redness = 0.28f;

	UPROPERTY(EditAnywhere, Category = "Eyelashes", meta = (UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "1"))
	float Roughness = 0.25f;

	UPROPERTY(EditAnywhere, Category = "Eyelashes", meta = (UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "1"))
	float SaltAndPepper = 0.20f;

	UPROPERTY(EditAnywhere, Category = "Eyelashes", meta = (UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "1"))
	float Lightness = 0.50f;

	UPROPERTY(EditAnywhere, Category = "Eyelashes")
	bool bEnableGrooms = true;

	bool operator==(const FMetaHumanCharacterEyelashesProperties& InOther) const
	{
		return Type == InOther.Type &&
			DyeColor == InOther.DyeColor &&
			Melanin == InOther.Melanin &&
			Redness == InOther.Redness &&
			Roughness == InOther.Roughness &&
			SaltAndPepper == InOther.SaltAndPepper &&
			Lightness == InOther.Lightness &&
			bEnableGrooms == InOther.bEnableGrooms;
	}

	bool operator!=(const FMetaHumanCharacterEyelashesProperties& InOther) const
	{
		return !(*this == InOther);
	}

	bool AreMaterialsUpdated(const FMetaHumanCharacterEyelashesProperties& InOther) const
	{
		return !(DyeColor == InOther.DyeColor &&
			Melanin == InOther.Melanin &&
			Redness == InOther.Redness &&
			Roughness == InOther.Roughness);
	}
};

UENUM()
enum class EMetaHumanCharacterTeethType : uint8
{
	// TODO names may change; this is how it is in titan currently
	None, 
	Variant_01,
	Variant_02,
	Variant_03,
	Variant_04,
	Variant_05,
	Variant_06,
	Variant_07,
	Variant_08,

	Count UMETA(Hidden)
};
ENUM_RANGE_BY_COUNT(EMetaHumanCharacterTeethType, EMetaHumanCharacterTeethType::Count);

USTRUCT()
struct FMetaHumanCharacterTeethProperties
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Teeth", meta = (UIMin = "-1", UIMax = "1", ClampMin = "-1", ClampMax = "1"))
	float ToothLength = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Teeth", meta = (UIMin = "-1", UIMax = "1", ClampMin = "-1", ClampMax = "1"))
	float ToothSpacing = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Teeth", meta = (UIMin = "-1", UIMax = "1", ClampMin = "-1", ClampMax = "1"))
	float UpperShift = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Teeth", meta = (UIMin = "-1", UIMax = "1", ClampMin = "-1", ClampMax = "1"))
	float LowerShift = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Teeth", meta = (UIMin = "-1", UIMax = "1", ClampMin = "-1", ClampMax = "1"))
	float Overbite = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Teeth", meta = (UIMin = "-1", UIMax = "1", ClampMin = "-1", ClampMax = "1"))
	float Overjet = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Teeth", meta = (UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "1"))
	float WornDown = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Teeth", meta = (UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "1"))
	float Polycanine = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Teeth", meta = (UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "1"))
	float RecedingGums = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Teeth", meta = (UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "1"))
	float Narrowness = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Teeth", meta = (UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "1"))
	float Variation = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Teeth", meta = (UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "1"))
	float JawOpen = 0.0f;
	
	UPROPERTY(EditAnywhere, Category = "Teeth", meta = (HideAlphaChannel))
	FLinearColor TeethColor = FLinearColor::White;
	
	UPROPERTY(EditAnywhere, Category = "Teeth", meta = (HideAlphaChannel))
	FLinearColor GumColor = FLinearColor::White;
	
	UPROPERTY(EditAnywhere, Category = "Teeth", meta = (HideAlphaChannel))
	FLinearColor PlaqueColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, Category = "Teeth", meta = (UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "1"))
	float PlaqueAmount = 0.0f;

	UPROPERTY(Transient)	
	bool EnableShowTeethExpression = false;

	bool operator==(const FMetaHumanCharacterTeethProperties& InOther) const
	{
		return ToothLength == InOther.ToothLength &&
			ToothSpacing == InOther.ToothSpacing &&
			UpperShift == InOther.UpperShift &&
			LowerShift == InOther.LowerShift &&
			Overbite == InOther.Overbite &&
			Overjet == InOther.Overjet &&
			WornDown == InOther.WornDown &&
			Polycanine == InOther.Polycanine &&
			RecedingGums == InOther.RecedingGums &&
			Narrowness == InOther.Narrowness &&
			Variation == InOther.Variation &&
			JawOpen == InOther.JawOpen &&
			TeethColor == InOther.TeethColor &&
			GumColor == InOther.GumColor &&
			PlaqueColor == InOther.PlaqueColor &&
			PlaqueAmount == InOther.PlaqueAmount &&
			EnableShowTeethExpression == InOther.EnableShowTeethExpression;
	}

	bool operator!=(const FMetaHumanCharacterTeethProperties& InOther) const
	{
		return !(*this == InOther);
	}

	bool AreMaterialsUpdated(const FMetaHumanCharacterTeethProperties& InOther) const
	{
		return !(TeethColor == InOther.TeethColor &&
			GumColor == InOther.GumColor &&
			PlaqueColor == InOther.PlaqueColor &&
			PlaqueAmount == InOther.PlaqueAmount);
	}

	bool IsVariantUpdated(const FMetaHumanCharacterTeethProperties& InOther) const
	{
		return !(ToothLength == InOther.ToothLength &&
			ToothSpacing == InOther.ToothSpacing &&
			UpperShift == InOther.UpperShift &&
			LowerShift == InOther.LowerShift &&
			Overbite == InOther.Overbite &&
			Overjet == InOther.Overjet &&
			WornDown == InOther.WornDown &&
			Polycanine == InOther.Polycanine &&
			RecedingGums == InOther.RecedingGums &&
			Narrowness == InOther.Narrowness &&
			Variation == InOther.Variation &&
			JawOpen == InOther.JawOpen);
	}
};

USTRUCT()
struct METAHUMANCHARACTER_API FMetaHumanCharacterHeadModelSettings
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, Category = "Eyelashes", meta = (ShowOnlyInnerProperties))
	FMetaHumanCharacterEyelashesProperties Eyelashes;

	UPROPERTY(EditAnywhere, Category = "Teeth", meta = (ShowOnlyInnerProperties))
	FMetaHumanCharacterTeethProperties Teeth;
};

USTRUCT()
struct METAHUMANCHARACTER_API FMetaHumanCharacterSkinSettings
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, Category = "Skin", meta = (ShowOnlyInnerProperties))
	FMetaHumanCharacterSkinProperties Skin;

	UPROPERTY(EditAnywhere, Category = "Freckles", meta = (ShowOnlyInnerProperties))
	FMetaHumanCharacterFrecklesProperties Freckles;

	UPROPERTY(EditAnywhere, Category = "Accents", meta = (ShowOnlyInnerProperties))
	FMetaHumanCharacterAccentRegions Accents;

	// Enables the use texture overrides in the skin material
	UPROPERTY(EditAnywhere, Category = "Texture Overrides")
	bool bEnableTextureOverrides = false;

	// If bEnableTextureOverrides is enabled, use textures in this texture set as textures of the skin material
	UPROPERTY(EditAnywhere, Category = "Texture Overrides")
	FMetaHumanCharacterSkinTextureSoftSet TextureOverrides;

	/**
	 * Returns a texture set considering the bEnableTextureOverrides flag. If the flag is enabled any texture
	 * in TextureOverrides are going to be present in the returned texture set
	 */
	FMetaHumanCharacterSkinTextureSet GetFinalSkinTextureSet(const FMetaHumanCharacterSkinTextureSet& InSkinTextureSet) const;
};

UENUM()
enum class EMetaHumanCharacterEyesBlendMethod : uint8
{
	Radial = 0,
	Structural = 1
};

UENUM()
enum class EMetaHumanCharacterEyesIrisPattern : uint8
{
	Iris001 = 0,
	Iris002,
	Iris003,
	Iris004,
	Iris005,
	Iris006,
	Iris007,
	Iris008,
	Iris009,

	Count UMETA(Hidden)
};
ENUM_RANGE_BY_COUNT(EMetaHumanCharacterEyesIrisPattern, EMetaHumanCharacterEyesIrisPattern::Count);

USTRUCT()
struct FMetaHumanCharacterEyeIrisProperties
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Iris")
	EMetaHumanCharacterEyesIrisPattern IrisPattern = EMetaHumanCharacterEyesIrisPattern::Iris001;

	UPROPERTY(EditAnywhere, Category = "Iris", meta = (UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "1"))
	float IrisRotation = 0.0f;	

	UPROPERTY(EditAnywhere, Category = "Iris", meta = (UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "1"))
	float PrimaryColorU = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Iris", meta = (UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "1"))
	float PrimaryColorV = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Iris", meta = (UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "1"))
	float SecondaryColorU = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Iris", meta = (UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "1"))
	float SecondaryColorV = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Iris", meta = (UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "1"))
	float ColorBlend = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Iris")
	float ColorBlendSoftness = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Iris")
	EMetaHumanCharacterEyesBlendMethod BlendMethod = EMetaHumanCharacterEyesBlendMethod::Structural;

	UPROPERTY(EditAnywhere, Category = "Iris", meta = (UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "1"))
	float ShadowDetails = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Iris")
	float LimbalRingSize = 0.725f;

	UPROPERTY(EditAnywhere, Category = "Iris")
	float LimbalRingSoftness = 0.085f;

	UPROPERTY(EditAnywhere, Category = "Iris", meta = (HideAlphaChannel))
	FLinearColor LimbalRingColor = FLinearColor::Black;

	UPROPERTY(EditAnywhere, Category = "Iris")
	float GlobalSaturation = 2.0f;

	UPROPERTY(EditAnywhere, Category = "Iris", meta = (HideAlphaChannel))
	FLinearColor GlobalTint = FLinearColor::White;
};

USTRUCT()
struct FMetaHumanCharacterEyePupilProperties
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Pupil")
	float Dilation = 1.025f;

	UPROPERTY(EditAnywhere, Category = "Pupil")
	float Feather = 0.8f;
};

USTRUCT()
struct FMetaHumanCharacterEyeScleraProperties
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Sclera", meta = (UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "1"))
	float Rotation = 0.0f;

	
	// If enabled allows the use of a custom sclera tint value.
	// If disabled, the Sclera tint will be calculated based on the Skin Tone
	UPROPERTY(EditAnywhere, Category = "Sclera")
	bool bUseCustomTint = false;

	UPROPERTY(EditAnywhere, Category = "Sclera", meta = (HideAlphaChannel, EditCondition = "bUseCustomTint"))
	FLinearColor Tint = FLinearColor::White;

	UPROPERTY(EditAnywhere, Category = "Sclera")
	float TransmissionSpread = 0.115f;

	UPROPERTY(EditAnywhere, Category = "Sclera")
	FLinearColor TransmissionColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, Category = "Sclera")
	float VascularityIntensity = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Sclera")
	float VascularityCoverage = 0.2f;
};

USTRUCT()
struct FMetaHumanCharacterEyeCorneaProperties
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Cornea")
	float Size = 0.165f;

	UPROPERTY(EditAnywhere, Category = "Cornea")
	float LimbusSoftness = 0.09f;

	UPROPERTY(EditAnywhere, Category = "Cornea", meta = (HideAlphaChannel))
	FLinearColor LimbusColor = FLinearColor::White;
};

USTRUCT()
struct METAHUMANCHARACTER_API FMetaHumanCharacterEyeProperties
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Iris", meta = (ShowOnlyInnerProperties))
	FMetaHumanCharacterEyeIrisProperties Iris;

	UPROPERTY(EditAnywhere, Category = "Pupil", meta = (ShowOnlyInnerProperties))
	FMetaHumanCharacterEyePupilProperties Pupil;

	UPROPERTY(EditAnywhere, Category = "Cornea", meta = (ShowOnlyInnerProperties))
	FMetaHumanCharacterEyeCorneaProperties Cornea;

	UPROPERTY(EditAnywhere, Category = "Sclera", meta = (ShowOnlyInnerProperties))
	FMetaHumanCharacterEyeScleraProperties Sclera;
};

USTRUCT()
struct METAHUMANCHARACTER_API FMetaHumanCharacterEyesSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Eye", meta = (ShowOnlyInnerProperties))
	FMetaHumanCharacterEyeProperties EyeLeft;

	UPROPERTY(EditAnywhere, Category = "Eye", meta = (ShowOnlyInnerProperties))
	FMetaHumanCharacterEyeProperties EyeRight;
};


USTRUCT()
struct FMetaHumanCharacterFoundationMakeupProperties
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Foundation")
	bool bApplyFoundation = false;

	UPROPERTY(EditAnywhere, Category = "Foundation", meta = (HideAlphaChannel, EditCondition = "bApplyFoundation"))
	FLinearColor Color = FLinearColor{ ForceInit };

	UPROPERTY(EditAnywhere, Category = "Foundation", meta = (UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "1", EditCondition = "bApplyFoundation"))
	float Intensity = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Foundation", meta = (UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "1", EditCondition = "bApplyFoundation"))
	float Roughness = 0.65f;

	UPROPERTY(EditAnywhere, Category = "Foundation", meta = (UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "1", EditCondition = "bApplyFoundation"))
	float Concealer = 0.57f;
};

UENUM()
enum class EMetaHumanCharacterEyeMakeupType : uint8
{
	None,
	ThinLiner,
	SoftSmokey,
	FullThinLiner,
	CatEye,
	PandaSmudge,
	DramaticSmudge,
	DoubleMod,
	ClassicBar,

	Count UMETA(Hidden)
};
ENUM_RANGE_BY_COUNT(EMetaHumanCharacterEyeMakeupType, EMetaHumanCharacterEyeMakeupType::Count);

USTRUCT()
struct FMetaHumanCharacterEyeMakeupProperties
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Eyes")
	EMetaHumanCharacterEyeMakeupType Type = EMetaHumanCharacterEyeMakeupType::None;

	UPROPERTY(EditAnywhere, Category = "Eyes", meta = (HideAlphaChannel, EditCondition = "Type != EMetaHumanCharacterEyeMakeupType::None"))
	FLinearColor PrimaryColor = FLinearColor{ 0.086f, 0.013f, 0.004f, 1.0f };

	UPROPERTY(EditAnywhere, Category = "Eyes", meta = (HideAlphaChannel, EditCondition = "Type != EMetaHumanCharacterEyeMakeupType::None"))
	FLinearColor SecondaryColor = FLinearColor::Black;

	UPROPERTY(EditAnywhere, Category = "Eyes", meta = (UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "1", EditCondition = "Type != EMetaHumanCharacterEyeMakeupType::None"))
	float Roughness = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Eyes", meta = (UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "1", EditCondition = "Type != EMetaHumanCharacterEyeMakeupType::None"))
	float Opacity = 0.6f;

	UPROPERTY(EditAnywhere, Category = "Eyes", meta = (UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "1", EditCondition = "Type != EMetaHumanCharacterEyeMakeupType::None"))
	float Metalness = 0.0f;
};

UENUM()
enum class EMetaHumanCharacterBlushMakeupType : uint8
{
	None,
	Angled,
	Apple,
	LowSweep,
	HighCurve,

	Count UMETA(Hidden)
};
ENUM_RANGE_BY_COUNT(EMetaHumanCharacterBlushMakeupType, EMetaHumanCharacterBlushMakeupType::Count);

USTRUCT()
struct FMetaHumanCharacterBlushMakeupProperties
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Blush")
	EMetaHumanCharacterBlushMakeupType Type = EMetaHumanCharacterBlushMakeupType::None;

	UPROPERTY(EditAnywhere, Category = "Blush", meta = (HideAlphaChannel, EditCondition = "Type != EMetaHumanCharacterBlushMakeupType::None"))
	FLinearColor Color = FLinearColor{ 0.224f, 0.011f, 0.02f, 1.0f };

	UPROPERTY(EditAnywhere, Category = "Blush", meta = (UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "1", EditCondition = "Type != EMetaHumanCharacterBlushMakeupType::None"))
	float Intensity = 0.4f;

	UPROPERTY(EditAnywhere, Category = "Blush", meta = (UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "1", EditCondition = "Type != EMetaHumanCharacterBlushMakeupType::None"))
	float Roughness = 0.6f;
};

UENUM()
enum class EMetaHumanCharacterLipsMakeupType : uint8
{
	None,
	Natural,
	Hollywood,
	Cupid,

	Count UMETA(Hidden)
};
ENUM_RANGE_BY_COUNT(EMetaHumanCharacterLipsMakeupType, EMetaHumanCharacterLipsMakeupType::Count)

USTRUCT()
struct FMetaHumanCharacterLipsMakeupProperties
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Lips")
	EMetaHumanCharacterLipsMakeupType Type = EMetaHumanCharacterLipsMakeupType::None;

	UPROPERTY(EditAnywhere, Category = "Lips", meta = (HideAlphaChannel, EditCondition = "Type != EMetaHumanCharacterLipsMakeupType::None"))
	FLinearColor Color = FLinearColor{ 0.22f, 0.011f, 0.02f, 1.0f };

	UPROPERTY(EditAnywhere, Category = "Lips", meta = (UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "1", EditCondition = "Type != EMetaHumanCharacterLipsMakeupType::None"))
	float Roughness = 0.25f;

	UPROPERTY(EditAnywhere, Category = "Lips", meta = (UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "1", EditCondition = "Type != EMetaHumanCharacterLipsMakeupType::None"))
	float Opacity = 0.85f;

	UPROPERTY(EditAnywhere, Category = "Blush", meta = (UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "1", EditCondition = "Type != EMetaHumanCharacterLipsMakeupType::None"))
	float Metalness = 1.0f;
};

USTRUCT()
struct FMetaHumanCharacterMakeupSettings
{
	GENERATED_BODY()

public:

	UPROPERTY(VisibleAnywhere, Category = "Foundation", meta = (ShowOnlyInnerProperties))
	FMetaHumanCharacterFoundationMakeupProperties Foundation;

	UPROPERTY(VisibleAnywhere, Category = "Eyes", meta = (ShowOnlyInnerProperties))
	FMetaHumanCharacterEyeMakeupProperties Eyes;

	UPROPERTY(VisibleAnywhere, Category = "Blush", meta = (ShowOnlyInnerProperties))
	FMetaHumanCharacterBlushMakeupProperties Blush;

	UPROPERTY(VisibleAnywhere, Category = "Lips", meta = (ShowOnlyInnerProperties))
	FMetaHumanCharacterLipsMakeupProperties Lips;
};

UENUM()
enum class EMetaHumanCharacterEnvironment : uint8
{
	Studio,
	Split,
	Fireside,
	Moonlight,
	Tungsten,
	Portrait,
	RedLantern,
	TextureBooth,

	Count UMETA(Hidden)
};
ENUM_RANGE_BY_COUNT(EMetaHumanCharacterEnvironment, EMetaHumanCharacterEnvironment::Count);

UENUM()
enum class EMetaHumanCharacterLOD : uint8
{
	LOD0,
	LOD1,
	LOD2,
	LOD3,
	LOD4,
	LOD5,
	LOD6,
	LOD7,
	Auto,
	Count UMETA(Hidden)
};
ENUM_RANGE_BY_COUNT(EMetaHumanCharacterLOD, EMetaHumanCharacterLOD::Count);

UENUM()
enum class EMetaHumanCharacterCameraFrame : uint8
{
	Auto,
	Face,
	Body,
	Far,
	Count UMETA(Hidden)
};
ENUM_RANGE_BY_COUNT(EMetaHumanCharacterCameraFrame, EMetaHumanCharacterCameraFrame::Count);

UENUM()
enum class EMetaHumanCharacterRenderingQuality : uint8
{
	Medium,
	High,
	Epic,
	Count UMETA(Hidden)
};
ENUM_RANGE_BY_COUNT(EMetaHumanCharacterRenderingQuality, EMetaHumanCharacterRenderingQuality::Count);


USTRUCT()
struct FMetaHumanCharacterViewportSettings
{
	GENERATED_BODY()
	
public:

	UPROPERTY(VisibleAnywhere, Category = "Viewport")
	EMetaHumanCharacterEnvironment CharacterEnvironment = EMetaHumanCharacterEnvironment::Studio;

	UPROPERTY(VisibleAnywhere, Category = "Viewport")
	FLinearColor BackgroundColor = FLinearColor::White;

	UPROPERTY(VisibleAnywhere, Category = "Viewport", meta = (UIMin = "-270", UIMax = "270", ClampMin = "-270", ClampMax = "270"))
	float LightRotation = 0.0f;

	UPROPERTY(VisibleAnywhere, Category = "Viewport")
	bool bTonemapperEnabled = true;

	UPROPERTY(VisibleAnywhere, Category = "Viewport")
	EMetaHumanCharacterLOD LevelOfDetail = EMetaHumanCharacterLOD::LOD0;

	UPROPERTY(VisibleAnywhere, Category = "Viewport")
	EMetaHumanCharacterCameraFrame CameraFrame = EMetaHumanCharacterCameraFrame::Auto;

	UPROPERTY(VisibleAnywhere, Category = "Viewport")
	EMetaHumanCharacterRenderingQuality RenderingQuality = EMetaHumanCharacterRenderingQuality::Epic;

	UPROPERTY(VisibleAnywhere, Category = "Viewport")
	bool bAlwaysUseHairCards = false;

	UPROPERTY(VisibleAnywhere, Category = "Viewport")
	bool bShowViewportOverlays = true;
};

UENUM()
enum class EMetaHumanCharacterSkinMaterialSlot : uint8
{
	LOD0 = 0,
	LOD1,
	LOD2,
	LOD3,
	LOD4,
	LOD5to7,

	Count UMETA(Hidden)
};
ENUM_RANGE_BY_COUNT(EMetaHumanCharacterSkinMaterialSlot, EMetaHumanCharacterSkinMaterialSlot::Count);

USTRUCT()
struct METAHUMANCHARACTER_API FMetaHumanCharacterFaceMaterialSet
{
	GENERATED_BODY()

	UPROPERTY()
	TMap<EMetaHumanCharacterSkinMaterialSlot, TObjectPtr<class UMaterialInstance>> Skin;

	UPROPERTY()
	TObjectPtr<class UMaterialInstance> EyeLeft;

	UPROPERTY()
	TObjectPtr<class UMaterialInstance> EyeRight;

	UPROPERTY()
	TObjectPtr<class UMaterialInstance> EyeShell;

	UPROPERTY()
	TObjectPtr<class UMaterialInstance> LacrimalFluid;

	UPROPERTY()
	TObjectPtr<class UMaterialInstance> Teeth;

	UPROPERTY()
	TObjectPtr<class UMaterialInstance> Eyelashes;

	UPROPERTY()
	TObjectPtr<class UMaterialInstance> EyelashesHiLods;

	/**
	 * Utility to iterate over all the skin materials casting them to a particular type
	 */
	template<typename MaterialType>
	void ForEachSkinMaterial(TFunction<void(EMetaHumanCharacterSkinMaterialSlot, MaterialType*)> InCallback) const
	{
		for (const TPair<EMetaHumanCharacterSkinMaterialSlot, TObjectPtr<class UMaterialInstance>>& SkinMaterialPair : Skin)
		{
			const EMetaHumanCharacterSkinMaterialSlot Slot = SkinMaterialPair.Key;
			class UMaterialInstance* Material = SkinMaterialPair.Value;

			if (MaterialType* SkinMaterial = Cast<MaterialType>(Material))
			{
				InCallback(Slot, SkinMaterial);
			}
		}
	}

	template<typename MaterialType>
	void ForEachEyelashMaterial(TFunction<void(MaterialType*)> InCallback) const
	{
		if (MaterialType* EyelashMaterial = Cast<MaterialType>(Eyelashes))
		{
			InCallback(EyelashMaterial);
		}

		if (MaterialType* EyelashMaterial = Cast<MaterialType>(EyelashesHiLods))
		{
			InCallback(EyelashMaterial);
		}
	}
};

USTRUCT()
struct METAHUMANCHARACTER_API FMetaHumanBodyRigLogicGeneratedAsset
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "RigLogic")
	FString SolverName;

	UPROPERTY(EditAnywhere, Category = "RigLogic")
	TObjectPtr<UAnimSequence> AnimSequence;

	UPROPERTY(EditAnywhere, Category = "RigLogic")
	TObjectPtr<UPoseAsset> PoseAsset;
};

/** A manifest of the assets generated by a UMetaHumanCharacter */
USTRUCT()
struct METAHUMANCHARACTER_API FMetaHumanCharacterGeneratedAssets
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<class USkeletalMesh> FaceMesh;

	UPROPERTY()
	TMap<EFaceTextureType, TObjectPtr<class UTexture2D>> SynthesizedFaceTextures;

	UPROPERTY()
	TMap<EBodyTextureType, TObjectPtr<class UTexture2D>> BodyTextures;

	UPROPERTY()
	TObjectPtr<class USkeletalMesh> BodyMesh;

	UPROPERTY()
	TObjectPtr<class UPhysicsAsset> PhysicsAsset;

	UPROPERTY()
	TArray<FMetaHumanBodyRigLogicGeneratedAsset> BodyRigLogicAssets;

	/** Model parameters generated by fitting the model to the face and body geometry */
	UPROPERTY()
	TMap<FString, float> BodyMeasurements;

	/**
	 * Metadata about each generated asset referenced from properties on this struct.
	 *
	 * Callers expect to be able to iterate over all generated objects by iterating this array, so
	 * every object created in the Outer provided to UMetaHumanCharacterEditorSubsystem::TryGenerateCharacterAssets
	 * must have an entry in this array, even if they have no specific metadata.
	 */
	UPROPERTY()
	TArray<FMetaHumanGeneratedAssetMetadata> Metadata;

	/**
	 * Utility to remove metadata for a given asset
	 */
	bool RemoveAssetMetadata(TNotNull<const UObject*> InAsset);
};

/**
 * Used by thumbnail system to generate additional thumbnails (e.g. face, body)
 * and store them inside the character package.
 */
UCLASS()
class METAHUMANCHARACTER_API UMetaHumanCharacterThumbnailAux : public UObject
{
	GENERATED_BODY()
};

/**
 * Camera framing positions for taking character's thumbnail.
 */
UENUM()
enum class EMetaHumanCharacterThumbnailCameraPosition : uint8
{
	Face,
	Body,
	Character_Body,
	Character_Face
};

/**
 * The MetaHuman Character Asset holds all the information required build a MetaHuman.
 * Any data that needs to be serialized for a MetaHuman should be stored in this class
 * This class relies on the UMetaHumanCharacterEditorSubsystem to have its properties
 * initialized and its basically a container for data associated with a MetaHuman
 */
UCLASS()
class METAHUMANCHARACTER_API UMetaHumanCharacter : public UObject
{
	GENERATED_BODY()

public:

	UMetaHumanCharacter();

	//~Begin UObject interface
#if WITH_EDITOR
	virtual void PostInitProperties() override;
	virtual void PostLoad() override;
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& InPropertyChangedEvent) override;
	virtual void PostTransacted(const FTransactionObjectEvent& InTransactionEvent) override;
#endif // WITH_EDITOR
	virtual void Serialize(FArchive& InAr) override;
	//~End UObject interface

	/**
	 * Returns true if the character is in a valid state, meaning all of its components
	 * are properly initialized. Call UMetaHumanCharacterEditorSubsystem::InitializeMetaHumanCharacter
	 * to make sure the character is in a valid state
	 */
	bool IsCharacterValid() const;

	/**
	 * Stores Face State data in a compressed buffer
	 */
	void SetFaceStateData(const FSharedBuffer& InFaceStateData);

	/**
	 * Retrieves the Face State data from the internal bulk data
	 */
	[[nodiscard]] FSharedBuffer GetFaceStateData() const;

	/**
	 * Stores Face DNA in a compressed buffer. 
	 */
	void SetFaceDNABuffer(TConstArrayView<uint8> InFaceDNABuffer, bool bInHasFaceDNABlendshapes);

	/**
	 * Returns true if the character has a face DNA stored in it
	 */
	bool HasFaceDNA() const;

	/**
	 * Returns a buffer with the Face DNA from the internal bulk data
	 */
	[[nodiscard]] TArray<uint8> GetFaceDNABuffer() const;

	/**
	 * Returns true if the character has blendshapes in the attached face DNA.
	 */
	bool HasFaceDNABlendshapes() const;

	/**
	 * Stores the Body State data in a compressed buffer
	 */
	void SetBodyStateData(const FSharedBuffer& InBodyStateData);

	/**
	 * Retrieves the Body State data from the internal bulk data
	 */
	[[nodiscard]] FSharedBuffer GetBodyStateData() const;

	/**
	 * Stores Body DNA in a compressed buffer
	 */
	void SetBodyDNABuffer(TConstArrayView<uint8> InBodyDNABuffer);

	/**
	 * Returns true if the character has a body DNA stored in it
	 */
	bool HasBodyDNA() const;

	/**
	 * Returns a buffer with the Body DNA from the internal bulk data
	 */
	[[nodiscard]] TArray<uint8> GetBodyDNABuffer() const;
	
	/**
	 * Returns true if the character has any synthesized textures stored in it
	 */
	bool HasSynthesizedTextures() const;

	/**
	 * Mark the character as having high resolution textures which can be used to
	 * prevent it from being overridden
	 */
	void SetHasHighResolutionTextures(bool bInHasHighResolutionTextures);

	/**
	 * Returns true if the character was marked as having high resolution textures
	 */
	bool HasHighResolutionTextures() const;

	/**
	 * Stores face texture data to be serialized
	 */
	void StoreSynthesizedFaceTexture(EFaceTextureType InTextureType, const FImage& InTextureData);

	/**
	 * Gets the synthesized face texture resolution.
	 */
	FInt32Point GetSynthesizedFaceTexturesResolution(EFaceTextureType InFaceTextureType) const;

	/**
	 * Gets the map of valid face textures. A texture is considered valid if its type is being referenced
	 * in SynthesizedFaceTexturesInfo
	 */
	TMap<EFaceTextureType, TObjectPtr<UTexture2D>> GetValidFaceTextures() const;

	/**
	 * Stores high res body texture data to be serialized
	 */
	void StoreHighResBodyTexture(EBodyTextureType InTextureType, const FImage& InTextureData);

	/**
	 * Resets the bulk data for any texture types that are missing texture infos
	 */
	void ResetUnreferencedHighResTextureData();

	/**
	 * Removes all textures stored in character
	 */
	void RemoveAllTextures();

	/**
	 * Gets a future that can be used to obtain the actual face texture data
	 */
	[[nodiscard]] TFuture<FSharedBuffer> GetSynthesizedFaceTextureDataAsync(EFaceTextureType InTextureType) const;

	/**
	 * Gets a future that can be used to obtain the actual body texture data
	 */
	[[nodiscard]] TFuture<FSharedBuffer> GetHighResBodyTextureDataAsync(EBodyTextureType InTextureType) const;

	/**
	 * Gets the synthesized body texture resolution.
	 */
	FInt32Point GetSynthesizedBodyTexturesResolution(EBodyTextureType InBodyTextureType) const;


	/** Gets the Character's internal Collection */
	[[nodiscard]] TObjectPtr<UMetaHumanCollection> GetMutableInternalCollection();
	[[nodiscard]] const TObjectPtr<UMetaHumanCollection> GetInternalCollection() const;
	[[nodiscard]] FMetaHumanPaletteItemKey GetInternalCollectionKey() const;

public:

	// TODO: These properties are VisibleAnywhere to facilitate debugging while developing

	// The character type used to load the appropriate identity template model
	UPROPERTY(VisibleAnywhere, Category = "Template")
	EMetaHumanCharacterTemplateType TemplateType = EMetaHumanCharacterTemplateType::MetaHuman;

	UPROPERTY(VisibleAnywhere, Category = "Face")
	FMetaHumanCharacterFaceEvaluationSettings FaceEvaluationSettings;

	UPROPERTY(VisibleAnywhere, Category = "Head")
	FMetaHumanCharacterHeadModelSettings HeadModelSettings;

	UPROPERTY(VisibleAnywhere, Category = "Skin")
	FMetaHumanCharacterSkinSettings SkinSettings;

	UPROPERTY(VisibleAnywhere, Category = "Eyes")
	FMetaHumanCharacterEyesSettings EyesSettings;

	UPROPERTY(VisibleAnywhere, Category = "Makeup")
	FMetaHumanCharacterMakeupSettings MakeupSettings;

	UPROPERTY(VisibleAnywhere, Category = "Skin")
	bool bHasHighResolutionTextures = false;

	// Fixed body types are either imported from dna as a whole rig, or a fixed compatibility body
	UPROPERTY(VisibleAnywhere, Category = "Body", AssetRegistrySearchable)
	bool bFixedBodyType = false;

	// Information about each of the face textures used to build the UTexture assets when the character is loaded
	UPROPERTY(VisibleAnywhere, Category = "Textures|Face")
	TMap<EFaceTextureType, FMetaHumanCharacterTextureInfo> SynthesizedFaceTexturesInfo;

	// Transient face textures created from the data stored in SynthesizedFaceTexturesData
	UPROPERTY(VisibleAnywhere, Category = "Textures|Face", Transient)
	TMap<EFaceTextureType, TObjectPtr<class UTexture2D>> SynthesizedFaceTextures;

	// Information about each of the high res body textures used to build the UTexture assets when the character is loaded
	UPROPERTY(VisibleAnywhere, Category = "Textures|Body")
	TMap<EBodyTextureType, FMetaHumanCharacterTextureInfo> HighResBodyTexturesInfo;

	// Transient body textures, can be created from the data stored in HighResBodyTexturesData
	UPROPERTY(VisibleAnywhere, Category = "Textures|Body", Transient)
	TMap<EBodyTextureType, TObjectPtr<class UTexture2D>> BodyTextures;

	UPROPERTY(VisibleAnywhere, Category = "Lighting")
	FMetaHumanCharacterViewportSettings ViewportSettings;

#if WITH_EDITORONLY_DATA
	/** Serialized preview material, so that the editor can load the last used one */
	UPROPERTY(VisibleAnywhere, Category = "Skin")
	EMetaHumanCharacterSkinPreviewMaterial PreviewMaterialType = EMetaHumanCharacterSkinPreviewMaterial::Default;

	/** Information for thumbnail rendering */
	UPROPERTY(VisibleAnywhere, Instanced, AdvancedDisplay, Category = "Thumbnail")
	TObjectPtr<class UThumbnailInfo> ThumbnailInfo;

	/** Character defined wardrobe paths */
	UPROPERTY(VisibleAnywhere, Category = "Wardrobe")
	TArray<FMetaHumanCharacterAssetsSection> WardrobePaths;

	/** Wardrobe individual assets per slot name */
	UPROPERTY(VisibleAnywhere, Category = "Wardrobe")
	TMap<FName, FMetaHumanCharacterWardrobeIndividualAssets> WardrobeIndividualAssets;

	/** Character individual assets for blend tool and presets library */
	UPROPERTY(VisibleAnywhere, Category = "Pipeline")
	TMap<FName, FMetaHumanCharacterIndividualAssets> CharacterIndividualAssets;

	/** 
	 * A list of Collection pipelines that have been instanced for this character, used to track pipeline properties.
	 * There should be only a single instance of a pipeline class. Stored in a map for convenience.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Pipeline")
	TMap<TSubclassOf<UMetaHumanCollectionPipeline>, TObjectPtr<UMetaHumanCollectionPipeline>> PipelinesPerClass;
#endif

#if WITH_EDITOR
	/** Callback when wardrobe settings changes in editor */
	DECLARE_MULTICAST_DELEGATE(FOnWardrobePathsChanged);
	FOnWardrobePathsChanged OnWardrobePathsChanged;

	/** Callback when rigging state changes in editor */
	DECLARE_MULTICAST_DELEGATE(FOnRiggingStateChanged);
	FOnRiggingStateChanged OnRiggingStateChanged;

	/** Generates a full object path from the character object path and camera position to be used in the package thumbnail map. */
	static FName GetThumbnailPathInPackage(const FString& InCharacterAssetPath, EMetaHumanCharacterThumbnailCameraPosition InThumbnailPosition);

private:
	/**
	 * Ensures the internal Collection is correctly set up to build this Character.
	 * 
	 * Should be called when the Collection is initialized and any time the Collection's Character 
	 * slot may have been modified.
	 */
	void ConfigureCollection();
#endif

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UMetaHumanCharacterThumbnailAux> ThumbnailAux_CharacterBody;

	UPROPERTY()
	TObjectPtr<UMetaHumanCharacterThumbnailAux> ThumbnailAux_Face;

	UPROPERTY()
	TObjectPtr<UMetaHumanCharacterThumbnailAux> ThumbnailAux_Body;
#endif

	/**
	 * The Character's built-in palette that is used for the build. Determines which build pipeline to use
	 * and contains all of the prepared assets that will be built for the platform.
	 */
	UPROPERTY(EditAnywhere, Instanced, Category = "Pipeline", DisplayName = "Internal Palette (build)")
	TObjectPtr<UMetaHumanCollection> InternalCollection;

	UPROPERTY()
	FMetaHumanPaletteItemKey InternalCollectionKey;

	// Stores the Character Face State
	UE::Serialization::FEditorBulkData FaceStateBulkData;

	// Stores the Character Face DNA (optional)
	UE::Serialization::FEditorBulkData FaceDNABulkData;

	// Stores whether the face DNA contains blendshapes
	UPROPERTY()
	bool bHasFaceDNABlendshapes = false;

	// Stores the Character Body State
	UE::Serialization::FEditorBulkData BodyStateBulkData;

	// Stores the Character Body DNA (optional)
	UE::Serialization::FEditorBulkData BodyDNABulkData;

	// Stores the Synthesized Face Textures data
	TSortedMap<EFaceTextureType, UE::Serialization::FEditorBulkData> SynthesizedFaceTexturesData;
	// Stores the high res body Textures data
	TSortedMap<EBodyTextureType, UE::Serialization::FEditorBulkData> HighResBodyTexturesData;
};