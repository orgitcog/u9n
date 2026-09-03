// Copyright Epic Games, Inc. All Rights Reserved.

#include "Backends/MoverPathedPhysicsLiaison.h"

#include "Chaos/PhysicsObjectInternalInterface.h"
#include "PBDRigidsSolver.h"
#include "TimerManager.h"
#include "Kismet/KismetMathLibrary.h"
#include "PhysicsMover/PathedMovement/PathedMovementMode.h"
#include "PhysicsMover/PathedMovement/PathedPhysicsMoverComponent.h"
#include "Runtime/Experimental/Chaos/Private/Chaos/PhysicsObjectInternal.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MoverPathedPhysicsLiaison)

#define LOCTEXT_NAMESPACE "Mover"

TAutoConsoleVariable<float> CVarPathedPhysicsLatencyDelayMs(
	TEXT("Mover.PathedPhysics.StartMovingLatencyDelayMs"),
	200.f,
	TEXT("How long (in ms) to delay starting pathed movement on the server to give the client(s) time to find out about it"));


UMoverPathedPhysicsLiaisonComponent::UMoverPathedPhysicsLiaisonComponent()
	: PhysicsUserData(&ConstraintInstance)
{
	bWantsInitializeComponent = true;
	
	if (Chaos::FPhysicsSolverBase::IsNetworkPhysicsPredictionEnabled())
	{
		if (AActor* MyActor = GetOwner())
		{
			// Regardless of how rewinds are triggered, they should always result in a resimulation
			MyActor->SetPhysicsReplicationMode(EPhysicsReplicationMode::Resimulation);
		}
	}
}

void UMoverPathedPhysicsLiaisonComponent::InitializeComponent()
{
	Super::InitializeComponent();
	
	UPathedPhysicsMoverComponent& MoverComp = GetPathedMoverComp();
	MoverComp.OnMovementModeChanged.AddUniqueDynamic(this, &ThisClass::HandleMovementModeChanged);

	if (const UPathedPhysicsMovementMode* InitialPathedMode = MoverComp.FindMode_Mutable<UPathedPhysicsMovementMode>(MoverComp.StartingMovementMode))
	{
		ApplyPathModeConfig(*InitialPathedMode);
	}
}

bool UMoverPathedPhysicsLiaisonComponent::HasValidPhysicsState() const
{
	return ConstraintHandle.IsValid();
}

void UMoverPathedPhysicsLiaisonComponent::OnCreatePhysicsState()
{
	Super::OnCreatePhysicsState();

	CreateTargetJoint();
}

void UMoverPathedPhysicsLiaisonComponent::OnDestroyPhysicsState()
{
	DestroyTargetJoint();

	Super::OnDestroyPhysicsState();
}

void UMoverPathedPhysicsLiaisonComponent::ConsumeOutput_External(const FPhysicsMoverAsyncOutput& Output, const double OutputTimeInSeconds)
{
	Super::ConsumeOutput_External(Output, OutputTimeInSeconds);
	
	if (const FPathedPhysicsMovementState* OutputMoveState = Output.SyncState.SyncStateCollection.FindDataByType<FPathedPhysicsMovementState>())
	{
		const bool bWasMoving = IsMoving();
		const bool bWasJointEnabled = IsJointEnabled();

		Inputs_External = OutputMoveState->MutableProps;		

		if (bWasMoving != IsMoving())
		{
			GetPathedMoverComp().NotifyIsMovingChanged(IsMoving());
		}

		if (bWasJointEnabled != IsJointEnabled() && NetworkPhysicsComponent && !NetworkPhysicsComponent->HasServerWorld())
		{
			if (UPathedPhysicsMovementMode* PathedMode = GetMoverComponent().GetActiveMode_Mutable<UPathedPhysicsMovementMode>())
			{
				// To have bIsJointEnabled on the mode behave like a normal replicated property, forward changes to it from physics input replication to the mode on the client
				// We don't listen for changes to the property on the client, so this is done purely so anything external that checks UPathedPhysicsMovementMode::IsUsingJoint()
				// on the client will still get the correct answer.
				//@todo DanH: Still not right - this will get rejected because client, and we still do want to respond to the change on the client as well to change the CompareState setting
				PathedMode->SetUseJointConstraint(IsJointEnabled());
			}
		}
	}
}

