// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/OutlinerVisibilityWidget.h"

#include "Columns/SlateHeaderColumns.h"
#include "Columns/TedsOutlinerColumns.h"
#include "Compatibility/SceneOutlinerTedsBridge.h"
#include "Elements/Columns/TypedElementCompatibilityColumns.h"
#include "Elements/Columns/TypedElementHiearchyColumns.h"
#include "Elements/Columns/TypedElementMiscColumns.h"
#include "Elements/Columns/TypedElementSelectionColumns.h"
#include "Elements/Columns/TypedElementVisibilityColumns.h"
#include "Elements/Common/EditorDataStorageFeatures.h"
#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "Elements/Interfaces/TypedElementDataStorageCompatibilityInterface.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "Widgets/SBoxPanel.h"
#include "ISceneOutliner.h"
#include "ActorTreeItem.h"
#include "TedsOutlinerItem.h"

#define LOCTEXT_NAMESPACE "OutlinerVisibilityWidget"

//
// Cell Factory
//

FOutlinerVisibilityWidgetConstructor::FOutlinerVisibilityWidgetConstructor()
	: FSimpleWidgetConstructor(StaticStruct())
{
}

void UOutlinerVisibilityWidgetFactory::RegisterWidgetConstructors(UE::Editor::DataStorage::ICoreProvider& DataStorage,
	UE::Editor::DataStorage::IUiProvider& DataStorageUi) const
{
	using namespace UE::Editor::DataStorage::Queries;
	DataStorageUi.RegisterWidgetFactory<FOutlinerVisibilityWidgetConstructor>(
		DataStorageUi.FindPurpose(IUiProvider::FPurposeInfo("SceneOutliner", "Cell", NAME_None).GeneratePurposeID()),
		TColumn<FVisibleInEditorColumn>());
	
}

TSharedPtr<SWidget> FOutlinerVisibilityWidgetConstructor::CreateWidget(UE::Editor::DataStorage::ICoreProvider* DataStorage,
	UE::Editor::DataStorage::IUiProvider* DataStorageUi,
	UE::Editor::DataStorage::RowHandle TargetRow, UE::Editor::DataStorage::RowHandle WidgetRow, const UE::Editor::DataStorage::FMetaDataView& Arguments)
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(STedsVisibilityWidget, TargetRow, WidgetRow)
				.ToolTipText(LOCTEXT("SceneOutlinerVisibilityToggleTooltip", "Toggles the visibility of this object in the level editor."))
		];
}

//
// Header Factory
//

FOutlinerVisibilityHeaderConstructor::FOutlinerVisibilityHeaderConstructor()
	: FSimpleWidgetConstructor(StaticStruct())
{
}

void UOutlinerVisibilityHeaderFactory::RegisterWidgetConstructors(UE::Editor::DataStorage::ICoreProvider& DataStorage,
	UE::Editor::DataStorage::IUiProvider& DataStorageUi) const
{
	using namespace UE::Editor::DataStorage::Queries;
	DataStorageUi.RegisterWidgetFactory<FOutlinerVisibilityHeaderConstructor>(
		DataStorageUi.FindPurpose(IUiProvider::FPurposeInfo("SceneOutliner", "Header", NAME_None).GeneratePurposeID()), TColumn<FVisibleInEditorColumn>());
}

TSharedPtr<SWidget> FOutlinerVisibilityHeaderConstructor::CreateWidget(UE::Editor::DataStorage::ICoreProvider* DataStorage,
	UE::Editor::DataStorage::IUiProvider* DataStorageUi,
	UE::Editor::DataStorage::RowHandle TargetRow, UE::Editor::DataStorage::RowHandle WidgetRow, const UE::Editor::DataStorage::FMetaDataView& Arguments)
{
	DataStorage->AddColumn(WidgetRow, FHeaderWidgetSizeColumn
	{
		.ColumnSizeMode = EColumnSizeMode::Fixed,
		.Width = 24.0f
	});

	return SNew(SImage)
				.DesiredSizeOverride(FVector2D(16.f, 16.f))
				.Image(FAppStyle::Get().GetBrush(TEXT("Level.VisibleHighlightIcon16x")))
				.ToolTipText(LOCTEXT("SceneOutlinerVisibilityHeaderTooltip", "Visibility"));
}

