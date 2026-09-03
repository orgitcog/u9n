// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SDataHierarchyEditor.h"

#include "DataHierarchyEditorCommands.h"
#include "UObject/Package.h"
#include "Framework/Commands/GenericCommands.h"
#include "Styling/StyleColors.h"
#include "Styling/AppStyle.h"
#include "DataHierarchyViewModelBase.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/Text/STextBlock.h"
#include "ScopedTransaction.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Framework/Application/MenuStack.h"
#include "Framework/Application/SlateApplication.h"
#include "ToolMenus.h"
#include "DataHierarchyEditorStyle.h"
#include "Widgets/Input/SSearchBox.h"

#define LOCTEXT_NAMESPACE "DataHierarchyEditor"

TSharedRef<SWidget> SummonContextMenu(TArray<TSharedPtr<FHierarchyElementViewModel>> MenuHierarchyElements)
{
	UHierarchyMenuContext* MenuContextObject = NewObject<UHierarchyMenuContext>();
	MenuContextObject->MenuHierarchyElements = MenuHierarchyElements;
	TWeakObjectPtr<UDataHierarchyViewModelBase> ViewModel = MenuHierarchyElements[0]->GetHierarchyViewModel();
	MenuContextObject->HierarchyViewModel = ViewModel;

	FToolMenuContext MenuContext(MenuContextObject);
	MenuContext.AppendCommandList(ViewModel->GetCommands());
	
	TSharedRef<SWidget> MenuWidget = UToolMenus::Get()->GenerateWidget(ViewModel->GetContextMenuName(), MenuContext);
	return MenuWidget;
}

void SHierarchyCategory::Construct(const FArguments& InArgs,	TSharedPtr<FHierarchyCategoryViewModel> InCategoryViewModel)
{
	CategoryViewModelWeak = InCategoryViewModel;
	
	InCategoryViewModel->GetOnRequestRename().BindSP(this, &SHierarchyCategory::EnterEditingMode);

	const UHierarchyCategory* CategoryData = InCategoryViewModel->GetData<UHierarchyCategory>();
	SetToolTipText(TAttribute<FText>::CreateLambda([CategoryData]
	{
		return CategoryData->GetTooltip();
	}));
	
	ChildSlot
	[
		SAssignNew(InlineEditableTextBlock, SInlineEditableTextBlock)
		.Style(&FDataHierarchyEditorStyle::Get().GetWidgetStyle<FInlineEditableTextBlockStyle>("HierarchyEditor.CategoryTextStyle"))
		.Text(this, &SHierarchyCategory::GetCategoryText)
		.OnTextCommitted(this, &SHierarchyCategory::OnRenameCategory)
		.OnVerifyTextChanged(this, &SHierarchyCategory::OnVerifyCategoryRename)
		.IsSelected(InArgs._IsSelected)
	];
}

void SHierarchyCategory::EnterEditingMode() const
{
	if(CategoryViewModelWeak.Pin()->CanRename())
	{
		InlineEditableTextBlock->EnterEditingMode();
	}
}

bool SHierarchyCategory::OnVerifyCategoryRename(const FText& NewName, FText& OutTooltip) const
{
	TArray<TSharedPtr<FHierarchyCategoryViewModel>> SiblingCategoryViewModels;
	CategoryViewModelWeak.Pin()->GetParent().Pin()->GetChildrenViewModelsForType<UHierarchyCategory, FHierarchyCategoryViewModel>(SiblingCategoryViewModels);

	if(GetCategoryText().ToString() != NewName.ToString())
	{
		TSet<FString> CategoryNames;
		for(const auto& SiblingCategoryViewModel : SiblingCategoryViewModels)
		{
			CategoryNames.Add(Cast<UHierarchyCategory>(SiblingCategoryViewModel->GetDataMutable())->GetCategoryName().ToString());
		}

		if(CategoryNames.Contains(NewName.ToString()))
		{
			OutTooltip = LOCTEXT("HierarchyCategoryCantRename_DuplicateOnLayer", "Another category of the same name already exists!");
			return false;
		}
	}

	return true;
}

FText SHierarchyCategory::GetCategoryText() const
{
	return FText::FromString(CategoryViewModelWeak.Pin()->ToString());
}

void SHierarchyCategory::OnRenameCategory(const FText& NewText, ETextCommit::Type) const
{
	if(CategoryViewModelWeak.IsValid() && CategoryViewModelWeak.Pin()->GetCategoryName().EqualTo(NewText) == false)
	{
		FScopedTransaction Transaction(LOCTEXT("Transaction_Rename_Category", "Renamed hierarchy category"));
		CategoryViewModelWeak.Pin()->GetHierarchyViewModel()->GetHierarchyRoot()->Modify();
		
		CategoryViewModelWeak.Pin()->Rename(FName(NewText.ToString()));
	}
}

void SHierarchySection::Construct(const FArguments& InArgs, TSharedPtr<FHierarchySectionViewModel> InSection)
{
	SectionViewModelWeak = InSection;
	HierarchyViewModel = InSection->GetHierarchyViewModel();

	IsSectionActive = InArgs._IsSectionActive;
	OnSectionActivatedDelegate = InArgs._OnSectionActivated;
	
	InSection->GetOnRequestRename().BindSP(this, &SHierarchySection::TryEnterEditingMode);

	SetToolTipText(TAttribute<FText>::CreateSP(this, &SHierarchySection::GetTooltipText));

	ChildSlot
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		[
			SAssignNew(CheckBox, SCheckBox)
			.Visibility(EVisibility::HitTestInvisible)
			.Style(FAppStyle::Get(), "DetailsView.SectionButton")
			.OnCheckStateChanged(this, &SHierarchySection::OnSectionCheckChanged)
			.IsChecked(this, &SHierarchySection::GetSectionCheckState)
			.Padding(FMargin(8.f, 4.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.f)
				[
					SNew(SImage).Image(InSection->GetSectionImage() != nullptr ? InSection->GetSectionImage() : FStyleDefaults::GetNoBrush())
				]
				+ SHorizontalBox::Slot()
				[
					SAssignNew(InlineEditableTextBlock, SInlineEditableTextBlock)
					.Visibility(EVisibility::HitTestInvisible)
					.Text(this, &SHierarchySection::GetText)
					.OnTextCommitted(this, &SHierarchySection::OnRenameSection)
					.OnVerifyTextChanged(this, &SHierarchySection::OnVerifySectionRename)
					.IsSelected(this, &SHierarchySection::IsSectionSelected)
					.IsReadOnly(this, &SHierarchySection::IsSectionReadOnly)
				]
			]
		]
	];		
}

SHierarchySection::~SHierarchySection()
{
	SectionViewModelWeak.Reset();
}

void SHierarchySection::TryEnterEditingMode() const
{
	if(SectionViewModelWeak.IsValid() && SectionViewModelWeak.Pin()->CanRename())
	{
		InlineEditableTextBlock->EnterEditingMode();
	}
}

TSharedPtr<FHierarchySectionViewModel> SHierarchySection::GetSectionViewModel()
{
	return SectionViewModelWeak.Pin();
}

int32 SHierarchySection::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	LayerId =  SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	LayerId = PaintDropIndicator(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	return LayerId;
}

int32 SHierarchySection::PaintDropIndicator(const FPaintArgs& Args, const FGeometry& Geometry, FSlateRect SlateRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& WidgetStyle, bool bParentEnabled) const
{
	if(CurrentItemDropZone.IsSet())
	{
		return OnPaintDropIndicator(CurrentItemDropZone.GetValue(), Args, Geometry, SlateRect, OutDrawElements, LayerId, WidgetStyle, bParentEnabled);
	}

	return LayerId;
}

int32 SHierarchySection::OnPaintDropIndicator(EItemDropZone InItemDropZone, const FPaintArgs& Args, const FGeometry& Geometry, FSlateRect SlateRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& WidgetStyle, bool bParentEnabled) const
{
	const FSlateBrush* DropIndicatorBrush = GetDropIndicatorBrush(InItemDropZone);

	const FVector2f LocalSize(Geometry.GetLocalSize());
	const FVector2f Pivot(LocalSize * 0.5f);
	const FVector2f RotatedLocalSize(LocalSize.Y, LocalSize.X);
	FSlateLayoutTransform RotatedTransform(Pivot - RotatedLocalSize * 0.5f);	// Make the box centered to the alloted geometry, so that it can be rotated around the center.
	
	FSlateDrawElement::MakeRotatedBox(
		OutDrawElements,
		LayerId++,
		Geometry.ToPaintGeometry(RotatedLocalSize, RotatedTransform),
		DropIndicatorBrush,
		ESlateDrawEffect::None,
		-UE_HALF_PI,	// 90 deg CCW
		RotatedLocalSize * 0.5f,	// Relative center to the flipped
		FSlateDrawElement::RelativeToElement,
		DropIndicatorBrush->GetTint(WidgetStyle) * WidgetStyle.GetColorAndOpacityTint()
	);

	return LayerId;
}

