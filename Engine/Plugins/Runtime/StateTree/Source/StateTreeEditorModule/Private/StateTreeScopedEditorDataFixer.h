// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "UObject/WeakObjectPtr.h"
#include "Misc/NotNull.h"

class UStateTreeEditorData;
struct FStateTreeEditorNode;
struct FStateTreeTransition;

namespace UE::StateTreeEditor
{
/**
 * Helper struct for fixing up Editor data following editor manipulations
 * @todo: this should take into account fixing up data for state manipulations in the future
 */
struct FScopedEditorDataFixer
{
	TNotNull<UStateTreeEditorData*> EditorData;
	uint8 bRemoveInvalidBindings : 1 = false;

	struct FEditorNodeDataFixItem
	{
		// either State or EditorData
		TNotNull<UObject*> NodeOwner;
		FStateTreeEditorNode& Node;
		uint8 bShouldCopyBindings : 1;
		uint8 bShouldReinstantiateInstanceData : 1;
		uint8 bShouldRegenerateGUID : 1;

		explicit FEditorNodeDataFixItem(TNotNull<UObject*> InNodeOwner, FStateTreeEditorNode& InNode, bool InbShouldCopyBindings, bool InbShouldReinstantiateInstanceData, bool InbShouldRegenerateGUID)
			: NodeOwner(InNodeOwner)
			, Node(InNode)
			, bShouldCopyBindings(InbShouldCopyBindings)
			, bShouldReinstantiateInstanceData(InbShouldReinstantiateInstanceData)
			, bShouldRegenerateGUID(InbShouldRegenerateGUID)
		{
		}
	};

	struct FTransitionDataFixItem
	{
		TNotNull<UObject*> TransitionOwner;
		FStateTreeTransition& Transition;
		uint8 bShouldCopyBindings : 1;
		uint8 bShouldReinstantiateInstanceData : 1;
		uint8 bShouldRegenerateGUID : 1;

		explicit FTransitionDataFixItem(TNotNull<UObject*> InTransitionOwner, FStateTreeTransition& InTransition, bool InbShouldCopyBindings, bool InbShouldReinstantiateInstanceData, bool InbShouldRegenerateGUID)
			: TransitionOwner(InTransitionOwner)
			, Transition(InTransition)
			, bShouldCopyBindings(InbShouldCopyBindings)
			, bShouldReinstantiateInstanceData(InbShouldReinstantiateInstanceData)
			, bShouldRegenerateGUID(InbShouldRegenerateGUID)
		{
		}
	};

	TArray<FEditorNodeDataFixItem> EditorNodesToFix;
	TArray<FTransitionDataFixItem> TransitionsToFix;

	explicit FScopedEditorDataFixer(TNotNull<UStateTreeEditorData*> InEditorData)
		: EditorData(InEditorData)
	{
	}

	~FScopedEditorDataFixer();
};
}
