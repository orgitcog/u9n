// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class ITableRow;
class STableViewBase;
template<typename ItemType> class STreeView;
class USkeletonTemplate;

namespace UE::Anim::STF
{
	class SAttributeSetsTreeView : public SCompoundWidget
	{
	public:
		DECLARE_DELEGATE(FOnNamedAttributeSetsChanged);
		
		SLATE_BEGIN_ARGS(SAttributeSetsTreeView) { }
			SLATE_EVENT(FOnNamedAttributeSetsChanged, OnNamedAttributeSetsChanged);
		SLATE_END_ARGS()

		struct FTreeItem
		{
			enum class EType
			{
				AttributeSet,
				Attribute,
			};

			EType ItemType;
			FName Name;
			FName Type;
			TSharedPtr<FTreeItem> SetItem;
			TSharedPtr<FTreeItem> Parent;
			TArray<TSharedPtr<FTreeItem>> Children;

			DECLARE_DELEGATE(FOnRenameRequested);
			FOnRenameRequested OnRenameRequested;
		};

		void Construct(const FArguments& InArgs, TObjectPtr<USkeletonTemplate> InSkeletonTemplate);

	private:
		void TreeView_HandleGetChildren(TSharedPtr<FTreeItem> InItem, TArray<TSharedPtr<FTreeItem>>& OutChildren) const;
		
		void TreeView_OnItemScrolledIntoView(TSharedPtr<FTreeItem> InItem, const TSharedPtr<ITableRow>& InWidget);

		TSharedRef<ITableRow> TreeView_GenerateItemRow(TSharedPtr<FTreeItem> Item, const TSharedRef<STableViewBase>& OwnerTable);
		
		TSharedPtr<SWidget> TreeView_HandleContextMenuOpening();

	private:
		TArray<TSharedPtr<FTreeItem>> GetAllTreeItems();

		void RegenerateTreeViewItems();

	private:
		TSharedPtr<STreeView<TSharedPtr<FTreeItem>>> TreeView;

		TArray<TSharedPtr<FTreeItem>> RootItems;

		TSharedPtr<FTreeItem> DeferredRenameRequest;

		TObjectPtr<USkeletonTemplate> SkeletonTemplate;
		
		FOnNamedAttributeSetsChanged OnNamedAttributeSetsChanged;
	};
}