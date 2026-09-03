// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Common/TypedElementCommonTypes.h"
#include "Framework/SlateDelegates.h"

#include "TedsTableViewerWidgetColumns.generated.h"

/**
* Column added to a widget row when an external widget manages exclusive selection for the widget referenced by the row,
 * such as an owning SListView or STreeView
 */
USTRUCT(meta = (DisplayName = "Widget with externally managed exclusive selection"))
struct FExternalWidgetExclusiveSelectionColumn : public FEditorDataStorageColumn
{
	GENERATED_BODY()
	/** Delegate to execute to check the status of if the widget is the only selected between multiple widgets
	 * Only needs to be hooked up if an external widget is managing selection on multiple widgets, such
	 * as a SListView or STreeView.
	 *
	 * E.g usage: Enables renaming by clicking the label twice if the widget is editable.
	 * @see SInlineEditableTextBlock::IsSelected
	 */
	FIsSelected IsSelectedExclusively;
};
