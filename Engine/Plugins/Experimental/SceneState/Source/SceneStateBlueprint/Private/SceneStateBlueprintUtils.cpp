// Copyright Epic Games, Inc. All Rights Reserved.

#include "SceneStateBlueprintUtils.h"
#include "Containers/ArrayView.h"
#include "EdGraph/EdGraph.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Nodes/SceneStateMachineNode.h"
#include "PropertyBagDetails.h"
#include "SceneStateBlueprint.h"
#include "SceneStateMachineGraph.h"
#include "Templates/Function.h"

namespace UE::SceneState::Graph
{

namespace Private
{

void VisitNodes(TConstArrayView<UEdGraph*> InGraphs, TFunctionRef<void(USceneStateMachineNode*, EIterationResult&)> InFunc, EIterationResult& IterationResult)
{
	for (UEdGraph* Graph : InGraphs)
	{
		if (!Graph)
		{
			continue;
		}

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (USceneStateMachineNode* StateMachineNode = Cast<USceneStateMachineNode>(Node))
			{
				IterationResult = EIterationResult::Continue;
				InFunc(StateMachineNode, IterationResult);

				if (IterationResult == EIterationResult::Break)
				{
					return;
				}

				TArray<UEdGraph*> BoundGraphs = StateMachineNode->GetSubGraphs();
				VisitNodes(BoundGraphs, InFunc, IterationResult);

				if (IterationResult == EIterationResult::Break)
				{
					return;
				}
			}
		}
	}
}

void VisitGraphs(TConstArrayView<UEdGraph*> InGraphs, TFunctionRef<void(USceneStateMachineGraph*, EIterationResult&)> InFunc, EIterationResult& IterationResult)
{
	for (UEdGraph* Graph : InGraphs)
	{
		USceneStateMachineGraph* StateMachineGraph = Cast<USceneStateMachineGraph>(Graph);
		if (!StateMachineGraph)
		{
			continue;
		}

		IterationResult = EIterationResult::Continue;
		InFunc(StateMachineGraph, IterationResult);

		if (IterationResult == EIterationResult::Break)
		{
			return;
		}

		for (UEdGraphNode* Node : StateMachineGraph->Nodes)
		{
			if (USceneStateMachineNode* StateMachineNode = Cast<USceneStateMachineNode>(Node))
			{
				IterationResult = EIterationResult::Continue;
				VisitGraphs(StateMachineNode->GetSubGraphs(), InFunc, IterationResult);

				if (IterationResult == EIterationResult::Break)
				{
					return;
				}
			}
		}
	}
}

} // Private

void VisitNodes(TConstArrayView<UEdGraph*> InGraphs, TFunctionRef<void(USceneStateMachineNode*, EIterationResult&)> InFunc)
{
	EIterationResult IterationResult = EIterationResult::Continue;
	Private::VisitNodes(InGraphs, InFunc, IterationResult);
}

void VisitGraphs(TConstArrayView<UEdGraph*> InGraphs, TFunctionRef<void(USceneStateMachineGraph*, EIterationResult&)> InFunc)
{
	EIterationResult IterationResult = EIterationResult::Continue;
	Private::VisitGraphs(InGraphs, InFunc, IterationResult);
}

void CreateBlueprintVariables(USceneStateBlueprint* InBlueprint, TArrayView<UE::PropertyBinding::FPropertyCreationDescriptor> InPropertyCreationDescs)
{
	if (!InBlueprint)
	{
		return;
	}

	for (UE::PropertyBinding::FPropertyCreationDescriptor& CreationDesc : InPropertyCreationDescs)
	{
		const FEdGraphPinType VariableType = UE::StructUtils::GetPropertyDescAsPin(CreationDesc.PropertyDesc);
		if (VariableType.PinCategory == NAME_None)
		{
			continue;
		}

		const FName MemberName = FBlueprintEditorUtils::FindUniqueKismetName(InBlueprint
			, CreationDesc.PropertyDesc.Name.ToString()
			, InBlueprint->SkeletonGeneratedClass);

		FString DefaultValue;
		if (CreationDesc.SourceProperty && CreationDesc.SourceContainerAddress)
		{
			const void* SourceValue = CreationDesc.SourceProperty->ContainerPtrToValuePtr<void>(CreationDesc.SourceContainerAddress);
			CreationDesc.SourceProperty->ExportText_Direct(DefaultValue, SourceValue, SourceValue, nullptr, PPF_None);
		}

		if (FBlueprintEditorUtils::AddMemberVariable(InBlueprint, MemberName, VariableType, DefaultValue))
		{
			CreationDesc.PropertyDesc.Name = MemberName;
		}
	}
}

} // namespace UE::SceneState::Graph
