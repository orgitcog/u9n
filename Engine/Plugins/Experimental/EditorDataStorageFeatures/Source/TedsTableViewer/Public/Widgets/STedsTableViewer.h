// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "ITedsTableViewer.h"
#include "Elements/Common/TypedElementHandles.h"
#include "TedsQueryStackInterfaces.h"
#include "TedsTableViewerModel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/SListView.h"

class ITableRow;
class STableViewBase;
class SHeaderRow;

namespace UE::Editor::DataStorage
{
	class ITedsWidget;
}

namespace UE::Editor::DataStorage
{
	class FTedsTableViewerColumn;

	/*
	 * A table viewer widget can be used to show a visual representation of data in TEDS.
	 * The rows to display can be specified using a RowQueryStack, and the columns to display are directly input into the widget
	 * Example usage:
	 * 
	 *	SNew(STedsTableViewer)
     *		.QueryStack(MakeShared<UE::Editor::DataStorage::FQueryStackNode_RowView>(&Rows))
	 *		.Columns({FTypedElementLabelColumn::StaticStruct(), FTypedElementClassTypeInfoColumn::StaticStruct());
	 */
	class STedsTableViewer : public SCompoundWidget, public ITableViewer
	{
	public:
		
		// Delegate fired when the selection in the table viewer changes
		DECLARE_DELEGATE_OneParam(FOnSelectionChanged, RowHandle)

		SLATE_BEGIN_ARGS(STedsTableViewer)
			: _ListSelectionMode(ESelectionMode::Type::Single)
			, _ItemHeight()
			, _ItemPadding(FMargin(0.f))
		{
			
		}
		

		// Query Stack that will supply the rows to be displayed
		SLATE_ARGUMENT(TSharedPtr<QueryStack::IRowNode>, QueryStack)

		// The Columns that this table viewer will display
		// Table Viewer TODO: How do we specify column metadata (ReadOnly or ReadWrite)?
		SLATE_ARGUMENT(TArray<TWeakObjectPtr<const UScriptStruct>>, Columns)

		// The widget purpose to use to create the cell widgets
		SLATE_ARGUMENT(IUiProvider::FPurposeID, CellWidgetPurpose)
			
		// The widget purpose to use to create the header widgets
		SLATE_ARGUMENT(IUiProvider::FPurposeID, HeaderWidgetPurpose)
		
		// Delegate called when the selection changes
		SLATE_EVENT(FOnSelectionChanged, OnSelectionChanged)

		// The selection mode for the table viewer (single/multi etc)
		SLATE_ARGUMENT(ESelectionMode::Type, ListSelectionMode)

		// The message to show in place of the table viewer when there are no rows provided by the current query stack
		// Empty = simply show the column headers instead of a message
		SLATE_ATTRIBUTE(FText, EmptyRowsMessage)

		/** The height of the list item */
		SLATE_ATTRIBUTE(float, ItemHeight)

		/** The padding of the list item */
		SLATE_ATTRIBUTE(FMargin, ItemPadding)

		SLATE_END_ARGS()

	public:
		
		TEDSTABLEVIEWER_API void Construct(const FArguments& InArgs);

		// Clear the current list of columns being displayed and set it to the given list
		TEDSTABLEVIEWER_API void SetColumns(const TArray<TWeakObjectPtr<const UScriptStruct>>& Columns);

		// Add a custom column to display in the table viewer, that doesn't necessarily map to a Teds column
		TEDSTABLEVIEWER_API void AddCustomColumn(const TSharedRef<FTedsTableViewerColumn>& InColumn);

		// Execute the given callback for each row that is selected in the table viewer
		TEDSTABLEVIEWER_API virtual void ForEachSelectedRow(TFunctionRef<void(RowHandle)> InCallback) const override;

		// Get the row handle for the widget row the table viewer's contents are stored in
		TEDSTABLEVIEWER_API virtual RowHandle GetWidgetRowHandle() const override;
		
		// Select the given row in the table viewer
		TEDSTABLEVIEWER_API virtual void SetSelection(RowHandle Row, bool bSelected, const ESelectInfo::Type SelectInfo) const override;

		// Scroll the given row into view in the table viewer
		TEDSTABLEVIEWER_API virtual void ScrollIntoView(RowHandle Row) const override;

		TEDSTABLEVIEWER_API virtual void ClearSelection() const override;
		
		TEDSTABLEVIEWER_API virtual TSharedRef<SWidget> AsWidget() override;

		TEDSTABLEVIEWER_API virtual bool IsSelected(RowHandle InRow) const override;

		TEDSTABLEVIEWER_API virtual bool IsSelectedExclusively(RowHandle InRow) const override;
	protected:
		
		TSharedRef<ITableRow> MakeTableRowWidget(TableViewerItemPtr InItem, const TSharedRef<STableViewBase>& OwnerTable) const;

		bool IsItemVisible(TableViewerItemPtr InItem) const;

		void CreateInternalWidget();

		void RefreshColumnWidgets();

		void OnListSelectionChanged(TableViewerItemPtr Item, ESelectInfo::Type SelectInfo);

		void AddWidgetColumns();

	private:
		// The actual height of each item
		TAttribute<float> ItemHeight;

		// The actual padding between each item
		TAttribute<FMargin> ItemPadding;

		// The actual ListView widget that displays the rows
		TSharedPtr<SListView<TableViewerItemPtr>> ListView;

		// The actual header widget
		TSharedPtr<SHeaderRow> HeaderRowWidget;

		// Our model class
		TSharedPtr<FTedsTableViewerModel> Model;

		// Delegate fired when the selection changes
		FOnSelectionChanged OnSelectionChanged;

		// Wrapper Teds Widget around our contents so we can use Teds columns to specify behavior
		TSharedPtr<ITedsWidget> TedsWidget;

		// The message to show in place of the table viewer when there are no rows provided by the current query stack
		TAttribute<FText> EmptyRowsMessage;
	};
}