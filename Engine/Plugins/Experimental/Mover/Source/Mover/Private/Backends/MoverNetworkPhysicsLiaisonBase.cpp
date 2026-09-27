// Copyright Epic Games, Inc. All Rights Reserved.

#include "Backends/MoverNetworkPhysicsLiaisonBase.h"

#include "MovementModeStateMachine.h"
#include "PBDRigidsSolver.h"
#include "Chaos/PhysicsObjectInterface.h"
#include "Chaos/PhysicsObjectInternalInterface.h"
#include "Components/ShapeComponent.h"
#include "Framework/Threading.h"
#include "GameFramework/Pawn.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "PhysicsEngine/PhysicsObjectExternalInterface.h"
#include "PhysicsMover/PhysicsMoverManager.h"
#include "PhysicsMover/Modes/PhysicsDrivenFallingMode.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(MoverNetworkPhysicsLiaisonBase)

#define LOCTEXT_NAMESPACE "Mover"

//////////////////////////////////////////////////////////////////////////

extern FPhysicsDrivenMotionDebugParams GPhysicsDrivenMotionDebugParams;

//////////////////////////////////////////////////////////////////////////
// FNetworkPhysicsMoverInputs

void FNetworkPhysicsMoverInputs::ApplyData(UActorComponent* NetworkComponent) const
{
	if (UMoverNetworkPhysicsLiaisonComponentBase* LiaisonComp = Cast<UMoverNetworkPhysicsLiaisonComponentBase>(NetworkComponent))
	{
		LiaisonComp->SetCurrentInputData(InputCmdContext);
	}
}

void FNetworkPhysicsMoverInputs::BuildData(const UActorComponent* NetworkComponent)
{
	if (const UMoverNetworkPhysicsLiaisonComponentBase* LiaisonComp = Cast<const UMoverNetworkPhysicsLiaisonComponentBase>(NetworkComponent))
	{
		LiaisonComp->GetCurrentInputData(InputCmdContext);
	}
}

void FNetworkPhysicsMoverInputs::DecayData(float DecayAmount)
{
	InputCmdContext.InputCollection.Decay(DecayAmount);
}

bool FNetworkPhysicsMoverInputs::NetSerialize(FArchive& Ar, class UPackageMap* PackageMap, bool& bOutSuccess)
{
	SerializeFrames(Ar);
	if (PackageMap)
	{
		InputCmdContext.NetSerialize(FNetSerializeParams(Ar));
		bOutSuccess = true;
	}
	else
	{
		bOutSuccess = false;
	}

	return bOutSuccess;
}

void FNetworkPhysicsMoverInputs::InterpolateData(const FNetworkPhysicsData& MinData, const FNetworkPhysicsData& MaxData)
{
	const FNetworkPhysicsMoverInputs& MinDataInput = static_cast<const FNetworkPhysicsMoverInputs&>(MinData);
	const FNetworkPhysicsMoverInputs& MaxDataInput = static_cast<const FNetworkPhysicsMoverInputs&>(MaxData);

	const float LerpFactor = (LocalFrame - MinDataInput.LocalFrame) / (MaxDataInput.LocalFrame - MinDataInput.LocalFrame);
	InputCmdContext.InputCollection.Interpolate(MinDataInput.InputCmdContext.InputCollection, MaxDataInput.InputCmdContext.InputCollection, LerpFactor);
}

void FNetworkPhysicsMoverInputs::MergeData(const FNetworkPhysicsData& FromData)
{
	const FNetworkPhysicsMoverInputs& TypedFrom = static_cast<const FNetworkPhysicsMoverInputs&>(FromData);
	InputCmdContext.InputCollection.Merge(TypedFrom.InputCmdContext.InputCollection);
}

void FNetworkPhysicsMoverInputs::ValidateData(const UActorComponent* NetworkComponent)
{
	if (NetworkComponent)
	{
		if (const UMoverNetworkPhysicsLiaisonComponentBase* LiaisonComp = Cast<UMoverNetworkPhysicsLiaisonComponentBase>(NetworkComponent))
		{
			LiaisonComp->ValidateInputData(InputCmdContext);
		}
	}
}

bool FNetworkPhysicsMoverInputs::CompareData(const FNetworkPhysicsData& PredictedData)
{
	const FMoverInputCmdContext& PredictedInputCmd = static_cast<const FNetworkPhysicsMoverInputs&>(PredictedData).InputCmdContext;
	return !PredictedInputCmd.InputCollection.ShouldReconcile(InputCmdContext.InputCollection);
}

const FString FNetworkPhysicsMoverInputs::DebugData()
{
	FAnsiStringBuilderBase StringBuilder;
	InputCmdContext.ToString(StringBuilder);
	return FString::Printf(TEXT("FNetworkPhysicsMoverInputs:\n%hs"), StringBuilder.ToString());
}

//////////////////////////////////////////////////////////////////////////
// FNetworkPhysicsMoverState

