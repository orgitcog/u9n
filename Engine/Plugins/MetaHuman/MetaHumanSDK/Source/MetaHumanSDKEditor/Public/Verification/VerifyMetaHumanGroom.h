// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "MetaHumanVerificationRuleCollection.h"

#include "VerifyMetaHumanGroom.generated.h"

/**
 * A rule to test if a UObject complies with the MetaHuman Groom standard
 */
UCLASS(BlueprintType)
class METAHUMANSDKEDITOR_API UVerifyMetaHumanGroom : public UMetaHumanVerificationRuleBase
{
	GENERATED_BODY()

public:
	// UMetaHumanVerificationRuleBase overrides
	virtual void Verify_Implementation(const UObject* ToVerify, UMetaHumanAssetReport* Report, const FMetaHumansVerificationOptions& Options) const override;
};
