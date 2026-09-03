// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassEntityManager.h"
#include "MassProcessingTypes.h"
#include "MassEntityTestTypes.h"
#include "MassEntityTypes.h"
#include "MassCommandBuffer.h"
#include "MassExecutionContext.h"
#include "MassObserverNotificationTypes.h"

#define LOCTEXT_NAMESPACE "MassTest"

UE_DISABLE_OPTIMIZATION_SHIP

//----------------------------------------------------------------------//
// tests 
//----------------------------------------------------------------------//
namespace UE::Mass::Test::Observers
{

auto EntityIndexSorted = [](const FMassEntityHandle& A, const FMassEntityHandle& B)
{
	return A.Index < B.Index;
};

struct FTagBaseOperation : FEntityTestBase
{
	using FTagStruct = FTestTag_A;

	TArray<FMassEntityHandle> AffectedEntities;
	UMassTestProcessorBase* ObserverProcessor = nullptr;
	EMassObservedOperation OperationObserved = EMassObservedOperation::MAX;
	TArray<FMassEntityHandle> EntitiesInt;
	TArray<FMassEntityHandle> EntitiesIntsFloat;
	TArray<FMassEntityHandle> ExpectedEntities;
	bool bCommandsFlushed = false;

	// @return signifies if the test can continue
	virtual bool PerformOperation() { return false; }

	virtual bool SetUp() override
	{
		if (FEntityTestBase::SetUp())
		{
			ObserverProcessor = NewTestProcessor<UMassTestProcessorBase>(EntityManager);
			ObserverProcessor->EntityQuery.AddRequirement<FTestFragment_Int>(EMassFragmentAccess::ReadOnly);
			ObserverProcessor->EntityQuery.AddTagRequirement<FTagStruct>(EMassFragmentPresence::All);
			ObserverProcessor->ForEachEntityChunkExecutionFunction = [bCommandsFlushedPtr = &bCommandsFlushed, AffectedEntitiesPtr = &AffectedEntities](FMassExecutionContext& Context)
			{
				AffectedEntitiesPtr->Append(Context.GetEntities().GetData(), Context.GetEntities().Num());
				Context.Defer().PushCommand<FMassDeferredSetCommand>([&bCommandsFlushedPtr](FMassEntityManager&)
					{
						// dummy command, here just to catch if commands issue by observers got executed at all
						*bCommandsFlushedPtr = true;
					});
			};

			return true;
		}
		return false;
	}

	virtual bool InstantTest() override
	{
		FMassObserverManager& ObserverManager = EntityManager->GetObserverManager();
		ObserverManager.AddObserverInstance(*FTagStruct::StaticStruct(), OperationObserved, *ObserverProcessor);

		EntityManager->BatchCreateEntities(IntsArchetype, 3, EntitiesInt);
		EntityManager->BatchCreateEntities(FloatsIntsArchetype, 3, EntitiesIntsFloat);

		if (PerformOperation())
		{
			EntityManager->FlushCommands();
			AITEST_EQUAL(TEXT("The observer is expected to be run for predicted number of entities"), AffectedEntities.Num(), ExpectedEntities.Num());
			AITEST_TRUE(TEXT("The commands issued by the observer are flushed"), bCommandsFlushed);

			ExpectedEntities.Sort(EntityIndexSorted);
			AffectedEntities.Sort(EntityIndexSorted);

			for (int i = 0; i < ExpectedEntities.Num(); ++i)
			{
				AITEST_EQUAL(TEXT("Expected and affected sets should be the same"), AffectedEntities[i], ExpectedEntities[i]);
			}
		}

		return true;
	}
};

struct FSingleEntitySingleArchetypeAdd : FTagBaseOperation
{
	FSingleEntitySingleArchetypeAdd() { OperationObserved = EMassObservedOperation::Add; }
	virtual bool PerformOperation() override 
	{
		ExpectedEntities = { EntitiesInt[1] };
		EntityManager->Defer().AddTag<FTagStruct>(EntitiesInt[1]);
		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FSingleEntitySingleArchetypeAdd, "System.Mass.Observer.Tag.SingleEntitySingleArchetypeAdd");

struct FSingleEntitySingleArchetypeRemove : FTagBaseOperation
{
	FSingleEntitySingleArchetypeRemove() { OperationObserved = EMassObservedOperation::Remove; }
	virtual bool PerformOperation() override
	{
		ExpectedEntities = { EntitiesInt[1] };

		EntityManager->Defer().AddTag<FTagStruct>(EntitiesInt[1]);
		EntityManager->FlushCommands();
		// since we're only observing tag removal we don't expect AffectedEntities to contain any data at this point
		AITEST_EQUAL(TEXT("Tag addition is not being observed and is not expected to produce results yet"), AffectedEntities.Num(), 0);
		EntityManager->Defer().RemoveTag<FTagStruct>(EntitiesInt[1]);
		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FSingleEntitySingleArchetypeRemove, "System.Mass.Observer.Tag.SingleEntitySingleArchetypeRemove");

struct FSingleEntitySingleArchetypeDestroy : FTagBaseOperation
{
	FSingleEntitySingleArchetypeDestroy() { OperationObserved = EMassObservedOperation::Remove; }
	virtual bool PerformOperation() override
	{
		ExpectedEntities = { EntitiesInt[1] };
		EntityManager->Defer().AddTag<FTagStruct>(EntitiesInt[1]);
		EntityManager->FlushCommands();
		// since we're only observing tag removal we don't expect AffectedEntities to contain any data at this point
		AITEST_EQUAL(TEXT("FTagStruct addition is not being observed and is not expected to produce results yet"), AffectedEntities.Num(), 0);
		EntityManager->Defer().DestroyEntity(EntitiesInt[1]);
		
		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FSingleEntitySingleArchetypeDestroy, "System.Mass.Observer.Tag.SingleEntitySingleArchetypeDestroy");

struct FMultipleArchetypeAdd : FTagBaseOperation
{
	FMultipleArchetypeAdd() { OperationObserved = EMassObservedOperation::Add; }

