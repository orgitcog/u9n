// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassObserverManager.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "MassExecutor.h"
#include "MassProcessingTypes.h"
#include "MassObserverRegistry.h"
#include "MassDebugger.h"
#include "MassEntityCollection.h"
#include "MassProcessingContext.h"
#include "MassObserverNotificationTypes.h"
#include "VisualLogger/VisualLogger.h"


#include UE_INLINE_GENERATED_CPP_BY_NAME(MassObserverManager)

namespace UE::Mass::ObserverManager
{
	namespace Tweakables
	{
		// Used as a template parameter for TInlineAllocator that we use when gathering UScriptStruct* of the observed types to process.
		constexpr int InlineAllocatorElementsForOverlapTypes = 8;
	} // Tweakables

	bool bCoalesceBufferedNotifications = false;

	namespace Private
	{
		FAutoConsoleVariableRef ConsoleVariables[] =
		{
			FAutoConsoleVariableRef(TEXT("mass.observers.CoalesceBufferedNotifications"), bCoalesceBufferedNotifications
				, TEXT("If enabled, when buffering new notification we'll check if it's the same type as the previously stored one, and if so then merge the two."), ECVF_Default)
		};

		// a helper function to reduce code duplication in FMassObserverManager::Initialize
		template<typename TBitSet, typename TPointerType>
		void AddRegisteredObserverProcessorInstances(FMassEntityManager& EntityManager, const EProcessorExecutionFlags WorldExecutionFlags, UObject& Owner
			, const TMap<TPointerType, FMassProcessorClassCollection>& RegisteredObserverTypes, TBitSet& InOutObservedBitSet, FMassObserversMap& Observers)
		{
			for (auto It : RegisteredObserverTypes)
			{
				if (It.Value.ClassCollection.Num() == 0)
				{
					continue;
				}

				InOutObservedBitSet.Add(*It.Key);
				FMassRuntimePipeline& Pipeline = (*Observers).FindOrAdd(It.Key);

				for (const TSubclassOf<UMassProcessor>& ProcessorClass : It.Value.ClassCollection)
				{
					if (ProcessorClass->GetDefaultObject<UMassProcessor>()->ShouldExecute(WorldExecutionFlags))
					{
						Pipeline.AppendProcessor(ProcessorClass, Owner);
					}
				}
				Pipeline.Initialize(Owner, EntityManager.AsShared());
			}
		};
	} // Private

	struct FDeprecationHelper
	{
		static void HandleSingleElement(TNotNull<FMassObserverManager*> ObserverManager, const UScriptStruct& ElementType, const FMassArchetypeEntityCollection& EntityCollection, FMassObserversMap& HandlersContainer)
		{
			UE::Mass::FProcessingContext ProcessingContext(ObserverManager->EntityManager);
			ProcessingContext.AuxData.InitializeAs(&ElementType);
			FMassObserverManager::HandleElementsImpl(ProcessingContext, { EntityCollection }, MakeArrayView((const UScriptStruct**)&ElementType, 1), HandlersContainer);
		}

		static void HandleSingleElement(TNotNull<FMassObserverManager*> ObserverManager, const UScriptStruct& ElementType, const FMassArchetypeEntityCollection& EntityCollection, const EMassObservedOperation Operation)
		{
			const int32 OperationIndex = int32(Operation);
			const bool bIsFragment = UE::Mass::IsA<FMassFragment>(&ElementType);
			check(bIsFragment || UE::Mass::IsA<FMassTag>(&ElementType));

			HandleSingleElement(ObserverManager, ElementType, EntityCollection
				, bIsFragment ? ObserverManager->FragmentObservers[OperationIndex] : ObserverManager->TagObservers[OperationIndex]);
		}
	};

	struct FNotificationContext
	{
		FMassObserverManager& ObserverManager;
		FProcessingContext& ProcessingContext;
	};

	struct FBufferedNotificationExecutioner 
	{
		FBufferedNotificationExecutioner(FNotificationContext& InNotificationContext, const EMassObservedOperation InOpType)
			: NotificationContext(InNotificationContext), OpType(InOpType)
		{}

		template<typename TEntities>
		void operator()(const FBufferedNotification::FEmptyComposition&, TEntities)
		{
			// no-op
		}

		void operator()(const FMassArchetypeCompositionDescriptor& Change, const FEntityCollection& Entities)
		{
			if (Change.Fragments.IsEmpty() == false)
			{
				(*this)(Change.Fragments, Entities);
			}
			if (Change.Tags.IsEmpty() == false)
			{
				(*this)(Change.Tags, Entities);
			}
		}

