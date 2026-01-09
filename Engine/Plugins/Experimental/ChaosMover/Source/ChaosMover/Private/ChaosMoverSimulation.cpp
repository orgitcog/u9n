// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosMover/ChaosMoverSimulation.h"

#include "Chaos/Character/CharacterGroundConstraint.h"
#include "Chaos/Character/CharacterGroundConstraintContainer.h"
#include "Chaos/ContactModification.h"
#include "Chaos/PhysicsObjectInternalInterface.h"
#include "PhysicsEngine/PhysicsObjectExternalInterface.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "ChaosMover/ChaosMovementMode.h"
#include "ChaosMover/ChaosMovementModeTransition.h"
#include "ChaosMover/ChaosMoverLog.h"
#include "ChaosMover/ChaosMoverSimulationTypes.h"
#include "ChaosMover/Utilities/ChaosMoverQueryUtils.h"
#include "Framework/Threading.h"
#include "MoveLibrary/FloorQueryUtils.h"
#include "MoveLibrary/MovementMixer.h"
#include "MoveLibrary/MoverBlackboard.h"
#include "MoveLibrary/WaterMovementUtils.h"
#include "MovementModeStateMachine.h"
#include "MoverSimulationTypes.h"
#include "PBDRigidsSolver.h"
#include "PhysicsProxy/CharacterGroundConstraintProxy.h"
#include "Chaos/PBDJointConstraintData.h"
#include "UObject/UObjectGlobals.h"
#include "Chaos/ChaosEngineInterface.h"
#include "Chaos/KinematicTargets.h"
#include "Engine/World.h"

#if WITH_CHAOS_VISUAL_DEBUGGER
#include "ChaosVisualDebugger/ChaosVisualDebuggerTrace.h"
#include "ChaosVisualDebugger/MoverCVDRuntimeTrace.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(ChaosMoverSimulation)

UChaosMoverSimulation::UChaosMoverSimulation()
{
}

const FMoverDataCollection& UChaosMoverSimulation::GetLocalSimInput() const
{
	return LocalSimInput;
}

FMoverDataCollection& UChaosMoverSimulation::GetLocalSimInput_Mutable()
{
	// Only the Gameplay Thread is allowed to write to the local simulation input data collection
	Chaos::EnsureIsInGameThreadContext();

	return LocalSimInput;
}

FMoverDataCollection& UChaosMoverSimulation::GetDebugSimData()
{
	return DebugSimData;
}

const UBaseMovementMode* UChaosMoverSimulation::GetCurrentMovementMode() const
{
	return StateMachine.GetCurrentMode().Get();
}

const UBaseMovementMode* UChaosMoverSimulation::FindMovementModeByName(const FName& Name) const
{
	return StateMachine.FindMovementMode(Name).Get();
}

void UChaosMoverSimulation::InitNetInputData(const FMoverInputCmdContext& InNetInputCmd)
{
	InputCmd = InNetInputCmd;
}

void UChaosMoverSimulation::ApplyNetInputData(const FMoverInputCmdContext& InNetInputCmd)
{
	InputCmd = InNetInputCmd;
	bInputCmdOverridden = true;
}

void UChaosMoverSimulation::BuildNetInputData(FMoverInputCmdContext& OutNetInputCmd) const
{
	OutNetInputCmd = InputCmd;
}

void UChaosMoverSimulation::ApplyNetStateData(const FMoverSyncState& InNetSyncState)
{
	CurrentSyncState = InNetSyncState;
}

void UChaosMoverSimulation::BuildNetStateData(FMoverSyncState& OutNetSyncState) const
{
	OutNetSyncState = CurrentSyncState;
}

