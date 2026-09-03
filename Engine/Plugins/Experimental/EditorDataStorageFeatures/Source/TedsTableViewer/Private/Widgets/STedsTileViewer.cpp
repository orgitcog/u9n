// Copyright Epic Games, Inc. All Rights Reserved.


#include "Widgets/STedsTileViewer.h"

#include "Brushes/SlateColorBrush.h"
#include "Columns/SlateDelegateColumns.h"
#include "Elements/Columns/TypedElementUIColumns.h"
#include "Elements/Common/EditorDataStorageFeatures.h"
#include "Elements/Framework/TypedElementAttributeBinding.h"
#include "Elements/Framework/TypedElementDataStorageWidget.h"
#include "TedsTableViewerColumn.h"
#include "TedsTableViewerWidgetColumns.h"
#include "Elements/Columns/TypedElementHiearchyColumns.h"
#include "Styling/StyleColors.h"
#include "Widgets/STedsTableViewerRow.h"

namespace UE::Editor::DataStorage
{
	void STedsTileViewer::Construct(const FArguments& InArgs)
	{
		OnSelectionChanged = InArgs._OnSelectionChanged;
		WidgetPurpose = InArgs._WidgetPurpose;
		Columns = InArgs._Columns;
		TableRowStyle = InArgs._TileStyle;
		TilePadding = InArgs._TilePadding;

		// Use the default purpose if the user didn't specify any
		// TODO: Have a better "default" widget for tiles instead of the default cell widget purpose
		if (!WidgetPurpose.IsSet())
		{
			IUiProvider* StorageUi = GetMutableDataStorageFeature<IUiProvider>(UiFeatureName);
			WidgetPurpose = StorageUi->GetGeneralWidgetPurposeID();
		}
		
		// While we might have columns for the widget constructor, we don't want to display any UI columns since that doesn't make sense
		// so we give FTedsTableViewerModel empty dummy columns and purposes
		IUiProvider::FPurposeID DummyPurpose;
		TArray<TWeakObjectPtr<const UScriptStruct>> DummyColumns;
		
		Model = MakeShared<FTedsTableViewerModel>(InArgs._QueryStack, DummyColumns, DummyPurpose, DummyPurpose,
			FTedsTableViewerModel::FIsItemVisible::CreateSP(this, &STedsTileViewer::IsItemVisible));

		IUiProvider* StorageUi = Model->GetDataStorageUiProvider();
		
		if (ensure(StorageUi))
		{
			TedsWidget = StorageUi->CreateContainerTedsWidget(InvalidRowHandle);
			
			ChildSlot
			[
				TedsWidget->AsWidget()
			];
		}

		AddWidgetColumns();
		
		// Attribute binder to bind widget columns to attributes on the ListView
		FAttributeBinder Binder(TedsWidget->GetRowHandle());

		TileView = SNew(STileView<TableViewerItemPtr>)
			.ItemAlignment(InArgs._ItemAlignment)
			.ListItemsSource(&Model->GetItems())
			.OnGenerateTile(this, &STedsTileViewer::MakeTileWidget)
			.OnSelectionChanged(this, &STedsTileViewer::OnListSelectionChanged)
			.ItemWidth(InArgs._ItemWidth)
			.ItemHeight(InArgs._ItemHeight)
			.SelectionMode(InArgs._SelectionMode)
			.OnContextMenuOpening(Binder.BindEvent(&FWidgetContextMenuColumn::OnContextMenuOpening))
			.OnItemScrolledIntoView(Binder.BindEvent(&FWidgetRowScrolledIntoView::OnItemScrolledIntoView))
			.OnMouseButtonDoubleClick(Binder.BindEvent(&FWidgetDoubleClickedColumn::OnMouseButtonDoubleClick));

		TedsWidget->SetContent(TileView.ToSharedRef());
		
		// Whenever the model changes, refresh the list to update the UI
		Model->GetOnModelChanged().AddLambda([this]()
		{
			TileView->RequestListRefresh();
		});

		CreateTileWidgetConstructor();
	}

	void STedsTileViewer::CreateTileWidgetConstructor()
	{
		// We're going to use a custom column to represent our tile so we can use FTedsTableViewerModel with STileView which doesn't have the concept
		// of columns
		TSharedPtr<FTedsTableViewerColumn> Column;
		
		auto AssignWidgetToColumn = [&Column](TUniquePtr<FTypedElementWidgetConstructor> Constructor, TConstArrayView<TWeakObjectPtr<const UScriptStruct>> InMatchedColumns)
		{
			TArray<TWeakObjectPtr<const UScriptStruct>> MatchedColumnsCopy(InMatchedColumns);
			TSharedPtr<FTypedElementWidgetConstructor> WidgetConstructor(Constructor.Release());
			Column = MakeShared<FTedsTableViewerColumn>(TEXT("TileView"), WidgetConstructor, MatchedColumnsCopy);
			return false;
		};
	
		IUiProvider* StorageUi = GetMutableDataStorageFeature<IUiProvider>(UiFeatureName);
		
		RowHandle WidgetPurposeRow = StorageUi->FindPurpose(WidgetPurpose);

		if (Columns.IsEmpty())
		{
			StorageUi->CreateWidgetConstructors(WidgetPurposeRow, FMetaDataView(), AssignWidgetToColumn);
		}
		else
		{
			StorageUi->CreateWidgetConstructors(WidgetPurposeRow, IUiProvider::EMatchApproach::ExactMatch, Columns,
				FMetaDataView(), AssignWidgetToColumn);
		}

		if (Column)
		{
			Model->AddCustomColumn(Column.ToSharedRef());
		}
	}

