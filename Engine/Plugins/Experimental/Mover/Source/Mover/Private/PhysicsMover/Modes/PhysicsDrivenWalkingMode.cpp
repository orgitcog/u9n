// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsMover/Modes/PhysicsDrivenWalkingMode.h"

#include "Chaos/Character/CharacterGroundConstraint.h"
#include "Chaos/Character/CharacterGroundConstraintContainer.h"
#include "Chaos/ContactModification.h"
#include "Chaos/PhysicsObject.h"
#include "Chaos/PhysicsObjectInternalInterface.h"
#include "DefaultMovementSet/LayeredMoves/BasicLayeredMoves.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"
#include "Engine/World.h"
#include "GameFramework/PhysicsVolume.h"
#include "Math/UnitConversion.h"
#include "MoverComponent.h"
#include "MoveLibrary/MovementUtils.h"
#include "MoveLibrary/GroundMovementUtils.h"
#include "MoveLibrary/WaterMovementUtils.h"
#include "PhysicsMover/PhysicsMovementUtils.h"
#include "PhysicsMover/PhysicsMoverSimulationTypes.h"
#if WITH_EDITOR
#include "Backends/MoverNetworkPhysicsLiaison.h"
#include "Internationalization/Text.h"
#include "Misc/DataValidation.h"
#endif // WITH_EDITOR

#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PhysicsDrivenWalkingMode)

extern FPhysicsDrivenMotionDebugParams GPhysicsDrivenMotionDebugParams;

#define LOCTEXT_NAMESPACE "PhysicsDrivenWalkingMode"

UPhysicsDrivenWalkingMode::UPhysicsDrivenWalkingMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UPhysicsDrivenWalkingMode::UpdateConstraintSettings(Chaos::FCharacterGroundConstraint& Constraint) const
{
	Constraint.SetRadialForceLimit(FUnitConversion::Convert(RadialForceLimit, EUnit::Newtons, EUnit::KilogramCentimetersPerSecondSquared));
	Constraint.SetFrictionForceLimit(FUnitConversion::Convert(FrictionForceLimit, EUnit::Newtons, EUnit::KilogramCentimetersPerSecondSquared));
	Constraint.SetTwistTorqueLimit(FUnitConversion::Convert(TwistTorqueLimit, EUnit::NewtonMeters, EUnit::KilogramCentimetersSquaredPerSecondSquared));
	Constraint.SetSwingTorqueLimit(FUnitConversion::Convert(SwingTorqueLimit, EUnit::NewtonMeters, EUnit::KilogramCentimetersSquaredPerSecondSquared));
	Constraint.SetTargetHeight(TargetHeight);
	Constraint.SetDampingFactor(GroundDamping);
	Constraint.SetMotionTargetMassBias(FractionalGroundReaction);
	Constraint.SetRadialForceMotionTargetScaling(FractionalRadialForceLimitScaling);
}

