// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MVVM/Extensions/IObjectModelExtension.h"
#include "MVVM/Extensions/ITrackAreaViewSpaceProviderExtension.h"
#include "MVVM/ViewModels/ViewModel.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

class ISequencer;
class UMovieSceneScalingAnchors;
class UToolMenu;

namespace UE::Sequencer
{

class FTrackAreaViewModel;

class MOVIESCENETOOLS_API FScalingAnchorsModel
	: public FViewModel
	, public IObjectModelExtension
	, public ITrackAreaViewSpaceProviderExtension
{
public:

	UE_SEQUENCER_DECLARE_CASTABLE(FScalingAnchorsModel, FViewModel, IObjectModelExtension, ITrackAreaViewSpaceProviderExtension);

	/*~ Begin IObjectModelExtension */
	virtual void InitializeObject(TWeakObjectPtr<> InWeakObject) override;
	virtual UObject* GetObject() const override;
	/*~ End IObjectModelExtension */

	/*~ Begin ITrackAreaViewSpaceProviderExtension */
	virtual void UpdateViewSpaces(FTrackAreaViewModel& TrackAreaViewModel) override;
	/*~ End ITrackAreaViewSpaceProviderExtension */

private:

	void ExtendSectionMenu(UToolMenu* InMenu);
	void CreateScalingGroup(TWeakPtr<ISequencer> InWeakSequencer);

private:

	TWeakObjectPtr<UMovieSceneScalingAnchors> WeakAnchors;
};


} // namespace UE::Sequencer

