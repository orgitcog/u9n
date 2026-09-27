// Copyright Epic Games, Inc. All Rights Reserved.


#include "ChaosMoverStateMachine.h"

#include "Backends/ChaosMoverSubsystem.h"
#include "ChaosMover/ChaosMoverDeveloperSettings.h"
#include "ChaosMover/ChaosMoverLog.h"
#include "ChaosMover/ChaosMoverSimulation.h"
#include "MoveLibrary/MovementMixer.h"
#include "MoveLibrary/MovementUtils.h"
#include "MovementModeTransition.h"
#include "MoverComponent.h"
#include "MoverDeveloperSettings.h"
#include "MoverLog.h"
#include "Templates/SubclassOf.h"
#include "Framework/Threading.h"

namespace UE::ChaosMover
{
	FMoverStateMachine::FMoverStateMachine()
	{
	}

	FMoverStateMachine::~FMoverStateMachine()
	{
	}

	void FMoverStateMachine::Init(const FInitParams& Params)
	{
		Chaos::EnsureIsInGameThreadContext();

		// Careful, this is called from the GT
		ImmediateMovementModeTransitionWeakPtr = Params.ImmediateMovementModeTransition;
		NullMovementModeWeakPtr = Params.NullMovementMode;
		Simulation = Params.Simulation;

		ClearAllMovementModes();
		ClearAllGlobalTransitions();
	}

	void FMoverStateMachine::RegisterMovementMode(FName ModeName, TWeakObjectPtr<UBaseMovementMode> Mode, bool bIsDefaultMode)
	{
		// JAH TODO: add validation and warnings for overwriting modes
		// JAH TODO: add validation of Mode

		Modes.Add(ModeName, Mode);

		if (bIsDefaultMode)
		{
			//JAH TODO: add validation that we are only overriding the default null mode
			DefaultModeName = ModeName;
		}

		Mode->OnRegistered(ModeName);
	}

	void FMoverStateMachine::UnregisterMovementMode(FName ModeName)
	{
		TWeakObjectPtr<UBaseMovementMode> ModeToUnregisterWeakPtr = Modes.FindAndRemoveChecked(ModeName);
		TStrongObjectPtr<UBaseMovementMode> ModeToUnregister = ModeToUnregisterWeakPtr.Pin();

		if (ModeToUnregister)
		{
			ModeToUnregister->OnUnregistered();
		}
	}

	void FMoverStateMachine::ClearAllMovementModes()
	{
		Modes.Empty();

		for (TPair<FName,TWeakObjectPtr<UBaseMovementMode>>& Element : Modes)
		{
			TStrongObjectPtr<UBaseMovementMode> Mode = Element.Value.Pin();

			if (Mode)
			{
				Mode->OnUnregistered();
			}
		}

		ConstructDefaultModes();	// Note that we're resetting to our defaults so we keep the null movement mode
	}

	void FMoverStateMachine::SetDefaultMode(FName NewDefaultModeName)
	{
		check(Modes.Contains(NewDefaultModeName));

		DefaultModeName = NewDefaultModeName;
	}

	FName FMoverStateMachine::GetDefaultModeName() const
	{
		return DefaultModeName;
	}

	void FMoverStateMachine::RegisterGlobalTransition(TWeakObjectPtr<UBaseMovementModeTransition> Transition)
	{
		GlobalTransitions.Add(Transition);

		Transition->OnRegistered();
	}

	void FMoverStateMachine::UnregisterGlobalTransition(TWeakObjectPtr<UBaseMovementModeTransition> Transition)
	{
		Transition->OnUnregistered();

		GlobalTransitions.Remove(Transition);
	}

	void FMoverStateMachine::ClearAllGlobalTransitions()
	{
		for (TWeakObjectPtr<UBaseMovementModeTransition> TransitionWeakPtr : GlobalTransitions)
		{
			TStrongObjectPtr<UBaseMovementModeTransition> Transition = TransitionWeakPtr.Pin();
			if (Transition)
			{
				Transition->OnUnregistered();
			}
		}

		GlobalTransitions.Empty();
	}