		void operator()(const FMassArchetypeCompositionDescriptor& Change, const FMassEntityHandle EntityHandle)
		{
			if (Change.Fragments.IsEmpty() == false)
			{
				(*this)(Change.Fragments, EntityHandle);
			}
			if (Change.Tags.IsEmpty() == false)
			{
				(*this)(Change.Tags, EntityHandle);
			}
		}

		void operator()(const FMassFragmentBitSet& Change, const FEntityCollection& Entities)
		{
			ObservedTypesOverlap.Reset();
			Change.ExportTypes(ObservedTypesOverlap);
			FMassObserverManager::HandleElementsImpl(NotificationContext.ProcessingContext
				, Entities.GetUpToDatePerArchetypeCollections(NotificationContext.ObserverManager.EntityManager)
				, ObservedTypesOverlap
				, NotificationContext.ObserverManager.FragmentObservers[static_cast<uint8>(OpType)]);
		}

		void operator()(const FMassFragmentBitSet& Change, const FMassEntityHandle EntityHandle)
		{
			ObservedTypesOverlap.Reset();
			Change.ExportTypes(ObservedTypesOverlap);

			FMassArchetypeHandle ArchetypeHandle = NotificationContext.ObserverManager.GetEntityManager().GetArchetypeForEntity(EntityHandle);
			NotificationContext.ObserverManager.HandleFragmentsImpl(NotificationContext.ProcessingContext
				, FMassArchetypeEntityCollection(MoveTemp(ArchetypeHandle), EntityHandle)
				, ObservedTypesOverlap
				, NotificationContext.ObserverManager.FragmentObservers[static_cast<uint8>(OpType)]);
		}

		void operator()(const FMassTagBitSet& Change, const FEntityCollection& Entities)
		{
			ObservedTypesOverlap.Reset();
			Change.ExportTypes(ObservedTypesOverlap);
			FMassObserverManager::HandleElementsImpl(NotificationContext.ProcessingContext
				, Entities.GetUpToDatePerArchetypeCollections(NotificationContext.ObserverManager.EntityManager)
				, ObservedTypesOverlap
				, NotificationContext.ObserverManager.TagObservers[static_cast<uint8>(OpType)]);
		}

		void operator()(const FMassTagBitSet& Change, const FMassEntityHandle EntityHandle)
		{
			ObservedTypesOverlap.Reset();
			Change.ExportTypes(ObservedTypesOverlap);

			FMassArchetypeHandle ArchetypeHandle = NotificationContext.ObserverManager.GetEntityManager().GetArchetypeForEntity(EntityHandle);
			NotificationContext.ObserverManager.HandleFragmentsImpl(NotificationContext.ProcessingContext
				, FMassArchetypeEntityCollection(MoveTemp(ArchetypeHandle), EntityHandle)
				, ObservedTypesOverlap
				, NotificationContext.ObserverManager.TagObservers[static_cast<uint8>(OpType)]);
		}

		TArray<const UScriptStruct*, TInlineAllocator<ObserverManager::Tweakables::InlineAllocatorElementsForOverlapTypes>> ObservedTypesOverlap;
		FNotificationContext& NotificationContext;
		EMassObservedOperation OpType;
	};

	struct FBufferedCreationNotificationExecutioner
	{
		explicit FBufferedCreationNotificationExecutioner(FNotificationContext& InNotificationContext)
			: NotificationContext(InNotificationContext)
		{}

		void operator()(FEntityCollection&& Entities) const
		{
			NotificationContext.ObserverManager.OnCollectionsCreatedImpl(NotificationContext.ProcessingContext
				, MoveTemp(Entities).ConsumeArchetypeCollections(NotificationContext.ObserverManager.EntityManager));
		}

		void operator()(const FMassEntityHandle EntityHandle) const
		{
			const FMassArchetypeHandle ArchetypeHandle = NotificationContext.ObserverManager.GetEntityManager().GetArchetypeForEntity(EntityHandle);
			const FMassArchetypeCompositionDescriptor& ArchetypeComposition = NotificationContext.ProcessingContext.GetEntityManager()->GetArchetypeComposition(ArchetypeHandle);
			NotificationContext.ObserverManager.OnCompositionChanged(EntityHandle, ArchetypeComposition, EMassObservedOperation::Add, &NotificationContext.ProcessingContext);
		}
		FNotificationContext& NotificationContext;
	};
} // UE::Mass::ObserverManager

//----------------------------------------------------------------------//
// FMassObserversMap
//----------------------------------------------------------------------//
void FMassObserversMap::DebugAddUniqueProcessors(TArray<const UMassProcessor*>& OutProcessors) const
{
#if WITH_MASSENTITY_DEBUG
	for (const auto& MapElement : Container)
	{
		for (const UMassProcessor* Processor : MapElement.Value.GetProcessorsView())
		{
			ensure(Processor);
			OutProcessors.AddUnique(Processor);
		}
	}
#endif // WITH_MASSENTITY_DEBUG
}

