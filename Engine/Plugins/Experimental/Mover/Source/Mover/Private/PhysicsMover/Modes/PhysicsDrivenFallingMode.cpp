// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsMover/Modes/PhysicsDrivenFallingMode.h"

#include "Chaos/Character/CharacterGroundConstraint.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"
#include "GameFramework/PhysicsVolume.h"
#include "Math/UnitConversion.h"
#include "MoveLibrary/FloorQueryUtils.h"
#include "MoveLibrary/WaterMovementUtils.h"
#include "MoverComponent.h"
#include "PhysicsMover/PhysicsMovementUtils.h"
#if WITH_EDITOR
#include "Backends/MoverNetworkPhysicsLiaison.h"
#include "Internationalization/Text.h"
#include "Misc/DataValidation.h"
#endif // WITH_EDITOR

#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "MoveLibrary/MovementUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PhysicsDrivenFallingMode)

#define LOCTEXT_NAMESPACE "PhysicsDrivenFallingMode"

UPhysicsDrivenFallingMode::UPhysicsDrivenFallingMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UPhysicsDrivenFallingMode::UpdateConstraintSettings(Chaos::FCharacterGroundConstraint& Constraint) const
{
	Constraint.SetRadialForceLimit(300000.0); // TEMP - Move radial force limit to shared mode data
	Constraint.SetTwistTorqueLimit(FUnitConversion::Convert(TwistTorqueLimit, EUnit::NewtonMeters, EUnit::KilogramCentimetersSquaredPerSecondSquared));
	Constraint.SetSwingTorqueLimit(FUnitConversion::Convert(SwingTorqueLimit, EUnit::NewtonMeters, EUnit::KilogramCentimetersSquaredPerSecondSquared));
	Constraint.SetTargetHeight(TargetHeight);
}

#if WITH_EDITOR
EDataValidationResult UPhysicsDrivenFallingMode::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	PhysicsMovementModeUtils::ValidateBackendClass(GetMoverComponent(), Context, Result);
	return Result;
}
#endif // WITH_EDITOR

void UPhysicsDrivenFallingMode::SetTargetHeightOverride(float InTargetHeight)
{
	TargetHeightOverride = InTargetHeight;
	TargetHeight = InTargetHeight;
}

void UPhysicsDrivenFallingMode::ClearTargetHeightOverride()
{
	TargetHeightOverride.Reset();
	 
	if (const ACharacter* CharacterOwner = GetTypedOuter<ACharacter>())
	{
		TargetHeight = -CharacterOwner->GetMesh()->GetRelativeLocation().Z;
	}
	else
	{
		TargetHeight = GetDefault<UPhysicsDrivenFallingMode>(GetClass())->TargetHeight;
	}
}

void UPhysicsDrivenFallingMode::OnRegistered(const FName ModeName)
{
	Super::OnRegistered(ModeName);

	if (TargetHeightOverride.IsSet())
	{
		TargetHeight = TargetHeightOverride.GetValue();
	}
	else if (const ACharacter* CharacterOwner = GetTypedOuter<ACharacter>())
	{
		TargetHeight = -CharacterOwner->GetMesh()->GetRelativeLocation().Z;
	}
}

