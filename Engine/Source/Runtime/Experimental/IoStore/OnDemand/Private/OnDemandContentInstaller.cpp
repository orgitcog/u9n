// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnDemandContentInstaller.h"

#include "Algo/RemoveIf.h"
#include "Async/UniqueLock.h"
#include "Containers/RingBuffer.h"
#include "HAL/Platform.h"
#include "IO/IoBuffer.h"
#include "IO/IoChunkId.h"
#include "IO/IoDispatcher.h"
#include "IO/IoStoreOnDemand.h"
#include "IO/PackageStore.h"
#include "IO/IoStatus.h"
#include "Misc/Timespan.h"
#include "Misc/PackageName.h"
#include "OnDemandHttpThread.h"
#include "OnDemandInstallCache.h"
#include "OnDemandIoStore.h"
#include "OnDemandPackageStoreBackend.h"
#include "Statistics.h"
#include <atomic>

namespace UE::IoStore
{
	namespace CVars
	{
#if !UE_BUILD_SHIPPING
		FString IoStoreErrorOnRequest = "";
		static FAutoConsoleVariableRef CVar_IoStoreErrorOnRequest(
			TEXT("iostore.ErrorOnRequest"),
			IoStoreErrorOnRequest,
			TEXT("When the request with a debug name partially matching this cvar is found iostore will error with a random error.")
			);
#endif
	}
namespace Private
{

////////////////////////////////////////////////////////////////////////////////
void ResolvePackageDependencies(
	const TSet<FPackageId>& PackageIds,
	bool bIncludeSoftReferences,
	TSet<FPackageId>& OutResolved,
	TSet<FPackageId>& OutMissing)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FOnDemandContentInstaller::ResolvePackageDependencies);

	TRingBuffer<FPackageId> Queue;
	TSet<FPackageId>		Visitied;

	Visitied.Reserve(PackageIds.Num());
	Queue.Reserve(PackageIds.Num());
	for (const FPackageId& PackageId : PackageIds)
	{
		Queue.Add(PackageId);
	}

	FPackageStore& PackageStore = FPackageStore::Get();
	FPackageStoreReadScope _(PackageStore);

	while (!Queue.IsEmpty())
	{
		FPackageId PackageId = Queue.PopFrontValue();
		{
			FName		SourcePackageName;
			FPackageId	RedirectedToPackageId;
			if (PackageStore.GetPackageRedirectInfo(PackageId, SourcePackageName, RedirectedToPackageId))
			{
				PackageId = RedirectedToPackageId;
			}
		}

		bool bIsAlreadyInSet = false;
		Visitied.Add(PackageId, &bIsAlreadyInSet);
		if (bIsAlreadyInSet)
		{
			continue;
		}

		FPackageStoreEntry PackageStoreEntry;
		const EPackageStoreEntryStatus EntryStatus = PackageStore.GetPackageStoreEntry(PackageId, NAME_None, PackageStoreEntry);
		if (EntryStatus != EPackageStoreEntryStatus::Missing)
		{
			OutResolved.Add(PackageId);
			for (const FPackageId& ImportedPackageId : PackageStoreEntry.ImportedPackageIds)
			{
				if (!Visitied.Contains(ImportedPackageId))
				{
					Queue.Add(ImportedPackageId);
				}
			}

			if (bIncludeSoftReferences)
			{
				TConstArrayView<FPackageId> SoftReferences;
				TConstArrayView<uint32> Indices = PackageStore.GetSoftReferences(PackageId, SoftReferences);
				for (uint32 Idx : Indices)
				{
					const FPackageId& SoftRef = SoftReferences[Idx];
					if (!Visitied.Contains(SoftRef))
					{
						Queue.Add(SoftRef);
					}
				}
			}
		}
		else
		{
			OutMissing.Add(PackageId);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
void ResolveChunksToInstall(
	const TSet<FSharedOnDemandContainer>& Containers,
	const TSet<FPackageId>& PackageIds,
	bool bIncludeSoftReferences,
	TArray<FResolvedContainerChunks>& OutResolvedContainerChunks,
	TSet<FPackageId>& OutMissing)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FOnDemandContentInstaller::ResolveChunksToInstall);

	// For now we always download these required chunks
	for (const FSharedOnDemandContainer& Container : Containers)
	{
		FResolvedContainerChunks& ResolvedChunks = OutResolvedContainerChunks.AddDefaulted_GetRef();
		ResolvedChunks.Container = Container;

		for (int32 EntryIndex = 0; const FIoChunkId& ChunkId : Container->ChunkIds)
		{
			switch(ChunkId.GetChunkType())
			{
				case EIoChunkType::ExternalFile:
				case EIoChunkType::ShaderCodeLibrary:
				case EIoChunkType::ShaderCode:
				{
					ResolvedChunks.EntryIndices.Emplace(EntryIndex); 
					ResolvedChunks.TotalSize += Container->ChunkEntries[EntryIndex].EncodedSize;
				}
				default:
					break;
			}
			++EntryIndex;
		}
	}

	auto FindChunkEntry = [&OutResolvedContainerChunks](const FIoChunkId& ChunkId, int32& OutIndex) -> FResolvedContainerChunks*
	{
		for (FResolvedContainerChunks& ContainerChunks : OutResolvedContainerChunks)
		{
			if (OutIndex = ContainerChunks.Container->FindChunkEntryIndex(ChunkId); OutIndex != INDEX_NONE)
			{
				return &ContainerChunks; 
			}
		}
		return nullptr; 
	};

	TSet<FPackageId> ResolvedPackageIds;
	ResolvePackageDependencies(
		PackageIds,
		bIncludeSoftReferences,
		ResolvedPackageIds,
		OutMissing);

	// Resolve all chunk entries from the resolved package ID's
	for (const FPackageId& PackageId : ResolvedPackageIds)
	{
		const FIoChunkId PackageChunkId				= CreatePackageDataChunkId(PackageId);
		int32 EntryIndex							= INDEX_NONE; 
		FResolvedContainerChunks* ResolvedChunks	= FindChunkEntry(PackageChunkId, EntryIndex);

		if (ResolvedChunks == nullptr) 
		{
			// The chunk resides in a base game container
			continue;
		}

		check(EntryIndex != INDEX_NONE);
		FOnDemandContainer& Container	= *ResolvedChunks->Container; 

		ResolvedChunks->EntryIndices.Emplace(EntryIndex); 
		ResolvedChunks->TotalSize += Container.ChunkEntries[EntryIndex].EncodedSize;

		// TODO: Installing optional bulkdata should probably be an install argument
		const EIoChunkType AdditionalPackageChunkTypes[] =
		{
			EIoChunkType::BulkData,
			EIoChunkType::OptionalBulkData,
			EIoChunkType::MemoryMappedBulkData 
		};

		for (EIoChunkType ChunkType : AdditionalPackageChunkTypes)
		{
			// TODO: For Mutable we need to traverse all possible bulk data chunk indices?
			const FIoChunkId ChunkId = CreateBulkDataIoChunkId(PackageId.Value(), 0, 0, ChunkType);
			if (ResolvedChunks = FindChunkEntry(ChunkId, EntryIndex); ResolvedChunks != nullptr)
			{
				check(EntryIndex != INDEX_NONE);
				FOnDemandContainer& OtherContainer = *ResolvedChunks->Container;
				ResolvedChunks->EntryIndices.Emplace(EntryIndex);
				ResolvedChunks->TotalSize += OtherContainer.ChunkEntries[EntryIndex].EncodedSize;
			}
		}
	}
}

} // namespace Private

