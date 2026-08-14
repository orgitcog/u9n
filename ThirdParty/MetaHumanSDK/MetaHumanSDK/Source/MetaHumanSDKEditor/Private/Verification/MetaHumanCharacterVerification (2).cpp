// Copyright Epic Games, Inc. All Rights Reserved.

#include "Verification/MetaHumanCharacterVerification.h"

#include "MetaHumanAssetReport.h"
#include "UObject/UObjectIterator.h"

#define LOCTEXT_NAMESPACE "MetaHumanCharacterVerification"

namespace UE::MetaHuman
{
FMetaHumanCharacterVerification::FMetaHumanCharacterVerification()
{
	for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
	{
		if (!(*ClassIt)->IsChildOf(UMetaHumanCharacterTypesVerificationExtensionBase::StaticClass()) || ((*ClassIt)->HasAnyClassFlags(CLASS_Abstract)))
		{
			continue;
		}

		// We only expect one implementation. Take the first.
		Extension.Reset(Cast<UMetaHumanCharacterTypesVerificationExtensionBase>((*ClassIt)->GetDefaultObject()));
		break;
	}
}

void FMetaHumanCharacterVerification::VerifyGroomWardrobeItem(TNotNull<const UObject*> Target, TNotNull<const UObject*> GroomBindingAsset, UMetaHumanAssetReport* Report) const
{
	if (Extension)
	{
		Extension->VerifyGroomWardrobeItem(Target, GroomBindingAsset, Report);
	}
}
void FMetaHumanCharacterVerification::VerifySkelMeshClothingWardrobeItem(TNotNull<const UObject*> Target, TNotNull<const UObject*> SkeletalMesh, UMetaHumanAssetReport* Report) const
{
	if (Extension)
	{
		Extension->VerifySkelMeshClothingWardrobeItem(Target, SkeletalMesh, Report);
	}
}

void FMetaHumanCharacterVerification::VerifyOutfitWardrobeItem(TNotNull<const UObject*> Target, TNotNull<const UObject*> OutfitAsset, UMetaHumanAssetReport* Report) const
{
	if (Extension)
	{
		Extension->VerifyOutfitWardrobeItem(Target, OutfitAsset, Report);
	}
}

void FMetaHumanCharacterVerification::VerifyOutfitAsset(const TNotNull<const UObject*> Target, UMetaHumanAssetReport* Report) const
{
	if (Extension)
	{
		Extension->VerifyOutfitAsset(Target, Report);
	}
}

void FMetaHumanCharacterVerification::VerifyMetaHumanCharacterAsset(TNotNull<const UObject*> Target, UMetaHumanAssetReport* Report) const
{
}

FClothingAssetDetails FMetaHumanCharacterVerification::GetDetailsForClothingAsset(TNotNull<const UObject*> Target) const
{
	if (Extension)
	{
		return Extension->GetDetailsForClothingAsset(Target);
	}
	return {};
}

// These methods are used by import to choose the correct Icons for display etc. and should work even if the Extension is not loaded, so use reflection
bool FMetaHumanCharacterVerification::IsWardrobeItem(TNotNull<const UObject*> Target)
{
	return Target->GetClass()->GetClassPathName() == FTopLevelAssetPath(FName("/Script/MetaHumanCharacterPalette"), FName("MetaHumanWardrobeItem"));
}

bool FMetaHumanCharacterVerification::IsCharacterAsset(TNotNull<const UObject*> Target)
{
	return Target->GetClass()->GetClassPathName() == FTopLevelAssetPath(FName("/Script/MetaHumanCharacter"), FName("MetaHumanCharacter"));
}

bool FMetaHumanCharacterVerification::IsOutfitAsset(TNotNull<const UObject*> Target)
{
	return Target->GetClass()->GetClassPathName() == FTopLevelAssetPath(FName("/Script/ChaosOutfitAssetEngine"), FName("ChaosOutfitAsset"));
}

FMetaHumanCharacterVerification& FMetaHumanCharacterVerification::Get()
{
	static FMetaHumanCharacterVerification TheInstance;
	return TheInstance;
}
} // namespace UE::MetaHuman

#undef LOCTEXT_NAMESPACE
