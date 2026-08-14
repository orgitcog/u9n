// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MeshCardRepresentation.cpp
=============================================================================*/

#include "MeshCardRepresentation.h"
#include "MeshCardBuild.h"
#include "Engine/StaticMeshSourceData.h"
#include "StaticMeshResources.h"
#include "EngineLogs.h"
#include "ProfilingDebugging/CookStats.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Misc/AutomationTest.h"
#include "DistanceFieldAtlas.h"
#include "Misc/QueuedThreadPoolWrapper.h"
#include "ObjectCacheContext.h"
#include "AsyncCompilationHelpers.h"
#include "AssetCompilingManager.h"

#if WITH_EDITOR
#include "DerivedDataCacheInterface.h"
#include "DerivedDataCacheKey.h"
#include "StaticMeshCompiler.h"
#endif

#define LOCTEXT_NAMESPACE "MeshCardRepresentation"

#if WITH_EDITORONLY_DATA
#include "IMeshBuilderModule.h"
#endif

#if ENABLE_COOK_STATS
namespace CardRepresentationCookStats
{
	FCookStats::FDDCResourceUsageStats UsageStats;
	static FCookStatsManager::FAutoRegisterCallback RegisterCookStats([](FCookStatsManager::AddStatFuncRef AddStat)
	{
		UsageStats.LogStats(AddStat, TEXT("CardRepresentation.Usage"), TEXT(""));
	});
}
#endif

static TAutoConsoleVariable<int32> CVarCardRepresentation(
	TEXT("r.MeshCardRepresentation"),
	1,
	TEXT(""),
	ECVF_ReadOnly);

static TAutoConsoleVariable<float> CVarCardRepresentationMinDensity(
	TEXT("r.MeshCardRepresentation.MinDensity"),
	0.2f,
	TEXT("How much of filled area needs to be there to spawn a card, [0;1] range."),
	ECVF_ReadOnly);

static TAutoConsoleVariable<float> CVarCardRepresentationNormalTreshold(
	TEXT("r.MeshCardRepresentation.NormalTreshold"),
	0.25f,
	TEXT("Normal treshold when surface elements should be clustered together."),
	ECVF_ReadOnly);

static TAutoConsoleVariable<int32> CVarCardRepresentationDebug(
	TEXT("r.MeshCardRepresentation.Debug"),
	0,
	TEXT("Enable mesh cards debugging. Skips DDCs and appends extra debug data."),
	ECVF_Cheat);

static TAutoConsoleVariable<int32> CVarCardRepresentationDebugSurfelDirection(
	TEXT("r.MeshCardRepresentation.Debug.SurfelDirection"),
	-1,
	TEXT("Generate cards for only surfels pointing in a specific direction."),
	ECVF_Default);

FAsyncCardRepresentationTask* CreateCardRepresentationTask(
	const FString& InDDCKey,
	UStaticMesh* Mesh,
	UStaticMesh* GenerateSource,
	int32 MaxLumenMeshCards,
	bool bGenerateDistanceFieldAsIfTwoSided,
	FSourceMeshDataForDerivedDataTask&& OptionalSourceMeshData,
	FCardRepresentationData& OutRenderData);

float MeshCardRepresentation::GetMinDensity()
{
	return FMath::Clamp(CVarCardRepresentationMinDensity.GetValueOnAnyThread(), 0.0f, 1.0f);
}

float MeshCardRepresentation::GetNormalTreshold()
{
	return FMath::Clamp(CVarCardRepresentationNormalTreshold.GetValueOnAnyThread(), 0.0f, 1.0f);
}

bool MeshCardRepresentation::IsDebugMode()
{
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
	return CVarCardRepresentationDebug.GetValueOnAnyThread() != 0;
#else
	return false;
#endif
}

int32 MeshCardRepresentation::GetDebugSurfelDirection()
{
	return FMath::Clamp(CVarCardRepresentationDebugSurfelDirection.GetValueOnAnyThread(), -1, 5);
}

FVector3f MeshCardRepresentation::GetAxisAlignedDirection(uint32 AxisAlignedDirectionIndex)
{
	const uint32 AxisIndex = AxisAlignedDirectionIndex / 2;

	FVector3f Direction(0.0f, 0.0f, 0.0f);
	Direction[AxisIndex] = AxisAlignedDirectionIndex & 1 ? 1.0f : -1.0f;
	return Direction;
}