bool SHierarchySection::OnCanAcceptDrop(TSharedPtr<FDragDropOperation> DragDropOperation, EItemDropZone InItemDropZone) const
{
	if(DragDropOperation->IsOfType<FHierarchyDragDropOp>() && SectionViewModelWeak.IsValid())
	{
		TSharedPtr<FHierarchyDragDropOp> HierarchyDragDropOp = StaticCastSharedPtr<FHierarchyDragDropOp>(DragDropOperation);
		TSharedPtr<FHierarchyElementViewModel> DraggedItem = HierarchyDragDropOp->GetDraggedElement().Pin();
		
		return SectionViewModelWeak.Pin()->CanDropOn(DraggedItem, InItemDropZone).bCanPerform;
	}

	return false;
}

FReply SHierarchySection::OnDroppedOn(const FGeometry&, const FDragDropEvent& DragDropEvent, EItemDropZone DropZone) const
{
	bDraggedOn = false;
	CurrentItemDropZone.Reset();

	if(TSharedPtr<FHierarchyDragDropOp> HierarchyDragDropOp = DragDropEvent.GetOperationAs<FHierarchyDragDropOp>())
	{
		TSharedPtr<FHierarchyElementViewModel> DraggedItem = HierarchyDragDropOp->GetDraggedElement().Pin();
		
		SectionViewModelWeak.Pin()->OnDroppedOn(DraggedItem, DropZone);
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FReply SHierarchySection::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	// we handle the event here so we can react on mouse button up
	if(MouseEvent.IsMouseButtonDown(EKeys::RightMouseButton))
	{
		return FReply::Handled();
	}
	else if(MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
	{			
		OnSectionActivatedDelegate.ExecuteIfBound(SectionViewModelWeak.Pin());
		return FReply::Handled().DetectDrag(AsShared(), EKeys::LeftMouseButton).SetUserFocus(AsShared());	
	}

	return FReply::Unhandled();
}

FReply SHierarchySection::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if(SectionViewModelWeak.IsValid() && SectionViewModelWeak.Pin()->IsForHierarchy() && SectionViewModelWeak.Pin()->GetData() != nullptr)
	{
		if(MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
		{			
			// Show menu to choose getter vs setter
			FSlateApplication::Get().PushMenu(
				AsShared(),
				FWidgetPath(),
				SummonContextMenu({GetSectionViewModel()}),
				FSlateApplication::Get().GetCursorPos(),
				FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu)
			);
				
			OnSectionActivatedDelegate.ExecuteIfBound(SectionViewModelWeak.Pin());			
			return FReply::Handled();
		}
	}

	return FReply::Unhandled();
}

FReply SHierarchySection::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	if(CurrentItemDropZone.IsSet())
	{
		if(OnCanAcceptDrop(DragDropEvent.GetOperation(), CurrentItemDropZone.GetValue()))
		{
			return OnDroppedOn(MyGeometry, DragDropEvent, CurrentItemDropZone.GetValue());
		}
	}

	return FReply::Unhandled();
}

void SHierarchySection::OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	bDraggedOn = true;
	if(DragDropEvent.GetOperationAs<FHierarchyDragDropOp>() && DragDropEvent.GetOperationAs<FSectionDragDropOp>() == nullptr)
	{
		RegisterActiveTimer(1.0f, FWidgetActiveTimerDelegate::CreateSP(this, &SHierarchySection::ActivateSectionIfDragging));
	}
}

FReply SHierarchySection::OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if(MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton) && SectionViewModelWeak.Pin()->CanDrag().bCanPerform)
	{
		TSharedRef<FSectionDragDropOp> SectionDragDropOp = MakeShared<FSectionDragDropOp>(SectionViewModelWeak.Pin());
		SectionDragDropOp->Construct();
		return FReply::Handled().BeginDragDrop(SectionDragDropOp);
	}
	
	return FReply::Unhandled();
}

void SHierarchySection::OnDragLeave(const FDragDropEvent& DragDropEvent)
{
	bDraggedOn = false;
	CurrentItemDropZone.Reset();
	
	if(TSharedPtr<FHierarchyDragDropOp> HierarchyDragDropOp = DragDropEvent.GetOperationAs<FHierarchyDragDropOp>())
	{
		HierarchyDragDropOp->SetDescription(FText::GetEmpty());
	}
}

FReply SHierarchySection::OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	const FVector2f LocalPointerPos = MyGeometry.AbsoluteToLocal(DragDropEvent.GetScreenSpacePosition());
	const EItemDropZone ItemHoverZone = ZoneFromPointerPosition(LocalPointerPos, MyGeometry.GetLocalSize());

	if(TSharedPtr<FHierarchyDragDropOp> HierarchyDragDropOp = DragDropEvent.GetOperationAs<FHierarchyDragDropOp>())
	{
		TSharedPtr<FHierarchyElementViewModel> DraggedItem = HierarchyDragDropOp->GetDraggedElement().Pin();
		FHierarchyElementViewModel::FCanPerformActionResults Results = SectionViewModelWeak.Pin()->CanDropOn(DraggedItem, ItemHoverZone);
		HierarchyDragDropOp->SetDescription(Results.CanPerformMessage);

		if(Results.bCanPerform)
		{
			CurrentItemDropZone = ItemHoverZone;
		}
		else
		{
			CurrentItemDropZone.Reset();
		}

		return FReply::Handled();
	}
	
	return FReply::Unhandled();
}

FReply SHierarchySection::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if(SectionViewModelWeak.IsValid() && InKeyEvent.GetKey() == EKeys::Delete && SectionViewModelWeak.Pin()->CanDelete())
	{
		SectionViewModelWeak.Pin()->Delete();
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void SHierarchySection::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	SCompoundWidget::OnMouseLeave(MouseEvent);
}

UHierarchySection* SHierarchySection::TryGetSectionData() const
{
	return Cast<UHierarchySection>(SectionViewModelWeak.Pin()->GetDataMutable());
}

FText SHierarchySection::GetText() const
{
	return SectionViewModelWeak.IsValid() ? SectionViewModelWeak.Pin()->GetSectionNameAsText() : FText::GetEmpty();
}

FText SHierarchySection::GetTooltipText() const
{
	return SectionViewModelWeak.IsValid() ? SectionViewModelWeak.Pin()->GetSectionTooltip() : FText::GetEmpty();
}

void SHierarchySection::OnRenameSection(const FText& Text, ETextCommit::Type CommitType) const
{
	if(SectionViewModelWeak.IsValid() && SectionViewModelWeak.Pin()->GetSectionNameAsText().EqualTo(Text) == false)
	{
		FScopedTransaction Transaction(LOCTEXT("Transaction_Rename_Section", "Renamed hierarchy section"));
		HierarchyViewModel->GetHierarchyRoot()->Modify();
		
		SectionViewModelWeak.Pin()->Rename(FName(Text.ToString()));
	}
}

bool SHierarchySection::OnVerifySectionRename(const FText& NewName, FText& OutTooltip) const
{
	// this function shouldn't be used in case the section isn't valid but we'll make sure regardless
	if(!SectionViewModelWeak.IsValid())
	{
		return false;
	}

	if(SectionViewModelWeak.Pin()->GetSectionName().ToString() != NewName.ToString())
	{
		TArray<FString> SectionNames;

		SectionNames.Add("All");
		for(auto& Section : HierarchyViewModel->GetHierarchyRootViewModel()->GetSectionViewModels())
		{
			SectionNames.Add(Section->GetSectionName().ToString());
		}

		if(SectionNames.Contains(NewName.ToString()))
		{
			OutTooltip = LOCTEXT("HierarchySectionCantRename_Duplicate", "A section with that name already exists!");
			return false;
		}
	}

	return true;
}

bool SHierarchySection::IsSectionSelected() const
{
	return GetSectionCheckState() == ECheckBoxState::Checked ? true : false;
}

bool SHierarchySection::IsSectionReadOnly() const
{
	return SectionViewModelWeak.IsValid() ? !SectionViewModelWeak.Pin()->CanRename() : true;
}

ECheckBoxState SHierarchySection::GetSectionCheckState() const
{
	return IsSectionActive.Get();
}

void SHierarchySection::OnSectionCheckChanged(ECheckBoxState NewState)
{
	OnSectionActivatedDelegate.ExecuteIfBound(SectionViewModelWeak.Pin());
}

