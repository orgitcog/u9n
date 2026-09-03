// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassDebugger.h"
#if WITH_MASSENTITY_DEBUG
#include "Algo/ForEach.h"
#include "MassProcessor.h"
#include "MassEntityManager.h"
#include "MassEntityManagerStorage.h"
#include "MassEntitySubsystem.h"
#include "MassArchetypeTypes.h"
#include "MassArchetypeData.h"
#include "MassRequirements.h"
#include "MassEntityQuery.h"
#include "Misc/OutputDevice.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "MassEntityUtils.h"
#include "MassCommandBuffer.h"
#include "MassEntityTrace.h"
#include "Misc/MessageDialog.h"
#include "HAL/PlatformMisc.h"
#include "ProfilingDebugging/TraceAuxiliary.h"

#define LOCTEXT_NAMESPACE "MassDebugger"

namespace UE::Mass::Debug
{
	bool bAllowProceduralDebuggedEntitySelection = false;
	bool bAllowBreakOnDebuggedEntity = false;
	bool bTestSelectedEntityAgainstProcessorQueries = true;

	FAutoConsoleVariableRef CVars[] =
	{
		{ TEXT("mass.debug.AllowProceduralDebuggedEntitySelection"), bAllowProceduralDebuggedEntitySelection
			, TEXT("Guards whether MASS_SET_ENTITY_DEBUGGED calls take effect."), ECVF_Cheat}
		, {TEXT("mass.debug.AllowBreakOnDebuggedEntity"), bAllowBreakOnDebuggedEntity
			, TEXT("Guards whether MASS_BREAK_IF_ENTITY_DEBUGGED calls take effect."), ECVF_Cheat}
		, {	TEXT("mass.debug.TestSelectedEntityAgainstProcessorQueries"), bTestSelectedEntityAgainstProcessorQueries
			, TEXT("Enabling will result in testing all processors' queries against SelectedEntity (as indicated by")
			TEXT("mass.debug.DebugEntity or the gameplay debugger) and storing potential failure results to be viewed in MassDebugger")
			, ECVF_Cheat }
	};
	

	FString DebugGetFragmentAccessString(EMassFragmentAccess Access)
	{
		switch (Access)
		{
		case EMassFragmentAccess::None:	return TEXT("--");
		case EMassFragmentAccess::ReadOnly:	return TEXT("RO");
		case EMassFragmentAccess::ReadWrite:	return TEXT("RW");
		default:
			ensureMsgf(false, TEXT("Missing string conversion for EMassFragmentAccess=%d"), Access);
			break;
		}
		return TEXT("Missing string conversion");
	}

	void DebugOutputDescription(TConstArrayView<UMassProcessor*> Processors, FOutputDevice& Ar)
	{
		const bool bAutoLineEnd = Ar.GetAutoEmitLineTerminator();
		Ar.SetAutoEmitLineTerminator(false);
		for (const UMassProcessor* Proc : Processors)
		{
			if (Proc)
			{
				Proc->DebugOutputDescription(Ar);
				Ar.Logf(TEXT("\n"));
			}
			else
			{
				Ar.Logf(TEXT("NULL\n"));
			}
		}
		Ar.SetAutoEmitLineTerminator(bAutoLineEnd);
	}

	// First Id of a range of lightweight entity for which we want to activate debug information
	int32 DebugEntityBegin = INDEX_NONE;

	// Last Id of a range of lightweight entity for which we want to activate debug information
	int32 DebugEntityEnd = INDEX_NONE;

	void SetDebugEntityRange(const int32 InDebugEntityBegin, const int32 InDebugEntityEnd)
	{
		DebugEntityBegin = InDebugEntityBegin;
		DebugEntityEnd = InDebugEntityEnd;
	}