void UChaosMoverSimulation::Init(const FInitParams& InitParams)
{
	// Only the Gameplay Thread is allowed to Init the chaos mover simulation
	Chaos::EnsureIsInGameThreadContext();

	MovementMixerWeakPtr = InitParams.MovementMixer.Get();
	CharacterConstraintProxy = InitParams.CharacterConstraintProxy;
	PathTargetConstraintProxy = InitParams.PathTargetConstraintProxy;
	PathTargetKinematicEndPointProxy = InitParams.PathTargetKinematicEndPointProxy;
	PhysicsObject = InitParams.PhysicsObject;
	Solver = InitParams.Solver;
	World = InitParams.World;

	CurrentSyncState = InitParams.InitialSyncState;

	UE::ChaosMover::FMoverStateMachine::FInitParams StateMachineInitParams;
	StateMachineInitParams.ImmediateMovementModeTransition = InitParams.ImmediateModeTransition;
	StateMachineInitParams.NullMovementMode = InitParams.NullMovementMode;
	StateMachineInitParams.Simulation = this;
	StateMachine.Init(StateMachineInitParams);

	for (const TPair<FName, TWeakObjectPtr<UBaseMovementMode>>& Element : InitParams.ModesToRegister)
	{
		const FName& ModeName = Element.Key;
		TWeakObjectPtr<UBaseMovementMode> Mode = Element.Value;

		if (Mode.Get() == nullptr)
		{
			UE_LOG(LogChaosMover, Warning, TEXT("Invalid Movement Mode type '%s' detected. Mover actor will not function correctly."), *ModeName.ToString());
			continue;
		}

		if (UChaosMovementMode* ChaosMode = Cast<UChaosMovementMode>(Mode.Get()))
		{
			ChaosMode->SetSimulation(this);
		}

		bool bIsDefaultMode = (InitParams.StartingMovementMode == ModeName);
		StateMachine.RegisterMovementMode(ModeName, Mode, bIsDefaultMode);
	}

	for (const TWeakObjectPtr<UBaseMovementModeTransition>& Transition : InitParams.TransitionsToRegister)
	{
		if (UChaosMovementModeTransition* ChaosTransition = Cast<UChaosMovementModeTransition>(Transition.Get()))
		{
			ChaosTransition->SetSimulation(this);
		}

		StateMachine.RegisterGlobalTransition(Transition);
	}

	StateMachine.SetModeImmediately(StateMachine.GetDefaultModeName());

	OnInit();
}

void UChaosMoverSimulation::Deinit()
{
	OnDeinit();
}

void UChaosMoverSimulation::OnInit()
{
	
}

void UChaosMoverSimulation::OnDeinit()
{

}

void UChaosMoverSimulation::SimulationTick(const FMoverTimeStep& TimeStep, const UE::ChaosMover::FSimulationInputData& InputData, UE::ChaosMover::FSimulationOutputData& OutputData)
{
	Chaos::EnsureIsInPhysicsThreadContext();

	OnPreSimulationTick(TimeStep, InputData);
	OnSimulationTick(TimeStep, InputData, OutputData);
	OnPostSimulationTick(TimeStep, OutputData);
}

void UChaosMoverSimulation::ModifyContacts(const FMoverTimeStep& TimeStep, const UE::ChaosMover::FSimulationInputData& InputData, const UE::ChaosMover::FSimulationOutputData& OutputData, Chaos::FCollisionContactModifier& Modifier) const
{
	Chaos::EnsureIsInPhysicsThreadContext();

	if (TStrongObjectPtr<const UBaseMovementMode> CurrentModePtr = StateMachine.GetCurrentMode().Pin())
	{
		if (const UChaosMovementMode* ChaosMode = Cast<UChaosMovementMode>(CurrentModePtr.Get()))
		{
			// Base contact modification
			// Disable collisions for actors and components on the ignore list in the query params
			if (ChaosMode->IgnoredCollisionMode == EChaosMoverIgnoredCollisionMode::DisableCollisionsWithIgnored)
			{
				Chaos::FGeometryParticleHandle* UpdatedParticle = nullptr;
				const FChaosMoverSimulationDefaultInputs* SimInputs = LocalSimInput.FindDataByType<FChaosMoverSimulationDefaultInputs>();
				if (SimInputs)
				{
					Chaos::FReadPhysicsObjectInterface_Internal ReadInterface = Chaos::FPhysicsObjectInternalInterface::GetRead();
					UpdatedParticle = ReadInterface.GetParticle(SimInputs->PhysicsObject);
				}

				if (!UpdatedParticle)
				{
					return;
				}

				for (Chaos::FContactPairModifier& PairModifier : Modifier.GetContacts(UpdatedParticle))
				{
					const int32 OtherIdx = UpdatedParticle == PairModifier.GetParticlePair()[0] ? 1 : 0;

					if (const Chaos::FShapeInstance* Shape = PairModifier.GetShape(OtherIdx))
					{
						uint32 ComponentID = ChaosInterface::GetSimulationFilterData(*Shape).Word2;
						if (SimInputs->CollisionQueryParams.GetIgnoredComponents().Contains(ComponentID))
						{
							PairModifier.Disable();
							continue;
						}

						uint32 ActorID = ChaosInterface::GetQueryFilterData(*Shape).Word0;
						if (SimInputs->CollisionQueryParams.GetIgnoredSourceObjects().Contains(ActorID))
						{
							PairModifier.Disable();
							continue;
						}

						FMaskFilter ShapeMaskFilter = ChaosInterface::GetQueryFilterData(*Shape).Word3 >> (32u - NumExtraFilterBits);
						if (SimInputs->CollisionQueryParams.IgnoreMask & ShapeMaskFilter)
						{
							PairModifier.Disable();
							continue;
						}
					}
				}
			}

			// Mode specific contact modification
			ChaosMode->ModifyContacts(TimeStep, InputData, OutputData, Modifier);
		}
	}

	OnModifyContacts(TimeStep, InputData, OutputData, Modifier);
}