//----------------------------------------------------------------------//
// FMassObserverManager
//----------------------------------------------------------------------//
FMassArchetypeEntityCollection FMassObserverManager::FCollectionRefOrHandle::DummyCollection;

FMassObserverManager::FMassObserverManager()
	: EntityManager(GetMutableDefault<UMassEntitySubsystem>()->GetMutableEntityManager())
{

}

FMassObserverManager::FMassObserverManager(FMassEntityManager& Owner)
	: EntityManager(Owner)
{

}

void FMassObserverManager::DebugGatherUniqueProcessors(TArray<const UMassProcessor*>& OutProcessors) const
{
#if WITH_MASSENTITY_DEBUG
	for (int32 OperationIndex = 0; OperationIndex < static_cast<uint32>(EMassObservedOperation::MAX); ++OperationIndex)
	{
		FragmentObservers[OperationIndex].DebugAddUniqueProcessors(OutProcessors);
		TagObservers[OperationIndex].DebugAddUniqueProcessors(OutProcessors);
	}
#endif // WITH_MASSENTITY_DEBUG
}

void FMassObserverManager::Initialize()
{
	// instantiate initializers
	const UMassObserverRegistry& Registry = UMassObserverRegistry::Get();

	UObject* Owner = EntityManager.GetOwner();
	check(Owner);
	const UWorld* World = Owner->GetWorld();
	const EProcessorExecutionFlags WorldExecutionFlags = UE::Mass::Utils::DetermineProcessorExecutionFlags(World);

	using UE::Mass::ObserverManager::Private::AddRegisteredObserverProcessorInstances;
	for (int i = 0; i < (int)EMassObservedOperation::MAX; ++i)
	{
		AddRegisteredObserverProcessorInstances(EntityManager, WorldExecutionFlags, *Owner, *Registry.FragmentObservers[i], ObservedFragments[i], FragmentObservers[i]);
		AddRegisteredObserverProcessorInstances(EntityManager, WorldExecutionFlags, *Owner, *Registry.TagObservers[i], ObservedTags[i], TagObservers[i]);
	}

#if WITH_MASSENTITY_DEBUG
	FMassDebugger::RegisterProcessorDataProvider(TEXT("Observers"), EntityManager.AsShared()
		, [WeakManager = EntityManager.AsWeak()](TArray<const UMassProcessor*>& OutProcessors)
	{
		if (TSharedPtr<FMassEntityManager> SharedEntityManager = WeakManager.Pin())
		{
			FMassObserverManager& ObserverManager = SharedEntityManager->GetObserverManager();
			ObserverManager.DebugGatherUniqueProcessors(OutProcessors);
		}
	});
#endif // WITH_MASSENTITY_DEBUG
}

void FMassObserverManager::DeInitialize()
{
	for (int32 i = 0; i < (int32)EMassObservedOperation::MAX; ++i)
	{
		(*FragmentObservers[i]).Empty();
		(*TagObservers[i]).Empty();
	}
}

bool FMassObserverManager::OnPostEntitiesCreated(const FMassArchetypeEntityCollection& EntityCollection)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(MassObserver_OnPostEntitiesCreated_Collection);

	if (LocksCount > 0)
	{
		TSharedRef<FObserverLock> ObserverLock = ActiveObserverLock.Pin().ToSharedRef();
		ObserverLock->AddCreatedEntitiesCollection(EntityCollection);
		return false;
	}

	const FMassArchetypeCompositionDescriptor& ArchetypeComposition = EntityManager.GetArchetypeComposition(EntityCollection.GetArchetype());
	return OnCompositionChanged(EntityCollection, ArchetypeComposition, EMassObservedOperation::Add);
}

bool FMassObserverManager::OnPostEntityCreated(FMassEntityHandle EntityHandle, const FMassArchetypeCompositionDescriptor& Composition)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(MassObserver_OnPostEntitiesCreated_Collection);

	if (LocksCount > 0)
	{
		TSharedRef<FObserverLock> ObserverLock = ActiveObserverLock.Pin().ToSharedRef();
		ObserverLock->AddCreatedEntity(EntityHandle);
		return false;
	}

	if (Composition.IsEmpty())
	{
		const FMassArchetypeHandle ArchetypeHandle = EntityManager.GetArchetypeForEntity(EntityHandle);
		const FMassArchetypeCompositionDescriptor& ArchetypeComposition = EntityManager.GetArchetypeComposition(ArchetypeHandle);
		return OnCompositionChanged(EntityHandle, ArchetypeComposition, EMassObservedOperation::Add);
	}

	return OnCompositionChanged(EntityHandle, Composition, EMassObservedOperation::Add);
}