////////////////////////////////////////////////////////////////////////////////
uint32 FOnDemandContentInstaller::FRequest::NextSeqNo = 0;

////////////////////////////////////////////////////////////////////////////////
FOnDemandContentInstaller::FOnDemandContentInstaller(FOnDemandIoStore& InIoStore, FOnDemandHttpThread& InHttpClient)
	: IoStore(InIoStore)
	, HttpClient(InHttpClient)
	, InstallerPipe(TEXT("IoStoreOnDemandInstallerPipe"))
{
}

FOnDemandContentInstaller::~FOnDemandContentInstaller()
{
	Shutdown();
}

FSharedInternalInstallRequest FOnDemandContentInstaller::EnqueueInstallRequest(
	FOnDemandInstallArgs&& Args,
	FOnDemandInstallCompleted&& OnCompleted,
	FOnDemandInstallProgressed&& OnProgress)
{
	FRequest* Request = nullptr;
	{
		TUniqueLock Lock(Mutex);
		Request = RequestAllocator.Construct(MoveTemp(Args), MoveTemp(OnCompleted), MoveTemp(OnProgress));
	}

	FSharedInternalInstallRequest InstallRequest = MakeShared<FOnDemandInternalInstallRequest, ESPMode::ThreadSafe>(UPTRINT(Request));
	Request->AsInstall().Request = InstallRequest;

	FOnDemandContentInstallerStats::OnRequestEnqueued();

	InstallerPipe.Launch(
		TEXT("ProcessIoStoreOnDemandInstallRequest"),
		[this, Request] { ProcessInstallRequest(*Request); },
		UE::Tasks::ETaskPriority::BackgroundLow);

	return InstallRequest;
}

void FOnDemandContentInstaller::EnqueuePurgeRequest(FOnDemandPurgeArgs&& Args, FOnDemandPurgeCompleted&& OnCompleted)
{
	{
		TUniqueLock Lock(Mutex);
		RequestQueue.HeapPush(RequestAllocator.Construct(MoveTemp(Args), MoveTemp(OnCompleted)), RequestSortPredicate);
	}

	TryExecuteNextRequest();
}

void FOnDemandContentInstaller::EnqueueDefragRequest(FOnDemandDefragArgs&& Args, FOnDemandDefragCompleted&& OnCompleted)
{
	{
		TUniqueLock Lock(Mutex);
		RequestQueue.HeapPush(RequestAllocator.Construct(MoveTemp(Args), MoveTemp(OnCompleted)), RequestSortPredicate);
	}

	TryExecuteNextRequest();
}

void FOnDemandContentInstaller::EnqueueVerifyRequest(FOnDemandVerifyCacheCompleted&& OnCompleted)
{
	{
		TUniqueLock Lock(Mutex);
		RequestQueue.HeapPush(RequestAllocator.Construct(MoveTemp(OnCompleted)), RequestSortPredicate);
	}

	TryExecuteNextRequest();
}

void FOnDemandContentInstaller::CancelInstallRequest(FSharedInternalInstallRequest InstallRequest)
{
	InstallerPipe.Launch(
		TEXT("CancelIoStoreOnDemandInstallRequest"),
		[this, InstallRequest]
		{
			FRequest* ToComplete = nullptr;
			{
				UE::TUniqueLock Lock(Mutex);

				if (InstallRequest->InstallerRequest == 0)
				{
					return;
				}

				FRequest* Request = reinterpret_cast<FRequest*>(InstallRequest->InstallerRequest);

				EIoErrorCode Expected = EIoErrorCode::Unknown;
				if (Request->ErrorCode.compare_exchange_strong(Expected, EIoErrorCode::Cancelled) == false)
				{
					return;
				}

				UE_LOG(LogIoStoreOnDemand, Log, TEXT("Cancelling install request, ContentHandle=(%s)"),
					*LexToString(Request->AsInstall().Args.ContentHandle));

				if (RequestQueue.Remove(Request) > 0)
				{
					ToComplete = Request;
					RequestQueue.Heapify(RequestSortPredicate);
				}
			}

			if (ToComplete != nullptr)
			{
				CompleteInstallRequest(*ToComplete);
			}
		});
}

