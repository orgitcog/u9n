// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassExecutionContext.h"
#include "MassArchetypeData.h"
#include "MassEntityManager.h"
#if WITH_MASSENTITY_DEBUG
#include "MassDebugger.h"
#endif // WITH_MASSENTITY_DEBUG
#include "MassTestableEnsures.h"

//-----------------------------------------------------------------------------
// FMassExecutionContext
//-----------------------------------------------------------------------------
FMassExecutionContext::FMassExecutionContext(FMassEntityManager& InEntityManager, const float InDeltaTimeSeconds, const bool bInFlushDeferredCommands)
	: SubsystemAccess(InEntityManager.GetWorld())
	, DeltaTimeSeconds(InDeltaTimeSeconds)
	, EntityManager(InEntityManager.AsShared())
	, bFlushDeferredCommands(bInFlushDeferredCommands)
{
}

FMassExecutionContext::FMassExecutionContext(const FMassExecutionContext& Other)
	: EntityManager(Other.EntityManager)
{
	// we're using assignment operator here as a setup helper, as per operator=='s comment.
	*this = Other;

	QueriesStack.Reset();
}

FMassExecutionContext::FMassExecutionContext(const FMassExecutionContext& Other, FMassEntityQuery& Query, const TSharedPtr<FMassCommandBuffer>& InCommandBuffer)
	: FMassExecutionContext(Other)
{
	ensureMsgf(Other.QueriesStack.Last().Query == &Query, TEXT("Creating a single-query execution context but the Query doesn't match the source context."));
	QueriesStack.Add(Other.QueriesStack.Last());
	SetDeferredCommandBuffer(InCommandBuffer);
}

FMassExecutionContext::~FMassExecutionContext()
{
	ensureMsgf(QueriesStack.Num() == 0, TEXT("Destroying a FMassExecutionContext instance while not all queries have been popped is unexpected."));
}

void FMassExecutionContext::FlushDeferred()
{
	if (bFlushDeferredCommands && DeferredCommandBuffer)
	{
		EntityManager->FlushCommands(DeferredCommandBuffer);
	}
}

void FMassExecutionContext::ClearExecutionData()
{
	FragmentViews.Reset();
	ChunkFragmentViews.Reset();
	ConstSharedFragmentViews.Reset();
	SharedFragmentViews.Reset();
	CurrentArchetypeCompositionDescriptor.Reset();
	EntityListView = {};
	ChunkSerialModificationNumber = -1;
#if WITH_MASSENTITY_DEBUG
	DebugColor = FColor();
#endif // WITH_MASSENTITY_DEBUG
}

bool FMassExecutionContext::CacheSubsystemRequirements(const FMassSubsystemRequirements& SubsystemRequirements)
{
	return SubsystemAccess.CacheSubsystemRequirements(SubsystemRequirements);
}

void FMassExecutionContext::SetEntityCollection(const FMassArchetypeEntityCollection& InEntityCollection)
{
	check(EntityCollection.IsEmpty());
	EntityCollection = InEntityCollection;
}

void FMassExecutionContext::SetEntityCollection(FMassArchetypeEntityCollection&& InEntityCollection)
{
	check(EntityCollection.IsEmpty());
	check(InEntityCollection.IsUpToDate());
	EntityCollection = MoveTemp(InEntityCollection);
}

