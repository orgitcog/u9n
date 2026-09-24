// Copyright Epic Games, Inc. All Rights Reserved.

#include "Editor/SModularRigTreeView.h"
#include "Styling/AppStyle.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Text/STextBlock.h"
#include "PropertyCustomizationHelpers.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "Editor/EditorEngine.h"
#include "HelperUtil.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "ControlRig.h"
#include "ControlRigEditorStyle.h"
#include "ModularRig.h"
#include "ModularRigRuleManager.h"
#include "SRigHierarchyTreeView.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Editor/SModularRigModel.h"
#include "Fonts/FontMeasure.h"
#include "Settings/ControlRigSettings.h"
#include "Graph/ControlRigGraphSchema.h"
#include "Rigs/AdditiveControlRig.h"
#include "Rigs/RigHierarchyController.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SButton.h"
#include "ScopedTransaction.h"
#include "DetailLayoutBuilder.h"
#include "Widgets/SRigVMVariantTagWidget.h"
#include "Editor/SRigConnectorTargetWidget.h"

#define LOCTEXT_NAMESPACE "SModularRigTreeView"

TMap<FSoftObjectPath, TSharedPtr<FSlateBrush>> FModularRigTreeElement::IconPathToBrush;

//////////////////////////////////////////////////////////////
/// FModularRigTreeElement
///////////////////////////////////////////////////////////
FModularRigTreeElement::FModularRigTreeElement(const FString& InKey, TWeakPtr<SModularRigTreeView> InTreeView, bool bInIsPrimary)
{
	Key = InKey;
	bIsPrimary = bInIsPrimary;

	FString ModuleNameString;
	FString ConnectorNameString = Key;
	(void)FRigHierarchyModulePath(ConnectorNameString).Split(&ModuleNameString, &ConnectorNameString);
	if (bIsPrimary)
	{
		ModuleName = *Key;
		if(const UModularRig* ModularRig = InTreeView.Pin()->GetRigTreeDelegates().GetModularRig())
		{
			if (const FRigModuleInstance* Module = ModularRig->FindModule(ModuleName))
			{
				if (const UControlRig* Rig = Module->GetRig())
				{
					if (const FRigModuleConnector* PrimaryConnector = Rig->GetRigModuleSettings().FindPrimaryConnector())
					{
						ConnectorName = PrimaryConnector->Name;
					}
				}
			}
		}
	}
	else
	{
		ConnectorName = ConnectorNameString;
		ModuleName = *ModuleNameString;
	}
	
	ShortName = *ConnectorNameString;
	
	if(InTreeView.IsValid())
	{
		if(const UModularRig* ModularRig = InTreeView.Pin()->GetRigTreeDelegates().GetModularRig())
		{
			RefreshDisplaySettings(ModularRig);
		}
	}
}

void FModularRigTreeElement::RefreshDisplaySettings(const UModularRig* InModularRig)
{
	const TPair<const FSlateBrush*, FSlateColor> Result = GetBrushAndColor(InModularRig);

	IconBrush = Result.Key;
	IconColor = Result.Value;
	TextColor = FSlateColor::UseForeground();
}

TSharedRef<ITableRow> FModularRigTreeElement::MakeTreeRowWidget(const TSharedRef<STableViewBase>& InOwnerTable, TSharedRef<FModularRigTreeElement> InRigTreeElement, TSharedPtr<SModularRigTreeView> InTreeView, bool bPinned)
{
	return SNew(SModularRigModelItem, InOwnerTable, InRigTreeElement, InTreeView, bPinned);
}

void FModularRigTreeElement::RequestRename()
{
	OnRenameRequested.ExecuteIfBound();
}

//////////////////////////////////////////////////////////////
/// SModularRigModelItem
///////////////////////////////////////////////////////////
void SModularRigModelItem::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable, TSharedRef<FModularRigTreeElement> InRigTreeElement, TSharedPtr<SModularRigTreeView> InTreeView, bool bPinned)
{
	WeakRigTreeElement = InRigTreeElement;
	Delegates = InTreeView->GetRigTreeDelegates();

	if (InRigTreeElement->Key.IsEmpty())
	{
		SMultiColumnTableRow<TSharedPtr<FModularRigTreeElement>>::Construct(
			SMultiColumnTableRow<TSharedPtr<FModularRigTreeElement>>::FArguments()
			.ShowSelection(false)
			.OnCanAcceptDrop(Delegates.OnCanAcceptDrop)
			.OnAcceptDrop(Delegates.OnAcceptDrop)
			, OwnerTable);
		return;
	}

	const FString& ModuleName = InRigTreeElement->ModuleName.ToString();
	const FRigHierarchyModulePath ConnectorModulePath(ModuleName, InRigTreeElement->ConnectorName);
	ConnectorKey = FRigElementKey(ConnectorModulePath.GetPathFName(), ERigElementType::Connector);

	SMultiColumnTableRow<TSharedPtr<FModularRigTreeElement>>::Construct(
		SMultiColumnTableRow<TSharedPtr<FModularRigTreeElement>>::FArguments()
		.OnDragDetected(Delegates.OnDragDetected)
		.OnCanAcceptDrop(Delegates.OnCanAcceptDrop)
		.OnAcceptDrop(Delegates.OnAcceptDrop)
		.ShowWires(true), OwnerTable);
}