void MeshCardRepresentation::SetCardsFromBounds(FMeshCardsBuildData& CardData, bool bCardCoversHalfBounds)
{
	CardData.CardBuildData.SetNum(6);

	const FVector3f MeshCenter(CardData.Bounds.GetCenter());
	const FVector3f MeshExtent(CardData.Bounds.GetExtent());
	const float CardZOffset = 5.f;

	for (uint32 DirectionIndex = 0; DirectionIndex < 6u; ++DirectionIndex)
	{
		FLumenCardOBBf OBB;

		OBB.AxisZ = GetAxisAlignedDirection(DirectionIndex);
		OBB.AxisZ.FindBestAxisVectors(OBB.AxisX, OBB.AxisY);
		OBB.AxisX = FVector3f::CrossProduct(OBB.AxisZ, OBB.AxisY);
		OBB.AxisX.Normalize();

		// Project 3D mesh extent onto the specific axes of this CardOBB (this just reshuffles them but the combinatorics are messy).
		OBB.Extent.X = FMath::Abs(OBB.AxisX.Dot(MeshExtent));
		OBB.Extent.Y = FMath::Abs(OBB.AxisY.Dot(MeshExtent));
		OBB.Extent.Z = FMath::Abs(OBB.AxisZ.Dot(MeshExtent));

		if (bCardCoversHalfBounds)
		{
			// Shrink and shift the card box to cover half the mesh bounds along Z and then bump it forward a bit
			OBB.Extent.Z = OBB.Extent.Z * 0.5f + CardZOffset;
			OBB.Origin = MeshCenter + OBB.Extent.Z * OBB.AxisZ;
		}
		else
		{
			OBB.Extent.Z += CardZOffset;
			OBB.Origin = MeshCenter + CardZOffset * OBB.AxisZ;
		}

		CardData.CardBuildData[DirectionIndex].OBB = OBB;
		CardData.CardBuildData[DirectionIndex].AxisAlignedDirectionIndex = DirectionIndex;
	}
}

FCardRepresentationAsyncQueue* GCardRepresentationAsyncQueue = NULL;

#if WITH_EDITORONLY_DATA

// DDC key for card representation data, must be changed when modifying the generation code or data format
#define CARDREPRESENTATION_DERIVEDDATA_VER TEXT("C832711A-D404-4898-80FE-21B263DEF19E")

void AppendMeshCardConstantsToClassSchema(FAppendToClassSchemaContext& Context)
{
	const float MinDensity = MeshCardRepresentation::GetMinDensity();
	const float NormalTreshold = MeshCardRepresentation::GetNormalTreshold();
	uint8 DebugMode = MeshCardRepresentation::IsDebugMode() ? 1 : 0;
	FStringView VersionString(CARDREPRESENTATION_DERIVEDDATA_VER);

	Context.Update(&MinDensity, sizeof(MinDensity));
	Context.Update(&NormalTreshold, sizeof(NormalTreshold));
	Context.Update(&DebugMode, sizeof(DebugMode));
	Context.Update(VersionString.GetData(), VersionString.Len() * sizeof(VersionString[0]));
}

FString BuildCardRepresentationDerivedDataKey(const FString& InMeshKey, int32 MaxLumenMeshCards)
{
	const float MinDensity = MeshCardRepresentation::GetMinDensity();
	const float NormalTreshold = MeshCardRepresentation::GetNormalTreshold();
	const bool bDebugMode = MeshCardRepresentation::IsDebugMode();

	static UE::DerivedData::FCacheBucket LegacyBucket(TEXTVIEW("LegacyCARD"), TEXTVIEW("CardRepresentation"));
	return FDerivedDataCacheInterface::BuildCacheKey(
		TEXT("CARD"),
		*FString::Printf(TEXT("%s_%s%s%.3f_%.3f_%d"), *InMeshKey, CARDREPRESENTATION_DERIVEDDATA_VER, bDebugMode ? TEXT("_DEBUG_") : TEXT(""),
			MinDensity, NormalTreshold, MaxLumenMeshCards),
		TEXT(""));
}

extern void BuildSignedDistanceFieldBuildSectionData(UStaticMesh* Mesh, uint32 LODIndex, TArray<FSignedDistanceFieldBuildSectionData>& OutData);

FAsyncCardRepresentationTask* BeginCacheMeshCardRepresentationInternal(
	UStaticMesh* StaticMeshAsset,
	FStaticMeshRenderData& RenderData,
	const FString& DistanceFieldKey,
	FSourceMeshDataForDerivedDataTask&& OptionalSourceMeshData)
{
	static const auto CVarCards = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MeshCardRepresentation"));

	FAsyncCardRepresentationTask* NewTask = nullptr;

	if (CVarCards->GetValueOnAnyThread() != 0)
	{
		if (RenderData.LODResources.IsValidIndex(0))
		{
			if (!RenderData.LODResources[0].CardRepresentationData)
			{
				RenderData.LODResources[0].CardRepresentationData = new FCardRepresentationData();
			}

			// Only generate distance fields and card representations for the base render data, not platform render data.
			if (&RenderData == StaticMeshAsset->GetRenderData())
			{
				const FMeshBuildSettings& BuildSettings = StaticMeshAsset->GetSourceModel(0).BuildSettings;
				FString Key = BuildCardRepresentationDerivedDataKey(DistanceFieldKey, BuildSettings.MaxLumenMeshCards);

				UStaticMesh* MeshToGenerateFrom = StaticMeshAsset;

				NewTask = CreateCardRepresentationTask(
					Key,
					StaticMeshAsset,
					MeshToGenerateFrom,
					BuildSettings.MaxLumenMeshCards,
					BuildSettings.bGenerateDistanceFieldAsIfTwoSided,
					MoveTemp(OptionalSourceMeshData),
					*RenderData.LODResources[0].CardRepresentationData);
			}
			else
			{
				// Copy the base render data to platform render data.  If it hasn't been generated or loaded from DDC yet, it will be empty,
				// and the async queue will later copy the actual data when complete.
				FCardRepresentationData* BaseCardRepresentationData = StaticMeshAsset->GetRenderData()->LODResources[0].CardRepresentationData;
				if (BaseCardRepresentationData)
				{
					*RenderData.LODResources[0].CardRepresentationData = *BaseCardRepresentationData;
				}
			}
		}
	}

	return NewTask;
}

