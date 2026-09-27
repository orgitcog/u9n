// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Elements/Common/TypedElementHandles.h"
#include "Widgets/SWidget.h"

namespace ESelectInfo { enum Type : int; }

namespace UE::Editor::DataStorage
{
	// Interface for all types of TEDS table viewers
    class ITableViewer
    {
    public:

    	// Execute the given callback for each row handle selected in the table viewer
        virtual void ForEachSelectedRow(TFunctionRef<void(RowHandle)> InCallback) const = 0;

    	// Get the row handle for the widget row that contains the contents of the table viewer itself
        virtual RowHandle GetWidgetRowHandle() const = 0;

    	// Set the currently selected row in the table viewer
        virtual void SetSelection(RowHandle Row, bool bSelected, const ESelectInfo::Type SelectInfo) const = 0;

    	// Scroll the input row into view in the table viewer
        virtual void ScrollIntoView(RowHandle Row) const = 0;

    	// Deselect all items in the table viewer
        virtual void ClearSelection() const = 0;

    	// Get the table viewer as an SWidget
    	virtual TSharedRef<SWidget> AsWidget() = 0;

    	// Get if a specific row is selected
    	virtual bool IsSelected(RowHandle InRow) const = 0;

    	// Get if a specific row is selected exclusively
    	virtual bool IsSelectedExclusively(RowHandle InRow) const = 0;
   
    	virtual ~ITableViewer() = default;
    };
}