bool FMassObserverManager::OnPreEntitiesDestroyed(const FMassArchetypeEntityCollection& EntityCollection)
{
	const FMassArchetypeCompositionDescriptor& ArchetypeComposition = EntityManager.GetArchetypeComposition(EntityCollection.GetArchetype());
	return OnCompositionChanged(EntityCollection, ArchetypeComposition, EMassObservedOperation::Remove);
}

bool FMassObserverManager::OnPreEntitiesDestroyed(UE::Mass::FProcessingContext& ProcessingContext, const FMassArchetypeEntityCollection& EntityCollection)
{
	const FMassArchetypeCompositionDescriptor& ArchetypeComposition = EntityManager.GetArchetypeComposition(EntityCollection.GetArchetype());	
	return OnCompositionChanged(EntityCollection, ArchetypeComposition, EMassObservedOperation::Remove, &ProcessingContext);
}

bool FMassObserverManager::OnPreEntityDestroyed(const FMassArchetypeCompositionDescriptor& ArchetypeComposition, const FMassEntityHandle Entity)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("OnPreEntityDestroyed")
	return OnCompositionChanged(Entity, ArchetypeComposition, EMassObservedOperation::Remove);
}

bool FMassObserverManager::OnCompositionChanged(FCollectionRefOrHandle&& EntityCollection, const FMassArchetypeCompositionDescriptor& CompositionDelta
	, const EMassObservedOperation Operation, UE::Mass::FProcessingContext* ProcessingContext)
{
	using UE::Mass::ObserverManager::Tweakables::InlineAllocatorElementsForOverlapTypes;
	ensureMsgf(EntityCollection.EntityHandle.IsValid() || EntityCollection.EntityCollection.IsUpToDate()
		, TEXT("Out-of-date FMassArchetypeEntityCollection used. Stored information is unreliable."));

	if (CompositionDelta.IsEmpty())
	{
		// nothing to do here.
		// @todo calling this function just to bail out would be a lot cheaper if we didn't have to create
		// FMassArchetypeCompositionDescriptor instances first - we usually just pass in tag or fragment bitsets.
		// like in FMassEntityManager::BatchChangeTagsForEntities
		return false;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(MassObserver_OnCompositionChanged);

	if (LocksCount > 0)
	{
		if (TSharedPtr<FCreationContext> CreationContext = GetCreationContext())
		{
			// a composition mutating operation is taking place, while creation lock is active - this operation invalidates the stored collections
			CreationContext->MarkDirty();
			return false;
		}
		UE_CVLOG_UELOG(Operation == EMassObservedOperation::Remove, EntityManager.GetOwner(), LogMass, Log
			, TEXT("%hs: Remove operation while observers are locked - the remove-observer will be executed after the data has already been removed."), __FUNCTION__);
	}

	const int32 OperationIndex = static_cast<int32>(Operation);

	FMassFragmentBitSet FragmentOverlap = ObservedFragments[OperationIndex].GetOverlap(CompositionDelta.Fragments);
	FMassTagBitSet TagOverlap = ObservedTags[OperationIndex].GetOverlap(CompositionDelta.Tags);
	const bool bHasFragmentsOverlap = !FragmentOverlap.IsEmpty();
	const bool bHasTagsOverlap = !TagOverlap.IsEmpty();

	if (bHasFragmentsOverlap || bHasTagsOverlap)
	{
		if (LocksCount > 0)
		{
			const UE::Mass::ObserverManager::EObservedOperationNotification NotificationType = static_cast<UE::Mass::ObserverManager::EObservedOperationNotification>(Operation);
			TSharedRef<FObserverLock> ObserverLockRef = ActiveObserverLock.Pin().ToSharedRef();

			// UE::Mass::FEntityCollection(EntityCollection) OR EntityHandle
			if (UE::Mass::ObserverManager::bCoalesceBufferedNotifications)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(MassObserver_Notify_Coalesced);

				if (EntityCollection.EntityHandle.IsSet())
				{
					ObserverLockRef->AddNotification(NotificationType, EntityCollection.EntityHandle
						, bHasFragmentsOverlap, MoveTemp(FragmentOverlap)
						, bHasTagsOverlap, MoveTemp(TagOverlap));
				}
				else
				{
					ObserverLockRef->AddNotification(NotificationType, EntityCollection.EntityCollection
						, bHasFragmentsOverlap, MoveTemp(FragmentOverlap) 
						, bHasTagsOverlap, MoveTemp(TagOverlap));
				}
			}
			else
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(MassObserver_Notify_Emplace);

				FBufferedNotification::FEntitiesContainer Entities;
				if (EntityCollection.EntityHandle.IsSet())
				{
					Entities.Emplace<FMassEntityHandle>(EntityCollection.EntityHandle);
				}
				else
				{
					Entities.Emplace<UE::Mass::FEntityCollection>(EntityCollection.EntityCollection);
				}

				if (bHasFragmentsOverlap && bHasTagsOverlap)
				{
					FMassArchetypeCompositionDescriptor ChangedComposition(MoveTemp(FragmentOverlap), MoveTemp(TagOverlap), {}, {}, {});
					ObserverLockRef->BufferedNotifications.Emplace(NotificationType, MoveTemp(ChangedComposition), MoveTemp(Entities));
				}
				else if (bHasFragmentsOverlap)
				{
					ObserverLockRef->BufferedNotifications.Emplace(NotificationType, MoveTemp(FragmentOverlap), MoveTemp(Entities));
				}
				else // bHasTagsOverlap
				{
					ObserverLockRef->BufferedNotifications.Emplace(NotificationType, MoveTemp(TagOverlap), MoveTemp(Entities));
				}
			}
		}
		else
		{
			auto HandleElements = [&](const FMassArchetypeEntityCollection& Collection)
			{
				TArray<const UScriptStruct*, TInlineAllocator<InlineAllocatorElementsForOverlapTypes>> ObservedTypesOverlap;

				alignas(UE::Mass::FProcessingContext) uint8 LocalContextBuffer[sizeof(UE::Mass::FProcessingContext)];
				UE::Mass::FProcessingContext* LocalProcessingContext = (ProcessingContext == nullptr)
					? new(&LocalContextBuffer) UE::Mass::FProcessingContext(EntityManager, /*DeltaSeconds=*/0.f, /*bFlushCommandBuffer=*/false)
					: ProcessingContext;

				if (bHasFragmentsOverlap)
				{
					FragmentOverlap.ExportTypes(ObservedTypesOverlap);

					HandleElementsImpl(*LocalProcessingContext, {Collection}, ObservedTypesOverlap, FragmentObservers[OperationIndex]);
				}

				if (bHasTagsOverlap)
				{
					ObservedTypesOverlap.Reset();
					TagOverlap.ExportTypes(ObservedTypesOverlap);

					HandleElementsImpl(*LocalProcessingContext, {Collection}, ObservedTypesOverlap, TagObservers[OperationIndex]);
				}

				if (ProcessingContext == nullptr)
				{
					LocalProcessingContext->~FProcessingContext();
				}
			};

			if (EntityCollection.EntityHandle.IsSet())
			{
				const FMassArchetypeHandle ArchetypeHandle = EntityManager.GetArchetypeForEntity(EntityCollection.EntityHandle);
				HandleElements(FMassArchetypeEntityCollection(ArchetypeHandle, EntityCollection.EntityHandle));
			}
			else
			{
				HandleElements(EntityCollection.EntityCollection);
			}

			return true;
		}
	}

	return false;
}

