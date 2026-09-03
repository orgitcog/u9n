// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SDropTarget.h"
#include "IDetailsView.h"
#include "DataHierarchyEditorStyle.h"
#include "Widgets/Views/STreeView.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/Input/SMenuAnchor.h"
#include "Styling/SlateTypes.h"
#include "DataHierarchyViewModelBase.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Misc/NotifyHook.h"
#include "UObject/GCObject.h"

class UDataHierarchyViewModelBase;
class UHierarchySection;
struct FHierarchyElementViewModel;
struct FHierarchySectionViewModel;

class DATAHIERARCHYEDITOR_API SHierarchySection : public SCompoundWidget
{
	DECLARE_DELEGATE_OneParam(FOnSectionActivated, TSharedPtr<FHierarchySectionViewModel> SectionViewModel)

	static const FSlateBrush LeftDropBrush;
	static const FSlateBrush RightDropBrush;
	
	SLATE_BEGIN_ARGS(SHierarchySection)
		{}
		SLATE_ATTRIBUTE(ECheckBoxState, IsSectionActive)
		SLATE_EVENT(FOnSectionActivated, OnSectionActivated)
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs, TSharedPtr<struct FHierarchySectionViewModel> InSection);
	virtual ~SHierarchySection() override;

	void TryEnterEditingMode() const;

	TSharedPtr<struct FHierarchySectionViewModel> GetSectionViewModel();
private:
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	int32 PaintDropIndicator(const FPaintArgs& Args, const FGeometry& Geometry, FSlateRect SlateRect, FSlateWindowElementList& OutDrawElements, int32 INT32, const FWidgetStyle& WidgetStyle, bool bParentEnabled) const;
	int32 OnPaintDropIndicator(EItemDropZone ItemDropZone, const FPaintArgs& Args, const FGeometry& Geometry, FSlateRect SlateRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& WidgetStyle, bool bParentEnabled) const;

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	
	bool OnCanAcceptDrop(TSharedPtr<FDragDropOperation> DragDropOperation, EItemDropZone ItemDropZone) const;
	FReply OnDroppedOn(const FGeometry&, const FDragDropEvent& DragDropEvent, EItemDropZone DropZone) const;
	
	virtual void OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	virtual FReply OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnDragLeave(const FDragDropEvent& DragDropEvent) override;
	virtual FReply OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;

	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;

	UHierarchySection* TryGetSectionData() const;

	FText GetText() const;
	FText GetTooltipText() const;
	void OnRenameSection(const FText& Text, ETextCommit::Type CommitType) const;
	bool OnVerifySectionRename(const FText& NewName, FText& OutTooltip) const;

	bool IsSectionSelected() const;
	bool IsSectionReadOnly() const;
	ECheckBoxState GetSectionCheckState() const;
	void OnSectionCheckChanged(ECheckBoxState NewState);
	EActiveTimerReturnType ActivateSectionIfDragging(double CurrentTime, float DeltaTime) const;

	const FSlateBrush* GetDropIndicatorBrush(EItemDropZone ItemDropZone) const;

	/** @return the zone (above, onto, below) based on where the user is hovering over within the row */
	EItemDropZone ZoneFromPointerPosition(UE::Slate::FDeprecateVector2DParameter LocalPointerPos, UE::Slate::FDeprecateVector2DParameter LocalSize);

private:
	TSharedPtr<SMenuAnchor> MenuAnchor;
	TSharedPtr<SCheckBox> CheckBox;
	TSharedPtr<SInlineEditableTextBlock> InlineEditableTextBlock;
	TWeakObjectPtr<UDataHierarchyViewModelBase> HierarchyViewModel;
	TWeakPtr<FHierarchySectionViewModel> SectionViewModelWeak;
private:
	TAttribute<ECheckBoxState> IsSectionActive;
	FOnSectionActivated OnSectionActivatedDelegate;
	
	mutable bool bDraggedOn = false;
	
	mutable TOptional<EItemDropZone> CurrentItemDropZone;
};

class DATAHIERARCHYEDITOR_API SDataHierarchyEditor : public SCompoundWidget, public FNotifyHook
{
	DECLARE_DELEGATE_RetVal_OneParam(TSharedRef<SWidget>, FOnGenerateRowContentWidget, TSharedRef<FHierarchyElementViewModel> HierarchyElement);
	DECLARE_DELEGATE_RetVal_OneParam(TSharedRef<SWidget>, FOnGenerateCustomDetailsPanelNameWidget, TSharedPtr<FHierarchyElementViewModel> HierarchyElement);

