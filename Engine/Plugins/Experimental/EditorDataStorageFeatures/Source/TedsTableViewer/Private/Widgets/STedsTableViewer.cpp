// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/STedsTableViewer.h"

#include "TedsTableViewerColumn.h"
#include "Columns/SlateDelegateColumns.h"
#include "Elements/Columns/TypedElementUIColumns.h"
#include "Elements/Common/EditorDataStorageFeatures.h"
#include "Elements/Framework/TypedElementAttributeBinding.h"
#include "Elements/Framework/TypedElementDataStorageWidget.h"
#include "Widgets/STedsTableViewerRow.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "STedsTableViewer"

namespace UE::Editor::DataStorage
{
	void STedsTableViewer::Construct(const FArguments& InArgs)
	{
		OnSelectionChanged = InArgs._OnSelectionChanged;
		EmptyRowsMessage = InArgs._EmptyRowsMessage;
		ItemHeight = InArgs._ItemHeight;
		ItemPadding = InArgs._ItemPadding;

		IUiProvider::FPurposeID CellWigetPurpose = InArgs._CellWidgetPurpose;

		if (!CellWigetPurpose.IsSet())
		{
			IUiProvider* StorageUi = GetMutableDataStorageFeature<IUiProvider>(UiFeatureName);
			CellWigetPurpose = StorageUi->GetGeneralWidgetPurposeID();
		}

		IUiProvider::FPurposeID HeaderWidgetPurpose = InArgs._HeaderWidgetPurpose;

		if (!HeaderWidgetPurpose.IsSet())
		{
			HeaderWidgetPurpose = IUiProvider::FPurposeInfo("General", "Header", NAME_None).GeneratePurposeID();
		}
		
		Model = MakeShared<FTedsTableViewerModel>(InArgs._QueryStack, InArgs._Columns, CellWigetPurpose, HeaderWidgetPurpose,
			FTedsTableViewerModel::FIsItemVisible::CreateSP(this, &STedsTableViewer::IsItemVisible));
		
		HeaderRowWidget = SNew( SHeaderRow )
							.CanSelectGeneratedColumn(true);
		
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

		ListView = SNew(SListView<TableViewerItemPtr>)
			.HeaderRow(HeaderRowWidget)
			.ListItemsSource(&Model->GetItems())
			.OnGenerateRow(this, &STedsTableViewer::MakeTableRowWidget)
			.OnSelectionChanged(this, &STedsTableViewer::OnListSelectionChanged)
			.SelectionMode(InArgs._ListSelectionMode)
			.OnContextMenuOpening(Binder.BindEvent(&FWidgetContextMenuColumn::OnContextMenuOpening))
			.OnItemScrolledIntoView(Binder.BindEvent(&FWidgetRowScrolledIntoView::OnItemScrolledIntoView))
			.OnMouseButtonDoubleClick(Binder.BindEvent(&FWidgetDoubleClickedColumn::OnMouseButtonDoubleClick));

		CreateInternalWidget();
		
		// Add each Teds column from the model to our header row widget
		Model->ForEachColumn([this](const TSharedRef<FTedsTableViewerColumn>& Column)
		{
			HeaderRowWidget->AddColumn(Column->ConstructHeaderRowColumn());
		});

		// Whenever the model changes, refresh the list to update the UI
		Model->GetOnModelChanged().AddLambda([this]()
		{
			ListView->RequestListRefresh();
			CreateInternalWidget();
		});
	}