	static FAutoConsoleCommand SetDebugEntityRangeCommand(
		TEXT("mass.debug.SetDebugEntityRange"),
		TEXT("Range of lightweight entity IDs that we want to debug.")
		TEXT("Usage: \"mass.debug.SetDebugEntityRange <FirstEntity> <LastEntity>\""),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
			{
				if (Args.Num() != 2)
				{
					UE_LOG(LogConsoleResponse, Display, TEXT("Error: Expecting 2 parameters"));
					return;
				}

				int32 FirstID = INDEX_NONE;
				int32 LastID = INDEX_NONE;
				if (!LexTryParseString<int32>(FirstID, *Args[0]))
				{
					UE_LOG(LogConsoleResponse, Display, TEXT("Error: first parameter must be an integer"));
					return;
				}
			
				if (!LexTryParseString<int32>(LastID, *Args[1]))
				{
					UE_LOG(LogConsoleResponse, Display, TEXT("Error: second parameter must be an integer"));
					return;
				}

				SetDebugEntityRange(FirstID, LastID);
			}));

	static FAutoConsoleCommand ResetDebugEntity(
		TEXT("mass.debug.ResetDebugEntity"),
		TEXT("Disables lightweight entities debugging.")
		TEXT("Usage: \"mass.debug.ResetDebugEntity\""),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
			{
				SetDebugEntityRange(INDEX_NONE, INDEX_NONE);
			}));

	bool HasDebugEntities()
	{
		return DebugEntityBegin != INDEX_NONE && DebugEntityEnd != INDEX_NONE;
	}

	bool IsDebuggingSingleEntity()
	{
		return DebugEntityBegin != INDEX_NONE && DebugEntityBegin == DebugEntityEnd;
	}

	bool GetDebugEntitiesRange(int32& OutBegin, int32& OutEnd)
	{
		OutBegin = DebugEntityBegin;
		OutEnd = DebugEntityEnd;
		return DebugEntityBegin != INDEX_NONE && DebugEntityEnd != INDEX_NONE && DebugEntityBegin <= DebugEntityEnd;
	}
	
	bool IsDebuggingEntity(FMassEntityHandle Entity, FColor* OutEntityColor)
	{
		const int32 EntityIdx = Entity.Index;
		const bool bIsDebuggingEntity = (DebugEntityBegin != INDEX_NONE && DebugEntityEnd != INDEX_NONE && DebugEntityBegin <= EntityIdx && EntityIdx <= DebugEntityEnd);
	
		if (bIsDebuggingEntity && OutEntityColor != nullptr)
		{
			*OutEntityColor = GetEntityDebugColor(Entity);
		}

		return bIsDebuggingEntity;
	}

	FColor GetEntityDebugColor(FMassEntityHandle Entity)
	{
		const int32 EntityIdx = Entity.Index;
		return EntityIdx != INDEX_NONE ? GColorList.GetFColorByIndex(EntityIdx % GColorList.GetColorsNum()) : FColor::Black;
	}

	FAutoConsoleCommandWithWorldArgsAndOutputDevice PrintEntityFragmentsCmd(
		TEXT("mass.PrintEntityFragments"),
		TEXT("Prints all fragment types and values (uproperties) for the specified Entity index"),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda(
			[](const TArray<FString>& Params, UWorld* World, FOutputDevice& Ar)
			{
				check(World);
				if (UMassEntitySubsystem* EntityManager = World->GetSubsystem<UMassEntitySubsystem>())
				{
					int32 Index = INDEX_NONE;
					if (LexTryParseString<int32>(Index, *Params[0]))
					{
						FMassDebugger::OutputEntityDescription(Ar, EntityManager->GetEntityManager(), Index);
					}
					else
					{
						Ar.Logf(ELogVerbosity::Error, TEXT("Entity index parameter must be an integer"));
					}
				}
				else
				{
					Ar.Logf(ELogVerbosity::Error, TEXT("Failed to find MassEntitySubsystem for world %s"), *GetPathNameSafe(World));
				}
			})
	);

	FAutoConsoleCommandWithWorldArgsAndOutputDevice LogArchetypesCmd(
		TEXT("mass.LogArchetypes"),
		TEXT("Dumps description of archetypes to log. Optional parameter controls whether to include or exclude non-occupied archetypes. Defaults to 'include'."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda([](const TArray<FString>& Params, UWorld*, FOutputDevice& Ar)
			{
				const TIndirectArray<FWorldContext>& WorldContexts = GEngine->GetWorldContexts();
				for (const FWorldContext& Context : WorldContexts)
				{
					UWorld* World = Context.World();
					if (World == nullptr || World->IsPreviewWorld())
					{
						continue;
					}

					Ar.Logf(ELogVerbosity::Log, TEXT("Dumping description of archetypes for world: %s (%s - %s)"),
						*GetPathNameSafe(World),
						LexToString(World->WorldType),
						*ToString(World->GetNetMode()));

					if (UMassEntitySubsystem* EntityManager = World->GetSubsystem<UMassEntitySubsystem>())
					{
						bool bIncludeEmpty = true;
						if (Params.Num())
						{
							LexTryParseString(bIncludeEmpty, *Params[0]);
						}
						Ar.Logf(ELogVerbosity::Log, TEXT("Include empty archetypes: %s"), bIncludeEmpty ? TEXT("TRUE") : TEXT("FALSE"));
						EntityManager->GetEntityManager().DebugGetArchetypesStringDetails(Ar, bIncludeEmpty);
					}
					else
					{
						Ar.Logf(ELogVerbosity::Error, TEXT("Failed to find MassEntitySubsystem for world: %s (%s - %s)"),
							*GetPathNameSafe(World),
							LexToString(World->WorldType),
							*ToString(World->GetNetMode()));
					}
				}
			})
	);

	// @todo these console commands will be reparented to "massentities" domain once we rename and shuffle the modules around 
	FAutoConsoleCommandWithWorld RecacheQueries(
		TEXT("mass.RecacheQueries"),
		TEXT("Forces EntityQueries to recache their valid archetypes"),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* InWorld)
			{
				check(InWorld);
				if (UMassEntitySubsystem* System = InWorld->GetSubsystem<UMassEntitySubsystem>())
				{
					System->GetMutableEntityManager().DebugForceArchetypeDataVersionBump();
				}
			}
	));

	FAutoConsoleCommandWithWorldArgsAndOutputDevice LogFragmentSizes(
		TEXT("mass.LogFragmentSizes"),
		TEXT("Logs all the fragment types being used along with their sizes."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda([](const TArray<FString>& Params, UWorld* World, FOutputDevice& Ar)
			{
				for (const TWeakObjectPtr<const UScriptStruct>& WeakStruct : FMassFragmentBitSet::DebugGetAllStructTypes())
				{
					if (const UScriptStruct* StructType = WeakStruct.Get())
					{
						Ar.Logf(ELogVerbosity::Log, TEXT("%s, size: %d"), *StructType->GetName(), StructType->GetStructureSize());
					}
				}
			})
	);

	FAutoConsoleCommandWithWorldArgsAndOutputDevice LogMemoryUsage(
		TEXT("mass.LogMemoryUsage"),
		TEXT("Logs how much memory the mass entity system uses"),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda([](const TArray<FString>& Params, UWorld* World, FOutputDevice& Ar)
			{
				check(World);
				if (UMassEntitySubsystem* System = World->GetSubsystem<UMassEntitySubsystem>())
				{
					FResourceSizeEx CumulativeResourceSize;
					System->GetResourceSizeEx(CumulativeResourceSize);
					Ar.Logf(ELogVerbosity::Log, TEXT("MassEntity system uses: %d bytes"), CumulativeResourceSize.GetDedicatedSystemMemoryBytes());
				}
			}));

	FAutoConsoleCommandWithOutputDevice LogFragments(
		TEXT("mass.LogKnownFragments"),
		TEXT("Logs all the known tags and fragments along with their \"index\" as stored via bitsets."),
		FConsoleCommandWithOutputDeviceDelegate::CreateStatic([](FOutputDevice& OutputDevice)
			{
				auto PrintKnownTypes = [&OutputDevice](TConstArrayView<TWeakObjectPtr<const UScriptStruct>> AllStructs) {
					int i = 0;
					for (TWeakObjectPtr<const UScriptStruct> Struct : AllStructs)
					{
						if (Struct.IsValid())
						{
							OutputDevice.Logf(TEXT("\t%d. %s"), i++, *Struct->GetName());
						}
					}
				};

				OutputDevice.Logf(TEXT("Known tags:"));
				PrintKnownTypes(FMassTagBitSet::DebugGetAllStructTypes());

				OutputDevice.Logf(TEXT("Known Fragments:"));
				PrintKnownTypes(FMassFragmentBitSet::DebugGetAllStructTypes());

				OutputDevice.Logf(TEXT("Known Shared Fragments:"));
				PrintKnownTypes(FMassSharedFragmentBitSet::DebugGetAllStructTypes());

				OutputDevice.Logf(TEXT("Known Chunk Fragments:"));
				PrintKnownTypes(FMassChunkFragmentBitSet::DebugGetAllStructTypes());
			}));

	static FAutoConsoleCommandWithWorldAndArgs DestroyEntity(
		TEXT("mass.debug.DestroyEntity"),
		TEXT("ID of a Mass entity that we want to destroy.")
		TEXT("Usage: \"mass.debug.DestoryEntity <Entity>\""),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() != 1)
		{
			UE_LOG(LogConsoleResponse, Display, TEXT("Error: Expecting 1 parameter"));
			return;
		}

		int32 ID = INDEX_NONE;
		if (!LexTryParseString<int32>(ID, *Args[0]))
		{
			UE_LOG(LogConsoleResponse, Display, TEXT("Error: parameter must be an integer"));
			return;
		}

		if (!World)
		{
			UE_LOG(LogConsoleResponse, Display, TEXT("Error: invalid world"));
			return;
		}

		FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*World);
		FMassEntityHandle EntityToDestroy = EntityManager.DebugGetEntityIndexHandle(ID);
		if (!EntityToDestroy.IsSet())
		{
			UE_LOG(LogConsoleResponse, Display, TEXT("Error: cannot find entity for this index"));
			return;
		}

		EntityManager.Defer().DestroyEntity(EntityToDestroy);
	}));

	static FAutoConsoleCommandWithWorldAndArgs SetDebugEntity(
		TEXT("mass.debug.DebugEntity"),
		TEXT("ID of a Mass entity that we want to debug.")
		TEXT("Note that this call results in the same behavior as if the entity was picked via the Mass GameplayDebugger's category.")
		TEXT("Usage: \"mass.debug.DebugEntity <Entity>\""),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (!World)
			{
				UE_LOG(LogConsoleResponse, Display, TEXT("Error: invalid world"));
				return;
			}

			int32 ID = INDEX_NONE;
			if (Args.Num() > 0)
			{
				LexTryParseString<int32>(ID, *Args[0]);
			}

			SetDebugEntityRange(ID, ID);

			FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*World);
			FMassEntityHandle EntityToDebug = EntityManager.DebugGetEntityIndexHandle(ID);
			if (!EntityToDebug.IsSet() && ID != INDEX_NONE)
			{
				UE_LOG(LogConsoleResponse, Display, TEXT("Cannot find entity for this index, clearing current selection"));
				return;
			}

			FMassDebugger::SelectEntity(EntityManager, EntityToDebug);
		}
	));

	const UScriptStruct* FindElementTypeByName(const FString& PartialFragmentName)
	{
		const UScriptStruct* Result = nullptr;
#if WITH_STRUCTUTILS_DEBUG
		Result = FMassFragmentBitSet::DebugFindTypeByPartialName(PartialFragmentName);
		if (Result == nullptr)
		{
			Result = FMassSharedFragmentBitSet::DebugFindTypeByPartialName(PartialFragmentName);
		}
		if (Result == nullptr)
		{
			Result = FMassConstSharedFragmentBitSet::DebugFindTypeByPartialName(PartialFragmentName);
		}
#endif // WITH_STRUCTUTILS_DEBUG
		return Result;
	}

	static FAutoConsoleCommandWithWorldAndArgs SetFragmentBreakpoint(
		TEXT("mass.debug.SetFragmentBreakpoint"),
		TEXT("Enables fragment write break-point on an arbitrary number of fragment types, on the selected entity (see `mass.debug.DebugEntity`).")
		TEXT("Usage: `mass.debug.SetFragmentBreakpoint <FragmentTypeName> <FragmentTypeName2> <FragmentTypeName3> <...>`"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (!World)
			{
				UE_LOG(LogConsoleResponse, Display, TEXT("Error: invalid world"));
				return;
			}

			if (Args.Num() == 0)
			{
				UE_LOG(LogConsoleResponse, Display, TEXT("No fragment types indicated"));
			}
			else
			{
				FMassEntityManager& EntityManager = Utils::GetEntityManagerChecked(*World);
				FMassEntityHandle SelectedEntity = FMassDebugger::GetSelectedEntity(EntityManager);
				if (SelectedEntity.IsValid())
				{
					for (const FString& PartialFragmentName : Args)
					{
						if (const UScriptStruct* FragmentType = FindElementTypeByName(PartialFragmentName))
						{
							FMassDebugger::SetFragmentWriteBreak(EntityManager, FragmentType, SelectedEntity);
						}
						else
						{
							UE_LOG(LogConsoleResponse, Display, TEXT("Warning: Unable to find element type %s"), *PartialFragmentName);
						}
					}
				}
				else
				{
					UE_LOG(LogConsoleResponse, Display, TEXT("Warning: No entity selected, no break points set"));
				}
			}
		}
	));

	static FAutoConsoleCommandWithWorldAndArgs ClearFragmentBreakpoint(
		TEXT("mass.debug.ClearFragmentBreakpoint"),
		TEXT("Clears fragment write break-point on an arbitrary number of fragment types, on the selected entity (see `mass.debug.DebugEntity`).")
		TEXT("If no entity is currently selected then the call will clear the type breakpoints on all entities.")
		TEXT("Usage: `mass.debug.ClearFragmentBreakpoint <FragmentTypeName> <FragmentTypeName2> <FragmentTypeName3> <...>`"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (!World)
			{
				UE_LOG(LogConsoleResponse, Display, TEXT("Error: invalid world"));
				return;
			}

			if (Args.Num() == 0)
			{
				UE_LOG(LogConsoleResponse, Display, TEXT("No fragment types indicated"));
			}
			else
			{
				FMassEntityManager& EntityManager = Utils::GetEntityManagerChecked(*World);
				FMassEntityHandle SelectedEntity = FMassDebugger::GetSelectedEntity(EntityManager);
				const bool bEntityValid = SelectedEntity.IsValid();

				for (const FString& PartialFragmentName : Args)
				{
					if (const UScriptStruct* FragmentType = FindElementTypeByName(PartialFragmentName))
					{
						bEntityValid
							? FMassDebugger::ClearFragmentWriteBreak(EntityManager, FragmentType, SelectedEntity)
							: FMassDebugger::ClearFragmentWriteBreak(EntityManager, FragmentType, FMassEntityHandle());
					}
					else
					{
						UE_LOG(LogConsoleResponse, Display, TEXT("Warning: Unable to find element type %s"), *PartialFragmentName);
					}
				}
			}
		}
	));

} // namespace UE::Mass::Debug

