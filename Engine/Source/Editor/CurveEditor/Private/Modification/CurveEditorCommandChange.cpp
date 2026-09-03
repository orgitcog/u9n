// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modification/CurveEditorCommandChange.h"

#include "Containers/UnrealString.h"
#include "CurveEditorTransactionObject.h"

namespace UE::CurveEditor
{
bool FCurveEditorCommandChange::HasExpired(UObject* InObject) const
{
	return !GetCurveEditor(InObject).IsValid();
}

FString FCurveEditorCommandChange::ToString() const
{
	return TEXT("FSelectionChangeCommand");
}

TSharedPtr<FCurveEditor> FCurveEditorCommandChange::GetCurveEditor(UObject* InObject)
{
	UCurveEditorTransactionObject* TransactionObject = Cast<UCurveEditorTransactionObject>(InObject);
	return ensure(TransactionObject) ? TransactionObject->OwningCurveEditor.Pin() : nullptr;
}
}
