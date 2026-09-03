// Copyright Epic Games, Inc. All Rights Reserved.

#include "SceneStateMachineNodeNameValidator.h"
#include "Nodes/SceneStateMachineNode.h"
#include "SceneStateMachineGraph.h"

namespace UE::SceneState::Graph
{

FStateMachineNodeNameValidator::FStateMachineNodeNameValidator(const USceneStateMachineNode* InNode)
{
	check(InNode);

	TArray<USceneStateMachineNode*> Nodes;

	USceneStateMachineGraph* Graph = CastChecked<USceneStateMachineGraph>(InNode->GetOuter());
	Graph->GetNodesOfClass<USceneStateMachineNode>(Nodes);

	Names.Reserve(Nodes.Num());
	for (USceneStateMachineNode* Node : Nodes)
	{
		if (Node != InNode)
		{
			Names.Add(Node->GetNodeName());
		}
	}
}

EValidatorResult FStateMachineNodeNameValidator::IsValid(const FName& InName, bool bInOriginal)
{
	if (InName.IsNone())
	{
		return EValidatorResult::EmptyName;
	}

	if (Names.Contains(InName))
	{
		return EValidatorResult::AlreadyInUse;
	}

	return EValidatorResult::Ok;
}

EValidatorResult FStateMachineNodeNameValidator::IsValid(const FString& InName, bool bInOriginal)
{
	return IsValid(FName(InName), bInOriginal);
}

} // UE::SceneState::Graph
