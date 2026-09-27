// Copyright Epic Games, Inc. All Rights Reserved.

#include "Nodes/SceneStateMachineStateNode.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Nodes/SceneStateMachineTaskNode.h"
#include "SceneStateMachineGraph.h"
#include "SceneStateMachineGraphSchema.h"

USceneStateMachineStateNode::USceneStateMachineStateNode()
{
	NodeName = TEXT("State");
	NodeType = UE::SceneState::Graph::EStateMachineNodeType::State;

	bCanRenameNode = true;
}

bool USceneStateMachineStateNode::FindEventHandlerId(const FSceneStateEventSchemaHandle& InEventSchemaHandle, FGuid& OutHandlerId) const
{
	for (const FSceneStateEventHandler& EventHandler : EventHandlers)
	{
		if (EventHandler.GetEventSchemaHandle() == InEventSchemaHandle)
		{
			OutHandlerId = EventHandler.GetHandlerId();
			return true;
		}
	}

	return false;
}

bool USceneStateMachineStateNode::HasValidPins() const
{
	return Super::HasValidPins() && GetTaskPin();
}

UEdGraph* USceneStateMachineStateNode::CreateBoundGraphInternal()
{
	UEdGraph* const NewGraph = FBlueprintEditorUtils::CreateNewGraph(this
		, TEXT("SceneStateMachine")
		, USceneStateMachineGraph::StaticClass()
		, USceneStateMachineGraphSchema::StaticClass());

	check(NewGraph);

	FBlueprintEditorUtils::RenameGraphWithSuggestion(NewGraph, FNameValidatorFactory::MakeValidator(this), TEXT("SubStateMachine"));
	return NewGraph;
}

void USceneStateMachineStateNode::AllocateDefaultPins()
{
	CreatePin(EGPD_Input, USceneStateMachineGraphSchema::PC_Transition, USceneStateMachineGraphSchema::PN_In);
	CreatePin(EGPD_Output, USceneStateMachineGraphSchema::PC_Transition, USceneStateMachineGraphSchema::PN_Out);
	CreatePin(EGPD_Output, USceneStateMachineGraphSchema::PC_Task, USceneStateMachineGraphSchema::PN_Task);

	// Hide pins that should be hidden
	HidePins(MakeArrayView(&USceneStateMachineGraphSchema::PN_In, 1));
}

bool USceneStateMachineStateNode::CanDuplicateNode() const
{
	return true;
}

void USceneStateMachineStateNode::PostPasteNode()
{
	Super::PostPasteNode();
	GenerateNewNodeName();
}

void USceneStateMachineStateNode::PostPlacedNewNode()
{
	Super::PostPlacedNewNode();
	GenerateNewNodeName();
}

void USceneStateMachineStateNode::PostLoad()
{
	Super::PostLoad();

	// Hide pins that should be hidden
	HidePins(MakeArrayView(&USceneStateMachineGraphSchema::PN_In, 1));

	// Move main graph to bound graph
	if (MainGraph && MainGraph->GetOuter() == this)
	{
		BoundGraphs.Empty(0);
		BoundGraphs.Add(MainGraph);
	}
	MainGraph = nullptr;
}