bool SModularRigModelItem::OnConnectorTargetChanged(TArray<FRigElementKey> InTargets, const FRigElementKey InConnectorKey)
{
	FScopedTransaction Transaction(LOCTEXT("ModuleHierarchyResolveConnector", "Resolve Connector"));
	Delegates.HandleResolveConnector(InConnectorKey, InTargets);
	return false;
}

void SModularRigModelItem::OnNameCommitted(const FText& InText, ETextCommit::Type InCommitType) const
{
	// for now only allow enter
	// because it is important to keep the unique names per pose
	if (InCommitType == ETextCommit::OnEnter)
	{
		FString NewName = InText.ToString();
		const FName OldModuleName = WeakRigTreeElement.Pin()->ModuleName;

		Delegates.HandleRenameElement(OldModuleName, *NewName);
	}
}

bool SModularRigModelItem::OnVerifyNameChanged(const FText& InText, FText& OutErrorMessage)
{
	const FName NewName = *InText.ToString();
	const FName OldModuleName = WeakRigTreeElement.Pin()->ModuleName;
	return Delegates.HandleVerifyElementNameChanged(OldModuleName, NewName, OutErrorMessage);
}

TSharedRef<SWidget> SModularRigModelItem::GenerateWidgetForColumn(const FName& ColumnName)
{
	if(ColumnName == SModularRigTreeView::Column_Module)
	{
		static constexpr float TopPadding = 2.f;
		
		TSharedPtr< SInlineEditableTextBlock > InlineWidget;
		TSharedRef<SWidget> Widget = SNew(SHorizontalBox)

		+SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(6, TopPadding, 0, 0)
		.VAlign(VAlign_Fill)
		[
			SNew(SExpanderArrow, SharedThis(this))
			.IndentAmount(12)
			.ShouldDrawWires(true)
		]

		+SHorizontalBox::Slot()
		.MaxWidth(25)
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.Padding(FMargin(0.f, TopPadding, 3.f, 0.f))
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.MaxHeight(25)
			[
				SNew(SImage)
				.Image_Lambda([this]() -> const FSlateBrush*
				{
					if(WeakRigTreeElement.IsValid())
					{
						return WeakRigTreeElement.Pin()->IconBrush;
					}
					return nullptr;
				})
				.ColorAndOpacity_Lambda([this]()
				{
					if(WeakRigTreeElement.IsValid())
					{
						return WeakRigTreeElement.Pin()->IconColor;
					}
					return FSlateColor::UseForeground();
				})
				.DesiredSizeOverride(FVector2D(16, 16))
			]
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0, TopPadding, 0, 0)
		[
			SAssignNew(InlineWidget, SInlineEditableTextBlock)
			.Text(this, &SModularRigModelItem::GetName, true)
			.MaximumLength(NAME_SIZE-1)
			.OnVerifyTextChanged(this, &SModularRigModelItem::OnVerifyNameChanged)
			.OnTextCommitted(this, &SModularRigModelItem::OnNameCommitted)
			.ToolTipText(this, &SModularRigModelItem::GetItemTooltip)
			.MultiLine(false)
			//.Font(IDetailLayoutBuilder::GetDetailFont())
			.ColorAndOpacity_Lambda([this]()
			{
				if(WeakRigTreeElement.IsValid())
				{
					return WeakRigTreeElement.Pin()->TextColor;
				}
				return FSlateColor::UseForeground();
			})
		];

		if(WeakRigTreeElement.IsValid())
		{
			WeakRigTreeElement.Pin()->OnRenameRequested.BindSP(InlineWidget.Get(), &SInlineEditableTextBlock::EnterEditingMode);
		}

		return Widget;
	}
	if (ColumnName == SModularRigTreeView::Column_Tags)
	{
		TSharedRef<SWidget> Widget = SNew(SImage)
		.Visibility_Lambda([this]() -> EVisibility
		{
			if (WeakRigTreeElement.IsValid())
			{
				if (!WeakRigTreeElement.Pin()->bIsPrimary)
				{
					return EVisibility::Hidden;
				}
			
				if (const UModularRig* ModularRig = Delegates.GetModularRig())
				{
					if (const FRigModuleInstance* Module = ModularRig->FindModule(WeakRigTreeElement.Pin()->ModuleName))
					{
						if (const UControlRigBlueprint* ModuleBlueprint = Cast<UControlRigBlueprint>(Module->GetRig()->GetClass()->ClassGeneratedBy))
						{
							for (const FRigVMTag& Tag : ModuleBlueprint->GetAssetVariant().Tags)
							{
								if (Tag.bMarksSubjectAsInvalid)
								{
									return EVisibility::Visible;
								}
							}
						}
					}
				}
			}
			return EVisibility::Hidden;
		})
		.ToolTipText_Lambda([this]() -> FText
		{
			TArray<FString> ToolTip;
			if (WeakRigTreeElement.IsValid())
			{
				if (const UModularRig* ModularRig = Delegates.GetModularRig())
				{
					if (const FRigModuleInstance* Module = ModularRig->FindModule(WeakRigTreeElement.Pin()->ModuleName))
					{
						if (const UControlRigBlueprint* ModuleBlueprint = Cast<UControlRigBlueprint>(Module->GetRig()->GetClass()->ClassGeneratedBy))
						{
							for (const FRigVMTag& Tag : ModuleBlueprint->GetAssetVariant().Tags)
							{
								if (Tag.bMarksSubjectAsInvalid)
								{
									ToolTip.Add(FString::Printf(TEXT("%s: %s"), *Tag.Label, *Tag.ToolTip.ToString()));
									ToolTip.Add(TEXT("Right click on the module to swap it to a newer variant.")); 
								}
							}
						}
					}
				}
			}

			return FText::FromString(FString::Join(ToolTip, TEXT("\n")));
		})
		.Image_Lambda([this]() -> const FSlateBrush*
		{
			const FSlateBrush* WarningBrush = FAppStyle::Get().GetBrush("Icons.WarningWithColor");
			return WarningBrush;
		})
		.DesiredSizeOverride(FVector2D(16, 16));

		return Widget;
	}
	if(ColumnName == SModularRigTreeView::Column_Connector)
	{
		bool bIsArrayConnector = false;
		if (const UModularRig* ModularRig = Delegates.GetModularRig())
		{
			if (URigHierarchy* Hierarchy = ModularRig->GetHierarchy())
			{
				if (const FRigConnectorElement* ConnectorElement = Cast<FRigConnectorElement>(Hierarchy->Find(ConnectorKey)))
				{
					bIsArrayConnector = ConnectorElement->IsArrayConnector();
				}
			}
		}

		TSharedPtr<SVerticalBox> ComboButtonBox;
		
		FRigTreeDelegates RigTreeDelegates;
		RigTreeDelegates.OnGetHierarchy = FOnGetRigTreeHierarchy::CreateLambda([this]()
		{
			return Delegates.GetModularRig()->GetHierarchy();
		});

		return SNew(SRigConnectorTargetWidget)
			.Outer(const_cast<UModularRig*>(Delegates.GetModularRig()))
			.ConnectorKey(ConnectorKey)
			.IsArray(bIsArrayConnector)
			.Targets(GetTargetKeys())
			.OnSetTargetArray(FRigConnectorTargetWidget_SetTargetArray::CreateSP(this, &SModularRigModelItem::OnConnectorTargetChanged, ConnectorKey))
			.RigTreeDelegates(RigTreeDelegates);
	}
	if(ColumnName == SModularRigTreeView::Column_Buttons)
	{
		return SNew(SHorizontalBox)

		// Reset button
		+SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0.f, 0.f, 0.f, 0.f)
		[
			SAssignNew(ResetConnectorButton, SButton)
			.ButtonStyle( FAppStyle::Get(), "NoBorder" )
			.ButtonColorAndOpacity_Lambda([this]()
			{
				return ResetConnectorButton.IsValid() && ResetConnectorButton->IsHovered()
					? FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.8))
					: FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.4));
			})
			.OnClicked_Lambda([this]()
			{
				Delegates.HandleDisconnectConnector(ConnectorKey);
				return FReply::Handled();
			})
			.ContentPadding(1.f)
			.ToolTipText(NSLOCTEXT("ControlRigModuleDetails", "Reset_Connector", "Reset Connector"))
			[
				SNew(SImage)
				.ColorAndOpacity_Lambda( [this]()
				{
					return ResetConnectorButton.IsValid() && ResetConnectorButton->IsHovered()
					? FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.8))
					: FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.4));
				})
				.Image(FSlateIcon(FAppStyle::Get().GetStyleSetName(), "PropertyWindow.DiffersFromDefault").GetIcon())
			]
		]

		// Use button
		+SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0.f, 0.f, 0.f, 0.f)
		[
			SAssignNew(UseSelectedButton, SButton)
			.ButtonStyle( FAppStyle::Get(), "NoBorder" )
			.ButtonColorAndOpacity_Lambda([this]()
			{
				return UseSelectedButton.IsValid() && UseSelectedButton->IsHovered()
					? FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.8))
					: FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.4));
			})
			.OnClicked_Lambda([this]()
			{
				if (const UModularRig* ModularRig = Delegates.GetModularRig())
				{
					const TArray<FRigElementKey>& Selected = ModularRig->GetHierarchy()->GetSelectedKeys();
					if (Selected.Num() > 0)
					{
						Delegates.HandleResolveConnector(ConnectorKey, Selected);
					}
				}
				return FReply::Handled();
			})
			.ContentPadding(1.f)
			.ToolTipText(NSLOCTEXT("ControlRigModuleDetails", "Use_Selected", "Use Selected"))
			[
				SNew(SImage)
				.ColorAndOpacity_Lambda( [this]()
				{
					return UseSelectedButton.IsValid() && UseSelectedButton->IsHovered()
					? FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.8))
					: FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.4));
				})
				.Image(FAppStyle::GetBrush("Icons.CircleArrowLeft"))
			]
		]
		
		// Select in hierarchy button
		+SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0.f, 0.f, 0.f, 0.f)
		[
			SAssignNew(SelectElementButton, SButton)
			.ButtonStyle( FAppStyle::Get(), "NoBorder" )
			.ButtonColorAndOpacity_Lambda([this]()
			{
				return SelectElementButton.IsValid() && SelectElementButton->IsHovered()
					? FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.8))
					: FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.4));
			})
			.OnClicked_Lambda([this]()
			{
				if (const UModularRig* ModularRig = Delegates.GetModularRig())
				{
					const FRigElementKeyRedirector& Redirector = ModularRig->GetElementKeyRedirector();
					if (const FRigElementKeyRedirector::FKeyArray* TargetKeys = Redirector.FindExternalKey(ConnectorKey))
					{
						bool bClearSelection = true;
						for(const FRigElementKey& TargetKey : *TargetKeys)
						{
							ModularRig->GetHierarchy()->GetController()->SelectElement(TargetKey, true, bClearSelection);
							bClearSelection = false;
						}
					}
				}
				return FReply::Handled();
			})
			.ContentPadding(1.f)
			.ToolTipText(NSLOCTEXT("ControlRigModuleDetails", "Select_Element", "Select Element"))
			[
				SNew(SImage)
				.ColorAndOpacity_Lambda( [this]()
				{
					return SelectElementButton.IsValid() && SelectElementButton->IsHovered()
					? FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.8))
					: FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.4));
				})
				.Image(FAppStyle::GetBrush("Icons.Search"))
			]
		];
	}
	return SNullWidget::NullWidget;
}