EActiveTimerReturnType SHierarchySection::ActivateSectionIfDragging(double CurrentTime, float DeltaTime) const
{
	if(bDraggedOn && FSlateApplication::Get().GetDragDroppingContent().IsValid() && FSlateApplication::Get().GetDragDroppingContent()->IsOfType<FHierarchyDragDropOp>())
	{
		if(IsSectionSelected() == false)
		{
			OnSectionActivatedDelegate.ExecuteIfBound(SectionViewModelWeak.Pin());
		}
	}

	return EActiveTimerReturnType::Stop;
}

const FSlateBrush* SHierarchySection::GetDropIndicatorBrush(EItemDropZone InItemDropZone) const
{
	switch (InItemDropZone)
	{
		case EItemDropZone::AboveItem: return FDataHierarchyEditorStyle::Get().GetBrush("HierarchyEditor.Drop.Section.Above");
		default:
		case EItemDropZone::OntoItem: return FDataHierarchyEditorStyle::Get().GetBrush("HierarchyEditor.Drop.Section.Onto");
		case EItemDropZone::BelowItem: return FDataHierarchyEditorStyle::Get().GetBrush("HierarchyEditor.Drop.Section.Below");
	};
}

EItemDropZone SHierarchySection::ZoneFromPointerPosition(UE::Slate::FDeprecateVector2DParameter LocalPointerPos, UE::Slate::FDeprecateVector2DParameter LocalSize)
{
	const float PointerPos = LocalPointerPos.X;
	const float Size = LocalSize.X;

	const float ZoneBoundarySu = FMath::Clamp(Size * 0.25f, 3.0f, 10.0f);
	if (PointerPos < ZoneBoundarySu)
	{
		return EItemDropZone::AboveItem;
	}
	else if (PointerPos > Size - ZoneBoundarySu)
	{
		return EItemDropZone::BelowItem;
	}
	else
	{
		return EItemDropZone::OntoItem;
	}
}

void SDataHierarchyEditor::Construct(const FArguments& InArgs, TObjectPtr<UDataHierarchyViewModelBase> InHierarchyViewModel)
{
	HierarchyViewModel = InHierarchyViewModel;

	// If the user hasn't called initialize themselves, we do it here, but ideally the user should do it themselves where appropriate
	if(HierarchyViewModel->IsInitialized() == false)
	{
		HierarchyViewModel->Initialize();
	}
	
	SourceRoot.Reset(NewObject<UHierarchyRoot>(HierarchyViewModel->GetOuterForSourceRoot(), MakeUniqueObjectName(HierarchyViewModel->GetOuterForSourceRoot(), UHierarchyRoot::StaticClass()), RF_Transient));
	TSharedPtr<FHierarchyElementViewModel> ViewModel = HierarchyViewModel->CreateViewModelForElement(SourceRoot.Get(), nullptr);
	SourceRootViewModel = StaticCastSharedPtr<FHierarchyRootViewModel>(ViewModel);	
	SourceRootViewModel->Initialize();
	SourceRootViewModel->AddChildFilter(FHierarchyElementViewModel::FOnFilterChild::CreateSP(this, &SDataHierarchyEditor::FilterForSourceSection));
	SourceRootViewModel->OnSyncPropagated().BindSP(this, &SDataHierarchyEditor::RequestRefreshSourceViewNextFrame, false);
	SourceRootViewModel->OnSectionsChanged().BindSP(this, &SDataHierarchyEditor::RefreshSectionsView);

	HierarchyViewModel->OnInitialized().BindSP(this, &SDataHierarchyEditor::Reinitialize);
	HierarchyViewModel->OnNavigateToElementIdentityInHierarchyRequested().BindSP(this, &SDataHierarchyEditor::NavigateToHierarchyElement);
	HierarchyViewModel->OnNavigateToElementInHierarchyRequested().BindSP(this, &SDataHierarchyEditor::NavigateToHierarchyElement);
	HierarchyViewModel->OnRefreshSourceItemsRequested().BindSP(this, &SDataHierarchyEditor::RefreshSourceItems);
	HierarchyViewModel->OnRefreshViewRequested().BindSP(this, &SDataHierarchyEditor::RefreshAllViews);
	HierarchyViewModel->OnRefreshSourceView().BindSP(this, &SDataHierarchyEditor::RefreshSourceView);
	HierarchyViewModel->OnRefreshHierarchyView().BindSP(this, &SDataHierarchyEditor::RefreshHierarchyView);
	HierarchyViewModel->OnRefreshSectionsView().BindSP(this, &SDataHierarchyEditor::RefreshSectionsView);
	HierarchyViewModel->OnHierarchySectionActivated().BindSP(this, &SDataHierarchyEditor::OnHierarchySectionActivated);
	HierarchyViewModel->OnElementAdded().BindSP(this, &SDataHierarchyEditor::OnElementAdded);

	BindToHierarchyRootViewModel();
	
	OnGenerateRowContentWidget = InArgs._OnGenerateRowContentWidget;
	OnGenerateCustomDetailsPanelNameWidget = InArgs._OnGenerateCustomDetailsPanelNameWidget;
	CategoryRowStyle = InArgs._CategoryRowStyle;
	ItemRowStyle = InArgs._ItemRowStyle;
	
	if(!ensureMsgf(OnGenerateRowContentWidget.IsBound(), TEXT("Please add a function binding to the OnGenerateRowContentWidget slate event. Using default row content.")))
	{
		OnGenerateRowContentWidget = FOnGenerateRowContentWidget::CreateLambda([](TSharedRef<FHierarchyElementViewModel> HierarchyElement) -> TSharedRef<SWidget>
		{
			if(HierarchyElement->GetDataMutable()->IsA<UHierarchyCategory>())
			{
				TSharedRef<FHierarchyCategoryViewModel> TreeViewCategory = StaticCastSharedRef<FHierarchyCategoryViewModel>(HierarchyElement);
				return SNew(SHierarchyCategory, TreeViewCategory);
			}
			
			return SNew(STextBlock).Text(HierarchyElement->ToStringAsText());
		});
	}
	SSplitter::FSlot* DetailsPanelSlot = nullptr;

	TSharedRef<SWidget> AddSectionButton = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
			.OnClicked(this, &SDataHierarchyEditor::OnAddSectionClicked)
			.ButtonStyle(FDataHierarchyEditorStyle::Get(), "HierarchyEditor.ButtonStyle")
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.f)
				[
					SNew(SImage)
					.Image(FAppStyle::Get().GetBrush("Icons.PlusCircle"))
				]
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				[			
					SNew(STextBlock)
					.Text(LOCTEXT("AddSectionLabel","Add Section"))				
				]
			]
		];

	TSharedRef<SWidget> AddCategoryButton = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
			.OnClicked(this, &SDataHierarchyEditor::OnAddCategoryClicked)
			.ButtonStyle(FDataHierarchyEditorStyle::Get(), "HierarchyEditor.ButtonStyle")
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.f)
				[
					SNew(SImage)
					.Image(FAppStyle::Get().GetBrush("Icons.PlusCircle"))
				]
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				[			
					SNew(STextBlock)
					.Text(LOCTEXT("AddCategoryLabel","Add Category"))				
				]
			]
		];
	
	ChildSlot
	[
		SNew(SBorder)
		.Padding(0.f)
		.BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel"))
		[
			SNew(SSplitter)
			.Orientation(Orient_Horizontal)
			.PhysicalSplitterHandleSize(2.f)
			+ SSplitter::Slot()
			.Value(0.3f)
			.MinSize(0.1f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(2.f)
				[
					SAssignNew(SourceSearchBox, SSearchBox)
					.OnTextChanged(this, &SDataHierarchyEditor::OnSourceSearchTextChanged)
					.OnTextCommitted(this, &SDataHierarchyEditor::OnSourceSearchTextCommitted)
					.OnSearch(SSearchBox::FOnSearch::CreateSP(this, &SDataHierarchyEditor::OnSearchButtonClicked))
					.DelayChangeNotificationsWhileTyping(true)
					.SearchResultData(this, &SDataHierarchyEditor::GetSearchResultData)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(2.f)
				[
					SAssignNew(SourceSectionBox, SWrapBox)
					.UseAllottedSize(true)
				]
				+ SVerticalBox::Slot()
				.Padding(1.f, 2.f)
				[
					SAssignNew(SourceTreeView, STreeView<TSharedPtr<FHierarchyElementViewModel>>)
					.TreeItemsSource(&GetSourceItems())
					.OnSelectionChanged(this, &SDataHierarchyEditor::OnSelectionChanged, false)
					.OnGenerateRow(this, &SDataHierarchyEditor::GenerateSourceItemRow)
					.OnGetChildren_UObject(InHierarchyViewModel.Get(), &UDataHierarchyViewModelBase::OnGetChildren)
					.OnItemToString_Debug_UObject(InHierarchyViewModel.Get(), &UDataHierarchyViewModelBase::OnElementToStringDebug)
					.OnContextMenuOpening(this, &SDataHierarchyEditor::SummonContextMenuForSelectedRows, false)
				]					
			]
			+ SSplitter::Slot()
			.Value(0.4f)
			.MinSize(0.1f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SBorder)
						.Padding(0.f)
						.BorderImage(FAppStyle::Get().GetBrush("Brushes.Header"))
						[
							AddSectionButton
						]
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(1.f)
					[
						SAssignNew(HierarchySectionBox, SWrapBox)
						.UseAllottedSize(true)
					]				
				]
				+ SVerticalBox::Slot()
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SBorder)
						.Padding(0.f)
						.BorderImage(FAppStyle::Get().GetBrush("Brushes.Header"))
						[
							AddCategoryButton
						]
					]
					+ SVerticalBox::Slot()
					.FillHeight(0.1f)
					.Padding(1.f, 4.f, 1.f, 0.f)
					[
						SNew(SDropTarget)
						.OnDropped(this, &SDataHierarchyEditor::HandleHierarchyRootDrop)
						.OnAllowDrop(this, &SDataHierarchyEditor::OnCanDropOnRoot)
						.OnDragEnter(this, &SDataHierarchyEditor::OnRootDragEnter)
						.OnDragLeave(this, &SDataHierarchyEditor::OnRootDragLeave)
						[
							SNew(SBorder)
							.Padding(0.f)
							.BorderImage(FAppStyle::Get().GetBrush("Brushes.Recessed"))
							[
								SNew(SBorder)
								.Padding(1.f)
								.BorderImage(FAppStyle::GetBrush("DashedBorder"))
								.BorderBackgroundColor(FLinearColor(0.2f, 0.2f, 0.2f, 0.5f))
								[
									SNew(SBox)
									.HAlign(HAlign_Center)
									.VAlign(VAlign_Center)
									[
										SNew(SImage)
										.Image(FDataHierarchyEditorStyle::Get().GetBrush("HierarchyEditor.RootDropIcon"))
										.ColorAndOpacity(this, &SDataHierarchyEditor::GetRootIconColor)
									]
								]
							]
						]
					]
					+ SVerticalBox::Slot()
					.Padding(1.f, 0.f)
					[
						SAssignNew(HierarchyTreeView, STreeView<TSharedPtr<FHierarchyElementViewModel>>)
						.TreeItemsSource(&InHierarchyViewModel->GetHierarchyItems())
						.OnSelectionChanged(this, &SDataHierarchyEditor::OnSelectionChanged, true)
						.OnGenerateRow(this, &SDataHierarchyEditor::GenerateHierarchyItemRow)
						.OnGetChildren_UObject(InHierarchyViewModel.Get(), &UDataHierarchyViewModelBase::OnGetChildren)
						.OnItemToString_Debug_UObject(InHierarchyViewModel.Get(), &UDataHierarchyViewModelBase::OnElementToStringDebug)
						.OnContextMenuOpening(this, &SDataHierarchyEditor::SummonContextMenuForSelectedRows, true)
					]
				]
			]
			+ SSplitter::Slot()
			.Value(0.3f)
			.MinSize(0.1f)
			.Expose(DetailsPanelSlot)		
		]
	];

	if(InHierarchyViewModel->SupportsDetailsPanel())
	{
		FPropertyEditorModule& PropertyEditorModule = FModuleManager::Get().LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

		FDetailsViewArgs DetailsViewArgs;
		DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::ENameAreaSettings::ObjectsUseNameArea;
		DetailsViewArgs.bShowObjectLabel = false;
		DetailsViewArgs.bAllowSearch = false;
		DetailsViewArgs.NotifyHook = this;

		DetailsPanel = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
		
		if(OnGenerateCustomDetailsPanelNameWidget.IsBound())
		{
			TSharedRef<SWidget> CustomDetailsPanelNameWidget = OnGenerateCustomDetailsPanelNameWidget.Execute(nullptr);
			DetailsPanel->SetNameAreaCustomContent(CustomDetailsPanelNameWidget);
		}

		DetailsPanel->SetIsPropertyEditingEnabledDelegate(FIsPropertyEditingEnabled::CreateSP(this, &SDataHierarchyEditor::IsDetailsPanelEditingAllowed));
		
		for(auto& Customizations : HierarchyViewModel->GetInstanceCustomizations())
		{
			DetailsPanel->RegisterInstancedCustomPropertyLayout(Customizations.Key, Customizations.Value);
		}
		
		DetailsPanelSlot->AttachWidget(DetailsPanel.ToSharedRef());
	}
		
	HierarchyViewModel->GetCommands()->MapAction(FGenericCommands::Get().Rename,
		FExecuteAction::CreateSP(this, &SDataHierarchyEditor::RequestRenameSelectedItem),
		FCanExecuteAction::CreateSP(this, &SDataHierarchyEditor::CanRequestRenameSelectedItem),
		FIsActionChecked(), FIsActionButtonVisible::CreateSP(this, &SDataHierarchyEditor::CanRequestRenameSelectedItem));
	
	HierarchyViewModel->GetCommands()->MapAction(FGenericCommands::Get().Delete,
		FExecuteAction::CreateSP(this, &SDataHierarchyEditor::DeleteSelectedHierarchyItems),
		FCanExecuteAction::CreateSP(this, &SDataHierarchyEditor::CanDeleteSelectedElements),
		FIsActionChecked(), FIsActionButtonVisible::CreateSP(this, &SDataHierarchyEditor::CanDeleteSelectedElements));
	
	HierarchyViewModel->GetCommands()->MapAction(FDataHierarchyEditorCommands::Get().FindInHierarchy,
		FExecuteAction::CreateSP(this, &SDataHierarchyEditor::NavigateToMatchingHierarchyElementFromSelectedSourceElement),
		FCanExecuteAction::CreateSP(this, &SDataHierarchyEditor::CanNavigateToMatchingHierarchyElementFromSelectedSourceElement),
		FIsActionChecked(), FIsActionButtonVisible::CreateSP(this, &SDataHierarchyEditor::CanNavigateToMatchingHierarchyElementFromSelectedSourceElement));
	
	HierarchyViewModel->ForceFullRefresh();
	
	SetActiveSourceSection(DefaultSourceSectionViewModel);
}

