// Copyright Epic Games, Inc. All Rights Reserved.

#include "SkeletonBindingNamedAttributeMappingsEditor.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Modules/ModuleManager.h"
#include "SkeletonTemplateFramework/SkeletonBinding.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/Views/STreeView.h"
#include "Animation/Skeleton.h"
#include "PropertyEditorModule.h"
#include "SkeletonBindingEditorToolkit.h"

#define LOCTEXT_NAMESPACE "SkeletonBindingNamedAttributeMappingsEditor"

namespace UE::Anim::STF
{
	struct FBindingAttributeTreeItem;

	struct FBindingAttributeTreeItem : public SBindingMappingsTreeView::ITreeItem
	{
		TObjectPtr<const UClass> ValueType;
		bool bHasValue;
		TSharedPtr<FBindingAttributeTreeItem> Parent;
		TArray<TSharedPtr<FBindingAttributeTreeItem>> ChildrenAttributes;

		virtual void GetChildren(TArray<TSharedPtr<ITreeItem>>& OutChildren) const override
		{
			OutChildren.Append(ChildrenAttributes);
		}
	};
	
	struct FBindingNamedAttributeMappingListColumns
	{
		static const FName NameId;
		static const FName SourceSetId;
	};

	const FName FBindingNamedAttributeMappingListColumns::NameId(TEXT("Name"));
	const FName FBindingNamedAttributeMappingListColumns::SourceSetId(TEXT("SourceSet"));
	
	struct FBindingNamedAttributeMappingsColumns
	{
		static const FName NameId;
		static const FName TypeId;
		static const FName ValueId;
	};

	const FName FBindingNamedAttributeMappingsColumns::NameId(TEXT("Name"));
	const FName FBindingNamedAttributeMappingsColumns::TypeId(TEXT("Type"));
	const FName FBindingNamedAttributeMappingsColumns::ValueId(TEXT("Value"));

	class SBindingMappingTableRow : public SMultiColumnTableRow<TSharedPtr<SBindingMappingsTreeView::ITreeItem>>
	{
	public:
		DECLARE_DELEGATE_RetVal_TwoParams(bool /* Success */, FOnRenamed, FName /* Old Name */, FName /* New Name */);

		SLATE_BEGIN_ARGS(SBindingMappingTableRow) { }
			SLATE_ARGUMENT(TSharedPtr<SBindingMappingsTreeView::ITreeItem>, TreeItem)
			SLATE_EVENT(FOnRenamed, OnRenamed);
		SLATE_END_ARGS()

		void OnCommitRename(const FText& InText, ETextCommit::Type CommitInfo)
		{
			const FName OldName = TreeItem->AttributeName;
			const FName NewName = FName(InText.ToString());

			if (OnRenamed.IsBound())
			{
				const bool bSuccess = OnRenamed.Execute(OldName, NewName);
				if (bSuccess)
				{
					TreeItem->AttributeName = NewName;
				}
			}
		}

		void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView, const TSharedPtr<SBindingMappingsTreeView> InTreeView)
		{
			TreeItem = InArgs._TreeItem;
			OnRenamed = InArgs._OnRenamed;
			TreeView = InTreeView;

			FSuperRowType::Construct(
				FSuperRowType::FArguments(),
				InOwnerTableView);
		}

		TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
		{
			if (ColumnName == FBindingNamedAttributeMappingsColumns::NameId)
			{
				return SNew(SHorizontalBox)
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
								.Text_Lambda([this]()
									{
										return FText::FromName(TreeItem->AttributeName);
									})
						];
			}
				
