// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/STedsHierarchyViewer.h"

#include "TedsTableViewerColumn.h"
#include "Columns/SlateDelegateColumns.h"
#include "Elements/Columns/TypedElementHiearchyColumns.h"
#include "Elements/Columns/TypedElementUIColumns.h"
#include "Elements/Common/EditorDataStorageFeatures.h"
#include "Elements/Framework/TypedElementAttributeBinding.h"
#include "Elements/Framework/TypedElementDataStorageWidget.h"
#include "Widgets/STedsTableViewerRow.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/STedsTreeView.h"

#define LOCTEXT_NAMESPACE "SHierarchyViewer"

namespace UE::Editor::DataStorage
{
	void SHierarchyViewer::Construct(const FArguments& InArgs)
	{
		OnSelectionChanged = InArgs._OnSelectionChanged;
		EmptyRowsMessage = InArgs._EmptyRowsMessage;
		
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
			FTedsTableViewerModel::FIsItemVisible::CreateSP(this, &SHierarchyViewer::IsItemVisible));

		constexpr bool bRowsShouldHaveColumn = false;
		HierarchyNode = MakeUnique<QueryStack::FRowFilterNode<FTableRowParentColumn>>(Model->GetDataStorageInterface(), InArgs._QueryStack, bRowsShouldHaveColumn);
		
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

		TreeView = SNew(STedsTreeView, STedsTreeView::FOnGetParent::CreateSP(this, &SHierarchyViewer::GetParentRow))
			.HeaderRow(HeaderRowWidget)
			.TopLevelRowsSource(&TopLevelRows)
			.RowsSource(&Model->GetItems())
			.OnGenerateRow(this, &SHierarchyViewer::MakeTableRowWidget)
			.OnSelectionChanged(this, &SHierarchyViewer::OnListSelectionChanged)
			.SelectionMode(InArgs._ListSelectionMode);

		CreateInternalWidget();
		
		// Add each Teds column from the model to our header row widget
		Model->ForEachColumn([this](const TSharedRef<FTedsTableViewerColumn>& Column)
		{
			HeaderRowWidget->AddColumn(Column->ConstructHeaderRowColumn());
		});

		// Whenever the model changes, refresh the list to update the UI
		Model->GetOnModelChanged().AddRaw(this, &SHierarchyViewer::OnModelChanged);
	}

	void SHierarchyViewer::OnModelChanged()
	{
		TopLevelRows.Empty();

		HierarchyNode->Update();
		
		FRowHandleArrayView Rows = HierarchyNode->GetRows();

		for(const RowHandle RowHandle : Rows)
		{
			if(Model->IsRowDisplayable(RowHandle))
			{
				FTedsRowHandle TedsRowHandle{ .RowHandle = RowHandle };
				TopLevelRows.Add(TedsRowHandle);
			}
		}
		
		TreeView->RequestListRefresh();
		CreateInternalWidget();
	}

	void SHierarchyViewer::AddWidgetColumns()
	{
		if(ICoreProvider* DataStorage = Model->GetDataStorageInterface())
		{
			const RowHandle WidgetRowHandle = TedsWidget->GetRowHandle();
		
			if(DataStorage->IsRowAvailable(WidgetRowHandle))
			{
				// FHideRowFromUITag - The table viewer should not show up as a row in a table viewer because that will cause all sorts of recursion issues
				// The others are Columns we are going to bind to attributes on STreeView
				DataStorage->AddColumns<FHideRowFromUITag, FWidgetContextMenuColumn, FWidgetRowScrolledIntoView, FWidgetDoubleClickedColumn>(WidgetRowHandle);
			}
		}
	}

	TableViewerItemPtr SHierarchyViewer::GetParentRow(TableViewerItemPtr InItem) const
	{
		if (FTableRowParentColumn* ParentColumn = Model->GetDataStorageInterface()->GetColumn<FTableRowParentColumn>(InItem))
		{
			return FTedsRowHandle{.RowHandle = ParentColumn->Parent};
		}

		return FTedsRowHandle{.RowHandle = InvalidRowHandle};
	}

	void SHierarchyViewer::CreateInternalWidget()
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
			ContentWidget = TreeView.ToSharedRef();
		}

		TedsWidget->SetContent(ContentWidget.ToSharedRef());
	}

	void SHierarchyViewer::RefreshColumnWidgets()
	{
		HeaderRowWidget->ClearColumns();
		Model->ForEachColumn([this](const TSharedRef<FTedsTableViewerColumn>& Column)
		{
			HeaderRowWidget->AddColumn(Column->ConstructHeaderRowColumn());
		});

		CreateInternalWidget();
	}

	void SHierarchyViewer::OnListSelectionChanged(TableViewerItemPtr Item, ESelectInfo::Type SelectInfo)
	{
		if(OnSelectionChanged.IsBound())
		{
			OnSelectionChanged.Execute(Item);
		}
	}

	void SHierarchyViewer::SetColumns(const TArray<TWeakObjectPtr<const UScriptStruct>>& Columns)
	{
		Model->SetColumns(Columns);
		RefreshColumnWidgets();
	}

	void SHierarchyViewer::AddCustomColumn(const TSharedRef<FTedsTableViewerColumn>& InColumn)
	{
		Model->AddCustomColumn(InColumn);
		RefreshColumnWidgets();
	}

	void SHierarchyViewer::ForEachSelectedRow(TFunctionRef<void(RowHandle)> InCallback) const
	{
		TArray<TableViewerItemPtr> SelectedRows;
		TreeView->GetSelectedItems(SelectedRows);

		for(TableViewerItemPtr& Row : SelectedRows)
		{
			InCallback(Row);
		}
	}

	RowHandle SHierarchyViewer::GetWidgetRowHandle() const
	{
		return TedsWidget->GetRowHandle();
	}

	void SHierarchyViewer::SetSelection(RowHandle Row, bool bSelected, const ESelectInfo::Type SelectInfo) const
	{
		FTedsRowHandle TedsRowHandle{ .RowHandle = Row };
		TreeView->SetItemSelection(TedsRowHandle, bSelected, SelectInfo);
	}

	void SHierarchyViewer::ScrollIntoView(RowHandle Row) const
	{
		FTedsRowHandle TedsRowHandle{ .RowHandle = Row };
		TreeView->RequestScrollIntoView(TedsRowHandle);
	}

	void SHierarchyViewer::ClearSelection() const
	{
		TreeView->ClearSelection();
	}

	TSharedRef<SWidget> SHierarchyViewer::AsWidget()
	{
		return AsShared();
	}

	bool SHierarchyViewer::IsSelected(RowHandle InRow) const
	{
		FTedsRowHandle TedsRowHandle{ .RowHandle = InRow };
		return TreeView->IsItemSelected(TedsRowHandle);
	}

	bool SHierarchyViewer::IsSelectedExclusively(RowHandle InRow) const
	{
		return IsSelected(InRow) && TreeView->GetNumItemsSelected() == 1;
	}

	bool SHierarchyViewer::IsItemVisible(TableViewerItemPtr InItem) const
	{
		return TreeView->IsItemVisible(InItem);
	}

	TSharedRef<ITableRow> SHierarchyViewer::MakeTableRowWidget(TableViewerItemPtr InItem, const TSharedRef<STableViewBase>& OwnerTable) const
	{
		return SNew(SHierarchyViewerRow, OwnerTable, Model.ToSharedRef())
				.Item(InItem);
	}
} // namespace UE::Editor::DataStorage

#undef LOCTEXT_NAMESPACE //"SHierarchyViewer"

