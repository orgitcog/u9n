#include "AGICoreCommunication.h"
#include "AGIPCGManager.h"
#include "CognitiveCycleManager.h"
#include "DeepTreeEchoCore.h"
#include "EngineUtils.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogAGICoreComm, Log, All);

AAGICoreCommunication::AAGICoreCommunication()
{
	PrimaryActorTick.bCanEverTick = true;
	PCGManager = nullptr;
	CognitiveSourceActor = nullptr;
	bSnapshotCognitiveComponents = true;
	SnapshotInterval = 0.0f;
	CycleManager = nullptr;
	EchoCore = nullptr;
	TimeAccumulator = 0.0f;
	SnapshotAccumulator = 0.0f;
	bLoggedMissingSource = false;
}

void AAGICoreCommunication::BeginPlay()
{
	Super::BeginPlay();
	BindCognitiveSources();
}

void AAGICoreCommunication::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	TimeAccumulator += DeltaTime;

	if (!bSnapshotCognitiveComponents)
	{
		return;
	}

	if (!HasCognitiveSource())
	{
		if (!bLoggedMissingSource)
		{
			UE_LOG(LogAGICoreComm, Warning,
				TEXT("%s: no cognitive source bound (no UCognitiveCycleManager or UDeepTreeEchoCore on %s or in the world). ")
				TEXT("No state updates will be broadcast until one is added or ReceiveStateUpdate() is called."),
				*GetName(),
				CognitiveSourceActor ? *CognitiveSourceActor->GetName() : *GetName());
			bLoggedMissingSource = true;
		}
		return;
	}

	SnapshotAccumulator += DeltaTime;
	if (SnapshotInterval > 0.0f && SnapshotAccumulator < SnapshotInterval)
	{
		return;
	}
	SnapshotAccumulator = 0.0f;

	FAGIStateUpdateMessage Message;
	if (BuildMessageFromComponents(Message))
	{
		Dispatch(Message);
	}
}

void AAGICoreCommunication::ReceiveStateUpdate(const FAGIStateUpdateMessage& StateUpdateMessage)
{
	Dispatch(StateUpdateMessage);
}

void AAGICoreCommunication::BindCognitiveSources()
{
	CycleManager = nullptr;
	EchoCore = nullptr;
	bLoggedMissingSource = false;

	TArray<AActor*> Candidates;
	if (CognitiveSourceActor)
	{
		Candidates.Add(CognitiveSourceActor);
	}
	Candidates.Add(this);

	for (AActor* Candidate : Candidates)
	{
		if (!CycleManager)
		{
			CycleManager = Candidate->FindComponentByClass<UCognitiveCycleManager>();
		}
		if (!EchoCore)
		{
			EchoCore = Candidate->FindComponentByClass<UDeepTreeEchoCore>();
		}
	}

	// Without an explicit source actor, accept the first actor in the world that carries a core.
	if (!CycleManager && !EchoCore && !CognitiveSourceActor && GetWorld())
	{
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		{
			AActor* Actor = *It;
			if (!Actor || Actor == this)
			{
				continue;
			}
			UDeepTreeEchoCore* FoundCore = Actor->FindComponentByClass<UDeepTreeEchoCore>();
			UCognitiveCycleManager* FoundCycle = Actor->FindComponentByClass<UCognitiveCycleManager>();
			if (FoundCore || FoundCycle)
			{
				EchoCore = FoundCore;
				CycleManager = FoundCycle;
				break;
			}
		}
	}

	// The core keeps a reference to the clock it found on its own owner.
	if (!CycleManager && EchoCore && EchoCore->CycleManager)
	{
		CycleManager = EchoCore->CycleManager;
	}

	if (HasCognitiveSource())
	{
		UE_LOG(LogAGICoreComm, Log, TEXT("%s: bound cognitive source (cycle manager: %s, echo core: %s)."),
			*GetName(),
			CycleManager ? *CycleManager->GetOwner()->GetName() : TEXT("none"),
			EchoCore ? *EchoCore->GetOwner()->GetName() : TEXT("none"));
	}
}

bool AAGICoreCommunication::HasCognitiveSource() const
{
	return CycleManager != nullptr || EchoCore != nullptr;
}