void UMoverPathedPhysicsLiaisonComponent::PostPhysicsUpdate_External()
{
	Super::PostPhysicsUpdate_External();

	//@todo DanH: Does this help with jitter on non-PBCM CMCs?
	const UMoverComponent& MoverComp = GetMoverComponent();
	MoverComp.GetUpdatedComponent()->ComponentVelocity = MoverComp.GetVelocity();
}

void UMoverPathedPhysicsLiaisonComponent::SetPathOrigin(const FTransform& NewPathOrigin)
{
	if (!GetPathOrigin().Equals(NewPathOrigin) && GetOwnerRole() == ROLE_Authority)
	{
		Inputs_External.PathOrigin = NewPathOrigin;
	}
}

void UMoverPathedPhysicsLiaisonComponent::SetPlaybackDirection(bool bPlayForward)
{
	if (bPlayForward == IsInReverse() && GetOwnerRole() == ROLE_Authority)
	{
		Inputs_External.bIsInReverse = !bPlayForward;
	}
}

void UMoverPathedPhysicsLiaisonComponent::SetIsMoving(bool bShouldMove, float StartDelay)
{
	if (bShouldMove != IsMoving() && GetOwnerRole() == ROLE_Authority)
	{
		if (!bShouldMove)
		{
			Inputs_External.MovementStartFrame = INDEX_NONE;
		}
		else if (Chaos::FPhysicsSolver* Solver = GetPhysicsSolver(); ensure(Solver))
		{
			if (bUsingAsyncPhysics)
			{
				const int32 CurrentFrame = Solver->GetCurrentFrame();
				const int32 StartDelayFrames = FMath::FloorToInt32(StartDelay / Solver->GetAsyncDeltaTime());
				const int32 LatencyDelayFrames = FMath::CeilToInt32(CVarPathedPhysicsLatencyDelayMs.GetValueOnGameThread() / (Solver->GetAsyncDeltaTime() * 1000.f));

				Inputs_External.MovementStartFrame = CurrentFrame + StartDelayFrames + LatencyDelayFrames;
			}
			else if (const UWorld* World = GetWorld())
			{
				// In a standalone game that isn't using async physics, the start delay can't be reliably converted to a physics frame.
				// So we have to use a world timer instead of delaying the start frame
				FTimerManager& TimerManager = World->GetTimerManager();
				if (DelayedStartTimerHandle.IsValid())
				{
					TimerManager.ClearTimer(DelayedStartTimerHandle);
				}

				if (StartDelay > 0.f)
				{
					TimerManager.SetTimer(DelayedStartTimerHandle, FTimerDelegate::CreateWeakLambda(this, [this]()
					{
						Inputs_External.MovementStartFrame = GetCurrentSimFrame();
					}), StartDelay, false);
				}
				else
				{
					Inputs_External.MovementStartFrame = Solver->GetCurrentFrame();
				}
			}			
		}
	}
}

void UMoverPathedPhysicsLiaisonComponent::SetPlaybackBehavior(EPathedPhysicsPlaybackBehavior PlaybackBehavior)
{
	if (PlaybackBehavior != GetPlaybackBehavior() && GetOwnerRole() == ROLE_Authority)
	{
		Inputs_External.PlaybackBehavior = PlaybackBehavior;
	}
}

UPathedPhysicsMoverComponent& UMoverPathedPhysicsLiaisonComponent::GetPathedMoverComp() const
{
	return *GetOuterUPathedPhysicsMoverComponent();
}

void UMoverPathedPhysicsLiaisonComponent::HandleComponentPhysicsStateChanged(UPrimitiveComponent* ChangedComponent, EComponentPhysicsStateChange StateChange)
{
	Super::HandleComponentPhysicsStateChanged(ChangedComponent, StateChange);
	if (StateChange == EComponentPhysicsStateChange::Destroyed)
	{
		DestroyTargetJoint();
	}
	else if (StateChange == EComponentPhysicsStateChange::Created)
	{
		CreateTargetJoint();
	}
}

void UMoverPathedPhysicsLiaisonComponent::PerformProduceInput_External(float DeltaTime, FPhysicsMoverAsyncInput& Input)
{
	Super::PerformProduceInput_External(DeltaTime, Input);
	
	FPathedPhysicsMovementInputs& InputState = Input.InputCmd.InputCollection.FindOrAddMutableDataByType<FPathedPhysicsMovementInputs>();
	InputState.Props = Inputs_External;
}

