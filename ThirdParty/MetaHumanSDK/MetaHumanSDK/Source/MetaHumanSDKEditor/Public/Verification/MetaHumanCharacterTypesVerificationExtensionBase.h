// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"

#include "MetaHumanCharacterTypesVerificationExtensionBase.generated.h"

class UGroomAsset;
class UMetaHumanAssetReport;
class USkeletalMesh;

struct FClothingAssetDetails
{
	bool bResizesWithBlendableBodies = false;
	bool bHasClothingMask = false;
};

/**
 * Interface class to allow verification rule implementations to live inside the MetaHumanCharacter plugin for access to types
 * defined in plugins that we can not reference from the MetaHumanSDK
 */
UCLASS(Abstract, MinimalAPI)
class UMetaHumanCharacterTypesVerificationExtensionBase : public UObject
{
	GENERATED_BODY()

public:
	// WardrobeItem verification
	METAHUMANSDKEDITOR_API virtual void VerifyGroomWardrobeItem(TNotNull<const UObject*> Target, TNotNull<const UObject*> GroomBindingAsset, UMetaHumanAssetReport* Report) PURE_VIRTUAL(UMetaHumanCharacterTypesVerificationExtensionBase::VerifyGroomWardrobeItem_Implementation,);
	METAHUMANSDKEDITOR_API virtual void VerifySkelMeshClothingWardrobeItem(TNotNull<const UObject*> Target, TNotNull<const UObject*> SkeletalMesh, UMetaHumanAssetReport* Report) PURE_VIRTUAL(UMetaHumanCharacterTypesVerificationExtensionBase::VerifySkelMeshClothingWardrobeItem_Implementation,);
	METAHUMANSDKEDITOR_API virtual void VerifyOutfitWardrobeItem(TNotNull<const UObject*> Target, TNotNull<const UObject*> OutfitAsset, UMetaHumanAssetReport* Report) PURE_VIRTUAL(UMetaHumanCharacterTypesVerificationExtensionBase::VerifyOutfitWardrobeItem_Implementation,);

	// Custom verification for individual types
	METAHUMANSDKEDITOR_API virtual void VerifyOutfitAsset(TNotNull<const UObject*> Target, UMetaHumanAssetReport* Report) PURE_VIRTUAL(UMetaHumanCharacterTypesVerificationExtensionBase::VerifyOutfitAsset_Implementation,);
	METAHUMANSDKEDITOR_API virtual void VerifyMetaHumanCharacterAsset(TNotNull<const UObject*> Target, UMetaHumanAssetReport* Report) PURE_VIRTUAL(UMetaHumanCharacterTypesVerificationExtensionBase::VerifyMetaHumanCharacterAsset_Implementation,);

	// Technical details for individual types
	METAHUMANSDKEDITOR_API virtual FClothingAssetDetails GetDetailsForClothingAsset(TNotNull<const UObject*> Target) PURE_VIRTUAL(UMetaHumanCharacterTypesVerificationExtensionBase::GetDetailsForClothingAsset_Implementation, return {};);
};