bool FMassObserverManager::OnCollectionsCreatedImpl(UE::Mass::FProcessingContext& ProcessingContext, TArray<FMassArchetypeEntityCollection>&& EntityCollections)
{
	using UE::Mass::ObserverManager::Tweakables::InlineAllocatorElementsForOverlapTypes;

	TRACE_CPUPROFILER_EVENT_SCOPE(MassObserver_OnCollectionsCreatedImpl_Collection);

	check(LocksCount == 0);

	constexpr int32 OperationIndex = static_cast<int32>(EMassObservedOperation::Add);

	FMassFragmentBitSet FragmentOverlap;
	FMassTagBitSet TagOverlap;

	for (FMassArchetypeEntityCollection Collection : EntityCollections)
	{
		checkfSlow(Collection.IsUpToDate(), TEXT("Out-of-date FMassArchetypeEntityCollection used. Stored information is unreliable."));

		const FMassArchetypeCompositionDescriptor& ArchetypeComposition = EntityManager.GetArchetypeComposition(Collection.GetArchetype());
		FragmentOverlap += ArchetypeComposition.Fragments;
		TagOverlap += ArchetypeComposition.Tags;
	}
	FragmentOverlap = ObservedFragments[OperationIndex].GetOverlap(FragmentOverlap);
	TagOverlap = ObservedTags[OperationIndex].GetOverlap(TagOverlap);

	const bool bHasFragmentsOverlap = !FragmentOverlap.IsEmpty();
	const bool bHasTagsOverlap = !TagOverlap.IsEmpty();
	if (bHasFragmentsOverlap || bHasTagsOverlap)
	{
		TArray<const UScriptStruct*, TInlineAllocator<InlineAllocatorElementsForOverlapTypes>> ObservedTypesOverlap;

		if (bHasFragmentsOverlap)
		{
			FragmentOverlap.ExportTypes(ObservedTypesOverlap);
			HandleElementsImpl(ProcessingContext, EntityCollections, ObservedTypesOverlap, FragmentObservers[OperationIndex]);
		}

		if (bHasTagsOverlap)
		{
			ObservedTypesOverlap.Reset();
			TagOverlap.ExportTypes(ObservedTypesOverlap);
			HandleElementsImpl(ProcessingContext, EntityCollections, ObservedTypesOverlap, TagObservers[OperationIndex]);
		}

		return true;
	}
	return false;
}

