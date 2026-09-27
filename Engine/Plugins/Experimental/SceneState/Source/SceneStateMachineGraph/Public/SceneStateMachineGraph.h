// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EdGraph/EdGraph.h"
#include "SceneStateEnums.h"
#include "StructUtils/PropertyBag.h"
#include "SceneStateMachineGraph.generated.h"

class USceneStateMachineEntryNode;

UCLASS(MinimalAPI, HideCategories=(Graph))
class USceneStateMachineGraph : public UEdGraph
{
	GENERATED_BODY()

public:
	using FOnParametersChanged = TMulticastDelegate<void(USceneStateMachineGraph*)>;

	USceneStateMachineGraph();

	SCENESTATEMACHINEGRAPH_API static FOnParametersChanged::RegistrationType& OnParametersChanged();

	SCENESTATEMACHINEGRAPH_API void NotifyParametersChanged();

	SCENESTATEMACHINEGRAPH_API USceneStateMachineEntryNode* GetEntryNode() const;

	//~ Begin UEdGraph
	SCENESTATEMACHINEGRAPH_API virtual void AddNode(UEdGraphNode* InNodeToAdd, bool bInUserAction, bool bInSelectNewNode) override;
	//~ End UEdGraph

	//~ Begin UObject
	SCENESTATEMACHINEGRAPH_API virtual void PostInitProperties() override;
	SCENESTATEMACHINEGRAPH_API virtual void PostLoad() override;
	SCENESTATEMACHINEGRAPH_API virtual void PostDuplicate(bool bInDuplicateForPIE) override;
	SCENESTATEMACHINEGRAPH_API virtual void PostEditImport() override;
	//~ End UObject

	/** Called to set a new unique id for parameters (e.g. after duplicating) */
	void GenerateNewParametersId();

	/** Identifier for the Parameters Struct Id */
	UPROPERTY(VisibleAnywhere, Category="Scene State")
	FGuid ParametersId;

	UPROPERTY(EditAnywhere, Category="Scene State")
	FInstancedPropertyBag Parameters;

	/** The run-mode for the State Machine. Currently only applies to Top-Level State Machines */
	UPROPERTY(EditAnywhere, Category="Scene State")
	ESceneStateMachineRunMode RunMode = ESceneStateMachineRunMode::Auto;

private:
	static FOnParametersChanged OnParametersChangedDelegate;
};
