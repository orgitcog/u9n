// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Async/Mutex.h"
#include "Containers/Set.h"
#include "IO/IoStoreOnDemand.h"
#include "IO/PackageId.h"
#include "IO/IoStoreOnDemandInternals.h"
#include "IO/IoAllocators.h"
#include "Misc/TVariant.h"
#include "OnDemandIoStore.h"
#include "Tasks/Pipe.h"

#include <atomic>

class FIoBuffer;
struct FAnalyticsEventAttribute;

namespace UE::IoStore
{

class FOnDemandHttpThread;
class FOnDemandIoStore;

using FSharedOnDemandContainer = TSharedPtr<struct FOnDemandContainer, ESPMode::ThreadSafe>;

namespace Private
{

////////////////////////////////////////////////////////////////////////////////
struct FResolvedContainerChunks
{
	FSharedOnDemandContainer	Container;
	TArray<int32>				EntryIndices;
	uint64						TotalSize = 0;
};

////////////////////////////////////////////////////////////////////////////////
void ResolvePackageDependencies(
	const TSet<FPackageId>& PackageIds,
	bool bIncludeSoftReferences,
	TSet<FPackageId>& OutResolved,
	TSet<FPackageId>& OutMissing);

////////////////////////////////////////////////////////////////////////////////
void ResolveChunksToInstall(
	const TSet<FSharedOnDemandContainer>& Containers,
	const TSet<FPackageId>& PackageIds,
	bool bIncludeSoftReferences,
	TArray<FResolvedContainerChunks>& OutResolvedContainerChunks,
	TSet<FPackageId>& OutMissing);

} // namespace Private

////////////////////////////////////////////////////////////////////////////////
class FOnDemandContentInstaller
{
	struct FChunkHttpRequestHandle
	{
		void*	Handle = nullptr;
		int32	ContainerIndex = INDEX_NONE;
		int32	EntryIndex = INDEX_NONE;
	};

	struct FRequest
	{
		struct FInstall
		{
			FInstall(
				FOnDemandInstallArgs&& InArgs,
				FOnDemandInstallCompleted&& InOnCompleted,
				FOnDemandInstallProgressed&& InOnProgress)
					: Args(MoveTemp(InArgs))
					, OnCompleted(MoveTemp(InOnCompleted))
					, OnProgress(MoveTemp(InOnProgress)) { }

			FOnDemandInstallArgs						Args;
			FOnDemandInstallCompleted					OnCompleted;
			FOnDemandInstallProgressed					OnProgress;
			FSharedInternalInstallRequest				Request;
			TArray<Private::FResolvedContainerChunks>	ResolvedChunks;
			TArray<FChunkHttpRequestHandle>				HttpRequestHandles;
			FOnDemandInstallProgress					Progress;
			uint64										ResolvedChunkCount = 0;
			uint64										DownloadedChunkCount = 0;
			uint64										LastProgressCycles = 0;
			std::atomic_bool							bHttpRequestsIssued{false};
			std::atomic_bool							bNotifyingProgressOnGameThread{false};
		};

		struct FPurge
		{
			FPurge(FOnDemandPurgeArgs&& InArgs, FOnDemandPurgeCompleted&& InOnCompleted)
				: Args(MoveTemp(InArgs))
				, OnCompleted(MoveTemp(InOnCompleted)) { }

			FOnDemandPurgeArgs		Args;
			FOnDemandPurgeCompleted	OnCompleted;
		};

		struct FDefrag
		{
			FDefrag(FOnDemandDefragArgs&& InArgs, FOnDemandDefragCompleted&& InOnCompleted)
				: Args(MoveTemp(InArgs))
				, OnCompleted(MoveTemp(InOnCompleted)) { }

			FOnDemandDefragArgs			Args;
			FOnDemandDefragCompleted	OnCompleted;
		};

		struct FVerify
		{
			FVerify(FOnDemandVerifyCacheCompleted&& InOnCompleted)
				: OnCompleted(MoveTemp(InOnCompleted)) { }

			FOnDemandVerifyCacheCompleted	OnCompleted;
		};

		using FRequestVariant = TVariant<FEmptyVariantState, FInstall, FPurge, FDefrag, FVerify>;

		FRequest(FOnDemandInstallArgs&& Args, FOnDemandInstallCompleted&& OnCompleted, FOnDemandInstallProgressed&& OnProgress)
		{
			Variant.Emplace<FInstall>(MoveTemp(Args), MoveTemp(OnCompleted), MoveTemp(OnProgress));
		}

		FRequest(FOnDemandPurgeArgs&& Args, FOnDemandPurgeCompleted&& OnCompleted)
		{
			Variant.Emplace<FPurge>(MoveTemp(Args), MoveTemp(OnCompleted));
		}

