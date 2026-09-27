// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UObject/Object.h"
#include "Verification/MetaHumanCharacterTypesVerificationExtensionBase.h"

class UGroomAsset;
class UMetaHumanAssetReport;
class USkeletalMesh;

namespace UE::MetaHuman
{
/**
 * Verification for more complex types used by MetaHuman. This code is designed to avoid heavyweight plugin
 * dependencies in the MetaHumanSDK while allowing use of those types if required.
 */
struct FMetaHumanCharacterVerification
{
	FMetaHumanCharacterVerification();

	// WardrobeItem verification
	void VerifyGroomWardrobeItem(TNotNull<const UObject*> Target, TNotNull<const UObject*> GroomBindingAsset, UMetaHumanAssetReport* Report) const;
	void VerifySkelMeshClothingWardrobeItem(TNotNull<const UObject*> Target, TNotNull<const UObject*> SkeletalMesh, UMetaHumanAssetReport* Report) const;
	void VerifyOutfitWardrobeItem(TNotNull<const UObject*> Target, TNotNull<const UObject*> OutfitAsset, UMetaHumanAssetReport* Report) const;

	// Custom verification for individual types
	void VerifyOutfitAsset(TNotNull<const UObject*> Target, UMetaHumanAssetReport* Report) const;
	void VerifyMetaHumanCharacterAsset(TNotNull<const UObject*> Target, UMetaHumanAssetReport* Report) const;

	// Technical details fro individual types
	FClothingAssetDetails GetDetailsForClothingAsset(TNotNull<const UObject*> Target) const;

	// Lightweight type checks
	bool IsWardrobeItem(TNotNull<const UObject*> Target);
	bool IsCharacterAsset(TNotNull<const UObject*> Target);
	bool IsOutfitAsset(TNotNull<const UObject*> Target);

	static FMetaHumanCharacterVerification& Get();

private:
	TStrongObjectPtr<UMetaHumanCharacterTypesVerificationExtensionBase> Extension;
};
} // namespace UE::MetaHuman