FText SModularRigModelItem::GetName(bool bUseShortName) const
{
	if(bUseShortName)
	{
		return (FText::FromName(WeakRigTreeElement.Pin()->ShortName));
	}
	return (FText::FromName(WeakRigTreeElement.Pin()->ModuleName));
}

FText SModularRigModelItem::GetItemTooltip() const
{
	const FText FullName = GetName(false);
	const FText ShortName = GetName(true);
	if(FullName.EqualTo(ShortName))
	{
		return FText();
	}
	return FullName;
}

TArray<FRigElementKey> SModularRigModelItem::GetTargetKeys() const
{
	TArray<FRigElementKey> Result;
	if (const UModularRig* ModularRig = Delegates.GetModularRig())
	{
		const FRigElementKeyRedirector ElementKeyRedirector = ModularRig->GetElementKeyRedirector();
		if(const FRigElementKeyRedirector::FCachedKeyArray* Cache = ElementKeyRedirector.Find(ConnectorKey))
		{
			Result.Reserve(Cache->Num());
			for(int32 Index = 0; Index < Cache->Num(); Index++)
			{
				Result.Add(Cache->operator[](Index).GetKey());
			}
		}
	}
	return Result;
}

//////////////////////////////////////////////////////////////
/// SModularRigTreeView
///////////////////////////////////////////////////////////

