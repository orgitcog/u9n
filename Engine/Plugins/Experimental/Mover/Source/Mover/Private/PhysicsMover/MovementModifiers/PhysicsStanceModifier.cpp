// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsMover/MovementModifiers/PhysicsStanceModifier.h"

#include "Components/CapsuleComponent.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "DefaultMovementSet/Settings/StanceSettings.h"
#include "MoveLibrary/MovementUtils.h"
#include "PhysicsMover/Modes/PhysicsDrivenWalkingMode.h"
#include "PhysicsMover/Modes/PhysicsDrivenFallingMode.h"


void FPhysicsStanceModifier::OnStart(UMoverComponent* MoverComp, const FMoverTimeStep& TimeStep, const FMoverSyncState& SyncState, const FMoverAuxStateContext& AuxState)
{
	if (UStanceSettings* StanceSettings = MoverComp->FindSharedSettings_Mutable<UStanceSettings>())
	{
		if (const UCapsuleComponent* CapsuleComponent = Cast<UCapsuleComponent>(MoverComp->GetUpdatedComponent()))
		{
			if (const UCapsuleComponent* OriginalCapsule = UMovementUtils::GetOriginalComponentType<UCapsuleComponent>(MoverComp->GetOwner()))
			{
				const float OriginalHalfHeight = OriginalCapsule->GetScaledCapsuleHalfHeight();
				const float CrouchedHalfHeight = StanceSettings->CrouchHalfHeight;
				const float CrouchedEyeHeight = StanceSettings->CrouchedEyeHeight;
				
				// Adjust the TargetHeight of all the relevant MovementModes
				UpdateTargetHeight<UPhysicsDrivenWalkingMode>(MoverComp, OriginalHalfHeight, CrouchedHalfHeight, true);
				UpdateTargetHeight<UPhysicsDrivenFallingMode>(MoverComp, OriginalHalfHeight, CrouchedHalfHeight, true);

				ApplyMovementSettings(MoverComp);
			}
		}
	}

	// Ensures crouching, especially if triggered through state syncing (rollbacks, etc.)  
	if (UCharacterMoverComponent* CharMoverComp = Cast<UCharacterMoverComponent>(MoverComp))
	{
		CharMoverComp->Crouch();
	}
}

void FPhysicsStanceModifier::OnEnd(UMoverComponent* MoverComp, const FMoverTimeStep& TimeStep, const FMoverSyncState& SyncState, const FMoverAuxStateContext& AuxState)
{
	if (UStanceSettings* StanceSettings = MoverComp->FindSharedSettings_Mutable<UStanceSettings>())
	{
		if (UCapsuleComponent* CapsuleComponent = Cast<UCapsuleComponent>(MoverComp->GetUpdatedComponent()))
		{
			if (const UCapsuleComponent* OriginalCapsule = UMovementUtils::GetOriginalComponentType<UCapsuleComponent>(MoverComp->GetOwner()))
			{
				const float OriginalHalfHeight = OriginalCapsule->GetScaledCapsuleHalfHeight();
				const float CrouchedHalfHeight = StanceSettings->CrouchHalfHeight;

				// Adjust the TargetHeight of all the relevant MovementModes
				UpdateTargetHeight<UPhysicsDrivenWalkingMode>(MoverComp, OriginalHalfHeight, CrouchedHalfHeight, false);
				UpdateTargetHeight<UPhysicsDrivenFallingMode>(MoverComp, OriginalHalfHeight, CrouchedHalfHeight, false);

				RevertMovementSettings(MoverComp);
			}
		}
	}

	// Ensures uncrouching, especially if triggered through state syncing (rollbacks, etc.)  
	if (UCharacterMoverComponent* CharMoverComp = Cast<UCharacterMoverComponent>(MoverComp))
	{
		CharMoverComp->UnCrouch();
	}
}