	virtual bool PerformOperation() override
	{
		ExpectedEntities = { EntitiesInt[0], EntitiesInt[2], EntitiesIntsFloat[1] };
		for (const FMassEntityHandle& ModifiedEntity : ExpectedEntities)
		{
			EntityManager->Defer().AddTag<FTagStruct>(ModifiedEntity);
		}
		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FMultipleArchetypeAdd, "System.Mass.Observer.Tag.MultipleArchetypesAdd");


struct FMultipleArchetypeAdd_Sync : FTagBaseOperation
{
	FMultipleArchetypeAdd_Sync() { OperationObserved = EMassObservedOperation::Add; }

	virtual bool PerformOperation() override
	{
		ExpectedEntities = { EntitiesInt[0], EntitiesInt[2], EntitiesIntsFloat[1] };
		for (const FMassEntityHandle& ModifiedEntity : ExpectedEntities)
		{
			EntityManager->AddTagToEntity(ModifiedEntity, FTagStruct::StaticStruct());
		}
		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FMultipleArchetypeAdd_Sync, "System.Mass.Observer.Tag.MultipleArchetypesAdd_Sync");

struct FMultipleArchetypeRemove : FTagBaseOperation
{
	FMultipleArchetypeRemove() { OperationObserved = EMassObservedOperation::Remove; }

	virtual bool PerformOperation() override
	{
		ExpectedEntities = { EntitiesInt[0], EntitiesInt[2], EntitiesIntsFloat[1] };
		for (const FMassEntityHandle& ModifiedEntity : ExpectedEntities)
		{
			EntityManager->Defer().AddTag<FTagStruct>(ModifiedEntity);
		}
		EntityManager->FlushCommands();
		// since we're only observing tag removal we don't expect AffectedEntities to contain any data at this point
		AITEST_EQUAL(TEXT("FTagStruct addition is not being observed and is not expected to produce results yet"), AffectedEntities.Num(), 0);
		for (const FMassEntityHandle& ModifiedEntity : ExpectedEntities)
		{
			EntityManager->Defer().RemoveTag<FTagStruct>(ModifiedEntity);
		}

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FMultipleArchetypeRemove, "System.Mass.Observer.Tag.MultipleArchetypesRemove");

struct FMultipleArchetypeRemove_Sync : FTagBaseOperation
{
	FMultipleArchetypeRemove_Sync() { OperationObserved = EMassObservedOperation::Remove; }

	virtual bool PerformOperation() override
	{
		ExpectedEntities = { EntitiesInt[0], EntitiesInt[2], EntitiesIntsFloat[1] };
		for (const FMassEntityHandle& ModifiedEntity : ExpectedEntities)
		{
			EntityManager->AddTagToEntity(ModifiedEntity, FTagStruct::StaticStruct());
		}
		
		// since we're only observing tag removal we don't expect AffectedEntities to contain any data at this point
		AITEST_EQUAL(TEXT("FTagStruct addition is not being observed and is not expected to produce results yet"), AffectedEntities.Num(), 0);
		for (const FMassEntityHandle& ModifiedEntity : ExpectedEntities)
		{
			EntityManager->RemoveTagFromEntity(ModifiedEntity, FTagStruct::StaticStruct());
		}

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FMultipleArchetypeRemove_Sync, "System.Mass.Observer.Tag.MultipleArchetypesRemove_Sync");

struct FMultipleArchetypeDestroy : FTagBaseOperation
{
	FMultipleArchetypeDestroy() { OperationObserved = EMassObservedOperation::Remove; }

	virtual bool PerformOperation() override
	{
		ExpectedEntities = { EntitiesInt[0], EntitiesInt[2], EntitiesIntsFloat[1] };
		for (const FMassEntityHandle& ModifiedEntity : ExpectedEntities)
		{
			EntityManager->Defer().AddTag<FTagStruct>(ModifiedEntity);
		}
		EntityManager->FlushCommands();
		// since we're only observing tag removal we don't expect AffectedEntities to contain any data at this point
		AITEST_EQUAL(TEXT("Tag addition is not being observed and is not expected to produce results yet"), AffectedEntities.Num(), 0);
		for (const FMassEntityHandle& ModifiedEntity : ExpectedEntities)
		{
			EntityManager->Defer().DestroyEntity(ModifiedEntity);
		}
		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FMultipleArchetypeDestroy, "System.Mass.Observer.Tag.MultipleArchetypesDestroy");

struct FForbidModifyOnDestroy : FTagBaseOperation
{
	using Super = FTagBaseOperation;
	FForbidModifyOnDestroy() { OperationObserved = EMassObservedOperation::Remove; }

	virtual bool SetUp() override
	{
		if (Super::SetUp())
		{
			ObserverProcessor->ForEachEntityChunkExecutionFunction = [bCommandsFlushedPtr = &bCommandsFlushed, AffectedEntitiesPtr = &AffectedEntities](FMassExecutionContext& Context)
				{
					AffectedEntitiesPtr->Append(Context.GetEntities().GetData(), Context.GetEntities().Num());

					// try changing the input entities' composition.
					const bool bIsProcessing = Context.GetEntityManagerChecked().IsProcessing();
					//for (int32 EntityIndex = 0; EntityIndex < Context.GetEntities().Num(); ++EntityIndex)
					for (const FMassEntityHandle EntityHandle : Context.GetEntities())
					{
						//Context.Defer().AddTag<FTestTag_A>(EntityHandle);
						//Context.GetEntityManagerChecked().AddTagToEntity(EntityHandle, FTestTag_B::StaticStruct());
					}

					Context.Defer().PushCommand<FMassDeferredSetCommand>([&bCommandsFlushedPtr](FMassEntityManager&)
						{
							// dummy command, here just to catch if commands issue by observers got executed at all
							*bCommandsFlushedPtr = true;
						});
				};

			return true;
		}
		return false;
	}

	virtual bool PerformOperation() override
	{
		ExpectedEntities = { EntitiesInt[0], EntitiesInt[2], EntitiesIntsFloat[1] };
		for (const FMassEntityHandle& ModifiedEntity : ExpectedEntities)
		{
			EntityManager->Defer().AddTag<FTagStruct>(ModifiedEntity);
		}
		EntityManager->FlushCommands();

		// since we're only observing tag removal we don't expect AffectedEntities to contain any data at this point
		AITEST_EQUAL(TEXT("Tag addition is not being observed and is not expected to produce results yet"), AffectedEntities.Num(), 0);
		for (const FMassEntityHandle& ModifiedEntity : ExpectedEntities)
		{
			EntityManager->Defer().DestroyEntity(ModifiedEntity);
		}
		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FForbidModifyOnDestroy, "System.Mass.Observer.ForbidModifyOnDestroy");

struct FMultipleArchetypeSwap : FTagBaseOperation
{
	FMultipleArchetypeSwap() { OperationObserved = EMassObservedOperation::Remove; }