void FOnDemandContentInstaller::UpdateInstallRequestPriority(FSharedInternalInstallRequest InstallRequest, int32 NewPriority)
{
	InstallerPipe.Launch(
		TEXT("UpdateIoStoreOnDemandInstallRequestPriority"),
		[this, InstallRequest, NewPriority]
		{
			UE::TUniqueLock Lock(Mutex);

			if (InstallRequest->InstallerRequest == 0)
			{
				return;
			}

			FRequest&			Request	= *reinterpret_cast<FRequest*>(InstallRequest->InstallerRequest);
			FRequest::FInstall& Install	= Request.AsInstall();

			UE_LOG(LogIoStoreOnDemand, Log, TEXT("Updating install request priority, SeqNo=%u, Priority=%d, NewPriority=%d, ContentHandle=(%s)"),
				Request.SeqNo, Request.Priority, NewPriority, *LexToString(Install.Args.ContentHandle));

			Request.Priority = NewPriority;

			if (Install.bHttpRequestsIssued.load(std::memory_order_seq_cst))
			{
				for (FChunkHttpRequestHandle& PendingHttpRequest : Install.HttpRequestHandles)
				{
					if (PendingHttpRequest.Handle != nullptr)
					{
						HttpClient.ReprioritizeRequest(PendingHttpRequest.Handle, Request.Priority);
					}
				}
			}
			else
			{
				RequestQueue.Heapify(RequestSortPredicate);
			}
		});
}

void FOnDemandContentInstaller::ReportAnalytics(TArray<FAnalyticsEventAttribute>& OutAnalyticsArray) const
{
	FOnDemandContentInstallerStats::ReportAnalytics(OutAnalyticsArray);
}

void FOnDemandContentInstaller::TryExecuteNextRequest()
{
	if (bShuttingDown.load(std::memory_order_relaxed))
	{
		return;
	}

	FRequest* NextRequest = nullptr;
	{
		TUniqueLock Lock(Mutex);
		if (CurrentRequest == nullptr && RequestQueue.IsEmpty() == false)
		{
			RequestQueue.HeapPop(NextRequest, RequestSortPredicate, EAllowShrinking::No);
			CurrentRequest = NextRequest;
		}
	}

	if (NextRequest != nullptr)
	{
		InstallerPipe.Launch(
			TEXT("ExecuteRequest"),
			[this, NextRequest] { ExecuteRequest(*NextRequest); },
			UE::Tasks::ETaskPriority::BackgroundLow);
	}
}

void FOnDemandContentInstaller::ExecuteRequest(FRequest& Request)
{
	struct FVisitor
	{
		void operator()(FEmptyVariantState& Empty)
		{
			ensure(false);
		}

		void operator()(FRequest::FInstall&)
		{
			Installer.ExecuteInstallRequest(Request, /*bRemoveAlreadyCachedChunks*/ true);
		}

		void operator()(FRequest::FPurge&)
		{
			Installer.ExecutePurgeRequest(Request);
		}

		void operator()(FRequest::FDefrag& Defrag)
		{
			Installer.ExecuteDefragRequest(Request);
		}

		void operator()(FRequest::FVerify& Verify)
		{
			Installer.ExecuteVerifyRequest(Request);
		}

		FOnDemandContentInstaller&	Installer;
		FRequest&					Request;
	};

	FVisitor Visitor { .Installer = *this, .Request = Request };
	Visit(Visitor, Request.Variant);
}