void FNetworkPhysicsMoverState::ApplyData(UActorComponent* NetworkComponent) const
{
	if (NetworkComponent)
	{
		if (UMoverNetworkPhysicsLiaisonComponentBase* LiaisonComp = Cast<UMoverNetworkPhysicsLiaisonComponentBase>(NetworkComponent))
		{
			LiaisonComp->SetCurrentStateData(SyncStateContext);
		}
	}
}

void FNetworkPhysicsMoverState::BuildData(const UActorComponent* NetworkComponent)
{
	if (NetworkComponent)
	{
		if (const UMoverNetworkPhysicsLiaisonComponentBase* LiaisonComp = Cast<const UMoverNetworkPhysicsLiaisonComponentBase>(NetworkComponent))
		{
			LiaisonComp->GetCurrentStateData(SyncStateContext);
		}
	}
}

bool FNetworkPhysicsMoverState::NetSerialize(FArchive& Ar, class UPackageMap* PackageMap, bool& bOutSuccess)
{
	SerializeFrames(Ar);
	if (PackageMap)
	{
		FNetSerializeParams Params(Ar);
		SyncStateContext.NetSerialize(Params);
		bOutSuccess = true;
	}
	else
	{
		bOutSuccess = false;
	}

	return bOutSuccess;
}

void FNetworkPhysicsMoverState::InterpolateData(const FNetworkPhysicsData& MinData, const FNetworkPhysicsData& MaxData)
{
	const FNetworkPhysicsMoverState& MinState = static_cast<const FNetworkPhysicsMoverState&>(MinData);
	const FNetworkPhysicsMoverState& MaxState = static_cast<const FNetworkPhysicsMoverState&>(MaxData);

	const float LerpFactor = (LocalFrame - MinState.LocalFrame) / (MaxState.LocalFrame - MinState.LocalFrame);
	SyncStateContext.Interpolate(&MinState.SyncStateContext, &MaxState.SyncStateContext, LerpFactor);
}

bool FNetworkPhysicsMoverState::CompareData(const FNetworkPhysicsData& PredictedData)
{
	const FMoverSyncState& PredictedSyncState = static_cast<const FNetworkPhysicsMoverState&>(PredictedData).SyncStateContext;
	return !PredictedSyncState.ShouldReconcile(SyncStateContext);
}

const FString FNetworkPhysicsMoverState::DebugData()
{
	FAnsiStringBuilderBase StringBuilder;
	SyncStateContext.ToString(StringBuilder);
	return FString::Printf(TEXT("FNetworkPhysicsMoverState:\n%hs"), StringBuilder.ToString());
}

//////////////////////////////////////////////////////////////////////////
// UMoverNetworkPhysicsLiaisonComponentBase

void UMoverNetworkPhysicsLiaisonComponentBase::GetCurrentInputData(OUT FMoverInputCmdContext& InputCmd) const
{
	InputCmd = LatestInputCmd;
}

void UMoverNetworkPhysicsLiaisonComponentBase::GetCurrentStateData(OUT FMoverSyncState& SyncState) const
{
	SyncState = LatestSyncState;
}

void UMoverNetworkPhysicsLiaisonComponentBase::SetCurrentInputData(const FMoverInputCmdContext& InputCmd)
{
	LatestInputCmd = InputCmd;
}

void UMoverNetworkPhysicsLiaisonComponentBase::SetCurrentStateData(const FMoverSyncState& SyncState)
{
	LatestSyncState = SyncState;
}

bool UMoverNetworkPhysicsLiaisonComponentBase::ValidateInputData(FMoverInputCmdContext& InputCmd) const
{
	// TODO - proper data validation
	return true;
}

UMoverNetworkPhysicsLiaisonComponentBase::UMoverNetworkPhysicsLiaisonComponentBase()
{
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	PrimaryComponentTick.bCanEverTick = true;

	bWantsInitializeComponent = true;
	bAutoActivate = true;

	if (const Chaos::FPhysicsSolver* Solver = GetPhysicsSolver())
	{
		bUsingAsyncPhysics = Solver->IsUsingAsyncResults();
	}

	if (Chaos::FPhysicsSolverBase::IsNetworkPhysicsPredictionEnabled())
	{
		SetIsReplicatedByDefault(true);
	}
}

//////////////////////////////////////////////////////////////////////////
//  UMoverNetworkPhysicsLiaisonComponentBase IMoverBackendLiaisonInterface

float UMoverNetworkPhysicsLiaisonComponentBase::GetCurrentSimTimeMs()
{
	// Note: this is implicitly an _External function
	if (const Chaos::FPhysicsSolver* Solver = GetPhysicsSolver())
	{
		return bUsingAsyncPhysics ? Solver->GetAsyncDeltaTime() * GetCurrentSimFrame() * 1000.0f : Solver->GetSolverTime() * 1000.0f;
	}
	return 0.0f;
}

