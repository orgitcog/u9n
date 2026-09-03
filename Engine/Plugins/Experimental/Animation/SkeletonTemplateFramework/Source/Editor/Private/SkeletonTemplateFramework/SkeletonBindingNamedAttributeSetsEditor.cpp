// Copyright Epic Games, Inc. All Rights Reserved.

#include "SkeletonBindingNamedAttributeSetsEditor.h"

#include "DragAndDrop/DecoratedDragDropOp.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Modules/ModuleManager.h"
#include "SkeletonTemplateFramework/SkeletonBinding.h"
#include "SkeletonTemplateFramework/SkeletonTemplateNamedAttributesEditor.h" // TODO: Change for drag-drop
#include "StructViewerFilter.h"
#include "StructViewerModule.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Views/STreeView.h"

#define LOCTEXT_NAMESPACE "SkeletonBindingNamedAttributeSetsEditor"

namespace UE::Anim::STF
{
	class FNamedElementDragDropOp;

	struct FBindingSetsColumns
	{
		static const FName NameId;
		static const FName TypeId;
	};

	const FName FBindingSetsColumns::NameId(TEXT("Name"));
	const FName FBindingSetsColumns::TypeId(TEXT("Type"));

	class SBindingSetTableRow : public SMultiColumnTableRow<TSharedPtr<SBindingSetsTreeView::FTreeItem>>
	{
	public:
		DECLARE_DELEGATE_ThreeParams(FOnAddAttributeToSet, FName /* Attribute Set Name */, FName /* Attribute Name */, bool /* Include Children */);

		SLATE_BEGIN_ARGS(SBindingSetTableRow) { }
			SLATE_ARGUMENT(TSharedPtr<SBindingSetsTreeView::FTreeItem>, TreeItem)
			SLATE_EVENT(FOnAddAttributeToSet, OnAddAttributeToSet)
		SLATE_END_ARGS()

		TOptional<EItemDropZone> OnCanAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone, TSharedPtr<SBindingSetsTreeView::FTreeItem> TargetItem)
		{
			using namespace UE::Anim::STF;

			TOptional<EItemDropZone> ReturnedDropZone;
	
			const TSharedPtr<FNamedElementDragDropOp> DragDropOp = DragDropEvent.GetOperationAs<FNamedElementDragDropOp>();
			if (DragDropOp.IsValid() && TreeItem->ItemType == SBindingSetsTreeView::FTreeItem::EType::AttributeSet)
			{
        		ReturnedDropZone = EItemDropZone::BelowItem;
			}
	
			return ReturnedDropZone;
		}

