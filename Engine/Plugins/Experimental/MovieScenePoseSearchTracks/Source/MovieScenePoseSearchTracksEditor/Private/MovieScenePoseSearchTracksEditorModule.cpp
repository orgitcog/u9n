// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovieScenePoseSearchTracksEditorModule.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "ISequencerModule.h"
#include "TrackEditors/StitchAnimTrackEditor.h"

namespace UE::MovieScene
{

void FMovieScenePoseSearchTracksEditorModule::StartupModule()
{
	using namespace UE::Sequencer;

	if (GIsEditor)
	{
		ISequencerModule& SequencerModule = FModuleManager::Get().LoadModuleChecked<ISequencerModule>("Sequencer");

		// register specialty track editors
		StitchAnimationTrackCreateEditorHandle = SequencerModule.RegisterTrackEditor(FOnCreateTrackEditor::CreateStatic(&FStitchAnimTrackEditor::CreateTrackEditor));
	}
}

void FMovieScenePoseSearchTracksEditorModule::ShutdownModule()
{
	if (!FModuleManager::Get().IsModuleLoaded("Sequencer"))
	{
		return;
	}

	ISequencerModule& SequencerModule = FModuleManager::Get().GetModuleChecked<ISequencerModule>("Sequencer");

	// unregister specialty track editors
	SequencerModule.UnRegisterTrackEditor(StitchAnimationTrackCreateEditorHandle);
}

}

IMPLEMENT_MODULE(UE::MovieScene::FMovieScenePoseSearchTracksEditorModule, MovieScenePoseSearchTracksEditor)