void FOnDemandContentInstaller::ProcessInstallRequest(FRequest& Request)
{
	using namespace UE::IoStore::Private;

	TRACE_CPUPROFILER_EVENT_SCOPE(FOnDemandContentInstaller::ProcessInstallRequest);

	FRequest::FInstall& InstallRequest	= Request.AsInstall();
	Request.Priority					= InstallRequest.Args.Priority;

	UE_LOG(LogIoStoreOnDemand, Verbose, TEXT("Processing install request, SeqNo=%u, Priority=%d, ContentHandle=(%s)"),
		Request.SeqNo, Request.Priority, *LexToString(InstallRequest.Args.ContentHandle));

	if (InstallRequest.Args.ContentHandle.IsValid() == false)
	{
		Request.ErrorCode	= EIoErrorCode::InvalidParameter;
		Request.ErrorReason	= TEXT("Invalid content handle");

		return CompleteInstallRequest(Request);
	}

	const TSharedPtr<FOnDemandInternalContentHandle>& ContentHandle = InstallRequest.Args.ContentHandle.Handle;
	if (ContentHandle->IoStore.IsValid() == false)
	{
		// First time this content handle is used
		ContentHandle->IoStore = IoStore.AsWeak();
	}

	TSet<FSharedOnDemandContainer> ContainersForInstallation;
	TSet<FPackageId> PackageIdsToInstall;
	if (FIoStatus Status = IoStore.GetContainersAndPackagesForInstall(
		InstallRequest.Args.MountId,
		InstallRequest.Args.TagSets,
		InstallRequest.Args.PackageIds,
		ContainersForInstallation,
		PackageIdsToInstall); !Status.IsOk())
	{
		Request.ErrorCode	= Status.GetErrorCode();
		Request.ErrorReason	= Status.ToString(); 

		return CompleteInstallRequest(Request);
	}

#if !UE_BUILD_SHIPPING
	if (!UE::IoStore::CVars::IoStoreErrorOnRequest.IsEmpty())
	{
		if (FCString::Strstr(*ContentHandle->DebugName, *UE::IoStore::CVars::IoStoreErrorOnRequest) != nullptr || 
			FCString::Strstr(*InstallRequest.Args.DebugName, *UE::IoStore::CVars::IoStoreErrorOnRequest) != nullptr)
		{
			Request.ErrorCode	= (EIoErrorCode)((FMath::Rand() / (RAND_MAX / ((int)EIoErrorCode::Last-1))) + 1);
			Request.ErrorReason = FString::Printf(TEXT("Debug error requested on debug name %s"), *UE::IoStore::CVars::IoStoreErrorOnRequest);

			return CompleteInstallRequest(Request);
		}
	}
#endif

	if (Request.ErrorCode.load(std::memory_order_relaxed) == EIoErrorCode::Cancelled)
	{
		return CompleteInstallRequest(Request);
	}

	TSet<FPackageId> Missing;
	const bool bIncludeSoftReferences = EnumHasAnyFlags(InstallRequest.Args.Options, EOnDemandInstallOptions::InstallSoftReferences);
	Private::ResolveChunksToInstall(
		ContainersForInstallation,
		PackageIdsToInstall,
		bIncludeSoftReferences,
		InstallRequest.ResolvedChunks,
		Missing);

	// Check the other I/O backends for missing package chunks
	if (Missing.IsEmpty() == false)
	{
		FIoDispatcher& IoDispatcher = FIoDispatcher::Get();
		uint32 MissingCount = 0;

		for (const FPackageId& PackageId : Missing)
		{
			const FIoChunkId ChunkId = CreatePackageDataChunkId(PackageId);
			if (IoDispatcher.DoesChunkExist(ChunkId) == false)
			{
				UE_CLOG(MissingCount == 0, LogIoStoreOnDemand, Warning, TEXT("Failed to resolve the following chunk(s) for content handle '%s':"),
					*LexToString(InstallRequest.Args.ContentHandle));

				UE_LOG(LogIoStoreOnDemand, Warning, TEXT("\tChunkId='%s'"), *LexToString(ChunkId));
				MissingCount++;
			}
		}

		if (MissingCount > 0) 
		{
			Request.ErrorCode	= EIoErrorCode::UnknownChunkID;
			Request.ErrorReason	= FString::Printf(TEXT("Missing chunk(s), Count=%u, ContentHandle='%s'"),
				MissingCount, *LexToString(InstallRequest.Args.ContentHandle));

			return CompleteInstallRequest(Request);
		}
	}

	if (Request.ErrorCode.load(std::memory_order_relaxed) == EIoErrorCode::Cancelled)
	{
		return CompleteInstallRequest(Request);
	}

	uint64 TotalContentSize	= 0;
	uint64 TotalInstallSize	= 0;

	// Find all chunks we need to fetch from the resolved chunk(s)
	{
		for (int32 ContainerIndex = 0; FResolvedContainerChunks& ResolvedChunks : InstallRequest.ResolvedChunks)
		{
			TArray<int32, TInlineAllocator<64>> CachedEntryIndices;
			for (int32 EntryIndex : ResolvedChunks.EntryIndices)
			{
				const FOnDemandChunkEntry& Entry = ResolvedChunks.Container->ChunkEntries[EntryIndex];
				const bool bCached = IoStore.InstallCache->IsChunkCached(Entry.Hash);
				if (bCached)
				{
					CachedEntryIndices.Add(EntryIndex);
				}
				else
				{
					InstallRequest.HttpRequestHandles.Add(FChunkHttpRequestHandle
					{
						.Handle			= nullptr,
						.ContainerIndex = ContainerIndex,
						.EntryIndex		= EntryIndex
					});
					TotalInstallSize += Entry.EncodedSize;
				}
				++InstallRequest.ResolvedChunkCount;
				TotalContentSize += Entry.EncodedSize;
			}

			// Add references to existing chunk(s)
			if (CachedEntryIndices.IsEmpty() == false)
			{
				TUniqueLock Lock(IoStore.ContainerMutex);

				FOnDemandChunkEntryReferences& References = ResolvedChunks.Container->FindOrAddChunkEntryReferences(*ContentHandle);
				for (int32 EntryIndex : CachedEntryIndices)
				{
					References.Indices[EntryIndex] = true;
				}
			}

			ContainerIndex++;
		}
	}

	InstallRequest.Progress.TotalContentSize	= TotalContentSize;
	InstallRequest.Progress.TotalInstallSize	= TotalInstallSize;
	InstallRequest.Progress.CurrentInstallSize	= 0;

	if (InstallRequest.HttpRequestHandles.IsEmpty())
	{
		Request.ErrorCode = EIoErrorCode::Ok;

		return CompleteInstallRequest(Request);
	}

	if (Request.ErrorCode.load(std::memory_order_relaxed) == EIoErrorCode::Cancelled)
	{
		return CompleteInstallRequest(Request);
	}

	bool bExecuteRequest = false;
	{
		TUniqueLock Lock(Mutex);
		if (CurrentRequest == nullptr)
		{
			CurrentRequest	= &Request;
			bExecuteRequest	= true;
		}
		else
		{
			RequestQueue.HeapPush(&Request, RequestSortPredicate);
		}
	}

	if (bExecuteRequest)
	{
		check(CurrentRequest == &Request);
		const bool bRemoveAlreadyCachedChunks = false;
		ExecuteInstallRequest(Request, bRemoveAlreadyCachedChunks);
	}
}

