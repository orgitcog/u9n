// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StateTreeSchema.h"
#include "GameFramework/Actor.h"
#include "StateTreeTypes.h"
#include "GameplayInteractionStateTreeSchema.generated.h"

#define UE_API GAMEPLAYINTERACTIONSMODULE_API

struct FStateTreeExternalDataDesc;

UCLASS(MinimalAPI, BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Gameplay Interactions"))
class UGameplayInteractionStateTreeSchema : public UStateTreeSchema
{
	GENERATED_BODY()

public:
	UE_API UGameplayInteractionStateTreeSchema();

	UClass* GetContextActorClass() const { return ContextActorClass; };
	UClass* GetSmartObjectActorClass() const { return SmartObjectActorClass; };

protected:
	UE_API virtual bool IsStructAllowed(const UScriptStruct* InScriptStruct) const override;
	UE_API virtual bool IsClassAllowed(const UClass* InScriptStruct) const override;
	UE_API virtual bool IsExternalItemAllowed(const UStruct& InStruct) const override;

	virtual TConstArrayView<FStateTreeExternalDataDesc> GetContextDataDescs() const override { return ContextDataDescs; }

	UE_API virtual void PostLoad() override;

#if WITH_EDITOR
	UE_API virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	
	/** Actor class the StateTree is expected to run on. Allows to bind to specific Actor class' properties. */
	UPROPERTY(EditAnywhere, Category="Defaults")
	TSubclassOf<AActor> ContextActorClass;

	/** Actor class of the SmartObject the StateTree is expected to run with. Allows to bind to specific Actor class' properties. */
	UPROPERTY(EditAnywhere, Category="Defaults")
	TSubclassOf<AActor> SmartObjectActorClass;

	/** List of named external data required by schema and provided to the state tree through the execution context. */
	UPROPERTY()
	TArray<FStateTreeExternalDataDesc> ContextDataDescs;
};

#undef UE_API
