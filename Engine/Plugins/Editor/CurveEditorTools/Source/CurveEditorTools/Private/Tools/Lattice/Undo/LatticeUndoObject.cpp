// Copyright Epic Games, Inc. All Rights Reserved.

#include "LatticeUndoObject.h"

#include "CurveEditor.h"

void UCurveEditorTools_LatticeUndoObject::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Ar << Snapshot;
}

void UCurveEditorTools_LatticeUndoObject::PostEditUndo()
{
	UObject::PostEditUndo();
	OnPostEditUndo.Broadcast();
}