int32 UMoverNetworkPhysicsLiaisonComponentBase::GetCurrentSimFrame()
{
	// Note: this is implicitly an _External function
	if (Chaos::FPhysicsSolver* Solver = GetPhysicsSolver())
	{
		return Solver->GetCurrentFrame() + GetNetworkPhysicsTickOffset_External();
	}
	return 0;
}

#if WITH_EDITOR
EDataValidationResult UMoverNetworkPhysicsLiaisonComponentBase::ValidateData(FDataValidationContext& Context, const UMoverComponent& ValidationMoverComp) const
{
	return EDataValidationResult::Valid;
}
#endif // WITH_EDITOR

//////////////////////////////////////////////////////////////////////////
// UMoverNetworkPhysicsLiaisonComponentBase UObject interface

void UMoverNetworkPhysicsLiaisonComponentBase::OnRegister()
{
	Super::OnRegister();

	if (UMoverComponent& MoverComp = GetMoverComponent(); MoverComp.UpdatedCompAsPrimitive)
	{
		MoverComp.UpdatedCompAsPrimitive->OnComponentPhysicsStateChanged.AddUniqueDynamic(this, &ThisClass::HandleComponentPhysicsStateChanged);
	}
}

void UMoverNetworkPhysicsLiaisonComponentBase::OnUnregister()
{
	if (UMoverComponent& MoverComp = GetMoverComponent(); MoverComp.UpdatedCompAsPrimitive)
	{
		MoverComp.UpdatedCompAsPrimitive->OnComponentPhysicsStateChanged.RemoveDynamic(this, &ThisClass::HandleComponentPhysicsStateChanged);
	}

	Super::OnUnregister();
}

void UMoverNetworkPhysicsLiaisonComponentBase::HandleComponentPhysicsStateChanged(UPrimitiveComponent* ChangedComponent, EComponentPhysicsStateChange StateChange)
{
	if (StateChange == EComponentPhysicsStateChange::Created)
	{
		if (UMoverComponent& MoverComp = GetMoverComponent(); MoverComp.ModeFSM)
		{
			MoverComp.ModeFSM->SetModeImmediately(MoverComp.StartingMovementMode);
		}

		InitializeSimOutputData();
	}
}

void UMoverNetworkPhysicsLiaisonComponentBase::HandleOwningPawnControllerChanged_Server(APawn* OwnerPawn, AController* OldController, AController* NewController)
{
	// Inputs for player-controlled pawns originate on the player's client, all others originate on the server
	NetworkPhysicsComponent->SetIsRelayingLocalInputs(!OwnerPawn->IsPlayerControlled());
}

Chaos::FPhysicsSolver* UMoverNetworkPhysicsLiaisonComponentBase::GetPhysicsSolver() const
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

void UMoverNetworkPhysicsLiaisonComponentBase::InitializeComponent()
{
	Super::InitializeComponent();

	if (UWorld* World = GetWorld(); World && World->IsGameWorld())
	{
		UMoverComponent& MoverComp = GetMoverComponent();
	}

	if (Chaos::FPhysicsSolverBase::IsNetworkPhysicsPredictionEnabled() && bUsingAsyncPhysics)
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
		NetworkPhysicsComponent->CreateDataHistory<FNetworkPhysicsMoverTraits>(this);

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

void UMoverNetworkPhysicsLiaisonComponentBase::UninitializeComponent()
{
	if (NetworkPhysicsComponent)
	{
		NetworkPhysicsComponent->RemoveDataHistory();
		NetworkPhysicsComponent->DestroyComponent();
		NetworkPhysicsComponent = nullptr;
	}

	Super::UninitializeComponent();
}

bool UMoverNetworkPhysicsLiaisonComponentBase::ShouldCreatePhysicsState() const
{
	if (!IsRegistered() || IsBeingDestroyed())
	{
		return false;
	}

	UWorld* World = GetWorld();
	return World && World->IsGameWorld() && World->GetPhysicsScene() && CanCreatePhysics();
}

bool UMoverNetworkPhysicsLiaisonComponentBase::HasValidState() const
{
	const UMoverComponent& MoverComp = GetMoverComponent();
	return HasValidPhysicsState() && GetControlledPhysicsObject() && GetPhysicsSolver() && MoverComp.UpdatedCompAsPrimitive && MoverComp.UpdatedComponent
		&& MoverComp.ModeFSM->IsValidLowLevelFast() && MoverComp.GetSimBlackboard()->IsValidLowLevelFast() && MoverComp.MovementMixer;
}

bool UMoverNetworkPhysicsLiaisonComponentBase::CanCreatePhysics() const
{
	check(GetOwner());
	FString ActorName = GetOwner()->GetName();

	const UMoverComponent& MoverComp = GetMoverComponent();
	if (!IsValid(MoverComp.UpdatedComponent))
	{
		UE_LOG(LogMover, Warning, TEXT("Can't create physics %s (%s). UpdatedComponent is not set."), *ActorName, *GetPathName());
		return false;
	}

	if (!IsValid(MoverComp.UpdatedCompAsPrimitive))
	{
		UE_LOG(LogMover, Warning, TEXT("Can't create physics %s (%s). UpdatedComponent is not a PrimitiveComponent."), *ActorName, *GetPathName());
		return false;
	}

	return true;
}

void UMoverNetworkPhysicsLiaisonComponentBase::BeginPlay()
{
	Super::BeginPlay();

	// Register with the physics mover manager
	if (UPhysicsMoverManager* MoverManager = UWorld::GetSubsystem<UPhysicsMoverManager>(GetWorld()))
	{
		MoverManager->RegisterPhysicsMoverComponent(this);

		InitializeSimOutputData();
	}
}

void UMoverNetworkPhysicsLiaisonComponentBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Unregister with the physics mover manager
	if (UPhysicsMoverManager* MoverManager = UWorld::GetSubsystem<UPhysicsMoverManager>(GetWorld()))
	{
		MoverManager->UnregisterPhysicsMoverComponent(this);
	}

	Super::EndPlay(EndPlayReason);
}