			if (ColumnName == FBindingNamedAttributeMappingsColumns::TypeId)
			{
				return SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
								.Text(TreeItem->AttributeType)
						];
			}
				
			if (ColumnName == FBindingNamedAttributeMappingsColumns::ValueId)
			{
				// Shown in details panel, TODO: Show read-only widget here?
				return SNullWidget::NullWidget;
			}
			
			return SNullWidget::NullWidget;
			
		}

	private:
		TSharedPtr<SBindingMappingsTreeView::ITreeItem> TreeItem;
		TSharedPtr<SBindingMappingsTreeView> TreeView;
		FOnRenamed OnRenamed;
	};

	void SBindingMappingsTreeView::Construct(const FArguments& InArgs, TObjectPtr<USkeletonBinding> InSkeletonBinding, ISkeletonBindingEditorToolkit* InToolkit)
	{
		SkeletonBinding = InSkeletonBinding;

		FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

		Toolkit = InToolkit;
							
		ChildSlot
		[
			SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.MinHeight(200.0f)
				.AutoHeight()
				[
					SAssignNew(ListView, SListView<TSharedPtr<FListItem>>)
						.SelectionMode(ESelectionMode::Single)
						.ListItemsSource(&ListItems)
						.HeaderRow
						(
							SNew(SHeaderRow)
								+ SHeaderRow::Column(FBindingNamedAttributeMappingListColumns::NameId)
									.FillWidth(0.5f)
									.DefaultLabel(LOCTEXT("NameLabel", "Name"))
								+ SHeaderRow::Column(FBindingNamedAttributeMappingListColumns::SourceSetId)
									.FillWidth(0.5f)
									.DefaultLabel(LOCTEXT("SourceSetLabel", "Source Set"))
						)
						.OnGenerateRow(this, &SBindingMappingsTreeView::ListView_GenerateItemRow)
						.OnSelectionChanged(this, &SBindingMappingsTreeView::ListView_OnSelectionChanged)
						.OnContextMenuOpening(this, &SBindingMappingsTreeView::ListView_HandleContextMenuOpening)
				]
				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				[
					SAssignNew(TreeView, STreeView<TSharedPtr<ITreeItem>>)
						.SelectionMode(ESelectionMode::Single)
						.TreeItemsSource(&RootItems)
						.OnGenerateRow(this, &SBindingMappingsTreeView::TreeView_GenerateItemRow)
						.OnSelectionChanged(this, &SBindingMappingsTreeView::TreeView_OnSelectionChanged)
						.OnGetChildren(this, &SBindingMappingsTreeView::TreeView_HandleGetChildren)
						.OnContextMenuOpening(this, &SBindingMappingsTreeView::TreeView_HandleContextMenuOpening)
						.HighlightParentNodesForSelection(true)
						.HeaderRow
						(
							SNew(SHeaderRow)
								+SHeaderRow::Column(FBindingNamedAttributeMappingsColumns::NameId)
									.FillWidth(0.5f)
									.DefaultLabel(LOCTEXT("NameLabel", "Name"))
								/*
								+SHeaderRow::Column(UE::Anim::STF::FNamedAttributeMappingsColumns::TypeId)
									.FillWidth(0.1f)
									.DefaultLabel(LOCTEXT("TypeLabel", "Type"))
								*/
								+SHeaderRow::Column(FBindingNamedAttributeMappingsColumns::ValueId)
									.FillWidth(0.5f)
									.DefaultLabel(LOCTEXT("ValueLabel", "Value"))
						)
				]
		];

		RegenerateListViewItems();
		RegenerateTreeViewItems();

		// Expand all tree items on construction
		for (TSharedPtr<ITreeItem> TreeItem : GetAllTreeItems())
		{
			TreeView->SetItemExpansion(TreeItem, true);
		}
	}
	
	void SBindingMappingsTreeView::OnNamedAttributeSetsChanged()
	{
		RegenerateListViewItems();
	}

	class SBindingListViewRow : public SMultiColumnTableRow<TSharedPtr<SBindingMappingsTreeView::FListItem>>
	{
	public:
		SLATE_BEGIN_ARGS(SBindingListViewRow) { }
			SLATE_ARGUMENT(TSharedPtr<SBindingMappingsTreeView::FListItem>, ListItem)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
		{
			ListItem = InArgs._ListItem;

			FSuperRowType::Construct(
				FSuperRowType::FArguments(),
				InOwnerTableView);
		}

		TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
		{
			if (ColumnName == FBindingNamedAttributeMappingListColumns::NameId)
			{
				return SNew(STextBlock)
					.Text(FText::FromName(ListItem->Name));
			}

			if (ColumnName == FBindingNamedAttributeMappingListColumns::SourceSetId)
			{
				return SNew(STextBlock)
					.Text(FText::FromName(ListItem->SourceSetName));
			}

			return SNullWidget::NullWidget;
		}

	private:
		TSharedPtr<SBindingMappingsTreeView::FListItem> ListItem;
	};
	
	TSharedRef<ITableRow> SBindingMappingsTreeView::ListView_GenerateItemRow(TSharedPtr<FListItem> Item, const TSharedRef<STableViewBase>& OwnerTable)
	{
		return SNew(SBindingListViewRow, OwnerTable)
			.ListItem(Item);
	}

	void SBindingMappingsTreeView::ListView_OnSelectionChanged(TSharedPtr<FListItem> InItem, ESelectInfo::Type InSelectInfo)
	{
		SelectedMappingName = InItem ? InItem->Name : NAME_None;
		RegenerateTreeViewItems();
	}
	
	TSharedPtr<SWidget> SBindingMappingsTreeView::ListView_HandleContextMenuOpening()
	{
		FMenuBuilder MenuBuilder(true, nullptr);		
		return MenuBuilder.MakeWidget();
	}

	void SBindingMappingsTreeView::TreeView_HandleGetChildren(TSharedPtr<ITreeItem> InItem, TArray<TSharedPtr<ITreeItem>>& OutChildren) const
	{
		InItem->GetChildren(OutChildren);
	}

	void SBindingMappingsTreeView::TreeView_OnSelectionChanged(TSharedPtr<ITreeItem> InItem, ESelectInfo::Type InSelectInfo)
	{
		if (InItem)
		{			
			const FSkeletonNamedAttributeMapping* const Mapping = SkeletonBinding->FindNamedAttributeMapping(SelectedMappingName);
			const FSkeletonNamedAttributeMappingEntry* const Entry = Mapping->TableData.FindByPredicate([InItem](const FSkeletonNamedAttributeMappingEntry& Candidate)
			{
				return Candidate.AttributeName == InItem->AttributeName;
			});

			if (Entry)
			{
				Toolkit->SetDetailsObject(Entry->Value);
			}
		}
		else
		{
			Toolkit->SetDetailsObject(nullptr);
		}
	}

	TSharedRef<ITableRow> SBindingMappingsTreeView::TreeView_GenerateItemRow(TSharedPtr<SBindingMappingsTreeView::ITreeItem> Item, const TSharedRef<STableViewBase>& OwnerTable)
	{
		return SNew(SBindingMappingTableRow, OwnerTable, SharedThis(this))
			.TreeItem(Item);
	}

	TSharedPtr<SWidget> SBindingMappingsTreeView::TreeView_HandleContextMenuOpening()
	{
		FMenuBuilder MenuBuilder(true, nullptr);
		return MenuBuilder.MakeWidget();
	}

	TArray<TSharedPtr<UE::Anim::STF::SBindingMappingsTreeView::ITreeItem>> SBindingMappingsTreeView::GetAllTreeItems()
	{
		using namespace UE::Anim::STF;

		TArray<TSharedPtr<SBindingMappingsTreeView::ITreeItem>> AllItems;
		AllItems.Append(RootItems);
	
		for (int32 Index = 0; Index < AllItems.Num(); ++Index)
		{
			AllItems[Index]->GetChildren(AllItems);
		}

		return AllItems;
	}

	void SBindingMappingsTreeView::RegenerateListViewItems()
	{
		ListItems.Reset();

		const TArray<FSkeletonNamedAttributeMapping>& Mappings = SkeletonBinding->GetNamedAttributeMappings();
		for (const FSkeletonNamedAttributeMapping& Mapping : Mappings)
		{
			TSharedPtr<FListItem> ListItem = MakeShared<FListItem>();
			ListItem->Name = Mapping.Name;
			ListItem->SourceSetName = Mapping.SourceAttributeSet;

			ListItems.Add(ListItem);
		}
	}

	void SBindingMappingsTreeView::RegenerateTreeViewItems()
	{
		// Make note of all tree items currently expanded
		/*
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
		*/

		// Rebuild items
		{
			RootItems.Reset();
			TMap<FName, TSharedPtr<FBindingAttributeTreeItem>> AttributesItemMap;

			if (const FSkeletonNamedAttributeMapping* const AttributeMapping = SkeletonBinding->FindNamedAttributeMapping(SelectedMappingName))
			{
				TArray<TPair<FSkeletonNamedAttribute, TSharedPtr<FBindingAttributeTreeItem>>> NamedAttributesQueue;
				
				for (const FSkeletonNamedAttributeMappingEntry& MappingEntry : AttributeMapping->TableData)
				{
					const FSkeletonNamedAttribute* NamedAttribute = SkeletonBinding->FindNamedAttribute(MappingEntry.AttributeName);
					check(NamedAttribute);
					
					TSharedPtr<FBindingAttributeTreeItem> MappingTreeItem = MakeShared<FBindingAttributeTreeItem>();
					MappingTreeItem->AttributeName = NamedAttribute->Name;
					MappingTreeItem->TreeView = this;
					MappingTreeItem->bHasValue = true;

					const FSkeletonNamedAttributeMappingType* const AttributeMappingType = AttributeMapping->MappingTypes.FindByPredicate([NamedAttribute](const FSkeletonNamedAttributeMappingType& MappingType)
					{
						return MappingType.SourceType == NamedAttribute->Type;
					});

					if (AttributeMappingType)
					{
						MappingTreeItem->ValueType = AttributeMappingType->TargetType;
						MappingTreeItem->AttributeType = FText::Format(LOCTEXT("AttributeType", "{0} -> {1}"), FText::FromName(NamedAttribute->Type.GetFName()), FText::FromName(AttributeMappingType->TargetType.GetFName()));
					}
					else
					{
						MappingTreeItem->ValueType = nullptr;
						MappingTreeItem->AttributeType = FText::Format(LOCTEXT("AttributeTypeNone", "{0} -> None"), FText::FromName(NamedAttribute->Type.GetFName()));
					}

					AttributesItemMap.Add(NamedAttribute->Name, MappingTreeItem);
					NamedAttributesQueue.Add(TPair<FSkeletonNamedAttribute, TSharedPtr<FBindingAttributeTreeItem>>(*NamedAttribute, MappingTreeItem));
				}

				while (!NamedAttributesQueue.IsEmpty())
				{
					const TPair<FSkeletonNamedAttribute, TSharedPtr<FBindingAttributeTreeItem>> Attribute = NamedAttributesQueue.Pop();
					const FSkeletonNamedAttribute NamedAttribute = Attribute.Key;
					TSharedPtr<FBindingAttributeTreeItem> AttributeTreeItem = Attribute.Value;

					FName AncestorName = NamedAttribute.ParentName;
					TSharedPtr<FBindingAttributeTreeItem> ClosestAncestorItem = nullptr;
					while (AncestorName != NAME_None)
					{
						TSharedPtr<FBindingAttributeTreeItem>* FoundClosestAncestorItem = AttributesItemMap.Find(AncestorName);
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
						ClosestAncestorItem->ChildrenAttributes.Add(AttributeTreeItem);
					}
					else
					{
						AttributeTreeItem->Parent = nullptr;
						RootItems.Add(AttributeTreeItem);
					}
				}
			}
		}

		// Update tree view and restore tree item expanded states
		{
			check(TreeView);
			TreeView->RequestTreeRefresh();

			for (TSharedPtr<ITreeItem> TreeItem : GetAllTreeItems())
			{
				TreeView->SetItemExpansion(TreeItem, true);
			}

			/*
			for (TSharedPtr<FTreeItem> TreeItem : GetAllTreeItems())
			{
				if (ExpandedAttributeNames.Contains(TreeItem->Name))
				{
					TreeView->SetItemExpansion(TreeItem, true);
				}
			}
			*/
		}
	}
}

#undef LOCTEXT_NAMESPACE