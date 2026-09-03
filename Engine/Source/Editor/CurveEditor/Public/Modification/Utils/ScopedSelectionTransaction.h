// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CurveEditorSelection.h"
#include "ScopedTransaction.h"

namespace UE::CurveEditor
{
/**
 * This util starts a transaction. When the scope ends, checks whether any changes to selection were made and adds it an undo-able action.
 * Use if you intend to change the selection.
 */
class CURVEEDITOR_API FScopedSelectionTransaction : public FScopedTransaction
{
public:

	[[nodiscard]] explicit FScopedSelectionTransaction(TWeakPtr<FCurveEditor> InCurveEditor, bool bInActuallyTransact = true);
	[[nodiscard]] explicit FScopedSelectionTransaction(TWeakPtr<FCurveEditor> InCurveEditor, const FText& InDescription, bool bInActuallyTransact = true);
	
	~FScopedSelectionTransaction();

private:

	const TWeakPtr<FCurveEditor> WeakCurveEditor;

	/** The selection the editor had when the transaction was started.  */
	FCurveEditorSelection OriginalSelection;
};
}