	void FMoverStateMachine::QueueNextMode(FName DesiredNextModeName, bool bShouldReenter)
	{
		TStrongObjectPtr<UImmediateMovementModeTransition> QueuedModeTransition = QueuedModeTransitionWeakPtr.Pin();
		if (QueuedModeTransition)
		{
			if (DesiredNextModeName != NAME_None)
			{
				const FName NextModeName = QueuedModeTransition->GetNextModeName();
				const bool bShouldNextModeReenter = QueuedModeTransition->ShouldReenter();

				if ((NextModeName != NAME_None) &&
					(NextModeName != DesiredNextModeName || bShouldReenter != bShouldNextModeReenter))
				{
					UE_LOG(LogChaosMover, Log, TEXT("%s (%s) Overwriting of queued mode change (%s, reenter: %i) with (%s, reenter: %i)"), *OwnerActorName, *UEnum::GetValueAsString(OwnerActorLocalNetRole), *NextModeName.ToString(), bShouldNextModeReenter, *DesiredNextModeName.ToString(), bShouldReenter);
				}

				if (Modes.Contains(DesiredNextModeName))
				{
					QueuedModeTransition->SetNextMode(DesiredNextModeName, bShouldReenter);
				}
				else
				{
					UE_LOG(LogChaosMover, Warning, TEXT("Attempted to queue an unregistered movement mode: %s on owner %s"), *DesiredNextModeName.ToString(), *OwnerActorName);
				}
			}
		}
	}

	void FMoverStateMachine::SetModeImmediately(FName DesiredModeName, bool bShouldReenter)
	{
		QueueNextMode(DesiredModeName, bShouldReenter);
		AdvanceToNextMode();
	}

	void FMoverStateMachine::ClearQueuedMode()
	{
		TStrongObjectPtr<UImmediateMovementModeTransition> QueuedModeTransition = QueuedModeTransitionWeakPtr.Pin();
		if (QueuedModeTransition)
		{
			QueuedModeTransition->Clear();
		}
	}

