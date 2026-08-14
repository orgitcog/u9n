// Copyright Epic Games, Inc. All Rights Reserved.

#include "Module/ModuleTickFunction.h"

#include "AnimNextDebugDraw.h"
#include "AnimNextStats.h"
#include "RigVMRuntimeDataRegistry.h"
#include "Algo/TopologicalSort.h"
#include "Component/AnimNextWorldSubsystem.h"
#include "Module/AnimNextModule.h"
#include "Module/ModuleTaskContext.h"
#include "Module/ProxyVariablesContext.h"
#include "Module/RigUnit_AnimNextModuleEvents.h"
#include "Module/ModuleEvents.h"
#include "TraitCore/TraitEventList.h"
#include "Module/ModuleGuard.h"


namespace UE::AnimNext
{

void FModuleEventTickFunction::Initialize(float InDeltaTime)
{
	ModuleInstance->RunRigVMEvent(FRigUnit_AnimNextInitializeEvent::EventName, InDeltaTime);
}

void FModuleEventTickFunction::ExecuteBindings_WT(float InDeltaTime)
{
	ModuleInstance->RunRigVMEvent(FRigUnit_AnimNextExecuteBindings_WT::EventName, InDeltaTime);
}

void FModuleEventTickFunction::EndTick(float DeltaTime)
{
	SCOPED_NAMED_EVENT(AnimNext_Module_EndTick, FColor::Orange);

	FModuleWriteGuard Guard(ModuleInstance);

	// Give module a chance to finish up processing
	ModuleInstance->EndExecution(DeltaTime);

	// Decrement the remaining lifetime of the input events we processed and queue up any remaining events
	DecrementLifetimeAndPurgeExpired(ModuleInstance->InputEventList, ModuleInstance->OutputEventList);

	// Filter out our schedule action events, we'll hand them off to the main thread to execute
	FTraitEventList MainThreadActionEventList;
	if (!ModuleInstance->OutputEventList.IsEmpty())
	{
		for (FAnimNextTraitEventPtr& Event : ModuleInstance->OutputEventList)
		{
			if (!Event->IsValid())
			{
				continue;
			}

			if (FAnimNextModule_ActionEvent* ActionEvent = Event->AsType<FAnimNextModule_ActionEvent>())
			{
				if (ActionEvent->IsThreadSafe())
				{
					// Execute this action now
					ActionEvent->Execute();
				}
				else
				{
					// Defer this action and execute it on the main thread
					MainThreadActionEventList.Push(Event);
				}
			}
		}

		// Reset our list of output events, we don't retain any
		ModuleInstance->OutputEventList.Reset();
	}

	if(ModuleInstance->InitState == FAnimNextModuleInstance::EInitState::FirstUpdate)
	{
		ModuleInstance->TransitionToInitState(FAnimNextModuleInstance::EInitState::Initialized);

		if(ModuleInstance->InitMethod == EAnimNextModuleInitMethod::InitializeAndPause
#if WITH_EDITOR
			|| (ModuleInstance->InitMethod == EAnimNextModuleInitMethod::InitializeAndPauseInEditor && ModuleInstance->WorldType == EWorldType::Editor)
#endif
		)
		{
			// Queue task to disable ourselves
			FAnimNextModuleInstance::RunTaskOnGameThread([ModuleHandle = ModuleInstance->Handle, WeakObject = TWeakObjectPtr<UObject>(ModuleInstance->Object)]()
			{
				check(IsInGameThread());
				if (UObject* Object = WeakObject.Get())
				{
					UAnimNextWorldSubsystem* WorldSubsystem = UAnimNextWorldSubsystem::Get(Object);
					WorldSubsystem->EnableHandle(ModuleHandle, false);
				}
			});
		}
	}

	if (!MainThreadActionEventList.IsEmpty())
	{
		FAnimNextModuleInstance::RunTaskOnGameThread([MainThreadActionEventList = MoveTemp(MainThreadActionEventList)]()
			{
				SCOPED_NAMED_EVENT(AnimNext_Module_EndTick_GameThread, FColor::Orange);
				check(IsInGameThread());
				for (const FAnimNextTraitEventPtr& Event : MainThreadActionEventList)
				{
					FAnimNextModule_ActionEvent* ActionEvent = Event->AsType<FAnimNextModule_ActionEvent>();
					ActionEvent->Execute();
				}
			});
	}

#if UE_ENABLE_DEBUG_DRAWING
	// Perform any debug drawing
	ModuleInstance->DebugDraw->Draw();
#endif
}

void FModuleEventTickFunction::ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	Run(DeltaTime);
}