SDataHierarchyEditor::~SDataHierarchyEditor()
{
	SourceSearchResults.Empty();
	FocusedSearchResult.Reset();
	
	ClearSourceItems();
	
	if(HierarchyViewModel.IsValid())
	{
		HierarchyViewModel->OnInitialized().Unbind();
		HierarchyViewModel->OnNavigateToElementIdentityInHierarchyRequested().Unbind();
		HierarchyViewModel->OnNavigateToElementInHierarchyRequested().Unbind();
		HierarchyViewModel->OnRefreshSourceItemsRequested().Unbind();
		HierarchyViewModel->OnRefreshViewRequested().Unbind();
		HierarchyViewModel->OnRefreshSourceView().Unbind();
		HierarchyViewModel->OnRefreshHierarchyView().Unbind();
		HierarchyViewModel->OnRefreshSectionsView().Unbind();
		HierarchyViewModel->OnHierarchySectionActivated().Unbind();
		HierarchyViewModel->OnElementAdded().Unbind();

		UnbindFromHierarchyRootViewModel();

		HierarchyViewModel->GetCommands()->UnmapAction(FGenericCommands::Get().Delete);
		HierarchyViewModel->GetCommands()->UnmapAction(FGenericCommands::Get().Rename);
		HierarchyViewModel->GetCommands()->UnmapAction(FDataHierarchyEditorCommands::Get().FindInHierarchy);
	}
	
	SourceRootViewModel->OnSyncPropagated().Unbind();
	SourceRootViewModel->OnSectionsChanged().Unbind();
	SourceRootViewModel.Reset();
	SourceRoot->ConditionalBeginDestroy();
	SourceRoot = nullptr;
}

void SDataHierarchyEditor::RefreshSourceItems()
{
	SourceRoot->EmptyAllData();
	HierarchyViewModel->PrepareSourceItems(SourceRoot.Get(), SourceRootViewModel);
	SourceRootViewModel->SyncViewModelsToData();
	RefreshSourceView(false);
	RefreshSectionsView();
}

void SDataHierarchyEditor::RefreshAllViews(bool bFullRefresh)
{
	RefreshSourceView(bFullRefresh);
	RefreshHierarchyView(bFullRefresh);
	RefreshSectionsView();
}

void SDataHierarchyEditor::RequestRefreshAllViewsNextFrame(bool bFullRefresh)
{
	RequestRefreshSourceViewNextFrame(bFullRefresh);
	RequestRefreshHierarchyViewNextFrame(bFullRefresh);
	RequestRefreshSectionsViewNextFrame();
}

