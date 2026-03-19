// Copyright Epic Games, Inc. All Rights Reserved.

#include "CurveChangeListener.h"

#include "CurveEditor.h"
#include "CurveModel.h"

namespace UE::CurveEditorTools
{
	FCurveChangeListener::FCurveChangeListener(const TSharedRef<FCurveEditor>& InCurveEditor)
		: FCurveChangeListener(InCurveEditor, [&InCurveEditor]()
		{
			TArray<FCurveModelID> Curves;
			InCurveEditor->Selection.GetAll().GetKeys(Curves);
			return Curves;
		}())
	{}

	FCurveChangeListener::FCurveChangeListener(const TSharedRef<FCurveEditor>& InCurveEditor, const TArray<FCurveModelID>& InCurvesToListenTo)
		: WeakCurveEditor(InCurveEditor)
		, SubscribedToCurves(InCurvesToListenTo)
	{
		for (const FCurveModelID& CurveId : SubscribedToCurves)
		{
			if (FCurveModel* Model = InCurveEditor->FindCurve(CurveId))
			{
				Model->OnCurveModified().AddRaw(this, &FCurveChangeListener::HandleCurveModified);
			}
		}
	}

	FCurveChangeListener::~FCurveChangeListener()
	{
		if (TSharedPtr<FCurveEditor> CurveEditorPin = WeakCurveEditor.Pin())
		{
			for (const FCurveModelID& CurveId : SubscribedToCurves)
			{
				if (FCurveModel* Model = CurveEditorPin->FindCurve(CurveId))
				{
					Model->OnCurveModified().RemoveAll(this);
				}
			}
		}
	}
}
