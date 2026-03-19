// Copyright Epic Games, Inc. All Rights Reserved.

#include "SkeletonBindingNamedAttributesEditor.h"

#include "Animation/Skeleton.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Modules/ModuleManager.h"
#include "SkeletonTemplateFramework/SkeletonBinding.h"
#include "SkeletonTemplateFramework/SkeletonTemplateNamedAttributesEditor.h"
#include "StructViewerFilter.h"
#include "StructViewerModule.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/Views/STreeView.h"

#define LOCTEXT_NAMESPACE "SkeletonBindingNamedAttributesEditor"

namespace UE::Anim::STF
{
	struct FAttributeBindingsColumns
	{
		static const FName NameId;
		static const FName TypeId;
		static const FName BindingId;
	};

	const FName FAttributeBindingsColumns::NameId(TEXT("Name"));
	const FName FAttributeBindingsColumns::TypeId(TEXT("Type"));
	const FName FAttributeBindingsColumns::BindingId(TEXT("Binding"));

	class SNamedElementBindingTableRow : public SMultiColumnTableRow<TSharedPtr<SAttributeBindingsTreeView::FTreeItem>>
	{
	public:
		DECLARE_DELEGATE_TwoParams(FOnBindingSelected, FName /* Old Binding */, FName /* New Binding */);
		
		SLATE_BEGIN_ARGS(SNamedElementBindingTableRow) { }
			SLATE_ARGUMENT(TSharedPtr<SAttributeBindingsTreeView::FTreeItem>, NamedElementTreeItem)
			SLATE_ARGUMENT(TArray<TSharedPtr<FName>>*, BindingOptions)
			SLATE_EVENT(FOnBindingSelected, OnBindingSelected)
		SLATE_END_ARGS()

		FReply OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
		{
			if (MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
			{
				const TSharedRef<FNamedElementDragDropOp> DragDropOp = FNamedElementDragDropOp::New(NamedElementTreeItem->Name);
				return FReply::Handled().BeginDragDrop(DragDropOp);
			}

			return FReply::Unhandled();
		}
		
		void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
		{
			NamedElementTreeItem = InArgs._NamedElementTreeItem;
			BindingOptions = InArgs._BindingOptions;
			OnBindingSelected = InArgs._OnBindingSelected;

			FSuperRowType::Construct(
				FSuperRowType::FArguments()
					.OnDragDetected(this, &SNamedElementBindingTableRow::OnDragDetected),
				InOwnerTableView);
		}

		TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
		{
			if (ColumnName == UE::Anim::STF::FAttributeBindingsColumns::NameId)
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
						[
							SNew(STextBlock)
								.Text(FText::FromName(NamedElementTreeItem->Name))
						];

				return HorizontalBox;
			}
			else if (ColumnName == UE::Anim::STF::FAttributeBindingsColumns::TypeId)
			{
				return SNew(STextBlock)
					.Text_Lambda([this]()
						{
							return FText::FromName(NamedElementTreeItem->Type);
						});
			}
			else if (ColumnName == UE::Anim::STF::FAttributeBindingsColumns::BindingId)
			{
				TSharedPtr<FName> InitialValue;
				for (TSharedPtr<FName> Option : *BindingOptions)
				{
					if (*Option == NamedElementTreeItem->Binding)
					{
						InitialValue = Option;
						break;
					}
				}
				
				return SNew(SComboBox<TSharedPtr<FName>>)
					.InitiallySelectedItem(InitialValue)
					.OptionsSource(BindingOptions)
					.OnGenerateWidget_Lambda([](TSharedPtr<FName> Name)
						{
							return SNew(STextBlock)
								.Text(FText::FromName(*Name));
						})
					.OnSelectionChanged_Lambda([this](TSharedPtr<FName> NewSelection, ESelectInfo::Type)
						{
							OnBindingSelected.ExecuteIfBound(NamedElementTreeItem->Binding, *NewSelection);
						})
					[
						SNew(STextBlock)
							.Text_Lambda([this]()
							{
								return FText::FromName(NamedElementTreeItem->Binding);
							})
					];
			}
			else
			{
				return SNullWidget::NullWidget;
			}
		}

	private:
		FOnBindingSelected OnBindingSelected;
		
		TArray<TSharedPtr<FName>>* BindingOptions;
		
		TSharedPtr<SAttributeBindingsTreeView::FTreeItem> NamedElementTreeItem;
	};