void UChaosMoverSimulation::PreSimulationTickCharacter(const IChaosCharacterMovementModeInterface& CharacterMode, const FMoverTimeStep& TimeStep, const UE::ChaosMover::FSimulationInputData& InputData)
{
	// Add inputs if we don't have them and make sure we have a valid input
	FCharacterDefaultInputs& CharacterDefaultInputs = InputData.InputCmd.InputCollection.FindOrAddMutableDataByType<FCharacterDefaultInputs>();
	if (CharacterDefaultInputs.GetMoveInputType() == EMoveInputType::Invalid)
	{
		CharacterDefaultInputs.SetMoveInput(EMoveInputType::DirectionalIntent, FVector::ZeroVector);
	}

	if (!CharacterDefaultInputs.SuggestedMovementMode.IsNone())
	{
		StateMachine.QueueNextMode(CharacterDefaultInputs.SuggestedMovementMode);
		CharacterDefaultInputs.SuggestedMovementMode = NAME_None;
	}
}

void UChaosMoverSimulation::OnPreSimulationTick(const FMoverTimeStep& TimeStep, const UE::ChaosMover::FSimulationInputData& InputData)
{
	if (bInputCmdOverridden)
	{
		InputData.InputCmd = InputCmd;
	}

	if (TimeStep.bIsResimulating)
	{
		check(Solver);
		if (Solver->GetEvolution()->IsResetting())
		{
			// Rollback blackboard on the first frame of resimulation
			Blackboard->Invalidate(EInvalidationReason::Rollback);
		}
	}

	// Update the sync state from the current physics state
	FMoverDefaultSyncState& PreSimDefaultSyncState = CurrentSyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();
	if (const Chaos::FPBDRigidParticleHandle* ParticleHandle = GetControlledParticle())
	{
		PreSimDefaultSyncState.SetTransforms_WorldSpace(ParticleHandle->GetX(), FRotator(ParticleHandle->GetR()), ParticleHandle->GetV());
	}

	if (TStrongObjectPtr<UBaseMovementMode> CurrentModePtr = StateMachine.GetCurrentMode().Pin())
	{
		if (IChaosCharacterMovementModeInterface* CharacterMode = Cast<IChaosCharacterMovementModeInterface>(CurrentModePtr.Get()))
		{
			PreSimulationTickCharacter(*CharacterMode, TimeStep, InputData);
		}
	}
}

void UChaosMoverSimulation::OnSimulationTick(const FMoverTimeStep& TimeStep, const UE::ChaosMover::FSimulationInputData& InputData, UE::ChaosMover::FSimulationOutputData& OutputData)
{
	check(Blackboard.Get());

	FMoverTickStartData TickStartData(InputData.InputCmd, CurrentSyncState, InputData.AuxInputState);
	FMoverTickEndData TickEndData(&CurrentSyncState, &InputData.AuxInputState);

	StateMachine.OnSimulationTick(TimeStep, TickStartData, Blackboard.Get(), MovementMixerWeakPtr.Get(), TickEndData);

	// Copy the sync state locally and to the output data
	OutputData.SyncState = TickEndData.SyncState;
	OutputData.LastUsedInputCmd = InputData.InputCmd;
}

