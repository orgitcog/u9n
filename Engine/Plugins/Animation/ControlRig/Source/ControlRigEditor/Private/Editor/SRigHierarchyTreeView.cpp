// Copyright Epic Games, Inc. All Rights Reserved.

#include "Editor/SRigHierarchyTreeView.h"
#include "Styling/AppStyle.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Text/STextBlock.h"
#include "PropertyCustomizationHelpers.h"
#include "Framework/Application/SlateApplication.h"
#include "Editor/EditorEngine.h"
#include "HelperUtil.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "ControlRig.h"
#include "ControlRigEditorStyle.h"
#include "Editor/SRigHierarchy.h"
#include "Settings/ControlRigSettings.h"
#include "Graph/ControlRigGraphSchema.h"
#include "Rigs/AdditiveControlRig.h"
#include "Rigs/RigHierarchyController.h"
#include "Styling/AppStyle.h"
#include "Algo/Sort.h"
#include "ControlRigBlueprint.h"

#define LOCTEXT_NAMESPACE "SRigHierarchyTreeView"

//////////////////////////////////////////////////////////////
/// FRigTreeDelegates
///////////////////////////////////////////////////////////
FRigTreeDisplaySettings FRigTreeDelegates::DefaultDisplaySettings;

//////////////////////////////////////////////////////////////
/// FRigTreeElement
///////////////////////////////////////////////////////////
FRigTreeElement::FRigTreeElement(const FRigHierarchyKey& InKey, TWeakPtr<SRigHierarchyTreeView> InTreeView, bool InSupportsRename, ERigTreeFilterResult InFilterResult)
{
	Key = InKey;
	ShortName = LongName = FText::FromName(InKey.GetFName());
	ChannelName = NAME_None;
	bIsTransient = false;
	bIsAnimationChannel = false;
	bIsProcedural = false;
	bSupportsRename = InSupportsRename;
	FilterResult = InFilterResult;
	bFadedOutDuringDragDrop = false;

	if(InTreeView.IsValid())
	{
		if(const URigHierarchy* Hierarchy = InTreeView.Pin()->GetRigTreeDelegates().GetHierarchy())
		{
			if(InKey.IsElement())
			{
				LongName = Hierarchy->GetDisplayNameForUI(InKey.GetElement(), EElementNameDisplayMode::ForceLong);
				ShortName = Hierarchy->GetDisplayNameForUI(InKey.GetElement(), EElementNameDisplayMode::ForceShort);
			}
			
			const FRigTreeDisplaySettings& Settings = InTreeView.Pin()->GetRigTreeDelegates().GetDisplaySettings();
			RefreshDisplaySettings(Hierarchy, Settings);
		}
	}
}


TSharedRef<ITableRow> FRigTreeElement::MakeTreeRowWidget(const TSharedRef<STableViewBase>& InOwnerTable, TSharedRef<FRigTreeElement> InRigTreeElement, TSharedPtr<SRigHierarchyTreeView> InTreeView, const FRigTreeDisplaySettings& InSettings, bool bPinned)
{
	return SNew(SRigHierarchyItem, InOwnerTable, InRigTreeElement, InTreeView, InSettings, bPinned);
}

void FRigTreeElement::RequestRename()
{
	if(bSupportsRename)
	{
		OnRenameRequested.ExecuteIfBound();
	}
}

void FRigTreeElement::RefreshDisplaySettings(const URigHierarchy* InHierarchy, const FRigTreeDisplaySettings& InSettings)
{
	const TPair<const FSlateBrush*, FSlateColor> Result = SRigHierarchyItem::GetBrushForElementType(InHierarchy, Key);

	bIsProcedural = false;
	if(Key.IsElement())
	{
		if(const FRigBaseElement* Element = InHierarchy->Find(Key.GetElement()))
		{
			bIsProcedural = Element->IsProcedural();
				
			if(const FRigControlElement* ControlElement = Cast<FRigControlElement>(Element))
			{
				bIsTransient = ControlElement->Settings.bIsTransientControl;
				bIsAnimationChannel = ControlElement->IsAnimationChannel();
				if(bIsAnimationChannel)
				{
					ChannelName = ControlElement->GetDisplayName();
				}
			}
		}
	}
	else
	{
		bIsProcedural = InHierarchy->IsProcedural(Key.GetComponent());
	}

	IconBrush = Result.Key;
	IconColor = Result.Value;
	if(IconColor.IsColorSpecified() && InSettings.bShowIconColors)
	{
		IconColor = FilterResult == ERigTreeFilterResult::Shown ? Result.Value : FSlateColor(Result.Value.GetSpecifiedColor() * 0.5f);
	}
	else
	{
		IconColor = FilterResult == ERigTreeFilterResult::Shown ? FSlateColor::UseForeground() : FSlateColor(FLinearColor::Gray * 0.5f);
	}

	TextColor = FilterResult == ERigTreeFilterResult::Shown ?
		(bIsProcedural ? FSlateColor(FLinearColor(0.9f, 0.8f, 0.4f)) : FSlateColor::UseForeground()) :
		(bIsProcedural ? FSlateColor(FLinearColor(0.9f, 0.8f, 0.4f) * 0.5f) : FSlateColor(FLinearColor::Gray * 0.5f));
}

FSlateColor FRigTreeElement::GetIconColor() const
{
	if(bFadedOutDuringDragDrop)
	{
		if(FSlateApplication::Get().IsDragDropping())
		{
			return IconColor.GetColor(FWidgetStyle()) * 0.3f;
		}
	}
	return IconColor;
}

FSlateColor FRigTreeElement::GetTextColor() const
{
	if(bFadedOutDuringDragDrop)
	{
		if(FSlateApplication::Get().IsDragDropping())
		{
			return TextColor.GetColor(FWidgetStyle()) * 0.3f;
		}
	}
	return TextColor;
}

