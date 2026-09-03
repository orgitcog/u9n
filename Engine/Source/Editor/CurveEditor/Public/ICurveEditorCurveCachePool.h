// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CurveEditorTypes.h"

class FCurveEditor;
struct FCurveDrawParams;

namespace UE::CurveEditor
{
	class CURVEEDITOR_API ICurveEditorCurveCachePool
	{
	public:
		/** Draws the cached curves. Called when all curves were drawn to the curve cache pool. */
		virtual void DrawCachedCurves(TWeakPtr<const FCurveEditor> WeakCurveEditor) = 0;
	};
}
