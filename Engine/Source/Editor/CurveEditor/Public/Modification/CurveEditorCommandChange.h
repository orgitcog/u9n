// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/Change.h"
#include "Templates/SharedPointerFwd.h"

class FCurveEditor;

namespace UE::CurveEditor
{
/** Base class for actions that support undo / redo in the curve editor. */
class CURVEEDITOR_API FCurveEditorCommandChange : public FCommandChange
{
public:

	//~ Begin FChange Interface
	virtual bool HasExpired(UObject* InObject) const override;
	virtual FString ToString() const override;
	//~ End FChange Interface

protected:

	/** @return The curve editor associated with Object. */
	static TSharedPtr<FCurveEditor> GetCurveEditor(UObject* InObject);
};
}