	void STedsTileViewer::ForEachSelectedRow(TFunctionRef<void(RowHandle)> InCallback) const
	{
		TArray<TableViewerItemPtr> SelectedRows;
		TileView->GetSelectedItems(SelectedRows);

		for(TableViewerItemPtr& Row : SelectedRows)
		{
			InCallback(Row);
		}
	}

	RowHandle STedsTileViewer::GetWidgetRowHandle() const
	{
		return TedsWidget->GetRowHandle();
	}

	void STedsTileViewer::SetSelection(RowHandle Row, bool bSelected, const ESelectInfo::Type SelectInfo) const
	{
		FTedsRowHandle TedsRowHandle{ .RowHandle = Row };
		TileView->SetItemSelection(TedsRowHandle, bSelected, SelectInfo);

	}

	void STedsTileViewer::ScrollIntoView(RowHandle Row) const
	{
		FTedsRowHandle TedsRowHandle{ .RowHandle = Row };
		TileView->RequestScrollIntoView(TedsRowHandle);
	}

	void STedsTileViewer::ClearSelection() const
	{
		TileView->ClearSelection();
	}

	TSharedRef<SWidget> STedsTileViewer::AsWidget()
	{
		return AsShared();
	}

	bool STedsTileViewer::IsSelected(RowHandle InRow) const
	{
		FTedsRowHandle TedsRowHandle{ .RowHandle = InRow };
		return TileView->IsItemSelected(TedsRowHandle);
	}

	bool STedsTileViewer::IsSelectedExclusively(RowHandle InRow) const
	{
		return IsSelected(InRow) && TileView->GetNumItemsSelected() == 1;
	}

	TSharedRef<ITableRow> STedsTileViewer::MakeTileWidget(TableViewerItemPtr InItem,
	                                                      const TSharedRef<STableViewBase>& OwnerTable) const
	{
		TSharedPtr<SWidget> RowWidget = SNullWidget::NullWidget;
		
		if (TSharedPtr<FTedsTableViewerColumn> Column = Model->GetColumn("TileView"))
		{
			RowWidget = Column->ConstructRowWidget(InItem,
				[&](ICoreProvider& DataStorage, const RowHandle& WidgetRow)
				{
					DataStorage.AddColumn(WidgetRow, FTableRowParentColumn{ .Parent = GetWidgetRowHandle() });
					DataStorage.AddColumn(WidgetRow, FExternalWidgetSelectionColumn{ .IsSelected = FIsSelected::CreateSP(this, &STedsTileViewer::IsSelected, InItem.RowHandle) });
					DataStorage.AddColumn(WidgetRow, FExternalWidgetExclusiveSelectionColumn{ .IsSelectedExclusively = FIsSelected::CreateSP(this, &STedsTileViewer::IsSelectedExclusively, InItem.RowHandle) });
				});
		}

		return SNew(STableRow<TableViewerItemPtr>, OwnerTable)
			.Padding(TilePadding)
			.Style(TableRowStyle)
			.Cursor(bAllowDragging ? EMouseCursor::GrabHand : EMouseCursor::Default)
			[
				RowWidget.ToSharedRef()
			];
	}

	bool STedsTileViewer::IsItemVisible(TableViewerItemPtr InItem) const
	{
		return TileView->IsItemVisible(InItem);
	}

	void STedsTileViewer::OnListSelectionChanged(TableViewerItemPtr Item, ESelectInfo::Type SelectInfo)
	{
		if(OnSelectionChanged.IsBound())
		{
			OnSelectionChanged.Execute(Item);
		}
	}

	void STedsTileViewer::AddWidgetColumns()
	{
		if(ICoreProvider* DataStorage = Model->GetDataStorageInterface())
		{
			const RowHandle WidgetRowHandle = TedsWidget->GetRowHandle();

			if (DataStorage->IsRowAvailable(WidgetRowHandle))
			{
				// The table viewer should not show up as a row in a table viewer because that will cause all sorts of recursion issues
				DataStorage->AddColumn(WidgetRowHandle, FHideRowFromUITag::StaticStruct());

				// Columns we are going to bind to attributes on SListView
				DataStorage->AddColumn(WidgetRowHandle, FWidgetContextMenuColumn::StaticStruct());
				DataStorage->AddColumn(WidgetRowHandle, FWidgetRowScrolledIntoView::StaticStruct());
				DataStorage->AddColumn(WidgetRowHandle, FWidgetDoubleClickedColumn::StaticStruct());
			}
		}
	}
}