void FOnDemandContentInstaller::ExecuteInstallRequest(FRequest& Request, bool bRemoveAlreadyCachedChunks)
{
	check(Request.IsInstall());
	check(&Request == CurrentRequest);

	FRequest::FInstall& InstallRequest = Request.AsInstall();
	check(InstallRequest.HttpRequestHandles.IsEmpty() == false);

	UE_LOG(LogIoStoreOnDemand, Verbose, TEXT("Executing install request, SeqNo=%u, Priority=%d, ContentHandle=(%s)"),
		Request.SeqNo, Request.Priority, *LexToString(InstallRequest.Args.ContentHandle));

	if (Request.ErrorCode.load(std::memory_order_relaxed) == EIoErrorCode::Cancelled)
	{
		return CompleteInstallRequest(Request);
	}

	if (bRemoveAlreadyCachedChunks)
	{
		InstallRequest.HttpRequestHandles.SetNum(
			Algo::RemoveIf(
				InstallRequest.HttpRequestHandles,
				[this, &InstallRequest](FChunkHttpRequestHandle& HttpRequest)
				{
					FSharedOnDemandContainer& Container		= InstallRequest.ResolvedChunks[HttpRequest.ContainerIndex].Container;
					const FOnDemandChunkEntry& ChunkEntry	= Container->ChunkEntries[HttpRequest.EntryIndex];

					if (IoStore.InstallCache->IsChunkCached(ChunkEntry.Hash))
					{
						InstallRequest.Progress.TotalInstallSize -= ChunkEntry.EncodedSize;
						return true;
					}

					return false;
				}));

		if (InstallRequest.HttpRequestHandles.IsEmpty())
		{
			Request.ErrorCode = EIoErrorCode::Ok;
			return CompleteInstallRequest(Request);
		}
	}

	// Make sure we have enough space in the cache
	{
		TSet<FIoHash> ChunksToInstall;
		for (FChunkHttpRequestHandle& HttpRequest : InstallRequest.HttpRequestHandles)
		{
			FSharedOnDemandContainer& Container		= InstallRequest.ResolvedChunks[HttpRequest.ContainerIndex].Container;
			const FOnDemandChunkEntry& ChunkEntry	= Container->ChunkEntries[HttpRequest.EntryIndex];

			ChunksToInstall.Add(ChunkEntry.Hash);
		}

		if (FIoStatus Status = IoStore.InstallCache->Purge(MoveTemp(ChunksToInstall)); !Status.IsOk())
		{
			Request.ErrorCode	= Status.GetErrorCode();
			Request.ErrorReason	= Status.ToString(); 

			return CompleteInstallRequest(Request);
		}
	}

	if (Request.ErrorCode.load(std::memory_order_relaxed) == EIoErrorCode::Cancelled)
	{
		return CompleteInstallRequest(Request);
	}

	NotifyInstallProgress(Request);

	for (FChunkHttpRequestHandle& HttpRequest : InstallRequest.HttpRequestHandles)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FOnDemandContentInstaller::IssueRequest);
		FSharedOnDemandContainer& Container		= InstallRequest.ResolvedChunks[HttpRequest.ContainerIndex].Container;
		const FOnDemandChunkEntry& ChunkEntry	= Container->ChunkEntries[HttpRequest.EntryIndex];

		HttpRequest.Handle = HttpClient.IssueRequest(
			FOnDemandChunkInfo(Container, ChunkEntry),
			FIoOffsetAndLength(),
			Request.Priority,
			[this, &Request, &HttpRequest](uint32 HttpStatusCode, FStringView ErrorReason, FIoBuffer&& Chunk)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(FOnDemandContentInstaller::Callback);
				InstallerPipe.Launch(
					TEXT("ProcessIoStoreOnDemandDownloadedChunk"),
					[this, &Request, &HttpRequest, HttpStatusCode, ErrorReason = FString(ErrorReason), Chunk = MoveTemp(Chunk)]() mutable
					{
						ProcessDownloadedChunk(
							Request,
							HttpRequest,
							HttpStatusCode,
							MoveTemp(ErrorReason),
							MoveTemp(Chunk));
					},
					UE::Tasks::ETaskPriority::BackgroundLow);
			}, EHttpRequestType::Installed);
	}
	
	InstallRequest.bHttpRequestsIssued = true;
}

void FOnDemandContentInstaller::ExecutePurgeRequest(FRequest& Request)
{
	check(Request.IsPurge());
	check(&Request == CurrentRequest);

	FRequest::FPurge& PurgeRequest	= Request.AsPurge();
	const bool bDefrag				= EnumHasAnyFlags(PurgeRequest.Args.Options, EOnDemandPurgeOptions::Defrag);
	const uint64* BytesToPurge		= PurgeRequest.Args.BytesToPurge.GetPtrOrNull();

	UE_LOG(LogIoStoreOnDemand, Log, TEXT("Executing purge request, BytesToPurge=%llu, Defrag='%s'"),
		BytesToPurge != nullptr ? *BytesToPurge : -1, bDefrag ? TEXT("True") : TEXT("False"));

	if (Request.ErrorCode.load(std::memory_order_relaxed) == EIoErrorCode::Cancelled)
	{
		return CompletePurgeRequest(Request);
	}

	const FIoStatus Status	= IoStore.InstallCache->PurgeAllUnreferenced(bDefrag, BytesToPurge);
	Request.ErrorCode		= Status.GetErrorCode();

	if (!Status.IsOk())
	{
		Request.ErrorReason = Status.ToString();
	}

	CompletePurgeRequest(Request); 
}

void FOnDemandContentInstaller::ExecuteDefragRequest(FRequest& Request)
{
	check(Request.IsDefrag());
	check(&Request == CurrentRequest);

	FRequest::FDefrag& DefragRequest	= Request.AsDefrag();
	const uint64* BytesToFree			= DefragRequest.Args.BytesToFree.GetPtrOrNull();

	UE_LOG(LogIoStoreOnDemand, Log, TEXT("Executing defrag request, BytesToFree=%llu"),
		BytesToFree != nullptr ? *BytesToFree : -1); 

	if (Request.ErrorCode.load(std::memory_order_relaxed) == EIoErrorCode::Cancelled)
	{
		return CompleteDefragRequest(Request);
	}

	const FIoStatus Status	= IoStore.InstallCache->DefragAll(BytesToFree);
	Request.ErrorCode		= Status.GetErrorCode();

	if (!Status.IsOk())
	{
		Request.ErrorReason = Status.ToString();
	}

	CompleteDefragRequest(Request); 
}

void FOnDemandContentInstaller::ExecuteVerifyRequest(FRequest& Request)
{
	check(Request.IsVerify());
	check(&Request == CurrentRequest);

	UE_LOG(LogIoStoreOnDemand, Log, TEXT("Executing verify cache request"));	

	if (Request.ErrorCode.load(std::memory_order_relaxed) == EIoErrorCode::Cancelled)
	{
		return CompleteVerifyRequest(Request);
	}

	const FIoStatus Status	= IoStore.InstallCache->Verify();
	Request.ErrorCode		= Status.GetErrorCode();

	if (!Status.IsOk())
	{
		Request.ErrorReason = Status.ToString();
	}

	CompleteVerifyRequest(Request); 
}