//////////////////////////////////////////////////////////////
/// SRigHierarchyItem
///////////////////////////////////////////////////////////
void SRigHierarchyItem::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable, TSharedRef<FRigTreeElement> InRigTreeElement, TSharedPtr<SRigHierarchyTreeView> InTreeView, const FRigTreeDisplaySettings& InSettings, bool bPinned)
{
	WeakRigTreeElement = InRigTreeElement;
	Delegates = InTreeView->GetRigTreeDelegates();
	FRigTreeDisplaySettings DisplaySettings = Delegates.GetDisplaySettings();

	if (!InRigTreeElement->Key.IsValid())
	{
		STableRow<TSharedPtr<FRigTreeElement>>::Construct(
			STableRow<TSharedPtr<FRigTreeElement>>::FArguments()
			.ShowSelection(false)
			.OnCanAcceptDrop(Delegates.OnCanAcceptDrop)
			.OnAcceptDrop(Delegates.OnAcceptDrop)
			.Content()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.FillHeight(200.f)
				[
					SNew(SSpacer)
				]
			], OwnerTable);
		return;
	}

	TSharedPtr< SInlineEditableTextBlock > InlineWidget;
	TSharedPtr< SHorizontalBox > HorizontalBox;

	STableRow<TSharedPtr<FRigTreeElement>>::Construct(
		STableRow<TSharedPtr<FRigTreeElement>>::FArguments()
		.Padding(FMargin(0, 1, 1, 1))
		.OnDragDetected(Delegates.OnDragDetected)
		.OnCanAcceptDrop(Delegates.OnCanAcceptDrop)
		.OnAcceptDrop(Delegates.OnAcceptDrop)
		.ShowWires(true)
		.Content()
		[
			SAssignNew(HorizontalBox, SHorizontalBox)
			+ SHorizontalBox::Slot()
			.MaxWidth(18)
			.AutoWidth()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Padding(FMargin(0.f, 0.f, 3.f, 0.f))
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
						return WeakRigTreeElement.Pin()->GetIconColor();
					}
					return FSlateColor::UseForeground();
				})
				.DesiredSizeOverride(FVector2D(16, 16))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SAssignNew(InlineWidget, SInlineEditableTextBlock)
				.Text(this, &SRigHierarchyItem::GetNameForUI)
				.ToolTipText(this, &SRigHierarchyItem::GetItemTooltip)
				.OnVerifyTextChanged(this, &SRigHierarchyItem::OnVerifyNameChanged)
				.OnTextCommitted(this, &SRigHierarchyItem::OnNameCommitted)
				.MultiLine(false)
				.ColorAndOpacity_Lambda([this]()
				{
					if(WeakRigTreeElement.IsValid())
					{
						return WeakRigTreeElement.Pin()->GetTextColor();
					}
					return FSlateColor::UseForeground();
				})
			]
		], OwnerTable);

	if(!InRigTreeElement->Tags.IsEmpty())
	{
		HorizontalBox->AddSlot()
		.FillWidth(1.f)
		[
			SNew(SSpacer)
		];
		
		for(const SRigHierarchyTagWidget::FArguments& TagArguments : InRigTreeElement->Tags)
		{
			TSharedRef<SRigHierarchyTagWidget> TagWidget = SArgumentNew(TagArguments, SRigHierarchyTagWidget);
			TagWidget->OnElementKeyDragDetected().BindSP(InTreeView.Get(), &SRigHierarchyTreeView::OnElementKeyTagDragDetected);
			
			HorizontalBox->AddSlot()
			.AutoWidth()
			[
				TagWidget
			];
		}
	}

	InRigTreeElement->OnRenameRequested.BindSP(InlineWidget.Get(), &SInlineEditableTextBlock::EnterEditingMode);
}

FText SRigHierarchyItem::GetNameForUI() const
{
	return GetName(Delegates.GetDisplaySettings().NameDisplayMode);
}

FText SRigHierarchyItem::GetName(EElementNameDisplayMode InNameDisplayMode) const
{
	if(WeakRigTreeElement.Pin()->bIsTransient)
	{
		static const FText TemporaryControl = FText::FromString(TEXT("Temporary Control"));
		return TemporaryControl;
	}
	if(WeakRigTreeElement.Pin()->bIsAnimationChannel)
	{
		return FText::FromName(WeakRigTreeElement.Pin()->ChannelName);
	}

	if(InNameDisplayMode == EElementNameDisplayMode::AssetDefault)
	{
		if(const URigHierarchy* Hierarchy = Delegates.GetHierarchy())
		{
			if(const UControlRig* ControlRig = Cast<UControlRig>(Hierarchy->GetOuter()))
			{
				InNameDisplayMode = ControlRig->HierarchySettings.ElementNameDisplayMode;
			}
		}
	}

	if(InNameDisplayMode == EElementNameDisplayMode::Auto)
	{
		if(const URigHierarchy* Hierarchy = Delegates.GetHierarchy())
		{
			const FRigElementKey& Key = WeakRigTreeElement.Pin()->Key.GetElement();
			InNameDisplayMode = Hierarchy->HasUniqueShortName(Key.Type, Key.Name) ? EElementNameDisplayMode::ForceShort : EElementNameDisplayMode::ForceLong;
		}
	}

	if(InNameDisplayMode == EElementNameDisplayMode::ForceShort)
	{
		return WeakRigTreeElement.Pin()->ShortName;
	}
	
	return WeakRigTreeElement.Pin()->LongName;
}

FText SRigHierarchyItem::GetItemTooltip() const
{
	if(Delegates.OnRigTreeGetItemToolTip.IsBound())
	{
		const TOptional<FText> ToolTip = Delegates.OnRigTreeGetItemToolTip.Execute(WeakRigTreeElement.Pin()->Key);
		if(ToolTip.IsSet())
		{
			return ToolTip.GetValue();
		}
	}
	const FText FullName = GetName(EElementNameDisplayMode::ForceLong);
	const FText ShortName = GetName(EElementNameDisplayMode::ForceShort);
	if(FullName.EqualTo(ShortName))
	{
		return FText();
	}
	return FullName;
}

//////////////////////////////////////////////////////////////
/// SRigHierarchyTreeView
///////////////////////////////////////////////////////////

