// Copyright Epic Games, Inc. All Rights Reserved.

#include "HarmonixMetasound/Components/MusicClockDriverBase.h"
#include "Engine/World.h"

void FMusicClockDriverBase::ResetDefaultSongMaps(float DefaultTempo, int32 DefaultTimeSigNum, int32 DefaultTimeSigDenom)
{
	DefaultMaps.EmptyAllMaps();
	DefaultMaps.Init(Harmonix::Midi::Constants::GTicksPerQuarterNoteInt);
	DefaultMaps.GetTempoMap().AddTempoInfoPoint(Harmonix::Midi::Constants::BPMToMidiTempo(DefaultTempo), 0);
	DefaultMaps.GetBarMap().AddTimeSignatureAtBarIncludingCountIn(0, DefaultTimeSigNum, DefaultTimeSigDenom);
}

FMusicClockDriverBase::FMusicClockDriverBase(UObject* WorldContextObject, float DefaultTempo, int32 DefaultTimeSigNum, int32 DefaultTimeSigDenom)
{
	ResetDefaultSongMaps(DefaultTempo, DefaultTimeSigNum, DefaultTimeSigDenom);
	ContextObj = TWeakObjectPtr<UObject>(WorldContextObject);
}

FMusicClockDriverBase::~FMusicClockDriverBase()
{
	
}

double FMusicClockDriverBase::GetWallClockTime() const
{
	if (TStrongObjectPtr<UObject> Obj = ContextObj.Pin())
	{
		if (Obj->GetWorld())
		{
			return Obj->GetWorld()->GetRealTimeSeconds();
		}
	}

	if (GWorld)
	{
		return GWorld->GetRealTimeSeconds();
	}
	
	return 0.0;
}


const FMidiSongPos& FMusicClockDriverBase::GetCurrentSongPos(ECalibratedMusicTimebase Timebase) const
{
	switch (Timebase)
	{
	case ECalibratedMusicTimebase::AudioRenderTime:
		return CurrentSmoothedAudioRenderSongPos;
	case ECalibratedMusicTimebase::ExperiencedTime:
		return CurrentPlayerExperiencedSongPos;
	case ECalibratedMusicTimebase::VideoRenderTime:
		return CurrentVideoRenderSongPos;
	case ECalibratedMusicTimebase::RawAudioRenderTime:
		return CurrentRawAudioRenderSongPos;
	}
	checkNoEntry();
	// minor robustness, return something even if there's an illegal value. 
	return CurrentVideoRenderSongPos;
}

const FMidiSongPos& FMusicClockDriverBase::GetPreviousSongPos(ECalibratedMusicTimebase Timebase) const
{
	switch (Timebase)
	{
	case ECalibratedMusicTimebase::AudioRenderTime:
		return PrevAudioRenderSongPos;
	case ECalibratedMusicTimebase::ExperiencedTime:
		return PrevPlayerExperiencedSongPos;
	case ECalibratedMusicTimebase::VideoRenderTime:
		return PrevVideoRenderSongPos;
	case ECalibratedMusicTimebase::RawAudioRenderTime:
		return PrevRawAudioRenderSongPos;
	}
	checkNoEntry();
	
	// minor robustness, return something even if there's an illegal value. 
	return CurrentVideoRenderSongPos;
}



void FMusicClockDriverBase::Start()
{
	OnStart();
	CurrentSmoothedAudioRenderSongPos.Reset();
    CurrentVideoRenderSongPos.Reset();
    CurrentPlayerExperiencedSongPos.Reset();
    CurrentRawAudioRenderSongPos.Reset();
	MusicClockState = EMusicClockState::Running;
}

void FMusicClockDriverBase::Continue()
{
	if (MusicClockState != EMusicClockState::Paused)
	{
		return;
	}
	
	OnContinue();
	MusicClockState = EMusicClockState::Running;
}

void FMusicClockDriverBase::Pause()
{
	if (MusicClockState != EMusicClockState::Running)
	{
		return;
	}

	OnPause();
	MusicClockState = EMusicClockState::Paused;
}