bool UMoverPathedPhysicsLiaisonComponent::CanProcessInputs_Internal(const FPhysicsMoverAsyncInput& Input) const
{
	if (Super::CanProcessInputs_Internal(Input))
	{
		if (Chaos::FJointConstraint* JointConstraint = GetJointConstraint();
			!JointConstraint ||
			!JointConstraint->IsValid() ||
			!JointConstraint->GetProxy<Chaos::FJointConstraintPhysicsProxy>()->GetHandle())
		{
			return false;
		}

		if (!GetMoverComponent().FindMode_Mutable<UPathedPhysicsMovementMode>(Input.SyncState.MovementMode))
		{
			return false;
		}
		
		return true;
	}
	return false;
}

void UMoverPathedPhysicsLiaisonComponent::PerformProcessInputs_Internal(int32 PhysicsStep, float DeltaTime, const FPhysicsMoverAsyncInput& Input) const
{
	Super::PerformProcessInputs_Internal(PhysicsStep, DeltaTime, Input);
	
	const UPathedPhysicsMovementMode* PathedMode = GetMoverComponent().GetActiveMode<UPathedPhysicsMovementMode>();
	if (const FPathedPhysicsMovementInputs* PathedMovementInputs = Input.InputCmd.InputCollection.FindDataByType<FPathedPhysicsMovementInputs>())
	{
		Chaos::FPBDRigidParticleHandle& ParticleHandle = *GetControlledParticle_Internal();
		Chaos::FJointConstraint& JointConstraint = *GetJointConstraint();

		bool bIsFirstProcess = false;
		FPathedPhysicsMovementState* InputMoveState = Input.SyncState.SyncStateCollection.FindMutableDataByType<FPathedPhysicsMovementState>();
		if (!InputMoveState)
		{
			// If there isn't an existing FPathedPhysicsMovementState, this is the first time we're processing input and establishing the MoveState on the SyncState
			bIsFirstProcess = true;
			InputMoveState = &Input.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FPathedPhysicsMovementState>();
			InputMoveState->MutableProps = PathedMovementInputs->Props;
		}

		FMutablePathedMovementProperties& CurProps = InputMoveState->MutableProps;
		const FMutablePathedMovementProperties& InputProps = PathedMovementInputs->Props;

		Chaos::FPBDRigidsEvolution& Evolution = *GetPhysicsSolver()->GetEvolution();

		// Toggle whether the joint is enabled and update the controlled particle to be kinematic/dynamic accordingly
		if (bIsFirstProcess || InputProps.bIsJointEnabled != CurProps.bIsJointEnabled)
		{
			if (InputProps.bIsJointEnabled)
			{
				Evolution.EnableConstraints(&ParticleHandle);
				Evolution.SetParticleObjectState(&ParticleHandle, Chaos::EObjectStateType::Dynamic);

				// When the joint becomes newly enabled, teleport the endpoint to last frame's progress (since we don't touch it while it's deactivated)
				Chaos::FRigidTransform3 TargetLastFrame; 
				if (!GetPathedMoverComp().GetSimBlackboard()->TryGet(PathBlackboard::TargetRelativeTransform, TargetLastFrame))
				{
					if (PathedMode)
					{
						TargetLastFrame = PathedMode->CalcTargetRelativeTransform(InputMoveState->CurrentProgress);
					}
					else
					{
						// If there's no blackboard and there's no active mode, use the origin
						TargetLastFrame = InputProps.PathOrigin;
					}
				}
				
				Chaos::FKinematicGeometryParticleHandle* EndpointParticleHandle = GetJointConstraint()->GetKinematicEndPoint()->GetHandle_LowLevel()->CastToKinematicParticle();
				Evolution.SetParticleTransform(EndpointParticleHandle, TargetLastFrame.GetLocation(), TargetLastFrame.GetRotation(), true);
			}
			else
			{
				Evolution.DisableConstraints(&ParticleHandle);
				Evolution.SetParticleObjectState(&ParticleHandle, Chaos::EObjectStateType::Kinematic);
			}
		}
		
		// Has the path origin shifted?
		if (!bIsFirstProcess)
		{
			const FVector OriginLocationDelta = CurProps.PathOrigin.GetLocation() - InputProps.PathOrigin.GetLocation();
			const FQuat OriginRotationDelta = CurProps.PathOrigin.GetRotation() - InputProps.PathOrigin.GetRotation();
			if (!OriginLocationDelta.IsNearlyZero() || !FRotator(OriginRotationDelta).IsNearlyZero())
			{
				// Teleport both the controlled particle and the joint endpoint by the change in origin
				TeleportParticleBy_Internal(ParticleHandle, OriginLocationDelta, OriginRotationDelta);
				TeleportParticleBy_Internal(*JointConstraint.GetKinematicEndPoint()->GetHandle_LowLevel(), OriginLocationDelta, OriginRotationDelta);
			}
		}

		if (PathedMode)
		{
			// Let the mode process things at the end before we copy the input props over to the sync state 
			PathedMode->OnProcessInput_Internal(PhysicsStep, DeltaTime, Input);
		}
		
		CurProps = InputProps;
	}
	else if (PathedMode)
	{
		// No high-level inputs, but always let the mode have a look too
		PathedMode->OnProcessInput_Internal(PhysicsStep, DeltaTime, Input);
	}
}

