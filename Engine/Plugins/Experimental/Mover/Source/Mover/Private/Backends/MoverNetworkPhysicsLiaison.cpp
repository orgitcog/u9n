// Copyright Epic Games, Inc. All Rights Reserved.

#include "Backends/MoverNetworkPhysicsLiaison.h"

#include "Chaos/Character/CharacterGroundConstraint.h"
#include "Chaos/PhysicsObject.h"
#include "MovementModeStateMachine.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"
#include "PBDRigidsSolver.h"
#include "Chaos/PhysicsObjectInterface.h"
#include "Chaos/PhysicsObjectInternalInterface.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "PhysicsMover/PhysicsMovementUtils.h"
#include "PhysicsMover/Modes/PhysicsDrivenWalkingMode.h"
#include "PhysicsProxy/CharacterGroundConstraintProxy.h"
#include "PhysicsMover/PhysicsMoverSimulationTypes.h"
#include "DefaultMovementSet/Modes/FallingMode.h"
#include "Misc/DataValidation.h"
#include "ChaosVisualDebugger/MoverCVDRuntimeTrace.h"

#define LOCTEXT_NAMESPACE "Mover"

extern FPhysicsDrivenMotionDebugParams GPhysicsDrivenMotionDebugParams;

//////////////////////////////////////////////////////////////////////////
// UMoverNetworkPhysicsLiaisonComponent

UMoverNetworkPhysicsLiaisonComponent::UMoverNetworkPhysicsLiaisonComponent()
{
}

void UMoverNetworkPhysicsLiaisonComponent::OnRegister()
{
	Super::OnRegister();

	CommonMovementSettings = GetMoverComponent().FindSharedSettings_Mutable<UCommonLegacyMovementSettings>();
	check(CommonMovementSettings);
}

//////////////////////////////////////////////////////////////////////////
// UMoverNetworkPhysicsLiaisonComponent UObject interface

void UMoverNetworkPhysicsLiaisonComponent::SetupConstraint()
{
	if (Chaos::FPhysicsSolver* Solver = GetPhysicsSolver())
	{
		const UMoverComponent& MoverComp = GetMoverComponent();
		if (FBodyInstance* BI = MoverComp.UpdatedCompAsPrimitive ? MoverComp.UpdatedCompAsPrimitive->GetBodyInstance() : nullptr)
		{
			if (Chaos::FSingleParticlePhysicsProxy* CharacterProxy = BI->GetPhysicsActor())
			{
				// Create and register the constraint
				Constraint = MakeUnique<Chaos::FCharacterGroundConstraint>();
				Constraint->Init(CharacterProxy);
				Solver->RegisterObject(Constraint.Get());

				// Set the common settings
				// The rest get set every frame depending on the current movement mode
				Constraint->SetCosMaxWalkableSlopeAngle(CommonMovementSettings->MaxWalkSlopeCosine);
				Constraint->SetVerticalAxis(MoverComp.GetUpDirection());
				Constraint->SetMaxCharacterGroundMassRatio(GPhysicsDrivenMotionDebugParams.MaxCharacterGroundMassRatio);

				// Enable Physics Simulation
				MoverComp.UpdatedCompAsPrimitive->SetSimulatePhysics(true);

				// Turn off sleeping
				Chaos::FRigidBodyHandle_External& PhysicsBody = CharacterProxy->GetGameThreadAPI();
				PhysicsBody.SetSleepType(Chaos::ESleepType::NeverSleep);
			}
		}
	}
}

void UMoverNetworkPhysicsLiaisonComponent::DestroyConstraint()
{
	UWorld* World = GetWorld();
	if (World && World->IsGameWorld() && HasValidPhysicsState())
	{
		if (FPhysScene_Chaos* PhysScene = World->GetPhysicsScene())
		{
			if (Chaos::FPhysicsSolver* Solver = PhysScene->GetSolver())
			{
				// Note: Proxy gets destroyed when the constraint is deregistered and that deletes the constraint
				Solver->UnregisterObject(Constraint.Release());
			}
		}
	}
}

void UMoverNetworkPhysicsLiaisonComponent::HandleComponentPhysicsStateChanged(UPrimitiveComponent* ChangedComponent, EComponentPhysicsStateChange StateChange)
{
	if (StateChange == EComponentPhysicsStateChange::Destroyed)
	{
		DestroyConstraint();
	}
	else if (StateChange == EComponentPhysicsStateChange::Created)
	{
		SetupConstraint();

		InitializeSimOutputData();
	}

	Super::HandleComponentPhysicsStateChanged(ChangedComponent, StateChange);
}

