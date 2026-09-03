// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "CurveEditor.h"
#include "CurveModel.h"

namespace UE::CurveEditor
{
/** Modifies all referenced curves. */
inline void ModifyCurves(TConstArrayView<FCurveModelID> InCurves, FCurveEditor& InCurveEditor)
{
	for (const FCurveModelID& CurveId : InCurves)
	{
		FCurveModel* Model = InCurveEditor.FindCurve(CurveId);
		Model->Modify();
	}
}
}