bool UMoverPathedPhysicsLiaisonComponent::CanSimulate_Internal(const FPhysicsMoverSimulationTickParams& TickParams, const FPhysicsMoverAsyncInput& Input) const
{
	const FPathedPhysicsMovementState& InputMoveState = Input.SyncState.SyncStateCollection.FindOrAddDataByType<FPathedPhysicsMovementState>();
	if (!InputMoveState.MutableProps.IsMoving() && !InputMoveState.MutableProps.bIsJointEnabled)
	{
		// If we're not moving or using the joint, we're completely static and there's nothing to sim
		return false;
	}
	
	return Super::CanSimulate_Internal(TickParams, Input);
}

void UMoverPathedPhysicsLiaisonComponent::PerformPreSimulate_Internal(const FPhysicsMoverSimulationTickParams& TickParams, const FPhysicsMoverAsyncInput& Input, FPhysicsMoverAsyncOutput& Output) const
{
	Super::PerformPreSimulate_Internal(TickParams, Input, Output);
	
	const FPathedPhysicsMovementState& OutputMoveState = Output.SyncState.SyncStateCollection.FindOrAddDataByType<FPathedPhysicsMovementState>();

	// If a path target was specified (i.e. if movement is happening), set it as the kinematic target
	Chaos::FRigidTransform3 TargetRelativeTransform;
	if (GetPathedMoverComp().GetSimBlackboard()->TryGet(PathBlackboard::TargetRelativeTransform, TargetRelativeTransform))
	{
		Chaos::FPBDRigidsEvolution& Evolution = *GetPhysicsSolver()->GetEvolution();
		
		const Chaos::FRigidTransform3 TargetWorldTransform = Chaos::FRigidTransform3::MultiplyNoScale(TargetRelativeTransform, OutputMoveState.MutableProps.PathOrigin);
		Chaos::FKinematicGeometryParticleHandle* EndpointParticleHandle = GetJointConstraint()->GetKinematicEndPoint()->GetHandle_LowLevel()->CastToKinematicParticle();
		Evolution.SetParticleKinematicTarget(EndpointParticleHandle, Chaos::FKinematicTarget::MakePositionTarget(TargetWorldTransform));
		if (!OutputMoveState.MutableProps.bIsJointEnabled)
		{
			// When we don't want to use the joint, just move the pathed component kinematically
			Evolution.SetParticleKinematicTarget(GetControlledParticle_Internal(), Chaos::FKinematicTarget::MakePositionTarget(TargetWorldTransform));
		}
		// else
		// {
		// }
	}
}

Chaos::FJointConstraint* UMoverPathedPhysicsLiaisonComponent::GetJointConstraint() const
{
	return static_cast<Chaos::FJointConstraint*>(ConstraintHandle.Constraint);
}