//----------------------------------------------------------------------//
// FMassDebugger
//----------------------------------------------------------------------//
FMassDebugger::FOnBreakpointsChanged FMassDebugger::OnBreakpointsChangedDelegate;
FMassDebugger::FOnEntitySelected FMassDebugger::OnEntitySelectedDelegate;

FMassDebugger::FOnMassEntityManagerEvent FMassDebugger::OnEntityManagerInitialized;
FMassDebugger::FOnMassEntityManagerEvent FMassDebugger::OnEntityManagerDeinitialized;
FMassDebugger::FOnEnvironmentEvent FMassDebugger::OnProcessorProviderRegistered;
FMassDebugger::FOnDebugEvent FMassDebugger::OnDebugEvent;
TArray<FMassDebugger::FEnvironment> FMassDebugger::ActiveEnvironments;
UE::FSpinLock FMassDebugger::EntityManagerRegistrationLock;
bool FMassDebugger::bHasBreakpoint = false;
TMap<FName, const UScriptStruct*> FMassDebugger::FragmentsByName;

TConstArrayView<FMassEntityQuery*> FMassDebugger::GetProcessorQueries(const UMassProcessor& Processor)
{
	return Processor.OwnedQueries;
}

TConstArrayView<FMassEntityQuery*> FMassDebugger::GetUpToDateProcessorQueries(const FMassEntityManager& EntityManager, UMassProcessor& Processor)
{
	for (FMassEntityQuery* Query : Processor.OwnedQueries)
	{
		if (Query)
		{
			Query->CacheArchetypes();
		}
	}

	return Processor.OwnedQueries;
}

UE::Mass::Debug::FQueryRequirementsView FMassDebugger::GetQueryRequirements(const FMassEntityQuery& Query)
{
	UE::Mass::Debug::FQueryRequirementsView View = { Query.FragmentRequirements, Query.ChunkFragmentRequirements, Query.ConstSharedFragmentRequirements, Query.SharedFragmentRequirements
		, Query.RequiredAllTags, Query.RequiredAnyTags, Query.RequiredNoneTags, Query.RequiredOptionalTags
		, Query.RequiredConstSubsystems, Query.RequiredMutableSubsystems };

	return View;
}

void FMassDebugger::GetQueryExecutionRequirements(const FMassEntityQuery& Query, FMassExecutionRequirements& OutExecutionRequirements)
{
	Query.ExportRequirements(OutExecutionRequirements);
}

TArray<FMassEntityHandle> FMassDebugger::GetEntitiesMatchingQuery(const FMassEntityManager& EntityManager, const FMassEntityQuery& Query)
{
	TArray<FMassEntityHandle> Entities;
	TArray<FMassArchetypeHandle> Archetypes;
	EntityManager.GetMatchingArchetypes(Query, Archetypes, 0);
	for (FMassArchetypeHandle& ArchHandle : Archetypes)
	{
		Entities.Append(GetEntitiesOfArchetype(ArchHandle));
	}
	return Entities;
}

void FMassDebugger::ForEachArchetype(const FMassEntityManager& EntityManager, const UE::Mass::Debug::FArchetypeFunction& Function)
{
	for (auto& KVP : EntityManager.FragmentHashToArchetypeMap)
	{
		for (const TSharedPtr<FMassArchetypeData>& Archetype : KVP.Value)
		{
			Function(FMassArchetypeHelper::ArchetypeHandleFromData(Archetype));
		}
	}
}

