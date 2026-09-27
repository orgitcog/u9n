// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class IDetailsView;
class ITableRow;
class STableViewBase;
template<typename ItemType> class SListView;
template<typename ItemType> class STreeView;
class USkeletonTemplate;

class ISkeletonTemplateEditorToolkit;

namespace UE::Anim::STF
{
	class SNamedElementMappingTableRow;
	
	class SAttributeMappingsTreeView : public SCompoundWidget
	{
	public:
		friend SNamedElementMappingTableRow;
		
		SLATE_BEGIN_ARGS(SAttributeMappingsTreeView) { }
		SLATE_END_ARGS()

		struct FListItem
		{
			FName Name;
			FName SourceSetName;

			DECLARE_DELEGATE(FOnRenameRequested);
			FOnRenameRequested OnRenameRequested;
		};
		
		struct ITreeItem
		{
			virtual ~ITreeItem() = default;
			
			FName AttributeName;
			FText AttributeType;
			bool bHasValue;
			SAttributeMappingsTreeView* TreeView;
			
			virtual void GetChildren(TArray<TSharedPtr<ITreeItem>>& OutChildren) const = 0;
		};

		friend ITreeItem;
		
		void Construct(const FArguments& InArgs, TObjectPtr<USkeletonTemplate> InSkeletonTemplate, ISkeletonTemplateEditorToolkit* Toolkit);

		void OnNamedAttributeSetsChanged();

	private:
		TSharedRef<ITableRow> ListView_GenerateItemRow(TSharedPtr<FListItem> Item, const TSharedRef<STableViewBase>& OwnerTable);

		void ListView_OnItemScrolledIntoView(TSharedPtr<FListItem> InItem, const TSharedPtr<ITableRow>& InWidget);

		TSharedPtr<SWidget> ListView_HandleContextMenuOpening();
		
		void ListView_OnSelectionChanged(TSharedPtr<FListItem> InItem, ESelectInfo::Type InSelectInfo);

		void TreeView_HandleGetChildren(TSharedPtr<ITreeItem> InItem, TArray<TSharedPtr<ITreeItem>>& OutChildren) const;

		void TreeView_OnSelectionChanged(TSharedPtr<ITreeItem> InItem, ESelectInfo::Type InSelectInfo);
		
		TSharedRef<ITableRow> TreeView_GenerateItemRow(TSharedPtr<ITreeItem> Item, const TSharedRef<STableViewBase>& OwnerTable);
		
		TSharedPtr<SWidget> TreeView_HandleContextMenuOpening();

	private:
		TArray<TSharedPtr<ITreeItem>> GetAllTreeItems();

		// Regenerates the cached list of named attribute sets
		void RegenerateNamedAttributeSetOptions();

		void RegenerateListViewItems();
		
		void RegenerateTreeViewItems();

	private:
		TSharedPtr<SListView<TSharedPtr<FListItem>>> ListView;

		TSharedPtr<FListItem> ListView_DeferredRenameRequest;

		TSharedPtr<STreeView<TSharedPtr<ITreeItem>>> TreeView;

		TArray<TSharedPtr<FListItem>> ListItems;
		
		FName SelectedMappingName;

		TArray<TSharedPtr<ITreeItem>> RootItems;

		TSharedPtr<ITreeItem> TreeView_DeferredRenameRequest;

		TObjectPtr<USkeletonTemplate> SkeletonTemplate;

		TArray<TSharedPtr<FName>> SourceSetOptions;

		ISkeletonTemplateEditorToolkit* Toolkit;
	};
}