const FName SModularRigTreeView::Column_Module = TEXT("Module");
const FName SModularRigTreeView::Column_Tags = TEXT("Tags");
const FName SModularRigTreeView::Column_Connector = TEXT("Connector");
const FName SModularRigTreeView::Column_Buttons = TEXT("Actions");

void SModularRigTreeView::Construct(const FArguments& InArgs)
{
	Delegates = InArgs._RigTreeDelegates;
	bAutoScrollEnabled = InArgs._AutoScrollEnabled;

	FilterText = InArgs._FilterText;
	ShowSecondaryConnectors = InArgs._ShowSecondaryConnectors;
	ShowOptionalConnectors = InArgs._ShowOptionalConnectors;
	ShowUnresolvedConnectors = InArgs._ShowUnresolvedConnectors;

	STreeView<TSharedPtr<FModularRigTreeElement>>::FArguments SuperArgs;
	SuperArgs.HeaderRow(InArgs._HeaderRow);
	SuperArgs.TreeItemsSource(&RootElements);
	SuperArgs.SelectionMode(ESelectionMode::Multi);
	SuperArgs.OnGenerateRow(this, &SModularRigTreeView::MakeTableRowWidget, false);
	SuperArgs.OnGetChildren(this, &SModularRigTreeView::HandleGetChildrenForTree);
	SuperArgs.OnSelectionChanged(FOnModularRigTreeSelectionChanged::CreateRaw(&Delegates, &FModularRigTreeDelegates::HandleSelectionChanged));
	SuperArgs.OnContextMenuOpening(Delegates.OnContextMenuOpening);
	SuperArgs.HighlightParentNodesForSelection(true);
	SuperArgs.AllowInvisibleItemSelection(true);  //without this we deselect everything when we filter or we collapse
	SuperArgs.OnMouseButtonClick(Delegates.OnMouseButtonClick);
	SuperArgs.OnMouseButtonDoubleClick(Delegates.OnMouseButtonDoubleClick);
	
	SuperArgs.ShouldStackHierarchyHeaders_Lambda([]() -> bool {
		return UControlRigEditorSettings::Get()->bShowStackedHierarchy;
	});
	SuperArgs.OnGeneratePinnedRow(this, &SModularRigTreeView::MakeTableRowWidget, true);
	SuperArgs.MaxPinnedItems_Lambda([]() -> int32
	{
		return FMath::Max<int32>(1, UControlRigEditorSettings::Get()->MaxStackSize);
	});

	STreeView<TSharedPtr<FModularRigTreeElement>>::Construct(SuperArgs);

	LastMousePosition = FVector2D::ZeroVector;
	TimeAtMousePosition = 0.0;
}

