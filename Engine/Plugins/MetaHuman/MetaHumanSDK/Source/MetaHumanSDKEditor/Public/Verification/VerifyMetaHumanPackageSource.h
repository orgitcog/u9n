// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "MetaHumanVerificationRuleCollection.h"

#include "VerifyMetaHumanPackageSource.generated.h"

/**
 * A generic rule for MetaHuman Asset Groups that tests that they are valid for the generation of a MetaHuman Package.
 * Only works for "normal" Asset Groups like grooms and clothing, not legacy characters.
 */
UCLASS()
class METAHUMANSDKEDITOR_API UVerifyMetaHumanPackageSource : public UMetaHumanVerificationRuleBase
{
	GENERATED_BODY()

public:
	// UMetaHumanVerificationRuleBase overrides
	virtual void Verify_Implementation(const UObject* ToVerify, UMetaHumanAssetReport* Report, const FMetaHumansVerificationOptions& Options) const override;
};