bool UMoverNetworkPhysicsLiaisonComponent::HasValidPhysicsState() const
{
	return Constraint.IsValid() && Constraint->IsValid();
}

void UMoverNetworkPhysicsLiaisonComponent::OnCreatePhysicsState()
{
	Super::OnCreatePhysicsState();

	SetupConstraint();
}

void UMoverNetworkPhysicsLiaisonComponent::OnDestroyPhysicsState()
{
	DestroyConstraint();

	Super::OnDestroyPhysicsState();
}

#if WITH_EDITOR
EDataValidationResult UMoverNetworkPhysicsLiaisonComponent::ValidateData(FDataValidationContext& Context, const UMoverComponent& ValidationMoverComp) const
{
	if (const AActor* OwnerActor = ValidationMoverComp.GetOwner())
	{
		if (!OwnerActor->IsReplicatingMovement())
		{
			Context.AddError(FText::Format(LOCTEXT("RequiresReplicateMovementProperty", "The owning actor ({0}) does not have the ReplicateMovement property enabled. This is required for use with Chaos Networked Physics and poor quality movement with occur without it. Please enable it."),
				FText::FromString(GetNameSafe(OwnerActor))));

			return EDataValidationResult::Invalid;
		}
	}

	return Super::ValidateData(Context, ValidationMoverComp);
}
#endif

//////////////////////////////////////////////////////////////////////////

void UMoverNetworkPhysicsLiaisonComponent::UpdateConstraintSettings()
{
	if (HasValidState())
	{
		const UMoverComponent& MoverComp = GetMoverComponent();
		Constraint->SetVerticalAxis(MoverComp.GetUpDirection());
		Constraint->SetCosMaxWalkableSlopeAngle(CommonMovementSettings->MaxWalkSlopeCosine);
		Constraint->SetMaxCharacterGroundMassRatio(GPhysicsDrivenMotionDebugParams.MaxCharacterGroundMassRatio);

		const UBaseMovementMode* CurrentMode = MoverComp.GetActiveMode();
		if (CurrentMode && CurrentMode->Implements<UPhysicsCharacterMovementModeInterface>())
		{
			const IPhysicsCharacterMovementModeInterface* PhysicsMode = CastChecked<IPhysicsCharacterMovementModeInterface>(CurrentMode);
			PhysicsMode->UpdateConstraintSettings(*Constraint);
		}
	}
}

void UMoverNetworkPhysicsLiaisonComponent::ConsumeOutput_External(const FPhysicsMoverAsyncOutput& Output, const double OutputTimeInSeconds)
{
	if (Output.bIsValid)
	{
		UpdateConstraintSettings();
	}

	Super::ConsumeOutput_External(Output, OutputTimeInSeconds);
}

void UMoverNetworkPhysicsLiaisonComponent::PostPhysicsUpdate_External()
{
	const UMoverComponent& MoverComp = GetMoverComponent();
	const UBaseMovementMode* PrevMode = MoverComp.GetMovementMode();
	bool bWasFalling = PrevMode ? PrevMode->HasGameplayTag(Mover_IsFalling, true) : false;

	Super::PostPhysicsUpdate_External();

	UpdateConstraintSettings();
	if (bWasFalling && PrevMode != MoverComp.GetMovementMode() && MoverComp.HasGameplayTag(Mover_IsOnGround, true))
	{
		if (UFallingMode* FallingMode = MoverComp.FindMode_Mutable<UFallingMode>())
		{
			FHitResult HitResult;
			MoverComp.TryGetFloorCheckHitResult(HitResult);

			FallingMode->OnLanded.Broadcast(MoverComp.GetMovementModeName(), HitResult);
		}
	}
}

