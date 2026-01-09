// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsMover/Transitions/PhysicsLaunchCheck.h"

#include "Chaos/PhysicsObject.h"
#include "Chaos/PhysicsObjectInternalInterface.h"
#include "MoverComponent.h"
#include "MoveLibrary/FloorQueryUtils.h"
#include "PhysicsMover/InstantMovementEffects/ApplyVelocityPhysicsMovementEffect.h"
#include "PhysicsMover/PhysicsMoverSimulationTypes.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

UPhysicsLaunchCheck::UPhysicsLaunchCheck(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bFirstSubStepOnly = true;
	TransitionToMode = NAME_None;
}

FTransitionEvalResult UPhysicsLaunchCheck::Evaluate_Implementation(const FSimulationTickParams& Params) const
{
	FTransitionEvalResult EvalResult;

	if (const FMoverLaunchInputs* LaunchInputs = Params.StartState.InputCmd.InputCollection.FindDataByType<FMoverLaunchInputs>())
	{
		if (TransitionToMode.IsNone())
		{
			EvalResult.NextMode = Params.StartState.SyncState.MovementMode;
		}
		else
		{
			EvalResult.NextMode = TransitionToMode;
		}
	}

	return EvalResult;
}

void UPhysicsLaunchCheck::Trigger_Implementation(const FSimulationTickParams& Params)
{
	if (UMoverComponent* MoverComp = Params.MovingComps.MoverComponent.Get())
	{
		if (const FMoverLaunchInputs* LaunchInputs = Params.StartState.InputCmd.InputCollection.FindDataByType<FMoverLaunchInputs>())
		{
			TSharedPtr<FApplyVelocityPhysicsEffect> ApplyVelocityEffect = MakeShared<FApplyVelocityPhysicsEffect>();
			ApplyVelocityEffect->bAdditiveVelocity = LaunchInputs->Mode == EMoverLaunchVelocityMode::Additive;
			ApplyVelocityEffect->VelocityToApply = LaunchInputs->LaunchVelocity;
			ApplyVelocityEffect->ForceMovementMode = TransitionToMode;
			MoverComp->QueueInstantMovementEffect(ApplyVelocityEffect);
		}
	}
}

#if WITH_EDITOR
EDataValidationResult UPhysicsLaunchCheck::IsDataValid(FDataValidationContext& Context) const
{
	return Super::IsDataValid(Context);
}
#endif // WITH_EDITOR
