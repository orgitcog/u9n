// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "MetaHumanVerificationRuleCollection.h"

#include "VerifyMetaHumanCharacter.generated.h"

/**
 * A verification rule that tests that a MetaHuman character is valid. Currently only handles "Legacy" MetaHuman Characters.
 */
UCLASS()
class METAHUMANSDKEDITOR_API UVerifyMetaHumanCharacter : public UMetaHumanVerificationRuleBase
{
	GENERATED_BODY()

public:
	// UMetaHumanVerificationRuleBase overrides
	virtual void Verify_Implementation(const UObject* ToVerify, UMetaHumanAssetReport* Report, const FMetaHumansVerificationOptions& Options) const override;
};