TArray<FMassArchetypeHandle> FMassDebugger::GetAllArchetypes(const FMassEntityManager& EntityManager)
{
	TArray<FMassArchetypeHandle> Archetypes;

	for (auto& KVP : EntityManager.FragmentHashToArchetypeMap)
	{
		for (const TSharedPtr<FMassArchetypeData>& Archetype : KVP.Value)
		{
			Archetypes.Add(FMassArchetypeHelper::ArchetypeHandleFromData(Archetype));
		}
	}

	return Archetypes;
}

const FMassArchetypeCompositionDescriptor& FMassDebugger::GetArchetypeComposition(const FMassArchetypeHandle& ArchetypeHandle)
{
	const FMassArchetypeData& ArchetypeData = FMassArchetypeHelper::ArchetypeDataFromHandleChecked(ArchetypeHandle);
	return ArchetypeData.CompositionDescriptor;
}

uint64 FMassDebugger::GetArchetypeTraceID(const FMassArchetypeData& ArchetypeData)
{
	return reinterpret_cast<uint64>(&ArchetypeData);
}

uint64 FMassDebugger::GetArchetypeTraceID(const FMassArchetypeHandle& ArchetypeHandle)
{ 
	const FMassArchetypeData& ArchetypeData = FMassArchetypeHelper::ArchetypeDataFromHandleChecked(ArchetypeHandle);
	return GetArchetypeTraceID(ArchetypeData);
}

TConstArrayView<FMassEntityHandle> FMassDebugger::GetEntitiesViewOfArchetype(const FMassArchetypeData& ArchetypeData, const FMassArchetypeChunk& Chunk)
{
	FMassArchetypeChunk& MutableChunk = const_cast<FMassArchetypeChunk&>(Chunk);
	TConstArrayView<FMassEntityHandle> View(&MutableChunk.GetEntityArrayElementRef(ArchetypeData.EntityListOffsetWithinChunk, 0), Chunk.GetNumInstances());
	return View;
}

const FMassArchetypeData* FMassDebugger::GetArchetypeData(const FMassArchetypeHandle& ArchetypeHandle)
{
	FMassArchetypeData* ArchetypeData = FMassArchetypeHelper::ArchetypeDataFromHandle(ArchetypeHandle);
	return ArchetypeData;
}

void FMassDebugger::EnumerateChunks(const FMassArchetypeData& Archetype, TFunctionRef<void(const FMassArchetypeChunk&)> Fn)
{
	for (const FMassArchetypeChunk& Chunk : Archetype.Chunks)
	{
		Fn(Chunk);
	}
}

void FMassDebugger::GetArchetypeEntityStats(const FMassArchetypeHandle& ArchetypeHandle, UE::Mass::Debug::FArchetypeStats& OutStats)
{
	const FMassArchetypeData& ArchetypeData = FMassArchetypeHelper::ArchetypeDataFromHandleChecked(ArchetypeHandle);
	OutStats.EntitiesCount = ArchetypeData.GetNumEntities();
	OutStats.EntitiesCountPerChunk = ArchetypeData.GetNumEntitiesPerChunk();
	OutStats.ChunksCount = ArchetypeData.GetChunkCount();
	OutStats.AllocatedSize = ArchetypeData.GetAllocatedSize();
	OutStats.BytesPerEntity = ArchetypeData.GetBytesPerEntity();

	SIZE_T ActiveChunksMemorySize = 0;
	SIZE_T ActiveEntitiesMemorySize = 0;
	ArchetypeData.DebugGetEntityMemoryNumbers(ActiveChunksMemorySize, ActiveEntitiesMemorySize);
	OutStats.WastedEntityMemory = ActiveChunksMemorySize - ActiveEntitiesMemorySize;
}

const TConstArrayView<FName> FMassDebugger::GetArchetypeDebugNames(const FMassArchetypeHandle& ArchetypeHandle)
{
	const FMassArchetypeData& ArchetypeData = FMassArchetypeHelper::ArchetypeDataFromHandleChecked(ArchetypeHandle);
	return ArchetypeData.GetDebugNames();
}

TArray<FMassEntityHandle> FMassDebugger::GetEntitiesOfArchetype(const FMassArchetypeHandle& ArchetypeHandle)
{
	TArray<FMassEntityHandle> EntitiesOfArchetype;
	FMassArchetypeData& ArchetypeData = FMassArchetypeHelper::ArchetypeDataFromHandleChecked(ArchetypeHandle);
	EntitiesOfArchetype.Reserve(ArchetypeData.GetNumEntities());
	for (FMassArchetypeChunk& Chunk : ArchetypeData.Chunks)
	{
		TArrayView<FMassEntityHandle> EntityListView = TArrayView<FMassEntityHandle>(&Chunk.GetEntityArrayElementRef(ArchetypeData.EntityListOffsetWithinChunk, 0), Chunk.GetNumInstances());
		EntitiesOfArchetype.Append(EntityListView);
	}
	return EntitiesOfArchetype;
}

TConstArrayView<UMassCompositeProcessor::FDependencyNode> FMassDebugger::GetProcessingGraph(const UMassCompositeProcessor& GraphOwner)
{
	return GraphOwner.FlatProcessingGraph;
}

TConstArrayView<TObjectPtr<UMassProcessor>> FMassDebugger::GetHostedProcessors(const UMassCompositeProcessor& GraphOwner)
{
	return GraphOwner.ChildPipeline.GetProcessors();
}

FString FMassDebugger::GetRequirementsDescription(const FMassFragmentRequirements& Requirements)
{
	TStringBuilder<256> StringBuilder;
	StringBuilder.Append(TEXT("<"));

	bool bNeedsComma = false;
	for (const FMassFragmentRequirementDescription& Requirement : Requirements.FragmentRequirements)
	{
		if (bNeedsComma)
		{
			StringBuilder.Append(TEXT(","));
		}
		StringBuilder.Append(*FMassDebugger::GetSingleRequirementDescription(Requirement));
		bNeedsComma = true;
	}

	StringBuilder.Append(TEXT(">"));
	return StringBuilder.ToString();
}

FString FMassDebugger::GetArchetypeRequirementCompatibilityDescription(const FMassFragmentRequirements& Requirements, const FMassArchetypeHandle& ArchetypeHandle)
{
	if (ArchetypeHandle.IsValid() == false)
	{
		return TEXT("Invalid");
	}

	const FMassArchetypeData& ArchetypeData = FMassArchetypeHelper::ArchetypeDataFromHandleChecked(ArchetypeHandle);
	return FMassDebugger::GetArchetypeRequirementCompatibilityDescription(Requirements, ArchetypeData.GetCompositionDescriptor());
}
	
