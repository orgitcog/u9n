// Copyright Epic Games, Inc. All Rights Reserved.

#include "SkeletonTemplateNamedAttributesEditor.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Modules/ModuleManager.h"
#include "SkeletonTemplateFramework/SkeletonTemplate.h"
#include "ClassViewerFilter.h"
#include "ClassViewerModule.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/Views/STreeView.h"

#define LOCTEXT_NAMESPACE "SkeletonTemplateNamedAttributesEditor"

namespace UE::Anim::STF
{
	struct FTemplateNamedAttributesColumns
	{
		static const FName NameId;
		static const FName TypeId;
	};

	const FName FTemplateNamedAttributesColumns::NameId(TEXT("Name"));
	const FName FTemplateNamedAttributesColumns::TypeId(TEXT("Type"));

	TSharedRef<FNamedElementDragDropOp> FNamedElementDragDropOp::New(const FName InNamedAttribute)
	{
		TSharedRef<FNamedElementDragDropOp> Operation = MakeShared<FNamedElementDragDropOp>();
		Operation->NamedAttribute = InNamedAttribute;
		Operation->Construct();
		return Operation;
	}

	TSharedPtr<SWidget> FNamedElementDragDropOp::GetDefaultDecorator() const
	{
		return SNew(SBorder)
			.Visibility(EVisibility::Visible)
			.BorderImage(FAppStyle::GetBrush("Menu.Background"))
			[
				SNew(STextBlock)
				.Text(FText::FromName(NamedAttribute))
			];
	}

	class SNamedElementTableRow : public SMultiColumnTableRow<TSharedPtr<SAttributesTreeView::FTreeItem>>
	{
	public:
		DECLARE_DELEGATE_RetVal_TwoParams(bool /* Success */, FOnRenamed, FName /* Old Name */, FName /* New Name */);
		DECLARE_DELEGATE_TwoParams(FOnReparented, FName /* Attribute Name */, FName /* New Parent Name */);
		DECLARE_DELEGATE_RetVal_TwoParams(bool /* Success */, FOnTypeSelected, FName /* Attribute Name */, TObjectPtr<const UClass> /* New Type */);
	
		SLATE_BEGIN_ARGS(SNamedElementTableRow) { }
			SLATE_ARGUMENT(TSharedPtr<SAttributesTreeView::FTreeItem>, NamedElementTreeItem)
			SLATE_EVENT(FOnRenamed, OnRenamed);
			SLATE_EVENT(FOnReparented, OnReparented);
			SLATE_EVENT(FOnTypeSelected, OnTypeSelected);
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

		TOptional<EItemDropZone> OnCanAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone, TSharedPtr<SAttributesTreeView::FTreeItem> TargetItem)
		{
			TOptional<EItemDropZone> ReturnedDropZone;
	
			const TSharedPtr<FNamedElementDragDropOp> DragDropOp = DragDropEvent.GetOperationAs<FNamedElementDragDropOp>();
			if (DragDropOp.IsValid())
			{
        		ReturnedDropZone = EItemDropZone::BelowItem;	
			}
	
			return ReturnedDropZone;
		}

