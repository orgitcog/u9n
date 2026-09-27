// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovieSceneAnimMixerEditorModule.h"

#include "ISequencerModule.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "MovieSceneAnimationMixerTrack.h"
#include "MovieSceneAnimationMixerTrackEditor.h"
#include "Channels/BuiltInChannelEditors.h"


namespace Sequencer
{
	FKeyHandle EvaluateAndAddKey(FMovieSceneByteChannelDefaultOnly* InChannel, const TMovieSceneChannelData<uint8>& InChannelData, FFrameNumber InTime, ISequencer& InSequencer, uint8 InDefaultValue = 0)
	{
		return FKeyHandle::Invalid();
	}
	TOptional<FKeyHandle> AddKeyForExternalValue(
		FMovieSceneByteChannelDefaultOnly*         InChannel,
		const TMovieSceneExternalValue<uint8>&     InExternalValue,
		FFrameNumber                               InTime,
		ISequencer&                                InSequencer,
		const FGuid&                               InObjectBindingID,
		FTrackInstancePropertyBindings*            InPropertyBindings
		)
	{
		return TOptional<FKeyHandle>();
	}

	FKeyHandle AddOrUpdateKey(
		FMovieSceneByteChannelDefaultOnly* InChannel,
		UMovieSceneSection*                InSectionToKey,
		FFrameNumber                       InTime,
		ISequencer&                        InSequencer,
		const FGuid&                       InObjectBindingID,
		FTrackInstancePropertyBindings*    InPropertyBindings
		)
	{
		return FKeyHandle::Invalid();
	}

	FKeyHandle AddOrUpdateKey(
		FMovieSceneByteChannelDefaultOnly*         InChannel,
		UMovieSceneSection*                        SectionToKey,
		const TMovieSceneExternalValue<uint8>&     InExternalValue,
		FFrameNumber                               InTime,
		ISequencer&                                InSequencer,
		const FGuid&                               InObjectBindingID,
		FTrackInstancePropertyBindings*            InPropertyBindings)
	{
		return FKeyHandle::Invalid();
	}

	void CopyKeys(FMovieSceneByteChannelDefaultOnly* InChannel, const UMovieSceneSection* InSection, FName KeyAreaName, FMovieSceneClipboardBuilder& ClipboardBuilder, TArrayView<const FKeyHandle> InHandles)
	{
	}
	void PasteKeys(FMovieSceneByteChannelDefaultOnly* InChannel, UMovieSceneSection* Section, const FMovieSceneClipboardKeyTrack& KeyTrack, const FMovieSceneClipboardEnvironment& SrcEnvironment, const FSequencerPasteEnvironment& DstEnvironment, TArray<FKeyHandle>& OutPastedKeys)
	{
	}
	bool SupportsCurveEditorModels(const TMovieSceneChannelHandle<FMovieSceneByteChannelDefaultOnly>& ChannelHandle)
	{
		return false;
	}
	TSharedRef<SWidget> CreateKeyEditor(const TMovieSceneChannelHandle<FMovieSceneByteChannelDefaultOnly>& Channel, const UE::Sequencer::FCreateKeyEditorParams& Params)
	{
		return CreateKeyEditor(Channel.Cast<FMovieSceneByteChannel>(), Params);
	}
	bool CanCreateKeyEditor(const FMovieSceneByteChannelDefaultOnly* Channel)
	{
		return true;
	}
} // namespace Sequencer


// Include order matters here since this header needs to be included after the above overloads in order for ADL to see them correctly
#include "SequencerChannelInterface.h"

namespace UE::MovieScene
{
	FLazyName SequencerModuleName("Sequencer");

	void FMovieSceneAnimMixerEditorModule::StartupModule()
	{
		using namespace UE::Sequencer;

		ISequencerModule& SequencerModule = FModuleManager::Get().LoadModuleChecked<ISequencerModule>(SequencerModuleName.Resolve());
		AnimationTrackEditorHandle = SequencerModule.RegisterTrackEditor(FOnCreateTrackEditor::CreateStatic( &FAnimationMixerTrackEditor::CreateTrackEditor));

		SequencerModule.RegisterChannelInterface<FMovieSceneByteChannelDefaultOnly>();
	}

	void FMovieSceneAnimMixerEditorModule::ShutdownModule()
	{
		if (ISequencerModule* SequencerModule = FModuleManager::Get().GetModulePtr<ISequencerModule>(SequencerModuleName.Resolve()))
		{
			SequencerModule->UnregisterTrackModel(AnimationTrackEditorHandle);
		}
	}
}

IMPLEMENT_MODULE(UE::MovieScene::FMovieSceneAnimMixerEditorModule, MovieSceneAnimMixerEditor)