bool FPhysicsStanceModifier::CanExpand_Internal(UMoverComponent* MoverComponent, USceneComponent* UpdatedComp, const FMoverSyncState& SyncState) const
{
	float StandingHalfHeight = 90;
	float CurrentHalfHeight = 55;

	bool bEncroached = true;

	if (const UCharacterMoverComponent* CharMoverComp = Cast<UCharacterMoverComponent>(MoverComponent))
	{
		const USceneComponent* UpdatedComponent = UpdatedComp;
		const UPrimitiveComponent* UpdatedCompAsPrimitive = Cast<UPrimitiveComponent>(UpdatedComponent);

		if (const UCapsuleComponent* OriginalCapsule = UMovementUtils::GetOriginalComponentType<UCapsuleComponent>(CharMoverComp->GetOwner()))
		{
			StandingHalfHeight = OriginalCapsule->GetScaledCapsuleHalfHeight();
		}

		if (const UCapsuleComponent* CapsuleComponent = Cast<UCapsuleComponent>(UpdatedCompAsPrimitive))
		{
			CurrentHalfHeight = CapsuleComponent->GetScaledCapsuleHalfHeight();
		}

		const float HalfHeightDifference = StandingHalfHeight - CurrentHalfHeight;

		const FMoverDefaultSyncState* DefaultSyncState = SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();

		const FVector PawnLocation = DefaultSyncState->GetLocation_WorldSpace();
		const FQuat PawnRot = DefaultSyncState->GetOrientation_WorldSpace().Quaternion();
		float PawnRadius = 0.0f;
		float PawnHalfHeight = 0.0f;
		UpdatedCompAsPrimitive->CalcBoundingCylinder(PawnRadius, PawnHalfHeight);

		// TODO: Compensate for the difference between current capsule size and standing size
		const FCollisionShape StandingCapsuleShape = FCollisionShape::MakeCapsule(PawnRadius, StandingHalfHeight);
		const ECollisionChannel CollisionChannel = UpdatedCompAsPrimitive->GetCollisionObjectType();

		// TODO: @Harsha Switch to physics thread safe IsOnGround_Internal() method when available.
		bool bShouldMaintainBase = false;
		if (const TObjectPtr<UBaseMovementMode>* CurrentMode = CharMoverComp->MovementModes.Find(SyncState.MovementMode))
		{
			const UBaseMovementMode* ActiveMode = CurrentMode->Get();
			if (ActiveMode && ActiveMode->HasGameplayTag(Mover_IsOnGround, true))
			{
				bShouldMaintainBase = true;
			}
		}

		if (!bShouldMaintainBase)
		{
			// Expand in place
			bEncroached = UMovementUtils::OverlapTest(UpdatedComponent, UpdatedCompAsPrimitive, PawnLocation, PawnRot, CollisionChannel, StandingCapsuleShape, CharMoverComp->GetOwner());
		}
		else
		{
			// Expand while keeping base location the same.
			const FVector StandingLocation = PawnLocation + (HalfHeightDifference + .01f) * CharMoverComp->GetUpDirection();
			bEncroached = UMovementUtils::OverlapTest(UpdatedComponent, UpdatedCompAsPrimitive, StandingLocation, PawnRot, CollisionChannel, StandingCapsuleShape, CharMoverComp->GetOwner());
		}
	}
	return !bEncroached;
}

void FPhysicsStanceModifier::OnPostSimulationTick(const FStanceModifier* Modifier, UMoverComponent* MoverComp, UPrimitiveComponent* UpdatedPrimitive,  bool bIsCrouching, bool& bPostProcessed, OUT bool& bStanceChanged)
{
	if(const UStanceSettings* StanceSettings = MoverComp->FindSharedSettings<UStanceSettings>())
	{
		if (UCapsuleComponent* CapsuleComponent = Cast<UCapsuleComponent>(UpdatedPrimitive))
		{
			const UCapsuleComponent* OriginalCapsule = UMovementUtils::GetOriginalComponentType<UCapsuleComponent>(MoverComp->GetOwner());

			if (OriginalCapsule)
			{
				if (Modifier && bIsCrouching && !bPostProcessed) // Crouching
				{
					const float OldHalfHeight = OriginalCapsule->GetScaledCapsuleHalfHeight();
					const float NewHalfHeight = StanceSettings->CrouchHalfHeight;
					const float HalfHeightDifference = NewHalfHeight - OldHalfHeight;
					const float ColliderOffset = -MoverComp->GetPrimaryVisualComponent()->GetRelativeLocation().Z - OldHalfHeight;
	
					CapsuleComponent->SetCapsuleSize(CapsuleComponent->GetUnscaledCapsuleRadius(), NewHalfHeight);
					MoverComp->GetPrimaryVisualComponent()->SetRelativeLocation(-(NewHalfHeight + ColliderOffset) * MoverComp->GetUpDirection());
	
					bStanceChanged = true;
					bPostProcessed = true;
				}
				else if (!Modifier && !bIsCrouching && bPostProcessed) // Uncrouching
				{
					const float OldHalfHeight = StanceSettings->CrouchHalfHeight;
					const float NewHalfHeight = OriginalCapsule->GetScaledCapsuleHalfHeight();
					const float HalfHeightDifference = NewHalfHeight - OldHalfHeight;
					const float ColliderOffset = -MoverComp->GetPrimaryVisualComponent()->GetRelativeLocation().Z - OldHalfHeight;
	
					CapsuleComponent->SetCapsuleSize(CapsuleComponent->GetUnscaledCapsuleRadius(), NewHalfHeight);
					MoverComp->GetPrimaryVisualComponent()->SetRelativeLocation(-(NewHalfHeight + ColliderOffset) * MoverComp->GetUpDirection());
	
					bStanceChanged = true;
					bPostProcessed = false;
				}
			}
		}
	}
}

FMovementModifierBase* FPhysicsStanceModifier::Clone() const
{
	FPhysicsStanceModifier* CopyPtr = new FPhysicsStanceModifier(*this);
	return CopyPtr;
}

void FPhysicsStanceModifier::NetSerialize(FArchive& Ar)
{
	Super::NetSerialize(Ar);
}

UScriptStruct* FPhysicsStanceModifier::GetScriptStruct() const
{
	return FPhysicsStanceModifier::StaticStruct();
}

FString FPhysicsStanceModifier::ToSimpleString() const
{
	return FString::Printf(TEXT("Physics Based Stance Modifier"));
}

void FPhysicsStanceModifier::AddReferencedObjects(FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(Collector);
}
