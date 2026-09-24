#include "AGICoreCommunication.h"
#include "Kismet/KismetMathLibrary.h"
#include "AGIPCGManager.h"

AAGICoreCommunication::AAGICoreCommunication()
{
	PrimaryActorTick.bCanEverTick = true;
	TimeAccumulator = 0.0f;
}

void AAGICoreCommunication::BeginPlay()
{
	Super::BeginPlay();
}

void AAGICoreCommunication::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	GenerateMockStateUpdate(DeltaTime);
}

void AAGICoreCommunication::GenerateMockStateUpdate(float DeltaTime)
{
	TimeAccumulator += DeltaTime;

	FAGIStateUpdateMessage Message;
	Message.Timestamp = TimeAccumulator;

	for (int32 i = 0; i < 3; ++i)
	{
		FEchoStreamState StreamState;
		StreamState.StreamId = i;
		float Phase = FMath::Fmod(TimeAccumulator * 3.0f + (i * 4.0f), 12.0f);
		StreamState.CurrentLoopStep = FMath::FloorToInt(Phase) + 1;
		UpdateStateFromLoopStep(StreamState, Phase);
		Message.CognitiveState.EchoStreams.Add(StreamState);
	}

	Message.CognitiveState.GlobalContext.Level = 3;
	for (int32 i = 0; i < 9; ++i)
	{
		FContextTerm Term;
		Term.TermId = FString::Printf(TEXT("Term_%d"), i);
		Term.Weight = FMath::RandRange(0.0f, 1.0f);
		Message.CognitiveState.GlobalContext.Terms.Add(Term);
	}

	if (FMath::Fmod(TimeAccumulator, 5.0f) < DeltaTime)
	{
		FPCGCommand Command;
		Command.CommandType = EPCGCommandType::Spawn;
		Command.TargetGraph = TEXT("Forest_Basic");
		Command.Location = FVector(FMath::RandRange(-2000.0f, 2000.0f), FMath::RandRange(-2000.0f, 2000.0f), 0.0f);
        Command.Scale = FVector(FMath::RandRange(0.5f, 1.5f));
		Message.PCGCommands.Add(Command);
	}

	OnAGIStateUpdate.Broadcast(Message);

	if (PCGManager)
	{
		for (const FPCGCommand& Command : Message.PCGCommands)
		{
			PCGManager->ExecutePCGCommand(Command);
		}
	}
}

void AAGICoreCommunication::UpdateStateFromLoopStep(FEchoStreamState& StreamState, float Phase)
{
    int32 Step = StreamState.CurrentLoopStep;

    if (Step == 1 || Step == 7)
    {
        StreamState.CognitiveLoad = FMath::RandRange(70.0f, 90.0f);
        StreamState.EmotionalValence = FMath::RandRange(-10.0f, 10.0f);
        StreamState.EchoStreamDivergence = FMath::RandRange(40.0f, 60.0f);
        StreamState.EntropyLevel = FMath::RandRange(50.0f, 70.0f);
        StreamState.TemporalCoherence = FMath::RandRange(60.0f, 80.0f);
    }
    else if (Step >= 2 && Step <= 6)
    {
        StreamState.CognitiveLoad = FMath::RandRange(40.0f, 60.0f);
        StreamState.EmotionalValence = FMath::Lerp(-50.0f, 50.0f, (Step - 2) / 4.0f);
        StreamState.EchoStreamDivergence = FMath::RandRange(10.0f, 30.0f);
        StreamState.EntropyLevel = FMath::RandRange(20.0f, 40.0f);
        StreamState.TemporalCoherence = FMath::RandRange(80.0f, 100.0f);
    }
    else
    {
        StreamState.CognitiveLoad = FMath::RandRange(60.0f, 80.0f);
        StreamState.EmotionalValence = FMath::Lerp(50.0f, -50.0f, (Step - 8) / 4.0f);
        StreamState.EchoStreamDivergence = FMath::RandRange(30.0f, 50.0f);
        StreamState.EntropyLevel = FMath::RandRange(40.0f, 60.0f);
        StreamState.TemporalCoherence = FMath::RandRange(70.0f, 90.0f);
    }

    StreamState.ConsciousnessStreamPhase = Phase * 30.0f;
}
