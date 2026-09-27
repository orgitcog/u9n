// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "MetaHumanVerificationRuleCollection.h"

#include "VerifyObjectValid.generated.h"

/**
 * A simple rule to test if a UObject is a valid asset
 */
UCLASS(BlueprintType)
class METAHUMANSDKEDITOR_API UVerifyObjectValid : public UMetaHumanVerificationRuleBase
{
	GENERATED_BODY()

public:
	// UMetaHumanVerificationRuleBase overrides
	virtual void Verify_Implementation(const UObject* ToVerify, UMetaHumanAssetReport* Report, const FMetaHumansVerificationOptions& Options) const override;
};
