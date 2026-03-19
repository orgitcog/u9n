// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Tickable.h"
#include "Delegates/IDelegateInstance.h"
#include "Subsystems/EngineSubsystem.h"

#include "HarmonixMetasound/Analysis/SpmcAnalysisResultQueue.h"
#include "HarmonixMetasound/Analysis/MidiClockSongPos.h"
#include "Analysis/MetasoundFrontendAnalyzerAddress.h"

#include "MidiClockUpdateSubsystem.generated.h"

#define UE_API HARMONIXMETASOUND_API

namespace HarmonixMetasound
{
	class FMidiClock;
}
class UMusicClockComponent;
namespace MidiClockUpdateSubsystem
{
	// TODO: Cleanup task - UE-205069 - Settle on one of these methods while testing Fortnite
	// and then delete this enum, the int32, and the CVar as they will no longer need to be switchable.
	enum class EUpdateMethod : uint8
	{
		EngineTickableObjectAndTickComponent = 0,
		EngineSubsystemCoreDelegatesOnBeginFrame = 1,
		EngineTickableObject = 2,
		EngineSubsystemCoreDelegatesOnSamplingInput = 3,
		NumMethods = 4
	};
	extern EUpdateMethod UpdateMethod;
}

/**
 * Handles updating low-resolution play cursors associated with a MIDI clock. This now includes both FMidiClock
 * and UMusicClockComponents.
 * MidiClock: Because FMidiClock instances do not have their lifecycle managed by the garbage collector,
 * we need a way to tick them on the game thread while avoiding races. This gives us a way to register clocks to be
 * ticked from within their constructors/destructor, and provides no user-facing API.
 * MusicClockComponent: Because the "current music time" is typically of interest to many game systems, and some of
 * those systems run in parallel in different threads (e.g. TickComponent functions and animation jobs), it is 
 * important that the current music time is updated at the beginning of the game frame, and then that same time
 * can be used for all systems for the frame. 
 * This subsystem solves for those problems. 
 */
UCLASS(MinimalAPI)
class UMidiClockUpdateSubsystem final : public UEngineSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	// Begin FTickableGameObject
	UE_API virtual bool IsTickable() const override;
	UE_API virtual void Tick(float DeltaTime) override;
	virtual ETickableTickType GetTickableTickType() const override { return (IsTemplate() ? ETickableTickType::Never : ETickableTickType::Conditional); }
	UE_API virtual TStatId GetStatId() const override;
	// End FTickableGameObject

	// Begin USubsystem
	UE_API virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	UE_API virtual void Deinitialize() override;
	// End USubsystem

	static UE_API void TrackMusicClockComponent(UMusicClockComponent* Clock);
	static UE_API void StopTrackingMusicClockComponent(UMusicClockComponent* Clock);

	using FClockHistoryPtr = TSharedPtr<HarmonixMetasound::Analysis::FMidiClockSongPositionHistory>;
	static UE_API FClockHistoryPtr GetOrCreateClockHistory(const Metasound::Frontend::FAnalyzerAddress& ForAddress);

private:

	mutable FCriticalSection TrackedMidiClocksMutex;

	TArray<TWeakObjectPtr<UMusicClockComponent>> TrackedMusicClockComponents;
	UE_API void TrackMusicClockComponentImpl(UMusicClockComponent* Clock);
	UE_API void StopTrackingMusicClockComponentImpl(UMusicClockComponent* Clock);
	UE_API void UpdateUMusicClockComponents();

	FDelegateHandle EngineBeginFrameDelegate;
	UE_API void CoreDelegatesBeginFrame();

	FDelegateHandle EngineSamplingInputDelegate;
	UE_API void CoreDelegatesSamplingInput();

	static UE_API FCriticalSection ClockHistoryMapLocker;
	static UE_API TMap<uint32, TWeakPtr<HarmonixMetasound::Analysis::FMidiClockSongPositionHistory>> ClockHistories;

	static UE_API uint32 MakeMidiSongPosAnalyzerAddressHash(const Metasound::Frontend::FAnalyzerAddress& ForAddress);

public:
	// Declare a "tick" method that can be used during automated testing so that
	// the test code doesn't need knowledge of how the low-res clocks are being ticked...
	UE_API void TickForTesting();
};

#undef UE_API
