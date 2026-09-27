// Copyright Epic Games, Inc. All Rights Reserved.

#include "SceneStateMachineGraphUtils.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Nodes/SceneStateMachineNode.h"

namespace UE::SceneState::Graph
{

bool CanDirectlyRemoveGraph(UEdGraph* InGraph)
{
	if (!InGraph || !InGraph->bAllowDeletion)
	{
		return false;
	}

	if (USceneStateMachineNode* const ParentNode = Cast<USceneStateMachineNode>(InGraph->GetOuter()))
	{
		return !ParentNode->GetBoundGraphs().Contains(InGraph);
	}

	return true;
}

void RemoveGraph(UEdGraph* InGraph)
{
	if (!InGraph)
	{
		return;
	}

	if (UEdGraph* const ParentGraph = InGraph->GetTypedOuter<UEdGraph>())
	{
		ParentGraph->SubGraphs.Remove(InGraph);
	}

	if (UBlueprint* const Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(InGraph))
	{
		Blueprint->LastEditedDocuments.RemoveAll(
			[InGraph](const FEditedDocumentInfo& InEditedDocumentInfo)
			{
				UObject* EditedObject = InEditedDocumentInfo.EditedObjectPath.ResolveObject();
				return EditedObject && (EditedObject == InGraph || EditedObject->IsIn(InGraph));
			});
	}
}

} //UE::SceneState::Graph