	SLATE_BEGIN_ARGS(SDataHierarchyEditor)
		: _bReadOnly(true)
		, _ItemRowStyle(&FAppStyle::Get().GetWidgetStyle<FTableRowStyle>("TableView.Row"))
		, _CategoryRowStyle(&FDataHierarchyEditorStyle::Get().GetWidgetStyle<FTableRowStyle>("HierarchyEditor.Row.Category"))
	{}
		SLATE_ARGUMENT(bool, bReadOnly)
		SLATE_EVENT(FOnGenerateRowContentWidget, OnGenerateRowContentWidget)
		SLATE_EVENT(FOnGenerateCustomDetailsPanelNameWidget, OnGenerateCustomDetailsPanelNameWidget)
		SLATE_STYLE_ARGUMENT(FTableRowStyle, ItemRowStyle)
		SLATE_STYLE_ARGUMENT(FTableRowStyle, CategoryRowStyle)
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs, TObjectPtr<UDataHierarchyViewModelBase> InHierarchyViewModel);
	virtual ~SDataHierarchyEditor() override;

	void RefreshSourceItems();
	void RefreshAllViews(bool bFullRefresh = false);
	void RequestRefreshAllViewsNextFrame(bool bFullRefresh = false);
	void RefreshSourceView(bool bFullRefresh = false) const;
	void RequestRefreshSourceViewNextFrame(bool bFullRefresh = false);
	void RefreshHierarchyView(bool bFullRefresh = false) const;
	void RequestRefreshHierarchyViewNextFrame(bool bFullRefresh = false);
	void RefreshSectionsView();
	void RequestRefreshSectionsViewNextFrame();

	void NavigateToHierarchyElement(FHierarchyElementIdentity Identity) const;
	void NavigateToHierarchyElement(TSharedPtr<FHierarchyElementViewModel> Item) const;
	bool IsItemSelected(TSharedPtr<FHierarchyElementViewModel> Item) const;
	
private:
	// need to do this to enable focus so we can handle shortcuts
	virtual bool SupportsKeyboardFocus() const override { return true; }
	
	TSharedRef<ITableRow> GenerateSourceItemRow(TSharedPtr<FHierarchyElementViewModel> HierarchyItem, const TSharedRef<STableViewBase>& TableViewBase);
	TSharedRef<ITableRow> GenerateHierarchyItemRow(TSharedPtr<FHierarchyElementViewModel> HierarchyItem, const TSharedRef<STableViewBase>& TableViewBase);

	bool FilterForSourceSection(TSharedPtr<const FHierarchyElementViewModel> ItemViewModel) const;