	void FMoverStateMachine::OnSimulationTick(const FMoverTimeStep& TimeStep, const FMoverTickStartData& StartState, UMoverBlackboard* SimBlackboard, UMovementMixer* MovementMixer, FMoverTickEndData& OutputState)
	{
		if (!ensure(MovementMixer))
		{
			return;
		}

		InternalSimTimeMs = TimeStep.BaseSimTimeMs;

		//GEngine->AddOnScreenDebugMessage(-1, -0.1f, FColor::White, FString::Printf(TEXT("Mode Tick: %s  Queued: %s"), *CurrentModeName.ToString(), *NextModeName.ToString()));

		FMoverTimeStep SubTimeStep = TimeStep;
		FMoverTickStartData SubstepStartData = StartState;
		FMoverDefaultSyncState* SubstepStartSyncState = SubstepStartData.SyncState.SyncStateCollection.FindMutableDataByType<FMoverDefaultSyncState>();

		TStrongObjectPtr<UImmediateMovementModeTransition> QueuedModeTransition = QueuedModeTransitionWeakPtr.Pin();
		if (QueuedModeTransition && !QueuedModeTransition->IsSet())
		{
			QueueNextMode(SubstepStartData.SyncState.MovementMode);
		}

		AdvanceToNextMode();

		int SubStepCount = 0;
		const int32 MaxConsecutiveFullRefundedSubsteps = GetDefault<UMoverDeveloperSettings>()->MaxTimesToRefundSubstep;
		int32 NumConsecutiveFullRefundedSubsteps = 0;

		float TotalUsedMs = 0.0f;
		while (TotalUsedMs < TimeStep.StepMs)
		{
			InternalSimTimeMs = SubTimeStep.BaseSimTimeMs;

			SubstepStartSyncState = SubstepStartData.SyncState.SyncStateCollection.FindMutableDataByType<FMoverDefaultSyncState>();
			FMoverDefaultSyncState* OutputSyncState = &OutputState.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();
			OutputState.SyncState.MovementMode = CurrentModeName;

			OutputState.MovementEndState.ResetToDefaults();

			SubTimeStep.StepMs = TimeStep.StepMs - TotalUsedMs;		// TODO: convert this to an overridable function that can support MaxStepTime, MaxIterations, etc.

			// Transfer any queued moves into the starting state. They'll be started during the move generation.
			FlushQueuedMovesToGroup(SubstepStartData.SyncState.LayeredMoves);
			OutputState.SyncState.LayeredMoves = SubstepStartData.SyncState.LayeredMoves;

			bool bModeSetFromInstantEffect = false;
			// Apply any instant effects that were queued up between ticks
			if (ApplyInstantEffects(SubstepStartData, SubTimeStep, OutputState.SyncState))
			{
				// Copying over our sync state collection to SubstepStartData so it is effectively the input sync state later for the movement mode. Doing this makes sure state modification from Instant Effects isn't overridden later by the movement mode
				for (auto SyncDataIt = OutputState.SyncState.SyncStateCollection.GetCollectionDataIterator(); SyncDataIt; ++SyncDataIt)
				{
					if (SyncDataIt->Get())
					{
						SubstepStartData.SyncState.SyncStateCollection.AddOrOverwriteData(TSharedPtr<FMoverDataStructBase>(SyncDataIt->Get()->Clone()));
					}
				}

				if (CurrentModeName != OutputState.SyncState.MovementMode)
				{
					bModeSetFromInstantEffect = true;
					SetModeImmediately(OutputState.SyncState.MovementMode);
					SubstepStartData.SyncState.MovementMode = CurrentModeName;
				}
			}

			FMovementModifierGroup& CurrentModifiers = OutputState.SyncState.MovementModifiers;
			TArray<TSharedPtr<FMovementModifierBase>> ActiveModifiers;
			ensureMsgf(!CurrentModifiers.HasAnyMoves(), TEXT("Movement Modifiers are not supported in Mover async mode"));

			FLayeredMoveGroup& CurrentLayeredMoves = OutputState.SyncState.LayeredMoves;

			// Gather any layered move contributions
			FProposedMove CombinedLayeredMove;
			CombinedLayeredMove.MixMode = EMoveMixMode::AdditiveVelocity;
			bool bHasLayeredMoveContributions = false;
			MovementMixer->ResetMixerState();

			TArray<TSharedPtr<FLayeredMoveBase>> ActiveMoves = CurrentLayeredMoves.GenerateActiveMoves_Async(SubTimeStep, SimBlackboard);

			// Tick and accumulate all active moves
			// Gather all proposed moves and distill this into a cumulative movement report. May include separate additive vs override moves.
			// TODO: may want to sort by priority or other factors
			for (TSharedPtr<FLayeredMoveBase>& ActiveMove : ActiveMoves)
			{
				FProposedMove MoveStep;
				bool GenerateMoveResult = ActiveMove->GenerateMove_Async(SubstepStartData, SubTimeStep, SimBlackboard, MoveStep);
				if (GenerateMoveResult)
				{
					// If this active move is already past it's first tick we don't need to set the preferred mode again
					if (ActiveMove->StartSimTimeMs < SubTimeStep.BaseSimTimeMs)
					{
						MoveStep.PreferredMode = NAME_None;
					}

					bHasLayeredMoveContributions = true;
					MovementMixer->MixLayeredMove(*ActiveMove, MoveStep, CombinedLayeredMove);
				}
			}

			if (bHasLayeredMoveContributions && !CombinedLayeredMove.PreferredMode.IsNone() && !bModeSetFromInstantEffect)
			{
				SetModeImmediately(CombinedLayeredMove.PreferredMode);
				OutputState.SyncState.MovementMode = CurrentModeName;
			}

			// Merge proposed movement from the current mode with movement from layered moves
			if (!CurrentModeName.IsNone() && Modes.Contains(CurrentModeName))
			{
				TStrongObjectPtr<UBaseMovementMode> CurrentMode = Modes[CurrentModeName].Pin();
				FProposedMove CombinedMove;
				bool bHasModeMoveContribution = false;

				if (!CVars::bSkipGenerateMoveIfOverridden ||
					!(bHasLayeredMoveContributions && CombinedLayeredMove.MixMode == EMoveMixMode::OverrideAll))
				{
					QUICK_SCOPE_CYCLE_COUNTER(STAT_GenerateMoveFromMode);
					CurrentMode->GenerateMove(SubstepStartData, SubTimeStep, OUT CombinedMove);

					bHasModeMoveContribution = true;
				}

				if (bHasModeMoveContribution && bHasLayeredMoveContributions)
				{
					FVector UpDir = FVector::UpVector;
					if (const FChaosMoverSimulationDefaultInputs* DefaultSimInputs = Simulation->GetLocalSimInput().FindDataByType<FChaosMoverSimulationDefaultInputs>())
					{
						UpDir = DefaultSimInputs->UpDir;
					}

					MovementMixer->MixProposedMoves(CombinedLayeredMove, UpDir, CombinedMove);
				}
				else if (bHasLayeredMoveContributions && !bHasModeMoveContribution)
				{
					CombinedMove = CombinedLayeredMove;
				}

				// Apply any layered move finish velocity settings
				if (CurrentLayeredMoves.bApplyResidualVelocity)
				{
					CombinedMove.LinearVelocity = CurrentLayeredMoves.ResidualVelocity;
				}
				if (CurrentLayeredMoves.ResidualClamping >= 0.0f)
				{
					CombinedMove.LinearVelocity = CombinedMove.LinearVelocity.GetClampedToMaxSize(CurrentLayeredMoves.ResidualClamping);
				}
				CurrentLayeredMoves.ResetResidualVelocity();

				// We need to replace this with some async equivalent (calling back to FSimulation? an optional FinalMoveProcessor object, a bit like the optional MoveMixer?)
				// SyncTickParams->MoverComponent->ProcessGeneratedMovement.ExecuteIfBound(SubstepStartData, SubTimeStep, OUT CombinedMove);

				// Execute the combined proposed move
				{
					FSimulationTickParams SimTickParams;
					// SimTickParams.MovingComps is left empty in the async case, so we don't access resources used by the concurrent gameplay thread
					SimTickParams.SimBlackboard = SimBlackboard;
					SimTickParams.StartState = SubstepStartData;
					SimTickParams.TimeStep = SubTimeStep;
					SimTickParams.ProposedMove = CombinedMove;

					// Check for any transitions, first those registered with the current movement mode, then global ones that could occur from any mode
					FTransitionEvalResult EvalResult = FTransitionEvalResult::NoTransition;
					TStrongObjectPtr<UBaseMovementModeTransition> TransitionToTrigger;

					for (UBaseMovementModeTransition* Transition : CurrentMode->Transitions)
					{
						if (IsValid(Transition) && ((SubStepCount == 0) || !Transition->bFirstSubStepOnly))
						{
							EvalResult = Transition->Evaluate(SimTickParams);

							if (!EvalResult.NextMode.IsNone())
							{
								if (EvalResult.NextMode != CurrentModeName || Transition->bAllowModeReentry)
								{
									TransitionToTrigger = TStrongObjectPtr<UBaseMovementModeTransition>(Transition);
									break;
								}
							}
						}
					}

					if (TransitionToTrigger == nullptr)
					{
						for (TWeakObjectPtr<UBaseMovementModeTransition> TransitionWeakPtr : GlobalTransitions)
						{
							TStrongObjectPtr<UBaseMovementModeTransition> Transition = TransitionWeakPtr.Pin();
							if (Transition)
							{
								EvalResult = Transition->Evaluate(SimTickParams);

								if (!EvalResult.NextMode.IsNone())
								{
									if (EvalResult.NextMode != CurrentModeName || Transition->bAllowModeReentry)
									{
										TransitionToTrigger = Transition;
										break;
									}
								}
							}
						}
					}

					if (TransitionToTrigger && !EvalResult.NextMode.IsNone())
					{
						OutputState.MovementEndState.NextModeName = EvalResult.NextMode;
						OutputState.MovementEndState.RemainingMs = SimTickParams.TimeStep.StepMs; 	// Pass all remaining time to next mode
						TransitionToTrigger->Trigger(SimTickParams);
					}
					else
					{
						CurrentMode->SimulationTick(SimTickParams, OutputState);
					}

					OutputState.MovementEndState.RemainingMs = FMath::Clamp(OutputState.MovementEndState.RemainingMs, 0.0f, SubTimeStep.StepMs);
				}

				QueueNextMode(OutputState.MovementEndState.NextModeName);

				// Check if all of the time for this Substep was refunded
				if (FMath::IsNearlyEqual(SubTimeStep.StepMs, OutputState.MovementEndState.RemainingMs, UE_KINDA_SMALL_NUMBER))
				{
					NumConsecutiveFullRefundedSubsteps++;
					// if we've done this sub step a lot before go ahead and just advance time to avoid freezing editor
					if (NumConsecutiveFullRefundedSubsteps >= MaxConsecutiveFullRefundedSubsteps)
					{
						UE_LOG(LogChaosMover, Warning, TEXT("Movement mode %s and %s on %s are stuck giving time back to each other. Overriding to advance to next substep."),
							*CurrentModeName.ToString(),
							*OutputState.MovementEndState.NextModeName.ToString(),
							*OwnerActorName);
						TotalUsedMs += SubTimeStep.StepMs;
					}
				}
				else
				{
					NumConsecutiveFullRefundedSubsteps = 0;
				}

				//GEngine->AddOnScreenDebugMessage(-1, -0.1f, FColor::White, FString::Printf(TEXT("NextModeName: %s  Queued: %s"), *Output.MovementEndState.NextModeName.ToString(), *NextModeName.ToString()));
			}

			// Switch modes if necessary (note that this will allow exit/enter on the same state)
			AdvanceToNextMode();
			OutputState.SyncState.MovementMode = CurrentModeName;

			ensureMsgf(ActiveModifiers.IsEmpty(), TEXT("Movement Modifiers are not supported in Async mode"));

			const float RemainingMs = FMath::Clamp(OutputState.MovementEndState.RemainingMs, 0.0f, SubTimeStep.StepMs);
			const float SubstepUsedMs = (SubTimeStep.StepMs - RemainingMs);
			SubTimeStep.BaseSimTimeMs += SubstepUsedMs;
			TotalUsedMs += SubstepUsedMs;
			SubTimeStep.StepMs = RemainingMs;

			SubstepStartData.SyncState = OutputState.SyncState;
			SubstepStartData.AuxState = OutputState.AuxState;

			++SubStepCount;
		}

		InternalSimTimeMs = TimeStep.BaseSimTimeMs + TotalUsedMs;

		// Apply any instant effects that were queued up during this tick and didn't get handled in a substep
		if (ApplyInstantEffects(SubstepStartData, SubTimeStep, OutputState.SyncState))
		{
			if (CurrentModeName != OutputState.SyncState.MovementMode)
			{
				SetModeImmediately(OutputState.SyncState.MovementMode);
			}
		}
	}

