// Copyright Epic Games, Inc. All Rights Reserved.

#include "SuperhotFusionBattle.h"
#include "Kismet/GameplayStatics.h"

ASuperhotFusionBattle::ASuperhotFusionBattle()
	: PyperParticipant(nullptr)
	, NeuroParticipant(nullptr)
	, BattleState(EBattleState::Idle)
	, SuperhotTimeDilation(0.1f)
	, bSuperhotEnabled(true)
	, FusionThreshold(0.85f)
	, BattleTimer(0.0f)
	, RoundTimer(0.0f)
	, CurrentRound(0)
{
	PrimaryActorTick.bCanEverTick = true;
}

void ASuperhotFusionBattle::BeginPlay()
{
	Super::BeginPlay();
	
	UE_LOG(LogTemp, Log, TEXT("SuperhotFusionBattle: Battle system initialized"));
}

void ASuperhotFusionBattle::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (BattleState == EBattleState::Fighting)
	{
		ProcessBattleRound(DeltaTime);
	}
}

void ASuperhotFusionBattle::InitializeBattle(APyperActor* Pyper, ANeuroEntity* Neuro)
{
	PyperParticipant = Pyper;
	NeuroParticipant = Neuro;
	
	BattleState = EBattleState::Preparing;
	BattleTimer = 0.0f;
	CurrentRound = 0;
	
	Result = FBattleResult();
	
	UE_LOG(LogTemp, Log, TEXT("SuperhotFusionBattle: Battle initialized - Pyper vs Neuro"));
}

void ASuperhotFusionBattle::StartBattle()
{
	if (!PyperParticipant || !NeuroParticipant)
	{
		UE_LOG(LogTemp, Warning, TEXT("SuperhotFusionBattle: Cannot start battle - missing participants"));
		return;
	}

	BattleState = EBattleState::Fighting;
	CurrentRound = 1;
	RoundTimer = 0.0f;
	
	UE_LOG(LogTemp, Log, TEXT("SuperhotFusionBattle: BATTLE START! Round %d"), CurrentRound);
	
	// Trigger superhot mode at battle start
	if (bSuperhotEnabled)
	{
		SetSuperhotMode(true);
	}
}

void ASuperhotFusionBattle::ProcessBattleRound(float DeltaTime)
{
	BattleTimer += DeltaTime;
	RoundTimer += DeltaTime;

	// Calculate current scores
	CalculateScores();

	// Check battle intensity for superhot activation
	float Intensity = GetBattleIntensity();
	if (Intensity > 0.8f && bSuperhotEnabled)
	{
		ApplySuperhotEffects();
	}

	// Check for fusion conditions
	if (CheckFusionConditions())
	{
		if (AttemptFusion())
		{
			BattleState = EBattleState::Fusing;
			ExecuteFusion();
			return;
		}
	}

	// Process round end
	if (RoundTimer >= 5.0f)
	{
		CurrentRound++;
		RoundTimer = 0.0f;
		
		UE_LOG(LogTemp, Log, TEXT("SuperhotFusionBattle: Round %d - Pyper: %.1f | Neuro: %.1f"), 
			CurrentRound, Result.PyperScore, Result.NeuroScore);
		
		// Check for battle end
		if (CurrentRound > 10 || FMath::Abs(Result.PyperScore - Result.NeuroScore) > 100.0f)
		{
			BattleState = EBattleState::Complete;
			Result.WinnerName = Result.PyperScore > Result.NeuroScore ? TEXT("Pyper") : TEXT("Neuro");
			
			UE_LOG(LogTemp, Log, TEXT("SuperhotFusionBattle: BATTLE COMPLETE! Winner: %s"), 
				*Result.WinnerName);
			
			// Restore normal time
			if (bSuperhotEnabled)
			{
				SetSuperhotMode(false);
			}
		}
	}
}

bool ASuperhotFusionBattle::AttemptFusion()
{
	if (!PyperParticipant || !NeuroParticipant)
	{
		return false;
	}

	// Calculate fusion probability based on scores similarity
	float ScoreDifference = FMath::Abs(Result.PyperScore - Result.NeuroScore);
	float FusionProbability = 1.0f - (ScoreDifference / 200.0f);
	
	if (FusionProbability >= FusionThreshold)
	{
		UE_LOG(LogTemp, Log, TEXT("SuperhotFusionBattle: FUSION TRIGGERED! Probability: %.2f"), 
			FusionProbability);
		return true;
	}

	return false;
}

FString ASuperhotFusionBattle::GetWinner() const
{
	return Result.WinnerName;
}