void SRigHierarchyTreeView::Construct(const FArguments& InArgs)
{
	Delegates = InArgs._RigTreeDelegates;
	bAutoScrollEnabled = InArgs._AutoScrollEnabled;

	STreeView<TSharedPtr<FRigTreeElement>>::FArguments SuperArgs;
	SuperArgs.TreeItemsSource(&RootElements);
	SuperArgs.SelectionMode(ESelectionMode::Multi);
	SuperArgs.OnGenerateRow(this, &SRigHierarchyTreeView::MakeTableRowWidget, false);
	SuperArgs.OnGetChildren(this, &SRigHierarchyTreeView::HandleGetChildrenForTree);
	SuperArgs.OnSelectionChanged(FOnRigTreeSelectionChanged::CreateRaw(&Delegates, &FRigTreeDelegates::HandleSelectionChanged));
	SuperArgs.OnContextMenuOpening(Delegates.OnContextMenuOpening);
	SuperArgs.OnMouseButtonClick(Delegates.OnMouseButtonClick);
	SuperArgs.OnMouseButtonDoubleClick(Delegates.OnMouseButtonDoubleClick);
	SuperArgs.OnSetExpansionRecursive(Delegates.OnSetExpansionRecursive);
	SuperArgs.HighlightParentNodesForSelection(true);
	SuperArgs.AllowInvisibleItemSelection(true);  //without this we deselect everything when we filter or we collapse
	
	SuperArgs.ShouldStackHierarchyHeaders_Lambda([]() -> bool {
		return UControlRigEditorSettings::Get()->bShowStackedHierarchy;
	});
	SuperArgs.OnGeneratePinnedRow(this, &SRigHierarchyTreeView::MakeTableRowWidget, true);
	SuperArgs.MaxPinnedItems_Lambda([]() -> int32
	{
		return FMath::Max<int32>(1, UControlRigEditorSettings::Get()->MaxStackSize);
	});

	STreeView<TSharedPtr<FRigTreeElement>>::Construct(SuperArgs);

	LastMousePosition = FVector2D::ZeroVector;
	TimeAtMousePosition = 0.0;

	if(InArgs._PopulateOnConstruct)
	{
		RefreshTreeView(true);
	}
}

void SRigHierarchyTreeView::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	STreeView<TSharedPtr<FRigTreeElement, ESPMode::ThreadSafe>>::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

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
				const TSharedPtr<FRigTreeElement>* Item = FindItemAtPosition(MousePosition);
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
}

TSharedPtr<FRigTreeElement> SRigHierarchyTreeView::FindElement(const FRigHierarchyKey& InElementKey) const
{
	for (int32 RootIndex = 0; RootIndex < RootElements.Num(); ++RootIndex)
	{
		TSharedPtr<FRigTreeElement> Found = FindElement(InElementKey, RootElements[RootIndex]);
		if (Found.IsValid())
		{
			return Found;
		}
	}
	return TSharedPtr<FRigTreeElement>();
}

TSharedPtr<FRigTreeElement> SRigHierarchyTreeView::FindElement(const FRigHierarchyKey& InElementKey, TSharedPtr<FRigTreeElement> CurrentItem)
{
	if (CurrentItem->Key == InElementKey)
	{
		return CurrentItem;
	}

	for (int32 ChildIndex = 0; ChildIndex < CurrentItem->Children.Num(); ++ChildIndex)
	{
		TSharedPtr<FRigTreeElement> Found = FindElement(InElementKey, CurrentItem->Children[ChildIndex]);
		if (Found.IsValid())
		{
			return Found;
		}
	}

	return TSharedPtr<FRigTreeElement>();
}

bool SRigHierarchyTreeView::AddElement(FRigHierarchyKey InKey, FRigHierarchyKey InParentKey)
{
	if(ElementMap.Contains(InKey))
	{
		return false;
	}

	// skip transient controls
	if(InKey.IsElement())
	{
		if(const URigHierarchy* Hierarchy = Delegates.GetHierarchy())
		{
			if(const FRigControlElement* ControlElement = Hierarchy->Find<FRigControlElement>(InKey.GetElement()))
			{
				if(ControlElement->Settings.bIsTransientControl)
				{
					return false;
				}
			}
		}
	}

	const FRigTreeDisplaySettings& Settings = Delegates.GetDisplaySettings();
	const bool bSupportsRename = Delegates.OnRenameElement.IsBound();

	const FString FilteredString = Settings.FilterText.ToString();
	bool bAnyFilteredOut = Delegates.OnRigTreeIsItemVisible.IsBound();
	if (!bAnyFilteredOut)
	{
		bAnyFilteredOut = !FilteredString.IsEmpty() && InKey.IsValid();
	}

	if (!bAnyFilteredOut)
	{
		TSharedPtr<FRigTreeElement> NewItem = MakeShared<FRigTreeElement>(InKey, SharedThis(this), bSupportsRename, ERigTreeFilterResult::Shown);

		if (InKey.IsValid())
		{
			ElementMap.Add(InKey, NewItem);
			if (InParentKey.IsValid())
			{
				ParentMap.Add(InKey, InParentKey);
			}

			if (InParentKey.IsValid())
			{
				TSharedPtr<FRigTreeElement>* FoundItem = ElementMap.Find(InParentKey);
				check(FoundItem);
				FoundItem->Get()->Children.Add(NewItem);
			}
			else
			{
				RootElements.Add(NewItem);
			}
		}
		else
		{
			RootElements.Add(NewItem);
		}
	}
	else
	{
		bool bIsFilteredOut = false;
		if (Delegates.OnRigTreeIsItemVisible.IsBound())
		{
			bIsFilteredOut = !Delegates.OnRigTreeIsItemVisible.Execute(InKey);
		}

		const URigHierarchy* Hierarchy = Delegates.GetHierarchy();
		auto GetFirstParent = [Hierarchy](const FRigHierarchyKey& InKey) -> FRigHierarchyKey
		{
			if(InKey.IsElement())
			{
				return Hierarchy->GetFirstParent(InKey.GetElement());;
			}
			if(InKey.IsComponent())
			{
				return InKey.GetComponent().ElementKey;
			}
			return FRigHierarchyKey();
		};
		
		FString FilteredStringUnderScores = FilteredString.Replace(TEXT(" "), TEXT("_"));
		if (!bIsFilteredOut && (InKey.GetName().Contains(FilteredString) || InKey.GetName().Contains(FilteredStringUnderScores)))
		{
			TSharedPtr<FRigTreeElement> NewItem = MakeShared<FRigTreeElement>(InKey, SharedThis(this), bSupportsRename, ERigTreeFilterResult::Shown);
			ElementMap.Add(InKey, NewItem);
			RootElements.Add(NewItem);

			if (!Settings.bFlattenHierarchyOnFilter && !Settings.bHideParentsOnFilter)
			{
				if(Hierarchy)
				{
					TSharedPtr<FRigTreeElement> ChildItem = NewItem;
					FRigHierarchyKey ParentKey = GetFirstParent(InKey);
					while (ParentKey.IsValid())
					{
						if (!ElementMap.Contains(ParentKey))
						{
							TSharedPtr<FRigTreeElement> ParentItem = MakeShared<FRigTreeElement>(ParentKey, SharedThis(this), bSupportsRename, ERigTreeFilterResult::ShownDescendant);							
							ElementMap.Add(ParentKey, ParentItem);
							RootElements.Add(ParentItem);

							ReparentElement(ChildItem->Key, ParentKey);

							ChildItem = ParentItem;
							ParentKey = GetFirstParent(ParentKey);
						}
						else
						{
							ReparentElement(ChildItem->Key, ParentKey);
							break;
						}						
					}
				}
			}
		}
	}

	return true;
}

