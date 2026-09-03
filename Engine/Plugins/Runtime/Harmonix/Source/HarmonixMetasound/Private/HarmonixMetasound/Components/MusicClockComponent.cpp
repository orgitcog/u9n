// Copyright Epic Games, Inc. All Rights Reserved.
#include "HarmonixMetasound/Components/MusicClockComponent.h"
#include "HarmonixMetasound/Components/MetasoundMusicClockDriver.h"
#include "HarmonixMetasound/Components/WallClockMusicClockDriver.h"
#include "HarmonixMetasound/Subsystems/MidiClockUpdateSubsystem.h"
#include "HarmonixMidi/MusicTimeSpan.h"
#include "Components/AudioComponent.h"
#include "Templates/SharedPointer.h"
#include "GameFramework/Actor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MusicClockComponent)

DEFINE_LOG_CATEGORY(LogMusicClock)

UMusicClockComponent::UMusicClockComponent()
{
	MakeDefaultSongMap();
	PrimaryComponentTick.bCanEverTick = true;
	bAutoActivate = false;
}

UMusicClockComponent* UMusicClockComponent::CreateMetasoundDrivenMusicClock(UObject* WorldContextObject, UAudioComponent* InAudioComponent, FName MetasoundOutputPinName, bool Start)
{
	UMusicClockComponent* NewClock = NewObject<UMusicClockComponent>(WorldContextObject);
	if (!NewClock->GetOwner())
	{
		UE_LOG(LogMusicClock, Warning, TEXT("Can't create a music clock in a non-actor context!"));
		return nullptr;
	}
	NewClock->RegisterComponent();
	NewClock->DriveMethod = EMusicClockDriveMethod::MetaSound;
	NewClock->MetasoundOutputName = MetasoundOutputPinName;
	NewClock->ConnectToMetasoundOnAudioComponent(InAudioComponent);
	if (Start)
	{
		NewClock->Start();
	}
	return NewClock;
}

UMusicClockComponent* UMusicClockComponent::CreateWallClockDrivenMusicClock(UObject* WorldContextObject, UMidiFile* InTempoMap, bool Start)
{
	UMusicClockComponent* NewClock = NewObject<UMusicClockComponent>(WorldContextObject);
	if (!NewClock->GetOwner())
	{
		UE_LOG(LogMusicClock, Warning, TEXT("Can't create a music clock in a non-actor context!"));
		return nullptr;
	}
	NewClock->RegisterComponent();
	NewClock->ConnectToWallClockForMidi(InTempoMap);
	if (Start)
	{
		NewClock->Start();
	}
	return NewClock;
}

bool UMusicClockComponent::ConnectToMetasoundOnAudioComponent(UAudioComponent* InAudioComponent)
{
	DriveMethod = EMusicClockDriveMethod::MetaSound;
	MetasoundsAudioComponent = InAudioComponent;
	return ConnectToMetasound();
}

void UMusicClockComponent::ConnectToWallClockForMidi(UMidiFile* InTempoMap)
{
	DriveMethod = EMusicClockDriveMethod::WallClock;
	TempoMap = InTempoMap;
	ConnectToWallClock();
}

void UMusicClockComponent::SetDefaultTempo(float TempoBpm)
{
	// Defaults only used to make the tempo map, once before ever running.
	ensure(GetState() == EMusicClockState::Stopped);
	DefaultTempo = TempoBpm;
}

void UMusicClockComponent::SetDefaultTimeSignatureNum(int Num)
{
	ensure(GetState() == EMusicClockState::Stopped);
	DefaultTimeSignatureNum = Num;
}

void UMusicClockComponent::SetDefaultTimeSignatureDenom(int Denom)
{
	ensure(GetState() == EMusicClockState::Stopped);
	DefaultTimeSignatureDenom = Denom;
}

void UMusicClockComponent::CalculateBarAndBeatDurations(float Tempo, float AdvanceRate, int32 TimeSigNum, int32 TimeSigDenom, float& OutBarDuration, float& OutBeatDuration)
{
	OutBeatDuration = 0.0f;
	OutBarDuration = 0.0f;
	if (Tempo > 0.0f && AdvanceRate > 0.0f && TimeSigNum > 0 && TimeSigDenom)
	{
		OutBeatDuration = (60.0f / Tempo) / AdvanceRate;
		OutBarDuration = ((TimeSigNum * OutBeatDuration) / (TimeSigDenom / 4.0f)) / AdvanceRate;
	}
}