FAsyncCardRepresentationTask* CreateCardRepresentationTask(
	const FString& InDDCKey,
	UStaticMesh* Mesh,
	UStaticMesh* GenerateSource,
	int32 MaxLumenMeshCards,
	bool bGenerateDistanceFieldAsIfTwoSided,
	FSourceMeshDataForDerivedDataTask&& OptionalSourceMeshData,
	FCardRepresentationData& OutRenderData)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FCardRepresentationData::CacheDerivedData);

	TArray<uint8> DerivedData;

	COOK_STAT(auto Timer = CardRepresentationCookStats::UsageStats.TimeSyncWork());

	if (!MeshCardRepresentation::IsDebugMode() && GetDerivedDataCacheRef().GetSynchronous(*InDDCKey, DerivedData, Mesh->GetPathName()))
	{
		COOK_STAT(Timer.AddHit(DerivedData.Num()));
		FMemoryReader Ar(DerivedData, /*bIsPersistent=*/ true);
		Ar << OutRenderData;

		if (Ar.IsError())
		{
			UE_LOG(LogStaticMesh, Error, TEXT("Error while deserializing Mesh Card derived data for %s from DDC (key %s)"), *Mesh->GetPathName(), *InDDCKey);
		}
	}
	else
	{
		check(Mesh && GenerateSource);

		// We don't actually build the resource until later, so only track the cycles used here.
		COOK_STAT(Timer.TrackCyclesOnly());
		FAsyncCardRepresentationTask* NewTask = new FAsyncCardRepresentationTask;
		NewTask->DDCKey = InDDCKey;
		NewTask->StaticMesh = Mesh;
		NewTask->GenerateSource = GenerateSource;
		NewTask->GeneratedCardRepresentation = new FCardRepresentationData();
		NewTask->MaxLumenMeshCards = MaxLumenMeshCards;
		NewTask->bGenerateDistanceFieldAsIfTwoSided = bGenerateDistanceFieldAsIfTwoSided;

		const uint32 LODIndex = 0;
		BuildSignedDistanceFieldBuildSectionData(Mesh, LODIndex, NewTask->SectionData);

		// Nanite overrides source static mesh with a coarse representation. Need to load original data before we build the mesh SDF.
		if (OptionalSourceMeshData.IsValid())
		{
			NewTask->SourceMeshData = MoveTemp(OptionalSourceMeshData);
		}
		else if (Mesh->IsNaniteEnabled())
		{
			// Always use GetRunningTargetPlatform() here (the cooker's platform, typically Windows Editor), as the data is intended to be platform independent.
			// Besides that, TargetPlatform is only used to fetch an IMeshBuilderModule interface, and in practice the BuildMeshVertexPositions function on
			// that interface doesn't vary per platform (all platforms internally use FStaticMeshBuilder::BuildMeshVertexPositions).
			IMeshBuilderModule& MeshBuilderModule = IMeshBuilderModule::GetForPlatform(GetTargetPlatformManagerRef().GetRunningTargetPlatform());
			if (!MeshBuilderModule.BuildMeshVertexPositions(Mesh, NewTask->SourceMeshData.TriangleIndices, NewTask->SourceMeshData.VertexPositions, NewTask->SourceMeshData.Sections))
			{
				if (UE::Tasks::FCancellationTokenScope::IsCurrentWorkCanceled())
				{
					delete NewTask;
					return nullptr;
				}

				UE_LOG(LogStaticMesh, Error, TEXT("Failed to build static mesh. See previous line(s) for details."));
			}
		}

		return NewTask;
	}

	return nullptr;
}

void BeginCacheMeshCardRepresentation(
	UStaticMesh* StaticMeshAsset,
	FStaticMeshRenderData& RenderData,
	const FString& DistanceFieldKey,
	FSourceMeshDataForDerivedDataTask&& OptionalSourceMeshData)
{
	FAsyncCardRepresentationTask* NewTask = BeginCacheMeshCardRepresentationInternal(StaticMeshAsset, RenderData, DistanceFieldKey, MoveTemp(OptionalSourceMeshData));

	if (NewTask)
	{
		GCardRepresentationAsyncQueue->AddTask(NewTask);
	}
}

