#pragma once

#include "CoreMinimal.h"
#include "AGIComms.generated.h"

UENUM(BlueprintType)
enum class EPCGCommandType : uint8
{
	Spawn,
	Modify,
	Destroy
};

USTRUCT(BlueprintType)
struct FPCGCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PCG")
	EPCGCommandType CommandType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PCG")
	FString TargetGraph;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PCG")
	FVector Location;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PCG")
	FRotator Rotation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PCG")
	FVector Scale;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PCG")
	TMap<FString, FString> Parameters;
};

USTRUCT(BlueprintType)
struct FEchoStreamState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AGI")
	int32 StreamId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AGI")
	int32 CurrentLoopStep;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AGI")
	float CognitiveLoad;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AGI")
	float EmotionalValence;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AGI")
	float EchoStreamDivergence;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AGI")
	float EntropyLevel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AGI")
	float TemporalCoherence;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AGI")
	float ConsciousnessStreamPhase;
};

USTRUCT(BlueprintType)
struct FContextTerm
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AGI")
	FString TermId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AGI")
	float Weight;
};

USTRUCT(BlueprintType)
struct FGlobalContext
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AGI")
	int32 Level;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AGI")
	TArray<FContextTerm> Terms;
};

USTRUCT(BlueprintType)
struct FCognitiveState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AGI")
	TArray<FEchoStreamState> EchoStreams;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AGI")
	FGlobalContext GlobalContext;
};

USTRUCT(BlueprintType)
struct FAGIStateUpdateMessage
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AGI")
	float Timestamp;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AGI")
	FCognitiveState CognitiveState;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PCG")
	TArray<FPCGCommand> PCGCommands;
};
