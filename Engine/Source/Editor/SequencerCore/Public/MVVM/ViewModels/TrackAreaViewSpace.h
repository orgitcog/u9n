// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "MVVM/ViewModels/ViewModel.h"
#include "TimeToPixel.h"

namespace UE::Sequencer
{

class SEQUENCERCORE_API FTrackAreaViewSpace
	: public FViewModel
	, public INonLinearTimeTransform
{
public:

	UE_SEQUENCER_DECLARE_CASTABLE(FTrackAreaViewSpace, FViewModel);

	FTrackAreaViewSpace();
	virtual ~FTrackAreaViewSpace();

	virtual double SourceToView(double Seconds) const override;
	virtual double ViewToSource(double Source) const override;
};

} // namespace UE::Sequencer