void FCardRepresentationData::CacheDerivedData(
	const FString& InDDCKey,
	UStaticMesh* Mesh,
	UStaticMesh* GenerateSource,
	int32 MaxLumenMeshCards,
	bool bGenerateDistanceFieldAsIfTwoSided,
	FSourceMeshDataForDerivedDataTask&& OptionalSourceMeshData)
{
	FAsyncCardRepresentationTask* NewTask = CreateCardRepresentationTask(InDDCKey, Mesh, GenerateSource, MaxLumenMeshCards, bGenerateDistanceFieldAsIfTwoSided, MoveTemp(OptionalSourceMeshData), *this);

	if (NewTask)
	{
		GCardRepresentationAsyncQueue->AddTask(NewTask);
	}
}

#endif

int32 GUseAsyncCardRepresentationBuildQueue = 1;
static FAutoConsoleVariableRef CVarCardRepresentationAsyncBuildQueue(
	TEXT("r.MeshCardRepresentation.Async"),
	GUseAsyncCardRepresentationBuildQueue,
	TEXT("."),
	ECVF_Default | ECVF_ReadOnly
	);

FCardRepresentationAsyncQueue::FCardRepresentationAsyncQueue()
	: Notification(MakeUnique<FAsyncCompilationNotification>(GetAssetNameFormat()))
{
#if WITH_EDITOR
	MeshUtilities = NULL;

	const int32 MaxConcurrency = -1;
	// In Editor, we allow faster compilation by letting the asset compiler's scheduler organize work.
	FQueuedThreadPool* InnerThreadPool = FAssetCompilingManager::Get().GetThreadPool();
#else
	const int32 MaxConcurrency = 1;
	FQueuedThreadPool* InnerThreadPool = GThreadPool;
#endif

	if (InnerThreadPool != nullptr)
	{
		ThreadPool = MakeUnique<FQueuedThreadPoolWrapper>(InnerThreadPool, MaxConcurrency, [](EQueuedWorkPriority) { return EQueuedWorkPriority::Lowest; });
	}

	FAssetCompilingManager::Get().RegisterManager(this);

	PostReachabilityAnalysisHandle = FCoreUObjectDelegates::PostReachabilityAnalysis.AddRaw(this, &FCardRepresentationAsyncQueue::OnPostReachabilityAnalysis);
}

FCardRepresentationAsyncQueue::~FCardRepresentationAsyncQueue()
{
	FAssetCompilingManager::Get().UnregisterManager(this);
	FCoreUObjectDelegates::PostReachabilityAnalysis.Remove(PostReachabilityAnalysisHandle);
}

void FCardRepresentationAsyncQueue::OnPostReachabilityAnalysis()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FCardRepresentationAsyncQueue::CancelUnreachableMeshes);

	CancelAndDeleteTaskByPredicate([this](FAsyncCardRepresentationTask* Task) { return IsTaskInvalid(Task); });
}
void FAsyncCardRepresentationTaskWorker::DoWork()
{
	// Put on background thread to avoid interfering with game-thread bound tasks
	FQueuedThreadPoolTaskGraphWrapper TaskGraphWrapper(ENamedThreads::AnyBackgroundThreadNormalTask);
	GCardRepresentationAsyncQueue->Build(&Task, TaskGraphWrapper);
}

FName FCardRepresentationAsyncQueue::GetStaticAssetTypeName()
{
	return TEXT("UE-MeshCard");
}

FName FCardRepresentationAsyncQueue::GetAssetTypeName() const 
{
	return GetStaticAssetTypeName();
}

FTextFormat FCardRepresentationAsyncQueue::GetAssetNameFormat() const
{
	return LOCTEXT("MeshCardNameFormat", "{0}|plural(one=Mesh Card,other=Mesh Cards)");
}

TArrayView<FName> FCardRepresentationAsyncQueue::GetDependentTypeNames() const 
{
	static FName DependentTypeNames[] = { FDistanceFieldAsyncQueue::GetStaticAssetTypeName() };
	return TArrayView<FName>(DependentTypeNames);
}

int32 FCardRepresentationAsyncQueue::GetNumRemainingAssets() const
{
	return GetNumOutstandingTasks();
}

void FCardRepresentationAsyncQueue::FinishAllCompilation()
{
	BlockUntilAllBuildsComplete();
}

bool FCardRepresentationAsyncQueue::IsTaskInvalid(FAsyncCardRepresentationTask* Task) const
{
	return (Task->StaticMesh && Task->StaticMesh->IsUnreachable()) || (Task->GenerateSource && Task->GenerateSource->IsUnreachable());
}

