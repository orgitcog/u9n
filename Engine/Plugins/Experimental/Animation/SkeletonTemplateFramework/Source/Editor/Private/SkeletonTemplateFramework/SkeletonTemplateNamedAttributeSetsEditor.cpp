// Copyright Epic Games, Inc. All Rights Reserved.

#include "SkeletonTemplateNamedAttributeSetsEditor.h"

#include "DragAndDrop/DecoratedDragDropOp.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Modules/ModuleManager.h"
#include "SkeletonTemplateFramework/SkeletonTemplate.h"
#include "SkeletonTemplateFramework/SkeletonTemplateNamedAttributesEditor.h"
#include "StructViewerFilter.h"
#include "StructViewerModule.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/Views/STreeView.h"

#define LOCTEXT_NAMESPACE "SkeletonTemplateNamedAttributeSetsEditor"

namespace UE::Anim::STF
{
	class FNamedElementDragDropOp;

	struct FTemplateNamedAttributeSetsColumns
	{
		static const FName NameId;
		static const FName TypeId;
	};

	const FName FTemplateNamedAttributeSetsColumns::NameId(TEXT("Name"));
	const FName FTemplateNamedAttributeSetsColumns::TypeId(TEXT("Type"));

	class SNamedElementSetTableRow : public SMultiColumnTableRow<TSharedPtr<SAttributeSetsTreeView::FTreeItem>>
	{
	public:
		DECLARE_DELEGATE_RetVal_TwoParams(bool /* Success */, FOnRenamed, FName /* Old Name */, FName /* New Name */);
		DECLARE_DELEGATE_TwoParams(FOnAddAttributeToSet, FName /* Attribute Set Name */, FName /* Attribute Name */);

		SLATE_BEGIN_ARGS(SNamedElementSetTableRow) { }
			SLATE_ARGUMENT(TSharedPtr<SAttributeSetsTreeView::FTreeItem>, TreeItem)
			SLATE_EVENT(FOnRenamed, OnRenamed);
			SLATE_EVENT(FOnAddAttributeToSet, OnAddAttributeToSet)
		SLATE_END_ARGS()

		TOptional<EItemDropZone> OnCanAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone, TSharedPtr<SAttributeSetsTreeView::FTreeItem> TargetItem)
		{
			using namespace UE::Anim::STF;

			TOptional<EItemDropZone> ReturnedDropZone;
	
			const TSharedPtr<FNamedElementDragDropOp> DragDropOp = DragDropEvent.GetOperationAs<FNamedElementDragDropOp>();
			if (DragDropOp.IsValid() && TreeItem->ItemType == SAttributeSetsTreeView::FTreeItem::EType::AttributeSet)
			{
        		ReturnedDropZone = EItemDropZone::BelowItem;
			}
	
			return ReturnedDropZone;
		}