	virtual bool PerformOperation() override
	{
		ExpectedEntities = { EntitiesIntsFloat[1], EntitiesInt[0], EntitiesInt[2] };
		for (const FMassEntityHandle& ModifiedEntity : ExpectedEntities)
		{
			EntityManager->Defer().AddTag<FTagStruct>(ModifiedEntity);
		}
		EntityManager->FlushCommands();
		// since we're only observing tag removal we don't expect AffectedEntities to contain any data at this point
		AITEST_EQUAL(TEXT("Tag addition is not being observed and is not expected to produce results yet"), AffectedEntities.Num(), 0);
		for (const FMassEntityHandle& ModifiedEntity : ExpectedEntities)
		{
			EntityManager->Defer().SwapTags<FTagStruct, FTestTag_B>(ModifiedEntity);
		}
		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FMultipleArchetypeSwap, "System.Mass.Observer.Tag.MultipleArchetypesSwap");

struct FEntityCreation_Individuals : FTagBaseOperation
{
	FEntityCreation_Individuals() { OperationObserved = EMassObservedOperation::Add; }

	virtual bool InstantTest() override
	{
		constexpr int32 EntitiesToSpawnCount = 6;
		
		FMassObserverManager& ObserverManager = EntityManager->GetObserverManager();
		ObserverManager.AddObserverInstance(*FTagStruct::StaticStruct(), OperationObserved, *ObserverProcessor);

		int32 ArrayMidPoint = 0;
		{
			TSharedRef<FMassEntityManager::FEntityCreationContext> CreationContext = EntityManager->BatchCreateEntities(IntsArchetype, EntitiesToSpawnCount, EntitiesInt);
			ArrayMidPoint = EntitiesInt.Num() / 2;

			for (int32 Index = 0; Index < ArrayMidPoint; ++Index)
			{
				EntityManager->AddTagToEntity(EntitiesInt[Index], FTagStruct::StaticStruct());
			}
			AITEST_EQUAL(TEXT("The tag observer is not expected to run yet"), AffectedEntities.Num(), 0);
		}
		AITEST_EQUAL(TEXT("The tag observer is expected to run just after FEntityCreationContext's destruction"), AffectedEntities.Num(), ArrayMidPoint);

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FEntityCreation_Individuals, "System.Mass.Observer.Create.TagInvididualEntities");

struct FEntityCreation_Batched : FTagBaseOperation
{
	FEntityCreation_Batched() { OperationObserved = EMassObservedOperation::Add; }

	virtual bool InstantTest() override
	{
		constexpr int32 EntitiesToSpawnCount = 6;

		FMassObserverManager& ObserverManager = EntityManager->GetObserverManager();
		ObserverManager.AddObserverInstance(*FTagStruct::StaticStruct(), OperationObserved, *ObserverProcessor);

		{
			TSharedRef<FMassEntityManager::FEntityCreationContext> CreationContext = EntityManager->BatchCreateEntities(IntsArchetype, EntitiesToSpawnCount, EntitiesInt);

			EntityManager->BatchChangeTagsForEntities(CreationContext->GetEntityCollections(*EntityManager.Get()), FMassTagBitSet(*FTagStruct::StaticStruct()), FMassTagBitSet());
			AITEST_TRUE(TEXT("The tag observer is not expected to run yet"), AffectedEntities.Num() == 0);
			AITEST_FALSE(TEXT("CreationContext's entity collection should be invalidated at this moment"), CreationContext->DebugAreEntityCollectionsUpToDate());

			EntityManager->BatchChangeTagsForEntities(CreationContext->GetEntityCollections(*EntityManager.Get()), FMassTagBitSet(*FTagStruct::StaticStruct()), FMassTagBitSet());
			AITEST_TRUE(TEXT("The tag observer is still not expected to run"), AffectedEntities.Num() == 0);
		}
		AITEST_TRUE(TEXT("The tag observer is expected to run just after FEntityCreationContext's destruction"), AffectedEntities.Num() > 0);
		AITEST_EQUAL(TEXT("The tag observer is expected to process every entity just once"), AffectedEntities.Num(), EntitiesInt.Num());

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FEntityCreation_Batched, "System.Mass.Observer.Create.TagBatchedEntities");

//-----------------------------------------------------------------------------
// fragments
//-----------------------------------------------------------------------------
struct FFragmentTestBase : FEntityTestBase
{
	using FFragmentStruct = FTestFragment_Float;

	TArray<FMassEntityHandle> AffectedEntities;
	UMassTestProcessorBase* ObserverProcessor = nullptr;
	EMassObservedOperation OperationObserved = EMassObservedOperation::MAX;
	TArray<FMassEntityHandle> EntitiesFloats;
	TArray<FMassEntityHandle> EntitiesInt;
	TArray<FMassEntityHandle> EntitiesIntsFloat;
	TArray<FMassEntityHandle> ExpectedEntities;
	bool bCommandsFlushed = false;

	// @return signifies if the test can continue
	virtual bool PerformOperation() { return false; }

	virtual bool SetUp() override
	{
		if (FEntityTestBase::SetUp())
		{
			ObserverProcessor = NewTestProcessor<UMassTestProcessorBase>(EntityManager);
			ObserverProcessor->EntityQuery.AddRequirement(FFragmentStruct::StaticStruct(), EMassFragmentAccess::ReadWrite);
			ObserverProcessor->ForEachEntityChunkExecutionFunction = [bCommandsFlushedPtr = &bCommandsFlushed, AffectedEntitiesPtr = &AffectedEntities](FMassExecutionContext& Context)
				{
					AffectedEntitiesPtr->Append(Context.GetEntities().GetData(), Context.GetEntities().Num());
					Context.Defer().PushCommand<FMassDeferredSetCommand>([&bCommandsFlushedPtr](FMassEntityManager&)
						{
							// dummy command, here just to catch if commands issue by observers got executed at all
							*bCommandsFlushedPtr = true;
						});
				};

			return true;
		}
		return false;
	}