void FCardRepresentationAsyncQueue::CancelAndDeleteTaskByPredicate(TFunctionRef<bool (FAsyncCardRepresentationTask*)> InShouldCancelPredicate)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FCardRepresentationAsyncQueue::CancelAndDeleteTaskByPredicate);

	FScopeLock Lock(&CriticalSection);

	if (ReferencedTasks.Num() || PendingTasks.Num() || CompletedTasks.Num())
	{
		TSet<FAsyncCardRepresentationTask*> Removed;

		auto RemoveByPredicate =
			[&InShouldCancelPredicate, &Removed](TSet<FAsyncCardRepresentationTask*>& Tasks)
		{
			for (auto It = Tasks.CreateIterator(); It; ++It)
			{
				FAsyncCardRepresentationTask* Task = *It;
				if (InShouldCancelPredicate(Task))
				{
					Removed.Add(Task);
					It.RemoveCurrent();
				}
			}
		};

		RemoveByPredicate(PendingTasks);
		RemoveByPredicate(ReferencedTasks);
		RemoveByPredicate(CompletedTasks);

		Lock.Unlock();

		CancelAndDeleteTask(Removed);
	}
}

void FCardRepresentationAsyncQueue::CancelAndDeleteTask(const TSet<FAsyncCardRepresentationTask*>& Tasks)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FCardRepresentationAsyncQueue::CancelAndDeleteTask);

	// Do all the cancellation first to make sure none of these tasks
	// get scheduled as we're waiting for completion.
	for (FAsyncCardRepresentationTask* Task : Tasks)
	{
		if (Task->AsyncTask)
		{
			Task->AsyncTask->Cancel();
		}
	}

	for (FAsyncCardRepresentationTask* Task : Tasks)
	{
		if (Task->AsyncTask)
		{
			Task->AsyncTask->EnsureCompletion();
			Task->AsyncTask.Reset();
		}
	}

	for (FAsyncCardRepresentationTask* Task : Tasks)
	{
		if (Task->GeneratedCardRepresentation != nullptr)
		{
			// Rendering thread may still be referencing the old one, use the deferred cleanup interface to delete it next frame when it is safe
			BeginCleanup(Task->GeneratedCardRepresentation);
		}

#if DO_GUARD_SLOW
		{
			FScopeLock Lock(&CriticalSection);
			check(!PendingTasks.Contains(Task));
			check(!ReferencedTasks.Contains(Task));
			check(!CompletedTasks.Contains(Task));
		}
#endif
		delete Task;
	}
}

void FCardRepresentationAsyncQueue::StartBackgroundTask(FAsyncCardRepresentationTask* Task)
{
	check(Task->AsyncTask == nullptr);
	Task->AsyncTask = MakeUnique<FAsyncTask<FAsyncCardRepresentationTaskWorker>>(*Task);
	int64 RequiredMemory = -1; // @todo RequiredMemory
	Task->AsyncTask->StartBackgroundTask(ThreadPool.Get(), EQueuedWorkPriority::Lowest, EQueuedWorkFlags::DoNotRunInsideBusyWait, RequiredMemory, TEXT("Card"));
}

void FCardRepresentationAsyncQueue::RunImmediateTask(FAsyncCardRepresentationTask* Task)
{
#if WITH_EDITOR
	if (!MeshUtilities)
	{
		FPlatformAtomics::InterlockedCompareExchangePointer((void**)&MeshUtilities, &FModuleManager::Get().LoadModuleChecked<IMeshUtilities>(TEXT("MeshUtilities")), nullptr);
	}

	// Thread pool argument isn't actually used by the function
	Build(Task, *ThreadPool.Get());

	// Array protection when called from multiple threads
	FScopeLock Lock(&CriticalSection);
	ReferencedTasks.Add(Task);
	CompletedTasks.Add(Task);
#endif
}

void FCardRepresentationAsyncQueue::ProcessPendingTasks()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FCardRepresentationAsyncQueue::ProcessPendingTasks);

	FScopeLock Lock(&CriticalSection);

	for (auto It = PendingTasks.CreateIterator(); It; ++It)
	{
		FAsyncCardRepresentationTask* Task = *It;
		if (Task->GenerateSource == nullptr || 
			Task->GenerateSource->IsCompiling() == false)
		{
			StartBackgroundTask(Task);
			It.RemoveCurrent();
		}
	}
}

