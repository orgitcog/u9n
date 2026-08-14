// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "MetaHumanVerificationRuleCollection.h"

#include "VerifyMetaHumanSkeletalClothing.generated.h"

/**
 * Verifies that a piece of clothing conforms to the standard for skeletal mesh-based clothing packages
 */
UCLASS()
class METAHUMANSDKEDITOR_API UVerifyMetaHumanSkeletalClothing : public UMetaHumanVerificationRuleBase
{
	GENERATED_BODY()

public:
	virtual void Verify_Implementation(const UObject* ToVerify, UMetaHumanAssetReport* Report, const FMetaHumansVerificationOptions& Options) const override;

	static void VerifyClothingCompatibleAssets(const UObject* ToVerify, UMetaHumanAssetReport* Report);
};