FString FMassDebugger::GetArchetypeRequirementCompatibilityDescription(const FMassFragmentRequirements& Requirements, const FMassArchetypeCompositionDescriptor& ArchetypeComposition)
{
	FStringOutputDevice OutDescription;

	if (Requirements.HasNegativeRequirements())
	{
		if (ArchetypeComposition.Fragments.HasNone(Requirements.RequiredNoneFragments) == false)
		{
			// has some of the fragments required absent
			OutDescription += TEXT("\nHas fragments required absent: ");
			(Requirements.RequiredNoneFragments & ArchetypeComposition.Fragments).DebugGetStringDesc(OutDescription);
		}

		if (ArchetypeComposition.Tags.HasNone(Requirements.RequiredNoneTags) == false)
		{
			// has some of the tags required absent
			OutDescription += TEXT("\nHas tags required absent: ");
			(Requirements.RequiredNoneTags & ArchetypeComposition.Tags).DebugGetStringDesc(OutDescription);
		}

		if (ArchetypeComposition.ChunkFragments.HasNone(Requirements.RequiredNoneChunkFragments) == false)
		{
			// has some of the chunk fragments required absent
			OutDescription += TEXT("\nHas chunk fragments required absent: ");
			(Requirements.RequiredNoneChunkFragments & ArchetypeComposition.ChunkFragments).DebugGetStringDesc(OutDescription);
		}

		if (ArchetypeComposition.SharedFragments.HasNone(Requirements.RequiredNoneSharedFragments) == false)
		{
			// has some of the chunk fragments required absent
			OutDescription += TEXT("\nHas shared fragments required absent: ");
			(Requirements.RequiredNoneSharedFragments & ArchetypeComposition.SharedFragments).DebugGetStringDesc(OutDescription);
		}

		if (ArchetypeComposition.ConstSharedFragments.HasNone(Requirements.RequiredNoneConstSharedFragments) == false)
		{
			// has some of the chunk fragments required absent
			OutDescription += TEXT("\nHas shared fragments required absent: ");
			(Requirements.RequiredNoneConstSharedFragments & ArchetypeComposition.ConstSharedFragments).DebugGetStringDesc(OutDescription);
		}
	}

	// if we have regular (i.e. non-optional) positive requirements then these are the determining factor, we don't check optionals
	if (Requirements.HasPositiveRequirements())
	{
		if (ArchetypeComposition.Fragments.HasAll(Requirements.RequiredAllFragments) == false)
		{
			// missing one of the strictly required fragments
			OutDescription += TEXT("\nMissing required fragments: ");
			(Requirements.RequiredAllFragments - ArchetypeComposition.Fragments).DebugGetStringDesc(OutDescription);
		}

		if (Requirements.RequiredAnyFragments.IsEmpty() == false && ArchetypeComposition.Fragments.HasAny(Requirements.RequiredAnyFragments) == false)
		{
			// missing all of the "any" fragments
			OutDescription += TEXT("\nMissing all \'any\' fragments: ");
			Requirements.RequiredAnyFragments.DebugGetStringDesc(OutDescription);
		}

		if (ArchetypeComposition.Tags.HasAll(Requirements.RequiredAllTags) == false)
		{
			// missing one of the strictly required tags
			OutDescription += TEXT("\nMissing required tags: ");
			(Requirements.RequiredAllTags - ArchetypeComposition.Tags).DebugGetStringDesc(OutDescription);
		}

		if (Requirements.RequiredAnyTags.IsEmpty() == false && ArchetypeComposition.Tags.HasAny(Requirements.RequiredAnyTags) == false)
		{
			// missing all of the "any" tags
			OutDescription += TEXT("\nMissing all \'any\' tags: ");
			Requirements.RequiredAnyTags.DebugGetStringDesc(OutDescription);
		}

		if (ArchetypeComposition.ChunkFragments.HasAll(Requirements.RequiredAllChunkFragments) == false)
		{
			// missing one of the strictly required chunk fragments
			OutDescription += TEXT("\nMissing required chunk fragments: ");
			(Requirements.RequiredAllChunkFragments - ArchetypeComposition.ChunkFragments).DebugGetStringDesc(OutDescription);
		}

		if (ArchetypeComposition.SharedFragments.HasAll(Requirements.RequiredAllSharedFragments) == false)
		{
			// missing one of the strictly required Shared fragments
			OutDescription += TEXT("\nMissing required Shared fragments: ");
			(Requirements.RequiredAllSharedFragments - ArchetypeComposition.SharedFragments).DebugGetStringDesc(OutDescription);
		}

		if (ArchetypeComposition.ConstSharedFragments.HasAll(Requirements.RequiredAllConstSharedFragments) == false)
		{
			// missing one of the strictly required Shared fragments
			OutDescription += TEXT("\nMissing required Shared fragments: ");
			(Requirements.RequiredAllConstSharedFragments - ArchetypeComposition.ConstSharedFragments).DebugGetStringDesc(OutDescription);
		}
	}
	// else we check if there are any optionals and if so test them
	else if (Requirements.HasOptionalRequirements() && (Requirements.DoesMatchAnyOptionals(ArchetypeComposition) == false))
	{
		// we report that none of the optionals has been met
		OutDescription += TEXT("\nNone of the optionals were safisfied while not having other positive hard requirements: ");

		Requirements.RequiredOptionalTags.DebugGetStringDesc(OutDescription);
		Requirements.RequiredOptionalFragments.DebugGetStringDesc(OutDescription);
		Requirements.RequiredOptionalChunkFragments.DebugGetStringDesc(OutDescription);
		Requirements.RequiredOptionalSharedFragments.DebugGetStringDesc(OutDescription);
		Requirements.RequiredOptionalConstSharedFragments.DebugGetStringDesc(OutDescription);
	}

	return OutDescription.Len() > 0 ? static_cast<FString>(OutDescription) : TEXT("Match");
}

FString FMassDebugger::GetSingleRequirementDescription(const FMassFragmentRequirementDescription& Requirement)
{
	return FString::Printf(TEXT("%s%s[%s]"), Requirement.IsOptional() ? TEXT("?") : (Requirement.Presence == EMassFragmentPresence::None ? TEXT("-") : TEXT("+"))
		, *GetNameSafe(Requirement.StructType), *UE::Mass::Debug::DebugGetFragmentAccessString(Requirement.AccessMode));
}

void FMassDebugger::OutputArchetypeDescription(FOutputDevice& Ar, const FMassArchetypeHandle& ArchetypeHandle)
{
	Ar.Logf(TEXT("%s"), ArchetypeHandle.IsValid() ? *FMassArchetypeHelper::ArchetypeDataFromHandleChecked(ArchetypeHandle).DebugGetDescription() : TEXT("INVALID"));
}

void FMassDebugger::OutputEntityDescription(FOutputDevice& Ar, const FMassEntityManager& EntityManager, const int32 EntityIndex, const TCHAR* InPrefix)
{
	if (EntityIndex >= EntityManager.DebugGetEntityStorageInterface().Num())
	{
		Ar.Logf(ELogVerbosity::Log, TEXT("Unable to list fragments values for out of range index in EntityManager owned by %s"), *GetPathNameSafe(EntityManager.GetOwner()));
		return;
	}
	
	if (!EntityManager.DebugGetEntityStorageInterface().IsValid(EntityIndex))
	{
		Ar.Logf(ELogVerbosity::Log, TEXT("Unable to list fragments values for invalid entity in EntityManager owned by %s"), *GetPathNameSafe(EntityManager.GetOwner()));
	}
	
	FMassEntityHandle Entity;
	Entity.Index = EntityIndex;
	Entity.SerialNumber = EntityManager.DebugGetEntityStorageInterface().GetSerialNumber(EntityIndex);
	OutputEntityDescription(Ar, EntityManager, Entity, InPrefix);
}

