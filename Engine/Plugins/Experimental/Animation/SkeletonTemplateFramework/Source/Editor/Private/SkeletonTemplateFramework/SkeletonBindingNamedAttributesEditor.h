// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DragAndDrop/DecoratedDragDropOp.h"
#include "Widgets/SCompoundWidget.h"
#include "SkeletonTemplateFramework/SkeletonBinding.h"

class ITableRow;
class STableViewBase;
template<typename ItemType> class STreeView;
class USkeletonBinding;

namespace UE::Anim::STF
{
	class SAttributeBindingsTreeView : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SAttributeBindingsTreeView) { }
		SLATE_END_ARGS()

		struct FTreeItem
		{
			FName Name;
			FName Type;
			FName Binding;
			TArray<TSharedPtr<FTreeItem>> Children;

			DECLARE_DELEGATE(FOnRenameRequested);
			FOnRenameRequested OnRenameRequested;
		};

		void Construct(const FArguments& InArgs, TObjectPtr<USkeletonBinding> InSkeletonBinding);

		virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
		
	private:
		
		void TreeView_HandleGetChildren(TSharedPtr<FTreeItem> InItem, TArray<TSharedPtr<FTreeItem>>& OutChildren) const;
		
		void TreeView_OnItemScrolledIntoView(TSharedPtr<FTreeItem> InItem, const TSharedPtr<ITableRow>& InWidget);

		TSharedRef<ITableRow> TreeView_GenerateItemRow(TSharedPtr<FTreeItem> Item, const TSharedRef<STableViewBase>& OwnerTable);
		
		TSharedPtr<SWidget> TreeView_HandleContextMenuOpening();

	private:
		TArray<TSharedPtr<FTreeItem>> GetAllTreeItems();

		void RegenerateTreeViewItems();

		void HandleAddAttribute();

		void HandleRenameAttribute();

		void HandleDeleteAttribute();

		void RequestRegenerateTreeItems();

		TSharedPtr<FTreeItem> FindTreeItemByTemplateNamedAttribute(const FName TemplateNamedAttribute);

	private:
		TSharedPtr<STreeView<TSharedPtr<FTreeItem>>> TreeView;

		TArray<TSharedPtr<FTreeItem>> RootItems;

		TSharedPtr<FTreeItem> DeferredRenameRequest;

		TObjectPtr<USkeletonBinding> SkeletonBinding;

		TArray<TSharedPtr<FName>> BindingOptions;

		bool bRequestRegenerateTreeItems;
	};
}