	virtual bool InstantTest() override
	{
		EntityManager->BatchCreateEntities(IntsArchetype, 3, EntitiesInt);
		EntityManager->BatchCreateEntities(FloatsIntsArchetype, 3, EntitiesIntsFloat);

		FMassObserverManager& ObserverManager = EntityManager->GetObserverManager();
		ObserverManager.AddObserverInstance(*FFragmentStruct::StaticStruct(), OperationObserved, *ObserverProcessor);
				
		if (PerformOperation())
		{
			EntityManager->FlushCommands();
			AITEST_EQUAL(TEXT("The fragment observer is expected to be run for predicted number of entities"), AffectedEntities.Num(), ExpectedEntities.Num());
			AITEST_TRUE(TEXT("The commands issued by the observer are flushed"), bCommandsFlushed);

			ExpectedEntities.Sort(EntityIndexSorted);
			AffectedEntities.Sort(EntityIndexSorted);

			for (int i = 0; i < ExpectedEntities.Num(); ++i)
			{
				AITEST_EQUAL(TEXT("Expected and affected sets should be the same"), AffectedEntities[i], ExpectedEntities[i]);
			}
		}

		return true;
	}
};

struct FFragmentTest_SingleEntitySingleArchetypeAdd : FFragmentTestBase
{
	FFragmentTest_SingleEntitySingleArchetypeAdd() { OperationObserved = EMassObservedOperation::Add; }
	virtual bool PerformOperation() override
	{
		ExpectedEntities = { EntitiesInt[1] };
		EntityManager->Defer().AddFragment<FFragmentStruct>(EntitiesInt[1]);
		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FFragmentTest_SingleEntitySingleArchetypeAdd, "System.Mass.Observer.Fragment.SingleEntitySingleArchetypeAdd");

struct FFragmentTest_SingleEntitySingleArchetypeRemove : FFragmentTestBase
{
	FFragmentTest_SingleEntitySingleArchetypeRemove() { OperationObserved = EMassObservedOperation::Remove; }
	virtual bool PerformOperation() override
	{
		ExpectedEntities = { EntitiesInt[1] };

		EntityManager->Defer().AddFragment<FFragmentStruct>(EntitiesInt[1]);
		EntityManager->FlushCommands();
		// since we're only observing Fragment removal we don't expect AffectedEntities to contain any data at this point
		AITEST_EQUAL(TEXT("Fragment addition is not being observed and is not expected to produce results yet"), AffectedEntities.Num(), 0);
		EntityManager->Defer().RemoveFragment<FFragmentStruct>(EntitiesInt[1]);
		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FFragmentTest_SingleEntitySingleArchetypeRemove, "System.Mass.Observer.Fragment.SingleEntitySingleArchetypeRemove");

struct FFragmentTest_SingleEntitySingleArchetypeDestroy : FFragmentTestBase
{
	FFragmentTest_SingleEntitySingleArchetypeDestroy() { OperationObserved = EMassObservedOperation::Remove; }
	virtual bool PerformOperation() override
	{
		ExpectedEntities = { EntitiesInt[1] };
		EntityManager->Defer().AddFragment<FFragmentStruct>(EntitiesInt[1]);
		EntityManager->FlushCommands();
		// since we're only observing Fragment removal we don't expect AffectedEntities to contain any data at this point
		AITEST_EQUAL(TEXT("Fragment addition is not being observed and is not expected to produce results yet"), AffectedEntities.Num(), 0);
		EntityManager->Defer().DestroyEntity(EntitiesInt[1]);
		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FFragmentTest_SingleEntitySingleArchetypeDestroy, "System.Mass.Observer.Fragment.SingleEntitySingleArchetypeDestroy");

struct FFragmentTest_MultipleArchetypeAdd : FFragmentTestBase
{
	FFragmentTest_MultipleArchetypeAdd() { OperationObserved = EMassObservedOperation::Add; }

	virtual bool PerformOperation() override
	{
		ExpectedEntities = { EntitiesInt[0], EntitiesInt[2], EntitiesInt[1] };
		for (const FMassEntityHandle& ModifiedEntity : ExpectedEntities)
		{
			EntityManager->Defer().AddFragment<FFragmentStruct>(ModifiedEntity);
		}
		// also adding the fragment to the other archetype that already has the fragment. This should not yield any results
		for (const FMassEntityHandle& OtherEntity : EntitiesIntsFloat)
		{
			EntityManager->Defer().AddFragment<FFragmentStruct>(OtherEntity);
		}
		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FFragmentTest_MultipleArchetypeAdd, "System.Mass.Observer.Fragment.MultipleArchetypesAdd");

struct FFragmentTest_MultipleArchetypeRemove : FFragmentTestBase
{
	FFragmentTest_MultipleArchetypeRemove() { OperationObserved = EMassObservedOperation::Remove; }

	virtual bool PerformOperation() override
	{
		ExpectedEntities = { EntitiesInt[0], EntitiesInt[2], EntitiesIntsFloat[1] };
		for (const FMassEntityHandle& ModifiedEntity : ExpectedEntities)
		{
			EntityManager->Defer().AddFragment<FFragmentStruct>(ModifiedEntity);
		}
		EntityManager->FlushCommands();
		// since we're only observing Fragment removal we don't expect AffectedEntities to contain any data at this point
		AITEST_EQUAL(TEXT("Fragment addition is not being observed and is not expected to produce results yet"), AffectedEntities.Num(), 0);
		for (const FMassEntityHandle& ModifiedEntity : ExpectedEntities)
		{
			EntityManager->Defer().RemoveFragment<FFragmentStruct>(ModifiedEntity);
		}
		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FFragmentTest_MultipleArchetypeRemove, "System.Mass.Observer.Fragment.MultipleArchetypesRemove");

struct FFragmentTest_MultipleArchetypeDestroy : FFragmentTestBase
{
	FFragmentTest_MultipleArchetypeDestroy() { OperationObserved = EMassObservedOperation::Remove; }