//////////////////////////////////////////////////////////////////////////

Chaos::FUniqueIdx UMoverNetworkPhysicsLiaisonComponentBase::GetUniqueIdx() const
{
	if (const UMoverComponent& MoverComp = GetMoverComponent(); MoverComp.UpdatedCompAsPrimitive)
	{
		if (FBodyInstance* BI = MoverComp.UpdatedCompAsPrimitive->GetBodyInstance())
		{
			if (FPhysicsActorHandle ActorHandle = BI->GetPhysicsActor())
			{
				return ActorHandle->GetGameThreadAPI().UniqueIdx();
			}
		}
	}

	return Chaos::FUniqueIdx();
}

FMoverTimeStep UMoverNetworkPhysicsLiaisonComponentBase::GetCurrentAsyncMoverTimeStep_Internal() const
{
	Chaos::EnsureIsInPhysicsThreadContext();
	ensure(bUsingAsyncPhysics);

	FMoverTimeStep TimeStep;
	if (Chaos::FPhysicsSolver* Solver = GetPhysicsSolver())
	{
		TimeStep.ServerFrame = Solver->GetCurrentFrame() + GetNetworkPhysicsTickOffset_Internal();
		TimeStep.StepMs = Solver->GetAsyncDeltaTime() * 1000.0f;
		TimeStep.BaseSimTimeMs = TimeStep.ServerFrame * TimeStep.StepMs;
		TimeStep.bIsResimulating = Solver->GetEvolution()->IsResimming();
	}

	return TimeStep;
}

FMoverTimeStep UMoverNetworkPhysicsLiaisonComponentBase::GetCurrentAsyncMoverTimeStep_External() const
{
	Chaos::EnsureIsInGameThreadContext();
	ensure(bUsingAsyncPhysics);

	FMoverTimeStep TimeStep;
	if (Chaos::FPhysicsSolver* Solver = GetPhysicsSolver())
	{
		const int32 Offset = GetNetworkPhysicsTickOffset_External();
		TimeStep.ServerFrame = Solver->GetCurrentFrame() + Offset;
		TimeStep.StepMs = Solver->GetAsyncDeltaTime() * 1000.0f;
		TimeStep.BaseSimTimeMs = Solver->GetPhysicsResultsTime_External() * 1000.0f + Offset * TimeStep.StepMs;
		TimeStep.bIsResimulating = Solver->GetEvolution()->IsResimming();
	}

	return TimeStep;
}

FMoverTimeStep UMoverNetworkPhysicsLiaisonComponentBase::GetCurrentMoverTimeStep(float DeltaSeconds) const
{
	ensure(!bUsingAsyncPhysics);

	FMoverTimeStep TimeStep;
	if (Chaos::FPhysicsSolver* Solver = GetPhysicsSolver())
	{
		TimeStep.ServerFrame = Solver->GetCurrentFrame();
		TimeStep.StepMs = DeltaSeconds * 1000.0f;
		TimeStep.BaseSimTimeMs = Solver->GetSolverTime() * 1000.0f;
		TimeStep.bIsResimulating = Solver->GetEvolution()->IsResimming();
	}

	return TimeStep;
}

void UMoverNetworkPhysicsLiaisonComponentBase::InitializeSimOutputData()
{
	SimOutputRecord.Clear();

	FMoverSyncState SyncState;
	SyncState.MovementMode = GetMoverComponent().StartingMovementMode;

	double SimTime = 0.0;
	if (Chaos::FPhysicsSolver* Solver = GetPhysicsSolver())
	{
		SimTime = Solver->GetPhysicsResultsTime_External();
	}

	FMoverInputCmdContext InputCmd;
	
	SimOutputRecord.Add(SimTime, SyncState, InputCmd);
	LatestSyncState = SyncState;
}

