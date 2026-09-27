// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ISceneStateEventHandlerProvider.h"
#include "SceneStateEventHandler.h"
#include "SceneStateMachineNode.h"
#include "SceneStateMachineStateNode.generated.h"

UCLASS(MinimalAPI, meta=(ToolTip="State node in a State Machine"))
class USceneStateMachineStateNode : public USceneStateMachineNode, public ISceneStateEventHandlerProvider
{
	GENERATED_BODY()

public:
	USceneStateMachineStateNode();

	UEdGraphPin* GetTaskPin() const
	{
		return Pins[2];
	}

	TConstArrayView<FSceneStateEventHandler> GetEventHandlers() const
	{
		return EventHandlers;
	}

	//~ Begin ISceneStateEventHandlerProvider
	virtual bool FindEventHandlerId(const FSceneStateEventSchemaHandle& InEventSchemaHandle, FGuid& OutHandlerId) const override;
	//~ End ISceneStateEventHandlerProvider

	//~ Begin USceneStateMachineNode
	virtual bool HasValidPins() const override;
	virtual UEdGraph* CreateBoundGraphInternal() override;
	//~ End USceneStateMachineNode

	//~ Begin UEdGraphNode
	virtual void AllocateDefaultPins() override;
	virtual bool CanDuplicateNode() const override;
	virtual void PostPasteNode() override;
	virtual void PostPlacedNewNode() override;
	//~ End UEdGraphNode

	//~ Begin UObject
	virtual void PostLoad() override;
	//~ End Uobject

	/** Deprecated: Graphs are now managed in the Node Base class */
	UPROPERTY()
	TObjectPtr<UEdGraph> MainGraph;

	UPROPERTY(EditAnywhere, Category="Events", meta=(NoBinding))
	TArray<FSceneStateEventHandler> EventHandlers;
};