void UMoverNetworkPhysicsLiaisonComponent::OnContactModification_Internal(const FPhysicsMoverAsyncInput& Input, Chaos::FCollisionContactModifier& Modifier) const
{
	Super::OnContactModification_Internal(Input, Modifier);

	if (!HasValidState())
	{
		return;
	}

	Chaos::FCharacterGroundConstraintHandle* ConstraintHandle = nullptr;
	if (Constraint && Constraint->IsValid())
	{
		ConstraintHandle = Constraint->GetProxy<Chaos::FCharacterGroundConstraintProxy>()->GetPhysicsThreadAPI();
	}

	if (!ConstraintHandle || !ConstraintHandle->IsEnabled() || !ConstraintHandle->GetCharacterParticle())
	{
		return;
	}

	const UMoverComponent& MoverComp = GetMoverComponent();

	// Global

	TArray<const Chaos::FGeometryParticleHandle*> IgnoreParticles;
	if (MoverComp.UpdatedCompAsPrimitive)
	{
		for (const UPrimitiveComponent* PrimComp : MoverComp.UpdatedCompAsPrimitive->GetMoveIgnoreComponents())
		{
			TArray<Chaos::FPhysicsObject*> PhysObjs = PrimComp->GetAllPhysicsObjects();
			Chaos::FReadPhysicsObjectInterface_Internal Interface = Chaos::FPhysicsObjectInternalInterface::GetRead();
			for (Chaos::FPhysicsObject* PhysObj : PhysObjs)
			{
				IgnoreParticles.Add(Interface.GetParticle(PhysObj));
			}
		}

		for (const AActor* Actor : MoverComp.UpdatedCompAsPrimitive->GetMoveIgnoreActors())
		{
			for (UActorComponent* ActorComp : Actor->GetComponents())
			{
				if (IPhysicsComponent* PhysComp = Cast<IPhysicsComponent>(ActorComp))
				{
					TArray<Chaos::FPhysicsObject*> PhysObjs = PhysComp->GetAllPhysicsObjects();
					Chaos::FReadPhysicsObjectInterface_Internal Interface = Chaos::FPhysicsObjectInternalInterface::GetRead();
					for (Chaos::FPhysicsObject* PhysObj : PhysObjs)
					{
						if (const Chaos::FGeometryParticleHandle* ParticleHandle = Interface.GetParticle(PhysObj))
						{
							IgnoreParticles.Add(ParticleHandle);
						}
					}
				}
			}
		}
	}

	if (!IgnoreParticles.IsEmpty())
	{
		Chaos::FPBDRigidParticleHandle* CharacterParticle = ConstraintHandle->GetCharacterParticle()->CastToRigidParticle();
		Chaos::FContactPairModifierParticleRange Contacts = Modifier.GetContacts(CharacterParticle);
		for (auto& Contact : Contacts)
		{
			const Chaos::FGeometryParticleHandle* OtherParticle = Contact.GetOtherParticle(CharacterParticle);
			for (const Chaos::FGeometryParticleHandle* Particle : IgnoreParticles)
			{
				if (Particle == OtherParticle)
				{
					Contact.Disable();
					break;
				}
			}
		}
	}

	// Per Mode

	if (MoverComp.MovementModes.Contains(Input.SyncState.MovementMode))
	{
		if (const IPhysicsCharacterMovementModeInterface* PhysicsMode = Cast<const IPhysicsCharacterMovementModeInterface>(MoverComp.MovementModes[Input.SyncState.MovementMode]))
		{
			const FPhysicsMoverSimulationContactModifierParams Params{ ConstraintHandle, MoverComp.UpdatedCompAsPrimitive };
			PhysicsMode->OnContactModification_Internal(Params, Modifier);
		}
	}
}

bool UMoverNetworkPhysicsLiaisonComponent::HasValidState() const
{
	// For character movement, we need to have a valid input producer (i.e. the character itself)
	return Super::HasValidState() && GetMoverComponent().InputProducer;
}

bool UMoverNetworkPhysicsLiaisonComponent::CanProcessInputs_Internal(const FPhysicsMoverAsyncInput& Input) const
{
	return Super::CanProcessInputs_Internal(Input);
}

void UMoverNetworkPhysicsLiaisonComponent::PerformProcessInputs_Internal(int32 PhysicsStep, float DeltaTime, const FPhysicsMoverAsyncInput& Input) const
{
	Super::PerformProcessInputs_Internal(PhysicsStep, DeltaTime, Input);

	// Override common settings data with data from FMovementSettingsInputs if present in the input cmd
	if (const FMovementSettingsInputs* MovementSettings = Input.InputCmd.InputCollection.FindDataByType<FMovementSettingsInputs>())
	{
		CommonMovementSettings->MaxSpeed = MovementSettings->MaxSpeed;
		CommonMovementSettings->Acceleration = MovementSettings->Acceleration;
	}
	
	// This will only do something if CVD is actively tracing and the mover info CVD data channel checked
	UE::MoverUtils::FMoverCVDRuntimeTrace::TraceMoverData(&GetMoverComponent(), &Input.InputCmd, &Input.SyncState);
}