void FMassDebugger::OutputEntityDescription(FOutputDevice& Ar, const FMassEntityManager& EntityManager, const FMassEntityHandle Entity, const TCHAR* InPrefix)
{
	if (!EntityManager.IsEntityActive(Entity))
	{
		Ar.Logf(ELogVerbosity::Log, TEXT("Unable to list fragments values for invalid entity in EntityManager owned by %s"), *GetPathNameSafe(EntityManager.GetOwner()));
	}

	Ar.Logf(ELogVerbosity::Log, TEXT("Listing fragments values for Entity[%s] in EntityManager owned by %s"), *Entity.DebugGetDescription(), *GetPathNameSafe(EntityManager.GetOwner()));

	FMassArchetypeData* Archetype = EntityManager.DebugGetEntityStorageInterface().GetArchetypeAsShared(Entity.Index).Get();
	if (Archetype == nullptr)
	{
		Ar.Logf(ELogVerbosity::Log, TEXT("Unable to list fragments values for invalid entity in EntityManager owned by %s"), *GetPathNameSafe(EntityManager.GetOwner()));
	}
	else
	{
		Archetype->DebugPrintEntity(Entity, Ar, InPrefix);
	}
}

void FMassDebugger::SelectEntity(const FMassEntityManager& EntityManager, const FMassEntityHandle EntityHandle)
{
	if (EntityManager.IsEntityValid(EntityHandle))
	{
		UE::Mass::Debug::SetDebugEntityRange(EntityHandle.Index, EntityHandle.Index);

		GetActiveEnvironment(EntityManager).SelectedEntity = EntityHandle;

		OnEntitySelectedDelegate.Broadcast(EntityManager, EntityHandle);
	}
}

FMassEntityHandle FMassDebugger::GetSelectedEntity(const FMassEntityManager& EntityManager)
{
	return GetActiveEnvironment(EntityManager).SelectedEntity;
}

void FMassDebugger::HighlightEntity(const FMassEntityManager& EntityManager, const FMassEntityHandle EntityHandle)
{
	GetActiveEnvironment(EntityManager).HighlightedEntity = EntityHandle;
}

FMassEntityHandle FMassDebugger::GetHighlightedEntity(const FMassEntityManager& EntityManager)
{
	return GetActiveEnvironment(EntityManager).HighlightedEntity;
}

int32 FMassDebugger::RegisterEntityManager(FMassEntityManager& EntityManager)
{
	int32 NewEnvironmentIndex = INDEX_NONE;
	{
		UE::TScopeLock<UE::FSpinLock> ScopeLock(EntityManagerRegistrationLock);
		NewEnvironmentIndex = ActiveEnvironments.Emplace(EntityManager);
	}
	OnEntityManagerInitialized.Broadcast(EntityManager);
	return NewEnvironmentIndex;
}

void FMassDebugger::UnregisterEntityManager(FMassEntityManager& EntityManager)
{
	if (EntityManager.DoesSharedInstanceExist())
	{
		UE::TScopeLock<UE::FSpinLock> ScopeLock(EntityManagerRegistrationLock);
		const int32 Index = ActiveEnvironments.IndexOfByPredicate([WeakManager = EntityManager.AsWeak()](const FEnvironment& Element) 
		{
			return Element.EntityManager == WeakManager;
		});
		if (Index != INDEX_NONE)
		{
			ActiveEnvironments.RemoveAt(Index, EAllowShrinking::No);
		}
	}
	else
	{
		UE::TScopeLock<UE::FSpinLock> ScopeLock(EntityManagerRegistrationLock);
		ActiveEnvironments.RemoveAll([](const FEnvironment& Item)
			{
				return Item.IsValid() == false;
			});
	}
	OnEntityManagerDeinitialized.Broadcast(EntityManager);
}

void FMassDebugger::RegisterProcessorDataProvider(FName ProviderName, const TSharedRef<FMassEntityManager>& EntityManager, const UE::Mass::Debug::FProcessorProviderFunction& ProviderFunction)
{
	UE::TScopeLock<UE::FSpinLock> ScopeLock(EntityManagerRegistrationLock);
	int32 Index = ActiveEnvironments.IndexOfByPredicate([WeakEntityManager = EntityManager->AsWeak()](const FEnvironment& Element) 
	{
		return Element.EntityManager == WeakEntityManager;
	});

	if (Index == INDEX_NONE)
	{
		Index = RegisterEntityManager(*EntityManager);
	}
	
	ActiveEnvironments[Index].ProcessorProviders.FindOrAdd(ProviderName, ProviderFunction);

	OnProcessorProviderRegistered.Broadcast(ActiveEnvironments[Index]);
}

FMassDebugger::FEnvironment* FMassDebugger::FindEnvironmentForEntityManager(const FMassEntityManager& EntityManager)
{
	for (FMassDebugger::FEnvironment& Environment : ActiveEnvironments)
	{
		if (Environment.EntityManager.HasSameObject(&EntityManager))
		{
			return &Environment;
		}
	}
	return nullptr;
}

bool FMassDebugger::DoesArchetypeMatchRequirements(const FMassArchetypeHandle& ArchetypeHandle, const FMassFragmentRequirements& Requirements, FOutputDevice& OutputDevice)
{
	if (const FMassArchetypeData* Archetype = FMassArchetypeHelper::ArchetypeDataFromHandle(ArchetypeHandle))
	{
		return FMassArchetypeHelper::DoesArchetypeMatchRequirements(*Archetype, Requirements, /*bBailOutOnFirstFail=*/false, &OutputDevice);
	}
	return false;
}

bool FMassDebugger::ShouldProcessorBreak(const FMassEntityManager& EntityManager, const UMassProcessor* Processor, FMassEntityHandle Entity)
{
	if (LIKELY(!bHasBreakpoint))
	{
		return false;
	}

	FEnvironment& ActiveEnvironment = GetActiveEnvironment(EntityManager);

	if (LIKELY(!ActiveEnvironment.bHasBreakpoint))
	{
		return false;
	}

	const FMassEntityHandle* Found = ActiveEnvironment.ProcessorBreakpoints.FindPair(Processor, Entity);
	
	return Found != nullptr;
}

bool FMassDebugger::HasAnyProcessorBreakpoints(const FMassEntityManager& EntityManager, const UMassProcessor* Processor)
{
	if (LIKELY(!bHasBreakpoint))
	{
		return false;
	}

	FEnvironment& ActiveEnvironment = GetActiveEnvironment(EntityManager);

	if (LIKELY(!ActiveEnvironment.bHasBreakpoint))
	{
		return false;
	}

	return ActiveEnvironment.ProcessorBreakpoints.Contains(Processor);
}

bool FMassDebugger::ShouldBreakOnFragmentWrite(const FMassEntityManager& EntityManager, const UScriptStruct* FragmentType, FMassEntityHandle Entity)
{
	if (LIKELY(!bHasBreakpoint))
	{
		return false;
	}

	FEnvironment& ActiveEnvironment = GetActiveEnvironment(EntityManager);

	if (LIKELY(!ActiveEnvironment.bHasBreakpoint))
	{
		return false;
	}

	const FMassEntityHandle* Found = ActiveEnvironment.FragmentWriteBreakpoints.FindPair(FragmentType, Entity);

	return Found != nullptr;
}

bool FMassDebugger::HasAnyFragmentWriteBreakpoints(const FMassEntityManager& EntityManager, const UScriptStruct* FragmentType)
{
	if (LIKELY(!bHasBreakpoint))
	{
		return false;
	}

	FEnvironment& ActiveEnvironment = GetActiveEnvironment(EntityManager);

	if (LIKELY(!ActiveEnvironment.bHasBreakpoint))
	{
		return false;
	}

	if (FragmentType == nullptr)
	{
		return ActiveEnvironment.FragmentWriteBreakpoints.Num() > 0;
	}

	return ActiveEnvironment.FragmentWriteBreakpoints.Contains(FragmentType);
}