void FCardRepresentationAsyncQueue::AddTask(FAsyncCardRepresentationTask* Task)
{
#if WITH_EDITOR
	// This could happen during the cancellation of async static mesh build
	// Simply delete the task if the static mesh are being garbage collected
	if (IsTaskInvalid(Task))
	{
		CancelAndDeleteTask({ Task });
		return;
	}

	if (!MeshUtilities)
	{
		FPlatformAtomics::InterlockedCompareExchangePointer((void**)&MeshUtilities, &FModuleManager::Get().LoadModuleChecked<IMeshUtilities>(TEXT("MeshUtilities")), nullptr);
	}

	const bool bUseAsyncBuild = GUseAsyncCardRepresentationBuildQueue || !IsInGameThread();
	const bool bIsCompiling = Task->GenerateSource->IsCompiling();
	{
		// Array protection when called from multiple threads
		FScopeLock Lock(&CriticalSection);
		check(!CompletedTasks.Contains(Task)); // reusing same pointer for a new task that is marked completed but has been canceled...
		ReferencedTasks.Add(Task);

		// The Source Mesh's RenderData is not ready yet, postpone the build
		if (bIsCompiling)
		{
			PendingTasks.Add(Task);
		}
		else if (bUseAsyncBuild)
		{
			// Make sure the Task is launched while we hold the lock to avoid
			// race with cancellation
			StartBackgroundTask(Task);
		}
	}

	if (!bIsCompiling && !bUseAsyncBuild)
	{
		// To avoid deadlocks, we must queue the inner build tasks on another thread pool, so use the task graph.
		// Put on background thread to avoid interfering with game-thread bound tasks
		FQueuedThreadPoolTaskGraphWrapper TaskGraphWrapper(ENamedThreads::AnyBackgroundThreadNormalTask);
		Build(Task, TaskGraphWrapper);
	}
#else
	UE_LOG(LogStaticMesh,Fatal,TEXT("Tried to build a card representation without editor support (this should have been done during cooking)"));
#endif
}

void FCardRepresentationAsyncQueue::CancelBuild(UStaticMesh* InStaticMesh)
{
	CancelBuilds({ InStaticMesh });
}

void FCardRepresentationAsyncQueue::CancelBuilds(const TSet<UStaticMesh*>& InStaticMeshes)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FCardRepresentationAsyncQueue::CancelBuilds);

	CancelAndDeleteTaskByPredicate(
		[&InStaticMeshes](FAsyncCardRepresentationTask* Task) 
		{
			return InStaticMeshes.Contains(Task->GenerateSource) || InStaticMeshes.Contains(Task->StaticMesh);
		}
	);
}

void FCardRepresentationAsyncQueue::CancelAllOutstandingBuilds()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FCardRepresentationAsyncQueue::CancelAllOutstandingBuilds);

	TSet<FAsyncCardRepresentationTask*> OutstandingTasks;
	{
		FScopeLock Lock(&CriticalSection);
		PendingTasks.Reset();
		OutstandingTasks = MoveTemp(ReferencedTasks);
	}

	CancelAndDeleteTask(OutstandingTasks);
}

void FCardRepresentationAsyncQueue::RescheduleBackgroundTask(FAsyncCardRepresentationTask* InTask, EQueuedWorkPriority InPriority)
{
	if (InTask->AsyncTask && InTask->AsyncTask->GetPriority() != InPriority)
	{
		InTask->AsyncTask->Reschedule(GThreadPool, InPriority);
	}
}

void FCardRepresentationAsyncQueue::FinishCompilationForObjects(TArrayView<UObject* const> InObjects)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FCardRepresentationAsyncQueue::FinishCompilationForObjects);

	TSet<UStaticMesh*> StaticMeshes;
	for (UObject* Object : InObjects)
	{
		if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(Object))
		{
			StaticMeshes.Add(StaticMesh);
		}
		else if (UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(Object))
		{
			if (StaticMeshComponent->GetStaticMesh())
			{
				StaticMeshes.Add(StaticMeshComponent->GetStaticMesh());
			}
		}
	}

	for (UStaticMesh* StaticMesh : StaticMeshes)
	{
		BlockUntilBuildComplete(StaticMesh, false);
	}
}

void FCardRepresentationAsyncQueue::MarkCompilationAsCanceled(TArrayView<UObject* const> InObjects)
{
	TSet<UStaticMesh*> StaticMeshes;
	for (UObject* Object : InObjects)
	{
		if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(Object))
		{
			StaticMeshes.Add(StaticMesh);
		}
	}

	if (StaticMeshes.Num())
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FCardRepresentationAsyncQueue::MarkCompilationAsCanceled);

		CancelAndDeleteTaskByPredicate(
		    [&StaticMeshes](FAsyncCardRepresentationTask* Task)
		    {
			    // Mark canceled is only a hint and should be non-blocking so only remove or cancel tasks that are either not started or already finished.
			    if (Task->AsyncTask == nullptr || Task->AsyncTask->IsDone())
			    {
				    return StaticMeshes.Contains(Task->GenerateSource) || StaticMeshes.Contains(Task->StaticMesh);
			    }

				return false;
		    }
		);
	}
}

void FCardRepresentationAsyncQueue::BlockUntilBuildComplete(UStaticMesh* InStaticMesh, bool bWarnIfBlocked)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FCardRepresentationAsyncQueue::BlockUntilBuildComplete);

	// We will track the wait time here, but only the cycles used.
	// This function is called whether or not an async task is pending, 
	// so we have to look elsewhere to properly count how many resources have actually finished building.
	COOK_STAT(auto Timer = CardRepresentationCookStats::UsageStats.TimeAsyncWait());
	COOK_STAT(Timer.TrackCyclesOnly());
	bool bReferenced = false;
	bool bHadToBlock = false;
	double StartTime = 0;