void FOnDemandContentInstaller::ProcessDownloadedChunk(
	FRequest& Request,
	FChunkHttpRequestHandle& HttpRequest,
	uint32 HttpStatusCode,
	FString&& ErrorReason,
	FIoBuffer&& Chunk)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FOnDemandContentInstaller::ProcessDownloadedChunk);

	FRequest::FInstall& InstallRequest			= Request.AsInstall();
	HttpRequest.Handle							= nullptr;
	FSharedOnDemandContainer& Container			= InstallRequest.ResolvedChunks[HttpRequest.ContainerIndex].Container;
	const FOnDemandChunkEntry& ChunkEntry		= Container->ChunkEntries[HttpRequest.EntryIndex];
	const FIoChunkId& ChunkId					= Container->ChunkIds[HttpRequest.EntryIndex];

	InstallRequest.Progress.CurrentInstallSize	+= ChunkEntry.EncodedSize;

	UE_LOG(LogIoStoreOnDemand, Verbose, TEXT("Install progress %.2lf/%.2lf MiB, SeqNo=%u, Priority=%d, ContentHandle=(%s), ChunkId='%s', ChunkSize=%.2lf KiB, HttpStatus=%u"),
		double(InstallRequest.Progress.CurrentInstallSize) / 1024.0 / 1024.0, double(InstallRequest.Progress.TotalInstallSize) / 1024.0 / 1024.0,
		Request.SeqNo, Request.Priority, *LexToString(InstallRequest.Args.ContentHandle), *LexToString(ChunkId), double(ChunkEntry.EncodedSize) / 1024.0, HttpStatusCode);

	if (Request.ErrorCode == EIoErrorCode::Unknown)
	{
		const bool bHttpOk = HttpStatusCode > 199 && HttpStatusCode < 300 && Chunk.GetSize() > 0;
		if (bHttpOk)
		{
			const FIoHash ChunkHash = FIoHash::HashBuffer(Chunk.GetView());

			if (ChunkEntry.Hash == ChunkHash)
			{
				if (FIoStatus Status = IoStore.InstallCache->PutChunk(MoveTemp(Chunk), ChunkHash); !Status.IsOk())
				{
					Request.ErrorCode	= Status.GetErrorCode();
					Request.ErrorReason	= Status.ToString();
				}
			}
			else
			{
				Request.ErrorCode	= EIoErrorCode::ReadError;
				Request.ErrorReason	= FString::Printf(TEXT("Hash mismatch, ChunkId='%s', ExpectedHash='%s', ActualHash='%s'"),
					*LexToString(ChunkId), *LexToString(ChunkEntry.Hash), *LexToString(ChunkHash));
			}
		}
		else
		{
			Request.ErrorCode	= EIoErrorCode::ReadError;
			Request.ErrorReason	= FString::Printf(TEXT("Http failure, StatusCode=%u, Reason=%s"), HttpStatusCode, *ErrorReason);
		}
	}

	const bool bCompleted = ++InstallRequest.DownloadedChunkCount >= InstallRequest.HttpRequestHandles.Num();
	if (bCompleted)
	{
		if (FIoStatus Status = IoStore.InstallCache->Flush(); !Status.IsOk())
		{
			Request.ErrorCode	= Status.GetErrorCode();
			Request.ErrorReason	= Status.ToString();
		}
		if (Request.ErrorCode == EIoErrorCode::Unknown)
		{
			Request.ErrorCode = EIoErrorCode::Ok;
		}

		CompleteInstallRequest(Request);
	}
	else 
	{
		NotifyInstallProgress(Request);

		if (Request.ErrorCode != EIoErrorCode::Unknown)
		{
			int32 NumCancelled = 0;
			for (FChunkHttpRequestHandle& PendingHttpRequest : InstallRequest.HttpRequestHandles)
			{
				if (PendingHttpRequest.Handle != nullptr)
				{
					HttpClient.CancelRequest(PendingHttpRequest.Handle);
					++NumCancelled;
				}
			}

			UE_CLOG(NumCancelled == 0, LogIoStoreOnDemand, Log, TEXT("Cancelled %d HTTP request(s) due to install error"), NumCancelled);
		}
	}
}

void FOnDemandContentInstaller::NotifyInstallProgress(FRequest& Request)
{
	ensure(Request.IsInstall());

	FRequest::FInstall& InstallRequest = Request.AsInstall();

	if (!InstallRequest.OnProgress)
	{
		return;
	}

	const uint64 Cycles = FPlatformTime::Cycles64();
	const double SecondsSinceLastProgress = FPlatformTime::ToSeconds64(Cycles - InstallRequest.LastProgressCycles);
	if (InstallRequest.bNotifyingProgressOnGameThread.load(std::memory_order_seq_cst) || SecondsSinceLastProgress < .25)
	{
		return;
	}
	InstallRequest.LastProgressCycles = Cycles;

	//TODO: Remove support for notifying progress on the game thread
	FOnDemandInstallProgress Progress = InstallRequest.Progress;
	if (EnumHasAnyFlags(InstallRequest.Args.Options, EOnDemandInstallOptions::CallbackOnGameThread))
	{
		InstallRequest.bNotifyingProgressOnGameThread = true;
		ExecuteOnGameThread(
			UE_SOURCE_LOCATION,
			[&InstallRequest, Progress]()
			{
				InstallRequest.OnProgress(InstallRequest.Progress);
				InstallRequest.bNotifyingProgressOnGameThread = false;
			});
	}
	else
	{
		InstallRequest.OnProgress(Progress);
	}
}

