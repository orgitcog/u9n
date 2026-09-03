// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modification/TransactionManager.h"

#include "CurveEditorTransactionObject.h"
#include "Misc/ITransaction.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

namespace UE::CurveEditor
{
FTransactionManager::FTransactionManager(TWeakPtr<FCurveEditor> InCurveEditor)
	: DummyTransactionObject(NewObject<UCurveEditorTransactionObject>(GetTransientPackage(), UCurveEditorTransactionObject::StaticClass(), NAME_None, RF_Transient | RF_Transactional))
{
	DummyTransactionObject->OwningCurveEditor = MoveTemp(InCurveEditor);
}

void FTransactionManager::AppendChange(TUniquePtr<FCurveEditorCommandChange> InChange) const
{
	if (ensureAlways(GUndo) && ensure(DummyTransactionObject) && ensure(InChange))
	{
		GUndo->StoreUndo(DummyTransactionObject, MoveTemp(InChange));
		OnCommandAppendedDelegate.Broadcast(*InChange);
	}
}

void FTransactionManager::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(DummyTransactionObject);
}

FString FTransactionManager::GetReferencerName() const
{
	return TEXT("FTransactionManager");
}
}

