// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SmartObjectTypes.h"
#include "UObject/ObjectMacros.h"
#include "SmartObjectRequestTypes.generated.h"

class USmartObjectBehaviorDefinition;

/**
 * Struct that can be used to filter results of a smart object request when trying to find or claim a smart object
 */
USTRUCT(BlueprintType)
struct FSmartObjectRequestFilter
{
	GENERATED_BODY()

	/** Gameplay tags of the Actor or Entity requesting the Smart Object slot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SmartObject)
	FGameplayTagContainer UserTags;

	/** The user's claim priority. The search will contain already claimed slots at lower priority. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SmartObject)
	ESmartObjectClaimPriority ClaimPriority = ESmartObjectClaimPriority::Normal; 

	/** Only return slots whose activity tags are matching this query. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SmartObject)
	FGameplayTagQuery ActivityRequirements;

	/** If set, will filter out any SmartObject that uses different BehaviorDefinition classes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SmartObject)
	TArray<TSubclassOf<USmartObjectBehaviorDefinition>> BehaviorDefinitionClasses;

	/** If true, will evaluate the slot and object conditions, otherwise will skip them. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SmartObject)
	bool bShouldEvaluateConditions = true;

	/** If true, this search will contain claimed slots. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SmartObject)
	bool bShouldIncludeClaimedSlots = false;

	/** If true, this search will contain disabled slots. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SmartObject)
	bool bShouldIncludeDisabledSlots = false;

	/** Is set, will filter out any SmartObject that does not pass the predicate. */
	TFunction<bool(FSmartObjectHandle)> Predicate;
};

/**
 * Struct used to find a smart object within a specific search range and with optional filtering
 */
USTRUCT(BlueprintType)
struct FSmartObjectRequest
{
	GENERATED_BODY()

	FSmartObjectRequest() = default;
	FSmartObjectRequest(const FBox& InQueryBox, const FSmartObjectRequestFilter& InFilter)
		: QueryBox(InQueryBox)
		, Filter(InFilter)
	{}

	/** Box defining the search range */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SmartObject)
	FBox QueryBox = FBox(ForceInitToZero);

	/** Struct used to filter out some results (all results allowed by default) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SmartObject)
	FSmartObjectRequestFilter Filter;
};

/**
 * Struct that holds the object and slot selected by processing a smart object request.
 */
USTRUCT(BlueprintType)
struct FSmartObjectRequestResult
{
	GENERATED_BODY()

	explicit FSmartObjectRequestResult(const FSmartObjectHandle InSmartObjectHandle, const FSmartObjectSlotHandle InSlotHandle = {})
		: SmartObjectHandle(InSmartObjectHandle)
		, SlotHandle(InSlotHandle)
	{}

	FSmartObjectRequestResult() = default;

	bool IsValid() const { return SmartObjectHandle.IsValid() && SlotHandle.IsValid(); }

	bool operator==(const FSmartObjectRequestResult& Other) const
	{
		return SmartObjectHandle == Other.SmartObjectHandle
			&& SlotHandle == Other.SlotHandle;
	}

	bool operator!=(const FSmartObjectRequestResult& Other) const
	{
		return !(*this == Other);
	}
	
	friend FString LexToString(const FSmartObjectRequestResult& Result)
	{
		return FString::Printf(TEXT("Object:%s Slot:%s"), *LexToString(Result.SmartObjectHandle), *LexToString(Result.SlotHandle));
	}

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = SmartObject)
	FSmartObjectHandle SmartObjectHandle;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = SmartObject)
	FSmartObjectSlotHandle SlotHandle;
};