void UPhysicsDrivenWalkingMode::OnContactModification_Internal(const FPhysicsMoverSimulationContactModifierParams& Params, Chaos::FCollisionContactModifier& Modifier) const
{
	Chaos::EnsureIsInPhysicsThreadContext();

	if (!Params.ConstraintHandle || !Params.UpdatedPrimitive)
	{
		return;
	}

	Chaos::FPBDRigidParticleHandle* CharacterParticle = Params.ConstraintHandle->GetCharacterParticle()->CastToRigidParticle();
	if (!CharacterParticle || CharacterParticle->Disabled())
	{
		return;
	}

	const Chaos::FGeometryParticleHandle* GroundParticle = Params.ConstraintHandle->GetGroundParticle();
	if (!GroundParticle)
	{
		return;
	}

	float PawnHalfHeight;
	float PawnRadius;
	Params.UpdatedPrimitive->CalcBoundingCylinder(PawnRadius, PawnHalfHeight);

	const float CharacterHeight = CharacterParticle->GetX().Z;
	const float EndCapHeight = CharacterHeight - PawnHalfHeight + PawnRadius;

	const float CosThetaMax = 0.707f;

	float MinContactHeightStepUps = CharacterHeight - 1.0e10f;
	const float StepDistance = FMath::Abs(TargetHeight - Params.ConstraintHandle->GetData().GroundDistance);
	if (StepDistance >= GPhysicsDrivenMotionDebugParams.MinStepUpDistance)
	{
		MinContactHeightStepUps = CharacterHeight - TargetHeight + CommonLegacySettings->MaxStepHeight;
	}

	for (Chaos::FContactPairModifier& PairModifier : Modifier.GetContacts(CharacterParticle))
	{
		const int32 CharacterIdx = CharacterParticle == PairModifier.GetParticlePair()[0] ? 0 : 1;
		const int32 OtherIdx = CharacterIdx == 0 ? 1 : 0;

		for (int32 Idx = 0; Idx < PairModifier.GetNumContacts(); ++Idx)
		{
			Chaos::FVec3 Point0, Point1;
			PairModifier.GetWorldContactLocations(Idx, Point0, Point1);
			Chaos::FVec3 CharacterPoint = CharacterIdx == 0 ? Point0 : Point1;

			Chaos::FVec3 ContactNormal = PairModifier.GetWorldNormal(Idx);
			if ((ContactNormal.Z > CosThetaMax) && CharacterPoint.Z < EndCapHeight)
			{
				// Disable any nearly vertical contact with the end cap of the capsule
				// This will be handled by the character ground constraint
				PairModifier.SetContactPointDisabled(Idx);
			}
			else if ((CharacterPoint.Z < MinContactHeightStepUps) && (GroundParticle == PairModifier.GetParticlePair()[OtherIdx]))
			{
				// In the case of steps ups disable all contacts below the max step height
				PairModifier.SetContactPointDisabled(Idx);
			}
		}
	}
}

bool UPhysicsDrivenWalkingMode::CanStepUpOnHitSurface(const FFloorCheckResult& FloorResult) const
{
	const float StepHeight = TargetHeight - FloorResult.FloorDist;

	bool bWalkable = StepHeight <= CommonLegacySettings->MaxStepHeight;
	constexpr float MinStepHeight = 2.0f;
	const bool SteppingUp = StepHeight > MinStepHeight;
	if (bWalkable && SteppingUp)
	{
		bWalkable = UGroundMovementUtils::CanStepUpOnHitSurface(FloorResult.HitResult);
	}

	return bWalkable;
}

