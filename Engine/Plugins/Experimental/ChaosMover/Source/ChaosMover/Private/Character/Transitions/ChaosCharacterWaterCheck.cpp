// Copyright Epic Games, Inc. All Rights Reserved.
 
#include "ChaosMover/Character/Transitions/ChaosCharacterWaterCheck.h"
 
#include "CharacterMovementComponentAsync.h"
#include "ChaosMover/ChaosMoverLog.h"
#include "ChaosMover/ChaosMoverSimulation.h"
#include "ChaosMover/ChaosMoverSimulationTypes.h"
#include "MoveLibrary/WaterMovementUtils.h"
#include "MoverSimulationTypes.h"
 
#include UE_INLINE_GENERATED_CPP_BY_NAME(ChaosCharacterWaterCheck)
 
UChaosCharacterWaterCheck::UChaosCharacterWaterCheck(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSupportsAsync = true;
 
	WaterModeName = DefaultModeNames::Swimming;
	GroundModeName = DefaultModeNames::Walking;
	AirModeName = DefaultModeNames::Falling;
}
 
FTransitionEvalResult UChaosCharacterWaterCheck::Evaluate_Implementation(const FSimulationTickParams& Params) const
{
	check(Simulation);
 
	FTransitionEvalResult EvalResult;
 
	const FMoverTickStartData& StartState = Params.StartState;
 
	if (const FChaosMoverSimulationDefaultInputs* SimInputs = Simulation->GetLocalSimInput().FindDataByType<FChaosMoverSimulationDefaultInputs>())
	{
		const UMoverBlackboard* Blackboard = Simulation->GetBlackboard();
		check(Blackboard);
 
		const FCharacterDefaultInputs* CharacterInputs = StartState.InputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>();
		const FMoverDefaultSyncState* StartingSyncState = StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
		if (!CharacterInputs || !StartingSyncState)
		{
			return EvalResult;
		}
 
		if (const IChaosCharacterMovementModeInterface* Mode = Cast<IChaosCharacterMovementModeInterface>(Simulation->FindMovementModeByName(Params.StartState.SyncState.MovementMode)))
		{
			const float DeltaSeconds = Params.TimeStep.StepMs * 0.001f;
			const FVector LinearVelocity = StartingSyncState->GetVelocity_WorldSpace();
			const float VerticalVelocity = SimInputs->UpDir.Dot(LinearVelocity);
			const bool bJumping = CharacterInputs && CharacterInputs->bIsJumpJustPressed;
			const bool bIsMovingUp = bJumping || (VerticalVelocity > UE_KINDA_SMALL_NUMBER);
 
			FFindFloorResult FloorResult;
			FWaterCheckResult WaterResult;
 
			Blackboard->TryGet(CommonBlackboard::LastFloorResult, FloorResult);
			Blackboard->TryGet(CommonBlackboard::LastWaterResult, WaterResult);
			
			const float ProjectedImmersionDepth = WaterResult.WaterSplineData.ImmersionDepth - VerticalVelocity * DeltaSeconds;
			const bool bStartSwimming = ProjectedImmersionDepth > WaterModeStartImmersionDepth;
			const bool bInWater = StartState.SyncState.MovementMode == WaterModeName;
			const bool bStopSwimming = bInWater && (ProjectedImmersionDepth < WaterModeStopImmersionDepth);
 
			// Check if we need to enter/exit water mode
			if (WaterResult.IsSwimmableVolume() && bStartSwimming)
			{
				if (!WaterModeName.IsNone())
				{
					if (Simulation->FindMovementModeByName(WaterModeName))
					{
						EvalResult.NextMode = WaterModeName;
					}
					else
					{
						UE_LOG(LogChaosMover, Warning, TEXT("Invalid water mode name %s in ChaosCharacterWaterCheck. Cannot make transition"), *WaterModeName.ToString());
					}
					return EvalResult;
				}
			}
			else if (bStopSwimming)
			{
				const bool bIsWithinReach = FloorResult.FloorDist <= Mode->GetTargetHeight();
 
				if (FloorResult.IsWalkableFloor() && bIsWithinReach && !bIsMovingUp)
				{
					if (!GroundModeName.IsNone())
					{
						if (Simulation->FindMovementModeByName(GroundModeName))
						{
							EvalResult.NextMode = GroundModeName;
						}
						else
						{
							UE_LOG(LogChaosMover, Warning, TEXT("Invalid ground mode name %s in ChaosCharacterWaterCheck. Cannot make transition"), *GroundModeName.ToString());
						}
						return EvalResult;
					}
				}
				else
				{
					if (!AirModeName.IsNone())
					{
						if (Simulation->FindMovementModeByName(AirModeName))
						{
							EvalResult.NextMode = AirModeName;
						}
						else
						{
							UE_LOG(LogChaosMover, Warning, TEXT("Invalid air mode name %s in ChaosCharacterWaterCheck. Cannot make transition"), *AirModeName.ToString());
						}
						return EvalResult;
					}
				}
			}
		}
	}
 
	return EvalResult;
}
 
void UChaosCharacterWaterCheck::Trigger_Implementation(const FSimulationTickParams& Params)
{
}