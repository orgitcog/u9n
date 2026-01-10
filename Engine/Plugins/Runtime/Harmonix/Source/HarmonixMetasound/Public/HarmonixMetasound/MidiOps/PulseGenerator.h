// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HarmonixDsp/Parameters/Parameter.h"

#include "HarmonixMetasound/DataTypes/MidiClock.h"
#include "HarmonixMetasound/DataTypes/MidiStream.h"
#include "HarmonixMetasound/DataTypes/MusicTimeInterval.h"

#include "HarmonixMidi/MidiVoiceId.h"

#define UE_API HARMONIXMETASOUND_API

namespace Harmonix::Midi::Ops
{
	class FPulseGenerator
	{
	public:
		virtual ~FPulseGenerator() = default;
		
		UE_API void Enable(bool bEnable);

		UE_API void SetInterval(const FMusicTimeInterval& NewInterval);
		FMusicTimeInterval GetInterval() const { return Interval; }

		UE_API virtual void Reset();
		
		struct FPulseInfo
		{
			int32 BlockFrameIndex;
			int32 Tick;
		};
		
		UE_API void Process(const HarmonixMetasound::FMidiClock& MidiClock, const TFunctionRef<void(const FPulseInfo&)>& OnPulse);

	protected:
		bool Enabled{ true };

		FMusicTimeInterval Interval{};
		FTimeSignature CurrentTimeSignature{};
		FMusicTimestamp NextPulseTimestamp{ -1, -1 };
	};
	
	class FMidiPulseGenerator : public FPulseGenerator
	{
	public:
		virtual ~FMidiPulseGenerator() override = default;
		
		Dsp::Parameters::TParameter<uint8> Channel{ 1, 16, 1 };

		Dsp::Parameters::TParameter<uint16> Track{ 1, UINT16_MAX, 1 };
		
		Dsp::Parameters::TParameter<uint8> NoteNumber{ 0, 127, 60 };
		
		Dsp::Parameters::TParameter<uint8> Velocity{ 0, 127, 127 };

		UE_API virtual void Reset() override;

		UE_API void Process(const HarmonixMetasound::FMidiClock& MidiClock, HarmonixMetasound::FMidiStream& OutStream);

	private:
		UE_API void AddPulseNote(const int32 BlockFrameIndex, const int32 PulseTick, HarmonixMetasound::FMidiStream& OutStream);

		FMidiVoiceGeneratorBase VoiceGenerator{};
		TOptional<HarmonixMetasound::FMidiStreamEvent> LastNoteOn;
	};
}

#undef UE_API