void FMassObserverManager::HandleElementsImpl(UE::Mass::FProcessingContext& ProcessingContext, TConstArrayView<FMassArchetypeEntityCollection> EntityCollections
		, TArrayView<const UScriptStruct*> ObservedTypes, FMassObserversMap& HandlersContainer)
{	
	TRACE_CPUPROFILER_EVENT_SCOPE(MassObserver_HandleFragmentsImpl);

	check(ObservedTypes.Num());
	ensureMsgf(EntityCollections.Num(), TEXT("Empty collections array is unexpected at this point. Nothing bad will happen, but it's a waste of perf."));

	FMassEntityManager::FScopedProcessing ProcessingScope = ProcessingContext.EntityManager->NewProcessingScope();

	for (const UScriptStruct* Type : ObservedTypes)
	{		
		ProcessingContext.AuxData.InitializeAs(Type);
		FMassRuntimePipeline& Pipeline = (*HandlersContainer).FindChecked(Type);

		UE::Mass::Executor::RunProcessorsView(Pipeline.GetMutableProcessors(), ProcessingContext, EntityCollections);
	}
}

void FMassObserverManager::AddObserverInstance(const UScriptStruct& ElementType, const EMassObservedOperation Operation, UMassProcessor& ObserverProcessor)
{
	const bool bIsFragment = UE::Mass::IsA<FMassFragment>(&ElementType);
	checkSlow(bIsFragment || UE::Mass::IsA<FMassTag>(&ElementType));

	FMassRuntimePipeline* Pipeline = nullptr;

	if (bIsFragment)
	{
		Pipeline = &(*FragmentObservers[static_cast<uint8>(Operation)]).FindOrAdd(&ElementType);
		ObservedFragments[static_cast<uint8>(Operation)].Add(ElementType);
	}
	else
	{
		Pipeline = &(*TagObservers[static_cast<uint8>(Operation)]).FindOrAdd(&ElementType);
		ObservedTags[static_cast<uint8>(Operation)].Add(ElementType);
	}

	// AppendUniqueProcessor will return true only if ObserverProcessor has not been a part of Pipeline yet.
	// Otherwise, we don't need to CallInitialize.
	if (Pipeline->AppendUniqueProcessor(ObserverProcessor))
	{
		// calling initialize to ensure the given processor is related to the same EntityManager
		if (UObject* Owner = EntityManager.GetOwner())
		{	
			ObserverProcessor.CallInitialize(Owner, EntityManager.AsShared());
		}
	}
}

void FMassObserverManager::RemoveObserverInstance(const UScriptStruct& ElementType, const EMassObservedOperation Operation, UMassProcessor& ObserverProcessor)
{
	const bool bIsFragmentObserver = UE::Mass::IsA<FMassFragment>(&ElementType);

	if (!ensure(bIsFragmentObserver || UE::Mass::IsA<FMassTag>(&ElementType)))
	{
		return;
	}

	TMap<TObjectPtr<const UScriptStruct>, FMassRuntimePipeline>& ObserversMap =
		bIsFragmentObserver ? *FragmentObservers[(uint8)Operation] : *TagObservers[(uint8)Operation];

	FMassRuntimePipeline* Pipeline = ObserversMap.Find(&ElementType);
	if (!ensureMsgf(Pipeline, TEXT("Trying to remove an observer for a fragment/tag that does not seem to be observed.")))
	{
		return;
	}
	Pipeline->RemoveProcessor(ObserverProcessor);

	if (Pipeline->Num() == 0)
	{
		ObserversMap.Remove(&ElementType);
		if (bIsFragmentObserver)
		{
			ObservedFragments[(uint8)Operation].Remove(ElementType);
		}
		else
		{
			ObservedTags[(uint8)Operation].Remove(ElementType);
		}
	}
}