		FReply OnAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone, TSharedPtr<SAttributesTreeView::FTreeItem> TargetItem)
		{
			const TSharedPtr<FNamedElementDragDropOp> DragDropOp = DragDropEvent.GetOperationAs<FNamedElementDragDropOp>();
			if (!DragDropOp.IsValid())
			{
				return FReply::Unhandled();
			}

			OnReparented.ExecuteIfBound(DragDropOp->NamedAttribute, NamedElementTreeItem->Name);

			return FReply::Handled();
		}

		void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
		{
			NamedElementTreeItem = InArgs._NamedElementTreeItem;
			OnRenamed = InArgs._OnRenamed;
			OnReparented = InArgs._OnReparented;
			OnTypeSelected = InArgs._OnTypeSelected;

			FSuperRowType::Construct(
				FSuperRowType::FArguments()
					.OnDragDetected(this, &SNamedElementTableRow::OnDragDetected)
					.OnCanAcceptDrop(this, &SNamedElementTableRow::OnCanAcceptDrop)
					.OnAcceptDrop(this, &SNamedElementTableRow::OnAcceptDrop),
				InOwnerTableView);
		}

		void OnCommitRename(const FText& InText, ETextCommit::Type CommitInfo)
		{
			const FName OldName = NamedElementTreeItem->Name;
			const FName NewName = FName(InText.ToString());

			if (OnRenamed.IsBound())
			{
				const bool bSuccess = OnRenamed.Execute(OldName, NewName);
				if (bSuccess)
				{
					NamedElementTreeItem->Name = NewName;
				}
			}
		}

		void OnClassPicked(UClass* InClass)
		{
			if (OnRenamed.IsBound())
			{
				const bool bSuccess = OnTypeSelected.Execute(NamedElementTreeItem->Name, InClass);
				if (bSuccess)
				{
					NamedElementTreeItem->Type = InClass ? (InClass->GetFName()) : NAME_None;
				}
			}
			TypeComboButton->SetIsOpen(false);
		}

		TSharedRef<SWidget> GenerateStructPicker()
		{
			class FClassFilter : public IClassViewerFilter
			{
			public:
				FClassFilter(const UClass* InBaseType) : BaseType(InBaseType)  { };
		
				virtual bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef< class FClassViewerFilterFuncs > InFilterFuncs) override
				{
					if (InClass->HasMetaData(TEXT("Hidden")))
					{
						return false;
					}
			
					return InClass->IsChildOf(BaseType) && InClass != BaseType;
				}
				
				virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef< const class IUnloadedBlueprintData > InUnloadedClassData, TSharedRef<FClassViewerFilterFuncs> InFilterFuncs)
				{
					return false;
				};
				
			private:
				const UClass* BaseType;
			};
			
			FClassViewerInitializationOptions Options;
			Options.bShowNoneOption = true;
			Options.NameTypeToDisplay = EClassViewerNameTypeToDisplay::DisplayName;
			Options.DisplayMode = EClassViewerDisplayMode::ListView;
			Options.ClassFilters.Add(MakeShared<FClassFilter>(USkeletonTemplateBaseType::StaticClass()));

			FOnClassPicked OnPicked(FOnClassPicked::CreateRaw(this, &SNamedElementTableRow::OnClassPicked));

			return FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer")
				.CreateClassViewer(Options, OnPicked);
		}

		TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
		{
			if (ColumnName == FTemplateNamedAttributesColumns::NameId)
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
							SAssignNew(InlineWidget, SInlineEditableTextBlock)
								.Text_Lambda([this]()
								{
									return FText::FromName(NamedElementTreeItem->Name);
								})
								.OnTextCommitted(this, &SNamedElementTableRow::OnCommitRename)
						];

				NamedElementTreeItem->OnRenameRequested.BindSP(InlineWidget.Get(), &SInlineEditableTextBlock::EnterEditingMode);

				return HorizontalBox;
			}

			if (ColumnName == FTemplateNamedAttributesColumns::TypeId)
			{
				return SAssignNew(TypeComboButton, SComboButton)
					.OnGetMenuContent(this, &SNamedElementTableRow::GenerateStructPicker)
					.ButtonContent()
					[
						SNew(STextBlock)
						.Text_Lambda([this]()
							{
								return FText::FromName(NamedElementTreeItem->Type);
							})
					];
			}

			return SNullWidget::NullWidget;
		}

	private:
		TSharedPtr<SAttributesTreeView::FTreeItem> NamedElementTreeItem;
		FOnRenamed OnRenamed;
		FOnReparented OnReparented;
		FOnTypeSelected OnTypeSelected;
		TSharedPtr<SComboButton> TypeComboButton;
	};

	void SAttributesTreeView::Construct(const FArguments& InArgs, TObjectPtr<USkeletonTemplate> InSkeletonTemplate)
	{
		SkeletonTemplate = InSkeletonTemplate;

		ChildSlot
		[
			SAssignNew(TreeView, STreeView<TSharedPtr<FTreeItem>>)
				.TreeItemsSource(&RootItems)
				.OnGenerateRow(this, &SAttributesTreeView::TreeView_GenerateItemRow)
				.OnGetChildren(this, &SAttributesTreeView::TreeView_HandleGetChildren)
				.OnContextMenuOpening(this, &SAttributesTreeView::TreeView_HandleContextMenuOpening)
				.OnItemScrolledIntoView(this, &SAttributesTreeView::TreeView_OnItemScrolledIntoView)
				.HighlightParentNodesForSelection(true)
				.HeaderRow
				(
					SNew(SHeaderRow)
						+SHeaderRow::Column(FTemplateNamedAttributesColumns::NameId)
							.FillWidth(0.5f)
							.DefaultLabel(LOCTEXT("NameLabel", "Name"))
						+SHeaderRow::Column(FTemplateNamedAttributesColumns::TypeId)
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

	void SAttributesTreeView::TreeView_HandleGetChildren(TSharedPtr<FTreeItem> InItem, TArray<TSharedPtr<FTreeItem>>& OutChildren) const
	{
		OutChildren.Append(InItem->Children);
	}

	void SAttributesTreeView::TreeView_OnItemScrolledIntoView(TSharedPtr<FTreeItem> InItem, const TSharedPtr<ITableRow>& InWidget)
	{
		if (DeferredRenameRequest.IsValid())
		{
			DeferredRenameRequest->OnRenameRequested.ExecuteIfBound();
			DeferredRenameRequest.Reset();
		}
	}

	TSharedRef<ITableRow> SAttributesTreeView::TreeView_GenerateItemRow(TSharedPtr<SAttributesTreeView::FTreeItem> Item, const TSharedRef<STableViewBase>& OwnerTable)
	{
		return SNew(SNamedElementTableRow, OwnerTable)
			.OnRenamed_Lambda([this](const FName OldName, const FName NewName) -> bool
				{
					return SkeletonTemplate->RenameNamedAttribute(OldName, NewName);
				})
			.OnReparented_Lambda([this](const FName AttributeName, const FName NewParentName)
				{
					SkeletonTemplate->ReparentNamedAttribute(AttributeName, NewParentName);
					RegenerateTreeViewItems();
				})
			.OnTypeSelected_Lambda([this](const FName AttributeName, const TObjectPtr<const UClass> NewType)
				{
					return SkeletonTemplate->SetNamedAttributeType(AttributeName, NewType);
				})
			.NamedElementTreeItem(Item);
	}

	TSharedPtr<SWidget> SAttributesTreeView::TreeView_HandleContextMenuOpening()
	{
		FMenuBuilder MenuBuilder(true, nullptr);

		TArray<TSharedPtr<FTreeItem>> SelectedItems;
		TreeView->GetSelectedItems(SelectedItems);

		FUIAction AddNewAttributeAction = FUIAction(
			FExecuteAction::CreateSP(this, &SAttributesTreeView::HandleAddAttribute)
		);

		if (SelectedItems.IsEmpty())
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT("AddNamedAttribute", "Add Named Attribute"),
				LOCTEXT("AddNamedAttribute_Tooltip", "Add a new named attribute"),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Plus"),
				AddNewAttributeAction);
		}
		else
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT("AddChildNamedAttribute", "Add Child Named Attribute"),
				LOCTEXT("AddChildNamedAttribute_Tooltip", "Add a new child named attribute"),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Plus"),
				AddNewAttributeAction);

			MenuBuilder.AddMenuEntry(
				LOCTEXT("RenameNamedAttribute", "Rename Named Attribute"),
				LOCTEXT("RenameNamedAttribute_Tooltip", "Renames the selected new child named attribute"),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "GenericCommands.Rename"),
				FUIAction(
					FExecuteAction::CreateSP(this, &SAttributesTreeView::HandleRenameAttribute)
				));
		
			MenuBuilder.AddMenuEntry(
				LOCTEXT("DeleteNamedAttribute", "Delete Named Attribute"),
				LOCTEXT("DeleteNamedAttribute_Tooltip", "Deletes the selected new child named attribute"),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Delete"),
				FUIAction(
					FExecuteAction::CreateSP(this, &SAttributesTreeView::HandleDeleteAttribute)
				));
		}

		return MenuBuilder.MakeWidget();
	}

	TArray<TSharedPtr<UE::Anim::STF::SAttributesTreeView::FTreeItem>> SAttributesTreeView::GetAllTreeItems()
	{
		using namespace UE::Anim::STF;

		TArray<TSharedPtr<SAttributesTreeView::FTreeItem>> AllItems;
		AllItems.Append(RootItems);
	
		for (int32 Index = 0; Index < AllItems.Num(); ++Index)
		{
			AllItems.Append(AllItems[Index]->Children);
		}

		return AllItems;
	}

	void SAttributesTreeView::RegenerateTreeViewItems()
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

			for (const FSkeletonNamedAttribute& NamedAttribute : SkeletonTemplate->GetNamedAttributes())
			{
				TSharedPtr<FTreeItem> Item = MakeShared<FTreeItem>();
				Item->Name = NamedAttribute.Name;
				Item->Type = NamedAttribute.Type ? (NamedAttribute.Type->GetFName()) : NAME_None;

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

	void SAttributesTreeView::HandleAddAttribute()
	{
		using namespace UE::Anim::STF;

		FSkeletonNamedAttribute NewAttribute;
		NewAttribute.Name = FName(LOCTEXT("NewAttribute", "NewAttribute").ToString());

		if (SkeletonTemplate->FindNamedAttribute(NewAttribute.Name) != nullptr)
		{
			// Resolve name clash
			int32 Suffix = 1;
			do
			{
				NewAttribute.Name = FName(FText::Format(LOCTEXT("NewAttributeFormat", "NewAttribute_{0}"), Suffix).ToString());
				++Suffix;
			}
			while (SkeletonTemplate->FindNamedAttribute(NewAttribute.Name) != nullptr);
		}

		TArray<TSharedPtr<SAttributesTreeView::FTreeItem>> SelectedItems;
		TreeView->GetSelectedItems(SelectedItems);
	
		if (!SelectedItems.IsEmpty())
		{
			NewAttribute.ParentName = SelectedItems[0]->Name;
		}

		const bool bSuccess = SkeletonTemplate->AddNamedAttribute(NewAttribute);
		if (bSuccess)
		{
			RegenerateTreeViewItems();
		}

		for (TSharedPtr<FTreeItem> Item : GetAllTreeItems())
		{
			if (Item->Name == NewAttribute.Name)
			{
				TreeView->RequestScrollIntoView(Item);
				DeferredRenameRequest = Item;
			}
			else if (Item->Name == NewAttribute.ParentName)
			{
				TreeView->SetItemExpansion(Item, true);
			}
		}
	}

	void SAttributesTreeView::HandleRenameAttribute()
	{
		using namespace UE::Anim::STF;

		FSkeletonNamedAttribute NewAttribute;

		TArray<TSharedPtr<SAttributesTreeView::FTreeItem>> SelectedItems;
		TreeView->GetSelectedItems(SelectedItems);
	
		check(!SelectedItems.IsEmpty());
		SelectedItems[0]->OnRenameRequested.ExecuteIfBound();
	}

	void SAttributesTreeView::HandleDeleteAttribute()
	{
		using namespace UE::Anim::STF;

		FSkeletonNamedAttribute NewAttribute;

		TArray<TSharedPtr<FTreeItem>> SelectedItems;
		TreeView->GetSelectedItems(SelectedItems);
	
		check(!SelectedItems.IsEmpty());

		const bool bSuccess = SkeletonTemplate->RemoveNamedAttribute(SelectedItems[0]->Name);
		if (bSuccess)
		{
			RegenerateTreeViewItems();
		}
	}
}

#undef LOCTEXT_NAMESPACE