void UMusicClockComponent::EnsureClockIsValidForGameFrameFromSubsystem()
{
	if (ClockDriver)
	{
		ClockDriver->EnsureClockIsValidForGameFrame();
	}
}

void UMusicClockComponent::CreateClockDriver()
{
	if (DriveMethod == EMusicClockDriveMethod::WallClock || !::IsValid(MetasoundsAudioComponent))
	{
		ConnectToWallClock();
	}
	else
	{
		ConnectToMetasound();
	}
}

bool UMusicClockComponent::ConnectToMetasound()
{
	check(DriveMethod == EMusicClockDriveMethod::MetaSound);
	if (!::IsValid(MetasoundsAudioComponent))
	{
		return false;
	}
	DisconnectFromClockDriver();
	TSharedPtr<FMetasoundMusicClockDriver> MetasoundClockDriver = MakeShared<FMetasoundMusicClockDriver>(this, DefaultTempo, DefaultTimeSignatureNum, DefaultTimeSignatureDenom);
	bool Connected = MetasoundClockDriver->ConnectToAudioComponentsMetasound(
		MetasoundsAudioComponent,
		MetasoundOutputName,
		UMetasoundGeneratorHandle::FOnAttached::FDelegate::CreateLambda([this](){ MusicClockConnectedEvent.Broadcast(); }),
		UMetasoundGeneratorHandle::FOnDetached::FDelegate::CreateLambda([this](){ MusicClockDisconnectedEvent.Broadcast(); }));
	MetasoundClockDriver->RunPastMusicEnd = RunPastMusicEnd;
	ClockDriver = MoveTemp(MetasoundClockDriver);
	if (IsActive())
	{
		ClockDriver->Start();
	}
	return Connected;
}

void UMusicClockComponent::ConnectToWallClock()
{
	// we don't 'check' that the driver mode is wall clock here because if the driver mode is metasound and we can't 
	// connect for some reason we will fall back to this clock driver!
	DisconnectFromClockDriver();
	ClockDriver = MakeShared<FWallClockMusicClockDriver>(this, TempoMap, DefaultTempo, DefaultTimeSignatureNum, DefaultTimeSignatureDenom);
	if (IsActive())
	{
		ClockDriver->Start();
	}
}

void UMusicClockComponent::DisconnectFromClockDriver()
{
	if (ClockDriver)
	{
		ClockDriver->Disconnect();
		ClockDriver = nullptr;
	}
}

FMidiSongPos UMusicClockComponent::CalculateSongPosWithOffset(float MsOffset, ECalibratedMusicTimebase Timebase) const
{
	FMidiSongPos Result;
	if (ClockDriver != nullptr)
	{
		if (ClockDriver->CalculateSongPosWithOffset(MsOffset, Timebase, Result))
		{
			return Result;
		}
	}

	// otherwise, use our song maps copy
	Result.SetByTime((GetCurrentSongPosInternal(Timebase).SecondsIncludingCountIn * 1000.0f) + MsOffset, DefaultMaps);
	return Result;
}

const FMidiSongPos& UMusicClockComponent::GetRawUnsmoothedAudioRenderPos() const
{
	static const FMidiSongPos PosZero;
	return ClockDriver ? ClockDriver->CurrentRawAudioRenderSongPos : PosZero;
}

void UMusicClockComponent::Activate(bool bReset)
{
	// cache the value of ShouldActivate before calling the super
	bool bShouldActivate = bReset || ShouldActivate();
	Super::Activate(bReset);

	if (!bShouldActivate)
	{
		return;
	}

	if (bReset && GetState() != EMusicClockState::Stopped)
	{
		if (ClockDriver)
		{
			ClockDriver->Stop();
			DisconnectFromClockDriver();
		}
		PlayStateEvent.Broadcast(EMusicClockState::Stopped);
	}

	if (GetState() == EMusicClockState::Running)
	{
		return;
	}

	MakeDefaultSongMap();

	if (!MetasoundsAudioComponent && GetOwner())
	{
		MetasoundsAudioComponent = GetOwner()->FindComponentByClass<UAudioComponent>();
	}

	if (!ClockDriver)
	{
		CreateClockDriver();
	}
	ClockDriver->Start();
	LastBroadcastBeat = -1;
	LastBroadcastBar  = -1;
	UMidiClockUpdateSubsystem::TrackMusicClockComponent(this);
	PlayStateEvent.Broadcast(EMusicClockState::Running);
}