		FReply OnAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone, TSharedPtr<SBindingSetsTreeView::FTreeItem> TargetItem)
		{
			const TSharedPtr<FNamedElementDragDropOp> DragDropOp = DragDropEvent.GetOperationAs<FNamedElementDragDropOp>();
			if (!DragDropOp.IsValid())
			{
				return FReply::Unhandled();
			}

			const bool bIncludeChildren = DragDropEvent.IsControlDown() || DragDropEvent.IsCommandDown();
			
			OnAddAttributeToSet.ExecuteIfBound(TreeItem->Name, DragDropOp->NamedAttribute, bIncludeChildren);

			return FReply::Handled();
		}

		void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
		{
			TreeItem = InArgs._TreeItem;
			OnAddAttributeToSet = InArgs._OnAddAttributeToSet;

			FSuperRowType::Construct(
				FSuperRowType::FArguments()
					.OnCanAcceptDrop(this, &SBindingSetTableRow::OnCanAcceptDrop)
					.OnAcceptDrop(this, &SBindingSetTableRow::OnAcceptDrop),
				InOwnerTableView);
		}

		TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
		{
			if (ColumnName == UE::Anim::STF::FBindingSetsColumns::NameId)
			{

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
								.Visibility(TreeItem->ItemType == SBindingSetsTreeView::FTreeItem::EType::AttributeSet ? EVisibility::Visible : EVisibility::Collapsed)
						]
					+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(4.0f, 0.0f, 0.0f, 0.0f)
						[
							SNew(STextBlock)
								.Text_Lambda([this]()
								{
									return FText::FromName(TreeItem->Name);
								})
						];

				return HorizontalBox;
			}
			else if (ColumnName == UE::Anim::STF::FBindingSetsColumns::TypeId)
			{
				if (TreeItem->ItemType == SBindingSetsTreeView::FTreeItem::EType::AttributeSet)
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
			else
			{
				return SNullWidget::NullWidget;
			}
		}

	private:
		TSharedPtr<SBindingSetsTreeView::FTreeItem> TreeItem;
		FOnAddAttributeToSet OnAddAttributeToSet;
	};

	void SBindingSetsTreeView::Construct(const FArguments& InArgs, TObjectPtr<USkeletonBinding> InSkeletonBinding)
	{
		SkeletonBinding = InSkeletonBinding;

		ChildSlot
		[
			SAssignNew(TreeView, STreeView<TSharedPtr<FTreeItem>>)
				.TreeItemsSource(&RootItems)
				.OnGenerateRow(this, &SBindingSetsTreeView::TreeView_GenerateItemRow)
				.OnGetChildren(this, &SBindingSetsTreeView::TreeView_HandleGetChildren)
				.OnContextMenuOpening(this, &SBindingSetsTreeView::TreeView_HandleContextMenuOpening)
				.HighlightParentNodesForSelection(true)
				.HeaderRow
				(
					SNew(SHeaderRow)
						+SHeaderRow::Column(UE::Anim::STF::FBindingSetsColumns::NameId)
							.FillWidth(0.5f)
							.DefaultLabel(LOCTEXT("NameLabel", "Name"))
						+SHeaderRow::Column(UE::Anim::STF::FBindingSetsColumns::TypeId)
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

	void SBindingSetsTreeView::TreeView_HandleGetChildren(TSharedPtr<FTreeItem> InItem, TArray<TSharedPtr<FTreeItem>>& OutChildren) const
	{
		OutChildren.Append(InItem->Children);
	}

	TSharedRef<ITableRow> SBindingSetsTreeView::TreeView_GenerateItemRow(TSharedPtr<SBindingSetsTreeView::FTreeItem> Item, const TSharedRef<STableViewBase>& OwnerTable)
	{
		if (Item->ItemType == FTreeItem::EType::AttributeSet)
		{
			return SNew(SBindingSetTableRow, OwnerTable)
				.TreeItem(Item)
				.OnAddAttributeToSet_Lambda([this, Item](const FName AttributeSet, const FName AttributeName, const bool bIncludeChildren)
					{
						SkeletonBinding->AddNamedAttributeToSet(AttributeSet, AttributeName);
						if (bIncludeChildren)
						{
							TArray<FName> ChildNamedAttributes;
							SkeletonBinding->GetChildNamedAttributes(AttributeName, ChildNamedAttributes);

							for (const FName ChildNamedAttribute : ChildNamedAttributes)
							{
								SkeletonBinding->AddNamedAttributeToSet(AttributeSet, ChildNamedAttribute);
							}
						}
					
						TSharedPtr<FTreeItem> Parent = Item->Parent;
						while (Parent)
						{
							TreeView->SetItemExpansion(Parent, true);
							Parent = Parent->Parent;
						}
						
						RegenerateTreeViewItems();
					});
		}
		else
		{
			return SNew(SBindingSetTableRow, OwnerTable)
				.TreeItem(Item);
		}
	}

	TSharedPtr<SWidget> SBindingSetsTreeView::TreeView_HandleContextMenuOpening()
	{
		FMenuBuilder MenuBuilder(true, nullptr);

		TArray<TSharedPtr<FTreeItem>> SelectedItems;
		TreeView->GetSelectedItems(SelectedItems);

		if (!SelectedItems.IsEmpty())
		{
			TSharedPtr<FTreeItem> SelectedItem = SelectedItems[0];

			switch (SelectedItem->ItemType)
			{
				case FTreeItem::EType::Attribute:
				{
					MenuBuilder.AddMenuEntry(
						LOCTEXT("AddNamedAttribute", "Remove Named Attribute"),
						LOCTEXT("AddNamedAttribute_Tooltip", "Remove the selected named attribute from the set"),
						FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Minus"),
						FUIAction(
							FExecuteAction::CreateLambda([this, SelectedItem]()
							{
								SkeletonBinding->RemoveNamedAttributeFromSet(SelectedItem->SetItem->Name, SelectedItem->Name);
								RegenerateTreeViewItems();
							})
						));

					break;
				}
			}
		}

		return MenuBuilder.MakeWidget();
	}

	TArray<TSharedPtr<UE::Anim::STF::SBindingSetsTreeView::FTreeItem>> SBindingSetsTreeView::GetAllTreeItems()
	{
		using namespace UE::Anim::STF;

		TArray<TSharedPtr<SBindingSetsTreeView::FTreeItem>> AllItems;
		AllItems.Append(RootItems);
	
		for (int32 Index = 0; Index < AllItems.Num(); ++Index)
		{
			AllItems.Append(AllItems[Index]->Children);
		}

		return AllItems;
	}

	void SBindingSetsTreeView::RegenerateTreeViewItems()
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
	
			for (const FSkeletonNamedAttributeSet& NamedAttributeSet : SkeletonBinding->GetNamedAttributeSets())
			{
				TMap<FName, TSharedPtr<FTreeItem>> AttributesItemMap;

				TArray<TPair<FSkeletonNamedAttribute, TSharedPtr<FTreeItem>>> NamedAttributesQueue;

				TSharedPtr<FTreeItem> SetItem = MakeShared<FTreeItem>();
				SetItem->Name = NamedAttributeSet.Name;
				SetItem->ItemType = FTreeItem::EType::AttributeSet;
				SetItem->Type = NAME_None;

				for (const FName& AttributeName : NamedAttributeSet.NamedAttributes)
				{
					const FSkeletonNamedAttribute* const NamedAttribute = SkeletonBinding->FindNamedAttribute(AttributeName);
					if (!ensure(NamedAttribute))
					{
						continue;
					}

					TSharedPtr<FTreeItem> AttributeItem = MakeShared<FTreeItem>();
					AttributeItem->Name = AttributeName;
					AttributeItem->ItemType = FTreeItem::EType::Attribute;
					AttributeItem->Type = NamedAttribute->Type.GetFName();
					AttributeItem->SetItem = SetItem;

					AttributesItemMap.Add(AttributeName, AttributeItem);
					NamedAttributesQueue.Add(TPair<FSkeletonNamedAttribute, TSharedPtr<FTreeItem>>(*NamedAttribute, AttributeItem));
				}

				while (!NamedAttributesQueue.IsEmpty())
				{
					const TPair<FSkeletonNamedAttribute, TSharedPtr<FTreeItem>> Attribute = NamedAttributesQueue.Pop();
					const FSkeletonNamedAttribute NamedAttribute = Attribute.Key;
					TSharedPtr<FTreeItem> AttributeTreeItem = Attribute.Value;

					FName AncestorName = NamedAttribute.ParentName;
					TSharedPtr<FTreeItem> ClosestAncestorItem = nullptr;
					while (AncestorName != NAME_None)
					{
						TSharedPtr<FTreeItem>* FoundClosestAncestorItem = AttributesItemMap.Find(AncestorName);
						if (FoundClosestAncestorItem)
						{
							ClosestAncestorItem = *FoundClosestAncestorItem;
							break;
						}

						if (const FSkeletonNamedAttribute* const Parent = SkeletonBinding->FindNamedAttribute(AncestorName))
						{
							AncestorName = Parent->ParentName;
						}
						else
						{
							break;
						}
					}

					if (ClosestAncestorItem)
					{
						AttributeTreeItem->Parent = ClosestAncestorItem;
						ClosestAncestorItem->Children.Add(AttributeTreeItem);
					}
					else
					{
						AttributeTreeItem->Parent = SetItem;
						SetItem->Children.Add(AttributeTreeItem);
					}
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