		FRequest(FOnDemandDefragArgs&& Args, FOnDemandDefragCompleted&& OnCompleted)
		{
			Variant.Emplace<FDefrag>(MoveTemp(Args), MoveTemp(OnCompleted));
		}

		FRequest(FOnDemandVerifyCacheCompleted&& OnCompleted)
		{
			Variant.Emplace<FVerify>(MoveTemp(OnCompleted));
		}

		bool			IsInstall() const	{ return Variant.IsType<FInstall>(); }
		bool			IsPurge() const		{ return Variant.IsType<FPurge>(); }
		bool			IsDefrag() const	{ return Variant.IsType<FDefrag>(); }
		bool			IsVerify() const	{ return Variant.IsType<FVerify>(); }

		FInstall&		AsInstall()			{ return Variant.Get<FInstall>(); }
		FPurge&			AsPurge()			{ return Variant.Get<FPurge>(); }
		FDefrag&		AsDefrag()			{ return Variant.Get<FDefrag>(); }
		FVerify&		AsVerify()			{ return Variant.Get<FVerify>(); }

		static uint32	NextSeqNo;

		uint32						SeqNo = NextSeqNo++;
		int32						Priority = 0;
		uint64						StartTimeCycles = FPlatformTime::Cycles64();
		FString						ErrorReason;
		std::atomic<EIoErrorCode>	ErrorCode{EIoErrorCode::Unknown};
		FRequestVariant				Variant;
	};

	static bool RequestSortPredicate(const FRequest& LHS, const FRequest& RHS)
	{
		if (LHS.Variant.GetIndex() == RHS.Variant.GetIndex())
		{
			if (LHS.Priority == RHS.Priority)
			{
				return LHS.SeqNo < RHS.SeqNo;
			}

			return LHS.Priority > RHS.Priority;
		}

		return LHS.SeqNo < RHS.SeqNo;
	}

	using FRequestAllocator	= TSingleThreadedSlabAllocator<FRequest, 32>;

public:
									FOnDemandContentInstaller(FOnDemandIoStore& IoStore, FOnDemandHttpThread& HttpClient);
									~FOnDemandContentInstaller();

	FSharedInternalInstallRequest	EnqueueInstallRequest(
										FOnDemandInstallArgs&& Args,
										FOnDemandInstallCompleted&& OnCompleted,
										FOnDemandInstallProgressed&& OnProgress);
	void							EnqueuePurgeRequest(FOnDemandPurgeArgs&& Args, FOnDemandPurgeCompleted&& OnCompleted);
	void							EnqueueDefragRequest(FOnDemandDefragArgs&& Args, FOnDemandDefragCompleted&& OnCompleted);
	void							EnqueueVerifyRequest(FOnDemandVerifyCacheCompleted&& OnCompleted);
	void							CancelInstallRequest(FSharedInternalInstallRequest InstallRequest);
	void							UpdateInstallRequestPriority(FSharedInternalInstallRequest InstallRequest, int32 NewPriority);
	void							ReportAnalytics(TArray<FAnalyticsEventAttribute>& OutAnalyticsArray) const;

private:
	void							TryExecuteNextRequest();
	void							ExecuteRequest(FRequest& Request);
	void							ProcessInstallRequest(FRequest& Request);
	void							ExecuteInstallRequest(FRequest& Request, bool bRemoveAlreadyCachedChunks);
	void							ExecutePurgeRequest(FRequest& Request);
	void							ExecuteDefragRequest(FRequest& Request);
	void							ExecuteVerifyRequest(FRequest& Request);
	void							ProcessDownloadedChunk(
										FRequest& Request,
										FChunkHttpRequestHandle& HttpRequest,
										uint32 HttpStatusCode,
										FString&& ErrorReason,
										FIoBuffer&& Chunk);
	void							NotifyInstallProgress(FRequest& Request);
	void							CompleteInstallRequest(FRequest& Request);
	void							CompletePurgeRequest(FRequest& Request);
	void							CompleteDefragRequest(FRequest& Request);
	void							CompleteVerifyRequest(FRequest& Request);
	void							Shutdown();

	FOnDemandIoStore&				IoStore;
	FOnDemandHttpThread&			HttpClient;
	UE::Tasks::FPipe				InstallerPipe;

	FMutex							Mutex;
	FRequestAllocator				RequestAllocator;
	TArray<FRequest*>				RequestQueue;
	FRequest*						CurrentRequest = nullptr;
	std::atomic_bool				bShuttingDown{false};
};

} // namespace UE::IoStore
