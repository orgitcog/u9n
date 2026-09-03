// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageFactory.h"
#include "Elements/Interfaces/TypedElementDataStorageUiInterface.h"
#include "Internationalization/Text.h"
#include "UObject/ObjectMacros.h"
#include "Widgets/Images/SImage.h"
#include "ScopedTransaction.h"
#include "SceneOutlinerFwd.h"

#include "OutlinerVisibilityWidget.generated.h"

namespace UE::Editor::DataStorage
{
	class ICoreProvider;
}

class UScriptStruct;

UCLASS()
class UOutlinerVisibilityWidgetFactory : public UEditorDataStorageFactory
{
	GENERATED_BODY()

public:
	~UOutlinerVisibilityWidgetFactory() override = default;

	TEDSOUTLINER_API void RegisterWidgetConstructors(UE::Editor::DataStorage::ICoreProvider& DataStorage, UE::Editor::DataStorage::IUiProvider& DataStorageUi) const override;
};

UCLASS()
class UOutlinerVisibilityHeaderFactory : public UEditorDataStorageFactory
{
	GENERATED_BODY()

public:
	~UOutlinerVisibilityHeaderFactory() override = default;

	TEDSOUTLINER_API void RegisterWidgetConstructors(UE::Editor::DataStorage::ICoreProvider& DataStorage, UE::Editor::DataStorage::IUiProvider& DataStorageUi) const override;
};

USTRUCT()
struct FOutlinerVisibilityWidgetConstructor : public FSimpleWidgetConstructor
{
	GENERATED_BODY()

public:
	TEDSOUTLINER_API FOutlinerVisibilityWidgetConstructor();
	~FOutlinerVisibilityWidgetConstructor() override = default;

	TEDSOUTLINER_API virtual TSharedPtr<SWidget> CreateWidget(
		UE::Editor::DataStorage::ICoreProvider* DataStorage,
		UE::Editor::DataStorage::IUiProvider* DataStorageUi,
		UE::Editor::DataStorage::RowHandle TargetRow,
		UE::Editor::DataStorage::RowHandle WidgetRow, 
		const UE::Editor::DataStorage::FMetaDataView& Arguments) override;
};

USTRUCT()
struct FOutlinerVisibilityHeaderConstructor : public FSimpleWidgetConstructor
{
	GENERATED_BODY()

public:
	TEDSOUTLINER_API FOutlinerVisibilityHeaderConstructor();
	~FOutlinerVisibilityHeaderConstructor() override = default;

	TEDSOUTLINER_API virtual TSharedPtr<SWidget> CreateWidget(
		UE::Editor::DataStorage::ICoreProvider* DataStorage,
		UE::Editor::DataStorage::IUiProvider* DataStorageUi,
		UE::Editor::DataStorage::RowHandle TargetRow,
		UE::Editor::DataStorage::RowHandle WidgetRow, 
		const UE::Editor::DataStorage::FMetaDataView& Arguments) override;
};

/** Widget responsible for managing the visibility for a single item */
class TEDSOUTLINER_API STedsVisibilityWidget : public SImage
{
	using RowHandle = UE::Editor::DataStorage::RowHandle;

public:
	SLATE_BEGIN_ARGS(STedsVisibilityWidget) {}
	SLATE_END_ARGS()

	/** Construct this widget */
	void Construct(const FArguments& InArgs, const RowHandle& InTargetRow, const RowHandle& InWidgetRow);

protected:

	/** Returns whether the widget is enabled or not */
	virtual bool IsEnabled() const { return true; }

	/** Get the brush for this widget */
	virtual const FSlateBrush* GetBrush() const;

	/** Start a new drag/drop operation for this widget */
	virtual FReply OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	/** If a visibility drag drop operation has entered this widget, set its item to the new visibility state */
	virtual void OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;

	FReply HandleClick();

	virtual FReply OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent) override;

	/** Called when the mouse button is pressed down on this widget */
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	/** Process a mouse up message */
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	/** Called when this widget had captured the mouse, but that capture has been revoked for some reason. */
	virtual void OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;

	/** Whether visibility change should propagate down to children. */
	virtual bool ShouldPropagateVisibilityChangeOnChildren() const { return true; }

	virtual FSlateColor GetForegroundColor() const;

	/** Check if the TargetRow Object is Visible */
	bool IsVisible() const;

	/** Check if the TargetRow Object is Selected */
	bool IsSelected() const;

	/** Set the item this widget is responsible for to be hidden or shown */
	void SetIsVisible(RowHandle InRow, const bool bVisible);

	RowHandle TargetRow = UE::Editor::DataStorage::InvalidRowHandle;
	RowHandle WidgetRow = UE::Editor::DataStorage::InvalidRowHandle;

	/** Scoped undo transaction */
	TUniquePtr<FScopedTransaction> UndoTransaction;

	/** Visibility brushes for the various states */
	const FSlateBrush* VisibleHoveredBrush = nullptr;
	const FSlateBrush* VisibleNotHoveredBrush = nullptr;
	const FSlateBrush* NotVisibleHoveredBrush = nullptr;
	const FSlateBrush* NotVisibleNotHoveredBrush = nullptr;

private:

	// Commit the visibility state into the Rows DataStorage. Will trigger a SyncBackToWorld manually but the object is never dirtied.
	static void CommitVisibility(UE::Editor::DataStorage::ICoreProvider& DataStorage, RowHandle Row, bool bVisible);

	// Recursive internal method to set visibility on a TreeItem and all of it's children.
	static void SetVisibility_Recursive(UE::Editor::DataStorage::ICoreProvider& DataStorage, FSceneOutlinerTreeItemPtr TreeItem, bool bVisible);

	// Returns the tree item associated with this widget.
	FSceneOutlinerTreeItemPtr GetTreeItem(RowHandle InRow) const;

	static UE::Editor::DataStorage::ICoreProvider* GetDataStorage();
	static UE::Editor::DataStorage::ICoreProvider* GetDataStorageUI();
	static UE::Editor::DataStorage::ICompatibilityProvider* GetDataStorageCompatibility();

	static void GetSelectedRows(TArray<RowHandle>& OutSelectedRows);
};