void SModularRigTreeView::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	STreeView<TSharedPtr<FModularRigTreeElement, ESPMode::ThreadSafe>>::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	const FGeometry PaintGeometry = GetPaintSpaceGeometry();
	const FVector2D MousePosition = FSlateApplication::Get().GetCursorPos();

	if(PaintGeometry.IsUnderLocation(MousePosition))
	{
		const FVector2D WidgetPosition = PaintGeometry.AbsoluteToLocal(MousePosition);

		static constexpr float SteadyMousePositionTolerance = 5.f;

		if(LastMousePosition.Equals(MousePosition, SteadyMousePositionTolerance))
		{
			TimeAtMousePosition += InDeltaTime;
		}
		else
		{
			LastMousePosition = MousePosition;
			TimeAtMousePosition = 0.0;
		}

		static constexpr float AutoScrollStartDuration = 0.5f; // in seconds
		static constexpr float AutoScrollDistance = 24.f; // in pixels
		static constexpr float AutoScrollSpeed = 150.f;

		if(TimeAtMousePosition > AutoScrollStartDuration && FSlateApplication::Get().IsDragDropping())
		{
			if((WidgetPosition.Y < AutoScrollDistance) || (WidgetPosition.Y > PaintGeometry.Size.Y - AutoScrollDistance))
			{
				if(bAutoScrollEnabled)
				{
					const bool bScrollUp = (WidgetPosition.Y < AutoScrollDistance);

					const float DeltaInSlateUnits = (bScrollUp ? -InDeltaTime : InDeltaTime) * AutoScrollSpeed; 
					ScrollBy(GetCachedGeometry(), DeltaInSlateUnits, EAllowOverscroll::No);
				}
			}
			else
			{
				const TSharedPtr<FModularRigTreeElement>* Item = FindItemAtPosition(MousePosition);
				if(Item && Item->IsValid())
				{
					if(!IsItemExpanded(*Item))
					{
						SetItemExpansion(*Item, true);
					}
				}
			}
		}
	}

	if (bRequestRenameSelected)
	{
		RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateLambda([this](double, float) {
			TArray<TSharedPtr<FModularRigTreeElement>> SelectedItems = GetSelectedItems();
			if (SelectedItems.Num() == 1)
			{
				SelectedItems[0]->RequestRename();
			}
			return EActiveTimerReturnType::Stop;
		}));
		bRequestRenameSelected = false;
	}
}

TSharedPtr<FModularRigTreeElement> SModularRigTreeView::FindElement(const FString& InElementKey)
{
	for (TSharedPtr<FModularRigTreeElement> Root : RootElements)
	{
		if (TSharedPtr<FModularRigTreeElement> Found = FindElement(InElementKey, Root))
		{
			return Found;
		}
	}

	return TSharedPtr<FModularRigTreeElement>();
}

TSharedPtr<FModularRigTreeElement> SModularRigTreeView::FindElement(const FString& InElementKey, TSharedPtr<FModularRigTreeElement> CurrentItem)
{
	if (CurrentItem->Key == InElementKey)
	{
		return CurrentItem;
	}

	for (int32 ChildIndex = 0; ChildIndex < CurrentItem->Children.Num(); ++ChildIndex)
	{
		TSharedPtr<FModularRigTreeElement> Found = FindElement(InElementKey, CurrentItem->Children[ChildIndex]);
		if (Found.IsValid())
		{
			return Found;
		}
	}

	return TSharedPtr<FModularRigTreeElement>();
}