void FModuleEventTickFunction::Run(float InDeltaTime)
{
	SCOPED_NAMED_EVENT(AnimNext_Module_EventTick, FColor::Orange);

	FModuleWriteGuard Guard(ModuleInstance);

	if (bFirstUserEvent)
	{
		if (bRunBindingsEvent)
		{
			// Execute any WT bindings
			ExecuteBindings_WT(InDeltaTime);
		}

		// Run the pending initialize if required
		if (ModuleInstance->InitState == FAnimNextModuleInstance::EInitState::PendingInitializeEvent)
		{
			Initialize(InDeltaTime);
			ModuleInstance->TransitionToInitState(FAnimNextModuleInstance::EInitState::FirstUpdate);
		}
	}

	while (!PreExecuteTasks.IsEmpty())
	{
		TOptional<TUniqueFunction<void(const FModuleTaskContext&)>> Function = PreExecuteTasks.Dequeue();
		check(Function.IsSet());
		Function.GetValue()(FModuleTaskContext(*ModuleInstance));
	}

	ModuleInstance->RaiseTraitEvents(ModuleInstance->InputEventList);

	OnPreModuleEvent.Broadcast(FModuleTaskContext(*ModuleInstance));

	ModuleInstance->RunRigVMEvent(EventName, InDeltaTime);

	ModuleInstance->RaiseTraitEvents(ModuleInstance->OutputEventList);

	while (!PostExecuteTasks.IsEmpty())
	{
		TOptional<TUniqueFunction<void(const FModuleTaskContext&)>> Function = PostExecuteTasks.Dequeue();
		check(Function.IsSet());
		Function.GetValue()(FModuleTaskContext(*ModuleInstance));
	}

	if (bLastUserEvent)
	{
		EndTick(InDeltaTime);
	}
}

#if WITH_EDITOR

void FModuleEventTickFunction::InitializeAndRunModule(FAnimNextModuleInstance& InModuleInstance)
{
	// Run sorted tick functions
	for(FModuleEventTickFunction& TickFunction : InModuleInstance.TickFunctions)
	{
		TickFunction.Run(0.0f);
	}
}

#endif

void FModuleEventTickFunction::AddSubsequent(UObject* InObject, FTickFunction& InTickFunction)
{
	auto FindExistingSubsequent = [InObject, &InTickFunction](const FTickPrerequisite& InSubsequent)
	{
		return InSubsequent.PrerequisiteObject == InObject && InSubsequent.PrerequisiteTickFunction == &InTickFunction;
	};

	InTickFunction.AddPrerequisite(ModuleInstance->GetObject(), *this);
	if(!ExternalSubsequents.ContainsByPredicate(FindExistingSubsequent))
	{
		ExternalSubsequents.Emplace(InObject, InTickFunction);
	}
}

void FModuleEventTickFunction::RemoveSubsequent(UObject* InObject, FTickFunction& InTickFunction)
{
	auto FindExistingSubsequent = [InObject, &InTickFunction](const FTickPrerequisite& InSubsequent)
	{
		return InSubsequent.PrerequisiteObject == InObject && InSubsequent.PrerequisiteTickFunction == &InTickFunction;
	};

	InTickFunction.RemovePrerequisite(ModuleInstance->GetObject(), *this);
	ExternalSubsequents.RemoveAllSwap(FindExistingSubsequent, EAllowShrinking::No);
}

void FModuleEventTickFunction::RemoveAllExternalSubsequents()
{
	for(FTickPrerequisite& Subsequent : ExternalSubsequents)
	{
		if(FTickFunction* TickFunction = Subsequent.Get())
		{
			TickFunction->RemovePrerequisite(ModuleInstance->GetObject(), *this);
		}
	}

	ExternalSubsequents.Reset();
}

FString FModuleEventTickFunction::DiagnosticMessage()
{
	TStringBuilder<256> Builder;
	Builder.Append(TEXT("AnimNext: "));
	EventName.AppendString(Builder);
	return Builder.ToString();
}

}
