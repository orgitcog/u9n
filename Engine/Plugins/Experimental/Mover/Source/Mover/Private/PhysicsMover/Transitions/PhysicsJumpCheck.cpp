// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsMover/Transitions/PhysicsJumpCheck.h"

#include "Chaos/PhysicsObject.h"
#include "Chaos/PhysicsObjectInternalInterface.h"
#include "DefaultMovementSet/InstantMovementEffects/BasicInstantMovementEffects.h"
#include "MoverComponent.h"
#include "MoveLibrary/FloorQueryUtils.h"
#include "PhysicsMover/InstantMovementEffects/ApplyVelocityPhysicsMovementEffect.h"
#include "PhysicsMover/PhysicsMovementUtils.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace Private
{
	static void ApplyImpulse(Chaos::FPBDRigidParticleHandle* Particle, const Chaos::FVec3& Impulse, const Chaos::FVec3& Location)
	{
		Chaos::FRigidTransform3 ComTransform = Particle->GetTransformXRCom();
		const Chaos::FVec3 Offset = Location - ComTransform.GetLocation();
		Particle->SetW(Particle->GetW() + Particle->InvI() * Offset.Cross(Impulse));
		Particle->SetV(Particle->GetV() + Particle->InvM() * Impulse);
	}
}

UPhysicsJumpCheck::UPhysicsJumpCheck(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bFirstSubStepOnly = true;
	TransitionToMode = DefaultModeNames::Falling;
}

FTransitionEvalResult UPhysicsJumpCheck::Evaluate_Implementation(const FSimulationTickParams& Params) const
{
	FTransitionEvalResult EvalResult; 

	const FMoverTickStartData& StartState = Params.StartState;
	if (const FCharacterDefaultInputs* CharacterInputs = StartState.InputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>())
	{
		if (CharacterInputs->bIsJumpJustPressed)
		{
			EvalResult.NextMode = TransitionToMode;
		}
	}

	return EvalResult;
}

void UPhysicsJumpCheck::Trigger_Implementation(const FSimulationTickParams& Params)
{
	TSharedPtr<FApplyVelocityPhysicsEffect> JumpMove = MakeShared<FApplyVelocityPhysicsEffect>();

	if (UMoverComponent* MoverComp = Params.MovingComps.MoverComponent.Get())
	{
		JumpMove->VelocityToApply = FVector(JumpUpwardsSpeed * MoverComp->GetUpDirection());
		JumpMove->bAdditiveVelocity = true;
		Params.MovingComps.MoverComponent->QueueInstantMovementEffect(JumpMove);

		if (FractionalGroundReactionImpulse > 0.0f)
		{
			Chaos::FPBDRigidParticleHandle* CharacterParticle = UPhysicsMovementUtils::GetRigidParticleHandleFromComponent(Params.MovingComps.UpdatedPrimitive.Get());
			Chaos::FPBDRigidParticleHandle* GroundParticle = nullptr;
			FHitResult HitResult;
			if (MoverComp->TryGetFloorCheckHitResult(HitResult))
			{
				GroundParticle = UPhysicsMovementUtils::GetRigidParticleHandleFromHitResult(HitResult);
			}

			if (GroundParticle && GroundParticle->IsDynamic() && CharacterParticle)
			{
				const Chaos::FVec3 Impulse = -FractionalGroundReactionImpulse * CharacterParticle->M() * JumpUpwardsSpeed * MoverComp->GetUpDirection();
				Private::ApplyImpulse(GroundParticle, Impulse, HitResult.ImpactPoint);
			}
		}
	}
}

#if WITH_EDITOR
EDataValidationResult UPhysicsJumpCheck::IsDataValid(FDataValidationContext& Context) const
{
	return Super::IsDataValid(Context);
}
#endif // WITH_EDITOR
