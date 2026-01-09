// Copyright Epic Games, Inc. All Rights Reserved.

#include "SequencerUtils.h"

#include "MVVM/CurveEditorExtension.h"
#include "MVVM/ViewModels/SequencerEditorViewModel.h"

namespace UE::ControlRigEditor
{
TSharedPtr<FCurveEditor> GetCurveEditorFromSequencer(const TSharedPtr<ISequencer>& InSequencer)
{
	if (!InSequencer)
	{
		return nullptr;
	}

	const TSharedPtr<Sequencer::FSequencerEditorViewModel> SequencerViewModel = InSequencer->GetViewModel();
	const Sequencer::FCurveEditorExtension* CurveEditorExtension = SequencerViewModel->CastDynamic<Sequencer::FCurveEditorExtension>();
	return ensure(CurveEditorExtension) ? CurveEditorExtension->GetCurveEditor() : nullptr;
}
}