void UMusicClockComponent::Deactivate()
{
	bool bShouldDeactivate = IsActive();
	Super::Deactivate();

	if (!bShouldDeactivate)
	{
		return;
	}

	if (GetState() == EMusicClockState::Stopped)
	{
		return;
	}

	if (ClockDriver)
	{
		ClockDriver->Stop();
	}
	LastBroadcastBeat = -1;
	LastBroadcastBar  = -1;
	UMidiClockUpdateSubsystem::StopTrackingMusicClockComponent(this);
	DisconnectFromClockDriver();
	PlayStateEvent.Broadcast(EMusicClockState::Stopped);
}

void UMusicClockComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UMusicClockComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	UMidiClockUpdateSubsystem::StopTrackingMusicClockComponent(this);
	DisconnectFromClockDriver();
}

void UMusicClockComponent::BeginDestroy()
{
	Super::BeginDestroy();
	UMidiClockUpdateSubsystem::StopTrackingMusicClockComponent(this);
	DisconnectFromClockDriver();
}

void UMusicClockComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	TickComponentInternal();
}

void UMusicClockComponent::TickComponentInternal()
{
	if (GetState() == EMusicClockState::Running)
	{
		BroadcastSongPosChanges(TimebaseForBarAndBeatEvents);
		BroadcastSeekLoopDetections(ECalibratedMusicTimebase::AudioRenderTime);
		BroadcastSeekLoopDetections(ECalibratedMusicTimebase::ExperiencedTime);
		BroadcastSeekLoopDetections(ECalibratedMusicTimebase::VideoRenderTime);
	}
}

void UMusicClockComponent::SetTempoMapForWallClock(UMidiFile* InTempoMap)
{
	ensure(GetState() == EMusicClockState::Stopped);
	TempoMap = InTempoMap;
	if (ClockDriver)
	{
		UE_LOG(LogMusicClock, Log, TEXT("Tempo Maps have changed! Clock requires restart"));
	}
}

void UMusicClockComponent::SetRunPastMusicEnd(bool bRunPastMusicEnd)
{
	RunPastMusicEnd = bRunPastMusicEnd;
	if (TSharedPtr<FMetasoundMusicClockDriver> MetasoundClockDriver = StaticCastSharedPtr<FMetasoundMusicClockDriver>(ClockDriver))
	{
		MetasoundClockDriver->RunPastMusicEnd = bRunPastMusicEnd;
	}
}

bool UMusicClockComponent::GetRunPastMusicEnd() const
{
	if (TSharedPtr<FMetasoundMusicClockDriver> MetasoundClockDriver = StaticCastSharedPtr<FMetasoundMusicClockDriver>(ClockDriver))
	{
		return MetasoundClockDriver->RunPastMusicEnd;
	}
	return RunPastMusicEnd;
}

float UMusicClockComponent::GetCurrentTempo() const
{
	if (ClockDriver)
	{
		return ClockDriver->Tempo;
	}
	return 0.0f;
}

void UMusicClockComponent::GetCurrentTimeSignature(int& OutNumerator, int& OutDenominator) const
{
	OutNumerator = 0;
	OutDenominator = 0;
	if (ClockDriver)
	{
		OutNumerator = ClockDriver->TimeSignatureNum;
		OutDenominator = ClockDriver->TimeSignatureDenom;
	}
}

float UMusicClockComponent::GetCurrentBarDurationSeconds() const
{
	if (ClockDriver)
	{
		return ClockDriver->CurrentBarDurationSec;
	}
	return 0;
}

float UMusicClockComponent::GetCurrentBeatDurationSeconds() const
{
	if (ClockDriver)
	{
		return ClockDriver->CurrentBeatDurationSec;
	}
	return 0;
}

float UMusicClockComponent::GetCurrentClockAdvanceRate() const
{
	if (ClockDriver)
	{
		return ClockDriver->CurrentClockAdvanceRate;
	}
	return 0.0f;
}

void UMusicClockComponent::Start()
{
	Activate(false);
}

void UMusicClockComponent::Pause()
{
	if (GetState() != EMusicClockState::Running)
	{
		return;
	}
	ClockDriver->Pause();
	PlayStateEvent.Broadcast(EMusicClockState::Paused);
}

