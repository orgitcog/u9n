// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosMover/Backends/ChaosMoverBackend.h"

#include "Backends/ChaosMoverSubsystem.h"
#include "ChaosMover/ChaosMoverLog.h"
#include "ChaosMover/ChaosMoverSimulation.h"
#include "Components/PrimitiveComponent.h"
#include "Framework/Threading.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PhysicsVolume.h"
#include "MoveLibrary/MovementMixer.h"
#include "MoveLibrary/MoverBlackboard.h"
#include "MovementModeStateMachine.h"
#include "NetworkChaosMoverData.h"
#include "PBDRigidsSolver.h"
#include "Physics/NetworkPhysicsComponent.h"
#include "PhysicsEngine/PhysicsObjectExternalInterface.h"
#include "PhysicsProxy/CharacterGroundConstraintProxy.h"
#include "PhysicsProxy/JointConstraintProxy.h"
#include "Runtime/Experimental/Chaos/Private/Chaos/PhysicsObjectInternal.h"


#include UE_INLINE_GENERATED_CPP_BY_NAME(ChaosMoverBackend)

UChaosMoverBackendComponent::UChaosMoverBackendComponent()
	: PathTargetConstraintPhysicsUserData(&PathTargetConstraintInstance)
{
	PrimaryComponentTick.bCanEverTick = false;

	bWantsInitializeComponent = true;
	bAutoActivate = true;

	if (const Chaos::FPhysicsSolver* Solver = GetPhysicsSolver())
	{
		bIsUsingAsyncPhysics = Solver->IsUsingAsyncResults();
	}

	if (Chaos::FPhysicsSolverBase::IsNetworkPhysicsPredictionEnabled())
	{
		SetIsReplicatedByDefault(true);

		// Let's make sure PhysicsReplicationMode is set to Resimulation and that movement is set to replicate
		UWorld* World = GetWorld();
		AActor* MyActor = GetOwner();
		if (MyActor && World && World->IsGameWorld() && (World->GetNetMode() != ENetMode::NM_Standalone))
		{
			if (MyActor->GetPhysicsReplicationMode() != EPhysicsReplicationMode::Resimulation)
			{
				MyActor->SetPhysicsReplicationMode(EPhysicsReplicationMode::Resimulation);
				UE_LOG(LogChaosMover, Log, TEXT("ChaosMoverBackend: Setting Physics Replication Mode to Resimulation for %s or movement will not replicate correctly"), *GetNameSafe(MyActor));
			}
			if (!MyActor->IsReplicatingMovement())
			{
				MyActor->SetReplicateMovement(true);
				UE_LOG(LogChaosMover, Log, TEXT("ChaosMoverBackend: Turning ON Replicate Movement for %s or movement will not replicate correctly"), *GetNameSafe(MyActor));
			}
		}
	}

	Simulation = CreateDefaultSubobject<UChaosMoverSimulation>(TEXT("ChaosMoverSimulation"));
}