UMoverComponent& UMoverNetworkPhysicsLiaisonComponentBase::GetMoverComponent() const
{
	return *GetOuterUMoverComponent();
}

void UMoverNetworkPhysicsLiaisonComponentBase::ProduceInput_External(int32 PhysicsStep, int32 NumSteps, OUT FPhysicsMoverAsyncInput& Input)
{
	Chaos::EnsureIsInGameThreadContext();

	if (HasValidState())
	{
		// Setting these denote the input as valid, and means ProcessInput_Internal will be called with it on the PT
		Input.MoverIdx = GetUniqueIdx();
		Input.MoverSimulation = this;

		// Propagate last frame's output sync state as the initial input sync state for the upcoming frame
		Input.SyncState = LatestSyncState;
		
		float DeltaSeconds = 0.0f;
		if (const UWorld* World = GetWorld())
		{
			if (bUsingAsyncPhysics)
			{
				if (const Chaos::FPhysicsSolver* Solver = GetPhysicsSolver(); Solver && Solver->IsUsingAsyncResults())
				{
					DeltaSeconds = Solver->GetAsyncDeltaTime();
				}
			}
			else
			{
				DeltaSeconds = World->GetDeltaSeconds();
			}
		}

		if (!NetworkPhysicsComponent || NetworkPhysicsComponent->IsLocallyControlled())
		{
			PerformProduceInput_External(DeltaSeconds, Input);
			
			// This is the net instance responsible for actually producing the input command, so set LatestInputCmd here to be picked up by the NPC for replication
			// All other net instances will have LatestInputCmd assigned in PreProcessInput by the NPC and applied as the actual InputCmd to process down in ProcessInputs_Internal
			LatestInputCmd = Input.InputCmd;
		}
		
		UMoverComponent& MoverComp = GetMoverComponent();
		const FMoverTimeStep MoverTimeStep = bUsingAsyncPhysics ? GetCurrentAsyncMoverTimeStep_External() : GetCurrentMoverTimeStep(DeltaSeconds);
		MoverComp.CachedLastSimTickTimeStep = MoverTimeStep;
		
		//@todo DanH: The input info broadcast here will be empty/incorrect when not locally controlled. That seems like a possible landmine of confusion?
		if (MoverComp.OnPreSimulationTick.IsBound())
		{
			MoverComp.OnPreSimulationTick.Broadcast(MoverTimeStep, Input.InputCmd);
		}
	}
}

void UMoverNetworkPhysicsLiaisonComponentBase::PerformProduceInput_External(float DeltaTime, FPhysicsMoverAsyncInput& Input)
{
	UMoverComponent& MoverComp = GetMoverComponent();
	const int DeltaTimeMS = FMath::RoundToInt(DeltaTime * 1000.0f);
	MoverComp.ProduceInput(DeltaTimeMS, &Input.InputCmd);
}

void UMoverNetworkPhysicsLiaisonComponentBase::ConsumeOutput_External(const FPhysicsMoverAsyncOutput& Output, double OutputTimeInSeconds)
{
	Chaos::EnsureIsInGameThreadContext();

	if (Output.bIsValid)
	{
		SimOutputRecord.Add(OutputTimeInSeconds, Output.SyncState, Output.InputCmd);
	}
}