void UMusicClockComponent::Continue()
{
	if (GetState() != EMusicClockState::Paused)
	{
		return;
	}
	ClockDriver->Continue();
	PlayStateEvent.Broadcast(EMusicClockState::Running);
}

void UMusicClockComponent::Stop()
{
	Deactivate();
}

EMusicClockState UMusicClockComponent::GetState() const
{
	if (!ClockDriver)
	{
		return EMusicClockState::Stopped;
	}

	return ClockDriver->GetState();
}


float UMusicClockComponent::GetSecondsIncludingCountIn(ECalibratedMusicTimebase Timebase) const
{
	return GetSongPos(Timebase).SecondsIncludingCountIn;
}

float UMusicClockComponent::GetSecondsFromBarOne(ECalibratedMusicTimebase Timebase) const
{
	return GetSongPos(Timebase).SecondsFromBarOne;
}

float UMusicClockComponent::GetBarsIncludingCountIn(ECalibratedMusicTimebase Timebase) const
{
	return GetSongPos(Timebase).BarsIncludingCountIn;
}

float UMusicClockComponent::GetBeatsIncludingCountIn(ECalibratedMusicTimebase Timebase) const
{
	return GetSongPos(Timebase).BeatsIncludingCountIn;
}

float UMusicClockComponent::GetTicksFromBarOne(ECalibratedMusicTimebase Timebase) const
{
	const float Seconds = GetSecondsFromBarOne(Timebase);
	return GetSongMaps().MsToTick(Seconds * 1000.0f);
}

float UMusicClockComponent::GetTicksIncludingCountIn(ECalibratedMusicTimebase Timebase) const
{
	const float Seconds = GetSecondsIncludingCountIn(Timebase);
	return GetSongMaps().MsToTick(Seconds * 1000.0f);
}

FMusicTimestamp UMusicClockComponent::GetCurrentTimestamp(ECalibratedMusicTimebase Timebase) const
{
	return GetSongPos(Timebase).Timestamp;
}

FString UMusicClockComponent::GetCurrentSectionName(ECalibratedMusicTimebase Timebase) const
{
	const FMidiSongPos& SongPos = GetSongPos(Timebase);
	return SongPos.CurrentSongSection.Name;
}

int32 UMusicClockComponent::GetCurrentSectionIndex(ECalibratedMusicTimebase Timebase) const
{
	const FMidiSongPos& SongPos = GetSongPos(Timebase);
	return GetSongMaps().GetSectionIndexAtTick(SongPos.CurrentSongSection.StartTick);
}

float UMusicClockComponent::GetCurrentSectionStartMs(ECalibratedMusicTimebase Timebase) const
{
	const FMidiSongPos& SongPos = GetSongPos(Timebase);
	return GetSongMaps().TickToMs(SongPos.CurrentSongSection.StartTick);
}

float UMusicClockComponent::GetCurrentSectionLengthMs(ECalibratedMusicTimebase Timebase) const
{
	const FMidiSongPos& SongPos = GetSongPos(Timebase);
	return GetSongMaps().TickToMs(SongPos.CurrentSongSection.LengthTicks);
}

float UMusicClockComponent::GetDistanceFromCurrentBeat(ECalibratedMusicTimebase Timebase) const
{
	return FMath::Fractional(GetSongPos(Timebase).BeatsIncludingCountIn);
}

float UMusicClockComponent::GetDistanceToNextBeat(ECalibratedMusicTimebase Timebase) const
{
	return 1 - GetDistanceFromCurrentBeat(Timebase);
}

float UMusicClockComponent::GetDistanceToClosestBeat(ECalibratedMusicTimebase Timebase) const
{
	return FMath::Min(GetDistanceFromCurrentBeat(Timebase), GetDistanceToNextBeat(Timebase));
}

float UMusicClockComponent::GetDistanceFromCurrentBar(ECalibratedMusicTimebase Timebase) const
{
	return FMath::Fractional(GetSongPos(Timebase).BarsIncludingCountIn);
}

float UMusicClockComponent::GetDistanceToNextBar(ECalibratedMusicTimebase Timebase) const
{
	return 1 - GetDistanceFromCurrentBar(Timebase);
}