void UChaosMoverBackendComponent::InitializeComponent()
{
	Super::InitializeComponent();

	if (UWorld* World = GetWorld(); World && World->IsGameWorld())
	{
		NullMovementMode = NewObject<UNullMovementMode>(&GetMoverComponent(), TEXT("NullMovementMode"));
		ImmediateModeTransition = NewObject<UImmediateMovementModeTransition>(&GetMoverComponent(), TEXT("ImmediateModeTransition"));

		// Create NetworkPhysicsComponent
		if ((World->GetNetMode() != ENetMode::NM_Standalone) && Chaos::FPhysicsSolverBase::IsNetworkPhysicsPredictionEnabled())
		{
			if (!bIsUsingAsyncPhysics)
			{
				// Verify that the Project Settings have bTickPhysicsAsync turned on.
				// It's easy to waste time forgetting that, since they are off by default.
				UE_LOG(LogChaosMover, Warning, TEXT("Chaos Mover Backend only supports networking with Physics Async. Networked Physics will not work well. Turn on 'Project Settings > Engine - Physics > Tick Physics Async', or play in Standalone Mode"));
				// This is important enough that we break for developers debugging in editor
				UE_DEBUG_BREAK();
			}
			else
			{
				NetworkPhysicsComponent = NewObject<UNetworkPhysicsComponent>(GetOwner(), TEXT("PhysMover_NetworkPhysicsComponent"));

				// This isn't technically a DSO component, but set it net addressable as though it is
				NetworkPhysicsComponent->SetNetAddressable();
				NetworkPhysicsComponent->SetIsReplicated(true);
				NetworkPhysicsComponent->RegisterComponent();
				if (!NetworkPhysicsComponent->HasBeenInitialized())
				{
					NetworkPhysicsComponent->InitializeComponent();
				}
				NetworkPhysicsComponent->Activate(true);

				// Register network data for recording and rewind/resim
				NetworkPhysicsComponent->CreateDataHistory<UE::ChaosMover::FNetworkDataTraits>(this);

				if (NetworkPhysicsComponent->HasServerWorld())
				{
					if (APawn* PawnOwner = Cast<APawn>(GetOwner()))
					{
						// When we're owned by a pawn, keep an eye on whether it's currently player-controlled or not
						PawnOwner->ReceiveControllerChangedDelegate.AddUniqueDynamic(this, &ThisClass::HandleOwningPawnControllerChanged_Server);
						HandleOwningPawnControllerChanged_Server(PawnOwner, nullptr, PawnOwner->Controller);
					}
					else
					{
						// If the owner isn't a pawn, there's no chance of player input happening, so inputs to the PT are always produced on the server
						NetworkPhysicsComponent->SetIsRelayingLocalInputs(true);
					}
				}
			}
		}
	}
}

void UChaosMoverBackendComponent::UninitializeComponent()
{
	if (NetworkPhysicsComponent)
	{
		NetworkPhysicsComponent->RemoveDataHistory();
		NetworkPhysicsComponent->DestroyComponent();
		NetworkPhysicsComponent = nullptr;
	}

	Super::UninitializeComponent();
}

void UChaosMoverBackendComponent::CreatePhysics()
{
	// Prevent the character particle from sleeping
	Chaos::FPhysicsSolver* Solver = GetPhysicsSolver();
	Chaos::FPBDRigidParticle* ControlledParticle = GetControlledParticle();
	Chaos::FSingleParticlePhysicsProxy* ControlledParticleProxy = ControlledParticle ? static_cast<Chaos::FSingleParticlePhysicsProxy*>(ControlledParticle->GetProxy()) : nullptr;
	if (ControlledParticleProxy)
	{
		ControlledParticle->SetSleepType(Chaos::ESleepType::NeverSleep);
	}

	// Create all possible constraints...
	// ... a character ground constraint, for constraint based character-like movement on ground
	CreateCharacterGroundConstraint();
	// ... a path target constraint, for constraint based pathed movement
	CreatePathTargetConstraint();
}

void UChaosMoverBackendComponent::DestroyPhysics()
{
	// Destroy all constraints
	DestroyCharacterGroundConstraint();
	DestroyPathTargetConstraint();
}

void UChaosMoverBackendComponent::CreateCharacterGroundConstraint()
{
	if (Chaos::FPhysicsSolver* Solver = GetPhysicsSolver())
	{
		if (Chaos::FPBDRigidParticle* ControlledParticle = GetControlledParticle())
		{
			if (Chaos::FSingleParticlePhysicsProxy* ControlledParticleProxy = static_cast<Chaos::FSingleParticlePhysicsProxy*>(ControlledParticle->GetProxy()))
			{
				// Create the character ground constraint, for character-like movement on ground
				CharacterGroundConstraint = MakeUnique<Chaos::FCharacterGroundConstraint>();
				CharacterGroundConstraint->Init(ControlledParticleProxy);
				Solver->RegisterObject(CharacterGroundConstraint.Get());
			}
		}
	}
}

void UChaosMoverBackendComponent::DestroyCharacterGroundConstraint()
{
	if (Chaos::FPhysicsSolver* Solver = GetPhysicsSolver())
	{
		if (CharacterGroundConstraint.IsValid())
		{
			// Note: Proxy gets destroyed when the constraint is deregistered and that deletes the constraint
			Solver->UnregisterObject(CharacterGroundConstraint.Release());
		}
	}
}

