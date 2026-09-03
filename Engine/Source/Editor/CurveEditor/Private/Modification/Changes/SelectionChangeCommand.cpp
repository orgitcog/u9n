// Copyright Epic Games, Inc. All Rights Reserved.

#include "SelectionChangeCommand.h"

#include "CurveEditor.h"
#include "Modification/Utils/SelectionChangeUtils.h"

namespace UE::CurveEditor
{
void FSelectionChangeCommand::Apply(UObject* Object)
{
	if (const TSharedPtr<FCurveEditor> CurveEditor = GetCurveEditor(Object))
	{
		ApplySelectionChange(CurveEditor->Selection, DeltaChange);
	}
}

void FSelectionChangeCommand::Revert(UObject* Object)
{
	if (const TSharedPtr<FCurveEditor> CurveEditor = GetCurveEditor(Object))
	{
		RevertSelectionChange(CurveEditor->Selection, DeltaChange);
	}
}
}