void UMoverNetworkPhysicsLiaisonComponentBase::PostPhysicsUpdate_External()
{
	Chaos::EnsureIsInGameThreadContext();

	if (!HasValidState())
	{
		return;
	}

	UMoverComponent& MoverComp = GetMoverComponent();
	const float DeltaSeconds = MoverComp.CachedLastSimTickTimeStep.StepMs * 0.001f;
	const FMoverTimeStep MoverTimeStep = bUsingAsyncPhysics ? GetCurrentAsyncMoverTimeStep_External() : GetCurrentMoverTimeStep(DeltaSeconds);

	FMoverSyncState InterpolatedSyncState;
	FMoverInputCmdContext InterpolatedInputCmd;
	if (bUsingAsyncPhysics)
	{
		const float ResultsTime = GetPhysicsSolver()->GetPhysicsResultsTime_External();
		SimOutputRecord.GetInterpolated(ResultsTime, InterpolatedSyncState, InterpolatedInputCmd);
	}
	else
	{
		InterpolatedSyncState = SimOutputRecord.GetLatestSyncState();
		InterpolatedInputCmd = SimOutputRecord.GetLatestInputCmd();
	}
	
	// Physics interactions in the last frame may have caused a change in position or velocity that's different from what a simple lerp would predict,
	// so stomp the lerped sync state's transform data with that of the actual particle after the last sim frame
	FMoverDefaultSyncState& TransformSyncState = InterpolatedSyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();
	if (Chaos::FPBDRigidParticle* Particle = GetControlledParticle_External())
	{
		TransformSyncState.SetTransforms_WorldSpace(Particle->GetX(), FRotator(Particle->GetR()), Particle->GetV(), TransformSyncState.GetMovementBase(), TransformSyncState.GetMovementBaseBoneName());
		
		// Make sure the move direction intent is in base space (the base quat is identity if there's no base, effectively making this a no-op)
		TransformSyncState.MoveDirectionIntent = TransformSyncState.GetCapturedMovementBaseQuat().UnrotateVector(TransformSyncState.MoveDirectionIntent);
	}

	const FName CachedMovementMode = MoverComp.GetMovementModeName();

	// The MoverComponent relies on its CachedLastSyncState for a lot of information, so setting it here is what makes the resulting state of the completed sim frame
	// "real" from the perspective the MoverComp and any objects that call getters on it
	FMoverSyncState& BufferedSyncState = MoverComp.MoverSyncStateDoubleBuffer.GetWritable();
	BufferedSyncState = InterpolatedSyncState;
	MoverComp.LastMoverDefaultSyncState = BufferedSyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	MoverComp.MoverSyncStateDoubleBuffer.Flip();
	
	MoverComp.CachedLastUsedInputCmd = InterpolatedInputCmd;

	// Note this may return something different now because its dependent on the CachedLastSyncState that we just changed above
	const FName NextMode = MoverComp.GetMovementModeName();
	if (CachedMovementMode != NextMode)
	{
		//@todo DanH: Is this sufficient to just trigger the event? Should we instead be queueing the next mode and going through AdvanceToNextMode on the FSM?
		MoverComp.OnMovementModeChanged.Broadcast(CachedMovementMode, NextMode);
	}

	MoverComp.CachedLastSimTickTimeStep = MoverTimeStep;
	MoverComp.OnPostSimulationTick.Broadcast(MoverTimeStep);
}

void UMoverNetworkPhysicsLiaisonComponentBase::ProcessInputs_Internal(int32 PhysicsStep, float DeltaTime, const FPhysicsMoverAsyncInput& Input) const
{
	Chaos::EnsureIsInPhysicsThreadContext();

	if (CanProcessInputs_Internal(Input))
	{
		if (NetworkPhysicsComponent && NetworkPhysicsComponent->GetNetworkPhysicsComponent_Internal())
		{
			const bool bIsInputGenerator = NetworkPhysicsComponent->GetNetworkPhysicsComponent_Internal()->IsLocallyControlled();
			if (!bIsInputGenerator)
			{
				// If this instance is not generating input for the owning actor, the NPC will have written our LatestInputCmd (via FNetworkPhysicsMoverInputs::ApplyData)
				// by now as part of PreProcessInput. Applying it as the frame input locally is the final step of replicating the inputs from the generator to everyone else.
				Input.InputCmd = LatestInputCmd;
			}

			// Unlike inputs, there's no case where the server isn't the authority on the sync state. By default, the LatestSyncState is
			// only set by NPC replication when resimulating (via FNetworkPhysicsMoverState::ApplyData), but that setting can be adjusted.
			// So to account for all possible configs, always establish the initial sync state of the frame based on LatestSyncState on clients (it'd be redundant on servers).
			if (!NetworkPhysicsComponent->GetNetworkPhysicsComponent_Internal()->IsServer())
			{
				Chaos::FPBDRigidsEvolution* RigidsEvolution = GetPhysicsSolver()->GetEvolution();
				const bool bIsSolverResim = RigidsEvolution->IsResimming();
				const bool bIsFirstResimFrame = RigidsEvolution->IsResetting();
				const bool bIsProxyRepResim = NetworkPhysicsComponent->GetNetworkPhysicsComponent_Internal()->GetPhysicsReplicationMode() == EPhysicsReplicationMode::Resimulation;

				// Rollback mover state if on the first resimulation frame
				if ((bIsInputGenerator || bIsProxyRepResim) && bIsSolverResim && bIsFirstResimFrame)
				{
					UMoverComponent& MoverComp = GetMoverComponent();
					FMoverAuxStateContext UnusedInvalidAuxState;
					FMoverAuxStateContext UnusedAuxState;
					MoverComp.OnSimulationPreRollback(&Input.SyncState, &LatestSyncState, &UnusedInvalidAuxState, &UnusedAuxState);
					GetCurrentStateData(Input.SyncState);
					MoverComp.OnSimulationRollback(&Input.SyncState, &UnusedAuxState);
				}
				else
				{
					GetCurrentStateData(Input.SyncState);
				}
			}
		}
		
		PerformProcessInputs_Internal(PhysicsStep, DeltaTime, Input);
	}
}

bool UMoverNetworkPhysicsLiaisonComponentBase::CanProcessInputs_Internal(const FPhysicsMoverAsyncInput& Input) const
{
	return HasValidState() &&
		GetPhysicsSolver() &&
		GetControlledParticle_Internal() &&
		GetMoverComponent().MovementModes.Contains(Input.SyncState.MovementMode);
}