//
// SVisibilityWidget
//

class FTEDSVisibilityDragDropOp : public FDragDropOperation
{
public:

	DRAG_DROP_OPERATOR_TYPE(FTEDSVisibilityDragDropOp, FDragDropOperation)

	/** Flag which defines whether to hide destination items or not */
	bool bHidden;

	/** Undo transaction stolen from the gutter which is kept alive for the duration of the drag */
	TUniquePtr<FScopedTransaction> UndoTransaction;

	/** The widget decorator to use */
	virtual TSharedPtr<SWidget> GetDefaultDecorator() const override
	{
		return SNullWidget::NullWidget;
	}

	/** Create a new drag and drop operation out of the specified flag */
	static TSharedRef<FTEDSVisibilityDragDropOp> New(const bool _bHidden, TUniquePtr<FScopedTransaction>& ScopedTransaction)
	{
		TSharedRef<FTEDSVisibilityDragDropOp> Operation = MakeShareable(new FTEDSVisibilityDragDropOp);

		Operation->bHidden = _bHidden;
		Operation->UndoTransaction = MoveTemp(ScopedTransaction);

		Operation->Construct();
		return Operation;
	}
};

void STedsVisibilityWidget::Construct(const FArguments& InArgs, const RowHandle& InTargetRow, const RowHandle& InWidgetRow)
{
	TargetRow = InTargetRow;
	WidgetRow = InWidgetRow;

	SImage::Construct(
		SImage::FArguments()
		.IsEnabled(this, &STedsVisibilityWidget::IsEnabled)
		.ColorAndOpacity(this, &STedsVisibilityWidget::GetForegroundColor)
		.Image(this, &STedsVisibilityWidget::GetBrush)
	);


	static const FName NAME_VisibleHoveredBrush = TEXT("Level.VisibleHighlightIcon16x");
	static const FName NAME_VisibleNotHoveredBrush = TEXT("Level.VisibleIcon16x");
	static const FName NAME_NotVisibleHoveredBrush = TEXT("Level.NotVisibleHighlightIcon16x");
	static const FName NAME_NotVisibleNotHoveredBrush = TEXT("Level.NotVisibleIcon16x");

	VisibleHoveredBrush = FAppStyle::Get().GetBrush(NAME_VisibleHoveredBrush);
	VisibleNotHoveredBrush = FAppStyle::Get().GetBrush(NAME_VisibleNotHoveredBrush);

	NotVisibleHoveredBrush = FAppStyle::Get().GetBrush(NAME_NotVisibleHoveredBrush);
	NotVisibleNotHoveredBrush = FAppStyle::Get().GetBrush(NAME_NotVisibleNotHoveredBrush);

}

FReply STedsVisibilityWidget::OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
	{
		return FReply::Handled().BeginDragDrop(FTEDSVisibilityDragDropOp::New(!IsVisible(), UndoTransaction));
	}
	else
	{
		return FReply::Unhandled();
	}
}

/** If a visibility drag drop operation has entered this widget, set its item to the new visibility state */
void STedsVisibilityWidget::OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	auto VisibilityOp = DragDropEvent.GetOperationAs<FTEDSVisibilityDragDropOp>();
	if (VisibilityOp.IsValid())
	{
		SetIsVisible(TargetRow, !VisibilityOp->bHidden);
	}
}

FReply STedsVisibilityWidget::HandleClick()
{
	if (!IsEnabled())
	{
		return FReply::Unhandled();
	}

	// Open an undo transaction
	UndoTransaction.Reset(new FScopedTransaction(LOCTEXT("SetOutlinerItemVisibility", "Set Item Visibility")));

	const bool bVisible = !IsVisible();

	TArray<RowHandle> SelectedRows;
	GetSelectedRows(SelectedRows);

	if (SelectedRows.Contains(TargetRow))
	{
		for (RowHandle Row : SelectedRows)
		{
			SetIsVisible(Row, bVisible);
		}
	}
	else
	{
		SetIsVisible(TargetRow, bVisible);
	}

	return FReply::Handled().DetectDrag(SharedThis(this), EKeys::LeftMouseButton);
}