void UChaosMoverBackendComponent::CreatePathTargetConstraint()
{
	const UMoverComponent& MoverComp = GetMoverComponent();
	if (Chaos::FPhysicsObject* PhysicsObject = GetPhysicsObject())
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
				PathTargetConstraintHandle = Handle;						
				PathTargetConstraintInstance.ConstraintHandle = PathTargetConstraintHandle;
				Constraint->SetUserData(&PathTargetConstraintPhysicsUserData/*has a (void*)FConstraintInstanceBase*/);
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

void UChaosMoverBackendComponent::DestroyPathTargetConstraint()
{
	if (PathTargetConstraintHandle.IsValid())
	{
		FChaosEngineInterface::ReleaseConstraint(PathTargetConstraintHandle);
	}
}

void UChaosMoverBackendComponent::HandleOwningPawnControllerChanged_Server(APawn* OwnerPawn, AController* OldController, AController* NewController)
{
	// Inputs for player-controlled pawns originate on the player's client, all others originate on the server
	if (NetworkPhysicsComponent)
	{
		NetworkPhysicsComponent->SetIsRelayingLocalInputs(!OwnerPawn->IsPlayerControlled());
	}
}

void UChaosMoverBackendComponent::HandleUpdatedComponentPhysicsStateChanged(UPrimitiveComponent* ChangedComponent, EComponentPhysicsStateChange StateChange)
{
	if (StateChange == EComponentPhysicsStateChange::Destroyed)
	{
		bWantsDestroySim = true;
	}
	else if (StateChange == EComponentPhysicsStateChange::Created)
	{
		bWantsCreateSim = true;
	}
}

Chaos::FPhysicsSolver* UChaosMoverBackendComponent::GetPhysicsSolver() const
{
	if (UWorld* World = GetWorld())
	{
		if (FPhysScene* Scene = World->GetPhysicsScene())
		{
			return Scene->GetSolver();
		}
	}
	return nullptr;
}

UMoverComponent& UChaosMoverBackendComponent::GetMoverComponent() const
{
	return *GetOuterUMoverComponent();
}

Chaos::FPhysicsObject* UChaosMoverBackendComponent::GetPhysicsObject() const
{
	IPhysicsComponent* PhysicsComponent = Cast<IPhysicsComponent>(GetMoverComponent().GetUpdatedComponent());
	return PhysicsComponent ? PhysicsComponent->GetPhysicsObjectByName(NAME_None) : nullptr;
}

Chaos::FPBDRigidParticle* UChaosMoverBackendComponent::GetControlledParticle() const
{
	if (Chaos::FPhysicsObject* PhysicsObject = GetPhysicsObject())
	{
		return FPhysicsObjectExternalInterface::LockRead(PhysicsObject)->GetRigidParticle(PhysicsObject);
	}

	return nullptr;
}

void UChaosMoverBackendComponent::InitSimulation()
{
	UMoverComponent& MoverComp = GetMoverComponent();

	UChaosMoverSimulation::FInitParams Params;
	for (const TPair<FName, TObjectPtr<UBaseMovementMode>>& Pair : MoverComp.MovementModes)
	{
		Params.ModesToRegister.Add(Pair.Key, TWeakObjectPtr<UBaseMovementMode>(Pair.Value.Get()));
	}
	for (const TObjectPtr<UBaseMovementModeTransition>& Transition : MoverComp.Transitions)
	{
		Params.TransitionsToRegister.Add(TWeakObjectPtr<UBaseMovementModeTransition>(Transition.Get()));
	}
	Params.MovementMixer = TWeakObjectPtr<UMovementMixer>(MoverComp.MovementMixer.Get());
	Params.ImmediateModeTransition = TWeakObjectPtr<UImmediateMovementModeTransition>(ImmediateModeTransition.Get());
	Params.NullMovementMode = TWeakObjectPtr<UNullMovementMode>(NullMovementMode.Get());
	Params.StartingMovementMode = MoverComp.StartingMovementMode;
	Params.CharacterConstraintProxy = CharacterGroundConstraint ? CharacterGroundConstraint->GetProxy<Chaos::FCharacterGroundConstraintProxy>() : nullptr;
	Params.PathTargetConstraintProxy = PathTargetConstraintHandle.IsValid() ? PathTargetConstraintHandle->GetProxy<Chaos::FJointConstraintPhysicsProxy>() : nullptr;
	Params.PathTargetKinematicEndPointProxy = Params.PathTargetConstraintProxy && Params.PathTargetConstraintProxy->GetConstraint() ? Params.PathTargetConstraintProxy->GetConstraint()->GetKinematicEndPoint() : nullptr;
	Params.PhysicsObject = GetPhysicsObject();
	Params.Solver = GetPhysicsSolver();
	Params.World = GetWorld();
	
	SimOutputRecord.Clear();

	UE::ChaosMover::FSimulationOutputData OutputData;
	FMoverAuxStateContext UnusedAuxState;
	MoverComp.InitializeSimulationState(&OutputData.SyncState, &UnusedAuxState);

	FMoverTimeStep TimeStep;
	if (Chaos::FPhysicsSolver* Solver = GetPhysicsSolver())
	{
		TimeStep.BaseSimTimeMs = Solver->GetPhysicsResultsTime_External() * 1000.0f;
		TimeStep.ServerFrame = Solver->GetCurrentFrame();
		TimeStep.StepMs = Solver->GetAsyncDeltaTime() * 1000.0f;
	}

	SimOutputRecord.Add(TimeStep, OutputData);

	Params.InitialSyncState = OutputData.SyncState;

	Simulation->Init(Params);
}

void UChaosMoverBackendComponent::DeinitSimulation()
{
	Simulation->Deinit();
}

void UChaosMoverBackendComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UWorld* World = GetWorld(); World && World->IsGameWorld())
	{
		CreatePhysics();
		InitSimulation();

		// Register with the world subsystem
		if (UChaosMoverSubsystem* ChaosMoverSubsystem = UWorld::GetSubsystem<UChaosMoverSubsystem>(GetWorld()))
		{
			ChaosMoverSubsystem->Register(this);
		}

		// Register a callback to watch for component state changes
		if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(GetMoverComponent().GetUpdatedComponent()))
		{
			PrimComp->OnComponentPhysicsStateChanged.AddUniqueDynamic(this, &ThisClass::HandleUpdatedComponentPhysicsStateChanged);
		}
	}
}

void UChaosMoverBackendComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DeinitSimulation();
	DestroyPhysics();

	if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(GetMoverComponent().GetUpdatedComponent()))
	{
		PrimComp->OnComponentPhysicsStateChanged.RemoveDynamic(this, &ThisClass::HandleUpdatedComponentPhysicsStateChanged);
	}

	if (UChaosMoverSubsystem* ChaosMoverSubsystem = UWorld::GetSubsystem<UChaosMoverSubsystem>(GetWorld()))
	{
		ChaosMoverSubsystem->Unregister(this);
	}

	Super::EndPlay(EndPlayReason);
}

void UChaosMoverBackendComponent::ProduceInputData(int32 PhysicsStep, int32 NumSteps, const FMoverTimeStep& TimeStep, UE::ChaosMover::FSimulationInputData& InputData)
{
	Chaos::EnsureIsInGameThreadContext();

	// Recreate the simulation if necessary
	if (bWantsDestroySim)
	{
		DeinitSimulation();
		DestroyPhysics();
		bWantsDestroySim = false;
		return;
	}
	if (bWantsCreateSim)
	{
		CreatePhysics();
		InitSimulation();
		bWantsCreateSim = false;
	}

	if (!NetworkPhysicsComponent || NetworkPhysicsComponent->IsLocallyControlled())
	{
		GenerateInput(TimeStep, InputData);
	}

	// Cache the produced input on the simulation so that it can be written to the network data
	// This happens before the async input is received
	Simulation->InitNetInputData(InputData.InputCmd);

	UMoverComponent& MoverComp = GetMoverComponent();

	// Add default simulation input data
	FChaosMoverSimulationDefaultInputs& SimInputs = Simulation->GetLocalSimInput_Mutable().FindOrAddMutableDataByType<FChaosMoverSimulationDefaultInputs>();
	SimInputs.Gravity = MoverComp.GetGravityAcceleration();
	SimInputs.UpDir = MoverComp.GetUpDirection();
	SimInputs.OwningActor = GetOwner();
	SimInputs.World = GetWorld();

	if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(MoverComp.GetUpdatedComponent()))
	{
		SimInputs.CollisionQueryParams = FCollisionQueryParams(SCENE_QUERY_STAT(ChaosMoverQuery), false, PrimComp->GetOwner());
		SimInputs.CollisionQueryParams.bTraceIntoSubComponents = false;
		SimInputs.CollisionResponseParams = FCollisionResponseParams(ECR_Overlap);
		SimInputs.CollisionResponseParams.CollisionResponse.SetResponse(ECC_WorldStatic, ECR_Block);
		SimInputs.CollisionResponseParams.CollisionResponse.SetResponse(ECC_WorldDynamic, ECR_Block);
		SimInputs.CollisionResponseParams.CollisionResponse.SetResponse(ECC_Vehicle, ECR_Block);
		SimInputs.CollisionResponseParams.CollisionResponse.SetResponse(ECC_Destructible, ECR_Block);
		SimInputs.CollisionResponseParams.CollisionResponse.SetResponse(ECC_PhysicsBody, ECR_Block);
		PrimComp->InitSweepCollisionParams(SimInputs.CollisionQueryParams, SimInputs.CollisionResponseParams);

		SimInputs.CollisionChannel = PrimComp->GetCollisionObjectType();
		PrimComp->CalcBoundingCylinder(SimInputs.PawnCollisionRadius, SimInputs.PawnCollisionHalfHeight);
	}
	if (IPhysicsComponent* PhysComp = Cast<IPhysicsComponent>(MoverComp.GetUpdatedComponent()))
	{
		SimInputs.PhysicsObject = PhysComp->GetPhysicsObjectById(0); // Get the root physics object
	}
	if (const APhysicsVolume* CurPhysVolume = MoverComp.GetUpdatedComponent()->GetPhysicsVolume())
	{
		SimInputs.PhysicsObjectGravity = CurPhysVolume->GetGravityZ();
	}

	if (MoverComp.OnPreSimulationTick.IsBound())
	{
		MoverComp.OnPreSimulationTick.Broadcast(TimeStep, InputData.InputCmd);
	}
}