	void STedsTableViewer::AddWidgetColumns()
	{
		if(ICoreProvider* DataStorage = Model->GetDataStorageInterface())
		{
			const RowHandle WidgetRowHandle = TedsWidget->GetRowHandle();
		
			if(DataStorage->IsRowAvailable(WidgetRowHandle))
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

	void STedsTableViewer::CreateInternalWidget()
	{
		TSharedPtr<SWidget> ContentWidget;

		// If there are no rows and the table viewer wants to show a message
		if(Model->GetRowCount() == 0 && EmptyRowsMessage.IsSet())
		{
			ContentWidget = 
				SNew(SBox)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
							.Text(EmptyRowsMessage)
					];
		}
		else if(Model->GetColumnCount() == 0)
		{
			ContentWidget =
				SNew(SBox)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
							.Text(LOCTEXT("EmptyTableViewerColumnsText", "No columns found to display."))
					];
		}
		else
		{
			ContentWidget = ListView.ToSharedRef();
		}

		TedsWidget->SetContent(ContentWidget.ToSharedRef());
	}

	void STedsTableViewer::RefreshColumnWidgets()
	{
		HeaderRowWidget->ClearColumns();
		Model->ForEachColumn([this](const TSharedRef<FTedsTableViewerColumn>& Column)
		{
			HeaderRowWidget->AddColumn(Column->ConstructHeaderRowColumn());
		});

		CreateInternalWidget();
	}

	void STedsTableViewer::OnListSelectionChanged(TableViewerItemPtr Item, ESelectInfo::Type SelectInfo)
	{
		if(OnSelectionChanged.IsBound())
		{
			OnSelectionChanged.Execute(Item);
		}
	}

	void STedsTableViewer::SetColumns(const TArray<TWeakObjectPtr<const UScriptStruct>>& Columns)
	{
		Model->SetColumns(Columns);
		RefreshColumnWidgets();
	}

	void STedsTableViewer::AddCustomColumn(const TSharedRef<FTedsTableViewerColumn>& InColumn)
	{
		Model->AddCustomColumn(InColumn);
		RefreshColumnWidgets();
	}

	void STedsTableViewer::ForEachSelectedRow(TFunctionRef<void(RowHandle)> InCallback) const
	{
		TArray<TableViewerItemPtr> SelectedRows;
		ListView->GetSelectedItems(SelectedRows);

		for(TableViewerItemPtr& Row : SelectedRows)
		{
			InCallback(Row);
		}
	}

	RowHandle STedsTableViewer::GetWidgetRowHandle() const
	{
		return TedsWidget->GetRowHandle();
	}

	void STedsTableViewer::SetSelection(RowHandle Row, bool bSelected, const ESelectInfo::Type SelectInfo) const
	{
		FTedsRowHandle TedsRowHandle{ .RowHandle = Row };
		ListView->SetItemSelection(TedsRowHandle, bSelected, SelectInfo);
	}

	void STedsTableViewer::ScrollIntoView(RowHandle Row) const
	{
		FTedsRowHandle TedsRowHandle{ .RowHandle = Row };
		ListView->RequestScrollIntoView(TedsRowHandle);
	}

	void STedsTableViewer::ClearSelection() const
	{
		ListView->ClearSelection();
	}

	TSharedRef<SWidget> STedsTableViewer::AsWidget()
	{
		return AsShared();
	}

	bool STedsTableViewer::IsSelected(RowHandle InRow) const
	{
		FTedsRowHandle TedsRowHandle{ .RowHandle = InRow };
		return ListView->IsItemSelected(TedsRowHandle);
	}

	bool STedsTableViewer::IsSelectedExclusively(RowHandle InRow) const
	{
		return IsSelected(InRow) && ListView->GetNumItemsSelected() == 1;
	}

	bool STedsTableViewer::IsItemVisible(TableViewerItemPtr InItem) const
	{
		return ListView->IsItemVisible(InItem);
	}

	TSharedRef<ITableRow> STedsTableViewer::MakeTableRowWidget(TableViewerItemPtr InItem, const TSharedRef<STableViewBase>& OwnerTable) const
	{
		return SNew(STedsTableViewerRow, OwnerTable, Model.ToSharedRef())
				.ItemHeight(ItemHeight)
				.Padding(ItemPadding)
				.ParentWidgetRowHandle(GetWidgetRowHandle())
				.Item(InItem);
	}
} // namespace UE::Editor::DataStorage

#undef LOCTEXT_NAMESPACE //"STedsTableViewer"