void UPhysicsDrivenFallingMode::SimulationTick_Implementation(const FSimulationTickParams& Params, FMoverTickEndData& OutputState)
{
	const FMoverTickStartData& StartState = Params.StartState;
	USceneComponent* UpdatedComponent = Params.MovingComps.UpdatedComponent.Get();
	UPrimitiveComponent* UpdatedPrimitive = Params.MovingComps.UpdatedPrimitive.Get();
	FProposedMove ProposedMove = Params.ProposedMove;
	const UMoverComponent* MoverComp = GetMoverComponent();

	const FCharacterDefaultInputs* CharacterInputs = StartState.InputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>();
	const FMoverDefaultSyncState* StartingSyncState = StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	check(StartingSyncState);

	FMoverDefaultSyncState& OutputSyncState = OutputState.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();

	const float DeltaSeconds = Params.TimeStep.StepMs * 0.001f;
	const FVector UpDir = MoverComp->GetUpDirection();

	// Floor query

	UMoverBlackboard* SimBlackboard = MoverComp->GetSimBlackboard_Mutable();
	if (!SimBlackboard)
	{
		OutputSyncState = *StartingSyncState;
		return;
	}

	SimBlackboard->Invalidate(CommonBlackboard::LastFloorResult);
	SimBlackboard->Invalidate(CommonBlackboard::LastWaterResult);

	// Find floor

	FFloorCheckResult FloorResult;
	FWaterCheckResult WaterResult;

	const float QueryDistance = 1.1f * FMath::Max(TargetHeight, TargetHeight - UpDir.Dot(ProposedMove.LinearVelocity) * DeltaSeconds);

	UPhysicsMovementUtils::FloorSweep_Internal(StartingSyncState->GetLocation_WorldSpace(), StartingSyncState->GetVelocity_WorldSpace() * DeltaSeconds,
		UpdatedPrimitive, UpDir, QueryRadius, QueryDistance, CommonLegacySettings->MaxWalkSlopeCosine, TargetHeight, FloorResult, WaterResult);

	SimBlackboard->Set(CommonBlackboard::LastFloorResult, FloorResult);
	SimBlackboard->Set(CommonBlackboard::LastWaterResult, WaterResult);

	const bool bJumping = CharacterInputs && CharacterInputs->bIsJumpPressed;
	const bool bIsMovingUp = bJumping || (UpDir.Dot(ProposedMove.LinearVelocity) > 0.0f);
	const float ProjectedImmersionDepth = WaterResult.WaterSplineData.ImmersionDepth - (UpDir.Dot(ProposedMove.LinearVelocity) * DeltaSeconds);
	const bool bStartSwimming = ProjectedImmersionDepth > CommonLegacySettings->SwimmingStartImmersionDepth;

	if (WaterResult.IsSwimmableVolume() && bStartSwimming && !bIsMovingUp)
	{
		OutputState.MovementEndState.NextModeName = CommonLegacySettings->SwimmingMovementModeName;
		OutputState.MovementEndState.RemainingMs = Params.TimeStep.StepMs;
		return;
	}

	// In air steering

	FRotator TargetOrient = StartingSyncState->GetOrientation_WorldSpace();
	if (!UMovementUtils::IsAngularVelocityZero(ProposedMove.AngularVelocity)) 
	{
		TargetOrient += (ProposedMove.AngularVelocity * DeltaSeconds);
	}

	FVector TargetVel = ProposedMove.LinearVelocity;
	if (const APhysicsVolume* CurPhysVolume = UpdatedComponent->GetPhysicsVolume())
	{
		// The physics simulation applies Z-only gravity acceleration via physics volumes, so we need to account for it here 
		TargetVel -= (CurPhysVolume->GetGravityZ() * FVector::UpVector * DeltaSeconds);
	}

	constexpr float FloorDistanceTolerance = 2.0f;
	const float FloorDistanceWithFloorNormal = FloorResult.HitResult.ImpactNormal.Dot(StartingSyncState->GetLocation_WorldSpace() - FloorResult.HitResult.ImpactPoint);
	const FVector ProjectedGroundVelocity = UPhysicsMovementUtils::ComputeIntegratedGroundVelocityFromHitResult(StartingSyncState->GetLocation_WorldSpace(), FloorResult.HitResult, DeltaSeconds);
	const float ProjectedRelativeVerticalVelocity = FloorResult.HitResult.ImpactNormal.Dot(ProposedMove.LinearVelocity - ProjectedGroundVelocity);
	const float ProjectedFloorDistance = FloorDistanceWithFloorNormal + ProjectedRelativeVerticalVelocity * DeltaSeconds;
	const bool bIsFloorWithinReach = ProjectedFloorDistance < TargetHeight + FloorDistanceTolerance;
	const bool bIsMovingUpRelativeToFloor = ProjectedRelativeVerticalVelocity > UE_KINDA_SMALL_NUMBER;

	FVector TargetPos = StartingSyncState->GetLocation_WorldSpace();
	if (FloorResult.IsWalkableFloor() && bIsFloorWithinReach && !bIsMovingUpRelativeToFloor)
	{
		OutputState.MovementEndState.NextModeName = CommonLegacySettings->GroundMovementModeName;
		TargetPos += UpDir * (TargetHeight - FloorResult.FloorDist) + (TargetVel - TargetVel.Dot(UpDir) * UpDir) * DeltaSeconds;
	}
	else
	{
		TargetPos += TargetVel * DeltaSeconds;
	}

	OutputState.MovementEndState.RemainingMs = 0.0f;
	OutputSyncState.MoveDirectionIntent = ProposedMove.bHasDirIntent ? ProposedMove.DirectionIntent : FVector::ZeroVector;
	OutputSyncState.SetTransforms_WorldSpace(
		TargetPos,
		TargetOrient,
		TargetVel);
}

#undef LOCTEXT_NAMESPACE