void ASuperhotFusionBattle::SetSuperhotMode(bool bEnabled)
{
	if (bEnabled)
	{
		// Slow down time
		UGameplayStatics::SetGlobalTimeDilation(GetWorld(), SuperhotTimeDilation);
		UE_LOG(LogTemp, Log, TEXT("SuperhotFusionBattle: SUPERHOT MODE ACTIVATED - Time: %.2f"), 
			SuperhotTimeDilation);
	}
	else
	{
		// Restore normal time
		UGameplayStatics::SetGlobalTimeDilation(GetWorld(), 1.0f);
		UE_LOG(LogTemp, Log, TEXT("SuperhotFusionBattle: Normal time restored"));
	}
}

float ASuperhotFusionBattle::GetBattleIntensity() const
{
	// Calculate intensity based on score competition and battle duration
	float ScoreIntensity = FMath::Min(Result.PyperScore + Result.NeuroScore, 200.0f) / 200.0f;
	float TimeIntensity = FMath::Min(BattleTimer / 30.0f, 1.0f);
	
	return (ScoreIntensity + TimeIntensity) * 0.5f;
}

void ASuperhotFusionBattle::CalculateScores()
{
	if (!PyperParticipant || !NeuroParticipant)
	{
		return;
	}

	// Calculate Pyper score based on cognitive abilities
	float PyperPersonalityBonus = 0.0f;
	PyperPersonalityBonus += PyperParticipant->PersonalityTraits.PhilosophicalDepth * 0.3f;
	PyperPersonalityBonus += PyperParticipant->PersonalityTraits.PlayfulWit * 0.2f;
	PyperPersonalityBonus += PyperParticipant->PersonalityTraits.MysteriousVision * 0.25f;
	PyperPersonalityBonus += PyperParticipant->PersonalityTraits.InventiveSpirit * 0.25f;
	
	float PyperCognitiveBonus = 0.0f;
	PyperCognitiveBonus += PyperParticipant->CognitiveState.AttentionFocus * 20.0f;
	PyperCognitiveBonus += PyperParticipant->CognitiveState.EnergyLevel * 20.0f;
	PyperCognitiveBonus += (1.0f - PyperParticipant->CognitiveState.CognitiveLoad) * 10.0f;

	Result.PyperScore = PyperPersonalityBonus + PyperCognitiveBonus;

	// Calculate Neuro score
	Result.NeuroScore = NeuroParticipant->GetCombatPower();
	
	// Add randomness for dynamic battles
	Result.PyperScore += FMath::RandRange(-5.0f, 5.0f);
	Result.NeuroScore += FMath::RandRange(-5.0f, 5.0f);
}

bool ASuperhotFusionBattle::CheckFusionConditions() const
{
	if (!PyperParticipant || !NeuroParticipant)
	{
		return false;
	}

	// Check if scores are very close
	float ScoreDifference = FMath::Abs(Result.PyperScore - Result.NeuroScore);
	
	// Check if battle has been going long enough
	bool bLongBattle = CurrentRound >= 5;
	
	// Check if both entities are at high power
	bool bHighPower = Result.PyperScore > 80.0f && Result.NeuroScore > 80.0f;
	
	return (ScoreDifference < 10.0f) && bLongBattle && bHighPower;
}

void ASuperhotFusionBattle::ApplySuperhotEffects()
{
	// Apply time dilation during intense moments
	float CurrentIntensity = GetBattleIntensity();
	float TimeDilation = FMath::Lerp(1.0f, SuperhotTimeDilation, CurrentIntensity);
	
	UGameplayStatics::SetGlobalTimeDilation(GetWorld(), TimeDilation);
}

void ASuperhotFusionBattle::ExecuteFusion()
{
	if (!PyperParticipant || !NeuroParticipant)
	{
		return;
	}

	// Calculate fusion power
	float PyperContribution = Result.PyperScore * 0.6f;
	float NeuroContribution = Result.NeuroScore * 0.4f;
	Result.FusionPower = PyperContribution + NeuroContribution;
	
	Result.bWasFusion = true;
	Result.WinnerName = TEXT("Pyper-Neuro Fusion");
	
	BattleState = EBattleState::Complete;
	
	// Create dramatic effect with superhot
	SetSuperhotMode(true);
	
	UE_LOG(LogTemp, Log, TEXT("SuperhotFusionBattle: *** FUSION COMPLETE ***"));
	UE_LOG(LogTemp, Log, TEXT("SuperhotFusionBattle: Fusion Power Level: %.1f"), Result.FusionPower);
	UE_LOG(LogTemp, Log, TEXT("SuperhotFusionBattle: A new entity has been born from Pyper and Neuro!"));
	
	// Restore time after a moment
	FTimerHandle TimerHandle;
	GetWorld()->GetTimerManager().SetTimer(TimerHandle, [this]()
	{
		SetSuperhotMode(false);
	}, 2.0f, false);
}