void UMoverNetworkPhysicsLiaisonComponentBase::PerformProcessInputs_Internal(int32 PhysicsStep, float DeltaTime, const FPhysicsMoverAsyncInput& Input) const
{
	const FMoverTimeStep MoverTimeStep = bUsingAsyncPhysics ? GetCurrentAsyncMoverTimeStep_Internal() : GetCurrentMoverTimeStep(DeltaTime);
	FMoverAuxStateContext UnusedAuxState;

	if (GetMoverComponent().OnPreMovement.IsBound())
	{
		GetMoverComponent().OnPreMovement.Broadcast(MoverTimeStep, Input.InputCmd, Input.SyncState, UnusedAuxState);
	}

	if (Chaos::FPBDRigidParticleHandle* ParticleHandle = GetControlledParticle_Internal())
	{
		FMoverDefaultSyncState& InputSyncState = Input.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();
		InputSyncState.SetTransforms_WorldSpace(ParticleHandle->GetX(), FRotator(ParticleHandle->GetR()), ParticleHandle->GetV());
	}
}

void UMoverNetworkPhysicsLiaisonComponentBase::PreSimulate_Internal(const FPhysicsMoverSimulationTickParams& TickParams, const FPhysicsMoverAsyncInput& Input, OUT FPhysicsMoverAsyncOutput& Output) const
{
	Chaos::EnsureIsInPhysicsThreadContext();

	// Sync state should carry over to the next sim frame by default unless something modifies it
	Output.SyncState = Input.SyncState;

	Output.InputCmd = Input.InputCmd;

	if (CanSimulate_Internal(TickParams, Input))
	{
		PerformPreSimulate_Internal(TickParams, Input, Output);

		// Physics can tick multiple times using the same input data from the game thread
		// so make sure to update it here using the results of this update
		Input.SyncState = Output.SyncState;

		// This is required for cases where we run a second physics update (including generating
		// input) before the output is saved on the game thread. We want to make sure the next
		// physics tick starts with the sync state from the previous tick
		LatestSyncState = Output.SyncState;

		Output.bIsValid = true;
	}
}

bool UMoverNetworkPhysicsLiaisonComponentBase::CanSimulate_Internal(const FPhysicsMoverSimulationTickParams& TickParams, const FPhysicsMoverAsyncInput& Input) const
{
	Chaos::EnsureIsInPhysicsThreadContext();

	if (!CanProcessInputs_Internal(Input))
	{
		return false;
	}

	if (!Input.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>())
	{
		return false;
	}

	const UMoverComponent& MoverComp = GetMoverComponent();
	if (!MoverComp.UpdatedCompAsPrimitive->IsSimulatingPhysics())
	{
		return false;
	}

	return true;
}

void UMoverNetworkPhysicsLiaisonComponentBase::PerformPreSimulate_Internal(const FPhysicsMoverSimulationTickParams& TickParams, const FPhysicsMoverAsyncInput& Input, FPhysicsMoverAsyncOutput& Output) const
{
	Chaos::EnsureIsInPhysicsThreadContext();

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Update the simulation

	UMoverComponent& MoverComp = GetMoverComponent();
	const FMoverTimeStep TimeStep = bUsingAsyncPhysics ? GetCurrentAsyncMoverTimeStep_Internal() : GetCurrentMoverTimeStep(TickParams.DeltaTimeSeconds);

	// Update movement state machine
	if (MoverComp.bHasRolledBack)
	{
		MoverComp.ProcessFirstSimTickAfterRollback(TimeStep);
	}

	//@todo DanH: Invoking the FSM OnSimulationTick can sometimes trigger AdvanceToNextMode to happen on the PT
	// Tick the actual simulation. This is where the proposed moves are queried and executed, affecting change to the moving actor's gameplay state and captured in the output sim state
	FMoverTickStartData TickStartData(Input.InputCmd, Input.SyncState, FMoverAuxStateContext());
	FMoverTickEndData TickEndData;
	TickEndData.SyncState = TickStartData.SyncState;

	MoverComp.ModeFSM->OnSimulationTick(MoverComp.UpdatedComponent, MoverComp.UpdatedCompAsPrimitive, MoverComp.GetSimBlackboard_Mutable(), TickStartData, TimeStep, TickEndData);

	// Set the output sync state and fill in the movement mode
	Output.SyncState = TickEndData.SyncState;

	FMoverDefaultSyncState& OutputSyncState = Output.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();

	const FName MovementModeAfterTick = MoverComp.ModeFSM->GetCurrentModeName();
	Output.SyncState.MovementMode = MovementModeAfterTick;

	MoverComp.GetSimBlackboard()->TryGet(CommonBlackboard::LastFloorResult, Output.FloorResult);

	if (Chaos::FPBDRigidParticleHandle* ParticleHandle = GetControlledParticle_Internal())
	{
		FVector TargetDeltaPos = OutputSyncState.GetLocation_WorldSpace() - ParticleHandle->GetX();
		if (TargetDeltaPos.SizeSquared2D() > GPhysicsDrivenMotionDebugParams.TeleportThreshold * GPhysicsDrivenMotionDebugParams.TeleportThreshold)
		{
			GetPhysicsSolver()->GetEvolution()->SetParticleTransform(ParticleHandle, OutputSyncState.GetLocation_WorldSpace(), OutputSyncState.GetOrientation_WorldSpace().Quaternion(), true);
		}

		ParticleHandle->SetV(OutputSyncState.GetVelocity_WorldSpace());

		//@todo DanH: Does the base need to concern itself with setting W on the particle?
	}
}