FReply SDataHierarchyEditor::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	UHierarchyMenuContext* HierarchyMenuContext = NewObject<UHierarchyMenuContext>();
	HierarchyMenuContext->HierarchyViewModel = HierarchyViewModel;
	
	FToolMenuContext Context;
	Context.AddObject(HierarchyMenuContext);
	
	UToolMenus::Get()->GenerateMenu(HierarchyViewModel->GetContextMenuName(), Context);
	return HierarchyViewModel->GetCommands()->ProcessCommandBindings(InKeyEvent) ? FReply::Handled() : FReply::Unhandled();
}

FReply SDataHierarchyEditor::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	// we catch any mouse button down event so that we can continue using our commands
	return FReply::Handled().SetUserFocus(AsShared(), EFocusCause::Mouse, true);
}

FReply SDataHierarchyEditor::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return FReply::Handled().SetUserFocus(AsShared(), EFocusCause::Mouse, true);
}

FReply SDataHierarchyEditor::OnAddCategoryClicked() const
{
	TArray<TSharedPtr<FHierarchyElementViewModel>> SelectedItems;
	HierarchyTreeView->GetSelectedItems(SelectedItems);

	// We can only add categories under categories or the root
	if(SelectedItems.Num() == 1 && SelectedItems[0]->GetData()->IsA<UHierarchyCategory>())
	{
		HierarchyViewModel.Get()->AddCategory(SelectedItems[0]);
	}
	else
	{
		HierarchyViewModel.Get()->AddCategory(nullptr);
	}
	
	return FReply::Handled();
}

FReply SDataHierarchyEditor::OnAddSectionClicked() const
{
	HierarchyViewModel.Get()->AddSection();
	return FReply::Handled();
}

TSharedPtr<SWidget> SDataHierarchyEditor::SummonContextMenuForSelectedRows(bool bFromHierarchy) const
{
	TArray<TSharedPtr<FHierarchyElementViewModel>> ViewModels;
	if(bFromHierarchy)
	{
		HierarchyTreeView->GetSelectedItems(ViewModels);
	}
	else
	{
		SourceTreeView->GetSelectedItems(ViewModels);
	}

	if(ViewModels.IsEmpty())
	{
		return nullptr;
	}
	
	return SummonContextMenu(ViewModels);
}

void SDataHierarchyEditor::RefreshSourceView(bool bFullRefresh) const
{
	SourceTreeView->SetTreeItemsSource(&GetSourceItems());
	if(bFullRefresh)
	{
		SourceTreeView->RebuildList();
	}
	else
	{
		SourceTreeView->RequestTreeRefresh();
	}
}

void SDataHierarchyEditor::RequestRefreshSourceViewNextFrame(bool bFullRefresh)
{
	if(!RefreshSourceViewNextFrameHandle.IsValid())
	{
		RefreshSourceViewNextFrameHandle = RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateLambda([this, bFullRefresh](double CurrentTime, float DeltaTime)
		{
			RefreshSourceView(bFullRefresh);
			RefreshSourceViewNextFrameHandle.Reset();
			return EActiveTimerReturnType::Stop;
		}));
	}
}

void SDataHierarchyEditor::RefreshHierarchyView(bool bFullRefresh) const
{
	// the top layer objects might have changed due to filtering. We need to refresh these too.
	HierarchyTreeView->SetTreeItemsSource(&HierarchyViewModel->GetHierarchyItems());
	if(bFullRefresh)
	{
		HierarchyTreeView->RebuildList();
	}
	else
	{
		HierarchyTreeView->RequestTreeRefresh();
	}
}

void SDataHierarchyEditor::RequestRefreshHierarchyViewNextFrame(bool bFullRefresh)
{
	if(!RefreshHierarchyViewNextFrameHandle.IsValid())
	{
		RefreshHierarchyViewNextFrameHandle = RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateLambda([this, bFullRefresh](double CurrentTime, float DeltaTime)
		{
			RefreshHierarchyView(bFullRefresh);
			RefreshHierarchyViewNextFrameHandle.Reset();
			return EActiveTimerReturnType::Stop;
		}));
	}
}

void SDataHierarchyEditor::RefreshSectionsView()
{
	SourceSectionBox->ClearChildren();
	HierarchySectionBox->ClearChildren();

	for (TSharedPtr<FHierarchySectionViewModel>& SourceSection : SourceRootViewModel->GetSectionViewModels())
	{
		TSharedPtr<SHierarchySection> SectionWidget = SNew(SHierarchySection, SourceSection)
		.IsSectionActive_Lambda([this, SourceSection]()
		{
			return GetActiveSourceSection() == SourceSection ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		})
		.OnSectionActivated_Lambda([this](TSharedPtr<FHierarchySectionViewModel> SectionViewModel)
		{
			SetActiveSourceSection(SectionViewModel);
		});
		
		SourceSectionBox->AddSlot()
		.Padding(2.f)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SectionWidget.ToSharedRef()		
		];
	}

	if(SourceRootViewModel->GetSectionViewModels().Num() > 0)
	{
		DefaultSourceSectionViewModel = MakeShared<FHierarchySectionViewModel>(nullptr, SourceRootViewModel.ToSharedRef(), HierarchyViewModel);
		
		TSharedPtr<SHierarchySection> DefaultSourceSection = SNew(SHierarchySection, DefaultSourceSectionViewModel)
		.IsSectionActive_Lambda([this]()
		{
			return GetActiveSourceSection() == DefaultSourceSectionViewModel ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		})
		.OnSectionActivated(this, &SDataHierarchyEditor::SetActiveSourceSection);

		SourceSectionBox->AddSlot()
		.Padding(2.f)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			DefaultSourceSection.ToSharedRef()
		];
	}
	
	for (TSharedPtr<FHierarchySectionViewModel>& HierarchySection : HierarchyViewModel->GetHierarchyRootViewModel()->GetSectionViewModels())
	{
		TSharedPtr<SHierarchySection> SectionWidget = SNew(SHierarchySection, HierarchySection)
		.IsSectionActive_Lambda([this, HierarchySection]()
		{
			return HierarchyViewModel->GetActiveHierarchySection() == HierarchySection ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		})
		.OnSectionActivated_Lambda([this](TSharedPtr<FHierarchySectionViewModel> SectionViewModel)
		{
			HierarchyViewModel->SetActiveHierarchySection(SectionViewModel);
		});
		
		HierarchySectionBox->AddSlot()
		.Padding(2.f)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SectionWidget.ToSharedRef()		
		];
	}
	
	TSharedPtr<SHierarchySection> DefaultHierarchySection = SNew(SHierarchySection, HierarchyViewModel->GetDefaultHierarchySectionViewModel())
	.IsSectionActive_Lambda([this]()
	{
		return HierarchyViewModel->GetActiveHierarchySection() == HierarchyViewModel->GetDefaultHierarchySectionViewModel() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	})
	.OnSectionActivated_Lambda([this](TSharedPtr<FHierarchySectionViewModel> SectionViewModel)
	{
		HierarchyViewModel->SetActiveHierarchySection(SectionViewModel);
	});

	HierarchySectionBox->AddSlot()
	.Padding(2.f)
	.HAlign(HAlign_Center)
	.VAlign(VAlign_Center)
	[
		DefaultHierarchySection.ToSharedRef()
	];
}

void SDataHierarchyEditor::RequestRefreshSectionsViewNextFrame()
{
	if(!RefreshSectionsViewNextFrameHandle.IsValid())
	{
		RefreshSectionsViewNextFrameHandle = RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateLambda([this](double CurrentTime, float DeltaTime)
		{
			RefreshSectionsView();
			RefreshSectionsViewNextFrameHandle.Reset();
			return EActiveTimerReturnType::Stop;
		}));
	}
}

void SDataHierarchyEditor::NavigateToHierarchyElement(FHierarchyElementIdentity Identity) const
{
	if(TSharedPtr<FHierarchyElementViewModel> ViewModel = HierarchyViewModel->GetHierarchyRootViewModel()->FindViewModelForChild(Identity, true))
	{
		NavigateToHierarchyElement(ViewModel);
	}
}

void SDataHierarchyEditor::NavigateToHierarchyElement(TSharedPtr<FHierarchyElementViewModel> Item) const
{
	TArray<TSharedPtr<FHierarchyElementViewModel>> ParentChain;
	for(TWeakPtr<FHierarchyElementViewModel> Parent = Item->GetParent(); Parent.IsValid(); Parent = Parent.Pin()->GetParent())
	{
		ParentChain.Add(Parent.Pin());
	}

	for(int32 ParentIndex = ParentChain.Num()-1; ParentIndex >= 0; ParentIndex--)
	{
		HierarchyTreeView->SetItemExpansion(ParentChain[ParentIndex], true);
	}
		
	HierarchyTreeView->SetSelection(Item);
	HierarchyTreeView->RequestScrollIntoView(Item);
}

