// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PyperActor.h"
#include "NeuroEntity.h"
#include "SuperhotFusionBattle.generated.h"

/**
 * Battle State
 */
UENUM(BlueprintType)
enum class EBattleState : uint8
{
	Idle UMETA(DisplayName = "Idle"),
	Preparing UMETA(DisplayName = "Preparing"),
	Fighting UMETA(DisplayName = "Fighting"),
	Fusing UMETA(DisplayName = "Fusing"),
	Complete UMETA(DisplayName = "Complete")
};

/**
 * Battle Result
 */
USTRUCT(BlueprintType)
struct FBattleResult
{
	GENERATED_BODY()

	/** Winner name */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Battle")
	FString WinnerName;

	/** Pyper score */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Battle")
	float PyperScore;

	/** Neuro score */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Battle")
	float NeuroScore;

	/** Was it a fusion? */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Battle")
	bool bWasFusion;

	/** Fusion power level */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Battle")
	float FusionPower;

	FBattleResult()
		: PyperScore(0.0f)
		, NeuroScore(0.0f)
		, bWasFusion(false)
		, FusionPower(0.0f)
	{
	}
};

/**
 * Superhot Fusion Battle System
 * Implements battle and fusion mechanics between Pyper and Neuro entities
 * Features "superhot" time-dilation mechanics during critical moments
 */
UCLASS()
class NEUROFUSION_API ASuperhotFusionBattle : public AActor
{
	GENERATED_BODY()
	
public:	
	ASuperhotFusionBattle();

	/** Pyper participant */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Battle")
	APyperActor* PyperParticipant;

	/** Neuro participant */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Battle")
	ANeuroEntity* NeuroParticipant;

	/** Current battle state */
	UPROPERTY(BlueprintReadOnly, Category = "Battle")
	EBattleState BattleState;

	/** Battle result */
	UPROPERTY(BlueprintReadOnly, Category = "Battle")
	FBattleResult Result;

	/** Superhot time dilation factor */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Battle|Superhot")
	float SuperhotTimeDilation;

	/** Enable superhot mode */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Battle|Superhot")
	bool bSuperhotEnabled;

	/** Fusion probability threshold */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Battle|Fusion")
	float FusionThreshold;

	/** Initialize battle */
	UFUNCTION(BlueprintCallable, Category = "Battle")
	void InitializeBattle(APyperActor* Pyper, ANeuroEntity* Neuro);

	/** Start battle */
	UFUNCTION(BlueprintCallable, Category = "Battle")
	void StartBattle();

	/** Process battle round */
	UFUNCTION(BlueprintCallable, Category = "Battle")
	void ProcessBattleRound(float DeltaTime);

	/** Attempt fusion */
	UFUNCTION(BlueprintCallable, Category = "Battle")
	bool AttemptFusion();

	/** Get battle winner */
	UFUNCTION(BlueprintCallable, Category = "Battle")
	FString GetWinner() const;

	/** Enable/disable superhot mode */
	UFUNCTION(BlueprintCallable, Category = "Battle")
	void SetSuperhotMode(bool bEnabled);

	/** Get current battle intensity */
	UFUNCTION(BlueprintCallable, Category = "Battle")
	float GetBattleIntensity() const;

protected:
	virtual void BeginPlay() override;

public:	
	virtual void Tick(float DeltaTime) override;

private:
	/** Calculate combat scores */
	void CalculateScores();

	/** Check for fusion conditions */
	bool CheckFusionConditions() const;

	/** Apply superhot effects */
	void ApplySuperhotEffects();

	/** Execute fusion */
	void ExecuteFusion();

	float BattleTimer;
	float RoundTimer;
	int32 CurrentRound;
};