bool UMoverNetworkPhysicsLiaisonComponent::CanSimulate_Internal(const FPhysicsMoverSimulationTickParams& TickParams, const FPhysicsMoverAsyncInput& Input) const
{
	if (Super::CanSimulate_Internal(TickParams, Input))
	{
		Chaos::FCharacterGroundConstraintHandle* ConstraintHandle = Constraint->GetProxy<Chaos::FCharacterGroundConstraintProxy>()->GetPhysicsThreadAPI();
		if (!ConstraintHandle || !ConstraintHandle->IsEnabled())
		{
			return false;
		}

		if (!Cast<IPhysicsCharacterMovementModeInterface>(GetMoverComponent().MovementModes.FindChecked(Input.SyncState.MovementMode)))
		{
			ensureMsgf(false, TEXT("Attempting to run non-character physics movement mode %s in physics mover update. Only modes that implement IPhysicsCharacterMovementModeInterface can be used with the CharacterPhysicsLiaison."), *Input.SyncState.MovementMode.ToString());
			UE_LOG(LogMover, Verbose, TEXT("Attempting to run non-character physics movement mode %s in physics mover update. Only modes that implement IPhysicsCharacterMovementModeInterface can be used with the CharacterPhysicsLiaison."),
				*Input.SyncState.MovementMode.ToString());
			return false;
		}

		return true;
	}

	return false;
}

