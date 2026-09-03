// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassArchetypeTypes.h"
#include "Async/TaskGraphInterfaces.h"


struct FMassRuntimePipeline;
namespace UE::Mass
{
	struct FProcessingContext;
}
struct FMassEntityHandle;
struct FMassArchetypeEntityCollection;
class UMassProcessor;


namespace UE::Mass::Executor
{
	/** Executes processors in a given RuntimePipeline */
	MASSENTITY_API void Run(FMassRuntimePipeline& RuntimePipeline, FProcessingContext& ProcessingContext);

	/** Executes given Processor. Used mainly for triggering calculations via MassCompositeProcessors, e.g processing phases */
	MASSENTITY_API void Run(UMassProcessor& Processor, FProcessingContext& ProcessingContext);

	/** Similar to the Run function, but instead of using all the entities hosted by ProcessingContext.EntitySubsystem 
	 *  it is processing only the entities given by EntityID via the Entities input parameter. 
	 *  Note that all the entities need to be of Archetype archetype. 
	 *  Under the hood the function converts Archetype-Entities pair to FMassArchetypeEntityCollection and calls the other flavor of RunSparse
	 */
	MASSENTITY_API void RunSparse(FMassRuntimePipeline& RuntimePipeline, FProcessingContext& ProcessingContext, FMassArchetypeHandle Archetype, TConstArrayView<FMassEntityHandle> Entities);

	/** Similar to the Run function, but instead of using all the entities hosted by ProcessingContext.EntitySubsystem 
	 *  it is processing only the entities given by SparseEntities input parameter.
	 *  @todo rename
	 */
	MASSENTITY_API void RunSparse(FMassRuntimePipeline& RuntimePipeline, FProcessingContext& ProcessingContext, const FMassArchetypeEntityCollection& EntityCollection);

	/** Executes given Processors array view. This function gets called under the hood by the rest of Run* functions */
	MASSENTITY_API void RunProcessorsView(TArrayView<UMassProcessor* const> Processors, FProcessingContext& ProcessingContext, TConstArrayView<FMassArchetypeEntityCollection> EntityCollections = {});

	/** 
	 *  Triggers tasks executing Processor (and potentially it's children) and returns the task graph event representing 
	 *  the task (the event will be "completed" once all the processors finish running). 
	 *  @param OnDoneNotification will be called after all the processors are done, just after flushing the command buffer.
	 *    Note that OnDoneNotification will be executed on GameThread.
	 */
	MASSENTITY_API FGraphEventRef TriggerParallelTasks(UMassProcessor& Processor, FProcessingContext&& ProcessingContext, TFunction<void()> OnDoneNotification
		, ENamedThreads::Type CurrentThread = ENamedThreads::GameThread);

	UE_DEPRECATED(5.5, "This flavor of RunProcessorsView is deprecated. Use the one with TConstArrayView<FMassArchetypeEntityCollection> parameter instead.")
	MASSENTITY_API void RunProcessorsView(TArrayView<UMassProcessor* const> Processors, FProcessingContext& ProcessingContext, const FMassArchetypeEntityCollection* EntityCollection);

	UE_DEPRECATED(5.6, "lvalue flavor of TriggerParallelTasks has been deprevate. Use the rvalue version.")
	MASSENTITY_API FGraphEventRef TriggerParallelTasks(UMassProcessor& Processor, FProcessingContext& ProcessingContext, TFunction<void()> OnDoneNotification
		, ENamedThreads::Type CurrentThread = ENamedThreads::GameThread);
};