float UMusicClockComponent::GetDistanceToClosestBar(ECalibratedMusicTimebase Timebase) const
{
	return FMath::Min(GetDistanceFromCurrentBar(Timebase), GetDistanceToNextBar(Timebase));
}

float UMusicClockComponent::GetDeltaBar(ECalibratedMusicTimebase Timebase) const
{
	if (ClockDriver)
	{
		switch (Timebase)
		{
		case ECalibratedMusicTimebase::RawAudioRenderTime: return ClockDriver->RawAudioRenderDeltaBarF;
		case ECalibratedMusicTimebase::AudioRenderTime:    return ClockDriver->AudioRenderDeltaBarF;
		case ECalibratedMusicTimebase::ExperiencedTime:	   return ClockDriver->PlayerExperienceDeltaBarF;
		case ECalibratedMusicTimebase::VideoRenderTime:
		default:                                           return ClockDriver->VideoRenderDeltaBarF;
		}
	}
	return 0.0f;
}

float UMusicClockComponent::GetDeltaBeat(ECalibratedMusicTimebase Timebase) const
{
	if (ClockDriver)
	{
		switch (Timebase)
		{
		case ECalibratedMusicTimebase::RawAudioRenderTime: return ClockDriver->RawAudioRenderDeltaBeatF;
		case ECalibratedMusicTimebase::AudioRenderTime:    return ClockDriver->AudioRenderDeltaBeatF;
		case ECalibratedMusicTimebase::ExperiencedTime:	   return ClockDriver->PlayerExperienceDeltaBeatF;
		case ECalibratedMusicTimebase::VideoRenderTime:
		default:                                           return ClockDriver->VideoRenderDeltaBeatF;
		}
	}
	return 0.0f;
}

const TArray<FSongSection>& UMusicClockComponent::GetSongSections() const
{
	return GetSongMaps().GetSections();
}

float UMusicClockComponent::GetCountInSeconds() const
{
	return GetSongMaps().GetCountInSeconds();
}

float UMusicClockComponent::TickToMs(float Tick) const
{
	return GetSongMaps().TickToMs(Tick);
}

float UMusicClockComponent::BeatToMs(float Beat) const
{
	return GetSongMaps().GetMsAtBeat(Beat);
}

float UMusicClockComponent::GetMsPerBeatAtMs(float Ms) const
{
	return GetSongMaps().GetMsPerBeatAtMs(Ms);
}

float UMusicClockComponent::GetNumBeatsInBarAtMs(float Ms) const
{
	return GetSongMaps().GetNumBeatsInPulseBarAtMs(Ms);
}

float UMusicClockComponent::GetBeatInBarAtMs(float Ms) const
{
	return GetSongMaps().GetBeatInPulseBarAtMs(Ms);
}

float UMusicClockComponent::BarToMs(float Bar) const
{
	if (const FTimeSignature* TimeSigAtBar = GetSongMaps().GetTimeSignatureAtBar(static_cast<int32>(Bar)))
	{
		return BeatToMs(static_cast<float>(TimeSigAtBar->Numerator) * Bar);
	}

	return 0.0f;
}

float UMusicClockComponent::GetMsPerBarAtMs(float Ms) const
{
	return GetSongMaps().GetMsPerBarAtMs(Ms);
}

FString UMusicClockComponent::GetSectionNameAtMs(float Ms) const
{
	return GetSongMaps().GetSectionNameAtMs(Ms);
}

float UMusicClockComponent::GetSectionLengthMsAtMs(float Ms) const
{
	return GetSongMaps().GetSectionLengthMsAtMs(Ms);
}

float UMusicClockComponent::GetSectionStartMsAtMs(float Ms) const
{
	return GetSongMaps().GetSectionStartMsAtMs(Ms);
}

float UMusicClockComponent::GetSectionEndMsAtMs(float Ms) const
{
	return GetSongMaps().GetSectionEndMsAtMs(Ms);
}

int32 UMusicClockComponent::GetNumSections() const
{
	return GetSongMaps().GetNumSections();
}

float UMusicClockComponent::GetSongLengthMs() const
{
	return GetSongMaps().GetSongLengthMs();
}

float UMusicClockComponent::GetSongLengthBeats() const
{
	return GetSongMaps().GetSongLengthBeats();
}

float UMusicClockComponent::GetSongLengthBars() const
{
	return GetSongMaps().GetSongLengthFractionalBars();
}