void FMassExecutionContext::SetFragmentRequirements(const FMassFragmentRequirements& FragmentRequirements)
{
	FragmentViews.Reset();
	for (const FMassFragmentRequirementDescription& Requirement : FragmentRequirements.GetFragmentRequirements())
	{
		if (Requirement.RequiresBinding())
		{
			FragmentViews.Emplace(Requirement);
		}
	}

	ChunkFragmentViews.Reset();
	for (const FMassFragmentRequirementDescription& Requirement : FragmentRequirements.GetChunkFragmentRequirements())
	{
		if (Requirement.RequiresBinding())
		{
			ChunkFragmentViews.Emplace(Requirement);
		}
	}

	ConstSharedFragmentViews.Reset();
	for (const FMassFragmentRequirementDescription& Requirement : FragmentRequirements.GetConstSharedFragmentRequirements())
	{
		if (Requirement.RequiresBinding())
		{
			ConstSharedFragmentViews.Emplace(Requirement);
		}
	}

	SharedFragmentViews.Reset();
	for (const FMassFragmentRequirementDescription& Requirement : FragmentRequirements.GetSharedFragmentRequirements())
	{
		if (Requirement.RequiresBinding())
		{
			SharedFragmentViews.Emplace(Requirement);
		}
	}
}

UWorld* FMassExecutionContext::GetWorld() 
{ 
	return EntityManager->GetWorld(); 
}

void FMassExecutionContext::PushQuery(FMassEntityQuery& InQuery)
{
	FQueryTransientRuntime& RuntimeData = QueriesStack.Add_GetRef({&InQuery});
	GetSubsystemRequirementBits(RuntimeData.ConstSubsystemsBitSet, RuntimeData.MutableSubsystemsBitSet);

#if WITH_MASSENTITY_DEBUG
	// check if this could possibly trigger a break before iterating to avoid extraneous breakpoint checks
	FMassEntityManager& EntityManagerRef = GetEntityManagerChecked();

	RuntimeData.bCheckProcessorBreaks = FMassDebugger::HasAnyProcessorBreakpoints(EntityManagerRef, DebugGetProcessor());
	
	if (UNLIKELY(FMassDebugger::HasAnyFragmentWriteBreakpoints(EntityManagerRef)))
	{
		auto CheckFragmentRequirement = [&RuntimeData, &EntityManagerRef](TConstArrayView<FMassFragmentRequirementDescription> Requirements) -> void
			{
				for (const FMassFragmentRequirementDescription& Req : Requirements)
				{
					if (Req.AccessMode == EMassFragmentAccess::ReadWrite &&
						FMassDebugger::HasAnyFragmentWriteBreakpoints(EntityManagerRef, Req.StructType))
					{
						if (ensureMsgf(RuntimeData.BreakFragmentsCount < FQueryTransientRuntime::MaxFragmentBreakpointCount, 
							TEXT("Fragment write breakpoint count limit exceeded for this query.")))
						{
							RuntimeData.FragmentTypesToBreakOn[RuntimeData.BreakFragmentsCount++] = Req.StructType;
						}
					}
				}
			};

		// don't need to check ConstSharedFragmentRequirements because those can't write
		CheckFragmentRequirement(InQuery.GetFragmentRequirements());
		CheckFragmentRequirement(InQuery.GetChunkFragmentRequirements());
		CheckFragmentRequirement(InQuery.GetSharedFragmentRequirements());
	}
#endif // WITH_MASSENTITY_DEBUG

	RuntimeData.IteratorSerialNumber = ++IteratorSerialNumberGenerator;
}

void FMassExecutionContext::PopQuery(const FMassEntityQuery& InQuery)
{
	const FQueryTransientRuntime& RuntimeData = QueriesStack.Last();
	checkf(&InQuery == RuntimeData.Query, TEXT("Queries are stored in a stack and as such it requires elements to be added in LIFO order"));

	SetSubsystemRequirementBits(RuntimeData.ConstSubsystemsBitSet, RuntimeData.MutableSubsystemsBitSet);

	QueriesStack.RemoveAt(QueriesStack.Num() - 1, EAllowShrinking::No);
}

FMassExecutionContext::FEntityIterator FMassExecutionContext::CreateEntityIterator()
{
	if (!testableEnsureMsgf(QueriesStack.Num(), TEXT("Attempting to create an Entity Iterator when no entity query is being executed.")))
	{
		return FEntityIterator(*this);
	}

	return FEntityIterator(*this, QueriesStack.Last());
}