void UChaosMoverSimulation::PostSimulationTickCharacter(const IChaosCharacterMovementModeInterface& CharacterMode, const FMoverTimeStep& TimeStep, UE::ChaosMover::FSimulationOutputData& OutputData)
{
	FChaosMoverCharacterSimState& CharacterSimState = InternalSimData.FindOrAddMutableDataByType<FChaosMoverCharacterSimState>();
	FMoverDefaultSyncState* PostSimDefaultSyncState = OutputData.SyncState.SyncStateCollection.FindMutableDataByType<FMoverDefaultSyncState>();
	check(PostSimDefaultSyncState);

	if (Chaos::FPBDRigidParticleHandle* ParticleHandle = GetControlledParticle())
	{
		// Linear motion
		CharacterSimState.TargetDeltaPosition = PostSimDefaultSyncState->GetLocation_WorldSpace() - ParticleHandle->GetX();
		ParticleHandle->SetV(PostSimDefaultSyncState->GetVelocity_WorldSpace());

		// Angular motion
		FQuat TgtQuat = PostSimDefaultSyncState->GetOrientation_WorldSpace().Quaternion();
		TgtQuat.EnforceShortestArcWith(ParticleHandle->GetR());
		FQuat QuatRotation = TgtQuat * ParticleHandle->GetR().Inverse();
		FVector AngularDisplacement = QuatRotation.ToRotationVector();

		FVector UpDir = FVector::UpVector;
		if (const FChaosMoverSimulationDefaultInputs* SimInputs = LocalSimInput.FindDataByType<FChaosMoverSimulationDefaultInputs>())
		{
			UpDir = SimInputs->UpDir;
		}
		CharacterSimState.TargetDeltaFacing = AngularDisplacement.Dot(UpDir);

		if (CharacterMode.ShouldCharacterRemainUpright())
		{
			const float DeltaTimeSeconds = TimeStep.StepMs * 0.001f;
			if (DeltaTimeSeconds > UE_SMALL_NUMBER)
			{
				ParticleHandle->SetW(AngularDisplacement / DeltaTimeSeconds);
			}
		}
	}
	else
	{
		CharacterSimState.TargetDeltaPosition = FVector::ZeroVector;
		CharacterSimState.TargetDeltaFacing = 0.0f;
	}

	// Update the movement base
	check(Blackboard.Get());
	FFloorCheckResult FloorResult;
	bool FoundLastFloorResult = Blackboard->TryGet(CommonBlackboard::LastFloorResult, FloorResult);
	bool FoundFloor = FoundLastFloorResult && FloorResult.bBlockingHit;
	// Note: We want to record the movement base but we don't record the transform
	// so don't use this to get a relative transform for the sync state
	PostSimDefaultSyncState->SetMovementBase(FoundFloor ? FloorResult.HitResult.GetComponent() : nullptr);
}