float UMusicClockComponent::GetSongRemainingMs(ECalibratedMusicTimebase Timebase) const
{
	const float SongLengthMs = GetSongMaps().GetSongLengthMs();
	return SongLengthMs <= 0.f ? 0.f : SongLengthMs - (GetSongPos(Timebase).SecondsIncludingCountIn * 1000.0f);
}

bool UMusicClockComponent::SeekedThisFrame(ECalibratedMusicTimebase Timebase) const
{
	if (ClockDriver)
	{
		return ClockDriver->SeekedThisFrame(Timebase);
	}
	return false;
}

bool UMusicClockComponent::LoopedThisFrame(ECalibratedMusicTimebase Timebase) const
{
	if (ClockDriver)
	{
		return ClockDriver->LoopedThisFrame(Timebase);
	}
	return false;
}

const ISongMapEvaluator& UMusicClockComponent::GetSongMaps() const
{
	const ISongMapEvaluator* SongMaps = ClockDriver ? ClockDriver->GetCurrentSongMapEvaluator() : nullptr;
	return SongMaps ? *SongMaps : DefaultMaps;
}

const FMidiSongPos& UMusicClockComponent::GetSongPos(ECalibratedMusicTimebase Timebase) const
{
	return GetCurrentSongPosInternal(Timebase);
}

const FMidiSongPos& UMusicClockComponent::GetPreviousSongPos(ECalibratedMusicTimebase Timebase) const
{
	return GetPreviousSongPosInternal(Timebase);
}

FMidiSongPos UMusicClockComponent::GetCurrentSmoothedAudioRenderSongPos() const
{
	return GetSongPos(ECalibratedMusicTimebase::AudioRenderTime);
}

FMidiSongPos UMusicClockComponent::GetPreviousSmoothedAudioRenderSongPos() const
{
	return GetPreviousSongPos(ECalibratedMusicTimebase::AudioRenderTime);
}

FMidiSongPos UMusicClockComponent::GetCurrentVideoRenderSongPos() const
{
	return GetSongPos(ECalibratedMusicTimebase::VideoRenderTime);
}

FMidiSongPos UMusicClockComponent::GetPreviousVideoRenderSongPos() const
{
	return GetPreviousSongPos(ECalibratedMusicTimebase::VideoRenderTime);
}

FMidiSongPos UMusicClockComponent::GetCurrentPlayerExperiencedSongPos() const
{
	return GetSongPos(ECalibratedMusicTimebase::ExperiencedTime);
}

FMidiSongPos UMusicClockComponent::GetPreviousPlayerExperiencedSongPos() const
{
	return GetPreviousSongPos(ECalibratedMusicTimebase::ExperiencedTime);
}

FMidiSongPos UMusicClockComponent::GetCurrentRawAudioRenderSongPos() const
{
	return GetSongPos(ECalibratedMusicTimebase::RawAudioRenderTime);
}

float UMusicClockComponent::MeasureSpanProgress(const FMusicalTimeSpan& Span, ECalibratedMusicTimebase Timebase) const
{
	return Span.CalcPositionInSpan(GetCurrentSongPosInternal(Timebase), GetSongMaps());
}

void UMusicClockComponent::BroadcastSongPosChanges(ECalibratedMusicTimebase Timebase)
{
	const FMidiSongPos& Basis = GetCurrentSongPosInternal(Timebase);

	// The basis for a clock can be invalid while the clock is still spinning up
	// (connecting to a metasound)
	// so check to make sure it's valid
	// otherwise we'll be broadcasting bar and beat events that don't make any sense
	// although this can mean the first Bar/Beat events will come in a fraction of a second late
	if (!Basis.IsValid())
	{
		return;
	}
	
	const int32 CurrBar = FMath::FloorToInt32(Basis.BarsIncludingCountIn);
	if (LastBroadcastBar != CurrBar)
	{
		BarEvent.Broadcast(Basis.Timestamp.Bar);
		LastBroadcastBar = CurrBar;
	}
	const int32 CurrBeat = FMath::FloorToInt32(Basis.BeatsIncludingCountIn);
	if (LastBroadcastBeat != CurrBeat)
	{
		BeatEvent.Broadcast(CurrBeat, FMath::FloorToInt32(Basis.Timestamp.Beat));
		LastBroadcastBeat = CurrBeat;
	}
	const FSongSection& SongSection = Basis.CurrentSongSection;
	if (LastBroadcastSongSection.StartTick != SongSection.StartTick || LastBroadcastSongSection.LengthTicks != SongSection.LengthTicks)
	{
		SectionEvent.Broadcast(SongSection.Name, SongSection.StartTick, SongSection.LengthTicks);
		LastBroadcastSongSection = FSongSection(SongSection.Name, SongSection.StartTick, SongSection.LengthTicks);
	}
}