void FOnDemandContentInstaller::CompleteInstallRequest(FRequest& Request)
{
	using namespace UE::IoStore::Private;

	FRequest::FInstall& InstallRequest = Request.AsInstall();
	ensure(Request.ErrorCode != EIoErrorCode::Unknown);

	// Mark all resolved chunk(s) as referenced by the content handle and notify the package store to update
	if (Request.ErrorCode == EIoErrorCode::Ok && InstallRequest.ResolvedChunkCount > 0)
	{
		{
			FOnDemandContentHandle& ContentHandle = InstallRequest.Args.ContentHandle;

			TUniqueLock Lock(IoStore.ContainerMutex);
			for (FResolvedContainerChunks& ResolvedChunks : InstallRequest.ResolvedChunks)
			{
				const FSharedOnDemandContainer& Container = ResolvedChunks.Container;
				FOnDemandChunkEntryReferences& References = Container->FindOrAddChunkEntryReferences(*ContentHandle.Handle);
				for (int32 EntryIndex : ResolvedChunks.EntryIndices)
				{
					References.Indices[EntryIndex] = true;
				}
			}
		}

		IoStore.PackageStoreBackend->NeedsUpdate(EOnDemandPackageStoreUpdateMode::ReferencedPackages);
	}

	const uint64 DurationCycles = FPlatformTime::Cycles64() - Request.StartTimeCycles; 
	const double CacheHitRatio	= InstallRequest.Progress.TotalContentSize > 0
		? double(InstallRequest.Progress.TotalContentSize - InstallRequest.Progress.TotalInstallSize) / double(InstallRequest.Progress.TotalContentSize)
		: 0.0;

	FOnDemandInstallResult InstallResult;
	InstallResult.Status				= Request.ErrorCode == EIoErrorCode::Ok ? FIoStatus::Ok : FIoStatus(Request.ErrorCode, Request.ErrorReason);
	InstallResult.Progress				= InstallRequest.Progress;
	InstallResult.DurationInSeconds		= FPlatformTime::ToSeconds64(DurationCycles);

	FOnDemandContentInstallerStats::OnRequestCompleted(
		InstallRequest.ResolvedChunkCount,
		InstallResult.Progress.TotalContentSize,
		static_cast<uint64>(InstallRequest.HttpRequestHandles.Num()),
		InstallResult.Progress.TotalInstallSize,
		CacheHitRatio,
		DurationCycles,
		Request.ErrorCode);

	UE_LOG(LogIoStoreOnDemand, Verbose, TEXT("Install request completed, Result='%s', SeqNo=%u, Priority=%d, ContentHandle=(%s), ContentSize=%.2lf MiB, InstallSize=%.2lf MiB, CacheHitRatio=%d%%, Duration=%dms"),
		GetIoErrorText(InstallResult.Status.GetErrorCode()), Request.SeqNo, Request.Priority, *LexToString(InstallRequest.Args.ContentHandle), double(InstallResult.Progress.TotalContentSize) / 1024.0 / 1024.0,
		double(InstallResult.Progress.TotalInstallSize) / 1024.0 / 1024.0, int32(CacheHitRatio * 100), int32(InstallResult.DurationInSeconds * 1000));

	{
		UE::TUniqueLock Lock(Mutex);

		InstallRequest.Request->InstallerRequest = 0;
		if (&Request == CurrentRequest)
		{
			CurrentRequest = nullptr;
		}
	}

	TryExecuteNextRequest();

	FOnDemandInstallRequest::EStatus RequestStatus = FOnDemandInstallRequest::EStatus::None;
	switch (InstallResult.Status.GetErrorCode())
	{
		case EIoErrorCode::Ok:
			RequestStatus = FOnDemandInstallRequest::Ok;
			break;
		case EIoErrorCode::Cancelled:
			RequestStatus = FOnDemandInstallRequest::Cancelled;
			break;
		default:
			RequestStatus = FOnDemandInstallRequest::Error;
			break;
	}

	if (!InstallRequest.OnCompleted)
	{
		InstallRequest.Request->Status.store(RequestStatus);
		{
			UE::TUniqueLock Lock(Mutex);
			RequestAllocator.Destroy(&Request);
		}
		return;
	}

	const bool bCallbackOnGameThread = EnumHasAnyFlags(InstallRequest.Args.Options, EOnDemandInstallOptions::CallbackOnGameThread);
	if (bCallbackOnGameThread)
	{
		ExecuteOnGameThread(
			UE_SOURCE_LOCATION,
			[this, &Request, InstallResult = MoveTemp(InstallResult), RequestStatus]() mutable
			{
				FRequest::FInstall& InstallRequest		= Request.AsInstall();
				FOnDemandInstallCompleted OnCompleted	= MoveTemp(InstallRequest.OnCompleted);

				ensure(InstallRequest.bNotifyingProgressOnGameThread == false);
				OnCompleted(MoveTemp(InstallResult));
				InstallRequest.Request->Status.store(RequestStatus);
				{
					UE::TUniqueLock Lock(Mutex);
					RequestAllocator.Destroy(&Request);
				}
			});
	}
	else
	{
		FOnDemandInstallCompleted OnCompleted = MoveTemp(InstallRequest.OnCompleted);
		OnCompleted(MoveTemp(InstallResult));
		InstallRequest.Request->Status.store(RequestStatus);
		{
			UE::TUniqueLock Lock(Mutex);
			RequestAllocator.Destroy(&Request);
		}
	}
}

