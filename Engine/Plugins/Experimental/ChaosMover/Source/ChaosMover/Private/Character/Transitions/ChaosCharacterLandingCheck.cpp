// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosMover/Character/Transitions/ChaosCharacterLandingCheck.h"

#include "ChaosMover/ChaosMoverLog.h"
#include "ChaosMover/ChaosMoverSimulation.h"
#include "ChaosMover/ChaosMoverSimulationTypes.h"
#include "ChaosMover/Utilities/ChaosGroundMovementUtils.h"
#include "DefaultMovementSet/CharacterMoverSimulationTypes.h"
#include "MoveLibrary/FloorQueryUtils.h"
#include "MoveLibrary/WaterMovementUtils.h"
#include "MoverSimulationTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ChaosCharacterLandingCheck)

UChaosCharacterLandingCheck::UChaosCharacterLandingCheck(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSupportsAsync = true;
	bFirstSubStepOnly = true;

	TransitionToGroundMode = DefaultModeNames::Walking;
}

FTransitionEvalResult UChaosCharacterLandingCheck::Evaluate_Implementation(const FSimulationTickParams& Params) const
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
			const bool bIsMovingUp = bJumping || (VerticalVelocity > 0.0f);

			// Check for ground landing
			if (!TransitionToGroundMode.IsNone())
			{
				FFloorCheckResult FloorResult;
				if (Blackboard->TryGet(CommonBlackboard::LastFloorResult, FloorResult) && FloorResult.IsWalkableFloor())
				{
					const float TargetHeight = Mode->GetTargetHeight();
					const float FloorDistanceWithFloorNormal = FloorResult.HitResult.ImpactNormal.Dot(StartingSyncState->GetLocation_WorldSpace() - FloorResult.HitResult.ImpactPoint);
					const FVector LocalGroundVelocity = UChaosGroundMovementUtils::ComputeLocalGroundVelocity_Internal(StartingSyncState->GetLocation_WorldSpace(), FloorResult);
					const float RelativeVerticalVelocity = FloorResult.HitResult.ImpactNormal.Dot(LinearVelocity - LocalGroundVelocity);
					const float ProjectedFloorDistance = FloorDistanceWithFloorNormal + RelativeVerticalVelocity * DeltaSeconds;
					const bool bIsFloorWithinReach = ProjectedFloorDistance < TargetHeight + FloorDistanceTolerance + UE_KINDA_SMALL_NUMBER;
					const bool bIsMovingUpRelativeToFloor = (RelativeVerticalVelocity > UE_KINDA_SMALL_NUMBER) || bJumping;

					if (bIsFloorWithinReach && !bIsMovingUpRelativeToFloor)
					{
						if (Simulation->FindMovementModeByName(TransitionToGroundMode))
						{
							EvalResult.NextMode = TransitionToGroundMode;
						}
						else
						{
							UE_LOG(LogChaosMover, Warning, TEXT("Invalid ground mode name %s in ChaosCharacterLandingCheck. Cannot make transition"), *TransitionToGroundMode.ToString());
						}
						
						return EvalResult;
					}
				}
			}
		}
	}

	return EvalResult;
}

void UChaosCharacterLandingCheck::Trigger_Implementation(const FSimulationTickParams& Params)
{
	// Add a landed event to the simulation (will be broadcast on GT during post sim)
	if (UChaosMoverSimulation* Sim = GetSimulation_Mutable())
	{
		if (const UMoverBlackboard* Blackboard = Sim->GetBlackboard())
		{
			FFloorCheckResult FloorResult;
			if (Blackboard->TryGet(CommonBlackboard::LastFloorResult, FloorResult))
			{
				Sim->AddEvent(MakeShared<FLandedEventData>(Params.TimeStep.BaseSimTimeMs, FloorResult.HitResult, TransitionToGroundMode));
			}
		}
	}
}