	virtual bool PerformOperation() override
	{
		ExpectedEntities = { EntitiesInt[0], EntitiesInt[2], EntitiesIntsFloat[1] };
		for (const FMassEntityHandle& ModifiedEntity : ExpectedEntities)
		{
			EntityManager->Defer().AddFragment<FFragmentStruct>(ModifiedEntity);
		}
		EntityManager->FlushCommands();
		// since we're only observing Fragment removal we don't expect AffectedEntities to contain any data at this point
		AITEST_EQUAL(TEXT("Fragment addition is not being observed and is not expected to produce results yet"), AffectedEntities.Num(), 0);
		for (const FMassEntityHandle& ModifiedEntity : ExpectedEntities)
		{
			EntityManager->Defer().DestroyEntity(ModifiedEntity);
		}
		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FFragmentTest_MultipleArchetypeDestroy, "System.Mass.Observer.Fragment.MultipleArchetypesDestroy");

struct FFragmentTest_EntityCreation_Individual : FFragmentTestBase
{
	FFragmentTest_EntityCreation_Individual() { OperationObserved = EMassObservedOperation::Add; }

	virtual bool InstantTest() override
	{
		constexpr float TestValue = 123.456f;
		float ValueOnNotification = 0.f;

		ObserverProcessor->ForEachEntityChunkExecutionFunction = [&ValueOnNotification](FMassExecutionContext& Context) //-V1047 - This lambda is cleared before routine exit
			{
				const TConstArrayView<FFragmentStruct> Fragments = Context.GetFragmentView<FFragmentStruct>();
				for (int32 EntityIndex = 0; EntityIndex < Context.GetNumEntities(); EntityIndex++)
				{
					ValueOnNotification = Fragments[EntityIndex].Value;
				};
			};

		FMassObserverManager& ObserverManager = EntityManager->GetObserverManager();
		ObserverManager.AddObserverInstance(*FFragmentStruct::StaticStruct(), OperationObserved, *ObserverProcessor);

		TArray<FInstancedStruct> FragmentInstanceList = { FInstancedStruct::Make(FFragmentStruct(TestValue)) };

		// BuildEntity
		{
			const FMassEntityHandle Entity= EntityManager->ReserveEntity();
			EntityManager->BuildEntity(Entity, FragmentInstanceList);	
			AITEST_EQUAL(TEXT("The fragment observer notified by BuildEntity is expected to be able to fetch the initial value"), ValueOnNotification, TestValue);
			EntityManager->DestroyEntity(Entity);
		}

		// CreateEntity
		{
			ValueOnNotification = 0.f;
			const FMassEntityHandle Entity = EntityManager->CreateEntity(FragmentInstanceList);
			AITEST_EQUAL(TEXT("The fragment observer notified by CreateEntity is expected to be able to fetch the initial value"), ValueOnNotification, TestValue);
			EntityManager->DestroyEntity(Entity);
		}

		ObserverProcessor->ForEachEntityChunkExecutionFunction = nullptr;

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FFragmentTest_EntityCreation_Individual, "System.Mass.Observer.Create.FragmentSingleEntity");

struct FFragmentTest_EntityCreation_Individuals : FFragmentTestBase
{
	FFragmentTest_EntityCreation_Individuals() { OperationObserved = EMassObservedOperation::Add; }

	virtual bool InstantTest() override
	{
		constexpr int32 EntitiesToSpawnCount = 6;

		FMassObserverManager& ObserverManager = EntityManager->GetObserverManager();
		ObserverManager.AddObserverInstance(*FFragmentStruct::StaticStruct(), OperationObserved, *ObserverProcessor);

		int32 ArrayMidPoint = 0;
		{
			TSharedRef<FMassEntityManager::FEntityCreationContext> CreationContext = EntityManager->BatchCreateEntities(IntsArchetype, EntitiesToSpawnCount, EntitiesInt);
			ArrayMidPoint = EntitiesInt.Num() / 2;

			for (int32 Index = 0; Index < ArrayMidPoint; ++Index)
			{
				EntityManager->AddFragmentToEntity(EntitiesInt[Index], FFragmentStruct::StaticStruct());
			}
			AITEST_EQUAL(TEXT("The fragment observer is not expected to run yet"), AffectedEntities.Num(), 0);
		}
		AITEST_EQUAL(TEXT("The fragment observer is expected to run just after FEntityCreationContext's destruction"), AffectedEntities.Num(), ArrayMidPoint);

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FFragmentTest_EntityCreation_Individuals, "System.Mass.Observer.Create.FragmentIndividualEntities");

#if WITH_MASSENTITY_DEBUG
struct FObserverChangingComposition_Sync : FFragmentTestBase
{
	FObserverChangingComposition_Sync()
	{
		OperationObserved = EMassObservedOperation::Add;
	}

	virtual bool InstantTest() override
	{
		constexpr int32 EntitiesToSpawn = 3;
		const FMassArchetypeHandle OriginalArchetype = FloatsArchetype;

		FMassObserverManager& ObserverManager = EntityManager->GetObserverManager();
		ObserverManager.AddObserverInstance(*FFragmentStruct::StaticStruct(), OperationObserved, *ObserverProcessor);

		{
			AITEST_SCOPED_CHECK("Use asynchronous API instead", 1);

			ObserverProcessor->ForEachEntityChunkExecutionFunction = [EntityManager = EntityManager](FMassExecutionContext& Context)
			{
				EntityManager->AddFragmentToEntity(Context.GetEntity(0), FTestFragment_Int::StaticStruct());
			};

			EntityManager->BatchCreateEntities(OriginalArchetype, EntitiesToSpawn, EntitiesInt);

			AITEST_EQUAL("Number of entities in the original archetype, no moves expected", EntityManager->DebugGetArchetypeEntitiesCount(OriginalArchetype), EntitiesToSpawn);
		}
		{
			AITEST_SCOPED_CHECK("Use asynchronous API instead", 1);

			ObserverProcessor->ForEachEntityChunkExecutionFunction = [EntityManager = EntityManager, OriginalArchetype](FMassExecutionContext& Context)
			{
				FMassArchetypeEntityCollection EntityCollection(OriginalArchetype, Context.GetEntities(), FMassArchetypeEntityCollection::NoDuplicates);
				EntityManager->BatchChangeFragmentCompositionForEntities(MakeArrayView(&EntityCollection, 1)
					, FMassFragmentBitSet(*FTestFragment_Int::StaticStruct()), {});
			};

			EntityManager->BatchCreateEntities(OriginalArchetype, EntitiesToSpawn, EntitiesInt);

			AITEST_EQUAL("Number of entities in the original archetype, no moves expected", EntityManager->DebugGetArchetypeEntitiesCount(OriginalArchetype), EntitiesToSpawn * 2);
		}

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FObserverChangingComposition_Sync, "System.Mass.Observer.ChangingCompositionSync");

struct FObserverChangingComposition_Deferred : FFragmentTestBase
{
	FObserverChangingComposition_Deferred()
	{
		OperationObserved = EMassObservedOperation::Add;
	}