	void SAttributeBindingsTreeView::Construct(const FArguments& InArgs, TObjectPtr<USkeletonBinding> InSkeletonBinding)
	{
		SkeletonBinding = InSkeletonBinding;

		ChildSlot
		[
			SAssignNew(TreeView, STreeView<TSharedPtr<FTreeItem>>)
				.TreeItemsSource(&RootItems)
				.OnGenerateRow(this, &SAttributeBindingsTreeView::TreeView_GenerateItemRow)
				.OnGetChildren(this, &SAttributeBindingsTreeView::TreeView_HandleGetChildren)
				.OnContextMenuOpening(this, &SAttributeBindingsTreeView::TreeView_HandleContextMenuOpening)
				.HighlightParentNodesForSelection(true)
				.HeaderRow
				(
					SNew(SHeaderRow)
						+ SHeaderRow::Column(UE::Anim::STF::FAttributeBindingsColumns::NameId)
							.FillWidth(0.5f)
							.DefaultLabel(LOCTEXT("NameLabel", "Name"))
						+ SHeaderRow::Column(UE::Anim::STF::FAttributeBindingsColumns::TypeId)
							.FillWidth(0.1f)
							.DefaultLabel(LOCTEXT("TypeLabel", "Type"))
						+ SHeaderRow::Column(UE::Anim::STF::FAttributeBindingsColumns::BindingId)
							.FillWidth(0.4f)
							.DefaultLabel(LOCTEXT("BindingLabel", "Binding"))
				)
		];

		RegenerateTreeViewItems();

		BindingOptions.Add(MakeShared<FName>(NAME_None));
		const TArray<FSkeletonNamedAttribute>& NamedAttributes = SkeletonBinding->GetUnboundSchemaNamedAttributes();
		for (const FSkeletonNamedAttribute& NamedAttribute : NamedAttributes)
		{
			BindingOptions.Add(MakeShared<FName>(NamedAttribute.Name));
		}

		// Expand all tree items on construction
		for (TSharedPtr<FTreeItem> TreeItem : GetAllTreeItems())
		{
			TreeView->SetItemExpansion(TreeItem, true);
		}
	}

	void SAttributeBindingsTreeView::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
	{
		if (bRequestRegenerateTreeItems)
		{
			RegenerateTreeViewItems();
			bRequestRegenerateTreeItems = false;
		}
	}

	void SAttributeBindingsTreeView::TreeView_HandleGetChildren(TSharedPtr<FTreeItem> InItem, TArray<TSharedPtr<FTreeItem>>& OutChildren) const
	{
		OutChildren.Append(InItem->Children);
	}

	TSharedRef<ITableRow> SAttributeBindingsTreeView::TreeView_GenerateItemRow(TSharedPtr<SAttributeBindingsTreeView::FTreeItem> Item, const TSharedRef<STableViewBase>& OwnerTable)
	{
		return SNew(SNamedElementBindingTableRow, OwnerTable)
			.NamedElementTreeItem(Item)
			.BindingOptions(&BindingOptions)
			.OnBindingSelected_Lambda([this, Item](const FName OldTemplateNamedAttribute, const FName NewTemplateNamedAttribute)
			{
				SkeletonBinding->UnbindTemplateNamedAttribute(OldTemplateNamedAttribute);
				SkeletonBinding->UnbindTemplateNamedAttribute(NewTemplateNamedAttribute);

				SkeletonBinding->BindAttribute(Item->Name, NewTemplateNamedAttribute);
				
				RequestRegenerateTreeItems();
			});
	}

	TSharedPtr<SWidget> SAttributeBindingsTreeView::TreeView_HandleContextMenuOpening()
	{
		FMenuBuilder MenuBuilder(true, nullptr);
		return MenuBuilder.MakeWidget();
	}

	TArray<TSharedPtr<UE::Anim::STF::SAttributeBindingsTreeView::FTreeItem>> SAttributeBindingsTreeView::GetAllTreeItems()
	{
		using namespace UE::Anim::STF;

		TArray<TSharedPtr<SAttributeBindingsTreeView::FTreeItem>> AllItems;
		AllItems.Append(RootItems);
	
		for (int32 Index = 0; Index < AllItems.Num(); ++Index)
		{
			AllItems.Append(AllItems[Index]->Children);
		}

		return AllItems;
	}

	void SAttributeBindingsTreeView::RegenerateTreeViewItems()
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
	
			TMap<FName, TSharedPtr<FTreeItem>> ItemMap;

			for (const FSkeletonNamedAttribute& NamedAttribute : SkeletonBinding->GetNamedAttributes())
			{
				TSharedPtr<FTreeItem> Item = MakeShared<FTreeItem>();
				Item->Name = NamedAttribute.Name;
				Item->Type = NamedAttribute.Type ? (NamedAttribute.Type->GetFName()) : NAME_None;

				if (const FSkeletonAttributeBinding* const Binding = SkeletonBinding->FindAttributeBinding(NamedAttribute.Name))
				{
					Item->Binding = Binding->AttributeName;
				}

				const TSharedPtr<FTreeItem>* ParentItem = ItemMap.Find(NamedAttribute.ParentName);
				if (ParentItem)
				{
					(*ParentItem)->Children.Add(Item);
				}
				else
				{
					RootItems.Add(Item);
				}

				ItemMap.Add(Item->Name, Item);
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

	TSharedPtr<UE::Anim::STF::SAttributeBindingsTreeView::FTreeItem> SAttributeBindingsTreeView::FindTreeItemByTemplateNamedAttribute(const FName TemplateNamedAttribute)
	{
		TArray<TSharedPtr<FTreeItem>> AllTreeItems = GetAllTreeItems();
		
		for (TSharedPtr<FTreeItem> TreeItem : AllTreeItems)
		{
			if (TreeItem->Binding == TemplateNamedAttribute)
			{
				return TreeItem;
			}
		}

		return nullptr;
	}

	void SAttributeBindingsTreeView::RequestRegenerateTreeItems()
	{
		bRequestRegenerateTreeItems = true;
	}

}

#undef LOCTEXT_NAMESPACE