void FMusicClockDriverBase::Stop()
{
	OnStop();
	MusicClockState = EMusicClockState::Stopped;
	CurrentSmoothedAudioRenderSongPos.Reset();
	CurrentVideoRenderSongPos.Reset();
	CurrentPlayerExperiencedSongPos.Reset();
	CurrentRawAudioRenderSongPos.Reset();
}

void FMusicClockDriverBase::UpdateMusicPlaybackRate(float InTempo, float InClockAdvanceRate, int32 InTimeSigNum, int32 InTimeSigDenom)
{
	bool bHasAnyTimingChanges = false;
	if (!FMath::IsNearlyEqual(Tempo, InTempo))
	{
		Tempo = InTempo;
		bHasAnyTimingChanges = true;
	}

	if (!FMath::IsNearlyEqual(CurrentClockAdvanceRate, InClockAdvanceRate))
	{
		CurrentClockAdvanceRate = InClockAdvanceRate;
		bHasAnyTimingChanges = true;
	}
	
	if (TimeSignatureNum != InTimeSigNum || TimeSignatureDenom != InTimeSigDenom)
	{
		TimeSignatureNum = InTimeSigNum;
		TimeSignatureDenom = InTimeSigDenom;
		bHasAnyTimingChanges = true;
	}

	// only update bar/beat durations if there have been any other timing changes
	if (bHasAnyTimingChanges)
	{
		UMusicClockComponent::CalculateBarAndBeatDurations(Tempo, CurrentClockAdvanceRate, TimeSignatureNum, TimeSignatureDenom, CurrentBarDurationSec, CurrentBeatDurationSec);
	}
}

void FMusicClockDriverBase::EnsureClockIsValidForGameFrame()
{
	//	Not for use outside the game thread.
	if (ensureMsgf(
		IsInGameThread(),
		TEXT("%hs called from non-game thread.  This is not supported!"), __FUNCTION__) == false)
	{
		return;
	}

	if (GFrameCounter == LastUpdateFrame)
	{
		return;
	}

	//	Run the actual clock update.
	if (GetState() == EMusicClockState::Running)
	{
		PrevRawAudioRenderSongPos = CurrentRawAudioRenderSongPos;
		PrevAudioRenderSongPos = CurrentSmoothedAudioRenderSongPos;
		PrevPlayerExperiencedSongPos = CurrentPlayerExperiencedSongPos;
		PrevVideoRenderSongPos = CurrentVideoRenderSongPos;

		if (RefreshCurrentSongPos())
		{
			RawAudioRenderDeltaBarF = CurrentRawAudioRenderSongPos.BarsIncludingCountIn - PrevRawAudioRenderSongPos.BarsIncludingCountIn;
			RawAudioRenderDeltaBeatF = CurrentRawAudioRenderSongPos.BeatsIncludingCountIn - PrevRawAudioRenderSongPos.BeatsIncludingCountIn;
			AudioRenderDeltaBarF = CurrentSmoothedAudioRenderSongPos.BarsIncludingCountIn - PrevAudioRenderSongPos.BarsIncludingCountIn;
			AudioRenderDeltaBeatF = CurrentSmoothedAudioRenderSongPos.BeatsIncludingCountIn - PrevAudioRenderSongPos.BeatsIncludingCountIn;
			PlayerExperienceDeltaBarF = CurrentPlayerExperiencedSongPos.BarsIncludingCountIn - PrevPlayerExperiencedSongPos.BarsIncludingCountIn;
			PlayerExperienceDeltaBeatF = CurrentPlayerExperiencedSongPos.BeatsIncludingCountIn - PrevPlayerExperiencedSongPos.BeatsIncludingCountIn;
			VideoRenderDeltaBarF = CurrentVideoRenderSongPos.BarsIncludingCountIn - PrevVideoRenderSongPos.BarsIncludingCountIn;
			VideoRenderDeltaBeatF = CurrentVideoRenderSongPos.BeatsIncludingCountIn - PrevVideoRenderSongPos.BeatsIncludingCountIn;
			LastUpdateFrame = GFrameCounter;
		}
	}
}