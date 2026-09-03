// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "HarmonixMetasound/Components/MusicClockComponent.h"
#include "Harmonix/MusicalTimebase.h"
#include "HarmonixMidi/MidiSongPos.h"
#include "HarmonixMidi/SongMaps.h"

struct ISongMapEvaluator;

struct FMusicClockDriverBase : public TSharedFromThis<FMusicClockDriverBase>
{
public:
	FMusicClockDriverBase(UObject* WorldContextObject, float DefaultTempo, int32 DefaultTimeSigNum, int32 DefaultTimeSigDenom);
	virtual ~FMusicClockDriverBase();

	void EnsureClockIsValidForGameFrame();

	virtual bool CalculateSongPosWithOffset(float MsOffset, ECalibratedMusicTimebase Timebase, FMidiSongPos& OutResult) const = 0;

	double GetWallClockTime() const;

	EMusicClockState GetState() const { return MusicClockState; }
	const FMidiSongPos& GetCurrentSongPos(ECalibratedMusicTimebase Timebase) const;
	const FMidiSongPos& GetPreviousSongPos(ECalibratedMusicTimebase Timebase) const;

	void Start();
	void Pause();
	void Continue();
	void Stop();
	
	virtual const ISongMapEvaluator* GetCurrentSongMapEvaluator() const = 0;

	virtual bool LoopedThisFrame(ECalibratedMusicTimebase Timebase) const = 0;
	virtual bool SeekedThisFrame(ECalibratedMusicTimebase Timebase) const = 0;

	virtual void Disconnect() = 0;

	void UpdateMusicPlaybackRate(float InTempo, float InClockAdvanceRate, int32 InTimeSigNum, int32 InTimeSigDenom);
	
	FSongMaps DefaultMaps;
	
	float Tempo = 0.0f;
	int TimeSignatureNum = 0;
	int TimeSignatureDenom = 0;

	float CurrentBeatDurationSec = 0.0f;
	float CurrentBarDurationSec = 0.0f;

	FMidiSongPos CurrentSmoothedAudioRenderSongPos;
	FMidiSongPos CurrentVideoRenderSongPos;
	FMidiSongPos CurrentPlayerExperiencedSongPos;
	FMidiSongPos CurrentRawAudioRenderSongPos;
	
	FMidiSongPos PrevRawAudioRenderSongPos;
	FMidiSongPos PrevAudioRenderSongPos;
	FMidiSongPos PrevPlayerExperiencedSongPos;
	FMidiSongPos PrevVideoRenderSongPos;
	
	float CurrentClockAdvanceRate = 1.0f;

	// TODO shouldn't these deltas be rolled into the MidiSongPos?
	float RawAudioRenderDeltaBarF = 0.0f;
	float RawAudioRenderDeltaBeatF = 0.0f;
	float AudioRenderDeltaBarF = 0.0f;
	float AudioRenderDeltaBeatF = 0.0f;
	float PlayerExperienceDeltaBarF = 0.0f;
	float PlayerExperienceDeltaBeatF = 0.0f;
	float VideoRenderDeltaBarF = 0.0f;
	float VideoRenderDeltaBeatF = 0.0f;
	
protected:
	
	virtual void OnStart() = 0;
	virtual void OnPause() = 0;
	virtual void OnContinue() = 0;
	virtual void OnStop() = 0;
	
private:

	void ResetDefaultSongMaps(float DefaultTempo, int32 DefaultTimeSigNum, int32 DefaultTimeSigDenom);
	virtual bool RefreshCurrentSongPos() = 0;

	uint64 LastUpdateFrame = 0;

	EMusicClockState MusicClockState = EMusicClockState::Stopped;

	TWeakObjectPtr<UObject> ContextObj;
};