// Copyright Epic Games, Inc. All Rights Reserved.

#include "AsyncGameplayMessageSystem.h"

#include "Async/Async.h"
#include "AsyncMessageSystemLogs.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "Tasks/TaskPrivate.h"

namespace UE::Private
{
	static FString LexToString(const ETickingGroup Group)
	{
		static FString Invalid = TEXT("Invalid");
		static UEnum* TGEnum = StaticEnum<ETickingGroup>();
		
		return TGEnum ? TGEnum->GetDisplayNameTextByValue(Group).ToString() : Invalid;
	};
}

/**
 * Tick function which will being the processing of messages for specific tick groups on the message system. 
 */
struct FMessageSystemTickFunction final : public FTickFunction
{
	friend class FAsyncGameplayMessageSystem;
	
	explicit FMessageSystemTickFunction(const ETickingGroup InGroup, TWeakPtr<FAsyncGameplayMessageSystem> InWeakMessageSystem)
		: FTickFunction()
		, WeakMessageSys(InWeakMessageSystem)
	{
		bCanEverTick = true;
		bStartWithTickEnabled = true;
		bAllowTickBatching = true;

		// Only run this tick function on the game thread, because our message system is our "sync point"
		// for everything. 
		bRunOnAnyThread = false;

		// We want to ensure that we start and end in the same tick group to make sure we are a valid sync point for other threads
		TickGroup = InGroup;
		EndTickGroup = TickGroup;
	}

	// FTickFunction interface
	virtual void ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent) override
	{
		// Each tick function can simply call the message system and let it know that the next tick group has started.
		if (TSharedPtr<FAsyncGameplayMessageSystem> MessageSys = WeakMessageSys.Pin())
		{
			MessageSys->ExecuteTick(DeltaTime, TickType, CurrentThread, MyCompletionGraphEvent, TickGroup);
		}
	}
	
	virtual FString DiagnosticMessage() override
	{
		return TEXT("FMessageSystemTickFunction::") + UE::Private::LexToString(TickGroup);
	}

	/**
	 * The owning message system which this tick function is going to update
	 */
	TWeakPtr<FAsyncGameplayMessageSystem> WeakMessageSys = nullptr;
};

FAsyncGameplayMessageSystem::FAsyncGameplayMessageSystem(UWorld* OwningWorld)
	: OuterWorld(MakeWeakObjectPtr<UWorld>(OwningWorld))
{
	
}

void FAsyncGameplayMessageSystem::Startup_Impl()
{
	check(OuterWorld.IsValid());
	
	// Create a tick function for each tick group
	CreateTickFunctions();
}

void FAsyncGameplayMessageSystem::Shutdown_Impl()
{
	// Remove all tick groups and wait for them to finish
	DestroyTickFunctions();
}

void FAsyncGameplayMessageSystem::PostQueueMessage(const FAsyncMessageId MessageId, const TArray<FAsyncMessageBindingOptions>& OptionsBoundTo)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FMessageSystem::PostQueueMessage);

	// When we queue a message, check if there are any listeners outside of tick groups who would need a specific
	// async task to process their message queue. 
	for (const FAsyncMessageBindingOptions& BindingOpts : OptionsBoundTo)
	{
		if (BindingOpts.GetType() == FAsyncMessageBindingOptions::EBindingType::UseNamedThreads || BindingOpts.GetType() == FAsyncMessageBindingOptions::EBindingType::UseTaskPriorities)
		{
			StartAsyncProcessForBinding(BindingOpts);
		}

		// TODO: We could potentially use this to conditionally control when the tick groups need to run as well, reducing the amount that they are called.
		// That way, the tick functions only execute if there are currently messages in their queue. We could possibly do this by registering/unregistering the
		// tick functions as needed for the tick groups as they run. For now though, just let them tick
	}
}

void FAsyncGameplayMessageSystem::CreateTickFunctions()
{
	check(TickFunctions.IsEmpty());

	if (!ensureMsgf(OuterWorld.IsValid(), TEXT("Failed to create message system tick functions, the outer world is invalid!")))
	{
		return;
	}

	// We will be binding to some owning world's persistent level to create our tick functions
	ULevel* TickLevel = OuterWorld->PersistentLevel;

	TWeakPtr<FAsyncGameplayMessageSystem> WeakThisPtr = StaticCastWeakPtr<FAsyncGameplayMessageSystem>(this->AsWeak());

	// Track the previous tick function so that we can add it as a dependency for each to finish
	TSharedPtr<FTickFunction> PreviousTickFunction = nullptr;

	const int StartingGroup = static_cast<int>(EarliestSupportedTickGroup);
	const int LastGroup = static_cast<int>(LatestSupportedTickGroup);

	// Spawn a tick function for every tick group that we can actually do any work in
	for (int i = StartingGroup; i <= LastGroup; ++i)
	{
		const ETickingGroup Group = static_cast<ETickingGroup>(i);

		TSharedPtr<FTickFunction> Func = MakeShared<FMessageSystemTickFunction>(Group, WeakThisPtr);

		Func->RegisterTickFunction(TickLevel);

		// We always want the previous tick function to wait until the next one to start processing
		if (PreviousTickFunction.IsValid())
		{
			Func->AddPrerequisite(TickLevel, *PreviousTickFunction);
		}

		PreviousTickFunction = Func;

		// Keep track of the tick functions we have created so that we can properly unregister them later
		TickFunctions.Emplace(MoveTemp(Func));
	}
}

