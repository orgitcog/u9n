// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UObject/Object.h"

#include "MetaHumanVerificationRuleCollection.generated.h"

class UMetaHumanAssetReport;

/**
 * Options for the Verification process
 */
USTRUCT(Blueprintable)
struct FMetaHumansVerificationOptions
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = " MetaHuman SDK | Verification ")
	bool bVerbose = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = " MetaHuman SDK | Verification ")
	bool bTreatWarningsAsErrors = false;
};

/**
 * A Rule which can be part of a MetaHuman verification test suite
 */
UCLASS(Abstract, Blueprintable)
class METAHUMANSDKEDITOR_API UMetaHumanVerificationRuleBase : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Applies the rule to the asset and updates the verification report
	 *
	 * @param ToVerify The root UObject of the asset that is being verified
	 * @param Report The report which should be updated with the results of the test
	 * @param Options Verification option flags to use when generating the report
	 */
	UFUNCTION(BlueprintNativeEvent, Category = " MetaHuman SDK | Verification ")
	void Verify(const UObject* ToVerify, UMetaHumanAssetReport* Report, const FMetaHumansVerificationOptions& Options);

	virtual void Verify_Implementation(const UObject* ToVerify, UMetaHumanAssetReport* Report, const FMetaHumansVerificationOptions& Options) const PURE_VIRTUAL(UMetaHumanVerificationRuleBase::Verify_Implementation,);
};

/**
 * A collection of Rules which make up a verification test for a class of MetaHuman asset compatibility, for example
 * groom compatibility, clothing compatibility, animation compatibility etc.
 */
UCLASS(BlueprintType)
class METAHUMANSDKEDITOR_API UMetaHumanVerificationRuleCollection : public UObject
{
	GENERATED_BODY()

public:
	/** Adds a rule to this collection */
	UFUNCTION(BlueprintCallable, Category = " MetaHuman SDK | Verification ")
	void AddVerificationRule(UMetaHumanVerificationRuleBase* Rule);

	/**
	 * Runs all registered rules against the Target. Compiles the results in OutReport.
	 *
	 * @param Target The root UObject of the asset that is being verified
	 * @param Report The report which should be updated with the results of the tests
	 * @param Options The options struct which may contain relevant options for the verification rule
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = " MetaHuman SDK | Verification ")
	UMetaHumanAssetReport* ApplyAllRules(const UObject* Target, UMetaHumanAssetReport* Report, const FMetaHumansVerificationOptions& Options) const;

private:
	UPROPERTY()
	TArray<TObjectPtr<UMetaHumanVerificationRuleBase>> Rules;
};
