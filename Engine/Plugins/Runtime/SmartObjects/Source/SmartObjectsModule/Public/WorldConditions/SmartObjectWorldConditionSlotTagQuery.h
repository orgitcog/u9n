// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SmartObjectTypes.h"
#include "WorldConditions/SmartObjectWorldConditionBase.h"
#include "SmartObjectWorldConditionSlotTagQuery.generated.h"

#define UE_API SMARTOBJECTSMODULE_API

/**
 * World condition to match Smart Object slots's runtime tags.
 */

USTRUCT()
struct FSmartObjectWorldConditionSlotTagQueryState
{
	GENERATED_BODY()

	FSmartObjectSlotHandle SlotHandle;
	
	FDelegateHandle DelegateHandle;
};

USTRUCT(meta=(DisplayName="Match Runtime Slot Tags"))
struct FSmartObjectWorldConditionSlotTagQuery : public FSmartObjectWorldConditionBase
{
	GENERATED_BODY()

	using FStateType = FSmartObjectWorldConditionSlotTagQueryState;

protected:
#if WITH_EDITOR
	UE_API virtual FText GetDescription() const override;
#endif
	virtual TObjectPtr<const UStruct>* GetRuntimeStateType() const override
	{
		static TObjectPtr<const UStruct> Ptr{FStateType::StaticStruct()};
		return &Ptr;
	}
	UE_API virtual bool Initialize(const UWorldConditionSchema& Schema) override;
	UE_API virtual bool Activate(const FWorldConditionContext& Context) const override;
	UE_API virtual FWorldConditionResult IsTrue(const FWorldConditionContext& Context) const override;
	UE_API virtual void Deactivate(const FWorldConditionContext& Context) const override;

	FWorldConditionContextDataRef SubsystemRef;
	FWorldConditionContextDataRef SlotHandleRef;

	/** Smart Object Slot's runtime tags needs to match this query for the condition to evaluate true. */
	UPROPERTY(EditAnywhere, Category="Default")
	FGameplayTagQuery TagQuery;
};

#undef UE_API