bool SDataHierarchyEditor::IsItemSelected(TSharedPtr<FHierarchyElementViewModel> Item) const
{
	return HierarchyTreeView->IsItemSelected(Item);
}

TSharedRef<ITableRow> SDataHierarchyEditor::GenerateSourceItemRow(TSharedPtr<FHierarchyElementViewModel> HierarchyItem, const TSharedRef<STableViewBase>& TableViewBase)
{
	return SNew(STableRow<TSharedPtr<FHierarchyElementViewModel>>, TableViewBase)
	.Style(HierarchyItem->GetData()->IsA<UHierarchyItem>() ? ItemRowStyle : CategoryRowStyle)
	.OnDragDetected(HierarchyItem.ToSharedRef(), &FHierarchyElementViewModel::OnDragDetected, true)
	.Padding(FMargin(2))
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(1.f)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.HeightOverride(10.f)
			.WidthOverride(10.f)
			.Visibility_Lambda([HierarchyItem, this]()
			{
				TArray<TSharedPtr<FHierarchyElementViewModel>> AllChildren;
				HierarchyItem->GetChildrenViewModelsForType<UHierarchyElement, FHierarchyElementViewModel>(AllChildren, true);

				bool bCanDrag = HierarchyItem->GetHierarchyViewModel()->GetHierarchyRootViewModel()->FindViewModelForChild(HierarchyItem->GetData()->GetPersistentIdentity(), true) == nullptr;			

				if(bCanDrag)
				{
					for(TSharedPtr<FHierarchyElementViewModel>& ItemViewModel : AllChildren)
					{
						if(HierarchyItem->GetHierarchyViewModel()->GetHierarchyRootViewModel()->FindViewModelForChild(ItemViewModel->GetData()->GetPersistentIdentity(), true) != nullptr)
						{
							bCanDrag = false;
							break;
						}
					}
				}

				return bCanDrag ? EVisibility::Collapsed : EVisibility::Visible;
			})
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush("Icons.Lock"))
				.ToolTipText(LOCTEXT("CantDragItemAlreadyInHierarchyTooltip", "This item already exists within the hierarchy and can not be dragged. Drag the existing one within the hierarchy directly."))
			]
		]
		+ SHorizontalBox::Slot()
		[
			OnGenerateRowContentWidget.Execute(HierarchyItem.ToSharedRef())
		]
	];
}

TSharedRef<ITableRow> SDataHierarchyEditor::GenerateHierarchyItemRow(TSharedPtr<FHierarchyElementViewModel> HierarchyItem, const TSharedRef<STableViewBase>& TableViewBase)
{
	return SNew(STableRow<TSharedPtr<FHierarchyElementViewModel>>, TableViewBase)
	.Style(HierarchyItem->GetData()->IsA<UHierarchyItem>() ? ItemRowStyle : CategoryRowStyle)
	.OnAcceptDrop(HierarchyItem.ToSharedRef(), &FHierarchyElementViewModel::OnDroppedOnRow)
	.OnCanAcceptDrop(HierarchyItem.ToSharedRef(), &FHierarchyElementViewModel::OnCanRowAcceptDrop)
	.OnDragDetected(HierarchyItem.ToSharedRef(), &FHierarchyElementViewModel::OnDragDetected, false)
	.OnDragLeave(HierarchyItem.ToSharedRef(), &FHierarchyElementViewModel::OnRowDragLeave)
	.Padding(FMargin(2))
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(1.f)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.HeightOverride(10.f)
			.WidthOverride(10.f)
			.Visibility_Lambda([HierarchyItem]()
			{
				return HierarchyItem->IsEditableByUser().bCanPerform ? EVisibility::Collapsed : EVisibility::Visible;
			})
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush("Icons.Lock"))
				.ToolTipText_Lambda([HierarchyItem]()
				{
					FHierarchyElementViewModel::FCanPerformActionResults IsEditableResults = HierarchyItem->IsEditableByUser();
					if(IsEditableResults.bCanPerform == false)
					{
						return IsEditableResults.CanPerformMessage;
					}

					return FText::GetEmpty();
				})
			]
		]
		+ SHorizontalBox::Slot()
		[
			OnGenerateRowContentWidget.Execute(HierarchyItem.ToSharedRef())
		]
	];
}

bool SDataHierarchyEditor::FilterForSourceSection(TSharedPtr<const FHierarchyElementViewModel> ItemViewModel) const
{
	if(ActiveSourceSection.IsValid())
	{
		// If the currently selected section data is nullptr, it's the All section, and we let everything pass
		if(ActiveSourceSection.Pin()->GetData() == nullptr)
		{
			return true;
		}

		// if not, we check against identical section data
		return ActiveSourceSection.Pin()->GetData() == ItemViewModel->GetSection();
	}

	return true;
}

void SDataHierarchyEditor::Reinitialize()
{
	// the hierarchy root view model has been recreated if the view model reinitialized. Therefore we update the bindings.
	BindToHierarchyRootViewModel();
	RefreshSourceItems();
	RefreshAllViews(true);
}

void SDataHierarchyEditor::BindToHierarchyRootViewModel()
{
	HierarchyViewModel->GetHierarchyRootViewModel()->OnSyncPropagated().BindSP(this, &SDataHierarchyEditor::RequestRefreshHierarchyViewNextFrame, false);
	HierarchyViewModel->GetHierarchyRootViewModel()->OnSectionsChanged().BindSP(this, &SDataHierarchyEditor::RefreshSectionsView);
	HierarchyViewModel->GetHierarchyRootViewModel()->OnSectionAdded().BindSP(this, &SDataHierarchyEditor::OnHierarchySectionAdded);
	HierarchyViewModel->GetHierarchyRootViewModel()->OnSectionDeleted().BindSP(this, &SDataHierarchyEditor::OnHierarchySectionDeleted);
}

void SDataHierarchyEditor::UnbindFromHierarchyRootViewModel() const
{
	if(HierarchyViewModel->GetHierarchyRootViewModel().IsValid())
	{
		HierarchyViewModel->GetHierarchyRootViewModel()->OnSyncPropagated().Unbind();
		HierarchyViewModel->GetHierarchyRootViewModel()->OnSectionsChanged().Unbind();
		HierarchyViewModel->GetHierarchyRootViewModel()->OnSectionAdded().Unbind();
		HierarchyViewModel->GetHierarchyRootViewModel()->OnSectionDeleted().Unbind();
	}
}

const TArray<TSharedPtr<FHierarchyElementViewModel>>& SDataHierarchyEditor::GetSourceItems() const
{
	return SourceRootViewModel->GetFilteredChildren();
}

bool SDataHierarchyEditor::IsDetailsPanelEditingAllowed() const
{
	return SelectedDetailsPanelItemViewModel.IsValid() && SelectedDetailsPanelItemViewModel.Pin()->IsEditableByUser().bCanPerform;
}

void SDataHierarchyEditor::RequestRenameSelectedItem()
{
	TArray<TSharedPtr<FHierarchyElementViewModel>> SelectedItems = HierarchyTreeView->GetSelectedItems();

	if(SelectedItems.Num() == 0)
	{
		TSharedPtr<FHierarchySectionViewModel> ActiveSection = HierarchyViewModel->GetActiveHierarchySection();
		if(ActiveSection)
		{
			SelectedItems = { ActiveSection };
		}
	}
	
	if(SelectedItems.Num() == 1)
	{
		SelectedItems[0]->RequestRename();
	}
}

bool SDataHierarchyEditor::CanRequestRenameSelectedItem() const
{
	TArray<TSharedPtr<FHierarchyElementViewModel>> SelectedItems = HierarchyTreeView->GetSelectedItems();

	if(SelectedItems.Num() == 0)
	{
		TSharedPtr<FHierarchySectionViewModel> ActiveSection = HierarchyViewModel->GetActiveHierarchySection();
		if(ActiveSection)
		{
			SelectedItems = { ActiveSection };
		}
	}
	
	if(SelectedItems.Num() == 1)
	{
		return SelectedItems[0]->CanRename();
	}

	return false;
}

void SDataHierarchyEditor::ClearSourceItems() const
{
	SourceRoot->GetChildrenMutable().Empty();
	SourceRoot->GetSectionDataMutable().Empty();
	SourceRootViewModel->GetChildrenMutable().Empty();
	SourceRootViewModel->GetSectionViewModels().Empty();
}

void SDataHierarchyEditor::DeleteItems(TArray<TSharedPtr<FHierarchyElementViewModel>> ItemsToDelete) const
{
	HierarchyViewModel->DeleteElements(ItemsToDelete);
}