void UChaosMoverSimulation::PostSimulationTickCharacterConstraint(const IChaosCharacterConstraintMovementModeInterface& CharacterConstraintMode, const FMoverTimeStep& TimeStep, UE::ChaosMover::FSimulationOutputData& OutputData)
{
	Chaos::FCharacterGroundConstraintHandle* ConstraintHandle = nullptr;
	if (CharacterConstraintProxy && CharacterConstraintProxy->IsInitialized())
	{
		ConstraintHandle = CharacterConstraintProxy->GetPhysicsThreadAPI();
	}
	if (!ConstraintHandle)
	{
		return;
	}

	if (CharacterConstraintMode.ShouldEnableConstraint() && !ConstraintHandle->IsEnabled())
	{
		EnableCharacterConstraint();
	}
	else if (!CharacterConstraintMode.ShouldEnableConstraint() && ConstraintHandle->IsEnabled())
	{
		DisableCharacterConstraint();
		return;
	}

	// Update the up direction in the settings
	Chaos::FCharacterGroundConstraintSettings& Settings = ConstraintHandle->GetSettings_Mutable();
	if (const FChaosMoverSimulationDefaultInputs* SimInputs = LocalSimInput.FindDataByType<FChaosMoverSimulationDefaultInputs>())
	{
		Settings.VerticalAxis = SimInputs->UpDir;
	}

	check(Blackboard.Get());

	FFloorCheckResult FloorResult;

	// Update the constraint data based on the floor result
	if (Blackboard->TryGet(CommonBlackboard::LastFloorResult, FloorResult) && FloorResult.bBlockingHit)
	{
		// Set the ground particle on the constraint
		Chaos::FGeometryParticleHandle* GroundParticle = nullptr;

		if (Chaos::FPhysicsObjectHandle GroundPhysicsObject = FloorResult.HitResult.PhysicsObject)
		{
			Chaos::FReadPhysicsObjectInterface_Internal ReadInterface = Chaos::FPhysicsObjectInternalInterface::GetRead();
			if (!ReadInterface.AreAllDisabled({ GroundPhysicsObject }))
			{
				GroundParticle = ReadInterface.GetParticle(GroundPhysicsObject);
				if (ReadInterface.AreAllSleeping({ GroundPhysicsObject }))
				{
					Chaos::FWritePhysicsObjectInterface_Internal WriteInterface = Chaos::FPhysicsObjectInternalInterface::GetWrite();
					WriteInterface.WakeUp({ GroundPhysicsObject });
				}
			}
		}
		ConstraintHandle->SetGroundParticle(GroundParticle);

		// Set the max walkable slope angle using any override from the hit component
		float WalkableSlopeCosine = ConstraintHandle->GetSettings().CosMaxWalkableSlopeAngle;
		if (const TStrongObjectPtr<UPrimitiveComponent> PrimComp = FloorResult.HitResult.Component.Pin())
		{
			const FWalkableSlopeOverride& SlopeOverride = PrimComp->GetWalkableSlopeOverride();
			WalkableSlopeCosine = SlopeOverride.ModifyWalkableFloorZ(WalkableSlopeCosine);
		}

		if (!FloorResult.bWalkableFloor)
		{
			WalkableSlopeCosine = 2.0f;
		}

		FChaosMoverCharacterSimState* CharacterSimState = InternalSimData.FindMutableDataByType<FChaosMoverCharacterSimState>();
		check(CharacterSimState);

		ConstraintHandle->SetData({
			FloorResult.HitResult.ImpactNormal,
			CharacterSimState->TargetDeltaPosition,
			CharacterSimState->TargetDeltaFacing,
			FloorResult.FloorDist,
			WalkableSlopeCosine
			});
	}
	else
	{
		ConstraintHandle->SetGroundParticle(nullptr);
		ConstraintHandle->SetData({
			ConstraintHandle->GetSettings().VerticalAxis,
			Chaos::FVec3::ZeroVector,
			0.0,
			1.0e10,
			0.5f
			});
	}
}

void UChaosMoverSimulation::PostSimulationTickPathedMovement(const IChaosPathedMovementModeInterface& ConstraintMode, const FMoverTimeStep& TimeStep, UE::ChaosMover::FSimulationOutputData& OutputData)
{
	Chaos::FPBDJointConstraintHandle* ConstraintHandle = nullptr;
	if (PathTargetConstraintProxy && PathTargetConstraintProxy->IsInitialized())
	{
		ConstraintHandle = PathTargetConstraintProxy->GetHandle();
	}
	if (!ConstraintHandle)
	{
		return;
	}

	if (ConstraintMode.ShouldUseConstraint())
	{
		if (!ConstraintHandle->IsEnabled())
		{
			EnablePathTargetConstraint();
		}

		if (!IsControlledParticleDynamic())
		{
			SetControlledParticleDynamic();
		}
	}
	else if (!ConstraintMode.ShouldUseConstraint())
	{
		if (ConstraintHandle->IsEnabled())
		{
			DisablePathTargetConstraint();
		}

		if (!IsControlledParticleKinematic())
		{
			SetControlledParticleKinematic();
		}
	}

	const FMoverDefaultSyncState* PostSimDefaultSyncState = OutputData.SyncState.SyncStateCollection.FindMutableDataByType<FMoverDefaultSyncState>();
	check(PostSimDefaultSyncState);
	check(Solver);
	Chaos::FPBDRigidsEvolution& Evolution = *Solver->GetEvolution();

	// We always update the path target end point, even when the controlled particle is not constrained.
	// That way we don't leave a particle behind and don't risk having it very far and whip lashing when re-enabling the constraint.
	Chaos::FKinematicGeometryParticleHandle* EndpointParticleHandle = (ConstraintHandle && PathTargetKinematicEndPointProxy) ? PathTargetKinematicEndPointProxy->GetHandle_LowLevel()->CastToKinematicParticle() : nullptr;
	if (EndpointParticleHandle)
	{	
		Evolution.SetParticleKinematicTarget(EndpointParticleHandle, Chaos::FKinematicTarget::MakePositionTarget(PostSimDefaultSyncState->GetTransform_WorldSpace()));
	}
	if (IsControlledParticleKinematic())
	{
		if (Chaos::FPBDRigidParticleHandle* ParticleHandle = GetControlledParticle())
		{
			Evolution.SetParticleKinematicTarget(ParticleHandle, Chaos::FKinematicTarget::MakePositionTarget(PostSimDefaultSyncState->GetTransform_WorldSpace()));
		}
	}
}

