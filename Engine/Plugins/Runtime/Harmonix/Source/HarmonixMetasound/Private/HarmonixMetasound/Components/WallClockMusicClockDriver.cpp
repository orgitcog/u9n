// Copyright Epic Games, Inc. All Rights Reserved.
#include "WallClockMusicClockDriver.h"
#include "Harmonix.h"

bool FWallClockMusicClockDriver::CalculateSongPosWithOffset(float MsOffset, ECalibratedMusicTimebase Timebase, FMidiSongPos& OutResult) const
{
	check(IsInGameThread());
	if (!TempoMapMidi.IsValid())
	{
		return false;
	}

	const FSongMaps* Maps = TempoMapMidi->GetSongMaps();
	switch (Timebase)
	{
	case ECalibratedMusicTimebase::AudioRenderTime:
		OutResult.SetByTime((CurrentSmoothedAudioRenderSongPos.SecondsIncludingCountIn * 1000.0f) + MsOffset, *Maps);
		break;
	case ECalibratedMusicTimebase::ExperiencedTime:
		OutResult.SetByTime((CurrentPlayerExperiencedSongPos.SecondsIncludingCountIn * 1000.0f) + MsOffset, *Maps);
		break;
	case ECalibratedMusicTimebase::VideoRenderTime:
	default:
		OutResult.SetByTime((CurrentVideoRenderSongPos.SecondsIncludingCountIn * 1000.0f) + MsOffset, *Maps);
		break;
	}

	return true;
}

void FWallClockMusicClockDriver::Disconnect()
{
	TempoMapMidi = nullptr;
}

bool FWallClockMusicClockDriver::RefreshCurrentSongPos()
{
	check(IsInGameThread());
	double RunTime = GetWallClockTime() - StartTimeSecs;

	const ISongMapEvaluator* Maps = GetCurrentSongMapEvaluator();
	check(Maps);

	CurrentSmoothedAudioRenderSongPos.SetByTime((float)(RunTime * 1000.0), *Maps);
	CurrentPlayerExperiencedSongPos.SetByTime(CurrentSmoothedAudioRenderSongPos.SecondsIncludingCountIn * 1000.0f - FHarmonixModule::GetMeasuredUserExperienceAndReactionToAudioRenderOffsetMs(), *Maps);
	CurrentVideoRenderSongPos.SetByTime(CurrentSmoothedAudioRenderSongPos.SecondsIncludingCountIn * 1000.0f - FHarmonixModule::GetMeasuredVideoToAudioRenderOffsetMs(), *Maps);
	UpdateMusicPlaybackRate(CurrentSmoothedAudioRenderSongPos.Tempo, CurrentClockAdvanceRate, CurrentSmoothedAudioRenderSongPos.TimeSigNumerator, CurrentSmoothedAudioRenderSongPos.TimeSigDenominator);
	return true;
}

void FWallClockMusicClockDriver::OnStart()
{
	check(IsInGameThread());
	StartTimeSecs = GetWallClockTime();
	PauseTimeSecs = 0.0;
}

void FWallClockMusicClockDriver::OnPause()
{
	check(IsInGameThread());
	PauseTimeSecs = GetWallClockTime();
}

void FWallClockMusicClockDriver::OnContinue()
{
	check(IsInGameThread());
	double CurrentTime = GetWallClockTime();
	StartTimeSecs += (CurrentTime - PauseTimeSecs);
	PauseTimeSecs = 0.0;
	RefreshCurrentSongPos();
}

const ISongMapEvaluator* FWallClockMusicClockDriver::GetCurrentSongMapEvaluator() const
{
	check(IsInGameThread());
	if (TempoMapMidi.IsValid())
	{
		return TempoMapMidi->GetSongMaps();
	}
	return &DefaultMaps;
}