	virtual bool InstantTest() override
	{
		constexpr int32 EntitiesToSpawn = 3;
		const FMassArchetypeHandle OriginalArchetype = FloatsArchetype;

		ObserverProcessor->ForEachEntityChunkExecutionFunction = [](FMassExecutionContext& Context)
			{
				Context.Defer().PushCommand<FMassCommandAddFragments<FTestFragment_Int>>(Context.GetEntities());
			};

		FMassObserverManager& ObserverManager = EntityManager->GetObserverManager();
		ObserverManager.AddObserverInstance(*FFragmentStruct::StaticStruct(), OperationObserved, *ObserverProcessor);

		EntityManager->BatchCreateEntities(OriginalArchetype, EntitiesToSpawn, EntitiesInt);

		AITEST_EQUAL("Number of entities in the original archetype", EntityManager->DebugGetArchetypeEntitiesCount(OriginalArchetype), 0);
		AITEST_EQUAL("Number of entities in the target archetype", EntityManager->DebugGetArchetypeEntitiesCount(FloatsIntsArchetype), EntitiesToSpawn);

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FObserverChangingComposition_Deferred, "System.Mass.Observer.ChangingCompositionDeferred");
#endif // WITH_MASSENTITY_DEBUG

/**
 * This test aims to verify expected behavior of observers when there's a creation context active, when composition-mutating
 * operations are affecting entities other than the ones being created.
 */
struct FModificationsWhileCreationContextActive : FTagBaseOperation
{
	FModificationsWhileCreationContextActive() { OperationObserved = EMassObservedOperation::Add; }

	virtual bool InstantTest() override
	{
		constexpr int32 EntitiesToSpawnInFirstBatch = 3;
		constexpr int32 EntitiesToSpawnInSecondBatch = 5;

		EntityManager->BatchCreateEntities(IntsArchetype, EntitiesToSpawnInFirstBatch, EntitiesInt);
		FMassArchetypeEntityCollection InitialEntitiesCollection(IntsArchetype, EntitiesInt, FMassArchetypeEntityCollection::EDuplicatesHandling::NoDuplicates);

		FMassObserverManager& ObserverManager = EntityManager->GetObserverManager();
		ObserverManager.AddObserverInstance(*FTagStruct::StaticStruct(), OperationObserved, *ObserverProcessor);

		{
			TSharedRef<FMassObserverManager::FObserverLock> ObserversLock = EntityManager->GetOrMakeObserversLock();
			{
				TSharedRef<FMassEntityManager::FEntityCreationContext> CreationContext = EntityManager->BatchCreateEntities(IntsArchetype, EntitiesToSpawnInSecondBatch, EntitiesInt);
				ensure(EntitiesInt.Num() == EntitiesToSpawnInFirstBatch + EntitiesToSpawnInSecondBatch);
				// note that the observers' behavior regarding the entities just created gets tested by FEntityCreation_Batched test above
				// we're testing only the behavior related to the previously created entities here
			}
			EntityManager->BatchChangeTagsForEntities(MakeArrayView(&InitialEntitiesCollection, 1)
				, FMassTagBitSet(*FTagStruct::StaticStruct()), FMassTagBitSet());

			AITEST_TRUE(TEXT("The tag observer is not expected to run yet"), AffectedEntities.Num() == 0);
		}
		AITEST_TRUE(TEXT("The tag observer is expected to run just after FEntityCreationContext's destruction"), AffectedEntities.Num() > 0);
		AITEST_EQUAL(TEXT("The tag observer is expected to process only the original entities, that had a tag added to them"), AffectedEntities.Num(), EntitiesToSpawnInFirstBatch);

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FModificationsWhileCreationContextActive, "System.Mass.Observer.Create.ModificationsToOtherEntities");

struct FCreationOperationOrder : FFragmentTestBase
{
	int32 Counter = 0;

	FCreationOperationOrder()
	{
		OperationObserved = EMassObservedOperation::Add;
	}

	virtual void BuildScenario(TArray<FMassEntityHandle>& PreExistingEntities, TArray<FMassEntityHandle>& NewEntities) = 0;