void UMoverPathedPhysicsLiaisonComponent::CreateTargetJoint()
{
	const UMoverComponent& MoverComp = GetMoverComponent();
	if (IPhysicsComponent* PhysicsComponent = Cast<IPhysicsComponent>(MoverComp.GetUpdatedComponent()))
	{
		if (Chaos::FPhysicsObject* PhysicsObject = PhysicsComponent->GetPhysicsObjectByName(NAME_None))
		{
			const FTransform ComponentWorldTransform = MoverComp.GetUpdatedComponent()->GetComponentTransform();

			// Create the constraint via FChaosEngineInterface directly because it allows jointing a "real" object with a point in space (it creates a dummy particle for us)
			FPhysicsConstraintHandle Handle = FChaosEngineInterface::CreateConstraint(PhysicsObject, nullptr, FTransform::Identity, FTransform::Identity);

			bool bIsConstraintValid = false;
			if (Handle.IsValid() && ensure(Handle->IsType(Chaos::EConstraintType::JointConstraintType)))
			{
				if (Chaos::FJointConstraint* Constraint = static_cast<Chaos::FJointConstraint*>(Handle.Constraint))
				{
					// Since we didn't use the ConstraintInstance to actually create the constraint (it requires both bodies exist, see comment above), link everything up manually
					ConstraintHandle = Handle;
					ConstraintInstance.ConstraintHandle = ConstraintHandle;
					Constraint->SetUserData(&PhysicsUserData/*has a (void*)FConstraintInstanceBase*/);
					bIsConstraintValid = true;

					if (Chaos::FPBDRigidParticle* EndpointParticle = Constraint->GetPhysicsBodies()[1]->GetParticle<Chaos::EThreadContext::External>()->CastToRigidParticle())
					{
						EndpointParticle->SetX(ComponentWorldTransform.GetLocation());
						EndpointParticle->SetR(ComponentWorldTransform.GetRotation());
					}
				}
			}

			if (!bIsConstraintValid)
			{
				FChaosEngineInterface::ReleaseConstraint(Handle);
			}
		
		}
	}
}

void UMoverPathedPhysicsLiaisonComponent::DestroyTargetJoint()
{
	FChaosEngineInterface::ReleaseConstraint(ConstraintHandle);
	ConstraintInstance.ConstraintHandle.Reset();
}

void UMoverPathedPhysicsLiaisonComponent::HandleMovementModeChanged(const FName& OldModeName, const FName& NewModeName)
{
	if (const UPathedPhysicsMovementMode* OldMode = GetMoverComponent().FindMode_Mutable<UPathedPhysicsMovementMode>(OldModeName))
	{
		OldMode->OnIsUsingJointChanged().RemoveAll(this);
	}
	
	if (const UPathedPhysicsMovementMode* NewMode = GetMoverComponent().FindMode_Mutable<UPathedPhysicsMovementMode>(NewModeName))
	{
		ApplyPathModeConfig(*NewMode);
	}
}

void UMoverPathedPhysicsLiaisonComponent::ApplyPathModeConfig(const UPathedPhysicsMovementMode& PathedMode)
{
	PathedMode.OnIsUsingJointChanged().AddUObject(this, &ThisClass::HandleIsUsingJointChanged);
	HandleIsUsingJointChanged(PathedMode.IsUsingJoint());

	const EPathedPhysicsPlaybackBehavior DefaultPlaybackBehavior = GetPathedMoverComp().GetDefaultPlaybackBehavior();
	SetPlaybackBehavior(PathedMode.GetPlaybackBehaviorOverride().Get(DefaultPlaybackBehavior));
	
	if (HasValidPhysicsState())
	{
		ConstraintInstance.CopyProfilePropertiesFrom(PathedMode.GetConstraintProperties());
	}
}

void UMoverPathedPhysicsLiaisonComponent::HandleIsUsingJointChanged(bool bIsUsingJoint)
{
	// When using the joint, we need to replicate the actual transform of the controlled component's particle to determine if a resim is needed
	//	This is easier, cheaper, and accounts for relevancy if we do so via the actor's built-in bReplicateMovement functionality
	// When not using the joint, however, the component's kinematic target is set each frame based entirely on FPathedPhysicsMovementState::CurrentProgress
	//	Therefore, we can simply compare the calculated progress between server and client to determine if a resim is needed, and don't need to send any transform data
	Inputs_External.bIsJointEnabled = bIsUsingJoint;
	if (AActor* MyActor = GetOwner())
	{
		MyActor->SetReplicatingMovement(bIsUsingJoint);
	}
	
	if (NetworkPhysicsComponent && !NetworkPhysicsComponent->HasServerWorld())
	{
		NetworkPhysicsComponent->SetCompareStateToTriggerRewind(!bIsUsingJoint);
	}
}

#undef LOCTEXT_NAMESPACE
