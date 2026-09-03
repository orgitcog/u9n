// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CurveEditorFilterBase.h"
#include "CurveEditorSmartSnapFilter.generated.h"

/**
 * 
 */
UCLASS(DisplayName = "Smart Snap", meta = (
	CurveEditorLabel = "Smart Snap",
	CurveEditorDescription = "Snaps selected keys to the closest whole frame.\nThe key is placed where the curve intersects with the frame (imagine vertical line going down from the frame).")
	)
class CURVEEDITOR_API UCurveEditorSmartSnapFilter : public UCurveEditorFilterBase
{
	GENERATED_BODY()
protected:

	//~ Begin UCurveEditorFilterBase Interface
	virtual void ApplyFilter_Impl(TSharedRef<FCurveEditor> InCurveEditor, const TMap<FCurveModelID, FKeyHandleSet>& InKeysToOperateOn, TMap<FCurveModelID, FKeyHandleSet>& OutKeysToSelect) override;
	virtual bool CanApplyFilter_Impl(TSharedRef<FCurveEditor> InCurveEditor) override;
	//~ End UCurveEditorFilterBase Interface
};