void FOnDemandContentInstaller::CompletePurgeRequest(FRequest& Request)
{
	check(&Request == CurrentRequest);

	const uint64 DurationCycles = FPlatformTime::Cycles64() - Request.StartTimeCycles; 
	
	FOnDemandPurgeResult PurgeResult;
	PurgeResult.Status				= Request.ErrorCode == EIoErrorCode::Ok ? FIoStatus::Ok : FIoStatus(Request.ErrorCode, Request.ErrorReason);
	PurgeResult.DurationInSeconds	= FPlatformTime::ToSeconds64(DurationCycles);

	UE_LOG(LogIoStoreOnDemand, Log, TEXT("Purge request completed, Result='%s', Duration=%d ms"),
		GetIoErrorText(PurgeResult.Status.GetErrorCode()), int32(PurgeResult.DurationInSeconds * 1000));
	
	const bool bCallbackOnGameThread	= EnumHasAnyFlags(Request.AsPurge().Args.Options, EOnDemandPurgeOptions::CallbackOnGameThread);
	FOnDemandPurgeCompleted OnCompleted = MoveTemp(Request.AsPurge().OnCompleted);
	{
		UE::TUniqueLock Lock(Mutex);
		if (&Request == CurrentRequest)
		{
			CurrentRequest = nullptr;
		}
		RequestAllocator.Destroy(&Request);
	}

	TryExecuteNextRequest();

	if (!OnCompleted)
	{
		return;
	}

	if (bCallbackOnGameThread)
	{
		ExecuteOnGameThread(
			UE_SOURCE_LOCATION,
			[OnCompleted = MoveTemp(OnCompleted), PurgeResult = MoveTemp(PurgeResult)]() mutable
			{
				OnCompleted(MoveTemp(PurgeResult));
			});
	}
	else
	{
		OnCompleted(MoveTemp(PurgeResult));
	}
}

void FOnDemandContentInstaller::CompleteDefragRequest(FRequest& Request)
{
	check(&Request == CurrentRequest);

	const uint64 DurationCycles = FPlatformTime::Cycles64() - Request.StartTimeCycles; 

	FOnDemandDefragResult DefragResult;
	DefragResult.Status				= Request.ErrorCode == EIoErrorCode::Ok ? FIoStatus::Ok : FIoStatus(Request.ErrorCode, Request.ErrorReason);
	DefragResult.DurationInSeconds	= FPlatformTime::ToSeconds64(DurationCycles);

	UE_LOG(LogIoStoreOnDemand, Log, TEXT("Defrag request completed, Result='%s', Duration=%d ms"),
		GetIoErrorText(DefragResult.Status.GetErrorCode()), int32(DefragResult.DurationInSeconds * 1000));

	const bool bCallbackOnGameThread		= EnumHasAnyFlags(Request.AsDefrag().Args.Options, EOnDemandDefragOptions::CallbackOnGameThread);
	FOnDemandDefragCompleted OnCompleted	= MoveTemp(Request.AsDefrag().OnCompleted);
	{
		UE::TUniqueLock Lock(Mutex);
		if (&Request == CurrentRequest)
		{
			CurrentRequest = nullptr;
		}
		RequestAllocator.Destroy(&Request);
	}

	TryExecuteNextRequest();	

	if (!OnCompleted)
	{
		return;
	}

	if (bCallbackOnGameThread)
	{
		ExecuteOnGameThread(
			UE_SOURCE_LOCATION,
			[OnCompleted = MoveTemp(OnCompleted), DefragResult = MoveTemp(DefragResult)]() mutable
			{
				OnCompleted(MoveTemp(DefragResult));
			});
	}
	else
	{
		OnCompleted(MoveTemp(DefragResult));
	}
}

void FOnDemandContentInstaller::CompleteVerifyRequest(FRequest& Request)
{
	const uint64 DurationCycles = FPlatformTime::Cycles64() - Request.StartTimeCycles; 

	FOnDemandVerifyCacheResult VerifyResult;
	VerifyResult.Status = Request.ErrorCode == EIoErrorCode::Ok ? FIoStatus::Ok : FIoStatus(Request.ErrorCode, Request.ErrorReason);
	VerifyResult.DurationInSeconds	= FPlatformTime::ToSeconds64(DurationCycles);

	UE_LOG(LogIoStoreOnDemand, Log, TEXT("Verify request completed, Result='%s', Duration=%d ms"),
		GetIoErrorText(VerifyResult.Status.GetErrorCode()), int32(VerifyResult.DurationInSeconds * 1000));

	FOnDemandVerifyCacheCompleted OnCompleted = MoveTemp(Request.AsVerify().OnCompleted);
	{
		UE::TUniqueLock Lock(Mutex);
		if (&Request == CurrentRequest)
		{
			CurrentRequest = nullptr;
		}
		RequestAllocator.Destroy(&Request);
	}

	if (!OnCompleted)
	{
		return;
	}

	OnCompleted(MoveTemp(VerifyResult));
}

void FOnDemandContentInstaller::Shutdown()
{
	bShuttingDown					= true;
	const double WaitTimeoutSeconds	= 5.0;
	const uint64 StartTimeCycles	= FPlatformTime::Cycles64();

	// Wait for the current request to finish
	for (;;)
	{
		double WaitTimeSeconds = FPlatformTime::ToSeconds64(FPlatformTime::Cycles64() - StartTimeCycles);
		if (WaitTimeSeconds > WaitTimeoutSeconds)
		{
			UE_LOG(LogIoStoreOnDemand, Warning, TEXT("Content installer shutdown cancelled after %.2lf"), WaitTimeSeconds);
			break;
		}

		InstallerPipe.WaitUntilEmpty(FTimespan::FromSeconds(1.0));
		{
			TUniqueLock Lock(Mutex);
			if (CurrentRequest == nullptr)
			{
				break;
			}
		}
	}

	{
		TUniqueLock Lock(Mutex);
		UE_CLOG(CurrentRequest != nullptr, LogIoStoreOnDemand, Error, TEXT("Content installer has still inflight request(s) while shutting down"));
	}

	// Cancel all remaining request(s)
	for (;;)
	{
		FRequest* NextRequest = nullptr;
		{
			TUniqueLock Lock(Mutex);
			if (RequestQueue.IsEmpty() == false)
			{
				RequestQueue.HeapPop(NextRequest, RequestSortPredicate, EAllowShrinking::No);
				CurrentRequest = NextRequest;
			}
		}

		if (NextRequest == nullptr)
		{
			break;
		}

		NextRequest->ErrorCode = EIoErrorCode::Cancelled;
		ExecuteRequest(*NextRequest);
	}
}

} // namespace UE::IoStore