void UChaosMoverSimulation::OnPostSimulationTick(const FMoverTimeStep& TimeStep, UE::ChaosMover::FSimulationOutputData& OutputData)
{
	// TODO - make this more extensible
	if (TStrongObjectPtr<UBaseMovementMode> CurrentModePtr = StateMachine.GetCurrentMode().Pin())
	{
		if (IChaosCharacterMovementModeInterface* CharacterMode = Cast<IChaosCharacterMovementModeInterface>(CurrentModePtr.Get()))
		{
			PostSimulationTickCharacter(*CharacterMode, TimeStep, OutputData);
		}

		if (IChaosCharacterConstraintMovementModeInterface* CharacterConstraintMode = Cast<IChaosCharacterConstraintMovementModeInterface>(CurrentModePtr.Get()))
		{
			PostSimulationTickCharacterConstraint(*CharacterConstraintMode, TimeStep, OutputData);
		}
		else
		{
			DisableCharacterConstraint();
		}

		if (IChaosPathedMovementModeInterface* ConstraintMode = Cast<IChaosPathedMovementModeInterface>(CurrentModePtr.Get()))
		{
			PostSimulationTickPathedMovement(*ConstraintMode, TimeStep, OutputData);
		}
		else
		{
			DisablePathTargetConstraint();
		}
	}

	CurrentSyncState = OutputData.SyncState;

	// Extract the events into the output data and clear
	OutputData.Events = Events;
	Events.Empty();

	// Send Debug Data to the Chaos Visual Debugger
	TraceMoverData(OutputData);
}

void UChaosMoverSimulation::TraceMoverData(const UE::ChaosMover::FSimulationOutputData& OutputData)
{
	// Send the latest physics thread data to CVD
#if WITH_CHAOS_VISUAL_DEBUGGER
	if (FChaosVisualDebuggerTrace::IsTracing())
	{
		static FName NAME_LocalSimImput("LocalSimImput");
		static FName NAME_InternalSimData("InternalSimData");
		static FName NAME_DebugSimData("DebugSimData");

		UE::MoverUtils::NamedDataCollections LocalSimDataCollections (
			{				
				{ NAME_LocalSimImput, &LocalSimInput},
				{ NAME_InternalSimData, &InternalSimData},
				{ NAME_DebugSimData, &DebugSimData}
			});

		Chaos::FReadPhysicsObjectInterface_Internal Interface = Chaos::FPhysicsObjectInternalInterface::GetRead();
		const Chaos::FGeometryParticleHandle* ParticleHandle = PhysicsObject? Interface.GetParticle(PhysicsObject) : nullptr;
		int32 ParticleID = ParticleHandle ? ParticleHandle->UniqueIdx().Idx : INDEX_NONE;

		int32 SolverID = CVD_TRACE_GET_SOLVER_ID_FROM_WORLD(World);

		UE::MoverUtils::FMoverCVDRuntimeTrace::TraceMoverData(SolverID, ParticleID, &OutputData.LastUsedInputCmd, &OutputData.SyncState, &LocalSimDataCollections);
	}
#endif
}

void UChaosMoverSimulation::OnModifyContacts(const FMoverTimeStep& TimeStep, const UE::ChaosMover::FSimulationInputData& InputData, const UE::ChaosMover::FSimulationOutputData& OutputData, Chaos::FCollisionContactModifier& Modifier) const
{
}

