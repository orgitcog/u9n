// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Misc/EnumRange.h"
#include "Misc/NotNull.h"
#include "MetaHumanCharacter.h"
#include "Engine/DataTable.h"

#include "MetaHumanCharacterSkinMaterials.generated.h"

UENUM()
enum class EMetaHumanCharacterAccentRegion : uint8
{
	Scalp,
	Forehead,
	Nose,
	UnderEye,
	Cheeks,
	Lips,
	Chin,
	Ears,

	Count UMETA(Hidden)
};
ENUM_RANGE_BY_COUNT(EMetaHumanCharacterAccentRegion, EMetaHumanCharacterAccentRegion::Count);

UENUM()
enum class EMetaHumanCharacterAccentRegionParameter : uint8
{
	Redness,
	Saturation,
	Lightness,

	Count UMETA(Hidden)
};
ENUM_RANGE_BY_COUNT(EMetaHumanCharacterAccentRegionParameter, EMetaHumanCharacterAccentRegionParameter::Count);

UENUM()
enum class EMetaHumanCharacterFrecklesParameter : uint8
{
	Mask,
	Density,
	Strength,
	Saturation,
	ToneShift,

	Count UMETA(Hidden)
};
ENUM_RANGE_BY_COUNT(EMetaHumanCharacterFrecklesParameter, EMetaHumanCharacterFrecklesParameter::Count);

USTRUCT()
struct FMetaHumanCharacterSkinMaterialOverrideRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Scalar Parameters")
	TMap<FName, float> ScalarParameterValues;
};


struct FMetaHumanCharacterSkinMaterials
{
	/**
	 * Returns the material slot names for the skin materials
	 */
	METAHUMANCHARACTEREDITOR_API static FName GetSkinMaterialSlotName(EMetaHumanCharacterSkinMaterialSlot InSlot);

	/**
	 * Returns the material parameter name for the a given synthesized texture type
	 */
	METAHUMANCHARACTEREDITOR_API static FName GetFaceTextureParameterName(EFaceTextureType InTextureType);

	/**
	 * Returns the material parameter name for the a given synthesized texture type
	 */
	METAHUMANCHARACTEREDITOR_API static FName GetBodyTextureParameterName(EBodyTextureType InTextureType);

	static const FName EyeLeftSlotName;
	static const FName EyeRightSlotName;
	static const FName SalivaSlotName;
	static const FName EyeShellSlotName;
	static const FName EyeEdgeSlotName;
	static const FName TeethSlotName;
	static const FName EyelashesSlotName;
	static const FName EyelashesHiLodSlotName;
	METAHUMANCHARACTEREDITOR_API static const FName UseCavityParamName;
	METAHUMANCHARACTEREDITOR_API static const FName UseAnimatedMapsParamName;
	static const FName UseTextureOverrideParamName;
	static const FName RoughnessUIMultiplyParamName;

	static void SetHeadMaterialsOnMesh(const FMetaHumanCharacterFaceMaterialSet& InMaterialSet, TNotNull<class USkeletalMesh*> InMesh);
	static void SetBodyMaterialOnMesh(TNotNull<class UMaterialInterface*> InBodyMaterial, TNotNull<class USkeletalMesh*> InMesh);

	/**
	 * Creates a Face Material set from the materials in the given face mesh
	 */
	METAHUMANCHARACTEREDITOR_API static FMetaHumanCharacterFaceMaterialSet GetHeadMaterialsFromMesh(TNotNull<class USkeletalMesh*> InFaceMesh);

	static void ApplyTextureOverrideParameterToMaterials(const FMetaHumanCharacterFaceMaterialSet& InFaceMaterialSet, TNotNull<UMaterialInstanceDynamic*> InBodyMaterial, const FMetaHumanCharacterSkinSettings& InSkinSettings);

	/**
	 * Apply skin material parameter overrides based on the face texture index for better visuals
	 */
	static void ApplySkinParametersToMaterials(const FMetaHumanCharacterFaceMaterialSet& InFaceMaterialSet, TNotNull<UMaterialInstanceDynamic*> InBodyMID, const FMetaHumanCharacterSkinSettings& InSkinSettings);

	/**
	 * Apply the Roughness UI Multiply to the skin materials
	 */
	static void ApplyRoughnessMultiplyToMaterials(const FMetaHumanCharacterFaceMaterialSet& InFaceMaterialSet, TNotNull<UMaterialInstanceDynamic*> InBodyMaterial, const FMetaHumanCharacterSkinSettings& InSkinSettings);

	/**
	 * Update the preview material parameter value of the given accent region.
	 */
	static void ApplySkinAccentParameterToMaterial(
		const FMetaHumanCharacterFaceMaterialSet& InFaceMaterialSet,
		EMetaHumanCharacterAccentRegion InRegion,
		EMetaHumanCharacterAccentRegionParameter InParameter,
		float InValue);

