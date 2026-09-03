// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WorldConditions/SmartObjectWorldConditionBase.h"
#include "GameplayTagContainer.h"
#include "WorldCondition_SmartObjectActorTagQuery.generated.h"

#define UE_API SMARTOBJECTSMODULE_API

USTRUCT()
struct FWorldCondition_SmartObjectActorTagQueryState
{
	GENERATED_BODY()
	
	FDelegateHandle DelegateHandle;
};

/**
 * World condition to match tags of the Smart Object's owning Actor (which must implement IGameplayTagAssetInterface or have an AbilitySystemComponent).
 */
USTRUCT(meta=(DisplayName="Match Gameplay tags on SmartObject actor"))
struct FWorldCondition_SmartObjectActorTagQuery : public FSmartObjectWorldConditionBase
{
	GENERATED_BODY()

	using FStateType = FWorldCondition_SmartObjectActorTagQueryState;

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

	/** Smart Object's owning actor for which the tags must match the query. The Actor must implement IGameplayTagAssetInterface or have an AbilitySystemComponent. */
	FWorldConditionContextDataRef SmartObjectActorRef;

public:
	/** Tags on the Smart Object's owning actor that need to match this query for the condition to evaluate true. */
	UPROPERTY(EditAnywhere, Category="Default")
	FGameplayTagQuery TagQuery;
};

#undef UE_API
