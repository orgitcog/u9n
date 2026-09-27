// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Views/List/ObjectMixerEditorListRowData.h"

#include "ActorTreeItem.h"
#include "GameFramework/Actor.h"
#include "SSceneOutliner.h"

struct OBJECTMIXEREDITOR_API FObjectMixerEditorListRowActor : FActorTreeItem
{
	explicit FObjectMixerEditorListRowActor(
		AActor* InObject, 
		SSceneOutliner* InSceneOutliner,
		const FText& InDisplayNameOverride = FText::GetEmpty(),
		AActor* InOverrideParent = nullptr)
	: FActorTreeItem(InObject)
	, OriginalObjectSoftPtr(InObject)
	, OverrideParent(InOverrideParent)
	{
		TreeType = Type;
		RowData = FObjectMixerEditorListRowData(InSceneOutliner, InDisplayNameOverride);
	}
	
	FObjectMixerEditorListRowData RowData;
	
	/** Used in scenarios where the original object may be reconstructed or trashed, such as when running a construction script. */
	TSoftObjectPtr<AActor> OriginalObjectSoftPtr;

	/** If set, the row for this actor will be treated as the current row's parent in the hierarchy */
	TSoftObjectPtr<AActor> OverrideParent;

	/* Begin ISceneOutlinerTreeItem Implementation */
	static const FSceneOutlinerTreeItemType Type;
	virtual void OnVisibilityChanged(const bool bNewVisibility) override;
	virtual FSceneOutlinerTreeItemID GetID() const override;
	/* End ISceneOutlinerTreeItem Implementation */
};