Chaos::FPhysicsObject* UMoverNetworkPhysicsLiaisonComponentBase::GetControlledPhysicsObject() const
{
	if (IPhysicsComponent* PhysicsComponent = Cast<IPhysicsComponent>(GetMoverComponent().GetUpdatedComponent()))
	{
		return PhysicsComponent->GetPhysicsObjectByName(NAME_None);
	}
	return nullptr;
}

Chaos::FPBDRigidParticle* UMoverNetworkPhysicsLiaisonComponentBase::GetControlledParticle_External() const
{
	if (Chaos::FPhysicsObject* ControlledObject = GetControlledPhysicsObject())
	{
		return FPhysicsObjectExternalInterface::LockRead(ControlledObject)->GetRigidParticle(ControlledObject);
	}
	return nullptr;
}

Chaos::FPBDRigidParticleHandle* UMoverNetworkPhysicsLiaisonComponentBase::GetControlledParticle_Internal() const
{
	if (Chaos::FPhysicsObject* ControlledObject = GetControlledPhysicsObject())
	{
		Chaos::FReadPhysicsObjectInterface_Internal Interface = Chaos::FPhysicsObjectInternalInterface::GetRead();
		return Interface.GetRigidParticle(ControlledObject);
	}
	return nullptr;
}

void UMoverNetworkPhysicsLiaisonComponentBase::OnContactModification_Internal(const FPhysicsMoverAsyncInput& Input, Chaos::FCollisionContactModifier& Modifier) const
{
	Chaos::EnsureIsInPhysicsThreadContext();

	// if (CanModifyContact_Internal(Input, Modifier))
	// {
	// 	PerformContactModification_Internal(Input, Modifier);
	// }
}

void UMoverNetworkPhysicsLiaisonComponentBase::TeleportParticleBy_Internal(Chaos::FGeometryParticleHandle& Particle, const FVector& PositionDelta, const FQuat& RotationDelta) const
{
	const FVector TeleportLocation = Particle.GetX() + PositionDelta;
	const FQuat TeleportRotation = Particle.GetR() + RotationDelta;
	if (const Chaos::FPhysicsSolver* Solver = GetPhysicsSolver(); Solver && Solver->GetEvolution())
	{
		Solver->GetEvolution()->SetParticleTransform(&Particle, TeleportLocation, TeleportRotation, true);
	}
}

void UMoverNetworkPhysicsLiaisonComponentBase::WakeParticleIfSleeping(Chaos::FGeometryParticleHandle* Particle) const
{
	if (Particle)
	{
		if (Chaos::FPBDRigidParticleHandle* RigidParticle = Particle->CastToRigidParticle();
			RigidParticle && RigidParticle->ObjectState() == Chaos::EObjectStateType::Sleeping)
		{
			if (const Chaos::FPhysicsSolver* Solver = GetPhysicsSolver(); Solver && Solver->GetEvolution())
			{
				Solver->GetEvolution()->SetParticleObjectState(RigidParticle, Chaos::EObjectStateType::Dynamic);
			}
		}
	}
}

int32 UMoverNetworkPhysicsLiaisonComponentBase::GetNetworkPhysicsTickOffset_Internal() const
{
	if (NetworkPhysicsComponent)
	{
		const FAsyncNetworkPhysicsComponent* NPCPhysicsThreadAPI = NetworkPhysicsComponent->GetNetworkPhysicsComponent_Internal();
		if (NPCPhysicsThreadAPI && !NPCPhysicsThreadAPI->IsServer())
		{
			return NPCPhysicsThreadAPI->GetNetworkPhysicsTickOffset();
		}
	}

	return 0;
}

int32 UMoverNetworkPhysicsLiaisonComponentBase::GetNetworkPhysicsTickOffset_External() const
{
	if (NetworkPhysicsComponent && !NetworkPhysicsComponent->HasServerWorld())
	{
		const APlayerController* PlayerController = NetworkPhysicsComponent->GetPlayerController();
		if (!PlayerController)
		{
			PlayerController = GetWorld()->GetFirstPlayerController();
		}

		if (PlayerController)
		{
			return PlayerController->GetNetworkPhysicsTickOffset();
		}
	}
	
	return 0;
}

#undef LOCTEXT_NAMESPACE