void UPhysicsDrivenWalkingMode::FloorCheck(const FMoverDefaultSyncState& SyncState, const FProposedMove& ProposedMove, UPrimitiveComponent* UpdatedPrimitive, float DeltaSeconds,
	FFloorCheckResult& OutFloorResult, FWaterCheckResult& OutWaterResult, FVector& OutDeltaPos) const
{
	const FVector UpDir = GetMoverComponent()->GetUpDirection();

	FVector DeltaPos = ProposedMove.LinearVelocity * DeltaSeconds;
	OutDeltaPos = DeltaPos;

	float PawnHalfHeight;
	float PawnRadius;
	UpdatedPrimitive->CalcBoundingCylinder(PawnRadius, PawnHalfHeight);

	const float FloorSweepDistance = TargetHeight + CommonLegacySettings->MaxStepHeight;

	UPhysicsMovementUtils::FloorSweep_Internal(SyncState.GetLocation_WorldSpace(), DeltaPos, UpdatedPrimitive, UpDir,
		QueryRadius, FloorSweepDistance, CommonLegacySettings->MaxWalkSlopeCosine, TargetHeight, OutFloorResult, OutWaterResult);

	if (!OutFloorResult.bBlockingHit)
	{
		// Floor not found
		return;
	}

	bool bWalkableFloor = OutFloorResult.bWalkableFloor && CanStepUpOnHitSurface(OutFloorResult);
	if (bWalkableFloor)
	{
		// Walkable floor found
		return;
	}

	// Hit something but not walkable. Try a new query to find a walkable surface
	const float StepBlockedHeight = TargetHeight - PawnHalfHeight + PawnRadius;
	const float StepHeight = TargetHeight - OutFloorResult.FloorDist;

	if (StepHeight > StepBlockedHeight)
	{
		// Collision should prevent movement. Just try to find ground at start of movement
		const float ShrinkMultiplier = 0.75f;
		UPhysicsMovementUtils::FloorSweep_Internal(SyncState.GetLocation_WorldSpace(), FVector::ZeroVector, UpdatedPrimitive, UpDir,
			ShrinkMultiplier * QueryRadius, FloorSweepDistance, CommonLegacySettings->MaxWalkSlopeCosine, TargetHeight, OutFloorResult, OutWaterResult);

		OutFloorResult.bWalkableFloor = OutFloorResult.bWalkableFloor && CanStepUpOnHitSurface(OutFloorResult);
		return;
	}

	if (DeltaPos.SizeSquared() < UE_SMALL_NUMBER)
	{
		// Stationary
		OutDeltaPos = FVector::ZeroVector;
		return;
	}

	// Try to limit the movement to remain on a walkable surface
	FVector NewDeltaPos = DeltaPos;
	float NewQueryRadius = QueryRadius;

	FVector HorizSurfaceDir = FVector::VectorPlaneProject(OutFloorResult.HitResult.ImpactNormal, UpDir);
	float HorizSurfaceDirSizeSq = HorizSurfaceDir.SizeSquared();
	bool bFoundOutwardDir = false;
	if (HorizSurfaceDirSizeSq > UE_SMALL_NUMBER)
	{
		HorizSurfaceDir *= FMath::InvSqrt(HorizSurfaceDirSizeSq);
		bFoundOutwardDir = true;
	}
	else
	{
		// Flat unwalkable surface. Try and get the horizontal direction from the normal instead
		HorizSurfaceDir = FVector::VectorPlaneProject(OutFloorResult.HitResult.Normal, UpDir);
		HorizSurfaceDirSizeSq = HorizSurfaceDir.SizeSquared();

		if (HorizSurfaceDirSizeSq > UE_SMALL_NUMBER)
		{
			HorizSurfaceDir *= FMath::InvSqrt(HorizSurfaceDirSizeSq);
			bFoundOutwardDir = true;
		}
	}

	if (bFoundOutwardDir)
	{
		// If we're moving away try a ray query at the end of the motion
		const float DP = DeltaPos.Dot(HorizSurfaceDir);
		if (DP > 0.0f)
		{
			NewQueryRadius = 0.0f;
		}
		else
		{
			NewQueryRadius = 0.25f * QueryRadius;
			NewDeltaPos = DeltaPos - DP * HorizSurfaceDir;
		}

		UPhysicsMovementUtils::FloorSweep_Internal(SyncState.GetLocation_WorldSpace(), NewDeltaPos, UpdatedPrimitive, UpDir,
			NewQueryRadius, FloorSweepDistance, CommonLegacySettings->MaxWalkSlopeCosine, TargetHeight, OutFloorResult, OutWaterResult);

		OutFloorResult.bWalkableFloor = OutFloorResult.bWalkableFloor && CanStepUpOnHitSurface(OutFloorResult);
		if (OutFloorResult.bWalkableFloor)
		{
			OutDeltaPos = NewDeltaPos;
		}
		else
		{
			OutFloorResult.bWalkableFloor = false;
		}
	}
	else
	{
		// Try a query at the start of the movement to find a walkable surface and prevent movement

		NewDeltaPos = FVector::ZeroVector;
		NewQueryRadius = 0.25f * QueryRadius;

		UPhysicsMovementUtils::FloorSweep_Internal(SyncState.GetLocation_WorldSpace(), NewDeltaPos, UpdatedPrimitive, UpDir,
			NewQueryRadius, FloorSweepDistance, CommonLegacySettings->MaxWalkSlopeCosine, TargetHeight, OutFloorResult, OutWaterResult);

		OutFloorResult.bWalkableFloor = OutFloorResult.bWalkableFloor && CanStepUpOnHitSurface(OutFloorResult);
		OutDeltaPos = NewDeltaPos;
	}
}