	void FMoverStateMachine::OnSimulationPreRollback(const FMoverSyncState* InvalidSyncState, const FMoverSyncState* SyncState, const FMoverAuxStateContext* InvalidAuxState, const FMoverAuxStateContext* AuxState)
	{
	}

	void FMoverStateMachine::OnSimulationRollback(const FMoverSyncState* SyncState, const FMoverAuxStateContext* AuxState)
	{
		ClearQueuedMode();
		QueuedLayeredMoves.Empty();
		QueuedInstantEffects.Empty();
	}


	const TWeakObjectPtr<UBaseMovementMode> FMoverStateMachine::GetCurrentMode() const
	{
		if (CurrentModeName != NAME_None && Modes.Contains(CurrentModeName))
		{
			return Modes[CurrentModeName];
		}

		return nullptr;
	}

	const TWeakObjectPtr<UBaseMovementMode> FMoverStateMachine::FindMovementMode(FName ModeName) const
	{
		if (ModeName != NAME_None && Modes.Contains(ModeName))
		{
			return Modes[ModeName];
		}

		return nullptr;
	}

	void FMoverStateMachine::QueueLayeredMove(TSharedPtr<FLayeredMoveBase> Move)
	{
		QueuedLayeredMoves.Add(Move);
	}

	void FMoverStateMachine::QueueInstantMovementEffect(TSharedPtr<FInstantMovementEffect> Effect)
	{
		QueuedInstantEffects.Add(Effect);
	}