void FMassDebugger::SetProcessorBreakpoint(const FMassEntityManager& EntityManager, TNotNull<const UMassProcessor*> Processor, FMassEntityHandle Entity)
{
	if (!FPlatformMisc::IsDebuggerPresent())
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("NoDebuggerAttached", "Breakpoint set but no debugger is attached."));
	}

	FEnvironment& ActiveEnvironment = GetActiveEnvironment(EntityManager);

	ActiveEnvironment.bHasBreakpoint = true;
	bHasBreakpoint = true;

	ActiveEnvironment.ProcessorBreakpoints.AddUnique(Processor, Entity);
	OnBreakpointsChangedDelegate.Broadcast();
}

void FMassDebugger::SetFragmentWriteBreak(const FMassEntityManager& EntityManager, const UScriptStruct* FragmentType, FMassEntityHandle Entity)
{
	if (!FPlatformMisc::IsDebuggerPresent())
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("NoDebuggerAttached", "Breakpoint set but no debugger is attached."));
	}

	FEnvironment& ActiveEnvironment = GetActiveEnvironment(EntityManager);

	ActiveEnvironment.bHasBreakpoint = true;
	bHasBreakpoint = true;

	ActiveEnvironment.FragmentWriteBreakpoints.AddUnique(FragmentType, Entity);
	OnBreakpointsChangedDelegate.Broadcast();
}

void FMassDebugger::ClearProcessorBreakpoint(const FMassEntityManager& EntityManager, const UMassProcessor* Processor, FMassEntityHandle Entity)
{
	FEnvironment& ActiveEnvironment = GetActiveEnvironment(EntityManager);

	ActiveEnvironment.ProcessorBreakpoints.Remove(Processor, Entity);

	UpdateHasBreakpoint();
	OnBreakpointsChangedDelegate.Broadcast();
}

void FMassDebugger::ClearAllProcessorBreakpoints(const FMassEntityManager& EntityManager, const UMassProcessor* Processor)
{
	FEnvironment& ActiveEnvironment = GetActiveEnvironment(EntityManager);
	ActiveEnvironment.ProcessorBreakpoints.Remove(Processor);
	OnBreakpointsChangedDelegate.Broadcast();
}

void FMassDebugger::ClearFragmentWriteBreak(const FMassEntityManager& EntityManager, const UScriptStruct* FragmentType, FMassEntityHandle Entity)
{
	FEnvironment& ActiveEnvironment = GetActiveEnvironment(EntityManager);
	ActiveEnvironment.FragmentWriteBreakpoints.Remove(FragmentType, Entity);
	UpdateHasBreakpoint();
	OnBreakpointsChangedDelegate.Broadcast();
}

void FMassDebugger::ClearAllFragmentWriteBreak(const FMassEntityManager& EntityManager, const UScriptStruct* FragmentType)
{
	FEnvironment& ActiveEnvironment = GetActiveEnvironment(EntityManager);
	ActiveEnvironment.FragmentWriteBreakpoints.Remove(FragmentType);
	OnBreakpointsChangedDelegate.Broadcast();
}

void FMassDebugger::ClearAllEntityBreakpoints(const FMassEntityManager& EntityManager, FMassEntityHandle Entity)
{
	FEnvironment& ActiveEnvironment = GetActiveEnvironment(EntityManager);
	TMultiMap<const UMassProcessor*, FMassEntityHandle>::TIterator ProccessorIterator = ActiveEnvironment.ProcessorBreakpoints.CreateIterator();

	for (; ProccessorIterator; ++ProccessorIterator)
	{
		if (ProccessorIterator.Value() == Entity)
		{
			ProccessorIterator.RemoveCurrent();
		}
	}

	TMultiMap<const UScriptStruct*, FMassEntityHandle>::TIterator FragmentIterator = ActiveEnvironment.FragmentWriteBreakpoints.CreateIterator();
	for (; FragmentIterator; ++FragmentIterator)
	{
		if (FragmentIterator.Value() == Entity)
		{
			FragmentIterator.RemoveCurrent();
		}
	}
	OnBreakpointsChangedDelegate.Broadcast();
}
 
void FMassDebugger::BreakOnFragmentWriteForSelectedEntity(FName FragmentName)
{
	for (FEnvironment& Environment : ActiveEnvironments)
	{
		if (Environment.EntityManager.Pin()->IsEntityValid(Environment.SelectedEntity))
		{
			SetFragmentWriteBreak(*Environment.EntityManager.Pin(), GetFragmentTypeFromName(FragmentName), Environment.SelectedEntity);
		}
	}
	OnBreakpointsChangedDelegate.Broadcast();
}

void FMassDebugger::ClearAllBreakpoints()
{
	for (FEnvironment& Environment : ActiveEnvironments)
	{
		Environment.ClearBreakpoints();
	}
	bHasBreakpoint = false;
	OnBreakpointsChangedDelegate.Broadcast();
}

const UScriptStruct* FMassDebugger::GetFragmentTypeFromName(FName FragmentName)
{
	const UScriptStruct** FoundType = FragmentsByName.Find(FragmentName);
	if (FoundType)
	{
		return *FoundType;
	}

	for (FEnvironment& Environment : ActiveEnvironments)
	{
		TArray<FMassArchetypeHandle> ArchetypeHandles = GetAllArchetypes(*Environment.EntityManager.Pin());
		for (FMassArchetypeHandle& ArchetypeHandle : ArchetypeHandles)
		{
			FMassArchetypeCompositionDescriptor Composition = GetArchetypeComposition(ArchetypeHandle);

			FMassFragmentBitSet::FIndexIterator It = Composition.Fragments.GetIndexIterator();
			while (It)
			{
				FName StructName = Composition.Fragments.DebugGetStructTypeName(*It);
				const UScriptStruct* StructType = Composition.Fragments.GetTypeAtIndex(*It);
				
				FragmentsByName.Add(StructName, StructType);
				++It;
			}

			FMassChunkFragmentBitSet::FIndexIterator ChunkIt = Composition.ChunkFragments.GetIndexIterator();
			while (ChunkIt)
			{
				FName StructName = Composition.ChunkFragments.DebugGetStructTypeName(*ChunkIt);
				const UScriptStruct* StructType = Composition.ChunkFragments.GetTypeAtIndex(*ChunkIt);

				FragmentsByName.Add(StructName, StructType);
				++ChunkIt;
			}

			FMassSharedFragmentBitSet::FIndexIterator SharedFragIt = Composition.SharedFragments.GetIndexIterator();
			while (SharedFragIt)
			{
				FName StructName = Composition.SharedFragments.DebugGetStructTypeName(*SharedFragIt);
				const UScriptStruct* StructType = Composition.SharedFragments.GetTypeAtIndex(*SharedFragIt);

				FragmentsByName.Add(StructName, StructType);
				++SharedFragIt;
			}

			FMassConstSharedFragmentBitSet::FIndexIterator ConstSharedFragIt = Composition.ConstSharedFragments.GetIndexIterator();
			while (ConstSharedFragIt)
			{
				FName StructName = Composition.ConstSharedFragments.DebugGetStructTypeName(*ConstSharedFragIt);
				const UScriptStruct* StructType = Composition.ConstSharedFragments.GetTypeAtIndex(*ConstSharedFragIt);

				FragmentsByName.Add(StructName, StructType);
				++ConstSharedFragIt;
			}
		}
	}

	FoundType = FragmentsByName.Find(FragmentName);
	if (FoundType)
	{
		return *FoundType;
	}

	return nullptr;
}

