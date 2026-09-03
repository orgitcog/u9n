// Copyright Epic Games, Inc. All Rights Reserved.

#include "SkeletonTemplateNamedAttributeMappingsEditor.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Modules/ModuleManager.h"
#include "SkeletonTemplateFramework/SkeletonTemplate.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/Views/STreeView.h"
#include "Animation/Skeleton.h"
#include "PropertyEditorModule.h"
#include "SkeletonTemplateEditorToolkit.h"

#define LOCTEXT_NAMESPACE "SkeletonTemplateNamedAttributeMappingsEditor"

namespace UE::Anim::STF
{
	struct FAttributeTreeItem;

	struct FAttributePartTreeItem : public SAttributeMappingsTreeView::ITreeItem
	{
		TObjectPtr<const UClass> ValueType;
		TSharedPtr<FAttributeTreeItem> Parent;

		virtual void GetChildren(TArray<TSharedPtr<ITreeItem>>& OutChildren) const override {}	
	};
	
	struct FAttributeTreeItem : public SAttributeMappingsTreeView::ITreeItem
	{
		TObjectPtr<const UClass> ValueType;
		bool bHasValue;
		TSharedPtr<FAttributeTreeItem> Parent;
		TArray<TSharedPtr<FAttributeTreeItem>> ChildrenAttributes;
		TArray<TSharedPtr<FAttributePartTreeItem>> ChildrenParts;

		virtual void GetChildren(TArray<TSharedPtr<ITreeItem>>& OutChildren) const override
		{
			OutChildren.Append(ChildrenParts);
			OutChildren.Append(ChildrenAttributes);
		}
	};
	
	struct FTemplateNamedAttributeMappingListColumns
	{
		static const FName NameId;
		static const FName SourceSetId;
	};

	const FName FTemplateNamedAttributeMappingListColumns::NameId(TEXT("Name"));
	const FName FTemplateNamedAttributeMappingListColumns::SourceSetId(TEXT("SourceSet"));
	
	struct FTemplateNamedAttributeMappingsColumns
	{
		static const FName NameId;
		static const FName TypeId;
		static const FName ValueId;
	};

	const FName FTemplateNamedAttributeMappingsColumns::NameId(TEXT("Name"));
	const FName FTemplateNamedAttributeMappingsColumns::TypeId(TEXT("Type"));
	const FName FTemplateNamedAttributeMappingsColumns::ValueId(TEXT("Value"));

	class SNamedElementMappingTableRow : public SMultiColumnTableRow<TSharedPtr<SAttributeMappingsTreeView::ITreeItem>>
	{
	public:
		DECLARE_DELEGATE_RetVal_TwoParams(bool /* Success */, FOnRenamed, FName /* Old Name */, FName /* New Name */);

		SLATE_BEGIN_ARGS(SNamedElementMappingTableRow) { }
			SLATE_ARGUMENT(TSharedPtr<SAttributeMappingsTreeView::ITreeItem>, TreeItem)
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

		void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView, const TSharedPtr<SAttributeMappingsTreeView> InTreeView)
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
			if (ColumnName == FTemplateNamedAttributeMappingsColumns::NameId)
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
				
