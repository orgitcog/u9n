// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTreeScopedEditorDataFixer.h"

#include "Customizations/StateTreeEditorNodeUtils.h"
#include "StateTreeEditorData.h"

namespace UE::StateTreeEditor
{
FScopedEditorDataFixer::~FScopedEditorDataFixer()
{
	auto CopyBindings = [](FStateTreeEditorPropertyBindings& Bindings, const FStateTreeEditorNode& EditorNode, const FGuid& CurrentID)
		{
			if (CurrentID.IsValid())
			{
				Bindings.CopyBindings(CurrentID, EditorNode.ID);
			}
		};

	auto ReinstantiateEditorNodeInstanceData = [](TNotNull<UObject*> NodeOwner, FStateTreeEditorNode& EditorNode)
		{
			EditorNodeUtils::InstantiateStructSubobjects(*NodeOwner, EditorNode.Node);
			if (EditorNode.InstanceObject)
			{
				EditorNode.InstanceObject = DuplicateObject(EditorNode.InstanceObject, NodeOwner);
			}
			else
			{
				EditorNodeUtils::InstantiateStructSubobjects(*NodeOwner, EditorNode.Instance);
			}
		};

	FStateTreeEditorPropertyBindings* Bindings = EditorData->GetPropertyEditorBindings();
	for (FEditorNodeDataFixItem& EditorNodeFixInfo : EditorNodesToFix)
	{
		UObject& NodeOwner = *EditorNodeFixInfo.NodeOwner;
		FStateTreeEditorNode& EditorNode = EditorNodeFixInfo.Node;

		FGuid CurrentID = EditorNode.ID;
		if (EditorNodeFixInfo.bShouldRegenerateGUID)
		{
			EditorNode.ID = FGuid::NewGuid();
		}

		if (EditorNodeFixInfo.bShouldCopyBindings && Bindings != nullptr)
		{
			CopyBindings(*Bindings, EditorNode, CurrentID);
		}

		if (EditorNodeFixInfo.bShouldReinstantiateInstanceData)
		{
			ReinstantiateEditorNodeInstanceData(&NodeOwner, EditorNode);
		}
	}

	for (FTransitionDataFixItem& TransitionFixInfo : TransitionsToFix)
	{
		UObject& TransitionOwner = *TransitionFixInfo.TransitionOwner;
		FStateTreeTransition& Transition = TransitionFixInfo.Transition;

		FGuid CurrentTransitionID = Transition.ID;
		TArray<FGuid> CurrentConditionIDs;
		CurrentConditionIDs.Reserve(Transition.Conditions.Num());
		for (FStateTreeEditorNode& ConditionNode : Transition.Conditions)
		{
			CurrentConditionIDs.Add(ConditionNode.ID);
		}

		if (TransitionFixInfo.bShouldRegenerateGUID)
		{
			Transition.ID = FGuid::NewGuid();

			for (FStateTreeEditorNode& ConditionNode : Transition.Conditions)
			{
				ConditionNode.ID = FGuid::NewGuid();
			}
		}

		if (TransitionFixInfo.bShouldCopyBindings && Bindings != nullptr)
		{
			for (int32 Index = 0; Index < Transition.Conditions.Num(); ++Index)
			{
				CopyBindings(*Bindings, Transition.Conditions[Index], CurrentConditionIDs[Index]);
			}
		}

		if (TransitionFixInfo.bShouldReinstantiateInstanceData)
		{
			for (int32 Index = 0; Index < Transition.Conditions.Num(); ++Index)
			{
				ReinstantiateEditorNodeInstanceData(&TransitionOwner, Transition.Conditions[Index]);
			}
		}
	}

	if (bRemoveInvalidBindings && Bindings != nullptr)
	{
		TMap<FGuid, const FPropertyBindingDataView> AllStructValues;
		EditorData->GetAllStructValues(AllStructValues);
		Bindings->RemoveInvalidBindings(AllStructValues);
	}
}
}
