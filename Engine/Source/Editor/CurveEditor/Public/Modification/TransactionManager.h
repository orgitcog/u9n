// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CurveEditorCommandChange.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/GCObject.h"
#include "UObject/ObjectPtr.h"

class FCurveEditor;
class UCurveEditorTransactionObject;

namespace UE::CurveEditor
{
/** Interacts with the transaction system to enable command-based actions for curve editor. */
class CURVEEDITOR_API FTransactionManager : public FNoncopyable, public FGCObject
{
public:

	explicit FTransactionManager(TWeakPtr<FCurveEditor> InCurveEditor);
	
	/**
	 * Appends a change to the current transaction.
	 *
	 * The transaction can consist of multiple changes, e.g.
	 * - system 1 may move keys, and
	 * - system 2 could coniditonally snap the keys to full frames after checking that the user has toggled auto-snapping
	 * 
	 * @param InChange The change to associate with the currently open transaction
	 */
	void AppendChange(TUniquePtr<FCurveEditorCommandChange> InChange) const;

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnCommandAppended, const FCurveEditorCommandChange&);
	/** Invoked after a command has been appended to the undo stack. */
	FOnCommandAppended& OnCommandAppended() { return OnCommandAppendedDelegate; }

	//~ Begin FGCObject Interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override;
	//~ End FGCObject Interface

private:

	/** The UObject that transactions are associated with. The transaction system needs to associate FCommandChanges with an UObject. */
	TObjectPtr<UCurveEditorTransactionObject> DummyTransactionObject;

	/** Invoked after a command has been appended to the undo stack. */
	FOnCommandAppended OnCommandAppendedDelegate;
};
}