void UChaosMoverSimulation::AddEvent(TSharedPtr<FMoverSimulationEventData> Event)
{
	// Events are added to the event list for later extraction to game thread
	// We also allow the simulation to react to the event immediately
	Events.Add(Event);

	if (const FMoverSimulationEventData* EventData = Event.Get())
	{
		ProcessSimulationEvent(*EventData);
	}
}

void UChaosMoverSimulation::ProcessSimulationEvent(const FMoverSimulationEventData& EventData)
{
	if (const FMovementModeChangedEventData* ModeChangedEventData = EventData.CastTo<FMovementModeChangedEventData>())
	{
		OnMovementModeChanged(*ModeChangedEventData);
	}
}

void UChaosMoverSimulation::OnMovementModeChanged(const FMovementModeChangedEventData& ModeChangedData)
{
	TStrongObjectPtr<UBaseMovementMode> PreviousModePtr = StateMachine.FindMovementMode(ModeChangedData.PreviousModeName).Pin();
	TStrongObjectPtr<UBaseMovementMode> NextModePtr = StateMachine.FindMovementMode(ModeChangedData.NewModeName).Pin();

	if (PreviousModePtr && NextModePtr)
	{
		if (IChaosCharacterConstraintMovementModeInterface* NextCharacterConstraintMode = Cast<IChaosCharacterConstraintMovementModeInterface>(NextModePtr.Get()))
		{
			bool IsCharacterConstraintInitialized = (CharacterConstraintProxy != nullptr) && CharacterConstraintProxy->IsInitialized();
			Chaos::FCharacterGroundConstraintHandle* ConstraintHandle = IsCharacterConstraintInitialized ? CharacterConstraintProxy->GetPhysicsThreadAPI() : nullptr;
			if (ConstraintHandle)
			{
				Chaos::FCharacterGroundConstraintSettings& Settings = ConstraintHandle->GetSettings_Mutable();
				NextCharacterConstraintMode->UpdateConstraintSettings(Settings);

				// Character ground constraint modes currently assume moving a dynamic particle and using a character ground constraint
				// Revise if we start supporting moving a character kinematically
				if (!IsControlledParticleDynamic())
				{
					SetControlledParticleDynamic();
				}
			}
		}

		if (IChaosPathedMovementModeInterface* NextPathTargetConstraintMode = Cast<IChaosPathedMovementModeInterface>(NextModePtr.Get()))
		{
			bool IsPathTargetConstraintInitialized = (PathTargetConstraintProxy != nullptr) && PathTargetConstraintProxy->IsInitialized();
			Chaos::FPBDJointConstraintHandle* ConstraintHandle = IsPathTargetConstraintInitialized ? PathTargetConstraintProxy->GetHandle() : nullptr;
			if (ConstraintHandle)
			{
				ConstraintHandle->SetSettings(NextPathTargetConstraintMode->GetConstraintSettings());
			}
		}
	}
}

Chaos::FPBDRigidParticleHandle* UChaosMoverSimulation::GetControlledParticle() const
{
	if (PhysicsObject)
	{
		Chaos::FReadPhysicsObjectInterface_Internal Interface = Chaos::FPhysicsObjectInternalInterface::GetRead();
		return Interface.GetRigidParticle(PhysicsObject);
	}
	return nullptr;
}

void UChaosMoverSimulation::SetControlledParticleDynamic()
{
	if (Chaos::FPBDRigidParticleHandle* ControlledParticle = GetControlledParticle())
	{
		check(Solver);
		Chaos::FPBDRigidsEvolution& Evolution = *Solver->GetEvolution();
		Evolution.SetParticleObjectState(ControlledParticle, Chaos::EObjectStateType::Dynamic);
	}
}

void UChaosMoverSimulation::SetControlledParticleKinematic()
{
	if (Chaos::FPBDRigidParticleHandle* ControlledParticle = GetControlledParticle())
	{
		check(Solver);
		Chaos::FPBDRigidsEvolution& Evolution = *Solver->GetEvolution();
		Evolution.SetParticleObjectState(ControlledParticle, Chaos::EObjectStateType::Kinematic);

		if (const Chaos::TPBDRigidParticleHandleImp<Chaos::FReal, 3, true>* ControlledRigidParticle = ControlledParticle->CastToRigidParticle())
		{
			if (ControlledRigidParticle->UpdateKinematicFromSimulation() == true)
			{
				// Should we instead call SetUpdateKinematicFromSimulation on the GT when some of the modes may animate kinematically?
				UE_LOG(LogChaosMover, Warning, TEXT("The updated component for %s is not set to Update Kinematic from Simulation but is being moved kinematically"), *GetClass()->GetName());
			}
		}
	}
}