private:
	void Reinitialize();

	void BindToHierarchyRootViewModel();
	void UnbindFromHierarchyRootViewModel() const;
	
	/** Source items reflect the base, unedited status of items to edit into a hierarchy */
	const TArray<TSharedPtr<FHierarchyElementViewModel>>& GetSourceItems() const;
	
	bool IsDetailsPanelEditingAllowed() const;
	
	void ClearSourceItems() const;
	
	void RequestRenameSelectedItem();
	bool CanRequestRenameSelectedItem() const;

	void DeleteItems(TArray<TSharedPtr<FHierarchyElementViewModel>> ItemsToDelete) const;
	void DeleteSelectedHierarchyItems() const;
	bool CanDeleteSelectedElements() const;

	void NavigateToMatchingHierarchyElementFromSelectedSourceElement() const;
	bool CanNavigateToMatchingHierarchyElementFromSelectedSourceElement() const;

	void DeleteActiveSection() const;
	bool CanDeleteActiveSection() const;

	void OnElementAdded(TSharedPtr<FHierarchyElementViewModel> AddedItem);
	void OnHierarchySectionActivated(TSharedPtr<FHierarchySectionViewModel> Section);
	void OnSourceSectionActivated(TSharedPtr<FHierarchySectionViewModel> Section);
	void OnHierarchySectionAdded(TSharedPtr<FHierarchySectionViewModel> AddedSection);
	void OnHierarchySectionDeleted(TSharedPtr<FHierarchySectionViewModel> DeletedSection);

	void SetActiveSourceSection(TSharedPtr<struct FHierarchySectionViewModel>);
	TSharedPtr<FHierarchySectionViewModel> GetActiveSourceSection() const;
	UHierarchySection* GetActiveSourceSectionData() const;
	
	void OnSelectionChanged(TSharedPtr<FHierarchyElementViewModel> HierarchyItem, ESelectInfo::Type Type, bool bFromHierarchy) const;

	void RunSourceSearch();
	void OnSourceSearchTextChanged(const FText& Text);
	void OnSourceSearchTextCommitted(const FText& Text, ETextCommit::Type CommitType);
	void OnSearchButtonClicked(SSearchBox::SearchDirection SearchDirection);

	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	FReply OnAddCategoryClicked() const;
	FReply OnAddSectionClicked() const;

	TSharedPtr<SWidget> SummonContextMenuForSelectedRows(bool bFromHierarchy) const;

	struct FSearchItem
	{
		TArray<TSharedPtr<FHierarchyElementViewModel>> Path;

		TSharedPtr<FHierarchyElementViewModel> GetEntry() const
		{
			return Path.Num() > 0 ? 
				Path[Path.Num() - 1] : 
				nullptr;
		}

		bool operator==(const FSearchItem& Item) const
		{
			return Path == Item.Path;
		}
	};

	/** This will recursively generated parent chain paths for all items within the given root. Used for expansion purposes. */
	void GenerateSearchItems(TSharedRef<FHierarchyElementViewModel> Root, TArray<TSharedPtr<FHierarchyElementViewModel>> ParentChain, TArray<FSearchItem>& OutSearchItems);
	void ExpandSourceSearchResults();
	void SelectNextSourceSearchResult();
	void SelectPreviousSourceSearchResult();
	TOptional<SSearchBox::FSearchResultData> GetSearchResultData() const;
	
	FHierarchyElementViewModel::FCanPerformActionResults CanDropOnRoot(TSharedPtr<FHierarchyElementViewModel> DraggedItem) const;

	/** Callback functions for the root widget */
	FReply HandleHierarchyRootDrop(const FGeometry&, const FDragDropEvent& DragDropEvent) const;
	bool OnCanDropOnRoot(TSharedPtr<FDragDropOperation> DragDropOperation) const;
	void OnRootDragEnter(const FDragDropEvent& DragDropEvent) const;
	void OnRootDragLeave(const FDragDropEvent& DragDropEvent) const;
	FSlateColor GetRootIconColor() const;

	virtual void NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged) override;
private:
	TWeakObjectPtr<UDataHierarchyViewModelBase> HierarchyViewModel;
	
	TArray<FSearchItem> SourceSearchResults;
	TOptional<FSearchItem> FocusedSearchResult;

	const FTableRowStyle* CategoryRowStyle = nullptr;
	const FTableRowStyle* ItemRowStyle = nullptr;
	
	mutable TWeakPtr<FHierarchyElementViewModel> SelectedDetailsPanelItemViewModel;

private:
	TStrongObjectPtr<UHierarchyRoot> SourceRoot;
	TSharedPtr<FHierarchyRootViewModel> SourceRootViewModel;
	TSharedPtr<STreeView<TSharedPtr<FHierarchyElementViewModel>>> SourceTreeView;
	TSharedPtr<STreeView<TSharedPtr<FHierarchyElementViewModel>>> HierarchyTreeView;
	TSharedPtr<FHierarchySectionViewModel> DefaultSourceSectionViewModel;
	TWeakPtr<struct FHierarchySectionViewModel> ActiveSourceSection;
	TSharedPtr<class SWrapBox> SourceSectionBox;
	TSharedPtr<class SWrapBox> HierarchySectionBox;
	TSharedPtr<SSearchBox> SourceSearchBox;
	TSharedPtr<IDetailsView> DetailsPanel;
private:
	FOnGenerateRowContentWidget OnGenerateRowContentWidget;
	FOnGenerateCustomDetailsPanelNameWidget OnGenerateCustomDetailsPanelNameWidget;
	TSharedPtr<FActiveTimerHandle> RefreshHierarchyViewNextFrameHandle;
	TSharedPtr<FActiveTimerHandle> RefreshSourceViewNextFrameHandle;
	TSharedPtr<FActiveTimerHandle> RefreshSectionsViewNextFrameHandle;
};

class DATAHIERARCHYEDITOR_API SHierarchyCategory : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SHierarchyCategory)
	{}
		SLATE_EVENT(FIsSelected, IsSelected)
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs, TSharedPtr<struct FHierarchyCategoryViewModel> InCategoryViewModel);

	void EnterEditingMode() const;
	bool OnVerifyCategoryRename(const FText& NewName, FText& OutTooltip) const;
	
	FText GetCategoryText() const;
	void OnRenameCategory(const FText& NewText, ETextCommit::Type) const;
	
private:
	TWeakPtr<struct FHierarchyCategoryViewModel> CategoryViewModelWeak;
	TSharedPtr<SInlineEditableTextBlock> InlineEditableTextBlock;
};