void SDataHierarchyEditor::DeleteSelectedHierarchyItems() const
{	
	TArray<TSharedPtr<FHierarchyElementViewModel>> SelectedElements = HierarchyTreeView->GetSelectedItems();
	
	if(SelectedElements.Num() == 0)
	{
		TSharedPtr<FHierarchySectionViewModel> ActiveSection = HierarchyViewModel->GetActiveHierarchySection();
		if(ActiveSection)
		{
			SelectedElements = { ActiveSection };
		}
	}
	
	DeleteItems(SelectedElements);
}

bool SDataHierarchyEditor::CanDeleteSelectedElements() const
{
	TArray<TSharedPtr<FHierarchyElementViewModel>> SelectedElements = HierarchyTreeView->GetSelectedItems();

	if(SelectedElements.Num() == 0)
	{
		TSharedPtr<FHierarchySectionViewModel> ActiveSection = HierarchyViewModel->GetActiveHierarchySection();
		if(ActiveSection)
		{
			SelectedElements = { ActiveSection };
		}
	}
	
	if(SelectedElements.Num() > 0)
	{
		bool bCanDelete = true;
		for(TSharedPtr<FHierarchyElementViewModel> SelectedElement : SelectedElements)
		{
			bCanDelete &= SelectedElements[0]->CanDelete();
		}

		return bCanDelete;
	}

	return false;
}

void SDataHierarchyEditor::NavigateToMatchingHierarchyElementFromSelectedSourceElement() const
{
	TArray<TSharedPtr<FHierarchyElementViewModel>> SelectedElements = SourceTreeView->GetSelectedItems();

	if(SelectedElements.Num() != 1)
	{
		return;
	}

	TSharedPtr<FHierarchyElementViewModel> SelectedElement = SelectedElements[0];

	if(TSharedPtr<FHierarchyElementViewModel> MatchingViewModelInHierarchy = HierarchyViewModel->GetHierarchyRootViewModel()->FindViewModelForChild(SelectedElement->GetData()->GetPersistentIdentity(), true))
	{
		NavigateToHierarchyElement(MatchingViewModelInHierarchy.ToSharedRef());
	}
}

bool SDataHierarchyEditor::CanNavigateToMatchingHierarchyElementFromSelectedSourceElement() const
{
	TArray<TSharedPtr<FHierarchyElementViewModel>> SelectedElements = SourceTreeView->GetSelectedItems();

	if(SelectedElements.Num() != 1)
	{
		return false;
	}

	TSharedPtr<FHierarchyElementViewModel> SelectedElement = SelectedElements[0];
	if(SelectedElement->IsForHierarchy())
	{
		return false;
	}
	
	if(TSharedPtr<FHierarchyElementViewModel> MatchingViewModelInHierarchy = HierarchyViewModel->GetHierarchyRootViewModel()->FindViewModelForChild(SelectedElement->GetData()->GetPersistentIdentity(), true))
	{
		return MatchingViewModelInHierarchy.IsValid();
	}
	
	return false;
}

void SDataHierarchyEditor::DeleteActiveSection() const
{
	if(TSharedPtr<FHierarchySectionViewModel> ActiveHierarchySectionViewModel = HierarchyViewModel->GetActiveHierarchySection())
	{
		DeleteItems({ActiveHierarchySectionViewModel});
	}
}

bool SDataHierarchyEditor::CanDeleteActiveSection() const
{
	return HierarchyViewModel->GetActiveHierarchySection() != nullptr;
}

void SDataHierarchyEditor::OnElementAdded(TSharedPtr<FHierarchyElementViewModel> AddedItem)
{
	// when a new item is created (opposed to dragged & dropped from source view, i.e. only categories so far)
	// we make sure to request a tree refresh, select the row, and request a pending rename since the widget will created a frame later
	if(AddedItem->GetData()->IsA<UHierarchyItem>() || AddedItem->GetData()->IsA<UHierarchyCategory>())
	{
		HierarchyTreeView->RequestTreeRefresh();
		NavigateToHierarchyElement(AddedItem);
	}
	else if(AddedItem->GetData()->IsA<UHierarchySection>())
	{
		RefreshSectionsView();
	}

	AddedItem->RequestRenamePending();
}

void SDataHierarchyEditor::OnHierarchySectionActivated(TSharedPtr<FHierarchySectionViewModel> Section)
{
	// We forward nullptr in case this is the 'All' default section. We determine this by checking its data for validity.
	// The all section does not have an actual element associated with it.
	OnSelectionChanged(Section.IsValid() && Section->GetData() ? Section : nullptr, ESelectInfo::Direct, true);
}

void SDataHierarchyEditor::OnSourceSectionActivated(TSharedPtr<FHierarchySectionViewModel> Section)
{
	OnSelectionChanged(Section.IsValid() && Section->GetData() ? Section : nullptr, ESelectInfo::Direct, false);
	RunSourceSearch();
}

void SDataHierarchyEditor::OnHierarchySectionAdded(TSharedPtr<FHierarchySectionViewModel> AddedSection)
{
	HierarchyViewModel->SetActiveHierarchySection(AddedSection);
	AddedSection->RequestRenamePending();
}

void SDataHierarchyEditor::OnHierarchySectionDeleted(TSharedPtr<FHierarchySectionViewModel> DeletedSection)
{
	if(HierarchyViewModel->GetActiveHierarchySection() == DeletedSection)
	{
		HierarchyViewModel->SetActiveHierarchySection(HierarchyViewModel->GetDefaultHierarchySectionViewModel());
	}
}

void SDataHierarchyEditor::SetActiveSourceSection(TSharedPtr<FHierarchySectionViewModel> Section)
{
	ActiveSourceSection = Section;	
	RefreshSourceView(true);
	OnSourceSectionActivated(Section);
}

TSharedPtr<FHierarchySectionViewModel> SDataHierarchyEditor::GetActiveSourceSection() const
{
	return ActiveSourceSection.IsValid() ? ActiveSourceSection.Pin() : nullptr;
}

UHierarchySection* SDataHierarchyEditor::GetActiveSourceSectionData() const
{
	return ActiveSourceSection.IsValid() ? ActiveSourceSection.Pin()->GetDataMutable<UHierarchySection>() : nullptr;
}

void SDataHierarchyEditor::OnSelectionChanged(TSharedPtr<FHierarchyElementViewModel> HierarchyItem, ESelectInfo::Type Type, bool bFromHierarchy) const
{
	SelectedDetailsPanelItemViewModel.Reset();
	if(DetailsPanel.IsValid())
	{
		if(HierarchyItem.IsValid() && HierarchyItem->AllowEditingInDetailsPanel())
		{
			// when se select a section, and the previous item selection is no longer available due to it, we would get a selection refresh next tick
			// to wipe out the current selection. We want to avoid that, so we manually clear the selected items in that case.
			if(HierarchyItem->GetData()->IsA<UHierarchySection>())
			{
				HierarchyTreeView->ClearSelection();
			}

			// we clear the selection of the other tree view to avoid 
			if(bFromHierarchy)
			{
				SourceTreeView->ClearSelection();
			}
			else
			{
				HierarchyTreeView->ClearSelection();
			}
			
			UObject* DataForEditing = HierarchyItem->GetDataForEditing();
			DataForEditing->SetFlags(RF_Transactional);

			// we make sure the object we are editing is transactional
			DetailsPanel->SetObject(DataForEditing);
			SelectedDetailsPanelItemViewModel = HierarchyItem;
		}
		else
		{
			SelectedDetailsPanelItemViewModel.Reset();
			DetailsPanel->SetObject(nullptr);
		}
	}
	
	if(DetailsPanel.IsValid() && OnGenerateCustomDetailsPanelNameWidget.IsBound() && SelectedDetailsPanelItemViewModel.IsValid())
	{
		TSharedRef<SWidget> NameWidget = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2.f)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SImage)
			.Image(FAppStyle::GetBrush("Icons.Lock"))
			.Visibility(SelectedDetailsPanelItemViewModel.Pin()->IsEditableByUser().bCanPerform ? EVisibility::Collapsed : EVisibility::Visible)
			.ToolTipText(SelectedDetailsPanelItemViewModel.Pin()->IsEditableByUser().CanPerformMessage)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2.f)
		[
			OnGenerateCustomDetailsPanelNameWidget.Execute(SelectedDetailsPanelItemViewModel.Pin())
		];
		
		DetailsPanel->SetNameAreaCustomContent(NameWidget);
	}
}

void SDataHierarchyEditor::RunSourceSearch()
{
	if(!SourceSearchBox->GetText().IsEmpty())
	{
		OnSourceSearchTextChanged(SourceSearchBox->GetText());
	}
}