	void FMoverStateMachine::ConstructDefaultModes()
	{
		RegisterMovementMode(UNullMovementMode::NullModeName, TObjectPtr<UBaseMovementMode>(NullMovementModeWeakPtr.Get()), /*bIsDefaultMode =*/ true);
		DefaultModeName = NAME_None;
		CurrentModeName = UNullMovementMode::NullModeName;

		QueuedModeTransitionWeakPtr = ImmediateMovementModeTransitionWeakPtr;

		ClearQueuedMode();
	}

	void FMoverStateMachine::AdvanceToNextMode()
	{
		TStrongObjectPtr<UImmediateMovementModeTransition> QueuedModeTransition = QueuedModeTransitionWeakPtr.Pin();
		if (QueuedModeTransition)
		{
			const FName NextModeName = QueuedModeTransition->GetNextModeName();

			if (NextModeName != NAME_None)
			{
				TWeakObjectPtr<UBaseMovementMode>* FoundNextMovementMode = Modes.Find(NextModeName);
				if (FoundNextMovementMode)
				{
					const bool bShouldNextModeReenter = QueuedModeTransition->ShouldReenter();
					if ((CurrentModeName != NextModeName) || bShouldNextModeReenter)
					{
						UE_LOG(LogChaosMover, Verbose, TEXT("AdvanceToNextMode: %s (%s) from %s to %s"),
							*OwnerActorName, *UEnum::GetValueAsString(OwnerActorLocalNetRole), *CurrentModeName.ToString(), *NextModeName.ToString());

						const FName PreviousModeName = CurrentModeName;
						CurrentModeName = NextModeName;

						if (PreviousModeName != NAME_None && Modes.Contains(PreviousModeName))
						{
							Modes[PreviousModeName]->Deactivate();
						}

						Modes[CurrentModeName]->Activate();

						// Notify the simulation of a mode change so it can react accordingly
						if (TStrongObjectPtr<UChaosMoverSimulation> SimStrongObjPtr = Simulation.Pin())
						{
							SimStrongObjPtr->AddEvent(MakeShared<FMovementModeChangedEventData>(InternalSimTimeMs, PreviousModeName, NextModeName));
						}
					}
				}
			}

			ClearQueuedMode();
		}
	}