		FReply OnAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone, TSharedPtr<SAttributeSetsTreeView::FTreeItem> TargetItem)
		{
			const TSharedPtr<FNamedElementDragDropOp> DragDropOp = DragDropEvent.GetOperationAs<FNamedElementDragDropOp>();
			if (!DragDropOp.IsValid())
			{
				return FReply::Unhandled();
			}

			OnAddAttributeToSet.ExecuteIfBound(TreeItem->Name, DragDropOp->NamedAttribute);

			return FReply::Handled();
		}

		void OnCommitRename(const FText& InText, ETextCommit::Type CommitInfo)
		{
			const FName OldName = TreeItem->Name;
			const FName NewName = FName(InText.ToString());

			if (OnRenamed.IsBound())
			{
				const bool bSuccess = OnRenamed.Execute(OldName, NewName);
				if (bSuccess)
				{
					TreeItem->Name = NewName;
				}
			}
		}

		void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
		{
			TreeItem = InArgs._TreeItem;
			OnRenamed = InArgs._OnRenamed;
			OnAddAttributeToSet = InArgs._OnAddAttributeToSet;

			FSuperRowType::Construct(
				FSuperRowType::FArguments()
					.OnCanAcceptDrop(this, &SNamedElementSetTableRow::OnCanAcceptDrop)
					.OnAcceptDrop(this, &SNamedElementSetTableRow::OnAcceptDrop),
				InOwnerTableView);
		}

		TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
		{
			if (ColumnName == FTemplateNamedAttributeSetsColumns::NameId)
			{
				TSharedPtr<SInlineEditableTextBlock> InlineWidget;

				TSharedRef<SHorizontalBox> HorizontalBox = SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(SExpanderArrow, SharedThis(this))
								.ShouldDrawWires(true)
						]
					+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(4.0f, 0.0f, 0.0f, 0.0f)
						[
							SNew(SImage)
								.Image(FAppStyle::GetBrush("LevelEditor.Tabs.Outliner"))
								.Visibility(TreeItem->ItemType == SAttributeSetsTreeView::FTreeItem::EType::AttributeSet ? EVisibility::Visible : EVisibility::Collapsed)
						]
					+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(4.0f, 0.0f, 0.0f, 0.0f)
						[
							SAssignNew(InlineWidget, SInlineEditableTextBlock)
								.Text_Lambda([this]()
								{
									return FText::FromName(TreeItem->Name);
								})
								.OnTextCommitted(this, &SNamedElementSetTableRow::OnCommitRename)
						];

				TreeItem->OnRenameRequested.BindSP(InlineWidget.Get(), &SInlineEditableTextBlock::EnterEditingMode);

				return HorizontalBox;
			}

			if (ColumnName == FTemplateNamedAttributeSetsColumns::TypeId)
			{
				if (TreeItem->ItemType == SAttributeSetsTreeView::FTreeItem::EType::AttributeSet)
				{
					return SNullWidget::NullWidget;
				}
				else
				{
					return SNew(STextBlock)
						.Text_Lambda([this]()
							{
								return FText::FromName(TreeItem->Type);
							});
				}
			}
			
			return SNullWidget::NullWidget;
		}

	private:
		TSharedPtr<SAttributeSetsTreeView::FTreeItem> TreeItem;
		FOnAddAttributeToSet OnAddAttributeToSet;
		FOnRenamed OnRenamed;
	};

	void SAttributeSetsTreeView::Construct(const FArguments& InArgs, TObjectPtr<USkeletonTemplate> InSkeletonTemplate)
	{
		SkeletonTemplate = InSkeletonTemplate;
		OnNamedAttributeSetsChanged = InArgs._OnNamedAttributeSetsChanged;

		ChildSlot
		[
			SAssignNew(TreeView, STreeView<TSharedPtr<FTreeItem>>)
				.TreeItemsSource(&RootItems)
				.OnGenerateRow(this, &SAttributeSetsTreeView::TreeView_GenerateItemRow)
				.OnGetChildren(this, &SAttributeSetsTreeView::TreeView_HandleGetChildren)
				.OnContextMenuOpening(this, &SAttributeSetsTreeView::TreeView_HandleContextMenuOpening)
				.OnItemScrolledIntoView(this, &SAttributeSetsTreeView::TreeView_OnItemScrolledIntoView)
				.HighlightParentNodesForSelection(true)
				.HeaderRow
				(
					SNew(SHeaderRow)
						+SHeaderRow::Column(FTemplateNamedAttributeSetsColumns::NameId)
							.FillWidth(0.5f)
							.DefaultLabel(LOCTEXT("NameLabel", "Name"))
						+SHeaderRow::Column(FTemplateNamedAttributeSetsColumns::TypeId)
							.FillWidth(0.5f)
							.DefaultLabel(LOCTEXT("TypeLabel", "Type"))
				)
		];

		RegenerateTreeViewItems();

		// Expand all tree items on construction
		for (TSharedPtr<FTreeItem> TreeItem : GetAllTreeItems())
		{
			TreeView->SetItemExpansion(TreeItem, true);
		}
	}

	void SAttributeSetsTreeView::TreeView_HandleGetChildren(TSharedPtr<FTreeItem> InItem, TArray<TSharedPtr<FTreeItem>>& OutChildren) const
	{
		OutChildren.Append(InItem->Children);
	}

	void SAttributeSetsTreeView::TreeView_OnItemScrolledIntoView(TSharedPtr<FTreeItem> InItem, const TSharedPtr<ITableRow>& InWidget)
	{
		if (DeferredRenameRequest.IsValid())
		{
			DeferredRenameRequest->OnRenameRequested.ExecuteIfBound();
			DeferredRenameRequest.Reset();
		}
	}

	TSharedRef<ITableRow> SAttributeSetsTreeView::TreeView_GenerateItemRow(TSharedPtr<SAttributeSetsTreeView::FTreeItem> Item, const TSharedRef<STableViewBase>& OwnerTable)
	{
		if (Item->ItemType == FTreeItem::EType::AttributeSet)
		{
			return SNew(SNamedElementSetTableRow, OwnerTable)
				.TreeItem(Item)
				.OnAddAttributeToSet_Lambda([this, Item](const FName AttributeSet, const FName AttributeName)
					{
						SkeletonTemplate->AddAttributeToSet(AttributeSet, AttributeName);					
						RegenerateTreeViewItems();

						for (TSharedPtr<FTreeItem> TreeItem : GetAllTreeItems())
						{
							if (TreeItem->Name == AttributeName)
							{
								TSharedPtr<FTreeItem> ParentTreeItem = TreeItem->Parent;
								while (ParentTreeItem)
								{
									TreeView->SetItemExpansion(ParentTreeItem, true);
									ParentTreeItem = ParentTreeItem->Parent;
								}
								
								break;
							}
						}

						OnNamedAttributeSetsChanged.ExecuteIfBound();
					})
				.OnRenamed_Lambda([this](const FName OldName, const FName NewName) -> bool
				{
					const bool bSuccess = SkeletonTemplate->RenameNamedAttributeSet(OldName, NewName);
					if (bSuccess)
					{
						OnNamedAttributeSetsChanged.ExecuteIfBound();
					}
					return bSuccess;
				});
		}
		else
		{
			return SNew(SNamedElementSetTableRow, OwnerTable)
				.TreeItem(Item);
		}
	}

	TSharedPtr<SWidget> SAttributeSetsTreeView::TreeView_HandleContextMenuOpening()
	{
		FMenuBuilder MenuBuilder(true, nullptr);

		TArray<TSharedPtr<FTreeItem>> SelectedItems;
		TreeView->GetSelectedItems(SelectedItems);

		if (!SelectedItems.IsEmpty())
		{
			TSharedPtr<FTreeItem> SelectedItem = SelectedItems[0];

			switch (SelectedItem->ItemType)
			{
				case FTreeItem::EType::AttributeSet:
				{
					MenuBuilder.AddMenuEntry(
						LOCTEXT("RenameNamedAttributeSet", "Rename Named Attribute Set"),
						LOCTEXT("RenameNamedAttributeSet_Tooltip", "Renames the selected named attribute set"),
						FSlateIcon(FAppStyle::GetAppStyleSetName(), "GenericCommands.Rename"),
						FUIAction(
							FExecuteAction::CreateLambda([this, SelectedItem]()
							{
								SelectedItem->OnRenameRequested.ExecuteIfBound();
							})
						));

					MenuBuilder.AddMenuEntry(
						LOCTEXT("DeleteNamedAttributeSet", "Delete Named Attribute Set"),
						LOCTEXT("DeleteNamedAttributeSet_Tooltip", "Deletes the selected named attribute set"),
						FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Delete"),
						FUIAction(
							FExecuteAction::CreateLambda([this, SelectedItem]()
							{
								SkeletonTemplate->RemoveAttributeSet(SelectedItem->Name);
								RegenerateTreeViewItems();
								OnNamedAttributeSetsChanged.ExecuteIfBound();
							})
						));

					break;
				}
				case FTreeItem::EType::Attribute:
				{
					MenuBuilder.AddMenuEntry(
						LOCTEXT("AddNamedAttribute", "Remove Named Attribute"),
						LOCTEXT("AddNamedAttribute_Tooltip", "Remove the selected named attribute from the set"),
						FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Minus"),
						FUIAction(
							FExecuteAction::CreateLambda([this, SelectedItem]()
							{
								SkeletonTemplate->RemoveAttributeFromSet(SelectedItem->SetItem->Name, SelectedItem->Name);
								RegenerateTreeViewItems();
							})
						));

					break;
				}
			}
		}
		else
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT("AddNamedAttributeSet", "Add Named Attribute Set"),
				LOCTEXT("AddNamedAttributeSet_Tooltip", "Add a new named attribute set"),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Plus"),
				FUIAction(
					FExecuteAction::CreateLambda([this]()
					{
						SkeletonTemplate->AddAttributeSet("NewAttributeSet");
						RegenerateTreeViewItems();

						for (TSharedPtr<FTreeItem> Item : GetAllTreeItems())
						{
							if (Item->ItemType == FTreeItem::EType::AttributeSet && Item->Name == "NewAttributeSet")
							{
								TreeView->RequestScrollIntoView(Item);
								DeferredRenameRequest = Item;
							}
						}

						OnNamedAttributeSetsChanged.ExecuteIfBound();
					})
				));
		}

		return MenuBuilder.MakeWidget();
	}

	TArray<TSharedPtr<UE::Anim::STF::SAttributeSetsTreeView::FTreeItem>> SAttributeSetsTreeView::GetAllTreeItems()
	{
		using namespace UE::Anim::STF;

		TArray<TSharedPtr<SAttributeSetsTreeView::FTreeItem>> AllItems;
		AllItems.Append(RootItems);
	
		for (int32 Index = 0; Index < AllItems.Num(); ++Index)
		{
			AllItems.Append(AllItems[Index]->Children);
		}

		return AllItems;
	}

	void SAttributeSetsTreeView::RegenerateTreeViewItems()
	{
		// Make note of all tree items currently expanded
		TSet<FName> ExpandedAttributeNames;
		{
			for (TSharedPtr<FTreeItem> TreeItem : GetAllTreeItems())
			{
				if (TreeView->IsItemExpanded(TreeItem))
				{
					ExpandedAttributeNames.Add(TreeItem->Name);
				}
			}
		}

		// Rebuild items
		{
			RootItems.Reset();
			TMap<FName, TSharedPtr<FTreeItem>> AttributesItemMap;
	
			for (const FSkeletonNamedAttributeSet& NamedAttributeSet : SkeletonTemplate->GetNamedAttributeSets())
			{
				TSharedPtr<FTreeItem> SetItem = MakeShared<FTreeItem>();
				SetItem->Name = NamedAttributeSet.Name;
				SetItem->ItemType = FTreeItem::EType::AttributeSet;
				SetItem->Type = NAME_None;

				for (const FName& AttributeName : NamedAttributeSet.NamedAttributes)
				{
					const FSkeletonNamedAttribute* NamedAttribute = SkeletonTemplate->FindNamedAttribute(AttributeName);
					if (!ensure(NamedAttribute))
					{
						continue;
					}

					TSharedPtr<FTreeItem>* ParentItem = AttributesItemMap.Find(NamedAttribute->ParentName);

					TSharedPtr<FTreeItem> AttributeItem = MakeShared<FTreeItem>();
					AttributeItem->Name = AttributeName;
					AttributeItem->ItemType = FTreeItem::EType::Attribute;
					AttributeItem->Type = NamedAttribute->Type.GetFName();
					AttributeItem->SetItem = SetItem;

					if (ParentItem)
					{
						AttributeItem->Parent = *ParentItem;
						(*ParentItem)->Children.Add(AttributeItem);
					}
					else
					{
						AttributeItem->Parent = SetItem;
						SetItem->Children.Add(AttributeItem);
					}
					
					AttributesItemMap.Add(AttributeName, AttributeItem);
				}

				RootItems.Add(SetItem);
			}
		}

		// Update tree view and restore tree item expanded states
		{
			check(TreeView);
			TreeView->RequestTreeRefresh();

			for (TSharedPtr<FTreeItem> TreeItem : GetAllTreeItems())
			{
				if (ExpandedAttributeNames.Contains(TreeItem->Name))
				{
					TreeView->SetItemExpansion(TreeItem, true);
				}
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE