// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CurveEditor.h"
#include "Math/Models/TweenModel.h"
#include "Misc/Attribute.h"
#include "Misc/Mirror/TangentSelectionFlattener.h"
#include <type_traits>

namespace UE::TweeningUtilsEditor
{
/**
 * This class squishes the curves based on how much the tween function squishes the keys vertically.
 * The squishing is achieved by interpolating the tangents to 0.
 */
template<typename TBase> requires std::is_base_of_v<FTweenModel, TBase>
class TTangentFlatteningTweenProxy : public TBase
{
public:

	template<typename... TArg>
	explicit TTangentFlatteningTweenProxy(TAttribute<TWeakPtr<FCurveEditor>> WeakCurveEditorAttr, TArg&&... Arg)
		: TBase(Forward<TArg>(Arg)...)
		, WeakCurveEditor(MoveTemp(WeakCurveEditorAttr))
	{
		check(WeakCurveEditorAttr.IsBound() || WeakCurveEditorAttr.IsSet());
	}

	//~ Begin FTweenModel Interface
	virtual void StartBlendOperation() override
	{
		TBase::StartBlendOperation();
		
		if (const TSharedPtr<FCurveEditor> CurveEditorPin = WeakCurveEditor.Get().Pin())
		{
			TangentTweener.ResetFromSelection(*CurveEditorPin);
		}
	}
	virtual void BlendValues(float InNormalizedValue) override
	{
		TBase::BlendValues(InNormalizedValue);
		if (const TSharedPtr<FCurveEditor> CurveEditorPin = WeakCurveEditor.Get().Pin())
		{
			TangentTweener.ComputeMirroringParallel(*CurveEditorPin);
		}
	}
	//~ Begin FTweenModel Interface

private:

	/** Needed as arg for TangentTweener. */
	const TAttribute<TWeakPtr<FCurveEditor>> WeakCurveEditor;
	/** Implements the logic for flattening the tangents. */
	CurveEditor::FTangentSelectionFlattener TangentTweener;
};
}

