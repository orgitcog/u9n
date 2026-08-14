// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "MetaHumanVerificationRuleCollection.h"

#include "VerifyMetaHumanOutfitClothing.generated.h"

/**
 * Verifies that a piece of clothing conforms to the standard for outfit-based clothing packages
 */
UCLASS()
class METAHUMANSDKEDITOR_API UVerifyMetaHumanOutfitClothing : public UMetaHumanVerificationRuleBase
{
	GENERATED_BODY()
	virtual void Verify_Implementation(const UObject* ToVerify, UMetaHumanAssetReport* Report, const FMetaHumansVerificationOptions& Options) const override;
};
