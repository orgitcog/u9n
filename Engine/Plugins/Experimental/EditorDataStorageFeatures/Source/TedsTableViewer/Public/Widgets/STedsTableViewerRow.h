// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TedsTableViewerModel.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

namespace UE::Editor::DataStorage
{
	class STedsTableViewer;
	
	/** Widget that represents a row in the table viewer.  Generates widgets for each column on demand. */
	class STedsTableViewerRow
		: public SMultiColumnTableRow< TableViewerItemPtr >
	{

	public:

		SLATE_BEGIN_ARGS( STedsTableViewerRow )
			: _ParentWidgetRowHandle(InvalidRowHandle)
			, _Padding(FMargin(0.f, 4.f))
			, _ItemHeight()
		{}

		/** The list item for this row */
		SLATE_ARGUMENT( TableViewerItemPtr, Item )

		SLATE_ARGUMENT( RowHandle, ParentWidgetRowHandle )

		SLATE_ATTRIBUTE( FMargin, Padding )
			
		/** The height of the list item */
		SLATE_ATTRIBUTE( float, ItemHeight )

		SLATE_END_ARGS()
		
		/** Construct function for this widget */
		void Construct( const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTableView, const TSharedRef<FTedsTableViewerModel>& InTableViewerModel );

		/** Overridden from SMultiColumnTableRow.  Generates a widget for this column of the tree row. */
		virtual TSharedRef<SWidget> GenerateWidgetForColumn( const FName& ColumnName ) override;

	private:
		/** Get the current ItemHeight as an OptionalSize for the Box HeightOverride */
		FOptionalSize GetCurrentItemHeight() const;

	protected:
		TAttribute<float> ItemHeight;
		RowHandle ParentWidgetRowHandle = InvalidRowHandle;
		TSharedPtr<FTedsTableViewerModel> TableViewerModel;
		TableViewerItemPtr Item;
	};

	/** Widget that represents a row in the hierarchy viewers.  Generates widgets for each column and adds an expander arrow to the very first column. */
	class SHierarchyViewerRow : public STedsTableViewerRow
	{
	public:
		virtual TSharedRef<SWidget> GenerateWidgetForColumn( const FName& ColumnName ) override;
	};
} // namespace UE::Editor::DataStorage