FReply STedsVisibilityWidget::OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	return HandleClick();
}

/** Called when the mouse button is pressed down on this widget */
FReply STedsVisibilityWidget::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}

	return HandleClick();
}

/** Process a mouse up message */
FReply STedsVisibilityWidget::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		UndoTransaction.Reset();
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

/** Called when this widget had captured the mouse, but that capture has been revoked for some reason. */
void STedsVisibilityWidget::OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	UndoTransaction.Reset();
}

/** Get the brush for this widget */
const FSlateBrush* STedsVisibilityWidget::GetBrush() const
{
	if (IsVisible())
	{
		return IsHovered() ? VisibleHoveredBrush : VisibleNotHoveredBrush;
	}
	else
	{
		return IsHovered() ? NotVisibleHoveredBrush : NotVisibleNotHoveredBrush;
	}
}

FSlateColor STedsVisibilityWidget::GetForegroundColor() const
{
	const bool bIsSelected = IsSelected();
	const bool bIsVisible = IsVisible();
	const bool bIsHovered = IsHovered();

	// make the foreground brush transparent if it is not selected and it is visible
	if (bIsVisible && !bIsHovered && !bIsSelected)
	{
		return FLinearColor::Transparent;
	}
	else if (bIsHovered && !bIsSelected)
	{
		return FAppStyle::Get().GetSlateColor("Colors.ForegroundHover");
	}

	return FSlateColor::UseForeground();
}

/** Check if our wrapped tree item is visible */
bool STedsVisibilityWidget::IsVisible() const
{
	if (UE::Editor::DataStorage::ICoreProvider* DataStorage = GetDataStorage())
	{
		FVisibleInEditorColumn* HiddenInEditorColumn = DataStorage->GetColumn<FVisibleInEditorColumn>(TargetRow);
		if (HiddenInEditorColumn)
		{
			return HiddenInEditorColumn->bIsVisibleInEditor;
		}
	}

	return true;
}

bool STedsVisibilityWidget::IsSelected() const
{
	if (UE::Editor::DataStorage::ICoreProvider* DataStorage = GetDataStorage())
	{
		return DataStorage->HasColumns<FTypedElementSelectionColumn>(TargetRow);
	}

	return false;
}

/** Set the item this widget is responsible for to be hidden or shown */
void STedsVisibilityWidget::SetIsVisible(RowHandle InRow, const bool bVisible)
{
	TWeakPtr<ISceneOutlinerTreeItem> RowTreeItem = GetTreeItem(InRow);
	if (UE::Editor::DataStorage::ICoreProvider* DataStorage = GetDataStorage(); RowTreeItem.IsValid())
	{
		SetVisibility_Recursive(*DataStorage, RowTreeItem.Pin(), bVisible);
	}
}

void STedsVisibilityWidget::CommitVisibility(UE::Editor::DataStorage::ICoreProvider& DataStorage, RowHandle Row, bool bVisible)
{
	// Set new visibility on the TargetRow
	FVisibleInEditorColumn* VisibileInEditorColumn = DataStorage.GetColumn<FVisibleInEditorColumn>(Row);
	if (VisibileInEditorColumn)
	{
		VisibileInEditorColumn->bIsVisibleInEditor = bVisible;
	}
	DataStorage.AddColumn<FTypedElementSyncBackToWorldTag>(Row);
}