bool SModularRigTreeView::AddElement(FString InKey, FString InParentKey, bool bApplyFilterText)
{
	if(ElementMap.Contains(InKey))
	{
		return false;
	}

	if (!InKey.IsEmpty())
	{
		const FString ModulePath = InKey;

		bool bFilteredOutElement = false;
		const FString FilterTextString = FilterText.Get().ToString();
		if(!FilterTextString.IsEmpty())
		{
			FString StringToSearch = InKey;
			(void)FRigHierarchyModulePath(StringToSearch).Split(nullptr, &StringToSearch);
			
			if(!StringToSearch.Contains(FilterTextString, ESearchCase::IgnoreCase))
			{
				bFilteredOutElement = true;
			}
		}

		TArray<FRigHierarchyModulePath> FilteredConnectors;
		if (const UModularRig* ModularRig = Delegates.GetModularRig())
		{
			const FModularRigModel& Model = ModularRig->GetModularRigModel();
			
			if (const FRigModuleInstance* Module = ModularRig->FindModule(*ModulePath))
			{
				if (const UControlRig* ModuleRig = Module->GetRig())
				{
					const UControlRig* CDO = ModuleRig->GetClass()->GetDefaultObject<UControlRig>();
					const TArray<FRigModuleConnector>& Connectors = CDO->GetRigModuleSettings().ExposedConnectors;

					for (const FRigModuleConnector& Connector : Connectors)
					{
						if (Connector.IsPrimary())
						{
							continue;
						}

						const FRigHierarchyModulePath Key(ModulePath, Connector.Name);
						bool bShouldFilterByConnectorType = true;

						if(!FilterTextString.IsEmpty())
						{
							const bool bMatchesFilter = Connector.Name.Contains(FilterTextString, ESearchCase::IgnoreCase);
							if(bFilteredOutElement)
							{
								if(!bMatchesFilter)
								{
									continue;
								}
							}
							bShouldFilterByConnectorType = !bMatchesFilter;
						}

						if(bShouldFilterByConnectorType)
						{
							if(Delegates.ShouldAlwaysShowConnector(Key.GetPathFName()))
							{
								bShouldFilterByConnectorType = false;
							}
						}

						if(bShouldFilterByConnectorType)
						{
							const bool bIsConnected = Model.Connections.HasConnection(FRigElementKey(Key.GetPathFName(), ERigElementType::Connector));
							if(bIsConnected || !ShowUnresolvedConnectors.Get())
							{
								if(Connector.IsOptional())
								{
									if(ShowOptionalConnectors.Get() == false)
									{
										continue;
									}
								}
								else if(Connector.IsSecondary())
								{
									if(ShowSecondaryConnectors.Get() == false)
									{
										continue;
									}
								}
							}
						}

						FilteredConnectors.Add(Key);
					}
				}
			}
		}
		
		if(bFilteredOutElement && bApplyFilterText && FilteredConnectors.IsEmpty())
		{
			return false;
		}
		
		TSharedPtr<FModularRigTreeElement> NewItem = MakeShared<FModularRigTreeElement>(InKey, SharedThis(this), true);

		ElementMap.Add(InKey, NewItem);
		if (!InParentKey.IsEmpty())
		{
			ParentMap.Add(InKey, InParentKey);

			TSharedPtr<FModularRigTreeElement>* FoundItem = ElementMap.Find(InParentKey);
			check(FoundItem);
			FoundItem->Get()->Children.Add(NewItem);
		}
		else
		{
			RootElements.Add(NewItem);
		}

		SetItemExpansion(NewItem, true);

		for (const FString& Key : FilteredConnectors)
		{
			TSharedPtr<FModularRigTreeElement> ConnectorItem = MakeShared<FModularRigTreeElement>(Key, SharedThis(this), false);
			NewItem.Get()->Children.Add(ConnectorItem);
			ElementMap.Add(Key, ConnectorItem);
			ParentMap.Add(Key, InKey);
		}
	}

	return true;
}

bool SModularRigTreeView::AddElement(const FRigModuleInstance* InElement, bool bApplyFilterText)
{
	check(InElement);
	
	if (ElementMap.Contains(InElement->Name.ToString()))
	{
		return false;
	}

	const UModularRig* ModularRig = Delegates.GetModularRig();

	if(!AddElement(InElement->Name.ToString(), FString(), bApplyFilterText))
	{
		return false;
	}

	if (ElementMap.Contains(InElement->Name.ToString()))
	{
		if(ModularRig)
		{
			const FName ParentModuleName = ModularRig->GetParentModuleName(InElement->Name);
			if (!ParentModuleName.IsNone())
			{
				if(const FRigModuleInstance* ParentElement = ModularRig->FindModule(ParentModuleName))
				{
					AddElement(ParentElement, false);

					if(ElementMap.Contains(ParentModuleName.ToString()))
					{
						ReparentElement(InElement->Name.ToString(), ParentModuleName.ToString());
					}
				}
			}
		}
	}

	return true;
}

void SModularRigTreeView::AddSpacerElement()
{
	AddElement(FString(), FString());
}