TSharedRef<FMassObserverManager::FObserverLock> FMassObserverManager::GetOrMakeObserverLock()
{
	if (ActiveObserverLock.IsValid())
	{
		return ActiveObserverLock.Pin().ToSharedRef();
	}
	else
	{
		FObserverLock* ObserverLock = new FObserverLock(*this);
		TSharedRef<FObserverLock> SharedContext = MakeShareable(ObserverLock);
		ActiveObserverLock = SharedContext;
		return SharedContext;
	}
}

TSharedRef<FMassObserverManager::FCreationContext> FMassObserverManager::GetOrMakeCreationContext()
{
	if (ActiveCreationContext.IsValid())
	{
		return ActiveCreationContext.Pin().ToSharedRef();
	}
	else
	{
		FCreationContext* ObserverLock = new FCreationContext(GetOrMakeObserverLock());
#if WITH_MASSENTITY_DEBUG
		ObserverLock->CreationHandle.SerialNumber = LockedNotificationSerialNumber;
#endif // WITH_MASSENTITY_DEBUG
		ObserverLock->CreationHandle.OpIndex = ObserverLock->Lock->AddCreatedEntities({});
		TSharedRef<FCreationContext> SharedContext = MakeShareable(ObserverLock);
		ActiveCreationContext = SharedContext;
		return SharedContext;
	}
}

TSharedRef<FMassObserverManager::FCreationContext> FMassObserverManager::GetOrMakeCreationContext(TConstArrayView<FMassEntityHandle> ReservedEntities
	, FMassArchetypeEntityCollection&& EntityCollection)
{
	if (TSharedPtr<FCreationContext> CreationContext = ActiveCreationContext.Pin())
	{
		CreationContext->GetObserverLock()->AddCreatedEntities(ReservedEntities, Forward<FMassArchetypeEntityCollection>(EntityCollection));
		return CreationContext.ToSharedRef();
	}
	else
	{
		FCreationContext* ObserverLock = new FCreationContext(GetOrMakeObserverLock());
#if WITH_MASSENTITY_DEBUG
		ObserverLock->CreationHandle.SerialNumber = LockedNotificationSerialNumber;
#endif // WITH_MASSENTITY_DEBUG
		ObserverLock->CreationHandle.OpIndex = ObserverLock->Lock->AddCreatedEntities(ReservedEntities, Forward<FMassArchetypeEntityCollection>(EntityCollection));
		TSharedRef<FCreationContext> SharedContext = MakeShareable(ObserverLock);
		ActiveCreationContext = SharedContext;
		return SharedContext;
	}
}

void FMassObserverManager::OnPostFork(EForkProcessRole)
{
	if (TSharedPtr<FObserverLock> ActiveContext = ActiveObserverLock.Pin())
	{
		ActiveContext->ForceUpdateCurrentThreadID();
	}
}

void FMassObserverManager::ResumeExecution(FObserverLock& LockBeingReleased)
{
	using namespace UE::Mass::ObserverManager;

	ensureMsgf(LocksCount == 0, TEXT("We only expect this function to be called if all locks are released."));
#if WITH_MASSENTITY_DEBUG
	ensureMsgf(LockBeingReleased.LockSerialNumber == LockedNotificationSerialNumber
		, TEXT("Lock's and ObserverManager's lock serial numbers are expected to match."));
	++LockedNotificationSerialNumber;
#endif // WITH_MASSENTITY_DEBUG

	if (LockBeingReleased.BufferedNotifications.IsEmpty() == false)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(MassObserver_ResumeExecution);

		UE::Mass::FProcessingContext ProcessingContext(EntityManager);

		FNotificationContext NotificationContext{*this, ProcessingContext};
		FBufferedNotificationExecutioner AddNotification(NotificationContext, EMassObservedOperation::Add);
		FBufferedNotificationExecutioner RemoveNotification(NotificationContext, EMassObservedOperation::Remove);
		FBufferedCreationNotificationExecutioner CreationOpExecutioner(NotificationContext);

		for (FBufferedNotification& Op : LockBeingReleased.BufferedNotifications)
		{
			switch (Op.Type)
			{
				case EObservedOperationNotification::Add:
					Visit(AddNotification, Op.CompositionChange, Op.AffectedEntities);
					break;
				case EObservedOperationNotification::Remove:
					Visit(RemoveNotification, Op.CompositionChange, Op.AffectedEntities);
					break;
				case EObservedOperationNotification::Create:
					Visit(CreationOpExecutioner, MoveTemp(Op.AffectedEntities));
					break;
				default:
					ensureMsgf(false, TEXT("%hs: Unhandled EObservedOperationNotification value"), __FUNCTION__);
			}
		}
#if WITH_MASSENTITY_DEBUG
		++DebugNonTrivialResumeExecutionCount;
#endif // WITH_MASSENTITY_DEBUG
	}
}

