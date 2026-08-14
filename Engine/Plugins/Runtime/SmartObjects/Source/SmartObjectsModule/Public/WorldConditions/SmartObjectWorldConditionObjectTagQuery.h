// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayTagContainer.h"
#include "WorldConditions/SmartObjectWorldConditionBase.h"
#include "SmartObjectWorldConditionObjectTagQuery.generated.h"

#define UE_API SMARTOBJECTSMODULE_API

/**
 * World condition to match Smart Object's runtime tags.
 */

USTRUCT()
struct FSmartObjectWorldConditionObjectTagQueryState
{
	GENERATED_BODY()
	
	FDelegateHandle DelegateHandle;
};

USTRUCT(meta=(DisplayName="Match Runtime Object Tags"))
struct FSmartObjectWorldConditionObjectTagQuery : public FSmartObjectWorldConditionBase
{
	GENERATED_BODY()

	using FStateType = FSmartObjectWorldConditionObjectTagQueryState;

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
	FWorldConditionContextDataRef ObjectHandleRef;

public:	
	/** Smart Object's runtime tags needs to match this query for the condition to evaluate true. */
	UPROPERTY(EditAnywhere, Category="Default")
	FGameplayTagQuery TagQuery;
};

#undef UE_API
