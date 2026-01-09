// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosMover/Character/Modes/ChaosWalkingMode.h"

#include "Chaos/Character/CharacterGroundConstraintSettings.h"
#include "Chaos/ContactModification.h"
#include "Chaos/PhysicsObjectInternalInterface.h"
#include "ChaosMover/ChaosMoverLog.h"
#include "ChaosMover/ChaosMoverSimulation.h"
#include "ChaosMover/ChaosMoverSimulationTypes.h"
#include "ChaosMover/Character/Settings/SharedChaosCharacterMovementSettings.h"
#include "ChaosMover/Character/Transitions/ChaosCharacterFallingCheck.h"
#include "ChaosMover/Character/Transitions/ChaosCharacterJumpCheck.h"
#include "ChaosMover/Utilities/ChaosGroundMovementUtils.h"
#include "ChaosMover/Utilities/ChaosMoverQueryUtils.h"
#include "Math/UnitConversion.h"
#include "MoveLibrary/MovementUtils.h"
#include "MoveLibrary/BasedMovementUtils.h"
#include "MoveLibrary/FloorQueryUtils.h"
#include "MoveLibrary/GroundMovementUtils.h"
#include "MoveLibrary/MovementUtils.h"
#include "MoveLibrary/WaterMovementUtils.h"
#include "Chaos/ParticleHandle.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ChaosWalkingMode)

UChaosWalkingMode::UChaosWalkingMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSupportsAsync = true;
	GameplayTags.AddTag(Mover_IsOnGround);

	RadialForceLimit = 2000.0f;
	SwingTorqueLimit = 3000.0f;
	TwistTorqueLimit = 1500.0f;

	Transitions.Add(CreateDefaultSubobject<UChaosCharacterFallingCheck>(TEXT("DefaultFallingCheck")));
	Transitions.Add(CreateDefaultSubobject<UChaosCharacterJumpCheck>(TEXT("DefaultJumpCheck")));
}

void UChaosWalkingMode::UpdateConstraintSettings(Chaos::FCharacterGroundConstraintSettings& ConstraintSettings) const
{
	Super::UpdateConstraintSettings(ConstraintSettings);
	ConstraintSettings.FrictionForceLimit = FUnitConversion::Convert(FrictionForceLimit, EUnit::Newtons, EUnit::KilogramCentimetersPerSecondSquared);
	ConstraintSettings.DampingFactor = GroundDamping;
	ConstraintSettings.MotionTargetMassBias = FractionalGroundReaction;
	ConstraintSettings.RadialForceMotionTargetScaling = FractionalRadialForceLimitScaling;
}