bool UChaosMoverSimulation::IsControlledParticleDynamic() const
{
	const Chaos::FPBDRigidParticleHandle* ControlledParticle = GetControlledParticle();
	return ControlledParticle ? ControlledParticle->IsDynamic() : false;
}

bool UChaosMoverSimulation::IsControlledParticleKinematic() const
{
	const Chaos::FPBDRigidParticleHandle* ControlledParticle = GetControlledParticle();
	return ControlledParticle ? ControlledParticle->IsKinematic() : false;
}

void UChaosMoverSimulation::EnableCharacterConstraint()
{
	if (CharacterConstraintProxy && CharacterConstraintProxy->IsInitialized())
	{
		Chaos::FCharacterGroundConstraintHandle* ConstraintHandle = CharacterConstraintProxy->GetPhysicsThreadAPI();

		if (ConstraintHandle->GetCharacterParticle())
		{
			ConstraintHandle->SetEnabled(true);
		}
	}
}

void UChaosMoverSimulation::DisableCharacterConstraint()
{
	if (CharacterConstraintProxy && CharacterConstraintProxy->IsInitialized())
	{
		Chaos::FCharacterGroundConstraintHandle* ConstraintHandle = CharacterConstraintProxy->GetPhysicsThreadAPI();

		ConstraintHandle->SetEnabled(false);
	}
}

void UChaosMoverSimulation::EnablePathTargetConstraint()
{
	if (PathTargetConstraintProxy && PathTargetConstraintProxy->IsInitialized())
	{
		if (Chaos::FPBDJointConstraintHandle* ConstraintHandle = PathTargetConstraintProxy->GetHandle())
		{
			ConstraintHandle->SetConstraintEnabled(true);
		}
	}
}

void UChaosMoverSimulation::DisablePathTargetConstraint()
{
	if (PathTargetConstraintProxy && PathTargetConstraintProxy->IsInitialized())
	{
		if (Chaos::FPBDJointConstraintHandle* ConstraintHandle = PathTargetConstraintProxy->GetHandle())
		{
			ConstraintHandle->SetConstraintEnabled(false);
		}
	}
}

void UChaosMoverSimulation::K2_QueueInstantMovementEffect(const int32& EffectAsRawData)
{
	// This will never be called, the exec version below will be hit instead
	checkNoEntry();
}

DEFINE_FUNCTION(UChaosMoverSimulation::execK2_QueueInstantMovementEffect)
{
	Stack.StepCompiledIn<FStructProperty>(nullptr);
	void* EffectPtr = Stack.MostRecentPropertyAddress;
	FStructProperty* StructProp = CastField<FStructProperty>(Stack.MostRecentProperty);

	P_FINISH;

	P_NATIVE_BEGIN;

	const bool bHasValidStructProp = StructProp && StructProp->Struct && StructProp->Struct->IsChildOf(FInstantMovementEffect::StaticStruct());

	if (ensureMsgf((bHasValidStructProp && EffectPtr), TEXT("An invalid type (%s) was sent to a QueueInstantMovementEffect node. A struct derived from FInstantMovementEffect is required. No Movement Effect will be queued."),
		StructProp ? *GetNameSafe(StructProp->Struct) : *Stack.MostRecentProperty->GetClass()->GetName()))
	{
		// Could we steal this instead of cloning? (move semantics)
		FInstantMovementEffect* EffectAsBasePtr = reinterpret_cast<FInstantMovementEffect*>(EffectPtr);
		FInstantMovementEffect* ClonedMove = EffectAsBasePtr->Clone();

		P_THIS->QueueInstantMovementEffect(TSharedPtr<FInstantMovementEffect>(ClonedMove));
	}

	P_NATIVE_END;
}

void UChaosMoverSimulation::QueueInstantMovementEffect(TSharedPtr<FInstantMovementEffect> InstantMovementEffect)
{
	StateMachine.QueueInstantMovementEffect(InstantMovementEffect);
}
