// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextControllerActions.h"
#include "AnimGraphUncookedOnlyUtils.h"

FAnimNextManifestAction::FAnimNextManifestAction()
	: FAnimNextBaseAction(nullptr)
{
}

FAnimNextManifestAction::FAnimNextManifestAction(UAnimNextController* InController, URigVMNode* InNode, bool bInNewIncludeInManifestState)
	: FAnimNextBaseAction(InController)
	, NodePath(InNode->GetNodePath())
	, bOldManifestState(UE::AnimNext::UncookedOnly::FAnimGraphUtils::IsExposedToManifest(InNode))
	, bIncludeInManifestState(bInNewIncludeInManifestState)
{
}

bool FAnimNextManifestAction::Merge(const FRigVMBaseAction* Other)
{
	if (!FRigVMBaseAction::Merge(Other))
	{
		return false;
	}

	const FAnimNextManifestAction* Action = (const FAnimNextManifestAction*)Other;
	if (NodePath != Action->NodePath)
	{
		return false;
	}

	bIncludeInManifestState = Action->bIncludeInManifestState;
	return true;
}

bool FAnimNextManifestAction::Undo()
{
	if (!FRigVMBaseAction::Undo())
	{
		return false;
	}

	if (bOldManifestState)
	{
		return GetAnimNextController()->AddNodeToManifestByName(*NodePath, false, false);
	}
	else
	{
		return GetAnimNextController()->RemoveNodeFromManifestByName(*NodePath, false, false);
	}
}

bool FAnimNextManifestAction::Redo()
{
	if (!CanUndoRedo())
	{
		return false;
	}

	if (bIncludeInManifestState)
	{
		if (!GetAnimNextController()->AddNodeToManifestByName(*NodePath, false, false))
		{
			return false;
		}
	}
	else
	{
		if (!GetAnimNextController()->RemoveNodeFromManifestByName(*NodePath, false, false))
		{
			return false;
		}
	}

	return FRigVMBaseAction::Redo();
}