#if WITH_EDITOR
	FStaticMeshCompilingManager::Get().FinishCompilation({ InStaticMesh });
	if (GDistanceFieldAsyncQueue)
	{
		GDistanceFieldAsyncQueue->BlockUntilBuildComplete(InStaticMesh, bWarnIfBlocked);
	}	
#endif

	TSet<UStaticMesh*> RequiredFinishCompilation;
	do 
	{
		ProcessAsyncTasks();

		bReferenced = false;

		{
			FScopeLock Lock(&CriticalSection);
			for (FAsyncCardRepresentationTask* Task : ReferencedTasks)
			{
				if (Task->StaticMesh == InStaticMesh ||
					Task->GenerateSource == InStaticMesh)
				{
					bReferenced = true;

					// If the task we are waiting on depends on other static meshes
					// we need to force finish them too.
#if WITH_EDITOR
					
					if (Task->GenerateSource != nullptr &&
						Task->GenerateSource->IsCompiling())
					{
						RequiredFinishCompilation.Add(Task->GenerateSource);
					}

					if (Task->StaticMesh != nullptr &&
						Task->StaticMesh->IsCompiling())
					{
						RequiredFinishCompilation.Add(Task->StaticMesh);
					}
#endif

					RescheduleBackgroundTask(Task, EQueuedWorkPriority::Blocking);
				}
			}
		}

#if WITH_EDITOR
		// Call the finish compilation outside of the critical section since those compilations
		// might need to register new distance field tasks which also uses the critical section.
		if (RequiredFinishCompilation.Num())
		{
			FStaticMeshCompilingManager::Get().FinishCompilation(RequiredFinishCompilation.Array());
			if (GDistanceFieldAsyncQueue)
			{
				for (UStaticMesh* StaticMesh : RequiredFinishCompilation)
				{
					GDistanceFieldAsyncQueue->BlockUntilBuildComplete(StaticMesh, bWarnIfBlocked);
				}
			}
		}
#endif

		if (bReferenced)
		{
			if (!bHadToBlock)
			{
				StartTime = FPlatformTime::Seconds();
			}

			bHadToBlock = true;
			FPlatformProcess::Sleep(.01f);
		}
	} 
	while (bReferenced);

	if (bHadToBlock &&
		bWarnIfBlocked
#if WITH_EDITOR
		&& !FAutomationTestFramework::Get().GetCurrentTest() // HACK - Don't output this warning during automation test
#endif
		)
	{
		UE_LOG(LogStaticMesh, Display, TEXT("Main thread blocked for %.3fs for async card representation build of %s to complete!  This can happen if the mesh is rebuilt excessively."),
			(float)(FPlatformTime::Seconds() - StartTime), 
			*InStaticMesh->GetName());
	}
}

void FCardRepresentationAsyncQueue::BlockUntilAllBuildsComplete()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FCardRepresentationAsyncQueue::BlockUntilAllBuildsComplete)
	while (true)
	{
#if WITH_EDITOR
		FStaticMeshCompilingManager::Get().FinishAllCompilation();
		if (GDistanceFieldAsyncQueue)
		{
			GDistanceFieldAsyncQueue->BlockUntilAllBuildsComplete();
		}
#endif
		// Reschedule as highest prio since we're explicitly waiting on them
		{
			FScopeLock Lock(&CriticalSection);
			for (FAsyncCardRepresentationTask* Task : ReferencedTasks)
			{
				RescheduleBackgroundTask(Task, EQueuedWorkPriority::Blocking);
			}
		}

		ProcessAsyncTasks();

		if (GetNumOutstandingTasks() <= 0)
		{
			break;
		}

		FPlatformProcess::Sleep(.01f);
	} 
}

void FCardRepresentationAsyncQueue::Build(FAsyncCardRepresentationTask* Task, FQueuedThreadPool& BuildThreadPool)
{
#if WITH_EDITOR
	COOK_STAT(auto Timer = CardRepresentationCookStats::UsageStats.TimeSyncWork());
	COOK_STAT(Timer.TrackCyclesOnly());
	
	// Editor 'force delete' can null any UObject pointers which are seen by reference collecting (eg UProperty or serialized)
	if (Task->StaticMesh && Task->GenerateSource)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FCardRepresentationAsyncQueue::Build);

		const FStaticMeshLODResources& LODModel = Task->GenerateSource->GetRenderData()->LODResources[0];

		FMeshDataForDerivedDataTask MeshData;
		MeshData.SourceMeshData = &Task->SourceMeshData;
		MeshData.LODModel = &LODModel;
		MeshData.SectionData = Task->SectionData;
		MeshData.Bounds = (FBoxSphereBounds3f)Task->GenerateSource->GetRenderData()->Bounds;

		Task->bSuccess = MeshUtilities->GenerateCardRepresentationData(
			Task->StaticMesh->GetName(),
			MeshData,
			Task->GenerateSource->GetRenderData()->LODResources[0].DistanceFieldData,
			Task->MaxLumenMeshCards,
			Task->bGenerateDistanceFieldAsIfTwoSided,
			*Task->GeneratedCardRepresentation);
	}

	{
		FScopeLock Lock(&CriticalSection);
		// Avoid adding to the completed list if the task has been canceled
		if (ReferencedTasks.Contains(Task))
		{
			CompletedTasks.Add(Task);
		}
	}