FMassExecutionContext& FMassExecutionContext::GetDummyInstance()
{
	static FMassExecutionContext DummyContext(*TSharedRef<FMassEntityManager>(MakeShareable(new FMassEntityManager())));
	return DummyContext;
}

//-----------------------------------------------------------------------------
// FMassExecutionContext::FQueryTransientRuntime
//-----------------------------------------------------------------------------
FMassExecutionContext::FQueryTransientRuntime& FMassExecutionContext::FQueryTransientRuntime::GetDummyInstance()
{
	static FMassEntityQuery DummyQuery;
	static FQueryTransientRuntime DummyInstance = { &DummyQuery };
	return DummyInstance;
}

//-----------------------------------------------------------------------------
// FMassExecutionContext::FEntityIterator
//-----------------------------------------------------------------------------
FMassExecutionContext::FEntityIterator::FEntityIterator()
	: ExecutionContext(FMassExecutionContext::GetDummyInstance())
	, QueryRuntime(FQueryTransientRuntime::GetDummyInstance())
{
	
}

FMassExecutionContext::FEntityIterator::FEntityIterator(FMassExecutionContext& InExecutionContext)
	: ExecutionContext(InExecutionContext)
	, QueryRuntime(FQueryTransientRuntime::GetDummyInstance())
{
	
}

FMassExecutionContext::FEntityIterator::FEntityIterator(FMassExecutionContext& InExecutionContext, FQueryTransientRuntime& InQueryRuntime)
	: ExecutionContext(InExecutionContext)
	, QueryRuntime(InQueryRuntime)
	, NumEntities(InExecutionContext.GetNumEntities())
	, SerialNumber(InQueryRuntime.IteratorSerialNumber)
{
	this->operator++();
}

#if WITH_MASSENTITY_DEBUG

UE_DISABLE_OPTIMIZATION_SHIP
void FMassExecutionContext::FEntityIterator::TestBreakpoints()
{
	FMassEntityManager& EntityManagerRef = ExecutionContext.GetEntityManagerChecked();
	FMassEntityHandle Entity = ExecutionContext.GetEntity(EntityIndex);
	if (QueryRuntime.bCheckProcessorBreaks && FMassDebugger::ShouldProcessorBreak(EntityManagerRef, ExecutionContext.DebugGetProcessor(), Entity))
	{
		bool bDisableThisBreakpoint = false;
		//====================================================================
		//= A breakpoint for this entity set in the MassDebugger has triggered
		//= Step out of this function to debug the actual code being run for the entity
		//=
		//= To disable this specific breakpoint use the Watch window do set
		//= bDisableThisBreakpoint to `true` or 1
		//====================================================================
		UE_DEBUG_BREAK();

		if (bDisableThisBreakpoint)
		{
			FMassDebugger::ClearProcessorBreakpoint(EntityManagerRef, ExecutionContext.DebugGetProcessor(), Entity);
		}

		// bailing out, no point to hit multiple breakpoints for the given entity/processor pair
		return;
	}

	for (const UScriptStruct* Fragment : QueryRuntime.FragmentTypesToBreakOn)
	{
		if (FMassDebugger::ShouldBreakOnFragmentWrite(EntityManagerRef, Fragment, Entity))
		{
			bool bDisableThisBreakpoint = false;
			//====================================================================
			//= A breakpoint for this entity set in the MassDebugger has triggered
			//= Step out of this function to debug the actual code being run for the entity
			// 
			//= To disable this specific breakpoint use the Watch window do set
			//= bDisableThisBreakpoint to `true` or 1
			//====================================================================
			UE_DEBUG_BREAK();

			if (bDisableThisBreakpoint)
			{
				FMassDebugger::ClearFragmentWriteBreak(EntityManagerRef, Fragment, Entity);
			}

			// bailing out, no point to hit multiple breakpoints for the given entity/processor pair
			return;
		}
	}
}
UE_ENABLE_OPTIMIZATION_SHIP

#endif // WITH_MASSENTITY_DEBUG