const FMusicTimeDiscontinuityEvent* UMusicClockComponent::GetMusicTimeDiscontinuityEventInternal(ECalibratedMusicTimebase Timebase) const
{
	switch (Timebase)
	{
	case ECalibratedMusicTimebase::AudioRenderTime:
		return &AudioRenderMusicTimeDiscontinuityEvent;
	case ECalibratedMusicTimebase::ExperiencedTime:
		return &PlayerExperienceMusicTimeDiscontinuityEvent;
	case ECalibratedMusicTimebase::VideoRenderTime:
		return &VideoRenderMusicTimeDiscontinuityEvent;
	default:
		return nullptr;
	}
}

void UMusicClockComponent::BroadcastSeekLoopDetections(ECalibratedMusicTimebase Timebase) const
{
	if (!ClockDriver)
	{
		return;
	}
	
	if (!SeekedThisFrame(Timebase) && !LoopedThisFrame(Timebase))
	{
		return;
	}
	
	if (const FMusicTimeDiscontinuityEvent* Event = GetMusicTimeDiscontinuityEventInternal(Timebase))
	{
		const FMidiSongPos& PrevSongPos = GetPreviousSongPosInternal(Timebase);
		const FMidiSongPos& CurrentSongPos = GetCurrentSongPosInternal(Timebase);

		if (SeekedThisFrame(Timebase))
		{
			Event->Broadcast(EMusicTimeDiscontinuityType::Seek, PrevSongPos, CurrentSongPos);
		}
		if (LoopedThisFrame(Timebase))
		{
			Event->Broadcast(EMusicTimeDiscontinuityType::Loop, PrevSongPos, CurrentSongPos);
		}
	}
}

void UMusicClockComponent::MakeDefaultSongMap()
{
	DefaultMaps.EmptyAllMaps();
	DefaultMaps.Init(Harmonix::Midi::Constants::GTicksPerQuarterNoteInt);
	DefaultMaps.GetTempoMap().AddTempoInfoPoint(Harmonix::Midi::Constants::BPMToMidiTempo(DefaultTempo), 0);
	DefaultMaps.GetBarMap().AddTimeSignatureAtBarIncludingCountIn(0, DefaultTimeSignatureNum, DefaultTimeSignatureDenom);
}

const FMidiSongPos& UMusicClockComponent::GetCurrentSongPosInternal(ECalibratedMusicTimebase Timebase) const
{
	if (ClockDriver)
	{
		return ClockDriver->GetCurrentSongPos(Timebase);
	}
	static const FMidiSongPos PosZero;
	return PosZero;
}

const FMidiSongPos& UMusicClockComponent::GetPreviousSongPosInternal(ECalibratedMusicTimebase Timebase) const
{
	if (ClockDriver)
	{
		return ClockDriver->GetPreviousSongPos(Timebase);
	}

	static const FMidiSongPos PosZero;
	return PosZero;
}

FMusicalTime UMusicClockComponent::GetMusicalTime() const
{
	FMusicalTime Result;
	if (!ClockDriver)
	{
		return Result;
	}
	
	const FMidiSongPos& SongPos = ClockDriver->GetCurrentSongPos(ECalibratedMusicTimebase::VideoRenderTime);
	if (SongPos.TimeSigDenominator < 1)
	{
		return Result;
	}

	Result.Bar = SongPos.Timestamp.Bar - 1;
	Result.TicksPerBeat = (MusicalTime::TicksPerQuarterNote * 4 / SongPos.TimeSigDenominator);
	Result.TicksPerBar = Result.TicksPerBeat * SongPos.TimeSigNumerator;
	Result.TickInBar = static_cast<int32>((SongPos.Timestamp.Beat - 1.0f) * static_cast<float>(Result.TicksPerBeat));
	return Result;
}