void SDataHierarchyEditor::OnSourceSearchTextChanged(const FText& Text)
{
	SourceSearchResults.Empty();
	FocusedSearchResult.Reset();
	SourceTreeView->ClearSelection();

	if(!Text.IsEmpty())
	{
		FString TextAsString = Text.ToString();;

		TArray<FSearchItem> SearchItems;
		GenerateSearchItems(SourceRootViewModel.ToSharedRef(), {}, SearchItems);
		
		for(const FSearchItem& SearchItem : SearchItems)
		{
			for(const FString& SearchTerm : SearchItem.GetEntry()->GetSearchTerms())
			{
				if(SearchTerm.Contains(TextAsString))
				{
					SourceSearchResults.Add(SearchItem);
				}
			}
		}

		ExpandSourceSearchResults();
		SelectNextSourceSearchResult();
	}
	else
	{
		SourceTreeView->ClearExpandedItems();
	}
}

void SDataHierarchyEditor::OnSourceSearchTextCommitted(const FText& Text, ETextCommit::Type CommitType)
{
	bool bIsShiftDown = FSlateApplication::Get().GetModifierKeys().IsShiftDown();
	if(CommitType == ETextCommit::OnEnter)
	{
		if(bIsShiftDown == false)
		{
			SelectNextSourceSearchResult();
		}
		else
		{
			SelectPreviousSourceSearchResult();
		}
	}
}

void SDataHierarchyEditor::OnSearchButtonClicked(SSearchBox::SearchDirection SearchDirection)
{
	if(SearchDirection == SSearchBox::Next)
	{
		SelectNextSourceSearchResult();
	}
	else
	{
		SelectPreviousSourceSearchResult();
	}
}

void SDataHierarchyEditor::GenerateSearchItems(TSharedRef<FHierarchyElementViewModel> Root, TArray<TSharedPtr<FHierarchyElementViewModel>> ParentChain, TArray<FSearchItem>& OutSearchItems)
{
	const TArray<TSharedPtr<FHierarchyElementViewModel>> FilteredChildren = Root->GetFilteredChildren();
	ParentChain.Add(Root);
	OutSearchItems.Add(FSearchItem{ParentChain});
	for(TSharedPtr<FHierarchyElementViewModel> Child : FilteredChildren)
	{
		GenerateSearchItems(Child.ToSharedRef(), ParentChain, OutSearchItems);
	}
}

void SDataHierarchyEditor::ExpandSourceSearchResults()
{
	SourceTreeView->ClearExpandedItems();

	for(FSearchItem& SearchResult : SourceSearchResults)
	{
		for(TSharedPtr<FHierarchyElementViewModel>& EntryInPath : SearchResult.Path)
		{
			SourceTreeView->SetItemExpansion(EntryInPath, true);
		}
	}
}

void SDataHierarchyEditor::SelectNextSourceSearchResult()
{
	if(SourceSearchResults.IsEmpty())
	{
		return;
	}
	
	if(!FocusedSearchResult.IsSet())
	{
		FocusedSearchResult = SourceSearchResults[0];
	}
	else
	{
		int32 CurrentSearchResultIndex = SourceSearchResults.Find(FocusedSearchResult.GetValue());
		if(SourceSearchResults.IsValidIndex(CurrentSearchResultIndex+1))
		{
			FocusedSearchResult = SourceSearchResults[CurrentSearchResultIndex+1];
		}
		else
		{
			FocusedSearchResult = SourceSearchResults[0];
		}
	}

	SourceTreeView->ClearSelection();
	SourceTreeView->RequestScrollIntoView(FocusedSearchResult.GetValue().GetEntry());
	SourceTreeView->SetItemSelection(FocusedSearchResult.GetValue().GetEntry(), true);
}

void SDataHierarchyEditor::SelectPreviousSourceSearchResult()
{
	if(SourceSearchResults.IsEmpty())
	{
		return;
	}
	
	if(!FocusedSearchResult.IsSet())
	{
		FocusedSearchResult = SourceSearchResults[0];
	}
	else
	{
		int32 CurrentSearchResultIndex = SourceSearchResults.Find(FocusedSearchResult.GetValue());
		if(SourceSearchResults.IsValidIndex(CurrentSearchResultIndex-1))
		{
			FocusedSearchResult = SourceSearchResults[CurrentSearchResultIndex-1];
		}
		else
		{
			FocusedSearchResult = SourceSearchResults[SourceSearchResults.Num()-1];
		}
	}

	SourceTreeView->ClearSelection();
	SourceTreeView->RequestScrollIntoView(FocusedSearchResult.GetValue().GetEntry());
	SourceTreeView->SetItemSelection(FocusedSearchResult.GetValue().GetEntry(), true);
}

TOptional<SSearchBox::FSearchResultData> SDataHierarchyEditor::GetSearchResultData() const
{
	if(SourceSearchResults.Num() > 0)
	{
		SSearchBox::FSearchResultData SearchResultData;
		SearchResultData.NumSearchResults = SourceSearchResults.Num();

		if(FocusedSearchResult.IsSet())
		{
			// we add one just to make it look nicer as this is merely for cosmetic purposes
			SearchResultData.CurrentSearchResultIndex = SourceSearchResults.Find(FocusedSearchResult.GetValue()) + 1;
		}
		else
		{
			SearchResultData.CurrentSearchResultIndex = INDEX_NONE;
		}

		return SearchResultData;
	}

	return TOptional<SSearchBox::FSearchResultData>();
}

FReply SDataHierarchyEditor::HandleHierarchyRootDrop(const FGeometry& Geometry, const FDragDropEvent& DragDropEvent) const
{
	if(TSharedPtr<FHierarchyDragDropOp> DragDropOp = DragDropEvent.GetOperationAs<FHierarchyDragDropOp>())
	{
		HierarchyViewModel->GetHierarchyRootViewModel()->OnDroppedOn(DragDropOp->GetDraggedElement().Pin(), EItemDropZone::OntoItem);
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FHierarchyElementViewModel::FCanPerformActionResults SDataHierarchyEditor::CanDropOnRoot(TSharedPtr<FHierarchyElementViewModel> DraggedItem) const
{
	return HierarchyViewModel->GetHierarchyRootViewModel()->CanDropOnInternal(DraggedItem, EItemDropZone::OntoItem);
}

bool SDataHierarchyEditor::OnCanDropOnRoot(TSharedPtr<FDragDropOperation> DragDropOperation) const
{
	if(DragDropOperation->IsOfType<FHierarchyDragDropOp>())
	{
		TSharedPtr<FHierarchyDragDropOp> HierarchyDragDropOp = StaticCastSharedPtr<FHierarchyDragDropOp>(DragDropOperation);
		TSharedPtr<FHierarchyElementViewModel> DraggedItem = HierarchyDragDropOp->GetDraggedElement().Pin();
		return CanDropOnRoot(DraggedItem).bCanPerform;
	}
	
	return false;
}

void SDataHierarchyEditor::OnRootDragEnter(const FDragDropEvent& DragDropEvent) const
{
	if(TSharedPtr<FHierarchyDragDropOp> HierarchyDragDropOp = DragDropEvent.GetOperationAs<FHierarchyDragDropOp>())
	{
		FHierarchyElementViewModel::FCanPerformActionResults CanPerformActionResults = CanDropOnRoot(HierarchyDragDropOp->GetDraggedElement().Pin());
		HierarchyDragDropOp->SetDescription(CanPerformActionResults.CanPerformMessage);
	}
}

void SDataHierarchyEditor::OnRootDragLeave(const FDragDropEvent& DragDropEvent) const
{
	if(TSharedPtr<FHierarchyDragDropOp> HierarchyDragDropOp = DragDropEvent.GetOperationAs<FHierarchyDragDropOp>())
	{
		HierarchyDragDropOp->SetDescription(FText::GetEmpty());
	}
}

FSlateColor SDataHierarchyEditor::GetRootIconColor() const
{
	if(FSlateApplication::Get().IsDragDropping())
	{
		if(FSlateApplication::Get().GetDragDroppingContent()->IsOfType<FHierarchyDragDropOp>())
		{
			TSharedPtr<FHierarchyDragDropOp> HierarchyDragDropOp = StaticCastSharedPtr<FHierarchyDragDropOp>(FSlateApplication::Get().GetDragDroppingContent());
			if(CanDropOnRoot(HierarchyDragDropOp->GetDraggedElement().Pin()).bCanPerform)
			{
				return FLinearColor(0.8f, 0.8f, 0.8f, 0.8f);
			}
		}
	}

	return FLinearColor(0.2f, 0.2f, 0.2f, 0.5f);
}

void SDataHierarchyEditor::NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent,	FProperty* PropertyThatChanged)
{
	HierarchyViewModel->OnHierarchyPropertiesChanged().Broadcast();
}

#undef LOCTEXT_NAMESPACE