	virtual bool InstantTest() override
	{
		TArray<FMassEntityHandle> PreExistingEntities;
		TArray<FMassEntityHandle> NewEntities;

		ObserverProcessor->ForEachEntityChunkExecutionFunction = [Counter = &this->Counter](FMassExecutionContext& Context)
			{
				for (auto EntityId : Context.CreateEntityIterator())
				{
					Context.GetMutableFragmentView<FFragmentStruct>()[EntityId].Value = static_cast<float>(++(*Counter));
				}
			};

		EntityManager->BatchCreateEntities(IntsArchetype, 2, PreExistingEntities);

		FMassObserverManager& ObserverManager = EntityManager->GetObserverManager();
		ObserverManager.AddObserverInstance(*FFragmentStruct::StaticStruct(), OperationObserved, *ObserverProcessor);

		BuildScenario(PreExistingEntities, NewEntities);

		// the specific order of entities handled within a single creation context doesn't
		// need to match the assumed order
		float FirstBatchValues[] = {
			EntityManager->GetFragmentDataChecked<FTestFragment_Float>(NewEntities[0]).Value
			, EntityManager->GetFragmentDataChecked<FTestFragment_Float>(NewEntities[1]).Value
		};
		AITEST_TRUE("First batch's values match", (FirstBatchValues[0] == 1.f && FirstBatchValues[1] == 2.f) || (FirstBatchValues[1] == 1.f && FirstBatchValues[0] == 2.f));

		float PreExistingEntitiesValues[] = {
			EntityManager->GetFragmentDataChecked<FTestFragment_Float>(PreExistingEntities[0]).Value
			, EntityManager->GetFragmentDataChecked<FTestFragment_Float>(PreExistingEntities[1]).Value
		};
		AITEST_EQUAL("First preexisting entity's value", PreExistingEntitiesValues[0], 3.f);
		AITEST_EQUAL("Second preexisting entity's value", PreExistingEntitiesValues[1], 6.f);

		float SecondBatchBatchValues[] = {
			EntityManager->GetFragmentDataChecked<FTestFragment_Float>(NewEntities[2]).Value
			, EntityManager->GetFragmentDataChecked<FTestFragment_Float>(NewEntities[3]).Value
		};
		AITEST_TRUE("First batch's values match", (SecondBatchBatchValues[0] == 4.f && SecondBatchBatchValues[1] == 5.f) || (SecondBatchBatchValues[1] == 4.f && SecondBatchBatchValues[0] == 5.f));

		return true;
	}
};

struct FCreationOperationOrder_Batch : FCreationOperationOrder
{
	virtual void BuildScenario(TArray<FMassEntityHandle>& PreExistingEntities, TArray<FMassEntityHandle>& NewEntities) override
	{
		TSharedRef<FMassObserverManager::FObserverLock> ObserversLock = EntityManager->GetOrMakeObserversLock();
		{
			// creating two separate entities, that should end up in the same creation context
			TSharedRef<FMassEntityManager::FEntityCreationContext> CreationContext = EntityManager->BatchCreateEntities(FloatsArchetype, 1, NewEntities);
			EntityManager->BatchCreateEntities(FloatsArchetype, 1, NewEntities);
		}
		{
			FMassArchetypeEntityCollection Collection(IntsArchetype, MakeArrayView(&PreExistingEntities[0], 1), FMassArchetypeEntityCollection::NoDuplicates);
			EntityManager->BatchChangeFragmentCompositionForEntities(MakeArrayView(&Collection, 1), FMassFragmentBitSet(*FTestFragment_Float::StaticStruct()), {});
		}
		{
			// creating two separate entities, that should end up in the same creation context
			TSharedRef<FMassEntityManager::FEntityCreationContext> CreationContext = EntityManager->BatchCreateEntities(FloatsArchetype, 1, NewEntities);
			EntityManager->BatchCreateEntities(FloatsArchetype, 1, NewEntities);
		}
		{
			FMassArchetypeEntityCollection Collection(IntsArchetype, MakeArrayView(&PreExistingEntities[1], 1), FMassArchetypeEntityCollection::NoDuplicates);
			EntityManager->BatchChangeFragmentCompositionForEntities(MakeArrayView(&Collection, 1), FMassFragmentBitSet(*FTestFragment_Float::StaticStruct()), {});
		}
	}
};
IMPLEMENT_AI_INSTANT_TEST(FCreationOperationOrder_Batch, "System.Mass.Observer.Create.CreationOperationOrder.Batch");

struct FCreationOperationOrder_Individual : FCreationOperationOrder
{
	virtual void BuildScenario(TArray<FMassEntityHandle>& PreExistingEntities, TArray<FMassEntityHandle>& NewEntities) override
	{
		TSharedRef<FMassObserverManager::FObserverLock> ObserversLock = EntityManager->GetOrMakeObserversLock();
		{
			// creating two separate entities, that should end up in the same creation context
			TSharedRef<FMassEntityManager::FEntityCreationContext> CreationContext = EntityManager->GetOrMakeCreationContext();
			NewEntities.Add(EntityManager->CreateEntity(FloatsArchetype));
			NewEntities.Add(EntityManager->CreateEntity(FloatsArchetype));
		}
		EntityManager->AddFragmentToEntity(PreExistingEntities[0], FTestFragment_Float::StaticStruct());
		{
			// creating two separate entities, that should end up in the same creation context
			TSharedRef<FMassEntityManager::FEntityCreationContext> CreationContext = EntityManager->GetOrMakeCreationContext();
			NewEntities.Add(EntityManager->CreateEntity(FloatsArchetype));
			NewEntities.Add(EntityManager->CreateEntity(FloatsArchetype));
		}
		EntityManager->AddFragmentToEntity(PreExistingEntities[1], FTestFragment_Float::StaticStruct());
	}
};
IMPLEMENT_AI_INSTANT_TEST(FCreationOperationOrder_Individual, "System.Mass.Observer.Create.CreationOperationOrder.Individual");

// The scenario being tested:
// 1. Create entities with Float fragment
// 2. Add an unobserved tag A - results in created entities changing archetype
// 3. Release the creation context - we expect Float observers to trigger
struct FCreatedEntitiesUnobservedCompositionChange : FFragmentTestBase
{
	FCreatedEntitiesUnobservedCompositionChange() { OperationObserved = EMassObservedOperation::Add; }

	virtual bool InstantTest() override
	{
		constexpr int32 EntitiesToSpawnCount = 6;

		FMassObserverManager& ObserverManager = EntityManager->GetObserverManager();
		ObserverManager.AddObserverInstance(*FFragmentStruct::StaticStruct(), OperationObserved, *ObserverProcessor);
		{
			TSharedRef<FMassEntityManager::FEntityCreationContext> CreationContext = EntityManager->BatchCreateEntities(FloatsArchetype, EntitiesToSpawnCount, EntitiesFloats);

			// add unobserved tag
			EntityManager->BatchChangeTagsForEntities(CreationContext->GetEntityCollections(*EntityManager.Get()), FMassTagBitSet(*FTestTag_A::StaticStruct()), FMassTagBitSet());

			AITEST_EQUAL(TEXT("The fragment observer is not expected to run yet"), AffectedEntities.Num(), 0);
		}
		AITEST_EQUAL(TEXT("The fragment observer is expected to run just after FEntityCreationContext's destruction"), AffectedEntities.Num(), EntitiesToSpawnCount);

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FCreatedEntitiesUnobservedCompositionChange, "System.Mass.Observer.Create.UnobservedCompositionChange");


struct FMoveToAnotherArchetype_SingleEntity : FEntityTestBase
{
	bool bTagAdded = false;
	bool bTagRemoved = false;
	bool bFloatAdded = false;
	bool bFloatRemoved = false;