	void FMoverStateMachine::FlushQueuedMovesToGroup(FLayeredMoveGroup& Group)
	{
		if (!QueuedLayeredMoves.IsEmpty())
		{
			for (TSharedPtr<FLayeredMoveBase>& QueuedMove : QueuedLayeredMoves)
			{
				Group.QueueLayeredMove(QueuedMove);
			}

			QueuedLayeredMoves.Empty();
		}
	}

	bool FMoverStateMachine::ApplyInstantEffects(const FMoverTickStartData& SubstepStartData, const FMoverTimeStep& SubTimeStep, FMoverSyncState& OutputState)
	{
		FApplyMovementEffectParams_Async EffectParams_Async;
		EffectParams_Async.StartState = &SubstepStartData;
		EffectParams_Async.TimeStep = &SubTimeStep;
		EffectParams_Async.Simulation = Simulation.Get();

		bool bInstantMovementEffectApplied = false;

		if (!QueuedInstantEffects.IsEmpty())
		{
			for (TSharedPtr<FInstantMovementEffect>& QueuedEffect : QueuedInstantEffects)
			{
				bInstantMovementEffectApplied |= QueuedEffect->ApplyMovementEffect_Async(EffectParams_Async, OutputState);
			}

			QueuedInstantEffects.Empty();
		}

		return bInstantMovementEffectApplied;
	}

	void FMoverStateMachine::SetOwnerActorName(const FString& InOwnerActorName)
	{
		OwnerActorName = InOwnerActorName;
	}
	
	void FMoverStateMachine::SetOwnerActorLocalNetRole(ENetRole InOwnerActorLocalNetRole)
	{
		OwnerActorLocalNetRole = InOwnerActorLocalNetRole;
	}

} // End of namespace UE::ChaosMover