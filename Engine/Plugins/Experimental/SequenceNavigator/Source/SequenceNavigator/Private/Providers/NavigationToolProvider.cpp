// Copyright Epic Games, Inc. All Rights Reserved.

#include "Providers/NavigationToolProvider.h"
#include "Filters/Filters/NavigationToolBuiltInFilterParams.h"
#include "Items/NavigationToolItemUtils.h"
#include "NavigationTool.h"
#include "NavigationToolSettings.h"
#include "NavigationToolView.h"
#include "SequenceNavigatorLog.h"

namespace UE::SequenceNavigator
{

void FNavigationToolProvider::OnExtendColumnViews(TSet<FNavigationToolColumnView>& OutColumnViews)
{
	INavigationToolProvider::OnExtendColumnViews(OutColumnViews);

	ExtendedColumnViewNames.Reset(OutColumnViews.Num());
	for (const FNavigationToolColumnView& ColumnView : OutColumnViews)
	{
		ExtendedColumnViewNames.Add(ColumnView.ViewName);
	}
}

void FNavigationToolProvider::OnExtendBuiltInFilters(TArray<FNavigationToolBuiltInFilterParams>& OutFilterParams)
{
	INavigationToolProvider::OnExtendBuiltInFilters(OutFilterParams);

	ExtendedBuiltInFilterNames.Reset(OutFilterParams.Num());
	for (const FNavigationToolBuiltInFilterParams& FilterParams : OutFilterParams)
	{
		ExtendedBuiltInFilterNames.Add(FilterParams.GetFilterId());
	}
}

void FNavigationToolProvider::UpdateItemIdContexts(const INavigationTool& InTool)
{
	FNavigationToolSaveState* const SaveState = GetSaveState(InTool);
	if (!SaveState)
	{
		return;
	}

	const FString ContextString = GetIdentifier().ToString();

	// Already updated
	if (SaveState->ContextPath == ContextString)
	{
		return;
	}

	ON_SCOPE_EXIT
	{
		SaveState->ContextPath = ContextString;
	};

	auto FixItemIdMap = [&ContextPath = SaveState->ContextPath, &ContextString]<typename InValueType>(TMap<FString, InValueType>& InItemIdMap)
		{
			TMap<FString, InValueType> ItemIdMapTemp = InItemIdMap;
			for (TPair<FString, InValueType>& Pair : ItemIdMapTemp)
			{
				FSoftObjectPath ObjectPath;
				ObjectPath.SetPath(ContextPath);

				FString AssetPath = ObjectPath.GetAssetPath().ToString();
				if (AssetPath.IsEmpty() || Pair.Key.StartsWith(AssetPath))
				{
					InItemIdMap.Remove(Pair.Key);

					Pair.Key.RemoveFromStart(AssetPath);
					Pair.Key.RemoveFromStart(TEXT(":"));

					FSoftObjectPath NewPath;
					NewPath.SetPath(ContextString);
					NewPath.SetSubPathString(Pair.Key);

					InItemIdMap.Add(NewPath.ToString(), Pair.Value);
				}
			}
		};

	FixItemIdMap(SaveState->ItemColorMap);

	for (FNavigationToolViewSaveState& ViewState : SaveState->ToolViewSaveStates)
	{
		FixItemIdMap(ViewState.ViewItemFlags);
	}
}

FNavigationToolViewSaveState* FNavigationToolProvider::GetViewSaveState(const INavigationTool& InTool, const int32 InToolViewId) const
{
	FNavigationToolSaveState* const SaveState = GetSaveState(InTool);
	if (!SaveState)
	{
		UE_LOG(LogSequenceNavigator, Warning, TEXT("GetViewSaveState(): Save state is NULL!"));
		return nullptr;
	}

	if (!SaveState->ToolViewSaveStates.IsValidIndex(InToolViewId))
	{
		UE_LOG(LogSequenceNavigator, Warning, TEXT("GetViewSaveState(): Invalid tool view Id: %d"), InToolViewId);
		return nullptr;
	}

	return &SaveState->ToolViewSaveStates[InToolViewId];
}

void FNavigationToolProvider::EnsureToolViewCount(const INavigationTool& InTool, const int32 InToolViewId)
{
	FNavigationToolSaveState* const SaveState = GetSaveState(InTool);
	if (!SaveState)
	{
		UE_LOG(LogSequenceNavigator, Warning, TEXT("EnsureToolViewCount(): Save state is NULL!"));
		return;
	}

	const int32 CurrentCount = SaveState->ToolViewSaveStates.Num();
	const int32 MinViewCount = InToolViewId + 1;

	if (CurrentCount < MinViewCount)
	{
		SaveState->ToolViewSaveStates.AddDefaulted(MinViewCount - CurrentCount);
	}
}

TOptional<EItemDropZone> FNavigationToolProvider::OnToolItemCanAcceptDrop(const FDragDropEvent& InDragDropEvent
	, EItemDropZone InDropZone
	, const FNavigationToolItemPtr& InTargetItem) const
{
	return TOptional<EItemDropZone>();
}

FReply FNavigationToolProvider::OnToolItemAcceptDrop(const FDragDropEvent& InDragDropEvent
	, EItemDropZone InDropZone
	, const FNavigationToolItemPtr& InTargetItem)
{
	return FReply::Unhandled();
}

void FNavigationToolProvider::Activate(FNavigationTool& InTool)
{
	const FName ProviderId = GetIdentifier();
	UE_LOG(LogSequenceNavigator, Log, TEXT("Activating provider: %s"), *ProviderId.ToString());

	if (const TSharedPtr<FUICommandList> BaseCommandList = InTool.GetBaseCommandList())
	{
		BindCommands(BaseCommandList.ToSharedRef());
	}

	LoadState(InTool);

	const TSharedRef<FNavigationToolProvider> SharedThisRef = SharedThis(this);

	InTool.ForEachToolView([&SharedThisRef](const TSharedRef<FNavigationToolView>& InToolView)
		{
			InToolView->CreateColumns(SharedThisRef);
			InToolView->CreateDefaultColumnViews(SharedThisRef);
		});

	OnActivate();
}

void FNavigationToolProvider::Deactivate(FNavigationTool& InTool)
{
	const FName ProviderId = GetIdentifier();
	UE_LOG(LogSequenceNavigator, Log, TEXT("Deactivating provider: %s"), *ProviderId.ToString());

	CleanupExtendedColumnViews();

	OnDeactivate();
}

void FNavigationToolProvider::CleanupExtendedColumnViews()
{
	UNavigationToolSettings* const ToolSettings = GetMutableDefault<UNavigationToolSettings>();
	if (!ToolSettings)
	{
		return;
	}

	TSet<FNavigationToolColumnView>& CustomColumnViews = ToolSettings->GetCustomColumnViews();

	for (const FText& ColumnViewName : ExtendedColumnViewNames)
	{
		CustomColumnViews.Remove(ColumnViewName);
	}

	ToolSettings->SaveConfig();
}

bool FNavigationToolProvider::IsSequenceSupported(UMovieSceneSequence* const InSequence) const
{
	if (InSequence)
	{
		return GetSupportedSequenceClasses().Contains(InSequence->GetClass());
	}
	return false;
}

void FNavigationToolProvider::SaveState(FNavigationTool& InTool)
{
	const FName ProviderId = GetIdentifier();
	UE_LOG(LogSequenceNavigator, Log, TEXT("Saving provider state: %s"), *ProviderId.ToString());

	SaveSerializedTree(InTool, /*bInResetTree*/true);

	const TSharedRef<FNavigationToolProvider> SharedThisRef = SharedThis(this);

	InTool.ForEachToolView([&SharedThisRef](const TSharedPtr<FNavigationToolView>& InToolView)
		{
			InToolView->SaveViewState(SharedThisRef);
		});

	if (FNavigationToolSaveState* const SaveState = GetSaveState(InTool))
	{
		// Remove any saved item colors that can no longer be found
		TArray<FString> ItemIds;
		SaveState->ItemColorMap.GetKeys(ItemIds);
		for (const FString& ItemId : ItemIds)
		{
			if (!InTool.FindItem(ItemId).IsValid())
			{
				SaveState->ItemColorMap.Remove(ItemId);
			}
		}
	}

	UpdateItemIdContexts(InTool);
}

void FNavigationToolProvider::LoadState(FNavigationTool& InTool)
{
	const FName ProviderId = GetIdentifier();
	UE_LOG(LogSequenceNavigator, Log, TEXT("Loading provider state: %s"), *ProviderId.ToString());

	const UNavigationToolSettings* const ToolSettings = GetDefault<UNavigationToolSettings>();
	if (!ToolSettings)
	{
		return;
	}

	UpdateItemIdContexts(InTool);

	if (FNavigationToolSaveState* const SaveState = GetSaveState(InTool))
	{
		LoadSerializedTree(InTool.GetTreeRoot(), &SaveState->SerializedTree);
	}

	const TSharedRef<FNavigationToolProvider> SharedThisRef = SharedThis(this);

	InTool.ForEachToolView([&SharedThisRef](const TSharedPtr<FNavigationToolView>& InToolView)
		{
			InToolView->LoadViewState(SharedThisRef);
		});
}

void FNavigationToolProvider::SaveSerializedTree(FNavigationTool& InTool, const bool bInResetTree)
{
	FNavigationToolSaveState* const SaveState = GetSaveState(InTool);
	if (!SaveState)
	{
		return;
	}

	if (bInResetTree)
	{
		SaveState->SerializedTree.Reset();
	}

	SaveSerializedTreeRecursive(InTool.GetTreeRoot(), SaveState->SerializedTree);
}

void FNavigationToolProvider::SaveSerializedTreeRecursive(const FNavigationToolItemPtr& InParentItem
	, FNavigationToolSerializedTree& InSerializedTree)
{
	const TArray<FNavigationToolItemPtr>& Children = InParentItem->GetChildren();

	const FNavigationToolSerializedItem ParentSceneItem = InParentItem->MakeSerializedItem();//MakeSerializedItemFromToolItem(InParentItem);

	for (int32 Index = 0; Index < Children.Num(); ++Index)
	{
		if (const FNavigationToolItemPtr& ChildItem = Children[Index])
		{
			if (ChildItem->ShouldSort())
			{
				const FNavigationToolSerializedItem SceneItem = ChildItem->MakeSerializedItem();
				if (SceneItem.IsValid())
				{
					InSerializedTree.GetOrAddTreeNode(SceneItem, ParentSceneItem);
				}
			}
			SaveSerializedTreeRecursive(ChildItem, InSerializedTree);
		}
	}
}

void FNavigationToolProvider::LoadSerializedTree(const FNavigationToolItemPtr& InParentItem
	, FNavigationToolSerializedTree* const InSerializedTree)
{
	TArray<FNavigationToolItemPtr>& Children = InParentItem->GetChildrenMutable();
	
	TArray<FNavigationToolItemPtr> Sortable;
	TArray<FNavigationToolItemPtr> Unsortable;
	SplitSortableAndUnsortableItems(Children, Sortable, Unsortable);

	// If Scene Tree is valid, Item Sorting should be empty as this function only takes a valid Scene Tree if
	// loaded version supports Scene Trees (i.e. when Item Sorting stops being loaded in)
	if (InSerializedTree)
	{
		Sortable.Sort([InSerializedTree](const FNavigationToolItemPtr& InItemA, const FNavigationToolItemPtr& InItemB)
			{
				const FNavigationToolSerializedTreeNode* const NodeA = InSerializedTree->FindTreeNode(InItemA->MakeSerializedItem());
				const FNavigationToolSerializedTreeNode* const NodeB = InSerializedTree->FindTreeNode(InItemB->MakeSerializedItem());
				return FNavigationToolSerializedTree::CompareTreeItemOrder(NodeA, NodeB);
			});
	}

	Children = MoveTemp(Unsortable);
	Children.Append(MoveTemp(Sortable));

	for (FNavigationToolItemPtr& Child : Children)
	{
		LoadSerializedTree(Child, InSerializedTree);
	}
}

} // namespace UE::SequenceNavigator