bool SRigHierarchyTreeView::AddElement(const FRigBaseElement* InElement)
{
	check(InElement);
	
	if (ElementMap.Contains(InElement->GetKey()))
	{
		return false;
	}

	const FRigTreeDisplaySettings& Settings = Delegates.GetDisplaySettings();
	const URigHierarchy* Hierarchy = Delegates.GetHierarchy();

	switch(InElement->GetType())
	{
		case ERigElementType::Bone:
		{
			if(!Settings.bShowBones)
			{
				return false;
			}

			const FRigBoneElement* BoneElement = CastChecked<FRigBoneElement>(InElement);
			if (!Settings.bShowImportedBones && BoneElement->BoneType == ERigBoneType::Imported)
			{
				return false;
			}
			break;
		}
		case ERigElementType::Null:
		{
			if(!Settings.bShowNulls)
			{
				return false;
			}
			break;
		}
		case ERigElementType::Control:
		{
			if(!Settings.bShowControls)
			{
				return false;
			}
			break;
		}
		case ERigElementType::Physics:
		{
			return false;
		}
		case ERigElementType::Reference:
		{
			if(!Settings.bShowReferences)
			{
				return false;
			}
			break;
		}
		case ERigElementType::Curve:
		{
			return false;
		}
		case ERigElementType::Connector:
		{
			if(Hierarchy)
			{
				// add the connector as a tag rather than its own element in the tree
				if(UControlRig* ControlRig = Hierarchy->GetTypedOuter<UControlRig>())
				{
					FRigElementKeyRedirector& Redirector = ControlRig->GetElementKeyRedirector();
					if(const FRigElementKeyRedirector::FCachedKeyArray* Cache = Redirector.Find(InElement->GetKey()))
					{
						bool bValid = true;
						TArray<TSharedPtr<FRigTreeElement>> TargetElementPtrs;
						for(const FCachedRigElement& CachedRigElement : *Cache)
						{
							if(!const_cast<FCachedRigElement*>(&CachedRigElement)->UpdateCache(Hierarchy))
							{
								bValid = false;
							}
							if(const TSharedPtr<FRigTreeElement>* TargetElementPtr = ElementMap.Find(CachedRigElement.GetKey()))
							{
								if(TargetElementPtr->IsValid())
								{
									TargetElementPtrs.Add(*TargetElementPtr);
								}
							}
						}
						
						if(bValid && !TargetElementPtrs.IsEmpty())
						{
							const FRigElementKey ConnectorKey = InElement->GetKey();

							SRigHierarchyTagWidget::FArguments TagArguments;

							static const FLinearColor BackgroundColor = FColor::FromHex(TEXT("#26BBFF"));
							static const FLinearColor TextColor = FColor::FromHex(TEXT("#0F0F0F"));
							static const FLinearColor IconColor = FColor::FromHex(TEXT("#1A1A1A"));

							static const FSlateBrush* PrimaryBrush = FControlRigEditorStyle::Get().GetBrush("ControlRig.ConnectorPrimary");
							static const FSlateBrush* SecondaryBrush = FControlRigEditorStyle::Get().GetBrush("ControlRig.ConnectorSecondary");
							static const FSlateBrush* OptionalBrush = FControlRigEditorStyle::Get().GetBrush("ControlRig.ConnectorOptional");

							const FSlateBrush* IconBrush = PrimaryBrush;
							if(const FRigConnectorElement* ConnectorElement = Cast<FRigConnectorElement>(InElement))
							{
								if(ConnectorElement->Settings.Type == EConnectorType::Secondary)
								{
									IconBrush = ConnectorElement->Settings.bOptional ? OptionalBrush : SecondaryBrush;
								}
							}

							const EElementNameDisplayMode NameDisplayMode = GetRigTreeDelegates().GetDisplaySettings().NameDisplayMode;
							const FText Label = Hierarchy->GetDisplayNameForUI(ConnectorKey, NameDisplayMode);
							TagArguments.Text(Label);
							TagArguments.TooltipText(FText::FromName(ConnectorKey.Name));
							TagArguments.Color(BackgroundColor);
							TagArguments.IconColor(IconColor);
							TagArguments.TextColor(TextColor);
							TagArguments.Icon(IconBrush);
							TagArguments.IconSize(FVector2d(16.f, 16.f));
							TagArguments.AllowDragDrop(true);
							FString Identifier;
							FRigElementKey::StaticStruct()->ExportText(Identifier, &ConnectorKey, nullptr, nullptr, PPF_None, nullptr);
							TagArguments.Identifier(Identifier);

							TagArguments.OnClicked_Lambda([ConnectorKey, this]()
							{
								Delegates.RequestDetailsInspection(ConnectorKey);
							});

							if (!ControlRig->IsModularRig())
							{
								TagArguments.OnRenamed_Lambda([ConnectorKey, this](const FText& InNewName, ETextCommit::Type InCommitType)
								{
									(void)Delegates.HandleRenameElement(ConnectorKey, InNewName.ToString());
								});
								TagArguments.OnVerifyRename_Lambda([ConnectorKey, this](const FText& InText, FText& OutError)
								{
									return Delegates.HandleVerifyElementNameChanged(ConnectorKey, InText.ToString(), OutError);
								});
							}

							for(TSharedPtr<FRigTreeElement> TargetElementPtr : TargetElementPtrs)
							{
								TargetElementPtr->Tags.Add(TagArguments);
							}
							return true;
						}
					}
				}
			}
			break;
		}
		case ERigElementType::Socket:
		{
			if(!Settings.bShowSockets)
			{
				return false;
			}
			break;
		}
		default:
		{
			break;
		}
	}

	if(!AddElement(InElement->GetKey(), FRigHierarchyKey()))
	{
		return false;
	}

	if (ElementMap.Contains(InElement->GetKey()))
	{
		if(Hierarchy)
		{
			if(InElement->GetType() == ERigElementType::Connector)
			{
				AddConnectorResolveWarningTag(ElementMap.FindChecked(InElement->GetKey()), InElement, Hierarchy);
			}
			
			FRigElementKey ParentKey = Hierarchy->GetFirstParent(InElement->GetKey());
			if(InElement->GetType() == ERigElementType::Connector)
			{
				ParentKey = Delegates.GetResolvedKey(InElement->GetKey()).GetElement();
				if(ParentKey == InElement->GetKey())
				{
					ParentKey.Reset();
				}
			}

			TArray<FRigElementWeight> ParentWeights = Hierarchy->GetParentWeightArray(InElement->GetKey());
			if(ParentWeights.Num() > 0)
			{
				TArray<FRigElementKey> ParentKeys = Hierarchy->GetParents(InElement->GetKey());
				check(ParentKeys.Num() == ParentWeights.Num());
				for(int32 ParentIndex=0;ParentIndex<ParentKeys.Num();ParentIndex++)
				{
					if(ParentWeights[ParentIndex].IsAlmostZero())
					{
						continue;
					}
					ParentKey = ParentKeys[ParentIndex];
					break;
				}
			}

			if (ParentKey.IsValid())
			{
				if(const FRigBaseElement* ParentElement = Hierarchy->Find(ParentKey))
				{
					AddElement(ParentElement);

					if(ElementMap.Contains(ParentKey))
					{
						ReparentElement(InElement->GetKey(), ParentKey);
					}
				}
			}
		}
	}

	for(int32 ComponentIndex = 0; ComponentIndex < InElement->NumComponents(); ComponentIndex++)
	{
		AddComponent(InElement->GetComponent(ComponentIndex));
	}

	return true;
}

