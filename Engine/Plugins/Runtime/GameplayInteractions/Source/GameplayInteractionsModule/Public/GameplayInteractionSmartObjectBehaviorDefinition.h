// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SmartObjectDefinition.h"
#include "StateTreeReference.h"
#include "GameplayInteractionSmartObjectBehaviorDefinition.generated.h"

#define UE_API GAMEPLAYINTERACTIONSMODULE_API

/**
 * SmartObject behavior definition for the GameplayInteractions
 */
UCLASS(MinimalAPI, BlueprintType)
class UGameplayInteractionSmartObjectBehaviorDefinition : public USmartObjectBehaviorDefinition
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "SmartObject", meta=(Schema="/Script/GameplayInteractionsModule.GameplayInteractionStateTreeSchema"))
	FStateTreeReference StateTreeReference;

	UFUNCTION(BlueprintCallable, Category = "StateTree")
	void SetStateTree(UStateTree* NewStateTree)
	{
		StateTreeReference.SetStateTree(NewStateTree);
	}

	UFUNCTION(BlueprintPure, Category = "StateTree")
	const UStateTree* GetStateTree() const
	{
		return StateTreeReference.GetStateTree();
	}

public:
#if WITH_EDITOR
	//~ UObject
	UE_API virtual void GetPreloadDependencies(TArray<UObject*>& OutDeps) override;
#endif //WITH_EDITOR
};

#undef UE_API