	virtual bool SetUp() override
	{
		if (FEntityTestBase::SetUp() == false)
		{
			return false;
		}

		FMassObserverManager& ObserverManager = EntityManager->GetObserverManager();

		auto CreateObserver = [this](const TFunction<void(FMassExecutionContext& Context)>& StoreResultFunction)
		{
			UMassTestProcessorBase* ObserverProcessor = NewTestProcessor<UMassTestProcessorBase>(EntityManager);
			ObserverProcessor->EntityQuery.AddRequirement<FTestFragment_Int>(EMassFragmentAccess::ReadOnly);
			ObserverProcessor->ForEachEntityChunkExecutionFunction = StoreResultFunction;
			return ObserverProcessor;
		};

		ObserverManager.AddObserverInstance(*FTestTag_A::StaticStruct(), EMassObservedOperation::Add
			, *CreateObserver([this](FMassExecutionContext& Context)
				{
					bTagAdded = true;
				}
			));

		ObserverManager.AddObserverInstance(*FTestTag_A::StaticStruct(), EMassObservedOperation::Remove
			, *CreateObserver([this](FMassExecutionContext& Context)
				{
					bTagRemoved = true;
				}
			));

		ObserverManager.AddObserverInstance(*FTestFragment_Float::StaticStruct(), EMassObservedOperation::Add
			, *CreateObserver([this](FMassExecutionContext& Context)
				{
					bFloatAdded = true;
				}
			));

		ObserverManager.AddObserverInstance(*FTestFragment_Float::StaticStruct(), EMassObservedOperation::Remove
			, *CreateObserver([this](FMassExecutionContext& Context)
				{
					bFloatRemoved = true;
				}
			));

		return true;
	}

	virtual bool InstantTest() override
	{
		const FMassEntityHandle EntityHandle = EntityManager->CreateEntity(IntsArchetype);

		// create target archetype
		const FMassArchetypeHandle TargetArchetypeHandle = EntityManager->CreateArchetype(IntsArchetype, { FTestTag_A::StaticStruct(), FTestFragment_Float::StaticStruct() });

		EntityManager->MoveEntityToAnotherArchetype(EntityHandle, TargetArchetypeHandle);
		AITEST_TRUE("Tag addition observer has been executed", bTagAdded);
		AITEST_TRUE("Fragment addition observer has been executed", bFloatAdded);
		AITEST_FALSE("(NOT) Tag removal observer has been executed", bTagRemoved);
		AITEST_FALSE("(NOT) Fragment addition observer has been executed", bFloatRemoved);

		// moving back to the original archetype will remove the two added elements, and should trigger observers
		EntityManager->MoveEntityToAnotherArchetype(EntityHandle, IntsArchetype);
		AITEST_TRUE("Tag removal observer has been executed", bTagRemoved);
		AITEST_TRUE("Fragment addition observer has been executed", bFloatRemoved);

		return true;
	}
};
IMPLEMENT_AI_INSTANT_TEST(FMoveToAnotherArchetype_SingleEntity, "System.Mass.Observer.MoveToAnotherArchetype");
//
//struct FRecursiveObserver : FEntityTestBase
//{
//	bool bTagAdded = false;
//	bool bTagRemoved = false;
//	
//	virtual bool SetUp() override
//	{
//		if (FEntityTestBase::SetUp() == false)
//		{
//			return false;
//		}
//
//		FMassObserverManager& ObserverManager = EntityManager->GetObserverManager();
//
//		auto CreateObserver = [this](const TFunction<void(FMassExecutionContext& Context)>& StoreResultFunction)
//		{
//			UMassTestProcessorBase* ObserverProcessor = NewTestProcessor<UMassTestProcessorBase>(EntityManager);
//			ObserverProcessor->EntityQuery.AddRequirement<FTestFragment_Int>(EMassFragmentAccess::ReadOnly);
//			ObserverProcessor->ForEachEntityChunkExecutionFunction = StoreResultFunction;
//			return ObserverProcessor;
//		};
//
//		ObserverManager.AddObserverInstance(*FTestTag_A::StaticStruct(), EMassObservedOperation::Add
//			, *CreateObserver([this](FMassExecutionContext& Context)
//				{
//					bTagAdded = true;
//				}
//			));
//
//		ObserverManager.AddObserverInstance(*FTestTag_A::StaticStruct(), EMassObservedOperation::Remove
//			, *CreateObserver([this](FMassExecutionContext& Context)
//				{
//					bTagRemoved = true;
//				}
//			));
//
//		return true;
//	}
//
//	virtual bool InstantTest() override
//	{
//		const FMassEntityHandle EntityHandle = EntityManager->CreateEntity(IntsArchetype);
//
//		// create target archetype
//		const FMassArchetypeHandle TargetArchetypeHandle = EntityManager->CreateArchetype(IntsArchetype, { FTestTag_A::StaticStruct(), FTestFragment_Float::StaticStruct() });
//
//		EntityManager->MoveEntityToAnotherArchetype(EntityHandle, TargetArchetypeHandle);
//		AITEST_TRUE("Tag addition observer has been executed", bTagAdded);
//		AITEST_TRUE("Fragment addition observer has been executed", bFloatAdded);
//		AITEST_FALSE("(NOT) Tag removal observer has been executed", bTagRemoved);
//		AITEST_FALSE("(NOT) Fragment addition observer has been executed", bFloatRemoved);
//
//		// moving back to the original archetype will remove the two added elements, and should trigger observers
//		EntityManager->MoveEntityToAnotherArchetype(EntityHandle, IntsArchetype);
//		AITEST_TRUE("Tag removal observer has been executed", bTagRemoved);
//		AITEST_TRUE("Fragment addition observer has been executed", bFloatRemoved);
//
//		return true;
//	}
//};
//IMPLEMENT_AI_INSTANT_TEST(FMoveToAnotherArchetype_SingleEntity, "System.Mass.Observer.Recursive");

} // UE::Mass::Test::Observers

UE_ENABLE_OPTIMIZATION_SHIP

#undef LOCTEXT_NAMESPACE