bool SRigHierarchyTreeView::AddComponent(const FRigBaseComponent* InComponent)
{
	check(InComponent);
	
	if (ElementMap.Contains(InComponent->GetKey()))
	{
		return false;
	}

	const FRigTreeDisplaySettings& Settings = Delegates.GetDisplaySettings();
	if(!Settings.bShowComponents)
	{
		return false;
	}

	if(!AddElement(InComponent->GetKey(), InComponent->GetElementKey()))
	{
		return false;
	}

	return true;
}

void SRigHierarchyTreeView::AddSpacerElement()
{
	AddElement(FRigElementKey(), FRigHierarchyKey());
}

bool SRigHierarchyTreeView::ReparentElement(FRigHierarchyKey InKey, FRigHierarchyKey InParentKey)
{
	if (!InKey.IsValid() || InKey == InParentKey)
	{
		return false;
	}

	if(InKey.IsElement())
	{
		if(InKey.GetElement().Type == ERigElementType::Connector)
		{
			return false;
		}
	}

	const FRigTreeDisplaySettings& Settings = Delegates.GetDisplaySettings();

	TSharedPtr<FRigTreeElement>* FoundItem = ElementMap.Find(InKey);
	if (FoundItem == nullptr)
	{
		return false;
	}

	if (!Settings.FilterText.IsEmpty() && Settings.bFlattenHierarchyOnFilter)
	{
		return false;
	}

	if (const FRigHierarchyKey* ExistingParentKey = ParentMap.Find(InKey))
	{
		if (*ExistingParentKey == InParentKey)
		{
			return false;
		}

		if (TSharedPtr<FRigTreeElement>* ExistingParent = ElementMap.Find(*ExistingParentKey))
		{
			(*ExistingParent)->Children.Remove(*FoundItem);
		}

		ParentMap.Remove(InKey);
	}
	else
	{
		if (!InParentKey.IsValid())
		{
			return false;
		}

		RootElements.Remove(*FoundItem);
	}

	if (InParentKey.IsValid())
	{
		ParentMap.Add(InKey, InParentKey);

		TSharedPtr<FRigTreeElement>* FoundParent = ElementMap.Find(InParentKey);
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

bool SRigHierarchyTreeView::RemoveElement(FRigHierarchyKey InKey)
{
	TSharedPtr<FRigTreeElement>* FoundItem = ElementMap.Find(InKey);
	if (FoundItem == nullptr)
	{
		return false;
	}

	ReparentElement(InKey, FRigHierarchyKey());

	RootElements.Remove(*FoundItem);
	return ElementMap.Remove(InKey) > 0;
}

void SRigHierarchyTreeView::RefreshTreeView(bool bRebuildContent)
{
		TMap<FRigHierarchyKey, bool> ExpansionState;

	if(bRebuildContent)
	{
		for (TPair<FRigHierarchyKey, TSharedPtr<FRigTreeElement>> Pair : ElementMap)
		{
			ExpansionState.FindOrAdd(Pair.Key) = IsItemExpanded(Pair.Value);
		}

		// internally save expansion states before rebuilding the tree, so the states can be restored later
		SaveAndClearSparseItemInfos();

		RootElements.Reset();
		ElementMap.Reset();
		ParentMap.Reset();
	}

	if(bRebuildContent)
	{
		const URigHierarchy* Hierarchy = Delegates.GetHierarchy();
		if(Hierarchy)
		{
			TArray<const FRigSocketElement*> Sockets;
			TArray<const FRigConnectorElement*> Connectors;
			TArray<const FRigBaseElement*> EverythingElse;
			TMap<const FRigBaseElement*, int32> ElementDepth;
			Sockets.Reserve(Hierarchy->Num(ERigElementType::Socket));
			Connectors.Reserve(Hierarchy->Num(ERigElementType::Connector));
			EverythingElse.Reserve(Hierarchy->Num() - Hierarchy->Num(ERigElementType::Socket) - Hierarchy->Num(ERigElementType::Connector));
			
			Hierarchy->Traverse([&](FRigBaseElement* Element, bool& bContinue)
			{
				int32& Depth = ElementDepth.Add(Element, 0);
				if(const FRigBaseElement* ParentElement = Hierarchy->GetFirstParent(Element))
				{
					if (int32* ParentDepth = ElementDepth.Find(ParentElement))
					{
						Depth = *ParentDepth + 1;
					}
				}
				
				if(const FRigSocketElement* Socket = Cast<FRigSocketElement>(Element))
				{
					Sockets.Add(Socket);
				}
				else if(const FRigConnectorElement* Connector = Cast<FRigConnectorElement>(Element))
				{
					Connectors.Add(Connector);
				}
				else
				{
					EverythingElse.Add(Element);
				}
				bContinue = true;
			});

			// first add the root components
			for(int32 Index = 0; Index < Hierarchy->NumTopLevelComponents(); Index++)
			{
				AddComponent(Hierarchy->GetTopLevelComponent(Index));
			}

			// sort the sockets by depth
			Algo::SortBy(Sockets, [ElementDepth](const FRigSocketElement* Socket) -> int32
			{
				return ElementDepth.FindChecked(Socket);
			});
			for(const FRigSocketElement* Socket : Sockets)
			{
				AddElement(Socket);
			}

			// add everything but connectors and sockets
			for(const FRigBaseElement* Element : EverythingElse)
			{
				AddElement(Element);
			}

			// add all of the connectors. their parent relationship in the tree represents resolve
			for(const FRigConnectorElement* Connector : Connectors)
			{
				AddElement(Connector);
			}

			// expand all elements upon the initial construction of the tree
			if (ExpansionState.Num() == 0)
			{
				for (TSharedPtr<FRigTreeElement> RootElement : RootElements)
				{
					SetExpansionRecursive(RootElement, false, true);
				}
			}
			else if (ExpansionState.Num() < ElementMap.Num())
			{
				for (const TPair<FRigHierarchyKey, TSharedPtr<FRigTreeElement>>& Element : ElementMap)
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

			if(Delegates.OnCompareKeys.IsBound())
			{
				Algo::Sort(RootElements, [&](const TSharedPtr<FRigTreeElement>& A, const TSharedPtr<FRigTreeElement>& B)
				{
					return Delegates.OnCompareKeys.Execute(A->Key, B->Key);
				});
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
			RootElements.RemoveAll([](TSharedPtr<FRigTreeElement> InElement)
			{
				return InElement.Get()->Key == FRigElementKey();
			});
			AddSpacerElement();
		}
	}

	RequestTreeRefresh();
	{
		ClearSelection();

		TArray<FRigHierarchyKey> Selection = Delegates.GetSelection();
		for (const FRigHierarchyKey& Key : Selection)
		{
			for (int32 RootIndex = 0; RootIndex < RootElements.Num(); ++RootIndex)
			{
				TSharedPtr<FRigTreeElement> Found = FindElement(Key, RootElements[RootIndex]);
				if (Found.IsValid())
				{
					SetItemSelection(Found, true, ESelectInfo::OnNavigation);
				}
			}
		}
	}
}

void SRigHierarchyTreeView::SetExpansionRecursive(TSharedPtr<FRigTreeElement> InElement, bool bTowardsParent,
	bool bShouldBeExpanded)
{
	SetItemExpansion(InElement, bShouldBeExpanded);

	if (bTowardsParent)
	{
		if (const FRigHierarchyKey* ParentKey = ParentMap.Find(InElement->Key))
		{
			if (TSharedPtr<FRigTreeElement>* ParentItem = ElementMap.Find(*ParentKey))
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

TSharedRef<ITableRow> SRigHierarchyTreeView::MakeTableRowWidget(TSharedPtr<FRigTreeElement> InItem,
	const TSharedRef<STableViewBase>& OwnerTable, bool bPinned)
{
	const FRigTreeDisplaySettings& Settings = Delegates.GetDisplaySettings();
	return InItem->MakeTreeRowWidget(OwnerTable, InItem.ToSharedRef(), SharedThis(this), Settings, bPinned);
}

void SRigHierarchyTreeView::HandleGetChildrenForTree(TSharedPtr<FRigTreeElement> InItem,
	TArray<TSharedPtr<FRigTreeElement>>& OutChildren)
{
	OutChildren = InItem.Get()->Children;
}

void SRigHierarchyTreeView::OnElementKeyTagDragDetected(const FRigElementKey& InDraggedTag)
{
	(void)Delegates.OnRigTreeElementKeyTagDragDetected.ExecuteIfBound(InDraggedTag);
}

TArray<FRigHierarchyKey> SRigHierarchyTreeView::GetSelectedKeys() const
{
	TArray<FRigHierarchyKey> Keys;
	TArray<TSharedPtr<FRigTreeElement>> SelectedElements = GetSelectedItems();
	for(const TSharedPtr<FRigTreeElement>& SelectedElement : SelectedElements)
	{
		Keys.Add(SelectedElement->Key);
	}
	return Keys;
}

const TSharedPtr<FRigTreeElement>* SRigHierarchyTreeView::FindItemAtPosition(FVector2D InScreenSpacePosition) const
{
	if (ItemsPanel.IsValid() && SListView<TSharedPtr<FRigTreeElement>>::HasValidItemsSource())
	{
		FArrangedChildren ArrangedChildren(EVisibility::Visible);
		const int32 Index = FindChildUnderPosition(ArrangedChildren, InScreenSpacePosition);
		if (ArrangedChildren.IsValidIndex(Index))
		{
			TSharedRef<SRigHierarchyItem> ItemWidget = StaticCastSharedRef<SRigHierarchyItem>(ArrangedChildren[Index].Widget);
			if (ItemWidget->WeakRigTreeElement.IsValid())
			{
				const FRigHierarchyKey Key = ItemWidget->WeakRigTreeElement.Pin()->Key;
				const TSharedPtr<FRigTreeElement>* ResultPtr = SListView<TSharedPtr<FRigTreeElement>>::GetItems().FindByPredicate([Key](const TSharedPtr<FRigTreeElement>& Item) -> bool
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

void SRigHierarchyTreeView::AddConnectorResolveWarningTag(TSharedPtr<FRigTreeElement> InTreeElement,
	const FRigBaseElement* InRigElement, const URigHierarchy* InHierarchy)
{
	check(InTreeElement.IsValid());
	check(InRigElement);
	check(InRigElement->GetType() == ERigElementType::Connector);

	if(const FRigConnectorElement* ConnectorElement = Cast<FRigConnectorElement>(InRigElement))
	{
		if(ConnectorElement->IsOptional())
		{
			return;
		}
	}

	if(UControlRig* ControlRig = InHierarchy->GetTypedOuter<UControlRig>())
	{
		TWeakObjectPtr<UControlRig> ControlRigPtr(ControlRig);
		const FRigElementKey ConnectorKey = InRigElement->GetKey();
		
		TAttribute<FText> GetTooltipText = TAttribute<FText>::CreateSP(this,
			&SRigHierarchyTreeView::GetConnectorWarningMessage, InTreeElement, ControlRigPtr, ConnectorKey);

		static const FLinearColor BackgroundColor = FColor::FromHex(TEXT("#FFB800"));
		static const FLinearColor TextColor = FColor::FromHex(TEXT("#0F0F0F"));
		static const FLinearColor IconColor = FColor::FromHex(TEXT("#1A1A1A"));
		static const FSlateBrush* WarningBrush = FControlRigEditorStyle::Get().GetBrush("ControlRig.ConnectorWarning");

		SRigHierarchyTagWidget::FArguments TagArguments;
		TagArguments.Visibility_Lambda([GetTooltipText]() -> EVisibility
		{
			return GetTooltipText.Get().IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible;
		});
		TagArguments.Text(LOCTEXT("ConnectorWarningTagLabel", "Warning"));
		TagArguments.ToolTipText(GetTooltipText);
		TagArguments.Color(BackgroundColor);
		TagArguments.IconColor(IconColor);
		TagArguments.TextColor(TextColor);
		TagArguments.Icon(WarningBrush);
		TagArguments.IconSize(FVector2d(16.f, 16.f));
		InTreeElement->Tags.Add(TagArguments);
	}
}

FText SRigHierarchyTreeView::GetConnectorWarningMessage(TSharedPtr<FRigTreeElement> InTreeElement,
	TWeakObjectPtr<UControlRig> InControlRigPtr, const FRigElementKey InConnectorKey) const
{
	if(UControlRig* ControlRig = InControlRigPtr.Get())
	{
		if(const UControlRigBlueprint* ControlRigBlueprint = Cast<UControlRigBlueprint>(ControlRig->GetClass()->ClassGeneratedBy))
		{
			const FRigElementKey TargetKey = ControlRigBlueprint->ModularRigModel.Connections.FindTargetFromConnector(InConnectorKey);
			if(TargetKey.IsValid())
			{
				const URigHierarchy* Hierarchy = ControlRig->GetHierarchy();
				if(Hierarchy->Contains(TargetKey))
				{
					return FText();
				}
			}
		}
	}

	static const FText NotResolvedWarning = LOCTEXT("ConnectorWarningConnectorNotResolved", "Connector is not resolved.");
	return NotResolvedWarning;
}

bool SRigHierarchyItem::OnVerifyNameChanged(const FText& InText, FText& OutErrorMessage)
{
	const FString NewName = InText.ToString();
	const FRigHierarchyKey OldKey = WeakRigTreeElement.Pin()->Key;
	return Delegates.HandleVerifyElementNameChanged(OldKey, NewName, OutErrorMessage);
}

TPair<const FSlateBrush*, FSlateColor> SRigHierarchyItem::GetBrushForElementType(const URigHierarchy* InHierarchy, const FRigHierarchyKey& InKey)
{
	static const FSlateBrush* ProxyControlBrush = FControlRigEditorStyle::Get().GetBrush("ControlRig.Tree.ProxyControl"); 
	static const FSlateBrush* ControlBrush = FControlRigEditorStyle::Get().GetBrush("ControlRig.Tree.Control");
	static const FSlateBrush* NullBrush = FControlRigEditorStyle::Get().GetBrush("ControlRig.Tree.Null");
	static const FSlateBrush* BoneImportedBrush = FControlRigEditorStyle::Get().GetBrush("ControlRig.Tree.BoneImported");
	static const FSlateBrush* BoneUserBrush = FControlRigEditorStyle::Get().GetBrush("ControlRig.Tree.BoneUser");
	static const FSlateBrush* PhysicsBrush = FControlRigEditorStyle::Get().GetBrush("ControlRig.Tree.RigidBody");
	static const FSlateBrush* SocketOpenBrush = FControlRigEditorStyle::Get().GetBrush("ControlRig.Tree.Socket_Open");
	static const FSlateBrush* SocketClosedBrush = FControlRigEditorStyle::Get().GetBrush("ControlRig.Tree.Socket_Closed");
	static const FSlateBrush* PrimaryConnectorBrush = FControlRigEditorStyle::Get().GetBrush("ControlRig.ConnectorPrimary");
	static const FSlateBrush* SecondaryConnectorBrush = FControlRigEditorStyle::Get().GetBrush("ControlRig.ConnectorSecondary");
	static const FSlateBrush* OptionalConnectorBrush = FControlRigEditorStyle::Get().GetBrush("ControlRig.ConnectorOptional");

	const FSlateBrush* Brush = nullptr;
	FSlateColor Color = FSlateColor::UseForeground();

	if(InKey.IsElement())
	{
		switch (InKey.GetElement().Type)
		{
			case ERigElementType::Control:
			{
				if(const FRigControlElement* Control = InHierarchy->Find<FRigControlElement>(InKey.GetElement()))
				{
					FLinearColor ShapeColor = FLinearColor::White;
					
					if(Control->Settings.SupportsShape())
					{
						if(Control->Settings.AnimationType == ERigControlAnimationType::ProxyControl)
						{
							Brush = ProxyControlBrush;
						}
						else
						{
							Brush = ControlBrush;
						}
						ShapeColor = Control->Settings.ShapeColor;
					}
					else
					{
						static const FLazyName TypeIcon(TEXT("Kismet.VariableList.TypeIcon"));
						Brush = FAppStyle::GetBrush(TypeIcon);
						ShapeColor = GetColorForControlType(Control->Settings.ControlType, Control->Settings.ControlEnum);
					}
					
					// ensure the alpha is always visible
					ShapeColor.A = 1.f;
					Color = FSlateColor(ShapeColor);
				}
				else
				{
					Brush = ControlBrush;
				}
				break;
			}
			case ERigElementType::Null:
			{
				Brush = NullBrush;
				break;
			}
			case ERigElementType::Bone:
			{
				ERigBoneType BoneType = ERigBoneType::User;

				if(InHierarchy)
				{
					const FRigBoneElement* BoneElement = InHierarchy->Find<FRigBoneElement>(InKey.GetElement());
					if(BoneElement)
					{
						BoneType = BoneElement->BoneType;
					}
				}

				switch (BoneType)
				{
					case ERigBoneType::Imported:
					{
						Brush = BoneImportedBrush;
						break;
					}
					case ERigBoneType::User:
					default:
					{
						Brush = BoneUserBrush;
						break;
					}
				}

				break;
			}
			case ERigElementType::Physics:
			{
				Brush = PhysicsBrush;
				break;
			}
			case ERigElementType::Reference:
			case ERigElementType::Socket:
			{
				Brush = SocketOpenBrush;

				if(UControlRig* ControlRig = Cast<UControlRig>(InHierarchy->GetOuter()))
				{
					if(const FRigElementKey* ConnectorKey = ControlRig->GetElementKeyRedirector().FindReverse(InKey.GetElement()))
					{
						if(ConnectorKey->Type == ERigElementType::Connector)
						{
							Brush = SocketClosedBrush;
						}
					}
				}

				if(const FRigSocketElement* Socket = InHierarchy->Find<FRigSocketElement>(InKey.GetElement()))
				{
					Color = Socket->GetColor(InHierarchy);
				}
				break;
			}
			case ERigElementType::Connector:
			{
				Brush = PrimaryConnectorBrush;
				if(const FRigConnectorElement* Connector = InHierarchy->Find<FRigConnectorElement>(InKey.GetElement()))
				{
					if(!Connector->IsPrimary())
					{
						Brush = Connector->IsOptional() ? OptionalConnectorBrush : SecondaryConnectorBrush;
					}
				}
				break;
			}
			default:
			{
				break;
			}
		}
	}
	else if(InKey.IsComponent())
	{
		if(const FRigBaseComponent* Component = InHierarchy->FindComponent(InKey.GetComponent()))
		{
			Brush = Component->GetIconForUI().GetIcon();
		}
	}

	return TPair<const FSlateBrush*, FSlateColor>(Brush, Color);
}

FLinearColor SRigHierarchyItem::GetColorForControlType(ERigControlType InControlType, UEnum* InControlEnum)
{
	FEdGraphPinType PinType;
	switch(InControlType)
	{
		case ERigControlType::Bool:
		{
			PinType = RigVMTypeUtils::PinTypeFromCPPType(RigVMTypeUtils::BoolTypeName, nullptr);
			break;
		}
		case ERigControlType::Float:
		case ERigControlType::ScaleFloat:
		{
			PinType = RigVMTypeUtils::PinTypeFromCPPType(RigVMTypeUtils::FloatTypeName, nullptr);
			break;
		}
		case ERigControlType::Integer:
		{
			if(InControlEnum)
			{
				PinType = RigVMTypeUtils::PinTypeFromCPPType(NAME_None, InControlEnum);
			}
			else
			{
				PinType = RigVMTypeUtils::PinTypeFromCPPType(RigVMTypeUtils::Int32TypeName, nullptr);
			}
			break;
		}
		case ERigControlType::Vector2D:
		{
			UScriptStruct* Struct = TBaseStructure<FVector2D>::Get(); 
			PinType = RigVMTypeUtils::PinTypeFromCPPType(*RigVMTypeUtils::GetUniqueStructTypeName(Struct), Struct);
			break;
		}
		case ERigControlType::Position:
		case ERigControlType::Scale:
		{
			UScriptStruct* Struct = TBaseStructure<FVector>::Get(); 
			PinType = RigVMTypeUtils::PinTypeFromCPPType(*RigVMTypeUtils::GetUniqueStructTypeName(Struct), Struct);
			break;
		}
		case ERigControlType::Rotator:
		{
			UScriptStruct* Struct = TBaseStructure<FRotator>::Get(); 
			PinType = RigVMTypeUtils::PinTypeFromCPPType(*RigVMTypeUtils::GetUniqueStructTypeName(Struct), Struct);
			break;
		}
		case ERigControlType::Transform:
		case ERigControlType::TransformNoScale:
		case ERigControlType::EulerTransform:
		default:
		{
			UScriptStruct* Struct = TBaseStructure<FTransform>::Get(); 
			PinType = RigVMTypeUtils::PinTypeFromCPPType(*RigVMTypeUtils::GetUniqueStructTypeName(Struct), Struct);
			break;
		}
	}
	const UControlRigGraphSchema* Schema = GetDefault<UControlRigGraphSchema>();
	return Schema->GetPinTypeColor(PinType);
}

void SRigHierarchyItem::OnNameCommitted(const FText& InText, ETextCommit::Type InCommitType) const
{
	// for now only allow enter
	// because it is important to keep the unique names per pose
	if (InCommitType == ETextCommit::OnEnter)
	{
		FString NewName = InText.ToString();
		const FRigHierarchyKey OldKey = WeakRigTreeElement.Pin()->Key;

		const FName NewSanitizedName = Delegates.HandleRenameElement(OldKey, NewName);
		if (NewSanitizedName.IsNone())
		{
			return;
		}
		NewName = NewSanitizedName.ToString();

		if (WeakRigTreeElement.IsValid())
		{
			WeakRigTreeElement.Pin()->Key.SetName(*NewName);
		}
	}
}

//////////////////////////////////////////////////////////////
/// SSearchableRigHierarchyTreeView
///////////////////////////////////////////////////////////

void SSearchableRigHierarchyTreeView::Construct(const FArguments& InArgs)
{
	FRigTreeDelegates TreeDelegates = InArgs._RigTreeDelegates;
	SuperGetRigTreeDisplaySettings = TreeDelegates.OnGetDisplaySettings;

	MaxHeight = InArgs._MaxHeight;

	TreeDelegates.OnGetDisplaySettings.BindSP(this, &SSearchableRigHierarchyTreeView::GetDisplaySettings);

	TSharedPtr<SVerticalBox> VerticalBox;
	ChildSlot
	[
		SAssignNew(VerticalBox, SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		.VAlign(VAlign_Top)
		.HAlign(HAlign_Fill)
		.Padding(0.0f)
		[
			SAssignNew(SearchBox, SSearchBox)
			.InitialText(InArgs._InitialFilterText)
			.OnTextChanged(this, &SSearchableRigHierarchyTreeView::OnFilterTextChanged)
		]

		+SVerticalBox::Slot()
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
					SAssignNew(TreeView, SRigHierarchyTreeView)
					.RigTreeDelegates(TreeDelegates)
				]
			]
		]
	];

	if (MaxHeight > SMALL_NUMBER)
	{
		VerticalBox->GetSlot(1).SetMaxHeight(MaxHeight);
	}
	else
	{
		VerticalBox->GetSlot(1).SetAutoHeight();
	}
}

const FRigTreeDisplaySettings& SSearchableRigHierarchyTreeView::GetDisplaySettings()
{
	if(SuperGetRigTreeDisplaySettings.IsBound())
	{
		Settings = SuperGetRigTreeDisplaySettings.Execute();
	}
	Settings.FilterText = FilterText;
	return Settings;
}

void SSearchableRigHierarchyTreeView::OnFilterTextChanged(const FText& SearchText)
{
	FilterText = SearchText;
	GetTreeView()->RefreshTreeView();
}


#undef LOCTEXT_NAMESPACE