void UMoverNetworkPhysicsLiaisonComponent::PerformPreSimulate_Internal(const FPhysicsMoverSimulationTickParams& TickParams, const FPhysicsMoverAsyncInput& Input, FPhysicsMoverAsyncOutput& Output) const
{
	Chaos::FCharacterGroundConstraintHandle* ConstraintHandle = Constraint->GetProxy<Chaos::FCharacterGroundConstraintProxy>()->GetPhysicsThreadAPI();
	Chaos::FPBDRigidParticleHandle* CharacterParticle = ConstraintHandle->GetCharacterParticle()->CastToRigidParticle();
	UMoverComponent& MoverComp = GetMoverComponent();

	ConstraintHandle->SetGroundParticle(nullptr);
	
	//@todo DanH: Ideally the physics walking mode can extricate the ground velocity from the original calculation
	// Make the sync state velocity relative to the ground if walking
	FVector LocalGroundVelocity = FVector::ZeroVector;
	const UBaseMovementMode* InputMode = MoverComp.MovementModes[Input.SyncState.MovementMode];
	if (InputMode->HasGameplayTag(Mover_IsOnGround, true))
	{
		const UMoverBlackboard* Blackboard = MoverComp.GetSimBlackboard();
		FFloorCheckResult LastFloorResult;
		if (Blackboard->TryGet(CommonBlackboard::LastFloorResult, LastFloorResult))
		{
			LocalGroundVelocity = UPhysicsMovementUtils::ComputeGroundVelocityFromHitResult(CharacterParticle->GetX(), LastFloorResult.HitResult, TickParams.DeltaTimeSeconds);
		}
	}

	// Add AI Move if it exists
	FVector AIMoveVelocity = FVector::ZeroVector;
	if (const FMoverAIInputs* MoverAIInputs = Input.InputCmd.InputCollection.FindDataByType<FMoverAIInputs>())
	{
		AIMoveVelocity = MoverAIInputs->RVOVelocityDelta;
	}

	FMoverDefaultSyncState& InputSyncState = Input.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();
	InputSyncState.SetTransforms_WorldSpace(CharacterParticle->GetX(), FRotator(CharacterParticle->GetR()), CharacterParticle->GetV() - LocalGroundVelocity + AIMoveVelocity);

	FCharacterDefaultInputs& CharacterDefaultInputs = Input.InputCmd.InputCollection.FindOrAddMutableDataByType<FCharacterDefaultInputs>();
	if (!CharacterDefaultInputs.SuggestedMovementMode.IsNone())
	{
		MoverComp.QueueNextMode(CharacterDefaultInputs.SuggestedMovementMode);
		CharacterDefaultInputs.SuggestedMovementMode = NAME_None;
	}

	// Make sure we have a valid input for the update
	if (CharacterDefaultInputs.GetMoveInputType() == EMoveInputType::Invalid)
	{
		CharacterDefaultInputs.SetMoveInput(EMoveInputType::DirectionalIntent, FVector::ZeroVector);
	}

	// Cache the particle XR before calling the parent, because the parent will teleport if the target is too far away and change the particle's transform
	const FVector CharacterParticleStartingX = CharacterParticle->GetX();
	const FRotator CharacterParticleStartingR = FRotator(CharacterParticle->GetR());

	Super::PerformPreSimulate_Internal(TickParams, Input, Output);

	FMoverDefaultSyncState& OutputSyncState = Output.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();

	// Add back the ground velocity that was subtracted to the movement velocity in local space
	FVector TargetVelocity = OutputSyncState.GetVelocity_WorldSpace() + LocalGroundVelocity;
	CharacterParticle->SetV(TargetVelocity);

	FRotator DeltaRotation = OutputSyncState.GetOrientation_WorldSpace() - CharacterParticleStartingR;
	FRotator Winding, Remainder;
	DeltaRotation.GetWindingAndRemainder(Winding, Remainder);
	float TargetDeltaFacing = FMath::DegreesToRadians(Remainder.Yaw);
	if (TickParams.DeltaTimeSeconds > UE_SMALL_NUMBER)
	{
		FVector AngularVelocity = (TargetDeltaFacing / TickParams.DeltaTimeSeconds) * Chaos::FVec3::ZAxisVector;
			
		if (!CommonMovementSettings->bShouldRemainVertical)
		{
			FVector PreviousUp = CharacterParticle->GetR().RotateVector(FVector::UpVector).GetSafeNormal();
			FVector TargetUp = OutputSyncState.GetOrientation_WorldSpace().Quaternion().RotateVector(FVector::UpVector).GetSafeNormal();
			AngularVelocity += (PreviousUp.Cross(TargetUp).GetSafeNormal() * FMath::Acos(TargetUp.Dot(PreviousUp))) / TickParams.DeltaTimeSeconds;
		}
			
		CharacterParticle->SetW(AngularVelocity);
	}		

	// Update the TargetHeight Constraint Settings
	if (const UBaseMovementMode* CurrentMode = MoverComp.FindMovementModeByName(Output.SyncState.MovementMode))
	{
		if (const IPhysicsCharacterMovementModeInterface* PhysicsMode = CastChecked<IPhysicsCharacterMovementModeInterface>(CurrentMode))
		{
			Chaos::FCharacterGroundConstraintSettings& ConstraintSettings = ConstraintHandle->GetSettings_Mutable();
			ConstraintSettings.TargetHeight = PhysicsMode->GetTargetHeight();
		}
	}

	// Update the constraint data based on the floor result
	if (Output.FloorResult.bBlockingHit)
	{
		// Set the ground particle on the constraint
		Chaos::FGeometryParticleHandle* GroundParticle = nullptr;

		if (Chaos::FPhysicsObjectHandle PhysicsObject = Output.FloorResult.HitResult.PhysicsObject)
		{
			Chaos::FReadPhysicsObjectInterface_Internal Interface = Chaos::FPhysicsObjectInternalInterface::GetRead();
			if (!Interface.AreAllDisabled({ PhysicsObject }))
			{
				GroundParticle = Interface.GetParticle(PhysicsObject);
				WakeParticleIfSleeping(GroundParticle);
			}
		}
		ConstraintHandle->SetGroundParticle(GroundParticle);

		// Set the max walkable slope angle using any override from the hit component
		float WalkableSlopeCosine = ConstraintHandle->GetSettings().CosMaxWalkableSlopeAngle;
		if (Output.FloorResult.HitResult.Component != nullptr)
		{
			const FWalkableSlopeOverride& SlopeOverride = Output.FloorResult.HitResult.Component->GetWalkableSlopeOverride();
			WalkableSlopeCosine = SlopeOverride.ModifyWalkableFloorZ(WalkableSlopeCosine);
		}

		if (!Output.FloorResult.bWalkableFloor)
		{
			WalkableSlopeCosine = 2.0f;
		}

		const FVector TargetDeltaPos = OutputSyncState.GetLocation_WorldSpace() - CharacterParticleStartingX;
		ConstraintHandle->SetData({
			Output.FloorResult.HitResult.ImpactNormal,
			TargetDeltaPos,
			TargetDeltaFacing,
			Output.FloorResult.FloorDist,
			WalkableSlopeCosine
			});

		// Note: We want to record the movement base but we don't record the transform
		// so don't use this to get a relative transform for the sync state
		OutputSyncState.SetMovementBase(Output.FloorResult.HitResult.GetComponent());
	}
	else
	{
		ConstraintHandle->SetGroundParticle(nullptr);
		
		OutputSyncState.SetMovementBase(nullptr);

		ConstraintHandle->SetData({
			ConstraintHandle->GetSettings().VerticalAxis,
			Chaos::FVec3::ZeroVector,
			0.0,
			1.0e10,
			0.5f
			});
	}
}

Chaos::FPhysicsObject* UMoverNetworkPhysicsLiaisonComponent::GetControlledPhysicsObject() const
{
	if (Constraint && Constraint->GetCharacterParticleProxy())
	{
		return Constraint->GetCharacterParticleProxy()->GetPhysicsObject();
	}
	return nullptr;
}

#undef LOCTEXT_NAMESPACE
