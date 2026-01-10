// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "HarmonixMetasound/Components/MusicClockDriverBase.h"
#include "HarmonixMidi/MidiFile.h"
#include "UObject/StrongObjectPtr.h"

struct FWallClockMusicClockDriver : public FMusicClockDriverBase
{
public:
	FWallClockMusicClockDriver(UObject* WorldContextObj, UMidiFile* InTempoMap, float DefaultTempo, int32 DefaultTimeSigNum, int32 DefaultTimeSigDenom)
		: FMusicClockDriverBase(WorldContextObj, DefaultTempo, DefaultTimeSigNum, DefaultTimeSigDenom)
		, TempoMapMidi(InTempoMap)
	{}

	virtual bool CalculateSongPosWithOffset(float MsOffset, ECalibratedMusicTimebase Timebase, FMidiSongPos& OutResult) const override;

	virtual void Disconnect() override;
	virtual bool RefreshCurrentSongPos() override;
	virtual void OnStart() override;
	virtual void OnPause() override;
	virtual void OnContinue() override;
	virtual void OnStop() override {}
	virtual const ISongMapEvaluator* GetCurrentSongMapEvaluator() const override;
	
	virtual bool LoopedThisFrame(ECalibratedMusicTimebase Timebase) const override { return false; }
	virtual bool SeekedThisFrame(ECalibratedMusicTimebase Timebase) const override { return false; }

private:
	TWeakObjectPtr<UMidiFile> TempoMapMidi;

	double StartTimeSecs = 0.0;
	double PauseTimeSecs = 0.0f;
};