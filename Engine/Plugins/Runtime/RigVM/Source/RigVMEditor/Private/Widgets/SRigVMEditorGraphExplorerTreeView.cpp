// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SRigVMEditorGraphExplorerTreeView.h"

#include "SPinTypeSelector.h"
#include "EdGraph/RigVMEdGraphSchema.h"
#include "Editor/RigVMNewEditor.h"
#include "RigVMModel/Nodes/RigVMLibraryNode.h"
#include "Widgets/SRigVMEditorGraphExplorer.h"

#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/Text/SRichTextBlock.h"
#include "Widgets/Input/SCheckBox.h"

#define LOCTEXT_NAMESPACE "SRigVMEditorGraphExplorerTreeView"

static const FText GraphsSectionName = LOCTEXT("Graphs", "Graphs");
static const FText FunctionsSectionName = LOCTEXT("Functions", "Functions");
static const FText VariablesSectionName = LOCTEXT("Variables", "Variables");
static const FText LocalVariablesSectionName = LOCTEXT("LocalVariables", "Local Variables");

void SRigVMEditorGraphExplorerItem::Construct(const FArguments& InArgs,
                                              const TSharedRef<STableViewBase>& InOwnerTable,
                                              TSharedRef<FRigVMEditorGraphExplorerTreeElement> InElement,
                                              TSharedPtr<SRigVMEditorGraphExplorerTreeView> InTreeView)
{
	WeakExplorerElement = InElement;
	Delegates = InTreeView->GetRigTreeDelegates();
	OnAddClickedOnSection = InArgs._OnAddClickedOnSection;

	FSlateFontInfo NameFont = FCoreStyle::GetDefaultFontStyle("Regular", 10);

	if (InElement->Key.Type == ERigVMExplorerElementType::Section)
	{
		STableRow<TSharedPtr<FRigVMEditorGraphExplorerTreeElement>>::Construct(
		STableRow<TSharedPtr<FRigVMEditorGraphExplorerTreeElement>>::FArguments()
		.ShowWires(false)
		.Padding(FMargin(0.0f, 2.0f, .0f, 0.0f))
		.Content()
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("Brushes.Header")) // SCategoryHeaderTableRow::GetBackgroundImage
			.Padding(FMargin(3.0f, 5.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				[
					SNew(SRichTextBlock)
					.Text(FText::FromString(InElement->Key.Name))
					.TransformPolicy(ETextTransformPolicy::ToUpper)
					.DecoratorStyleSet(&FAppStyle::Get())
					.TextStyle(FAppStyle::Get(), "DetailsView.CategoryTextStyle")
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Right)
				.Padding(FMargin(0,0,2,0))
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "SimpleButton")
					.ContentPadding(FMargin(1, 0))
					.ForegroundColor(FAppStyle::GetSlateColor("DefaultForeground"))
					.OnClicked(this, &SRigVMEditorGraphExplorerItem::OnAddButtonClickedOnSection, InElement)
					// .IsEnabled(this, &SRigVMEditorGraphExplorer::CanAddNewElementToSection, InSectionID)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SNew(SImage)
						.Image(FAppStyle::GetBrush(TEXT("Icons.PlusCircle")))
						.ColorAndOpacity(FSlateColor::UseForeground())
						.ToolTipText(LOCTEXT("AddNewGraphTooltip", "Create a new graph"))
					]
				]
			]
			
		], InOwnerTable);
	}
	else if(InElement->Key.Type == ERigVMExplorerElementType::FunctionCategory
		|| InElement->Key.Type == ERigVMExplorerElementType::VariableCategory)
	{
		int32 LastSeparator;
		InElement->Key.Name.FindLastChar('|', LastSeparator);
		FString CategoryName = InElement->Key.Name.RightChop(LastSeparator+1);
		CategoryName[0] = FChar::ToUpper(CategoryName[0]);
		STableRow<TSharedPtr<FRigVMEditorGraphExplorerTreeElement>>::Construct(
		STableRow<TSharedPtr<FRigVMEditorGraphExplorerTreeElement>>::FArguments()
		.ShowWires(false)
		.Padding(FMargin(0.0f, 2.0f, .0f, 0.0f))
		.Content()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			[
				SAssignNew(InlineRenameWidget, SInlineEditableTextBlock)
				.Font( FCoreStyle::GetDefaultFontStyle("Bold", 9) )
				.Text( FText::FromString(CategoryName ))
				.ToolTipText( FText::FromString(CategoryName ))
				.HighlightText( InTreeView->FilterText )
				.OnVerifyTextChanged(this, &SRigVMEditorGraphExplorerItem::OnVerifyNameChanged)
				.OnTextCommitted(this, &SRigVMEditorGraphExplorerItem::OnNameCommitted)
			]
		], InOwnerTable);
	}
	else if (InElement->Key.Type == ERigVMExplorerElementType::Variable || InElement->Key.Type == ERigVMExplorerElementType::LocalVariable)
	{
		TArray<TSharedPtr<IPinTypeSelectorFilter>> CustomPinTypeFilters = Delegates.GetCustomPinFilters();
		
		STableRow<TSharedPtr<FRigVMEditorGraphExplorerTreeElement>>::Construct(
		   STableRow<TSharedPtr<FRigVMEditorGraphExplorerTreeElement>>::FArguments()
		   .ShowWires(false)
		   .OnDragDetected(Delegates.OnDragDetected)
		   .Content()
		   [
			   SNew(SHorizontalBox)
			   +SHorizontalBox::Slot()
				   .FillWidth(0.6f)
				   .VAlign(VAlign_Center)
				   .Padding(3.0f, 0.0f)
				   [
					   CreateTextSlotWidget(InElement->Key, InTreeView->FilterText)
				   ]

				+SHorizontalBox::Slot()
				   .FillWidth(0.4f)
				   .HAlign(HAlign_Left)
				   .VAlign(VAlign_Center)
				   [
						SNew(SPinTypeSelector, FGetPinTypeTree::CreateUObject(GetDefault<UEdGraphSchema_K2>(), &UEdGraphSchema_K2::GetVariableTypeTree))
						.Schema(GetDefault<URigVMEdGraphSchema>())
						.TargetPinType_Lambda([this, InElement]() { return Delegates.GetVariablePinType(InElement->Key); })
						.OnPinTypeChanged_Lambda([this, InElement](const FEdGraphPinType& InType) { Delegates.SetVariablePinType(InElement->Key, InType); })
						.TypeTreeFilter(ETypeTreeFilter::None)
						.SelectorType(SPinTypeSelector::ESelectorType::Partial)
						.CustomFilters(CustomPinTypeFilters)
				   ]

				+SHorizontalBox::Slot()
				   .AutoWidth()
				   .Padding(FMargin(6.0f, 0.0f, 3.0f, 0.0f))
				   .HAlign(HAlign_Right)
				   .VAlign(VAlign_Center)
				   [
					   SNew(SBorder)
					   .Padding(0.0f)
					   .Visibility(InElement->Key.Type == ERigVMExplorerElementType::Variable ? EVisibility::Visible : EVisibility::Collapsed)
					   .BorderImage(FStyleDefaults::GetNoBrush())
					   [
						   SNew(SCheckBox)
						   .ToolTipText_Lambda([this, InElement]()
						   {
						   	FText ToolTipText = FText::GetEmpty();
						   	if (Delegates.IsVariablePublic(InElement->Key.Name))
							{
								ToolTipText = LOCTEXT("VariablePrivacy_is_public_Tooltip", "Variable is public and is editable on each instance of this Blueprint.");
							}
							else
							{
								ToolTipText = LOCTEXT("VariablePrivacy_not_public_Tooltip", "Variable is not public and will not be editable on an instance of this Blueprint.");
							}
							return ToolTipText;
						   })
						   .OnCheckStateChanged_Lambda([this, InElement](ECheckBoxState) { Delegates.ToggleVariablePublic(InElement->Key.Name); })
						   .IsChecked_Lambda([this, InElement]() { return Delegates.IsVariablePublic(InElement->Key.Name) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
						   .Style(FAppStyle::Get(), "TransparentCheckBox")
						   [
							   SNew(SImage)
							   .Image_Lambda([this, InElement]()
							   {
								   if (Delegates.IsVariablePublic(InElement->Key.Name))
								   {
									   return FAppStyle::GetBrush( "Kismet.VariableList.HideForInstance" );
								   }
							   		return FAppStyle::GetBrush( "Kismet.VariableList.ExposeForInstance" );
							   })
							   .ColorAndOpacity(FSlateColor::UseForeground())
						   ]
					   ]
				   ]

		   ], InOwnerTable);
	}
	else
	{
		STableRow<TSharedPtr<FRigVMEditorGraphExplorerTreeElement>>::Construct(
		   STableRow<TSharedPtr<FRigVMEditorGraphExplorerTreeElement>>::FArguments()
		   .ShowWires(false)
		   .OnDragDetected(Delegates.OnDragDetected)
		   .Content()
		   [
			   SNew(SHorizontalBox)
			   + SHorizontalBox::Slot()
			   .AutoWidth()
			   .VAlign(VAlign_Center)
			   [
				   CreateIconWidget(InElement->Key)
			   ]
			   + SHorizontalBox::Slot()
			   .FillWidth(1.f)
			   .VAlign(VAlign_Center)
			   .Padding(/* horizontal */ 3.0f, /* vertical */ 3.0f)
			   [
				   CreateTextSlotWidget(InElement->Key, InTreeView->FilterText)
			   ]
		   ], InOwnerTable);
	}

	if(WeakExplorerElement.IsValid() && InlineRenameWidget.IsValid())
	{
		WeakExplorerElement.Pin()->OnRenameRequested.BindSP(InlineRenameWidget.Get(), &SInlineEditableTextBlock::EnterEditingMode);
	}
}

TSharedRef<SWidget> SRigVMEditorGraphExplorerItem::CreateIconWidget(const FRigVMExplorerElementKey& Key)
{
	TSharedPtr<SWidget> IconWidget;

	switch (Key.Type)
	{
		case ERigVMExplorerElementType::Graph:
			{
				IconWidget = SNew(SImage)
							.Image(Delegates.GetGraphIcon(Key.Name));
				break;
			}
		case ERigVMExplorerElementType::Event:
			{
				static FSlateIcon EventIcon(FAppStyle::GetAppStyleSetName(), "GraphEditor.Event_16x");
				IconWidget = SNew(SImage)
							.Image(EventIcon.GetIcon());
				break;
			}
		case ERigVMExplorerElementType::Function:
			{
				static FSlateIcon FunctionIcon(FAppStyle::GetAppStyleSetName(), "Kismet.AllClasses.FunctionIcon");
				IconWidget = SNew(SImage)
							.Image(FunctionIcon.GetIcon());
				break;
			}
	}

	if (IconWidget.IsValid())
	{
		return IconWidget.ToSharedRef();
	}
	else
	{
		return SNullWidget::NullWidget;
	}
}

TSharedRef<SWidget> SRigVMEditorGraphExplorerItem::CreateTextSlotWidget(const FRigVMExplorerElementKey& Key, const FText& InHighlightText)
{
	TSharedPtr<SInlineEditableTextBlock> EditableTextElement = SNew(SInlineEditableTextBlock)
		.Text(this, &SRigVMEditorGraphExplorerItem::GetDisplayText)
		.HighlightText(InHighlightText)
		.ToolTipText(this, &SRigVMEditorGraphExplorerItem::GetItemTooltip)
		.OnVerifyTextChanged(this, &SRigVMEditorGraphExplorerItem::OnVerifyNameChanged)
		.OnTextCommitted(this, &SRigVMEditorGraphExplorerItem::OnNameCommitted);

	InlineRenameWidget = EditableTextElement.ToSharedRef();

	return InlineRenameWidget.ToSharedRef();
}

FText SRigVMEditorGraphExplorerItem::GetDisplayText() const
{
	FText DisplayText;
	if (!WeakExplorerElement.IsValid())
	{
		return DisplayText;
	}
	TSharedPtr<FRigVMEditorGraphExplorerTreeElement> Element = WeakExplorerElement.Pin();

	switch (Element->Key.Type)
	{
		case ERigVMExplorerElementType::Graph:
			{
				return Delegates.GetGraphDisplayName(Element->Key.Name);
			}
		case ERigVMExplorerElementType::Event:
			{
				return Delegates.GetEventDisplayName(Element->Key.Name);
			}
		case ERigVMExplorerElementType::Function:
		case ERigVMExplorerElementType::Variable:
		case ERigVMExplorerElementType::LocalVariable:
			{
				return FText::FromString(Element->Key.Name);
			}
	}
	
	return DisplayText;
}

FText SRigVMEditorGraphExplorerItem::GetItemTooltip() const
{
	FText Tooltip;
	if (!WeakExplorerElement.IsValid())
	{
		return Tooltip;
	}
	TSharedPtr<FRigVMEditorGraphExplorerTreeElement> Element = WeakExplorerElement.Pin();

	switch (Element->Key.Type)
	{
		case ERigVMExplorerElementType::Graph:
			{
				return Delegates.GetGraphTooltip(Element->Key.Name);
			}
		case ERigVMExplorerElementType::Event:
			{
				return Delegates.GetEventDisplayName(Element->Key.Name);
			}
	}
	
	return Tooltip;
}

bool SRigVMEditorGraphExplorerItem::OnVerifyNameChanged(const FText& InText, FText& OutErrorMessage)
{
	FString NewName = InText.ToString();
	const FRigVMExplorerElementKey OldKey = WeakExplorerElement.Pin()->Key;

	switch (OldKey.Type)
	{
		case ERigVMExplorerElementType::Section:
			{
				break;
			}
		case ERigVMExplorerElementType::FunctionCategory:
		case ERigVMExplorerElementType::VariableCategory:
			{
				return true;
			}
		case ERigVMExplorerElementType::Graph:
			{
				return Delegates.CanRenameGraph(OldKey.Name, *NewName, OutErrorMessage);
			}
		case ERigVMExplorerElementType::Function:
			{
				return Delegates.CanRenameFunction(OldKey.Name, *NewName, OutErrorMessage);
			}
		case ERigVMExplorerElementType::Variable:
		case ERigVMExplorerElementType::LocalVariable:
			{
				return Delegates.CanRenameVariable(OldKey, *NewName, OutErrorMessage);
			}
	}
	return false;
}

void SRigVMEditorGraphExplorerItem::OnNameCommitted(const FText& InText, ETextCommit::Type InCommitType) const
{
	if (!WeakExplorerElement.IsValid())
	{
		return;
	}
	
	// for now only allow enter
	if (InCommitType == ETextCommit::OnEnter)
	{
		FString NewName = InText.ToString();
		TSharedPtr<FRigVMEditorGraphExplorerTreeElement> Element = WeakExplorerElement.Pin();
		const FRigVMExplorerElementKey OldKey = Element->Key;

		switch (OldKey.Type)
		{
			case ERigVMExplorerElementType::Section:
				{
					break;
				}
			case ERigVMExplorerElementType::FunctionCategory:
			case ERigVMExplorerElementType::VariableCategory:
				{
					int32 LastSeparator;
					FString OldCategoryPath = Element->Key.Name;
					Element->Key.Name.FindLastChar('|', LastSeparator);
					FString Prefix;
					if (LastSeparator != INDEX_NONE)
					{
						Prefix = Element->Key.Name.LeftChop(Element->Key.Name.Len() - LastSeparator);
					}
					if (!Prefix.IsEmpty() && !InText.IsEmpty())
					{
						Prefix.Append(TEXT("|"));
					}
					FString NewCategoryPath = FString::Printf(TEXT("%s%s"), *Prefix, *InText.ToString());
					TArray<TSharedPtr<FRigVMEditorGraphExplorerTreeElement>> SubElements = Element->Children;
					for (int32 i=0; i<SubElements.Num(); ++i)
					{
						TSharedPtr<FRigVMEditorGraphExplorerTreeElement> SubElement = SubElements[i];
						if (SubElement->Key.Type == ERigVMExplorerElementType::Function || SubElement->Key.Type == ERigVMExplorerElementType::Variable || SubElement->Key.Type == ERigVMExplorerElementType::LocalVariable)
						{
							FString Category = OldKey.Type == ERigVMExplorerElementType::FunctionCategory ?
								Delegates.GetFunctionCategory(SubElement->Key.Name)
									: Delegates.GetVariableCategory(SubElement->Key.Name);
							Category.RemoveFromStart(OldCategoryPath);
							Category.InsertAt(0, NewCategoryPath);
							Category.RemoveFromStart(TEXT("|"));
							Category.RemoveFromEnd(TEXT("|"));
							if (OldKey.Type == ERigVMExplorerElementType::FunctionCategory)
							{
								Delegates.SetFunctionCategory(SubElement->Key.Name, Category);
							}
							else if (OldKey.Type == ERigVMExplorerElementType::VariableCategory)
							{
								Delegates.SetVariableCategory(SubElement->Key.Name, Category);
							}
						}
						SubElements.Append(SubElement->Children);
					}
					break;
				}
			case ERigVMExplorerElementType::Graph:
				{
					Delegates.RenameGraph(OldKey.Name, *NewName);
					break;
				}
			case ERigVMExplorerElementType::Function:
				{
					Delegates.RenameFunction(OldKey.Name, *NewName);
					break;
				}
			case ERigVMExplorerElementType::Variable:
			case ERigVMExplorerElementType::LocalVariable:
				{
					Delegates.RenameVariable(OldKey, *NewName);
					break;
				}
		}
	}
}

FReply SRigVMEditorGraphExplorerItem::OnAddButtonClickedOnSection(TSharedRef<FRigVMEditorGraphExplorerTreeElement> InElement)
{
	if (OnAddClickedOnSection.IsBound())
	{
		return OnAddClickedOnSection.Execute(InElement->Key);
	}
	return FReply::Unhandled();
}

TSharedRef<ITableRow> FRigVMEditorGraphExplorerTreeElement::MakeTreeRowWidget(const TSharedRef<STableViewBase>& InOwnerTable,
	TSharedRef<FRigVMEditorGraphExplorerTreeElement> InElement, TSharedPtr<SRigVMEditorGraphExplorerTreeView> InTreeView)
{
	return SNew(SRigVMEditorGraphExplorerItem, InOwnerTable, InElement, InTreeView)
	.OnAddClickedOnSection(InTreeView.Get(), &SRigVMEditorGraphExplorerTreeView::OnAddButtonClickedOnSection);
}

void FRigVMEditorGraphExplorerTreeElement::RequestRename()
{
	OnRenameRequested.ExecuteIfBound();
}

void SRigVMEditorGraphExplorerTreeView::Construct(const FArguments& InArgs)
{
	Delegates = InArgs._RigTreeDelegates;
	
	STreeView<TSharedPtr<FRigVMEditorGraphExplorerTreeElement>>::FArguments SuperArgs;
	SuperArgs.SelectionMode(ESelectionMode::Single);
	SuperArgs.TreeItemsSource(&RootElements);
	SuperArgs.OnGenerateRow(this, &SRigVMEditorGraphExplorerTreeView::MakeTableRowWidget);
	SuperArgs.OnGetChildren(this, &SRigVMEditorGraphExplorerTreeView::HandleGetChildrenForTree);
	SuperArgs.HighlightParentNodesForSelection(true);
	SuperArgs.AllowInvisibleItemSelection(true);  //without this we deselect everything when we filter or we collapse
	SuperArgs.OnGeneratePinnedRow(this, &SRigVMEditorGraphExplorerTreeView::MakeTableRowWidget);
	SuperArgs.OnMouseButtonClick(this, &SRigVMEditorGraphExplorerTreeView::OnItemClicked);
	SuperArgs.OnMouseButtonDoubleClick(this, &SRigVMEditorGraphExplorerTreeView::OnItemDoubleClicked);
	SuperArgs.OnSelectionChanged(FRigVMGraphExplorer_OnSelectionChanged::CreateRaw(&Delegates, &FRigVMEditorGraphExplorerTreeDelegates::HandleSelectionChanged));
	SuperArgs.OnContextMenuOpening(FRigVMGraphExplorer_OnRequestContextMenu::CreateRaw(&Delegates, &FRigVMEditorGraphExplorerTreeDelegates::RequestContextMenu));
	

	STreeView<TSharedPtr<FRigVMEditorGraphExplorerTreeElement>>::Construct(SuperArgs);
}

void SRigVMEditorGraphExplorerTreeView::RefreshTreeView(bool bRebuildContent)
{
	TArray<FRigVMExplorerElementKey> Selection;
	FString FilterTextStr = FilterText.ToString();
	
	if(bRebuildContent)
	{
		// store expansion state
		TMap<FRigVMExplorerElementKey, bool> ExpansionState;
		for (TPair<FRigVMExplorerElementKey, TSharedPtr<FRigVMEditorGraphExplorerTreeElement>> Pair : ElementMap)
		{
			ExpansionState.FindOrAdd(Pair.Key) = IsItemExpanded(Pair.Value);
		}

		Selection = GetSelectedKeys();

		// internally save expansion states before rebuilding the tree, so the states can be restored later
		SaveAndClearSparseItemInfos();
		
		RootElements.Reset();
		ElementMap.Reset();
		ParentMap.Reset();

		// Add Elements
		{
			// GRAPHS section
			{
				FRigVMExplorerElementKey SectionKey(ERigVMExplorerElementType::Section, GraphsSectionName.ToString());
				TSharedPtr<FRigVMEditorGraphExplorerTreeElement> SectionItem = MakeShared<FRigVMEditorGraphExplorerTreeElement>(SectionKey, SharedThis(this));
				ElementMap.Add(SectionKey, SectionItem);
				RootElements.Add(SectionItem);
				SetItemExpansion(SectionItem, true);

				TArray<const URigVMGraph*> Graphs = Delegates.GetRootGraphs();
				for (const URigVMGraph* Graph : Graphs)
				{
					FRigVMExplorerElementKey Key(ERigVMExplorerElementType::Graph, Graph->GetNodePath());
					ParentMap.Add(Key, SectionKey);
				}
			
				// Add Graphs to section
				for (int32 i=0; i<Graphs.Num(); ++i)
				{
					const URigVMGraph* Graph = Graphs[i];
				
					FRigVMExplorerElementKey Key(ERigVMExplorerElementType::Graph, Graph->GetNodePath());
				
					TSharedPtr<FRigVMEditorGraphExplorerTreeElement> NewItem = MakeShared<FRigVMEditorGraphExplorerTreeElement>(Key, SharedThis(this));
					ElementMap.Add(Key, NewItem);
					SetItemExpansion(NewItem, true);

					TArray<const URigVMGraph*> ChildrenGraphs = Delegates.GetChildrenGraphs(Key.Name);
					for (const URigVMGraph* Child : ChildrenGraphs)
					{
						FRigVMExplorerElementKey ChildKey(ERigVMExplorerElementType::Graph, Child->GetNodePath());
						ParentMap.Add(ChildKey, Key);
						Graphs.Add(Child);
					}

					TArray<URigVMNode*> EventNodes = Delegates.GetEventNodesInGraph(Key.Name);
					for (const URigVMNode* EventNode : EventNodes)
					{
					
						FRigVMExplorerElementKey EventKey(ERigVMExplorerElementType::Event, EventNode->GetNodePath());
						ParentMap.Add(EventKey, Key);
						TSharedPtr<FRigVMEditorGraphExplorerTreeElement> EventItem = MakeShared<FRigVMEditorGraphExplorerTreeElement>(EventKey, SharedThis(this));
						ElementMap.Add(EventKey, EventItem);
						NewItem->Children.Add(EventItem);
					}

					FRigVMExplorerElementKey ParentKey = ParentMap.FindChecked(Key);
					TSharedPtr<FRigVMEditorGraphExplorerTreeElement> ParentItem = ElementMap.FindChecked(ParentKey);

					ParentItem->Children.Add(NewItem);
				}
			}

			// FUNCTIONS section
			{
				FRigVMExplorerElementKey SectionKey(ERigVMExplorerElementType::Section, FunctionsSectionName.ToString());
				TSharedPtr<FRigVMEditorGraphExplorerTreeElement> SectionItem = MakeShared<FRigVMEditorGraphExplorerTreeElement>(SectionKey, SharedThis(this));
				ElementMap.Add(SectionKey, SectionItem);
				RootElements.Add(SectionItem);
				SetItemExpansion(SectionItem, true);

				TArray<URigVMLibraryNode*> Functions = Delegates.GetFunctions();
				for (const URigVMLibraryNode* Function : Functions)
				{
					const FString Category = Function->GetNodeCategory();
					TArray<FString> SingleCategories;
					Category.ParseIntoArray(SingleCategories, TEXT("|"));
					FString CategoryParentPath;
					TSharedPtr<FRigVMEditorGraphExplorerTreeElement> PreviousParentItem = SectionItem;
					for (const FString& SingleCategory : SingleCategories)
					{
						if (!CategoryParentPath.IsEmpty())
						{
							CategoryParentPath.Append(TEXT("|"));
						}
						CategoryParentPath.Append(SingleCategory);
						FRigVMExplorerElementKey ParentKey(ERigVMExplorerElementType::FunctionCategory, CategoryParentPath);
						TSharedPtr<FRigVMEditorGraphExplorerTreeElement> ParentItem = FindElement(ParentKey);
						if (!ParentItem)
						{
							ParentItem = MakeShared<FRigVMEditorGraphExplorerTreeElement>(ParentKey, SharedThis(this));
							ElementMap.Add(ParentKey, ParentItem);
							ParentMap.Add(ParentKey, PreviousParentItem->Key);
							PreviousParentItem->Children.Add(ParentItem);
							SetItemExpansion(ParentItem, true);
						}
						PreviousParentItem = ParentItem;
					}
					
					FRigVMExplorerElementKey Key(ERigVMExplorerElementType::Function, Function->GetNodePath());
				
					TSharedPtr<FRigVMEditorGraphExplorerTreeElement> NewItem = MakeShared<FRigVMEditorGraphExplorerTreeElement>(Key, SharedThis(this));
					ElementMap.Add(Key, NewItem);
					ParentMap.Add(Key, PreviousParentItem->Key);
					PreviousParentItem->Children.Add(NewItem);
					SetItemExpansion(NewItem, true);
				}
			}

			// VARIABLES section
			{
				FRigVMExplorerElementKey SectionKey(ERigVMExplorerElementType::Section, VariablesSectionName.ToString());
				TSharedPtr<FRigVMEditorGraphExplorerTreeElement> SectionItem = MakeShared<FRigVMEditorGraphExplorerTreeElement>(SectionKey, SharedThis(this));
				ElementMap.Add(SectionKey, SectionItem);
				RootElements.Add(SectionItem);
				SetItemExpansion(SectionItem, true);

				TArray<FRigVMGraphVariableDescription> Variables = Delegates.GetVariables();
				for (const FRigVMGraphVariableDescription& Variable : Variables)
				{
					const FString Category = Variable.Category.ToString();
					TSharedPtr<FRigVMEditorGraphExplorerTreeElement> PreviousParentItem = SectionItem;
					if (!Category.Equals(UEdGraphSchema_K2::VR_DefaultCategory.ToString()))
					{
						TArray<FString> SingleCategories;
						Category.ParseIntoArray(SingleCategories, TEXT("|"));
						FString CategoryParentPath;
						for (const FString& SingleCategory : SingleCategories)
						{
							if (!CategoryParentPath.IsEmpty())
							{
								CategoryParentPath.Append(TEXT("|"));
							}
							CategoryParentPath.Append(SingleCategory);
							FRigVMExplorerElementKey ParentKey(ERigVMExplorerElementType::VariableCategory, CategoryParentPath);
							TSharedPtr<FRigVMEditorGraphExplorerTreeElement> ParentItem = FindElement(ParentKey);
							if (!ParentItem)
							{
								ParentItem = MakeShared<FRigVMEditorGraphExplorerTreeElement>(ParentKey, SharedThis(this));
								ElementMap.Add(ParentKey, ParentItem);
								ParentMap.Add(ParentKey, PreviousParentItem->Key);
								PreviousParentItem->Children.Add(ParentItem);
								SetItemExpansion(ParentItem, true);
							}
							PreviousParentItem = ParentItem;
						}
					}
					
					FRigVMExplorerElementKey Key(ERigVMExplorerElementType::Variable, Variable.Name.ToString());
				
					TSharedPtr<FRigVMEditorGraphExplorerTreeElement> NewItem = MakeShared<FRigVMEditorGraphExplorerTreeElement>(Key, SharedThis(this));
					ElementMap.Add(Key, NewItem);
					ParentMap.Add(Key, PreviousParentItem->Key);
					PreviousParentItem->Children.Add(NewItem);
					SetItemExpansion(NewItem, true);
				}
			}

			// LOCAL VARIABLES section
			if (Delegates.IsFunctionFocused())
			{
				FRigVMExplorerElementKey SectionKey(ERigVMExplorerElementType::Section, LocalVariablesSectionName.ToString());
				TSharedPtr<FRigVMEditorGraphExplorerTreeElement> SectionItem = MakeShared<FRigVMEditorGraphExplorerTreeElement>(SectionKey, SharedThis(this));
				ElementMap.Add(SectionKey, SectionItem);
				RootElements.Add(SectionItem);
				SetItemExpansion(SectionItem, true);

				TArray<FRigVMGraphVariableDescription> LocalVariables = Delegates.GetLocalVariables();
				for (const FRigVMGraphVariableDescription& Variable : LocalVariables)
				{
					FRigVMExplorerElementKey Key(ERigVMExplorerElementType::LocalVariable, Variable.Name.ToString());
				
					TSharedPtr<FRigVMEditorGraphExplorerTreeElement> NewItem = MakeShared<FRigVMEditorGraphExplorerTreeElement>(Key, SharedThis(this));
					ElementMap.Add(Key, NewItem);
					ParentMap.Add(Key, SectionItem->Key);
					SectionItem->Children.Add(NewItem);
					SetItemExpansion(NewItem, true);
				}
			}
		}

		// Filter Items
		if (!FilterText.IsEmpty())
		{
			for (TSharedPtr<FRigVMEditorGraphExplorerTreeElement> RootElement : RootElements)
			{
				struct Local
				{
					static bool ChildNameSatisfiesFilter(TSharedPtr<FRigVMEditorGraphExplorerTreeElement> InElement, const FString& InFilterText, FRigVMEditorGraphExplorerTreeDelegates& Delegates)
					{
						FText DisplayName;
						switch (InElement->Key.Type)
						{
						case ERigVMExplorerElementType::Graph:
							{
								DisplayName = Delegates.GetGraphDisplayName(InElement->Key.Name);
								break;
							}
						case ERigVMExplorerElementType::Event:
							{
								DisplayName = Delegates.GetEventDisplayName(InElement->Key.Name);
								break;
							}
						case ERigVMExplorerElementType::Function:
						case ERigVMExplorerElementType::Variable:
						case ERigVMExplorerElementType::LocalVariable:
						case ERigVMExplorerElementType::FunctionCategory:
						case ERigVMExplorerElementType::VariableCategory:
							{
								DisplayName = FText::FromString(InElement->Key.Name);
								break;
							}
						}
						
						if (InFilterText.IsEmpty() || DisplayName.ToString().Contains(InFilterText))
						{
							return true;
						}
						return false;
					}
					
					static bool HasVisibleChildren(TSharedPtr<FRigVMEditorGraphExplorerTreeElement> InElement, const FString& InFilterText, FRigVMEditorGraphExplorerTreeDelegates& Delegates)
					{
						TArray<TSharedPtr<FRigVMEditorGraphExplorerTreeElement>> NewChildren;
						NewChildren.Reserve(InElement->Children.Num());
						for (TSharedPtr<FRigVMEditorGraphExplorerTreeElement> Child : InElement->Children)
						{
							if (HasVisibleChildren(Child, InFilterText, Delegates) || ChildNameSatisfiesFilter(Child, InFilterText, Delegates))
							{
								NewChildren.Add(Child);
							}
						}
						InElement->Children = NewChildren;
						return !NewChildren.IsEmpty();
					}
				};
				TArray<TSharedPtr<FRigVMEditorGraphExplorerTreeElement>> NewChildren;
				NewChildren.Reserve(RootElement->Children.Num());
				for (TSharedPtr<FRigVMEditorGraphExplorerTreeElement> Child : RootElement->Children)
				{
					if (Local::HasVisibleChildren(Child, FilterTextStr, Delegates) || Local::ChildNameSatisfiesFilter(Child, FilterTextStr, Delegates))
					{
						NewChildren.Add(Child);
					}
				}
				RootElement->Children = NewChildren;
			}
		}


		// expand all elements upon the initial construction of the tree
		if (ExpansionState.Num() == 0)
		{
			for (TSharedPtr<FRigVMEditorGraphExplorerTreeElement> RootElement : RootElements)
			{
				SetExpansionRecursive(RootElement, false, true);
			}
		}
		// expand any new items
		else if (ExpansionState.Num() < ElementMap.Num())
		{
			for (const TPair<FRigVMExplorerElementKey, TSharedPtr<FRigVMEditorGraphExplorerTreeElement>>& Element : ElementMap)
			{
				if (!ExpansionState.Contains(Element.Key))
				{
					SetItemExpansion(Element.Value, true);
				}
			}
		}

		// restore infos
		for (const TPair<FRigVMExplorerElementKey, TSharedPtr<FRigVMEditorGraphExplorerTreeElement>>& Pair : ElementMap)
		{
			RestoreSparseItemInfos(Pair.Value);
		}
	}


	RequestTreeRefresh();
	{
		TGuardValue<bool> Guard(Delegates.bSuspendSelectionDelegate, true);
		ClearSelection();

		if(!Selection.IsEmpty())
		{
			TArray<FRigVMExplorerElementKey> SelectedElements;
			for(const FRigVMExplorerElementKey& SelectedPath : Selection)
			{
				SelectedElements.Add(SelectedPath);
			}
			if(!SelectedElements.IsEmpty())
			{
				SetSelection(SelectedElements);
			}
		}

		Selection = GetSelectedKeys();
	}
}

TSharedRef<ITableRow> SRigVMEditorGraphExplorerTreeView::MakeTableRowWidget(TSharedPtr<FRigVMEditorGraphExplorerTreeElement> InItem,
	const TSharedRef<STableViewBase>& OwnerTable)
{
	return InItem->MakeTreeRowWidget(OwnerTable, InItem.ToSharedRef(), SharedThis(this));
}

void SRigVMEditorGraphExplorerTreeView::SetExpansionRecursive(TSharedPtr<FRigVMEditorGraphExplorerTreeElement> InElement, bool bTowardsParent, bool bShouldBeExpanded)
{
	SetItemExpansion(InElement, bShouldBeExpanded);

	if (bTowardsParent)
	{
		if (const FRigVMExplorerElementKey* ParentKey = ParentMap.Find(InElement->Key))
		{
			if (TSharedPtr<FRigVMEditorGraphExplorerTreeElement>* ParentItem = ElementMap.Find(*ParentKey))
			{
				SetExpansionRecursive(*ParentItem, bTowardsParent, bShouldBeExpanded);
			}
		}
	}
	else
	{
		for (int32 ChildIndex = 0; ChildIndex < InElement->Children.Num(); ++ChildIndex)
		{
			SetExpansionRecursive(InElement->Children[ChildIndex], bTowardsParent, bShouldBeExpanded);
		}
	}
}

void SRigVMEditorGraphExplorerTreeView::HandleGetChildrenForTree(TSharedPtr<FRigVMEditorGraphExplorerTreeElement> InItem,
	TArray<TSharedPtr<FRigVMEditorGraphExplorerTreeElement>>& OutChildren)
{
	OutChildren = InItem.Get()->Children;
}

TArray<FRigVMExplorerElementKey> SRigVMEditorGraphExplorerTreeView::GetSelectedKeys() const
{
	TArray<FRigVMExplorerElementKey> Keys;
	TArray<TSharedPtr<FRigVMEditorGraphExplorerTreeElement>> SelectedElements = GetSelectedItems();
	for(const TSharedPtr<FRigVMEditorGraphExplorerTreeElement>& SelectedElement : SelectedElements)
	{
		if (SelectedElement.IsValid())
		{
			Keys.AddUnique(SelectedElement->Key);
		}
	}
	return Keys;
}

void SRigVMEditorGraphExplorerTreeView::SetSelection(TArray<FRigVMExplorerElementKey>& InSelectedKeys)
{
	ClearSelection();
	TArray<TSharedPtr<FRigVMEditorGraphExplorerTreeElement>> Selection;
	for (const FRigVMExplorerElementKey& Key : InSelectedKeys)
	{
		if (TSharedPtr<FRigVMEditorGraphExplorerTreeElement>* Element = ElementMap.Find(Key))
		{
			Selection.Add(*Element);
		}
	}
	SetItemSelection(Selection, true, ESelectInfo::Direct);
}

TSharedPtr<FRigVMEditorGraphExplorerTreeElement> SRigVMEditorGraphExplorerTreeView::FindElement(const FRigVMExplorerElementKey& Key)
{
	if (TSharedPtr<FRigVMEditorGraphExplorerTreeElement>* Element = ElementMap.Find(Key))
	{
		return *Element;
	}
	return nullptr;
}

void SRigVMEditorGraphExplorerTreeView::OnItemClicked(TSharedPtr<FRigVMEditorGraphExplorerTreeElement> InElement)
{
	switch(InElement->Key.Type)
	{
		case ERigVMExplorerElementType::Section:
			{
				FRigVMExplorerElementKey SectionKey(ERigVMExplorerElementType::Section, InElement->Key.Name);
				TSharedPtr<FRigVMEditorGraphExplorerTreeElement> Section = ElementMap.FindChecked(SectionKey);
				SetItemExpansion(Section, !IsItemExpanded(Section));
				break;
			}
		case ERigVMExplorerElementType::Graph:
			{
				Delegates.GraphClicked(InElement->Key.Name);
				break;
			}
		case ERigVMExplorerElementType::Event:
			{
				Delegates.EventClicked(InElement->Key.Name);
				break;
			}
		case ERigVMExplorerElementType::Function:
			{
				Delegates.FunctionClicked(InElement->Key.Name);
				break;
			}
		case ERigVMExplorerElementType::Variable:
		case ERigVMExplorerElementType::LocalVariable:
			{
				Delegates.VariableClicked(InElement->Key);
				break;
			}
	}
}

void SRigVMEditorGraphExplorerTreeView::OnItemDoubleClicked(TSharedPtr<FRigVMEditorGraphExplorerTreeElement> InElement)
{
	switch(InElement->Key.Type)
	{
		case ERigVMExplorerElementType::Section:
		case ERigVMExplorerElementType::Variable:
		case ERigVMExplorerElementType::LocalVariable:
			{
				break;
			}
		case ERigVMExplorerElementType::Graph:
			{
				Delegates.GraphDoubleClicked(InElement->Key.Name);
				break;
			}
		case ERigVMExplorerElementType::Event:
			{
				Delegates.EventDoubleClicked(InElement->Key.Name);
			}
		case ERigVMExplorerElementType::Function:
			{
				Delegates.FunctionDoubleClicked(InElement->Key.Name);
			}
	}
}

FReply SRigVMEditorGraphExplorerTreeView::OnAddButtonClickedOnSection(const FRigVMExplorerElementKey& InSectionKey)
{
	if (InSectionKey.Name == GraphsSectionName.ToString())
	{
		Delegates.CreateGraph();
		return FReply::Handled();
	}
	else if (InSectionKey.Name == FunctionsSectionName.ToString())
	{
		Delegates.CreateFunction();
		return FReply::Handled();
	}
	else if (InSectionKey.Name == VariablesSectionName.ToString())
	{
		Delegates.CreateVariable();
		return FReply::Handled();
	}
	else if (InSectionKey.Name == LocalVariablesSectionName.ToString())
	{
		Delegates.CreateLocalVariable();
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

#undef LOCTEXT_NAMESPACE