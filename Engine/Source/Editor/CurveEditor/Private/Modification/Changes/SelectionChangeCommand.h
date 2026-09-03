// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SelectionDeltaChange.h"
#include "Modification/CurveEditorCommandChange.h"

namespace UE::CurveEditor
{
/** Changes that changes the curve editor's selection. */
class FSelectionChangeCommand : public FCurveEditorCommandChange
{
public:

	explicit FSelectionChangeCommand(FSelectionDeltaChange InDeltaChange) : DeltaChange(MoveTemp(InDeltaChange)) {}
	
	//~ Begin FChange Interface
	virtual void Apply(UObject* Object) override;
	virtual void Revert(UObject* Object) override;
	//~ End FChange Interface

private:

	/** The change made to the selection. */
	const FSelectionDeltaChange DeltaChange;
};
}