			if (ColumnName == FTemplateNamedAttributeMappingsColumns::TypeId)
			{
				return SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
								.Text(TreeItem->AttributeType)
						];
			}
				
			if (ColumnName == FTemplateNamedAttributeMappingsColumns::ValueId)
			{
				// Shown in details panel, TODO: Show read-only widget here?
				return SNullWidget::NullWidget;
			}
			
			return SNullWidget::NullWidget;
			
		}

	private:
		TSharedPtr<SAttributeMappingsTreeView::ITreeItem> TreeItem;
		TSharedPtr<SAttributeMappingsTreeView> TreeView;
		FOnRenamed OnRenamed;
	};

	void SAttributeMappingsTreeView::Construct(const FArguments& InArgs, TObjectPtr<USkeletonTemplate> InSkeletonTemplate, ISkeletonTemplateEditorToolkit* InToolkit)
	{
		SkeletonTemplate = InSkeletonTemplate;

		RegenerateNamedAttributeSetOptions();

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
								+ SHeaderRow::Column(FTemplateNamedAttributeMappingListColumns::NameId)
									.FillWidth(0.5f)
									.DefaultLabel(LOCTEXT("NameLabel", "Name"))
								+ SHeaderRow::Column(FTemplateNamedAttributeMappingListColumns::SourceSetId)
									.FillWidth(0.5f)
									.DefaultLabel(LOCTEXT("SourceSetLabel", "Source Set"))
						)
						.OnGenerateRow(this, &SAttributeMappingsTreeView::ListView_GenerateItemRow)
						.OnSelectionChanged(this, &SAttributeMappingsTreeView::ListView_OnSelectionChanged)
						.OnContextMenuOpening(this, &SAttributeMappingsTreeView::ListView_HandleContextMenuOpening)
						.OnItemScrolledIntoView(this, &SAttributeMappingsTreeView::ListView_OnItemScrolledIntoView)
				]
				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				[
					SAssignNew(TreeView, STreeView<TSharedPtr<ITreeItem>>)
						.SelectionMode(ESelectionMode::Single)
						.TreeItemsSource(&RootItems)
						.OnGenerateRow(this, &SAttributeMappingsTreeView::TreeView_GenerateItemRow)
						.OnSelectionChanged(this, &SAttributeMappingsTreeView::TreeView_OnSelectionChanged)
						.OnGetChildren(this, &SAttributeMappingsTreeView::TreeView_HandleGetChildren)
						.OnContextMenuOpening(this, &SAttributeMappingsTreeView::TreeView_HandleContextMenuOpening)
						.HighlightParentNodesForSelection(true)
						.HeaderRow
						(
							SNew(SHeaderRow)
								+SHeaderRow::Column(FTemplateNamedAttributeMappingsColumns::NameId)
									.FillWidth(0.5f)
									.DefaultLabel(LOCTEXT("NameLabel", "Name"))
								/*
								+SHeaderRow::Column(UE::Anim::STF::FNamedAttributeMappingsColumns::TypeId)
									.FillWidth(0.1f)
									.DefaultLabel(LOCTEXT("TypeLabel", "Type"))
								*/
								+SHeaderRow::Column(FTemplateNamedAttributeMappingsColumns::ValueId)
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
	
	void SAttributeMappingsTreeView::OnNamedAttributeSetsChanged()
	{
		RegenerateNamedAttributeSetOptions();
		RegenerateListViewItems();
	}

	class SListViewRow : public SMultiColumnTableRow<TSharedPtr<SAttributeMappingsTreeView::FListItem>>
	{
	public:
		DECLARE_DELEGATE_RetVal_OneParam(bool /* Success */, FOnSourceSetChanged, FName /* New Source Set */);
		DECLARE_DELEGATE_RetVal_TwoParams(bool /* Success */, FOnRenamed, FName /* Old Name */, FName /* New Name */);

		SLATE_BEGIN_ARGS(SListViewRow) { }
			SLATE_ARGUMENT(TSharedPtr<SAttributeMappingsTreeView::FListItem>, ListItem)
			SLATE_ARGUMENT(TArray<TSharedPtr<FName>>*, SourceSetOptions)
			SLATE_EVENT(FOnRenamed, OnRenamed);
			SLATE_EVENT(FOnSourceSetChanged, OnSourceSetChanged);
			
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
		{
			ListItem = InArgs._ListItem;
			OnRenamed = InArgs._OnRenamed;
			OnSourceSetChanged = InArgs._OnSourceSetChanged;
			SourceSetOptions = InArgs._SourceSetOptions;

			FSuperRowType::Construct(
				FSuperRowType::FArguments(),
				InOwnerTableView);
		}

		TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
		{
			if (ColumnName == FTemplateNamedAttributeMappingListColumns::NameId)
			{
				TSharedPtr<SInlineEditableTextBlock> InlineWidget;

				TSharedRef<SInlineEditableTextBlock> Row = SAssignNew(InlineWidget, SInlineEditableTextBlock)
					.Text_Lambda([this]()
					{
						return FText::FromName(ListItem->Name);
					})
					.OnTextCommitted_Lambda([this](const FText& InText, ETextCommit::Type CommitInfo)
					{
						const FName OldName = ListItem->Name;
						const FName NewName = FName(InText.ToString());

						if (OnRenamed.IsBound())
						{
							const bool bSuccess = OnRenamed.Execute(OldName, NewName);
							if (bSuccess)
							{
								ListItem->Name = NewName;
							}
						}
					});
				
				ListItem->OnRenameRequested.BindSP(InlineWidget.Get(), &SInlineEditableTextBlock::EnterEditingMode);
		
				return Row;
			}

			if (ColumnName == FTemplateNamedAttributeMappingListColumns::SourceSetId)
			{
				TSharedPtr<FName> InitialValue;
				for (TSharedPtr<FName> SourceSetName : *SourceSetOptions)
				{
					if (*SourceSetName == ListItem->SourceSetName)
					{
						InitialValue = SourceSetName;
						break;
					}
				}
				
				return SNew(SComboBox<TSharedPtr<FName>>)
					.InitiallySelectedItem(InitialValue)
					.OptionsSource(SourceSetOptions)
					.OnGenerateWidget_Lambda([](TSharedPtr<FName> SourceSetName)
						{
							return SNew(STextBlock)
								.Text(FText::FromName(*SourceSetName));
						})
					.OnSelectionChanged_Lambda([this](TSharedPtr<FName> NewSelection, ESelectInfo::Type)
						{
							if (NewSelection && OnSourceSetChanged.IsBound())
							{
								const bool bSuccess = OnSourceSetChanged.Execute(*NewSelection);
								if (bSuccess)
								{
									ListItem->SourceSetName = *NewSelection;
								}
							}
						})
					[
						SNew(STextBlock)
							.Text_Lambda([this]()
							{
								return FText::FromName(ListItem->SourceSetName);
							})
					];
			}

			return SNullWidget::NullWidget;
		}

	private:
		FOnSourceSetChanged OnSourceSetChanged;
		
		TArray<TSharedPtr<FName>>* SourceSetOptions;
		
		TSharedPtr<SAttributeMappingsTreeView::FListItem> ListItem;
		FOnRenamed OnRenamed;
	};
	
	TSharedRef<ITableRow> SAttributeMappingsTreeView::ListView_GenerateItemRow(TSharedPtr<FListItem> Item, const TSharedRef<STableViewBase>& OwnerTable)
	{
		return SNew(SListViewRow, OwnerTable)
			.ListItem(Item)
			.OnRenamed_Lambda([this](const FName OldAttributeSetName, const FName NewAttributeSetName)
			{
				return SkeletonTemplate->RenameNamedAttributeMapping(OldAttributeSetName, NewAttributeSetName);
			})
			.OnSourceSetChanged_Lambda([this, Item](const FName NewSourceSetName)
			{
				const bool bSuccess = SkeletonTemplate->SetNamedAttributeMappingSourceSet(Item->Name, NewSourceSetName);
				if (SelectedMappingName == Item->Name)
				{
					RegenerateTreeViewItems();
				}
				return bSuccess;
			})
			.SourceSetOptions(&SourceSetOptions);
	}
	
	void SAttributeMappingsTreeView::ListView_OnItemScrolledIntoView(TSharedPtr<FListItem> InItem, const TSharedPtr<ITableRow>& InWidget)
	{
		if (ListView_DeferredRenameRequest.IsValid())
		{
			ListView_DeferredRenameRequest->OnRenameRequested.ExecuteIfBound();
			ListView_DeferredRenameRequest.Reset();
		}
	}

	void SAttributeMappingsTreeView::ListView_OnSelectionChanged(TSharedPtr<FListItem> InItem, ESelectInfo::Type InSelectInfo)
	{
		SelectedMappingName = InItem ? InItem->Name : NAME_None;
		RegenerateTreeViewItems();
	}
	
	TSharedPtr<SWidget> SAttributeMappingsTreeView::ListView_HandleContextMenuOpening()
	{
		FMenuBuilder MenuBuilder(true, nullptr);

		const TArray<TSharedPtr<FListItem>> Selection = ListView->GetSelectedItems();
		if (Selection.IsEmpty())
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT("AddNamedAttributeMapping", "Add Named Attribute Mapping"),
				LOCTEXT("AddNamedAttributeMapping_Tooltip", "Create a new named attribute mapping"),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Plus"),
				FUIAction(
					FExecuteAction::CreateLambda([this]()
					{
						SkeletonTemplate->AddAttributeMapping("NewAttributeMapping");
						RegenerateListViewItems();

						for (const TSharedPtr<FListItem>& Item : ListItems)
						{
							if (Item->Name == "NewAttributeMapping")
							{
								ListView->RequestScrollIntoView(Item);
								ListView_DeferredRenameRequest = Item;
							}
						}
					})
				));
		}
		
		return MenuBuilder.MakeWidget();
	}

	void SAttributeMappingsTreeView::TreeView_HandleGetChildren(TSharedPtr<ITreeItem> InItem, TArray<TSharedPtr<ITreeItem>>& OutChildren) const
	{
		InItem->GetChildren(OutChildren);
	}

	void SAttributeMappingsTreeView::TreeView_OnSelectionChanged(TSharedPtr<ITreeItem> InItem, ESelectInfo::Type InSelectInfo)
	{
		if (InItem)
		{			
			const FSkeletonNamedAttributeMapping* const Mapping = SkeletonTemplate->FindNamedAttributeMapping(SelectedMappingName);
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

	TSharedRef<ITableRow> SAttributeMappingsTreeView::TreeView_GenerateItemRow(TSharedPtr<SAttributeMappingsTreeView::ITreeItem> Item, const TSharedRef<STableViewBase>& OwnerTable)
	{
		return SNew(SNamedElementMappingTableRow, OwnerTable, SharedThis(this))
			.TreeItem(Item);
	}

	TSharedPtr<SWidget> SAttributeMappingsTreeView::TreeView_HandleContextMenuOpening()
	{
		FMenuBuilder MenuBuilder(true, nullptr);
		return MenuBuilder.MakeWidget();
	}

	TArray<TSharedPtr<UE::Anim::STF::SAttributeMappingsTreeView::ITreeItem>> SAttributeMappingsTreeView::GetAllTreeItems()
	{
		using namespace UE::Anim::STF;

		TArray<TSharedPtr<SAttributeMappingsTreeView::ITreeItem>> AllItems;
		AllItems.Append(RootItems);
	
		for (int32 Index = 0; Index < AllItems.Num(); ++Index)
		{
			AllItems[Index]->GetChildren(AllItems);
		}

		return AllItems;
	}

	void SAttributeMappingsTreeView::RegenerateNamedAttributeSetOptions()
	{
		SourceSetOptions.Reset();
		
		SourceSetOptions.Add(MakeShared<FName>(NAME_None));
		for (const FSkeletonNamedAttributeSet& AttributeSet : SkeletonTemplate->GetNamedAttributeSets())
		{
			SourceSetOptions.Add(MakeShared<FName>(AttributeSet.Name));
		}
	}

	void SAttributeMappingsTreeView::RegenerateListViewItems()
	{
		ListItems.Reset();

		const TArray<FSkeletonNamedAttributeMapping>& Mappings = SkeletonTemplate->GetNamedAttributeMappings();
		for (const FSkeletonNamedAttributeMapping& Mapping : Mappings)
		{
			TSharedPtr<FListItem> ListItem = MakeShared<FListItem>();
			ListItem->Name = Mapping.Name;
			ListItem->SourceSetName = Mapping.SourceAttributeSet;

			ListItems.Add(ListItem);
		}
	}

	void SAttributeMappingsTreeView::RegenerateTreeViewItems()
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
			TMap<FName, TSharedPtr<FAttributeTreeItem>> AttributesItemMap;

			if (const FSkeletonNamedAttributeMapping* const AttributeMapping = SkeletonTemplate->FindNamedAttributeMapping(SelectedMappingName))
			{
				TArray<TPair<FSkeletonNamedAttribute, TSharedPtr<FAttributeTreeItem>>> NamedAttributesQueue;
				
				for (const FSkeletonNamedAttributeMappingEntry& MappingEntry : AttributeMapping->TableData)
				{
					const FSkeletonNamedAttribute* NamedAttribute = SkeletonTemplate->FindNamedAttribute(MappingEntry.AttributeName);
					check(NamedAttribute);
					
					TSharedPtr<FAttributeTreeItem> MappingTreeItem = MakeShared<FAttributeTreeItem>();
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
					NamedAttributesQueue.Add(TPair<FSkeletonNamedAttribute, TSharedPtr<FAttributeTreeItem>>(*NamedAttribute, MappingTreeItem));
				}

				while (!NamedAttributesQueue.IsEmpty())
				{
					const TPair<FSkeletonNamedAttribute, TSharedPtr<FAttributeTreeItem>> Attribute = NamedAttributesQueue.Pop();
					const FSkeletonNamedAttribute NamedAttribute = Attribute.Key;
					TSharedPtr<FAttributeTreeItem> AttributeTreeItem = Attribute.Value;

					FName AncestorName = NamedAttribute.ParentName;
					TSharedPtr<FAttributeTreeItem> ClosestAncestorItem = nullptr;
					while (AncestorName != NAME_None)
					{
						TSharedPtr<FAttributeTreeItem>* FoundClosestAncestorItem = AttributesItemMap.Find(AncestorName);
						if (FoundClosestAncestorItem)
						{
							ClosestAncestorItem = *FoundClosestAncestorItem;
							break;
						}

						if (const FSkeletonNamedAttribute* const Parent = SkeletonTemplate->FindNamedAttribute(AncestorName))
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