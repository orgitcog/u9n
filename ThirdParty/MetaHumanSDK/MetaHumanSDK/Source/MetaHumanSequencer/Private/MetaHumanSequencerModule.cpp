// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanSequencerModule.h"

#include "ISequencerModule.h"
#include "MetaHumanMediaTrackEditor.h"
#include "MetaHumanAudioTrackEditor.h"
#include "MetaHumanMovieSceneChannel.h"
#include "SequencerChannelInterface.h"
#include "ClipboardTypes.h"

void FMetaHumanSequencerModule::StartupModule()
{
	ISequencerModule& SequencerModule = FModuleManager::LoadModuleChecked<ISequencerModule>("Sequencer");
	MediaTrackEditorBindingHandle = SequencerModule.RegisterPropertyTrackEditor<FMetaHumanMediaTrackEditor>();
	AudioTrackEditorBindingHandle = SequencerModule.RegisterPropertyTrackEditor<FMetaHumanAudioTrackEditor>();

	SequencerModule.RegisterChannelInterface<FMetaHumanMovieSceneChannel>();
}

void FMetaHumanSequencerModule::ShutdownModule()
{
	if (ISequencerModule* SequencerModulePtr = FModuleManager::Get().GetModulePtr<ISequencerModule>("Sequencer"))
	{
		SequencerModulePtr->UnRegisterTrackEditor(MediaTrackEditorBindingHandle);
		SequencerModulePtr->UnRegisterTrackEditor(AudioTrackEditorBindingHandle);
	}
}

IMPLEMENT_MODULE(FMetaHumanSequencerModule, MetaHumanSequencer)