int32 UMusicClockComponent::GetAbsoluteTickPosition() const
{
	// Note: Since this is an override of a function in the music environment system we need to
	// be sure we are using its standard ticks per quarter note units.
	const ISongMapEvaluator& MapEvaluator = GetSongMaps();
	float CurrentTick = MapEvaluator.FractionalBarIncludingCountInToTick(GetCurrentSongPosInternal(ECalibratedMusicTimebase::VideoRenderTime).BarsIncludingCountIn);
	return FMath::FloorToInt32((CurrentTick / static_cast<float>(MapEvaluator.GetTicksPerQuarterNote())) * static_cast<float>(MusicalTime::TicksPerQuarterNote));
}


FMusicalTime UMusicClockComponent::GetMusicalTimeWithSourceSpaceOffset(const FMusicalTime& Offset) const
{
	FMusicalTime Result;
	const ISongMapEvaluator& MapEvaluator = GetSongMaps();
	float CurrentTick = MapEvaluator.FractionalBarIncludingCountInToTick(GetCurrentSongPosInternal(ECalibratedMusicTimebase::VideoRenderTime).BarsIncludingCountIn);
	
	float OffsetTick = MapEvaluator.FractionalBarIncludingCountInToTick(static_cast<float>(Offset.FractionalBar()));
	float AdjustedTick = CurrentTick - OffsetTick;

	const FTimeSignature* TimeSignature = MapEvaluator.GetTimeSignatureAtTick(FMath::FloorToInt32(AdjustedTick));
	FMusicTimestamp Timestamp = MapEvaluator.TickToMusicTimestamp(AdjustedTick);
	Result.Bar = Timestamp.Bar - 1;
	Result.TicksPerBeat = (MusicalTime::TicksPerQuarterNote * 4 / TimeSignature->Denominator);
	Result.TicksPerBar = Result.TicksPerBeat * TimeSignature->Numerator;
	Result.TickInBar = static_cast<int32>((Timestamp.Beat - 1.0f) * static_cast<float>(Result.TicksPerBeat));
	return Result;
}

int32 UMusicClockComponent::GetAbsoluteTickPositionWithSourceSpaceOffset(const FMusicalTime& Offset) const
{
	// Note: Since this is an override of a function in the music environment system we need to
	// be sure we are using its standard ticks per quarter note units.
	const ISongMapEvaluator& MapEvaluator = GetSongMaps();
	float CurrentTick = MapEvaluator.FractionalBarIncludingCountInToTick(GetCurrentSongPosInternal(ECalibratedMusicTimebase::VideoRenderTime).BarsIncludingCountIn);
	float OffsetTick = MapEvaluator.FractionalBarIncludingCountInToTick(static_cast<float>(Offset.FractionalBar()));
	float AdjustedTick = CurrentTick - OffsetTick;
	return FMath::FloorToInt32((AdjustedTick / static_cast<float>(MapEvaluator.GetTicksPerQuarterNote())) * static_cast<float>(MusicalTime::TicksPerQuarterNote));
}

FMusicalTime UMusicClockComponent::Quantize(const FMusicalTime& MusicalTime, int32 QuantizationInterval, UFrameBasedMusicMap::EQuantizeDirection Direction) const
{
	FMusicalTime Result;
	const ISongMapEvaluator& MapEvaluator = GetSongMaps();
	float Tick = MapEvaluator.FractionalBarIncludingCountInToTick(static_cast<float>(MusicalTime.FractionalBar()));
	int32 QuantizedTick = MapEvaluator.QuantizeTickToNearestSubdivision(FMath::FloorToInt32(Tick), (EMidiFileQuantizeDirection)Direction, (EMidiClockSubdivisionQuantization) FrameBasedMusicMap::QuantiazationIntervalToQuartz(QuantizationInterval));
	FMusicTimestamp Timestamp = MapEvaluator.TickToMusicTimestamp(QuantizedTick);
	const FTimeSignature* TimeSignature = MapEvaluator.GetTimeSignatureAtTick(QuantizedTick);
	check(TimeSignature);
	Result.Bar = Timestamp.Bar - 1;
	Result.TicksPerBeat = (MusicalTime::TicksPerQuarterNote * 4 / TimeSignature->Denominator);
	Result.TicksPerBar = Result.TicksPerBeat * TimeSignature->Numerator;
	Result.TickInBar = static_cast<int32>((Timestamp.Beat - 1.0f) * static_cast<float>(Result.TicksPerBeat));
	return Result;
}

