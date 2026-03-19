// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Delegates/Delegate.h"
#include "Templates/SharedPointer.h"
#include "Templates/UnrealTemplate.h"

class FCurveEditor;
struct FCurveModelID;

namespace UE::CurveEditorTools
{
/** Listens to changes made to curves and cleans the subscriptions on destruction. */
class FCurveChangeListener : public FNoncopyable
{
public:
	
	explicit FCurveChangeListener(const TSharedRef<FCurveEditor>& InCurveEditor);
	explicit FCurveChangeListener(const TSharedRef<FCurveEditor>& InCurveEditor, const TArray<FCurveModelID>& InCurvesToListenTo);
	~FCurveChangeListener();

	FSimpleMulticastDelegate& OnCurveModified() { return OnCurveModifiedDelegate; }

private:

	/** Used to unsubscribe on destruction. */
	TWeakPtr<FCurveEditor> WeakCurveEditor;
	/** The curves we subscribed to */
	const TArray<FCurveModelID> SubscribedToCurves;

	/** Invokes when any of the listeners is changed. */
	FSimpleMulticastDelegate OnCurveModifiedDelegate;
	
	void HandleCurveModified() const { OnCurveModifiedDelegate.Broadcast(); }
};
}