void UChaosMoverBackendComponent::GenerateInput(const FMoverTimeStep& TimeStep, UE::ChaosMover::FSimulationInputData& InputData)
{
	UMoverComponent& MoverComp = GetMoverComponent();
	MoverComp.ProduceInput(TimeStep.StepMs, &InputData.InputCmd);
}

void UChaosMoverBackendComponent::ConsumeOutputData(const FMoverTimeStep& TimeStep, const UE::ChaosMover::FSimulationOutputData& OutputData)
{
	Chaos::EnsureIsInGameThreadContext();

	SimOutputRecord.Add(TimeStep, OutputData);
}

void UChaosMoverBackendComponent::FinalizeFrame(float ResultsTimeInMs)
{
	Chaos::EnsureIsInGameThreadContext();

	UMoverComponent& MoverComp = GetMoverComponent();

	FMoverTimeStep TimeStep;
	UE::ChaosMover::FSimulationOutputData InterpolatedOutput;
	SimOutputRecord.GetInterpolated(ResultsTimeInMs, TimeStep, InterpolatedOutput);

	// Physics interactions in the last frame may have caused a change in position or velocity that's different from what a simple lerp would predict,
	// so stomp the lerped sync state's transform data with that of the actual particle after the last sim frame
	FMoverDefaultSyncState& TransformSyncState = InterpolatedOutput.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();
	if (Chaos::FPBDRigidParticle* Particle = GetControlledParticle())
	{
		TransformSyncState.SetTransforms_WorldSpace(Particle->GetX(), FRotator(Particle->GetR()), Particle->GetV(), TransformSyncState.GetMovementBase(), TransformSyncState.GetMovementBaseBoneName());

		// Make sure the move direction intent is in base space (the base quat is identity if there's no base, effectively making this a no-op)
		TransformSyncState.MoveDirectionIntent = TransformSyncState.GetCapturedMovementBaseQuat().UnrotateVector(TransformSyncState.MoveDirectionIntent);
	}

	MoverComp.SetSimulationOutput(TimeStep, InterpolatedOutput);
	
	if (MoverComp.OnPostSimulationTick.IsBound())
	{
		MoverComp.OnPostSimulationTick.Broadcast(TimeStep);
	}
}
