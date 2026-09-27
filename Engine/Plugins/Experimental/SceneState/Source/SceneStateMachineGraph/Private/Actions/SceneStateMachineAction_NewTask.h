// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EdGraph/EdGraphSchema.h"

namespace UE::SceneState::Graph
{

/** Adds a new task node to the State Machine Graph */
struct FStateMachineAction_NewTask : public FEdGraphSchemaAction
{
	FStateMachineAction_NewTask() = default;

	explicit FStateMachineAction_NewTask(const UScriptStruct* InTaskStruct, int32 InGrouping);

	//~ Begin FEdGraphSchemaAction
	virtual UEdGraphNode* PerformAction(UEdGraph* InParentGraph, UEdGraphPin* InSourcePin, const FVector2f& InLocation, bool bInSelectNewNode = true) override;
	virtual void AddReferencedObjects(FReferenceCollector& InCollector) override;
	//~ End FEdGraphSchemaAction

private:
	TObjectPtr<const UScriptStruct> TaskStruct;
};

} // UE::SceneState::Graph