void FAsyncGameplayMessageSystem::DestroyTickFunctions()
{
	for (TSharedPtr<FTickFunction> Func : TickFunctions)
	{
		if (Func.IsValid())
		{
			Func->UnRegisterTickFunction();
		}
	}

	TickFunctions.Empty();
}

void FAsyncGameplayMessageSystem::StartAsyncProcessForBinding(const FAsyncMessageBindingOptions& Options)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FMessageSystem::StartAsyncProcessForBinding);

	// We only want to do this for task ID's and priorities. Tick groups are already being processed via our tick functions
	check(
		Options.GetType() == FAsyncMessageBindingOptions::EBindingType::UseNamedThreads ||
		Options.GetType() == FAsyncMessageBindingOptions::EBindingType::UseTaskPriorities);

	// Kick off a weak lambda to process the message queue for this task graph ID, ensuring
	// that any listeners bound to these options will get the message called back when they expect
	TWeakPtr<FAsyncGameplayMessageSystem> WeakThisPtr = StaticCastWeakPtr<FAsyncGameplayMessageSystem>(this->AsWeak());
	
	UE::Tasks::ETaskPriority TaskPri = Options.GetTaskPriority();
	UE::Tasks::EExtendedTaskPriority ExtendedTaskPri = Options.GetExtendedTaskPriority();

	// Translate from the old named thread model to the newer UE Tasks model if we need to
	if (Options.GetType() == FAsyncMessageBindingOptions::EBindingType::UseNamedThreads)
	{
		const ENamedThreads::Type ThreadToProcessOn = Options.GetNamedThreads();
		UE::Tasks::Private::TranslatePriority(ThreadToProcessOn, OUT TaskPri, OUT ExtendedTaskPri);
	}
	
	UE::Tasks::Launch(UE_SOURCE_LOCATION, [WeakThisPtr, Options]()
    {
    	if (TSharedPtr<FAsyncGameplayMessageSystem> This = WeakThisPtr.Pin())
    	{
    		This->ProcessMessageQueueForBinding(Options);
    	}
    },
    TaskPri,
    ExtendedTaskPri);
	
	// The above UE::Tasks::Launch is the same thing as this older Task Graph syntax:
	// AsyncTask(ThreadToProcessOn, [WeakThisPtr, Options]()
	// {
	// 	if (TSharedPtr<FAsyncGameplayMessageSystem> This = WeakThisPtr.Pin())
	// 	{
	// 		This->ProcessMessageQueueForBinding(Options);
	// 	}
	// });
	//
	// We utilize the UE::Tasks system here and translate the ENamedThread because the UE::Tasks system
	// should have less overhead and better scheduling capabilities in most scenarios
}

void FAsyncGameplayMessageSystem::ExecuteTick(
	float DeltaTime,
	ELevelTick TickType,
	ENamedThreads::Type CurrentThread,
	const FGraphEventRef& MyCompletionGraphEvent,
	ETickingGroup TickGroup)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FMessageSystem::ExecuteTick);

	if (!OuterWorld.IsValid())
	{
		UE_LOG(LogAsyncMessageSystem, Warning, TEXT("[%hs] OuterWorld weak pointer is no longer valid for message system. Messages will not be processed, and this system will be shut down."), __func__);
		Shutdown();
		return;
	}
	
	CurrentTickGroup = TickGroup;

	// TODO: Right now the message system uses GFrameCounter as a quick and easy way to determine when messages are sent
	// We should get away from that, and instead use an atomic in in the message system itself and increment it once per frame
	
	// If we are in the last tick group of the frame, then increment the frame counter of the message system that it is using.
	//const bool bIsLastGroupOfFrame = (CurrentTickGroup == LatestSupportedTickGroup);
	const uint64 CurrentFrame = GFrameCounter;
	
	UE_LOG(LogAsyncMessageSystem, VeryVerbose, TEXT("Frame %llu Exectute tick %s :: Last tick group was %s"),
		CurrentFrame,
		*UE::Private::LexToString(CurrentTickGroup),
		*UE::Private::LexToString(LastTickedGroup));

	ensure(CurrentTickGroup >= EarliestSupportedTickGroup || CurrentTickGroup <= LatestSupportedTickGroup);	

	// Process the messages for this current tick group
	FAsyncMessageBindingOptions Options = {};
	Options.SetTickGroup(CurrentTickGroup);
	
	ProcessMessageQueueForBinding(Options);
		
	LastTickedGroup = TickGroup;
}