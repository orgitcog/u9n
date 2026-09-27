// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Verification/MetaHumanCharacterTypesVerificationExtensionBase.h"
#include "MetaHumanCharacterTypesVerification.generated.h"

/**
 * 
 */
UCLASS()
class METAHUMANCHARACTEREDITOR_API UMetaHumanCharacterTypesVerification : public UMetaHumanCharacterTypesVerificationExtensionBase
{
	GENERATED_BODY()

public:
	virtual void VerifyGroomWardrobeItem(TNotNull<const UObject*> Target, TNotNull<const UObject*> GroomBindingAsset, UMetaHumanAssetReport* Report) override;
	virtual void VerifySkelMeshClothingWardrobeItem(TNotNull<const UObject*> Target, TNotNull<const UObject*> SkeletalMesh, UMetaHumanAssetReport* Report) override;
	virtual void VerifyOutfitWardrobeItem(TNotNull<const UObject*> Target, TNotNull<const UObject*> OutfitAsset, UMetaHumanAssetReport* Report) override;
	virtual void VerifyOutfitAsset(TNotNull<const UObject*> Target, UMetaHumanAssetReport* Report) override;
	virtual void VerifyMetaHumanCharacterAsset(TNotNull<const UObject*> Target, UMetaHumanAssetReport* Report) override;
	virtual FClothingAssetDetails GetDetailsForClothingAsset(TNotNull<const UObject*> Target) override;
};
