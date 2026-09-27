// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Async/TaskGraphFwd.h"			// FGraphEventRef
#include "AsyncMessageSystemBase.h"
#include "Engine/EngineBaseTypes.h"		// For ELevelTick

#define UE_API ASYNCMESSAGESYSTEM_API

class UWorld;

/**
 * Implementation of an async message system which schedules the processing of messages based on tick groups
 * and named thread async tasks, making it integrate nicely with common gameplay frameworks in Unreal.
 */
class FAsyncGameplayMessageSystem final :
	public FAsyncMessageSystemBase
{
	friend struct FMessageSystemTickFunction;

public:
	UE_API explicit FAsyncGameplayMessageSystem(UWorld* OwningWorld);

private:
	//~ Begin FAsyncMessageSystemBase interface
	UE_API virtual void Startup_Impl() override;
	UE_API virtual void Shutdown_Impl() override;
	UE_API virtual void PostQueueMessage(const FAsyncMessageId MessageId, const TArray<FAsyncMessageBindingOptions>& OptionsBoundTo) override;
	//~ End FAsyncMessageSystemBase interface

	/**
	 * Creates and registers tick functions for this message system to the outer world.
	 */
	UE_API void CreateTickFunctions();

	/**
	 * Destroy any registered tick functions on this message system
	 */
	UE_API void DestroyTickFunctions();

	/**
	 * Starts an async task to process the messages with listeners that have the given binding options.
	 * This should ONLY be called for binding options which are not tick groups.
	 * 
	 * @param Options The options of which to start async processing for.
	 */
	UE_API void StartAsyncProcessForBinding(const FAsyncMessageBindingOptions& Options);
	
	/**
	 * Executes a single tick of this message system for a given group 
	 */
	UE_API void ExecuteTick(
		float DeltaTime,
		ELevelTick TickType,
		ENamedThreads::Type CurrentThread,
		const FGraphEventRef& MyCompletionGraphEvent,
		ETickingGroup TickGroup);

	/**
	 * The world in which this message system belongs to. The tick functions of this message system will
	 * be registered to this world's persistent level.
	 */
	TWeakObjectPtr<UWorld> OuterWorld = nullptr;

	/**
	 * Tick functions which are created when this message system starts. There is one tick functions
	 * for each tick group that this message system supports and they will depend on one another
	 * to keep a deterministic ordering of ticking on this message system.
	 */
	TArray<TSharedPtr<FTickFunction>> TickFunctions;

	/***
	 * The tick group earliest in the frame which this message system supports.
	 */
	static constexpr ETickingGroup EarliestSupportedTickGroup = TG_PrePhysics;
	
	/**
	 * The tick group that is the latest in frame time which this message system will support
	 */
	static constexpr ETickingGroup LatestSupportedTickGroup = TG_PostUpdateWork;

	/**
	 * Keep track of the state of what tick group is currently executing and what the last one was.
	 */
	ETickingGroup CurrentTickGroup = EarliestSupportedTickGroup;
	ETickingGroup LastTickedGroup = LatestSupportedTickGroup;
};

#undef UE_API