bool SModularRigTreeView::ReparentElement(const FString InKey, const FString InParentKey)
{
	if (InKey.IsEmpty() || InKey == InParentKey)
	{
		return false;
	}

	TSharedPtr<FModularRigTreeElement>* FoundItem = ElementMap.Find(InKey);
	if (FoundItem == nullptr)
	{
		return false;
	}

	if (const FString* ExistingParentKey = ParentMap.Find(InKey))
	{
		if (*ExistingParentKey == InParentKey)
		{
			return false;
		}

		if (TSharedPtr<FModularRigTreeElement>* ExistingParent = ElementMap.Find(*ExistingParentKey))
		{
			(*ExistingParent)->Children.Remove(*FoundItem);
		}

		ParentMap.Remove(InKey);
	}
	else
	{
		if (InParentKey.IsEmpty())
		{
			return false;
		}

		RootElements.Remove(*FoundItem);
	}

	if (!InParentKey.IsEmpty())
	{
		ParentMap.Add(InKey, InParentKey);

		TSharedPtr<FModularRigTreeElement>* FoundParent = ElementMap.Find(InParentKey);
		if(FoundParent)
		{
			FoundParent->Get()->Children.Add(*FoundItem);
		}
	}
	else
	{
		RootElements.Add(*FoundItem);
	}

	return true;
}

void SModularRigTreeView::RefreshTreeView(bool bRebuildContent)
{
	TMap<FString, bool> ExpansionState;
	TArray<FName> Selection;

	if(bRebuildContent)
	{
		for (TPair<FString, TSharedPtr<FModularRigTreeElement>> Pair : ElementMap)
		{
			ExpansionState.FindOrAdd(Pair.Key) = IsItemExpanded(Pair.Value);
		}

		// internally save expansion states before rebuilding the tree, so the states can be restored later
		SaveAndClearSparseItemInfos();

		RootElements.Reset();
		ElementMap.Reset();
		ParentMap.Reset();

		Selection = GetSelectedModuleNames();
	}

	if(bRebuildContent)
	{
		const UModularRig* ModularRig = Delegates.GetModularRig();
		if(ModularRig)
		{
			ModularRig->ForEachModule([&](const FRigModuleInstance* Element)
			{
				AddElement(Element, true);
				return true;
			});

			// expand all elements upon the initial construction of the tree
			if (ExpansionState.Num() < ElementMap.Num())
			{
				for (const TPair<FString, TSharedPtr<FModularRigTreeElement>>& Element : ElementMap)
				{
					if (!ExpansionState.Contains(Element.Key))
					{
						SetItemExpansion(Element.Value, true);
					}
				}
			}

			for (const auto& Pair : ElementMap)
			{
				RestoreSparseItemInfos(Pair.Value);
			}

			if (RootElements.Num() > 0)
			{
				AddSpacerElement();
			}
		}
	}
	else
	{
		if (RootElements.Num()> 0)
		{
			// elements may be added at the end of the list after a spacer element
			// we need to remove the spacer element and re-add it at the end
			RootElements.RemoveAll([](TSharedPtr<FModularRigTreeElement> InElement)
			{
				return InElement.Get()->Key == FString();
			});
			AddSpacerElement();
		}
	}

	RequestTreeRefresh();
	{
		TGuardValue<bool> Guard(Delegates.bSuspendSelectionDelegate, true);
		ClearSelection();

		if(!Selection.IsEmpty())
		{
			TArray<TSharedPtr<FModularRigTreeElement>> SelectedElements;
			for(const FName& SelectedModuleName : Selection)
			{
				if(const TSharedPtr<FModularRigTreeElement> ElementToSelect = FindElement(SelectedModuleName.ToString()))
				{
					SelectedElements.Add(ElementToSelect);
				}
			}
			if(!SelectedElements.IsEmpty())
			{
				SetSelection(SelectedElements);
			}
		}
	}
}

TSharedRef<ITableRow> SModularRigTreeView::MakeTableRowWidget(TSharedPtr<FModularRigTreeElement> InItem,
	const TSharedRef<STableViewBase>& OwnerTable, bool bPinned)
{
	return InItem->MakeTreeRowWidget(OwnerTable, InItem.ToSharedRef(), SharedThis(this), bPinned);
}

void SModularRigTreeView::HandleGetChildrenForTree(TSharedPtr<FModularRigTreeElement> InItem,
	TArray<TSharedPtr<FModularRigTreeElement>>& OutChildren)
{
	OutChildren = InItem.Get()->Children;
}

TArray<FName> SModularRigTreeView::GetSelectedModuleNames() const
{
	TArray<FName> ModuleNames;
	TArray<TSharedPtr<FModularRigTreeElement>> SelectedElements = GetSelectedItems();
	for(const TSharedPtr<FModularRigTreeElement>& SelectedElement : SelectedElements)
	{
		if (SelectedElement.IsValid())
		{
			ModuleNames.AddUnique(SelectedElement->ModuleName);
		}
	}
	return ModuleNames;
}

void SModularRigTreeView::SetSelection(const TArray<TSharedPtr<FModularRigTreeElement>>& InSelection) 
{
	ClearSelection();
	SetItemSelection(InSelection, true, ESelectInfo::Direct);
}