#if WITH_EDITOR
EDataValidationResult UPhysicsDrivenWalkingMode::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	PhysicsMovementModeUtils::ValidateBackendClass(GetMoverComponent(), Context, Result);
	return Result;
}
#endif // WITH_EDITOR

void UPhysicsDrivenWalkingMode::SetTargetHeightOverride(float InTargetHeight)
{
	TargetHeightOverride = InTargetHeight;
	TargetHeight = InTargetHeight;
}

void UPhysicsDrivenWalkingMode::ClearTargetHeightOverride()
{
	TargetHeightOverride.Reset();
	
	if (const ACharacter* CharacterOwner = GetTypedOuter<ACharacter>())
	{
		TargetHeight = -CharacterOwner->GetMesh()->GetRelativeLocation().Z;
	}
	else
	{
		TargetHeight = GetDefault<UPhysicsDrivenWalkingMode>(GetClass())->TargetHeight;
	}
}

void UPhysicsDrivenWalkingMode::OnRegistered(const FName ModeName)
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

void UPhysicsDrivenWalkingMode::SimulationTick_Implementation(const FSimulationTickParams& Params, FMoverTickEndData& OutputState)
{
	const UMoverComponent* MoverComp = GetMoverComponent();
	const FMoverTickStartData& StartState = Params.StartState;
	USceneComponent* UpdatedComponent = Params.MovingComps.UpdatedComponent.Get();
	UPrimitiveComponent* UpdatedPrimitive = Params.MovingComps.UpdatedPrimitive.Get();
	FProposedMove ProposedMove = Params.ProposedMove;

	const FVector UpDir = MoverComp->GetUpDirection();

	const FMoverDefaultSyncState* StartingSyncState = StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	check(StartingSyncState);

	FMoverDefaultSyncState& OutputSyncState = OutputState.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();

	const float DeltaSeconds = Params.TimeStep.StepMs * 0.001f;

	UMoverBlackboard* SimBlackboard = MoverComp->GetSimBlackboard_Mutable();
	if (!SimBlackboard)
	{
		OutputSyncState = *StartingSyncState;
		return;
	}

	// Store the previous ground normal that was used to compute the proposed move
	FFloorCheckResult PrevFloorResult;
	FVector PrevGroundNormal = UpDir;
	if (SimBlackboard->TryGet(CommonBlackboard::LastFloorResult, PrevFloorResult))
	{
		PrevGroundNormal = PrevFloorResult.HitResult.ImpactNormal;
	}

	// Floor query
	FFloorCheckResult FloorResult;
	FWaterCheckResult WaterResult;
	FVector OutDeltaPos;

	FloorCheck(*StartingSyncState, ProposedMove, UpdatedPrimitive, DeltaSeconds, FloorResult, WaterResult, OutDeltaPos);

	ProposedMove.LinearVelocity = OutDeltaPos / DeltaSeconds;

	// The base movement mode does not apply gravity in walking mode so apply here
	// This is so that the gravity in this mode will be consistent with the gravity
	// set on the mover, not the default physics gravity
	FVector TargetVelocity = StartingSyncState->GetVelocity_WorldSpace() + UMovementUtils::ComputeVelocityFromGravity(MoverComp->GetGravityAcceleration(), DeltaSeconds);
	if (const APhysicsVolume* CurPhysVolume = UpdatedComponent->GetPhysicsVolume())
	{
		// The physics simulation applies Z-only gravity acceleration via physics volumes, so we need to account for it here 
		TargetVelocity -= (CurPhysVolume->GetGravityZ() * FVector::UpVector * DeltaSeconds);
	}

	SimBlackboard->Set(CommonBlackboard::LastFloorResult, FloorResult);
	SimBlackboard->Set(CommonBlackboard::LastWaterResult, WaterResult);

	const bool bStartSwimming = WaterResult.WaterSplineData.ImmersionDepth > CommonLegacySettings->SwimmingStartImmersionDepth;

	if (WaterResult.IsSwimmableVolume() && bStartSwimming)
	{
		SwitchToState(CommonLegacySettings->SwimmingMovementModeName, Params, OutputState);
	}
	else if (FloorResult.IsWalkableFloor())
	{
		const FVector StartGroundVelocity = UPhysicsMovementUtils::ComputeGroundVelocityFromHitResult(StartingSyncState->GetLocation_WorldSpace(), FloorResult.HitResult, DeltaSeconds);
		const FVector ProjectedGroundVelocity = UPhysicsMovementUtils::ComputeIntegratedGroundVelocityFromHitResult(StartingSyncState->GetLocation_WorldSpace(), FloorResult.HitResult, DeltaSeconds);
		const bool bIsGroundMoving = ProjectedGroundVelocity.SizeSquared() > UE_KINDA_SMALL_NUMBER;

		TargetVelocity = StartingSyncState->GetVelocity_WorldSpace();
		FVector TargetPosition = StartingSyncState->GetLocation_WorldSpace();
		if (FloorResult.FloorDist > TargetHeight + UE_KINDA_SMALL_NUMBER)
		{
			TargetPosition += UpDir * (TargetHeight - FloorResult.FloorDist);
		}

		constexpr float ParallelCosThreshold = 0.999f;
		const bool bVerticalVelocity = FVector::Parallel(TargetVelocity.GetSafeNormal(), UpDir, ParallelCosThreshold);
		const bool bUseProposedMove = !(bHandleVerticalLandingSeparately && bVerticalVelocity) || ProposedMove.bHasDirIntent;

		if (bUseProposedMove)
		{
			const FVector ProposedMovePlaneVelocity = ProposedMove.LinearVelocity - ProposedMove.LinearVelocity.ProjectOnToNormal(PrevGroundNormal);

			// If there is velocity intent in the normal direction then use the velocity from the proposed move. Otherwise
			// retain the previous vertical velocity
			FVector ProposedNormalVelocity = ProposedMove.LinearVelocity - ProposedMovePlaneVelocity;
			if (ProposedNormalVelocity.SizeSquared() > UE_KINDA_SMALL_NUMBER)
			{
				TargetVelocity += ProposedNormalVelocity - TargetVelocity.ProjectOnToNormal(PrevGroundNormal);
			}

			TargetPosition += ProposedMovePlaneVelocity * DeltaSeconds;
		}

		// Check if the proposed velocity would lift off the movement surface.
		bool bIsLiftingOffSurface = false;

		float CharacterGravity = 0.0f;
		if (const APhysicsVolume* PhysVolume = UpdatedComponent->GetPhysicsVolume())
		{
			CharacterGravity = PhysVolume->GetGravityZ();
		}

		FVector ProjectedVelocity = TargetVelocity + CharacterGravity * FVector::UpVector * DeltaSeconds;

		const float ProjectedRelativeVerticalVelocity = FloorResult.HitResult.ImpactNormal.Dot(ProjectedVelocity - ProjectedGroundVelocity);
		const float VerticalVelocityLimit = 2.0f / DeltaSeconds;

		if ((ProjectedRelativeVerticalVelocity > VerticalVelocityLimit) && bIsGroundMoving && (ProjectedVelocity.Dot(UpDir) > VerticalVelocityLimit))
		{
			bIsLiftingOffSurface = true;
		}

		// Determine if the character is stepping up or stepping down.
		// If stepping up make sure that the step height is less than the max step height
		// and the new surface has CanCharacterStepUpOn set to true.
		// If stepping down make sure the step height is less than the max step height.
		const float StartHeightAboveGround = FloorResult.FloorDist - TargetHeight;
		const float EndHeightAboutGround = StartHeightAboveGround + UpDir.Dot(ProjectedVelocity - ProjectedGroundVelocity) * DeltaSeconds ;
		const bool bIsSteppingDown = StartHeightAboveGround > GPhysicsDrivenMotionDebugParams.MinStepUpDistance;
		const bool bIsWithinReach = EndHeightAboutGround <= CommonLegacySettings->MaxStepHeight;

		// If the character is unsupported allow some grace period before falling
		bool bIsSupported = bIsWithinReach && !bIsLiftingOffSurface;
		float TimeSinceSupported = MaxUnsupportedTimeBeforeFalling;
		SimBlackboard->TryGet(CommonBlackboard::TimeSinceSupported, TimeSinceSupported);
		if (bIsSupported)
		{
			SimBlackboard->Set(CommonBlackboard::TimeSinceSupported, 0.0f);
		}
		else if (!bIsLiftingOffSurface)
		{
			// Falling
			TimeSinceSupported += DeltaSeconds;
			SimBlackboard->Set(CommonBlackboard::TimeSinceSupported, TimeSinceSupported);
			bIsSupported = TimeSinceSupported < MaxUnsupportedTimeBeforeFalling;
		}
		else
		{
			// Moving up relative to ground
			SimBlackboard->Set(CommonBlackboard::TimeSinceSupported, MaxUnsupportedTimeBeforeFalling);
		}

		// Apply vertical velocity to target if stepping down
		const bool bNeedsVerticalVelocityToTarget = bIsSupported && bIsSteppingDown && (EndHeightAboutGround > 0.0f) && !bIsLiftingOffSurface;
		if (bNeedsVerticalVelocityToTarget)
		{
			TargetVelocity -= FractionalDownwardVelocityToTarget * (EndHeightAboutGround / DeltaSeconds) * UpDir;
		}

		// Target orientation
		// This is always applied regardless of whether the character is supported
		FRotator TargetOrientation = StartingSyncState->GetOrientation_WorldSpace();
		if (!UMovementUtils::IsAngularVelocityZero(ProposedMove.AngularVelocity)) 
		{
			TargetOrientation += (ProposedMove.AngularVelocity * DeltaSeconds);
		}

		if (bIsSupported)
		{
			OutputState.MovementEndState.RemainingMs = 0.0f;
			OutputSyncState.MoveDirectionIntent = ProposedMove.bHasDirIntent ? ProposedMove.DirectionIntent : FVector::ZeroVector;
			OutputSyncState.SetTransforms_WorldSpace(
				TargetPosition,
				TargetOrientation,
				TargetVelocity,
				nullptr);
		}
		else
		{
			// Blocking hit but not supported
			SwitchToState(CommonLegacySettings->AirMovementModeName, Params, OutputState);
		}
	}
	else
	{
		// No water or floor not found
		float TimeSinceSupported = MaxUnsupportedTimeBeforeFalling;
		SimBlackboard->TryGet(CommonBlackboard::TimeSinceSupported, TimeSinceSupported);
		TimeSinceSupported += DeltaSeconds;
		SimBlackboard->Set(CommonBlackboard::TimeSinceSupported, TimeSinceSupported);
		if (TimeSinceSupported >= MaxUnsupportedTimeBeforeFalling)
		{
			SwitchToState(CommonLegacySettings->AirMovementModeName, Params, OutputState);
		}
	}
}

void UPhysicsDrivenWalkingMode::SwitchToState(const FName& StateName, const FSimulationTickParams& Params, FMoverTickEndData& OutputState)
{
	OutputState.MovementEndState.RemainingMs = Params.TimeStep.StepMs;
	OutputState.MovementEndState.NextModeName = StateName;

	const FMoverDefaultSyncState* StartingSyncState = Params.StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	FMoverDefaultSyncState& OutputSyncState = OutputState.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();
	OutputSyncState.SetTransforms_WorldSpace(
		StartingSyncState->GetLocation_WorldSpace(),
		StartingSyncState->GetOrientation_WorldSpace(),
		StartingSyncState->GetVelocity_WorldSpace(),
		nullptr);
}

#undef LOCTEXT_NAMESPACE