void FMassObserverManager::ReleaseCreationHandle(FCreationNotificationHandle InCreationNotificationHandle)
{
	ensureMsgf(InCreationNotificationHandle.IsSet(), TEXT("Invalid creation handle passed to %hs"), __FUNCTION__);
#if WITH_MASSENTITY_DEBUG
	ensureMsgf(InCreationNotificationHandle.SerialNumber == LockedNotificationSerialNumber
		, TEXT("Creation handle's serial number doesn't match the ObserverManager's data"));
#endif // WITH_MASSENTITY_DEBUG

	TSharedPtr<FObserverLock> LockInstance = ActiveObserverLock.Pin();
	if (ensure(LockInstance))
	{
		ensure(LockInstance->ReleaseCreationNotification(InCreationNotificationHandle));
		ensure(ActiveCreationContext.IsValid() == false);
	}
}

//----------------------------------------------------------------------//
// DEPRECATED
//----------------------------------------------------------------------//
bool FMassObserverManager::OnPostEntitiesCreated(UE::Mass::FProcessingContext&, const FMassArchetypeEntityCollection& EntityCollection)
{
	return OnPostEntitiesCreated(EntityCollection);
}

bool FMassObserverManager::OnPostEntitiesCreated(UE::Mass::FProcessingContext&, TConstArrayView<FMassArchetypeEntityCollection> EntityCollections)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("OnPostEntitiesCreated")

	bool bReturnValue = false;

	for (const FMassArchetypeEntityCollection& Collection : EntityCollections)
	{
		const FMassArchetypeCompositionDescriptor& ArchetypeComposition = EntityManager.GetArchetypeComposition(Collection.GetArchetype());
		bReturnValue |= OnCompositionChanged(Collection, ArchetypeComposition, EMassObservedOperation::Add);
	}

	return bReturnValue;
}

bool FMassObserverManager::OnCompositionChanged(UE::Mass::FProcessingContext&, const FMassArchetypeEntityCollection& EntityCollection
	, const FMassArchetypeCompositionDescriptor& CompositionDelta, const EMassObservedOperation InOperation)
{
	return OnCompositionChanged(EntityCollection, CompositionDelta, InOperation);
}

void FMassObserverManager::HandleSingleEntityImpl(const UScriptStruct& FragmentType, const FMassArchetypeEntityCollection& EntityCollection, FMassObserversMap& HandlersContainer)
{
	UE::Mass::ObserverManager::FDeprecationHelper::HandleSingleElement(this, FragmentType, EntityCollection, HandlersContainer);
}

void FMassObserverManager::OnPostFragmentOrTagAdded(const UScriptStruct& FragmentOrTagType, const FMassArchetypeEntityCollection& EntityCollection)
{
	UE::Mass::ObserverManager::FDeprecationHelper::HandleSingleElement(this, FragmentOrTagType, EntityCollection, EMassObservedOperation::Add);
}

void FMassObserverManager::OnPreFragmentOrTagRemoved(const UScriptStruct& FragmentOrTagType, const FMassArchetypeEntityCollection& EntityCollection)
{
	UE::Mass::ObserverManager::FDeprecationHelper::HandleSingleElement(this, FragmentOrTagType, EntityCollection, EMassObservedOperation::Remove);
}

void FMassObserverManager::OnFragmentOrTagOperation(const UScriptStruct& FragmentOrTagType, const FMassArchetypeEntityCollection& EntityCollection, const EMassObservedOperation Operation)
{
	UE::Mass::ObserverManager::FDeprecationHelper::HandleSingleElement(this, FragmentOrTagType, EntityCollection, Operation);
}

TConstArrayView<FMassArchetypeEntityCollection> FMassObserverManager::FCreationContext::GetEntityCollections() const
{
	return {};
}

int32 FMassObserverManager::FCreationContext::GetSpawnedNum() const
{
	return 0;
}

bool FMassObserverManager::FCreationContext::IsDirty() const
{
	return true;
}

void FMassObserverManager::FCreationContext::AppendEntities(const TConstArrayView<FMassEntityHandle>)
{
}

void FMassObserverManager::FCreationContext::AppendEntities(const TConstArrayView<FMassEntityHandle>, FMassArchetypeEntityCollection&&)
{
}

FMassObserverManager::FCreationContext::FCreationContext(const int32)
	: FCreationContext()
{}

const FMassArchetypeEntityCollection& FMassObserverManager::FCreationContext::GetEntityCollection() const
{
	static FMassArchetypeEntityCollection DummyInstance;
	return DummyInstance;
}