const TSharedPtr<FModularRigTreeElement>* SModularRigTreeView::FindItemAtPosition(FVector2D InScreenSpacePosition) const
{
	if (ItemsPanel.IsValid() && SListView<TSharedPtr<FModularRigTreeElement>>::HasValidItemsSource())
	{
		FArrangedChildren ArrangedChildren(EVisibility::Visible);
		const int32 Index = FindChildUnderPosition(ArrangedChildren, InScreenSpacePosition);
		if (ArrangedChildren.IsValidIndex(Index))
		{
			TSharedRef<SModularRigModelItem> ItemWidget = StaticCastSharedRef<SModularRigModelItem>(ArrangedChildren[Index].Widget);
			if (ItemWidget->WeakRigTreeElement.IsValid())
			{
				const FString Key = ItemWidget->WeakRigTreeElement.Pin()->Key;
				const TSharedPtr<FModularRigTreeElement>* ResultPtr = SListView<TSharedPtr<FModularRigTreeElement>>::GetItems().FindByPredicate([Key](const TSharedPtr<FModularRigTreeElement>& Item) -> bool
					{
						return Item->Key == Key;
					});

				if (ResultPtr)
				{
					return ResultPtr;
				}
			}
		}
	}
	return nullptr;
}

TPair<const FSlateBrush*, FSlateColor> FModularRigTreeElement::GetBrushAndColor(const UModularRig* InModularRig)
{
	const FSlateBrush* Brush = nullptr;
	FLinearColor Color = FSlateColor(EStyleColor::Foreground).GetColor(FWidgetStyle());
	float Opacity = 1.f;

	if (const FRigModuleInstance* ConnectorModule = InModularRig->FindModule(ModuleName))
	{
		const FModularRigModel& Model = InModularRig->GetModularRigModel();
		const FRigHierarchyModulePath ConnectorPath(ModuleName.ToString(), ConnectorName);
		bool bIsConnected = Model.Connections.HasConnection(FRigElementKey(ConnectorPath.GetPathFName(), ERigElementType::Connector));
		bool bConnectionWarning = !bIsConnected;
		
		if (const UControlRig* ModuleRig = ConnectorModule->GetRig())
		{
			const FRigModuleConnector* Connector = ModuleRig->GetRigModuleSettings().ExposedConnectors.FindByPredicate([this](FRigModuleConnector& Connector)
			{
				return Connector.Name == ConnectorName;
			});
			if (Connector)
			{
				if (Connector->IsPrimary())
				{
					if (bIsConnected)
					{
						const FSoftObjectPath IconPath = ModuleRig->GetRigModuleSettings().Icon;
						const TSharedPtr<FSlateBrush>* ExistingBrush = IconPathToBrush.Find(IconPath);
						if(ExistingBrush && ExistingBrush->IsValid())
						{
							Brush = ExistingBrush->Get();
						}
						else
						{
							if(UTexture2D* Icon = Cast<UTexture2D>(IconPath.TryLoad()))
							{
								const TSharedPtr<FSlateBrush> NewBrush = MakeShareable(new FSlateBrush(UWidgetBlueprintLibrary::MakeBrushFromTexture(Icon, 16.0f, 16.0f)));
								IconPathToBrush.FindOrAdd(IconPath) = NewBrush;
								Brush = NewBrush.Get();
							}
						}
					}
					else
					{
						Brush = FControlRigEditorStyle::Get().GetBrush("ControlRig.ConnectorWarning");
					}
				}
				else if (Connector->Settings.bOptional)
				{
					bConnectionWarning = false;
					if (!bIsConnected)
					{
						Opacity = 0.7;
						Color = FSlateColor(EStyleColor::Hover2).GetColor(FWidgetStyle());
					}
					Brush = FControlRigEditorStyle::Get().GetBrush("ControlRig.ConnectorOptional");
				}
				else
				{
					Brush = FControlRigEditorStyle::Get().GetBrush("ControlRig.ConnectorSecondary");
				}
			}
		}

		if (bConnectionWarning)
		{
			Color = FSlateColor(EStyleColor::Warning).GetColor(FWidgetStyle());
		}
	}
	if (!Brush)
	{
		Brush = FControlRigEditorStyle::Get().GetBrush("ControlRig.Tree.RigidBody");
	}

	// Apply opacity
	Color = Color.CopyWithNewOpacity(Opacity);
	
	return TPair<const FSlateBrush*, FSlateColor>(Brush, Color);
}

//////////////////////////////////////////////////////////////
/// SSearchableModularRigTreeView
///////////////////////////////////////////////////////////

void SSearchableModularRigTreeView::Construct(const FArguments& InArgs)
{
	FModularRigTreeDelegates TreeDelegates = InArgs._RigTreeDelegates;
	
	ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		.VAlign(VAlign_Top)
		.HAlign(HAlign_Fill)
		.Padding(0.0f, 0.0f)
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot()
			[
				SNew(SBorder)
				.Padding(2.0f)
				.BorderImage(FAppStyle::GetBrush("SCSEditor.TreePanel"))
				[
					SAssignNew(TreeView, SModularRigTreeView)
					.RigTreeDelegates(TreeDelegates)
				]
			]
		]
	];
}


#undef LOCTEXT_NAMESPACE