TSharedPtr<FStructOnScope> FMassDebugger::GetFragmentData(const FMassEntityManager& EntityManager, const UScriptStruct* FragmentType, FMassEntityHandle Entity)
{
	TSharedPtr<FStructOnScope> StructOnScope = MakeShared<FStructOnScope>(FragmentType);
	if (GetFragmentData(EntityManager, FragmentType, Entity, StructOnScope))
	{
		return StructOnScope;
	}
	return nullptr;
}

bool FMassDebugger::GetFragmentData(const FMassEntityManager& EntityManager, const UScriptStruct* FragmentType, FMassEntityHandle Entity, TSharedPtr<FStructOnScope>& OutStructData)
{
	TSharedPtr<FMassArchetypeData> Archetype = EntityManager.DebugGetEntityStorageInterface().GetArchetypeAsShared(Entity.Index);
	if (Archetype.IsValid())
	{
		void* FragmentData = Archetype->GetFragmentDataForEntity(FragmentType, Entity.Index);
		if (FragmentData)
		{
			const UStruct* AsUStruct = FragmentType;
			if (OutStructData->GetStruct() != AsUStruct)
			{
				OutStructData->Initialize(FragmentType);
			}
			
			CastChecked<UScriptStruct>(OutStructData->GetStruct())->CopyScriptStruct(OutStructData->GetStructMemory(), FragmentData);
			return true;
		}
	}
	return false;
}

const FMassArchetypeSharedFragmentValues& FMassDebugger::GetSharedFragmentValues(const FMassEntityManager& EntityManager, FMassEntityHandle Entity)
{
	TSharedPtr<FMassArchetypeData> Archetype = EntityManager.DebugGetEntityStorageInterface().GetArchetypeAsShared(Entity.Index);
	if (Archetype.IsValid())
	{
		return Archetype->GetSharedFragmentValues(Entity);
	}

	static FMassArchetypeSharedFragmentValues Dummy;
	return Dummy;
}

TSharedPtr<FStructOnScope> FMassDebugger::GetSharedFragmentData(const FMassEntityManager& EntityManager, const UScriptStruct* FragmentType, FMassEntityHandle Entity)
{
	TSharedPtr<FStructOnScope> StructOnScope = MakeShared<FStructOnScope>(FragmentType);
	if (GetSharedFragmentData(EntityManager, FragmentType, Entity, StructOnScope))
	{
		return StructOnScope;
	}
	return nullptr;
}

bool FMassDebugger::GetSharedFragmentData(const FMassEntityManager& EntityManager, const UScriptStruct* FragmentType, FMassEntityHandle Entity, TSharedPtr<FStructOnScope>& OutStructData)
{
	TSharedPtr<FMassArchetypeData> Archetype = EntityManager.DebugGetEntityStorageInterface().GetArchetypeAsShared(Entity.Index);
	if (Archetype.IsValid())
	{
		const FSharedStruct* SharedFragment = Archetype->GetSharedFragmentValues(Entity).GetSharedFragments().FindByPredicate(FStructTypeEqualOperator(FragmentType));
		void* FragmentData = (SharedFragment != nullptr) ? SharedFragment->GetMemory() : nullptr;

		if (FragmentData)
		{
			const UStruct* AsUStruct = FragmentType;
			if (OutStructData->GetStruct() != AsUStruct)
			{
				OutStructData->Initialize(FragmentType);
			}

			CastChecked<UScriptStruct>(OutStructData->GetStruct())->CopyScriptStruct(OutStructData->GetStructMemory(), FragmentData);
			return true;
		}
	}
	return false;
}

TSharedPtr<FStructOnScope> FMassDebugger::GetConstSharedFragmentData(const FMassEntityManager& EntityManager, const UScriptStruct* FragmentType, FMassEntityHandle Entity)
{
	TSharedPtr<FStructOnScope> StructOnScope = MakeShared<FStructOnScope>(FragmentType);
	if (GetConstSharedFragmentData(EntityManager, FragmentType, Entity, StructOnScope))
	{
		return StructOnScope;
	}
	return nullptr;
}

bool FMassDebugger::GetConstSharedFragmentData(const FMassEntityManager& EntityManager, const UScriptStruct* FragmentType, FMassEntityHandle Entity, TSharedPtr<FStructOnScope>& OutStructData)
{
	TSharedPtr<FMassArchetypeData> Archetype = EntityManager.DebugGetEntityStorageInterface().GetArchetypeAsShared(Entity.Index);
	if (Archetype.IsValid())
	{
		const FConstSharedStruct* SharedFragment = Archetype->GetSharedFragmentValues(Entity).GetConstSharedFragments().FindByPredicate(FStructTypeEqualOperator(FragmentType));
		const void* FragmentData = (SharedFragment != nullptr) ? SharedFragment->GetMemory() : nullptr;

		if (FragmentData)
		{
			const UStruct* AsUStruct = FragmentType;
			if (OutStructData->GetStruct() != AsUStruct)
			{
				OutStructData->Initialize(FragmentType);
			}

			CastChecked<UScriptStruct>(OutStructData->GetStruct())->CopyScriptStruct(OutStructData->GetStructMemory(), FragmentData);
			return true;
		}
	}
	return false;
}

void FMassDebugger::UpdateHasBreakpoint()
{
	bHasBreakpoint = false;
	for (FEnvironment& Environment : ActiveEnvironments)
	{
		Environment.bHasBreakpoint = Environment.ProcessorBreakpoints.Num() != 0
			|| Environment.FragmentWriteBreakpoints.Num() != 0;
		bHasBreakpoint |= Environment.bHasBreakpoint;
	}
}

FMassDebugger::FEnvironment& FMassDebugger::GetActiveEnvironment(const FMassEntityManager& EntityManager)
{
	const int32 Index = ActiveEnvironments.IndexOfByPredicate([WeakManager = EntityManager.AsWeak()](const FEnvironment& Element)
	{
		return Element.EntityManager == WeakManager;
	});

	checkf(Index != INDEX_NONE, TEXT("Mass Debug Environment not found for specified EntitManager"));

	return ActiveEnvironments[Index];
}

FMassDebugger::FEnvironment::FEnvironment(const FMassEntityManager& InEntityManager)
	: EntityManager(InEntityManager.AsWeak())
{
#if UE_MASS_TRACE_ENABLED
	TraceStartedDelegateHandle = FTraceAuxiliary::OnTraceStarted.AddLambda([WeakEntityManager = EntityManager](FTraceAuxiliary::EConnectionType TraceType, const FString& TraceDestination)
	{
		if (!WeakEntityManager.IsValid())
		{
			return;
		}

		const FMassEntityManager& Manager = *WeakEntityManager.Pin();

		ForEachArchetype(Manager, [](FMassArchetypeHandle ArchetypeHandle)
		{
			UE_TRACE_MASS_ARCHETYPE_CREATED(ArchetypeHandle)
		});

	});
#endif
}

FMassDebugger::FEnvironment::~FEnvironment()
{
#if UE_MASS_TRACE_ENABLED
	FTraceAuxiliary::OnTraceStarted.Remove(TraceStartedDelegateHandle);
#endif
}

void FMassDebugger::FEnvironment::ClearBreakpoints()
{
	ProcessorBreakpoints.Reset();
	FragmentWriteBreakpoints.Reset();
	bHasBreakpoint = false;
}

#undef LOCTEXT_NAMESPACE
#endif // WITH_MASSENTITY_DEBUG