	/**
	 * Updates the accent region parameters in the given face material set
	 */
	static void ApplySkinAccentsToMaterial(const FMetaHumanCharacterFaceMaterialSet& InFaceMaterialSet, const FMetaHumanCharacterAccentRegions& InAccentProperties);

	/**
	 * Updates the freckles mask in the given face material set
	 */
	static void ApplyFrecklesMaskToMaterial(const FMetaHumanCharacterFaceMaterialSet& InFaceMaterialSet, EMetaHumanCharacterFrecklesMask InMask);

	/**
	 * Updates one of the freckles material parameters in the given face material set
	 */
	static void ApplyFrecklesParameterToMaterial(const FMetaHumanCharacterFaceMaterialSet& InFaceMaterialSet, EMetaHumanCharacterFrecklesParameter InParam, float InValue);

	/**
	 * Updates all freckle parameters in the given face material set
	 */
	static void ApplyFrecklesToMaterial(const FMetaHumanCharacterFaceMaterialSet& InFaceMaterialSet, const FMetaHumanCharacterFrecklesProperties& InFrecklesProperties);

	/**
	 * Apply the foundation makeup properties to the given face material
	 */
	static void ApplyFoundationMakeupToMaterial(const FMetaHumanCharacterFaceMaterialSet& InFaceMaterialSet, const FMetaHumanCharacterFoundationMakeupProperties& InFoundationMakeupProperties);

	/**
	 * Apply the eye makeup properties to the given face material
	 */
	static void ApplyEyeMakeupToMaterial(const FMetaHumanCharacterFaceMaterialSet& InFaceMaterialSet, const FMetaHumanCharacterEyeMakeupProperties& InEyeMakeupProperties);

	/**
	 * Apply the blush makeup properties to given face material
	 */
	static void ApplyBlushMakeupToMaterial(const FMetaHumanCharacterFaceMaterialSet& InFaceMaterialSet, const FMetaHumanCharacterBlushMakeupProperties& InBlushMakeupProperties);

	/**
	 * Apply the lipstick makeup properties to the given face material
	 */
	static void ApplyLipsMakeupToMaterial(const FMetaHumanCharacterFaceMaterialSet& InFaceMaterialSet, const FMetaHumanCharacterLipsMakeupProperties& InLipsMakeupProperties);

	/**
	 * Helper to apply update the MH face material so that it references the (transient) synthesized textures
	 */
	static void ApplySynthesizedTexturesToFaceMaterial(const FMetaHumanCharacterFaceMaterialSet& InFaceMaterialSet, const TMap<EFaceTextureType, TObjectPtr<UTexture2D>>& InSynthesizedFaceTextures);

	/**
	 * Helper to apply all eye material settings to the given face material set
	 */
	static void ApplyEyeSettingsToMaterial(const FMetaHumanCharacterFaceMaterialSet& InFaceMaterialSet, const FMetaHumanCharacterEyesSettings& InEyeSettings);

	/**
	 * Set the Sclera tint based on skin tone U value if not using a custom sclera tint.
	 * InOutEyeSettings will have its sclera tint values based on the skin tone
	 */
	static void ApplyEyeScleraTintBasedOnSkinTone(const FMetaHumanCharacterSkinSettings& InSkinSettings, FMetaHumanCharacterEyesSettings& InOutEyeSettings);

	/**
	 * Read the eye settings from the default eye material
	 */
	static void GetDefaultEyeSettings(FMetaHumanCharacterEyesSettings& OutEyeSettings);

	/**
	* Applies eyelashes material properties to given face material set
	*/
	static void ApplyEyelashesPropertiesToMaterial(const FMetaHumanCharacterFaceMaterialSet& InFaceMaterialSet, const FMetaHumanCharacterEyelashesProperties& InEyelashesProperties);

	/**
	* Applies teeth material properties to given face material set
	*/
	static void ApplyTeethPropertiesToMaterial(const FMetaHumanCharacterFaceMaterialSet& InFaceMaterialSet, const FMetaHumanCharacterTeethProperties& InTeethProperties);

	/**
	 * Returns a new material instance for the head for a given preview material type
	 */
	static FMetaHumanCharacterFaceMaterialSet GetHeadPreviewMaterialInstance(EMetaHumanCharacterSkinPreviewMaterial InPreviewMaterialType);

	/**
	 * Returns a new material instance for the body for a given preview material type
	 */
	static class UMaterialInstanceDynamic* GetBodyPreviewMaterialInstance(EMetaHumanCharacterSkinPreviewMaterial InPreviewMaterialType);

	/**
	 * Set the parent of InMaterial to InNewParent preserving overrides and static switches
	 */
	METAHUMANCHARACTEREDITOR_API static void SetMaterialInstanceParent(TNotNull<class UMaterialInstanceConstant*> InMaterial, TNotNull<class UMaterialInterface*> InNewParent);

	/**
	* Returns the active mask texture used for the eyelashes mesh given the input eyelashes properties
	*/
	static class UTexture2D* GetEyelashesMask(const FMetaHumanCharacterEyelashesProperties& InEyelashesProperties);
};
