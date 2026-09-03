// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modification/Utils/ScopedSelectionTransaction.h"

#include "CurveEditor.h"
#include "Internationalization/Internationalization.h"
#include "Modification/Changes/SelectionChangeCommand.h"
#include "SelectionChangeUtils.h"

#define LOCTEXT_NAMESPACE "FScopedSelectionChange"

namespace UE::CurveEditor
{
FScopedSelectionTransaction::FScopedSelectionTransaction(TWeakPtr<FCurveEditor> InCurveEditor, bool bInActuallyTransact)
	: FScopedSelectionTransaction(MoveTemp(InCurveEditor), LOCTEXT("ChangeSelection", "Change selection"), bInActuallyTransact)
{}

FScopedSelectionTransaction::FScopedSelectionTransaction(TWeakPtr<FCurveEditor> InCurveEditor, const FText& InDescription, bool bInActuallyTransact)
	: FScopedTransaction(InDescription, bInActuallyTransact)
	, WeakCurveEditor(MoveTemp(InCurveEditor))
{
	if (const TSharedPtr<FCurveEditor> CurveEditorPin = WeakCurveEditor.Pin())
	{
		OriginalSelection = CurveEditorPin->Selection;
	}
}

FScopedSelectionTransaction::~FScopedSelectionTransaction()
{
	if (!IsOutstanding()
		// This happens when this is a sub-transaction (Index > 1) and somebody cancels the root transaction (GUndo->Cancel()).
		// That's technically a valid use of API. Now, calling AppendChange no longer makes sense as the owning root transaction was cancelled. 
		|| !GUndo)
	{
		return;
	}
	
	const TSharedPtr<FCurveEditor> CurveEditorPin = WeakCurveEditor.Pin();
	if (!CurveEditorPin)
	{
		return;
	}
	
	if (FSelectionDeltaChange DeltaChange = DiffSelection(OriginalSelection, CurveEditorPin->Selection))
	{
		CurveEditorPin->GetTransactionManager()->AppendChange(
			MakeUnique<FSelectionChangeCommand>(MoveTemp(DeltaChange))
			);
	}
}
}

#undef LOCTEXT_NAMESPACE