void STedsVisibilityWidget::SetVisibility_Recursive(UE::Editor::DataStorage::ICoreProvider& DataStorage, FSceneOutlinerTreeItemPtr TreeItem, bool bVisible)
{
	using namespace UE::Editor::DataStorage;
	using namespace UE::Editor::Outliner;

	if (ICompatibilityProvider* DataStorageCompat = GetDataStorageCompatibility())
	{
		if (FActorTreeItem* ActorTreeItem = TreeItem->CastTo<FActorTreeItem>())
		{
			RowHandle ActorRow = DataStorageCompat->FindRowWithCompatibleObject(ActorTreeItem->Actor);
			CommitVisibility(DataStorage, ActorRow, bVisible);
		}
		else if (FTedsOutlinerTreeItem* TedsItem = TreeItem->CastTo<FTedsOutlinerTreeItem>())
		{
			CommitVisibility(DataStorage, TedsItem->GetRowHandle(), bVisible);
		}
	}

	for (auto& ChildTreeItemPtr : TreeItem->GetChildren())
	{
		auto ChildTreeItem = ChildTreeItemPtr.Pin();
		if (ChildTreeItem.IsValid())
		{
			SetVisibility_Recursive(DataStorage, ChildTreeItem, bVisible);
		}
	}
}

FSceneOutlinerTreeItemPtr STedsVisibilityWidget::GetTreeItem(RowHandle InRow) const
{
	using namespace UE::Editor::DataStorage;
	ICoreProvider* DataStorage = GetDataStorage();
	if (!DataStorage)
	{
		return nullptr;
	}

	const FTedsOutlinerColumn* TEDSOutlinerColumn = DataStorage->GetColumn<FTedsOutlinerColumn>(WidgetRow);
	if (!TEDSOutlinerColumn)
	{
		return nullptr;
	}

	TSharedPtr<ISceneOutliner> Outliner = TEDSOutlinerColumn->Outliner.IsValid() ? TEDSOutlinerColumn->Outliner.Pin() : nullptr;
	if (!Outliner.IsValid())
	{
		return nullptr;
	}

	FSceneOutlinerTedsQueryBinder& Binder = FSceneOutlinerTedsQueryBinder::GetInstance();
	if (FTreeItemIDDealiaser Dealiaser = Binder.GetTreeItemIDDealiaser(Outliner); Dealiaser.IsBound())
	{
		FSceneOutlinerTreeItemID ID = Dealiaser.Execute(InRow);
		return Outliner->GetTreeItem(ID);
	}

	return nullptr;
}

UE::Editor::DataStorage::ICoreProvider* STedsVisibilityWidget::GetDataStorage()
{
	using namespace UE::Editor::DataStorage;
	return GetMutableDataStorageFeature<ICoreProvider>(StorageFeatureName);
}

UE::Editor::DataStorage::ICoreProvider* STedsVisibilityWidget::GetDataStorageUI()
{
	using namespace UE::Editor::DataStorage;
	return GetMutableDataStorageFeature<ICoreProvider>(UiFeatureName);
}

UE::Editor::DataStorage::ICompatibilityProvider* STedsVisibilityWidget::GetDataStorageCompatibility()
{
	using namespace UE::Editor::DataStorage;
	return GetMutableDataStorageFeature<ICompatibilityProvider>(CompatibilityFeatureName);
}

void STedsVisibilityWidget::GetSelectedRows(TArray<RowHandle>& OutSelectedRows)
{
	using namespace UE::Editor::DataStorage;
	using namespace UE::Editor::DataStorage::Queries;
	if (ICoreProvider* DataStorage = GetDataStorage())
	{
		static QueryHandle AllSelectedItemsQuery = DataStorage->RegisterQuery(Select().Where().All<FTypedElementSelectionColumn>().Compile());
		DataStorage->RunQuery(AllSelectedItemsQuery,
			CreateDirectQueryCallbackBinding([&OutSelectedRows, &DataStorage](const IDirectQueryContext& Context)
			{
				// Only add selections from the level editor - for now.
				// UE-231184: Support Visibility Column in multiple editor contexts
				for (RowHandle Row : Context.GetRowHandles())
				{
					FTypedElementSelectionColumn* Selection = DataStorage->GetColumn<FTypedElementSelectionColumn>(Row);
					if (Selection && Selection->SelectionSet.IsNone()) // SelectionSet.IsNone() == Level Editor
					{
						OutSelectedRows.Add(Row);
					}
				}
			}));
	}
}

#undef LOCTEXT_NAMESPACE