#endif
}

void FCardRepresentationAsyncQueue::ProcessAsyncTasks(bool bLimitExecutionTime)
{
#if WITH_EDITOR
	TRACE_CPUPROFILER_EVENT_SCOPE(FCardRepresentationAsyncQueue::ProcessAsyncTasks);

	ProcessPendingTasks();

	FObjectCacheContextScope ObjectCacheScope;
	const double MaxProcessingTime = 0.016f;
	double StartTime = FPlatformTime::Seconds();
	bool bMadeProgress = false;
	while (!bLimitExecutionTime || (FPlatformTime::Seconds() - StartTime) < MaxProcessingTime)
	{
		FAsyncCardRepresentationTask* Task = nullptr;
		{
			FScopeLock Lock(&CriticalSection);
			if (CompletedTasks.Num() > 0)
			{
				// Pop any element from the set
				auto Iterator = CompletedTasks.CreateIterator();
				Task = *Iterator;
				Iterator.RemoveCurrent();

				verify(ReferencedTasks.Remove(Task));
			}
		}

		if (Task == nullptr)
		{
			break;
		}
		bMadeProgress = true;

		// We want to count each resource built from a DDC miss, so count each iteration of the loop separately.
		COOK_STAT(auto Timer = CardRepresentationCookStats::UsageStats.TimeSyncWork());

		if (Task->AsyncTask)
		{
			Task->AsyncTask->EnsureCompletion();
			Task->AsyncTask.Reset();
		}

		// Editor 'force delete' can null any UObject pointers which are seen by reference collecting (eg UProperty or serialized)
		if (Task->StaticMesh && Task->bSuccess)
		{
			check(!Task->StaticMesh->IsCompiling());

			FStaticMeshRenderData* RenderData = Task->StaticMesh->GetRenderData();
			FCardRepresentationData* OldCardData = RenderData->LODResources[0].CardRepresentationData;

			// Assign the new data, this is safe because the render threads makes a copy of the pointer at scene proxy creation time.
			RenderData->LODResources[0].CardRepresentationData = Task->GeneratedCardRepresentation;

			// Any already created render state needs to be dirtied
			if (RenderData->IsInitialized())
			{
				for (IStaticMeshComponent* Component : ObjectCacheScope.GetContext().GetStaticMeshComponents(Task->StaticMesh))
				{
					IPrimitiveComponent* PrimitiveComponent = Component->GetPrimitiveComponentInterface();

					if (PrimitiveComponent->IsRegistered() && PrimitiveComponent->IsRenderStateCreated())
					{
						PrimitiveComponent->MarkRenderStateDirty();
					}
				}
			}

			// Rendering thread may still be referencing the old one, use the deferred cleanup interface to delete it next frame when it is safe
			BeginCleanup(OldCardData);

			// Need also to update platform render data if it's being cached
			FStaticMeshRenderData* PlatformRenderData = RenderData->NextCachedRenderData.Get();
			while (PlatformRenderData)
			{
				if (PlatformRenderData->LODResources[0].CardRepresentationData)
				{
					*PlatformRenderData->LODResources[0].CardRepresentationData = *Task->GeneratedCardRepresentation;
				}
				PlatformRenderData = PlatformRenderData->NextCachedRenderData.Get();
			}

			if (!MeshCardRepresentation::IsDebugMode())
			{
				TArray<uint8> DerivedData;
				// Save built data to DDC
				FMemoryWriter Ar(DerivedData, /*bIsPersistent=*/ true);
				Ar << *(Task->StaticMesh->GetRenderData()->LODResources[0].CardRepresentationData);
				GetDerivedDataCacheRef().Put(*Task->DDCKey, DerivedData, Task->StaticMesh->GetPathName());
				COOK_STAT(Timer.AddMiss(DerivedData.Num()));
			}
		}

		delete Task;
	}

	if (bMadeProgress)
	{
		Notification->Update(GetNumRemainingAssets());
	}
#endif
}

void FCardRepresentationAsyncQueue::Shutdown()
{
	CancelAllOutstandingBuilds();

	UE_LOG(LogStaticMesh, Log, TEXT("Abandoning remaining async card representation tasks for shutdown"));
	ThreadPool.Reset();
}

#undef LOCTEXT_NAMESPACE