bool AAGICoreCommunication::BuildMessageFromComponents(FAGIStateUpdateMessage& OutMessage) const
{
	if (!HasCognitiveSource())
	{
		return false;
	}

	OutMessage.Timestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : TimeAccumulator;

	// Valence comes from the embodied somatic markers when the core publishes one under that name.
	float Valence = 0.0f;
	bool bHasValence = false;
	if (EchoCore)
	{
		for (const TPair<FString, float>& Marker : EchoCore->CognitionState4E.SomaticMarkers)
		{
			if (Marker.Key.Contains(TEXT("Valence"), ESearchCase::IgnoreCase))
			{
				Valence = FMath::Clamp(Marker.Value, -1.0f, 1.0f) * 100.0f;
				bHasValence = true;
				break;
			}
		}
	}

	if (CycleManager)
	{
		const FCognitiveCycleState CycleState = CycleManager->GetCycleState();
		const float StepFraction = FMath::Frac(FMath::Clamp(CycleState.CycleProgress, 0.0f, 1.0f) * 12.0f);

		for (int32 Index = 0; Index < CycleState.Streams.Num(); ++Index)
		{
			const FStreamState& Stream = CycleState.Streams[Index];

			FEchoStreamState StreamState;
			StreamState.StreamId = Index;
			StreamState.CurrentLoopStep = FMath::Clamp(Stream.CurrentStep, 1, 12);
			StreamState.CognitiveLoad = FMath::Clamp(Stream.ActivationLevel, 0.0f, 1.0f) * 100.0f;
			StreamState.EmotionalValence = bHasValence ? Valence : 0.0f;
			StreamState.EchoStreamDivergence = (1.0f - FMath::Clamp(Stream.InterStreamCoherence, 0.0f, 1.0f)) * 100.0f;
			StreamState.EntropyLevel = ComputeActivationEntropy(Stream.ReservoirState);
			StreamState.TemporalCoherence = FMath::Clamp(Stream.Coherence, 0.0f, 1.0f) * 100.0f;
			StreamState.ConsciousnessStreamPhase = ((StreamState.CurrentLoopStep - 1) + StepFraction) * 30.0f;
			OutMessage.CognitiveState.EchoStreams.Add(StreamState);

			// A stream's current focus is a real context term weighted by its activation.
			if (!Stream.CurrentFocus.IsEmpty())
			{
				FContextTerm Term;
				Term.TermId = Stream.CurrentFocus;
				Term.Weight = FMath::Clamp(Stream.ActivationLevel, 0.0f, 1.0f);
				OutMessage.CognitiveState.GlobalContext.Terms.Add(Term);
			}
		}

		OutMessage.CognitiveState.GlobalContext.Level = CycleManager->GetCurrentStepConfig().NestingLevel;
	}

	if (EchoCore)
	{
		OutMessage.CognitiveState.GlobalContext.Level = EchoCore->GetCurrentNestingLevel();

		for (const TPair<FString, float>& Allocation : EchoCore->RelevanceState.AttentionAllocation)
		{
			FContextTerm Term;
			Term.TermId = Allocation.Key;
			Term.Weight = FMath::Clamp(Allocation.Value, 0.0f, 1.0f);
			OutMessage.CognitiveState.GlobalContext.Terms.Add(Term);
		}
	}

	// PCG commands are never derived here; only the core itself may issue them through ReceiveStateUpdate().
	OutMessage.PCGCommands.Reset();

	return OutMessage.CognitiveState.EchoStreams.Num() > 0 || OutMessage.CognitiveState.GlobalContext.Terms.Num() > 0;
}

void AAGICoreCommunication::Dispatch(const FAGIStateUpdateMessage& Message)
{
	LastStateUpdate = Message;
	OnAGIStateUpdate.Broadcast(Message);

	if (PCGManager)
	{
		for (const FPCGCommand& Command : Message.PCGCommands)
		{
			PCGManager->ExecutePCGCommand(Command);
		}
	}
}

float AAGICoreCommunication::ComputeActivationEntropy(const TArray<float>& Activations)
{
	if (Activations.Num() < 2)
	{
		return 0.0f;
	}

	float Total = 0.0f;
	for (float Value : Activations)
	{
		Total += FMath::Abs(Value);
	}
	if (Total <= KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	float Entropy = 0.0f;
	for (float Value : Activations)
	{
		const float P = FMath::Abs(Value) / Total;
		if (P > KINDA_SMALL_NUMBER)
		{
			Entropy -= P * FMath::Loge(P);
		}
	}

	const float MaxEntropy = FMath::Loge(static_cast<float>(Activations.Num()));
	return MaxEntropy > KINDA_SMALL_NUMBER ? FMath::Clamp(Entropy / MaxEntropy, 0.0f, 1.0f) * 100.0f : 0.0f;
}