void UChaosWalkingMode::GenerateMove_Implementation(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const
{
	if (!Simulation)
	{
		UE_LOG(LogChaosMover, Warning, TEXT("No Simulation set on ChaosFallingMode"));
		return;
	}

	const FChaosMoverSimulationDefaultInputs* DefaultSimInputs = Simulation->GetLocalSimInput().FindDataByType<FChaosMoverSimulationDefaultInputs>();
	const FCharacterDefaultInputs* CharacterInputs = StartState.InputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>();
	if (!DefaultSimInputs || !CharacterInputs)
	{
		UE_LOG(LogChaosMover, Warning, TEXT("ChaosFallingMode requires FChaosMoverSimulationDefaultInputs and FCharacterDefaultInputs"));
		return;
	}

	const FMoverDefaultSyncState* StartingSyncState = StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	check(StartingSyncState);

	const float DeltaSeconds = TimeStep.StepMs * 0.001f;
	const FVector UpDirection = DefaultSimInputs->UpDir;

	// Try to use the floor as the basis for the intended move direction (i.e. try to walk along slopes, rather than into them)
	UMoverBlackboard* SimBlackboard = Simulation->GetBlackboard_Mutable();
	FFloorCheckResult LastFloorResult;
	FVector MovementNormal;
	FVector LocalGroundVelocity = FVector::ZeroVector;
	if (SimBlackboard && SimBlackboard->TryGet(CommonBlackboard::LastFloorResult, LastFloorResult) && LastFloorResult.IsWalkableFloor())
	{
		MovementNormal = LastFloorResult.HitResult.ImpactNormal;
		LocalGroundVelocity = UChaosGroundMovementUtils::ComputeLocalGroundVelocity_Internal(StartingSyncState->GetLocation_WorldSpace(), LastFloorResult);
	}
	else
	{
		MovementNormal = UpDirection;
	}

	FRotator IntendedOrientation_WorldSpace;
	// If there's no intent from input to change orientation, use the current orientation
	if (!CharacterInputs || CharacterInputs->OrientationIntent.IsNearlyZero())
	{
		IntendedOrientation_WorldSpace = StartingSyncState->GetOrientation_WorldSpace();
	}
	else
	{
		IntendedOrientation_WorldSpace = CharacterInputs->GetOrientationIntentDir_WorldSpace().ToOrientationRotator();
	}

	const FQuat WorldToGravityTransform = FQuat::FindBetweenNormals(FVector::UpVector, DefaultSimInputs->UpDir);
	IntendedOrientation_WorldSpace = UMovementUtils::ApplyGravityToOrientationIntent(IntendedOrientation_WorldSpace, WorldToGravityTransform, bShouldCharacterRemainUpright);

	FGroundMoveParams Params;

	if (CharacterInputs)
	{
		Params.MoveInputType = CharacterInputs->GetMoveInputType();

		const bool bMaintainInputMagnitude = true;
		Params.MoveInput = CharacterInputs->GetMoveInput_WorldSpace();
		//Params.MoveInput = UPlanarConstraintUtils::ConstrainDirectionToPlane(MoverComp->GetPlanarConstraint(), CharacterInputs->GetMoveInput_WorldSpace(), bMaintainInputMagnitude);
	}
	else
	{
		Params.MoveInputType = EMoveInputType::None;
		Params.MoveInput = FVector::ZeroVector;
	}

	const FVector CurrentRelativeVelocity = StartingSyncState->GetVelocity_WorldSpace() - LocalGroundVelocity;

	Params.OrientationIntent = IntendedOrientation_WorldSpace;
	Params.PriorVelocity = FVector::VectorPlaneProject(CurrentRelativeVelocity, MovementNormal);
	Params.PriorOrientation = StartingSyncState->GetOrientation_WorldSpace();
	Params.GroundNormal = MovementNormal;
	Params.TurningRate = SharedSettings->TurningRate;
	Params.TurningBoost = SharedSettings->TurningBoost;
	Params.MaxSpeed = SharedSettings->MaxSpeed;
	Params.Acceleration = SharedSettings->Acceleration;
	Params.Deceleration = SharedSettings->Deceleration;
	Params.DeltaSeconds = DeltaSeconds;
	Params.WorldToGravityQuat = WorldToGravityTransform;
	Params.UpDirection = UpDirection;
	Params.bUseAccelerationForVelocityMove = SharedSettings->bUseAccelerationForVelocityMove;

	if (Params.MoveInput.SizeSquared() > 0.f && !UMovementUtils::IsExceedingMaxSpeed(Params.PriorVelocity, SharedSettings->MaxSpeed))
	{
		Params.Friction = SharedSettings->GroundFriction;
	}
	else
	{
		Params.Friction = SharedSettings->bUseSeparateBrakingFriction ? SharedSettings->BrakingFriction : SharedSettings->GroundFriction;
		Params.Friction *= SharedSettings->BrakingFrictionFactor;
	}

	OutProposedMove = UGroundMovementUtils::ComputeControlledGroundMove(Params);

	// Add back in ground velocity to make the proposed move velocity in world space
	OutProposedMove.LinearVelocity += LocalGroundVelocity;

	if (SimBlackboard)
	{
		// Update the floor result and check the proposed move to prevent movement onto unwalkable surfaces
		FVector OutDeltaPos = FVector::ZeroVector;
		FFloorCheckResult FloorResult;
		FWaterCheckResult WaterResult;
		GetFloorAndCheckMovement(*StartingSyncState, OutProposedMove, *DefaultSimInputs, DeltaSeconds, FloorResult, WaterResult, OutDeltaPos);

		OutProposedMove.LinearVelocity = OutDeltaPos / DeltaSeconds;

		SimBlackboard->Set(CommonBlackboard::LastFloorResult, FloorResult);
		SimBlackboard->Set(CommonBlackboard::LastWaterResult, WaterResult);
	};
}

void UChaosWalkingMode::SimulationTick_Implementation(const FSimulationTickParams& Params, FMoverTickEndData& OutputState)
{
	if (!Simulation)
	{
		UE_LOG(LogChaosMover, Warning, TEXT("No Simulation set on ChaosFallingMode"));
		return;
	}

	const FChaosMoverSimulationDefaultInputs* DefaultSimInputs = Simulation->GetLocalSimInput().FindDataByType<FChaosMoverSimulationDefaultInputs>();
	const FCharacterDefaultInputs* CharacterInputs = Params.StartState.InputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>();
	if (!DefaultSimInputs || !CharacterInputs)
	{
		UE_LOG(LogChaosMover, Warning, TEXT("ChaosFallingMode requires FChaosMoverSimulationDefaultInputs and FCharacterDefaultInputs"));
		return;
	}

	FProposedMove ProposedMove = Params.ProposedMove;
	const FMoverDefaultSyncState* StartingSyncState = Params.StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	check(StartingSyncState);

	FMoverDefaultSyncState& OutputSyncState = OutputState.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();
	OutputSyncState = *StartingSyncState;

	const float DeltaSeconds = Params.TimeStep.StepMs * 0.001f;
	const FVector UpDirection = DefaultSimInputs->UpDir;

	FVector StartingGroundNormal = UpDirection;
	const UMoverBlackboard* SimBlackboard = Simulation->GetBlackboard();
	FFloorCheckResult FloorResult;
	if (SimBlackboard)
	{
		SimBlackboard->TryGet(CommonBlackboard::LastFloorResult, FloorResult);
		StartingGroundNormal = FloorResult.HitResult.ImpactNormal;
	};

	if (FloorResult.IsWalkableFloor())
	{
		const float InitialHeightAboveFloor = FloorResult.FloorDist - GetTargetHeight();

		// Put the target position on the floor at the target height
		FVector TargetPosition = StartingSyncState->GetLocation_WorldSpace();
		if (FloorResult.FloorDist > GetTargetHeight() + UE_KINDA_SMALL_NUMBER)
		{
			TargetPosition -= UpDirection * InitialHeightAboveFloor;
		}

		// The base movement mode does not apply gravity in walking mode so apply here.
		// Also remove the gravity that will be applied by the physics simulation.
		// This is so that the gravity in this mode will be consistent with the gravity
		// set on the mover, not the default physics gravity
		const FVector ProjectedVelocity = StartingSyncState->GetVelocity_WorldSpace() + DefaultSimInputs->Gravity * DeltaSeconds;
		FVector TargetVelocity = ProjectedVelocity - DefaultSimInputs->PhysicsObjectGravity * FVector::UpVector * DeltaSeconds;

		// If we have movement intent and not moving straight up/down then use the proposed move plane velocity
		// otherwise just fall with gravity
		constexpr float ParallelCosThreshold = 0.999f;
		const bool bNonVerticalVelocity = !FVector::Parallel(TargetVelocity.GetSafeNormal(), UpDirection, ParallelCosThreshold);
		const bool bUseProposedMove = bNonVerticalVelocity || ProposedMove.bHasDirIntent;
		if (bUseProposedMove)
		{
			const FVector ProposedMovePlaneVelocity = ProposedMove.LinearVelocity - ProposedMove.LinearVelocity.ProjectOnToNormal(StartingGroundNormal);

			// If there is velocity intent in the normal direction then use the velocity from the proposed move. Otherwise
			// retain the previous vertical velocity
			FVector ProposedNormalVelocity = ProposedMove.LinearVelocity - ProposedMovePlaneVelocity;
			if (ProposedNormalVelocity.SizeSquared() > UE_KINDA_SMALL_NUMBER)
			{
				TargetVelocity += ProposedNormalVelocity - TargetVelocity.ProjectOnToNormal(StartingGroundNormal);
			}

			TargetPosition += ProposedMovePlaneVelocity * DeltaSeconds;
		}

		FVector ProjectedGroundVelocity = UChaosGroundMovementUtils::ComputeLocalGroundVelocity_Internal(StartingSyncState->GetLocation_WorldSpace(), FloorResult);
		const Chaos::FPBDRigidParticleHandle* GroundParticle = UChaosGroundMovementUtils::GetRigidParticleHandleFromFloorResult_Internal(FloorResult);
		if (GroundParticle && GroundParticle->IsDynamic() && GroundParticle->GravityEnabled())
		{
			// This might not be correct if different physics objects have different gravity but is saves having to go
			// to the component to get the gravity on the physics volume.
			ProjectedGroundVelocity += DefaultSimInputs->PhysicsObjectGravity * DefaultSimInputs->UpDir * DeltaSeconds;
		}
		const bool bIsGroundMoving = ProjectedGroundVelocity.SizeSquared() > UE_KINDA_SMALL_NUMBER;
		const FVector ProjectedRelativeVelocity = ProjectedVelocity - ProjectedGroundVelocity;
		const float ProjectedRelativeNormalVelocity = FloorResult.HitResult.ImpactNormal.Dot(ProjectedVelocity - ProjectedGroundVelocity);
		const float ProjectedRelativeVerticalVelocity = UpDirection.Dot(ProjectedVelocity - ProjectedGroundVelocity);
		const float VerticalVelocityLimit = 2.0f / DeltaSeconds;

		bool bIsLiftingOffSurface = false;
		if ((ProjectedRelativeNormalVelocity > VerticalVelocityLimit) && bIsGroundMoving && (ProjectedRelativeVerticalVelocity > VerticalVelocityLimit))
		{
			bIsLiftingOffSurface = true;
		}

		// Determine if the character is stepping up or stepping down.
		// If stepping up make sure that the step height is less than the max step height
		// and the new surface has CanCharacterStepUpOn set to true.
		// If stepping down make sure the step height is less than the max step height.
		const float EndHeightAboveFloor = InitialHeightAboveFloor + ProjectedRelativeVerticalVelocity * DeltaSeconds;
		const bool bIsSteppingDown = InitialHeightAboveFloor > UE_KINDA_SMALL_NUMBER;
		const bool bIsWithinReach = EndHeightAboveFloor <= SharedSettings->MaxStepHeight;
		const bool bIsSupported = bIsWithinReach && !bIsLiftingOffSurface;
		const bool bNeedsVerticalVelocityToTarget = bIsSupported && bIsSteppingDown && (EndHeightAboveFloor > 0.0f) && !bIsLiftingOffSurface;
		if (bNeedsVerticalVelocityToTarget)
		{
			TargetVelocity -= FractionalDownwardVelocityToTarget * (EndHeightAboveFloor / DeltaSeconds) * UpDirection;
		}

		// Target orientation
		// This is always applied regardless of whether the character is supported
		FRotator TargetOrientation = StartingSyncState->GetOrientation_WorldSpace();
		if (!UMovementUtils::IsAngularVelocityZero(ProposedMove.AngularVelocity))
		{
			TargetOrientation += (ProposedMove.AngularVelocity * DeltaSeconds);
		}

		OutputSyncState.SetTransforms_WorldSpace(TargetPosition, TargetOrientation, TargetVelocity);
	}
	
	OutputState.MovementEndState.RemainingMs = 0.0f;
	OutputSyncState.MoveDirectionIntent = ProposedMove.bHasDirIntent ? ProposedMove.DirectionIntent : FVector::ZeroVector;
}

bool UChaosWalkingMode::CanStepUpOnHitSurface(const FFloorCheckResult& FloorResult) const
{
	const float StepHeight = GetTargetHeight() - FloorResult.FloorDist;

	bool bWalkable = StepHeight <= SharedSettings->MaxStepHeight;
	constexpr float MinStepHeight = 2.0f;
	const bool SteppingUp = StepHeight > MinStepHeight;
	if (bWalkable && SteppingUp)
	{
		bWalkable = UGroundMovementUtils::CanStepUpOnHitSurface(FloorResult.HitResult);
	}

	return bWalkable;
}

void UChaosWalkingMode::GetFloorAndCheckMovement(const FMoverDefaultSyncState& SyncState, const FProposedMove& ProposedMove, const FChaosMoverSimulationDefaultInputs& DefaultSimInputs, float DeltaSeconds, FFloorCheckResult& OutFloorResult, FWaterCheckResult& OutWaterResult, FVector& OutDeltaPos) const
{
	FVector DeltaPos = ProposedMove.LinearVelocity * DeltaSeconds;
	OutDeltaPos = DeltaPos;

	UE::ChaosMover::Utils::FFloorSweepParams SweepParams{
		.ResponseParams = DefaultSimInputs.CollisionResponseParams,
		.QueryParams = DefaultSimInputs.CollisionQueryParams,
		.Location = SyncState.GetLocation_WorldSpace(),
		.DeltaPos = DeltaPos,
		.UpDir = DefaultSimInputs.UpDir,
		.World = DefaultSimInputs.World,
		.QueryDistance = GetTargetHeight() + SharedSettings->MaxStepHeight,
		.QueryRadius = FMath::Min(GetGroundQueryRadius(), FMath::Max(DefaultSimInputs.PawnCollisionRadius - 5.0f, 0.0f)),
		.MaxWalkSlopeCosine = GetMaxWalkSlopeCosine(),
		.TargetHeight = GetTargetHeight(),
		.CollisionChannel = DefaultSimInputs.CollisionChannel
	};

	// First, try a sweep at the end position
	UE::ChaosMover::Utils::FloorSweep_Internal(SweepParams, OutFloorResult, OutWaterResult);

	if (!OutFloorResult.bBlockingHit)
	{
		// No result at the end position. Fall back on the current floor result
		return;
	}

	bool bWalkableFloor = OutFloorResult.bWalkableFloor && CanStepUpOnHitSurface(OutFloorResult);
	if (bWalkableFloor)
	{
		// Walkable floor found
		return;
	}

	// Hit something but not walkable. Try a new query to find a walkable surface
	const float StepBlockedHeight = GetTargetHeight() - DefaultSimInputs.PawnCollisionHalfHeight + DefaultSimInputs.PawnCollisionRadius;
	const float StepHeight = GetTargetHeight() - OutFloorResult.FloorDist;

	if (StepHeight > StepBlockedHeight)
	{
		// Collision should prevent movement. Just try to find ground at start of movement
		SweepParams.QueryRadius = 0.75f * GetGroundQueryRadius();
		SweepParams.DeltaPos = FVector::ZeroVector;

		UE::ChaosMover::Utils::FloorSweep_Internal(SweepParams, OutFloorResult, OutWaterResult);
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
	FVector HorizSurfaceDir = FVector::VectorPlaneProject(OutFloorResult.HitResult.ImpactNormal, DefaultSimInputs.UpDir);
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
		HorizSurfaceDir = FVector::VectorPlaneProject(OutFloorResult.HitResult.Normal, DefaultSimInputs.UpDir);
		HorizSurfaceDirSizeSq = HorizSurfaceDir.SizeSquared();

		if (HorizSurfaceDirSizeSq > UE_SMALL_NUMBER)
		{
			HorizSurfaceDir *= FMath::InvSqrt(HorizSurfaceDirSizeSq);
			bFoundOutwardDir = true;
		}
	}

	if (bFoundOutwardDir)
	{
		const float DP = DeltaPos.Dot(HorizSurfaceDir);
		FVector NewDeltaPos = DeltaPos;
		if (DP > 0.0f)
		{
			// If we're moving away try a ray query at the end of the motion
			SweepParams.QueryRadius = 0.0f;
		}
		else
		{
			// Otherwise, try to find a walkable floor along the surface
			SweepParams.QueryRadius = 0.25f * GetGroundQueryRadius();
			NewDeltaPos = DeltaPos - DP * HorizSurfaceDir;
		}
		SweepParams.DeltaPos = NewDeltaPos;

		UE::ChaosMover::Utils::FloorSweep_Internal(SweepParams, OutFloorResult, OutWaterResult);
		OutFloorResult.bWalkableFloor = OutFloorResult.bWalkableFloor && CanStepUpOnHitSurface(OutFloorResult);

		if (OutFloorResult.bWalkableFloor)
		{
			OutDeltaPos = NewDeltaPos;
		}
	}
	else
	{
		OutDeltaPos = FVector::ZeroVector;
	}
}

void UChaosWalkingMode::ModifyContacts(const FMoverTimeStep& TimeStep, const UE::ChaosMover::FSimulationInputData& InputData, const UE::ChaosMover::FSimulationOutputData& OutputData, Chaos::FCollisionContactModifier& Modifier) const
{
	check(Simulation);

	// Get the updated (character) particle
	Chaos::FGeometryParticleHandle* UpdatedParticle = nullptr;
	const FChaosMoverSimulationDefaultInputs* SimInputs = Simulation->GetLocalSimInput().FindDataByType<FChaosMoverSimulationDefaultInputs>();
	if (SimInputs)
	{
		Chaos::FReadPhysicsObjectInterface_Internal ReadInterface = Chaos::FPhysicsObjectInternalInterface::GetRead();
		UpdatedParticle = ReadInterface.GetParticle(SimInputs->PhysicsObject);
	}

	if (!UpdatedParticle)
	{
		return;
	}

	// Try and find the ground particle if there is one in the latest floor result
	Chaos::FGeometryParticleHandle* GroundParticle = nullptr;
	FFloorCheckResult FloorResult;
	FloorResult.FloorDist = 1.0e10;
	if (const UMoverBlackboard* SimBlackboard = Simulation->GetBlackboard())
	{
		SimBlackboard->TryGet(CommonBlackboard::LastFloorResult, FloorResult);
		Chaos::FReadPhysicsObjectInterface_Internal Interface = Chaos::FPhysicsObjectInternalInterface::GetRead();
		GroundParticle = Interface.GetParticle(FloorResult.HitResult.PhysicsObject);
	}

	const float CharacterHeight = UpdatedParticle->GetX().Dot(SimInputs->UpDir);
	const float EndCapHeight = CharacterHeight - SimInputs->PawnCollisionHalfHeight + SimInputs->PawnCollisionRadius;
	constexpr float CosThetaMax = 0.707f;

	float MinContactHeightStepUps = CharacterHeight - 1.0e10f;
	const float StepDistance = FMath::Abs(GetTargetHeight() - FloorResult.FloorDist);
	if (StepDistance >= UE_KINDA_SMALL_NUMBER)
	{
		MinContactHeightStepUps = CharacterHeight - GetTargetHeight() + SharedSettings->MaxStepHeight;
	}

	for (Chaos::FContactPairModifier& PairModifier : Modifier.GetContacts(UpdatedParticle))
	{
		const int32 CharacterIdx = UpdatedParticle == PairModifier.GetParticlePair()[0] ? 0 : 1;
		Chaos::FGeometryParticleHandle* OtherParticle = PairModifier.GetOtherParticle(UpdatedParticle);
		if (!OtherParticle)
		{
			continue;
		}
		const bool bOtherParticleIsGround = OtherParticle == GroundParticle;

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
			else if (bOtherParticleIsGround && CharacterPoint.Z < MinContactHeightStepUps)
			{
				// In the case of steps ups disable all contacts below the max step height
				PairModifier.SetContactPointDisabled(Idx);
			}
		}
	}
}

