// Copyright Epic Games, Inc. All Rights Reserved.

#include "FilterUtils.h"

#include "Filters/CurveEditorFilterBase.h"
#include "Modification/Utils/ScopedSelectionTransaction.h"

#include "Containers/Map.h"
#include "Templates/SharedPointer.h"

namespace UE::CurveEditor::FilterUtils
{
	void ApplyFilter(const TSharedRef<FCurveEditor>& InCurveEditor, UCurveEditorFilterBase& InFilter)
	{
		ApplyFilter(InCurveEditor, InFilter, InCurveEditor->Selection.GetAll());
	}

	void ApplyFilter(
		const TSharedRef<FCurveEditor>& InCurveEditor,
		UCurveEditorFilterBase& InFilter,
		const TMap<FCurveModelID, FKeyHandleSet>& InSelectedKeys
		)
	{
		TMap<FCurveModelID, FKeyHandleSet> OutKeysToSelect;
		InFilter.ApplyFilter(InCurveEditor, InSelectedKeys, OutKeysToSelect);
		
		const FScopedSelectionTransaction Transaction(InCurveEditor);
		// Clear their selection and then set it to the keys the filter thinks you should have selected.
		InCurveEditor->GetSelection().Clear();

		for (const TTuple<FCurveModelID, FKeyHandleSet>& OutSet : OutKeysToSelect)
		{
			InCurveEditor->GetSelection().Add(OutSet.Key, ECurvePointType::Key, OutSet.Value.AsArray());
		}
	}
}