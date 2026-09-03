// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnDemandIoDispatcherBackend.h"

#include "Containers/BitArray.h"
#include "Containers/StringView.h"
#include "DistributionEndpoints.h"
#include "HAL/Event.h"
#include "HAL/LowLevelMemTracker.h"
#include "HAL/Platform.h"
#include "HAL/PlatformTime.h"
#include "HAL/PreprocessorHelpers.h"
#include "IO/Http/LaneTrace.h"
#include "IO/IoAllocators.h"
#include "IO/IoAllocators.h"
#include "IO/IoChunkEncoding.h"
#include "IO/IoDispatcher.h"
#include "IO/IoOffsetLength.h"
#include "IO/IoStatus.h"
#include "IO/IoStore.h"
#include "IO/IoStoreOnDemand.h"
#include "IasCache.h"
#include "IasHostGroup.h"
#include "LatencyTesting.h"
#include "Logging/StructuredLog.h"
#include "Math/NumericLimits.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/EnumClassFlags.h"
#include "Misc/PathViews.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "Misc/ScopeRWLock.h"
#include "OnDemandBackendStatus.h"
#include "OnDemandHttpClient.h"
#include "OnDemandHttpThread.h"
#include "OnDemandIoStore.h"
#include "ProfilingDebugging/IoStoreTrace.h"
#include "Statistics.h"
#include "String/Numeric.h"
#include "Tasks/Task.h"
#include "ThreadSafeIntrusiveQueue.h"

#include <atomic>

#if WITH_IOSTORE_ONDEMAND_TESTS
#include "TestHarness.h"
#include "TestMacros/Assertions.h"
#include <catch2/generators/catch_generators.hpp>
#endif

/** When enabled the IAS system can add additional debug console commands for development use */
#define UE_IAS_DEBUG_CONSOLE_CMDS (1 && !NO_CVARS && !UE_BUILD_SHIPPING)

/** When enabled we will run some limited testing on start up for issues that are hard to reproduce with normal gameplay */
#define UE_ENABLE_IAS_TESTING 0 && !UE_BUILD_SHIPPING

/** When enabled it is possible to disable request cancellation via the cvar '' */
#define UE_ALLOW_DISABLE_CANCELLING 1 && !UE_BUILD_SHIPPING

namespace UE::IoStore
{

////////////////////////////////////////////////////////////////////////////////
namespace HTTP {

IOSTOREHTTPCLIENT_API const void* GetIaxTraceChannel();

}

FLaneEstate* GRequestLaneEstate = LaneEstate_New({
	.Name = "Iax/Backend",
	.Group = "Iax",
	.Channel = HTTP::GetIaxTraceChannel(),
	.Weight = 10,
});

#if UE_TRACE_ENABLED

static void Trace(
	bool bIsPiggyback,
	const FIoChunkId& ChunkId,
	const struct FChunkRequestParams* Params);

#else

static void Trace(...) {}

#endif // UE_TRACE_ENABLED

///////////////////////////////////////////////////////////////////////////////

/** Note that GIasHttpPrimaryEndpoint has no effect after initial start up */
int32 GIasHttpPrimaryEndpoint = 0;
static FAutoConsoleVariableRef CVar_IasHttpPrimaryEndpoint(
	TEXT("ias.HttpPrimaryEndpoint"),
	GIasHttpPrimaryEndpoint,
	TEXT("Primary endpoint to use returned from the distribution endpoint")
);

int32 GIasHttpTimeOutMs = 10 * 1000;
static FAutoConsoleVariableRef CVar_IasHttpTimeOutMs(
	TEXT("ias.HttpTimeOutMs"),
	GIasHttpTimeOutMs,
	TEXT("Time out value for HTTP requests in milliseconds")
);

bool GIasHttpEnabled = true;
static FAutoConsoleVariableRef CVar_IasHttpEnabled(
	TEXT("ias.HttpEnabled"),
	GIasHttpEnabled,
	TEXT("Enables individual asset streaming via HTTP")
);

bool GIasHttpOptionalBulkDataEnabled = true;
static FAutoConsoleVariableRef CVar_IasHttpOptionalBulkDataEnabled(
	TEXT("ias.HttpOptionalBulkDataEnabled"),
	GIasHttpOptionalBulkDataEnabled,
	TEXT("Enables optional bulk data via HTTP")
);

bool GIasReportAnalyticsEnabled = true;
static FAutoConsoleVariableRef CVar_IoReportAnalytics(
	TEXT("ias.ReportAnalytics"),
	GIasReportAnalyticsEnabled,
	TEXT("Enables reporting statics to the analytics system")
);

int32 GIasHttpRangeRequestMinSizeKiB = 128;
static FAutoConsoleVariableRef CVar_IasHttpRangeRequestMinSizeKiB(
	TEXT("ias.HttpRangeRequestMinSizeKiB"),
	GIasHttpRangeRequestMinSizeKiB,
	TEXT("Minimum chunk size for partial chunk request(s)")
);

static int32 GDistributedEndpointRetryWaitTime = 15;
static FAutoConsoleVariableRef CVar_DistributedEndpointRetryWaitTime(
	TEXT("ias.DistributedEndpointRetryWaitTime"),
	GDistributedEndpointRetryWaitTime,
	TEXT("How long to wait (in seconds) after failing to resolve a distributed endpoint before retrying")
);

static int32 GDistributedEndpointAttemptCount = 5;
static FAutoConsoleVariableRef CVar_DistributedEndpointAttemptCount(
	TEXT("ias.DistributedEndpointAttemptCount"),
	GDistributedEndpointAttemptCount,
	TEXT("Number of times we should try to resolve a distributed endpoint befor eusing the fallback url (if there is one)")
);

bool GIasEnableWriteOnlyDecoding = false;
static FAutoConsoleVariableRef CVar_WriteOnlyDecoding(
	TEXT("ias.EnableWriteOnlyDecoding"),
	GIasEnableWriteOnlyDecoding,
	TEXT("Enables the use of 'WriteOnly' flag when decoding to buffers with the 'HardwareTargetBuffer' flag")
);

#if UE_ALLOW_DISABLE_CANCELLING
bool GIasCancelationEnabled = true;
static FAutoConsoleVariableRef CVar_IasCancelationEnabled(
	TEXT("ias.EnableCancelation"),
	GIasCancelationEnabled,
	TEXT("Allows existing IO requests to be canceled")
);
#endif //UE_ALLOW_DISABLE_CANCELLING

#if !UE_BUILD_SHIPPING
static int32 GIasPoisonCache = false;
static FAutoConsoleVariableRef CVar_IasPoisonCache(
	TEXT("ias.PoisonCache"),
	GIasPoisonCache,
	TEXT("Fills all data materialized from the cache with 0x4d")
);

#endif // !UE_BUILD_SHIPPING

// These priorities are indexed using the cvar below
static UE::Tasks::ETaskPriority GCompleteMaterializeTaskPriorities[] =
{
	UE::Tasks::ETaskPriority::High,
	UE::Tasks::ETaskPriority::Normal,
	UE::Tasks::ETaskPriority::BackgroundHigh,
	UE::Tasks::ETaskPriority::BackgroundNormal,
	UE::Tasks::ETaskPriority::BackgroundLow
};

static int32 GCompleteMaterializeTaskPriority = 3;
FAutoConsoleVariableRef CVarCompleteMaterializeTaskPriority(
	TEXT("ias.CompleteMaterializeTaskPriority"),
	GCompleteMaterializeTaskPriority,
	TEXT("Task priority for the CompleteCacheRead task (0 = foreground/high, 1 = foreground/normal, 2 = background/high, 3 = background/normal, 4 = background/low)."),
	ECVF_Default
);

///////////////////////////////////////////////////////////////////////////////

[[nodiscard]] UE::Tasks::ETaskPriority GetRequestCompletionTaskPriority()
{
	return GCompleteMaterializeTaskPriorities[FMath::Clamp(GCompleteMaterializeTaskPriority, 0, UE_ARRAY_COUNT(GCompleteMaterializeTaskPriorities) - 1)];
}

///////////////////////////////////////////////////////////////////////////////

static FIoHash GetChunkKey(const FIoHash& ChunkHash, const FIoOffsetAndLength& Range)
{
	FIoHashBuilder HashBuilder;
	HashBuilder.Update(ChunkHash.GetBytes(), sizeof(FIoHash::ByteArray));
	HashBuilder.Update(&Range, sizeof(FIoOffsetAndLength));

	return HashBuilder.Finalize();
}

///////////////////////////////////////////////////////////////////////////////
struct FChunkRequestParams
{
	static FChunkRequestParams Create(const FIoOffsetAndLength& OffsetLength, const FOnDemandChunkInfo& ChunkInfo)
	{
		FIoOffsetAndLength ChunkRange;
		if (ChunkInfo.EncodedSize() <= (uint64(GIasHttpRangeRequestMinSizeKiB) << 10))
		{
			ChunkRange = FIoOffsetAndLength(0, ChunkInfo.EncodedSize());
		}
		else
		{
			const uint64 RawSize = FMath::Min<uint64>(OffsetLength.GetLength(), ChunkInfo.RawSize() - OffsetLength.GetOffset());

			ChunkRange = FIoChunkEncoding::GetChunkRange(
				ChunkInfo.RawSize(),
				ChunkInfo.BlockSize(),
				ChunkInfo.Blocks(),
				OffsetLength.GetOffset(),
				RawSize).ConsumeValueOrDie();
		}

		return FChunkRequestParams{ GetChunkKey(ChunkInfo.Hash(), ChunkRange), ChunkRange, ChunkInfo };
	}

	static FChunkRequestParams Create(FIoRequestImpl* Request, const FOnDemandChunkInfo& ChunkInfo)
	{
		check(Request);
		check(Request->NextRequest == nullptr);
		return Create(FIoOffsetAndLength(Request->Options.GetOffset(), Request->Options.GetSize()), ChunkInfo);
	}

	const FIoHash& GetUrlHash() const
	{
		return ChunkInfo.Hash();
	}

	void GetUrl(FAnsiStringBuilderBase& Url) const
	{
		ChunkInfo.GetUrl(Url);
	}

	FIoChunkDecodingParams GetDecodingParams() const
	{
		FIoChunkDecodingParams Params;
		Params.EncryptionKey = ChunkInfo.EncryptionKey();
		Params.CompressionFormat = ChunkInfo.CompressionFormat();
		Params.BlockSize = ChunkInfo.BlockSize();
		Params.TotalRawSize = ChunkInfo.RawSize();
		Params.EncodedBlockSize = ChunkInfo.Blocks();
		Params.BlockHash = ChunkInfo.BlockHashes();
		Params.EncodedOffset = ChunkRange.GetOffset();

		return Params;
	}

	FIoHash ChunkKey;
	FIoOffsetAndLength ChunkRange;
	FOnDemandChunkInfo ChunkInfo;
};

///////////////////////////////////////////////////////////////////////////////

struct FChunkRequest
{
	explicit FChunkRequest(FIoRequestImpl* Request, const FChunkRequestParams& RequestParams)
		: NextRequest()
		, Params(RequestParams)
		, RequestHead(Request)
		, RequestTail(Request)
		, StartTime(FPlatformTime::Cycles64())
		, Priority(Request->Priority)
		, RequestCount(1)
		, bCached(false)
	{
		check(Request && NextRequest == nullptr);

		FLaneTrace* Lane = LaneEstate_Build(GRequestLaneEstate, this);
		static uint32 LaneScopeId = LaneTrace_NewScope("Iax/Request");
		LaneTrace_Enter(Lane, LaneScopeId);
	}

	~FChunkRequest()
	{
		LaneScope = FLaneTraceScope();
		LaneEstate_Demolish(GRequestLaneEstate, this);
	}

	bool AddDispatcherRequest(FIoRequestImpl* Request)
	{
		/* disabled for the moment as closing of these scopes is little off
		FLaneTrace* Lane = LaneEstate_Lookup(GRequestLaneEstate, this);
		static uint32 LaneScopeId = LaneTrace_NewScope("Iax/Piggyback");
		LaneTrace_Enter(Lane, LaneScopeId);
		*/

		check(RequestHead && RequestTail);
		check(Request && !Request->NextRequest);

		const bool bPriorityChanged = Request->Priority > RequestHead->Priority;
		if (bPriorityChanged)
		{
			Priority = Request->Priority;
			Request->NextRequest = RequestHead;
			RequestHead = Request;
		}
		else
		{
			FIoRequestImpl* It = RequestHead;
			while (It->NextRequest != nullptr && Request->Priority <= It->NextRequest->Priority)
			{
				It = It->NextRequest;
			}

			if (RequestTail == It)
			{
				check(It->NextRequest == nullptr);
				RequestTail = Request;
			}

			Request->NextRequest = It->NextRequest;
			It->NextRequest = Request;
		}

		RequestCount++;
		return bPriorityChanged;
	}

	int32 RemoveDispatcherRequest(FIoRequestImpl* Request)
	{
		check(Request != nullptr);
		check(RequestCount > 0);

		if (RequestHead == Request)
		{
			RequestHead = Request->NextRequest;
			if (RequestTail == Request)
			{
				check(RequestHead == nullptr);
				RequestTail = nullptr;
			}
		}
		else
		{
			FIoRequestImpl* It = RequestHead;
			while (It->NextRequest != Request)
			{
				It = It->NextRequest;
				if (It == nullptr)
				{
					return INDEX_NONE; // Not found
				}
			}
			check(It->NextRequest == Request);
			It->NextRequest = It->NextRequest->NextRequest;

			if (RequestTail == Request)
			{
				check(It->NextRequest == nullptr);
				RequestTail = It;
			}
		}

		Request->NextRequest = nullptr;
		RequestCount--;

		return RequestCount;
	}

	FIoRequestImpl* DeqeueDispatcherRequests()
	{
		FIoRequestImpl* Head = RequestHead;
		RequestHead = RequestTail = nullptr;
		RequestCount = 0;

		return Head;
	}

	FChunkRequest* NextRequest;
	FChunkRequestParams Params;
	FIoRequestImpl* RequestHead;
	FIoRequestImpl* RequestTail;
	FOnDemandHttpThread::FRequestHandle HttpRequest = 0;
	FIASHostGroup HostGroup; // Still used in a few places even when UE_ENABLE_IAS_REQUEST_THREAD is enabled
	FIoBuffer Chunk;
	uint64 StartTime;
	int32 Priority;
	uint16 RequestCount;
	bool bCached;
	bool bCancelled = false;
	EIoErrorCode CacheGetStatus;
	FLaneTraceScope LaneScope;
};

//////////////////////////////////////////////////////////////////////////////

static void LogIoResult(
	const FIoChunkId& ChunkId,
	const FIoHash& UrlHash,
	uint64 DurationMs,
	uint64 UncompressedSize,
	uint64 UncompressedOffset,
	const FIoOffsetAndLength& ChunkRange,
	uint64 ChunkSize,
	int32 Priority,
	bool bCached)
{
	const TCHAR* Prefix = [bCached, UncompressedSize]() -> const TCHAR*
	{
		if (UncompressedSize == 0)
		{
			return bCached ? TEXT("io-cache-error") : TEXT("io-http-error ");
		}
		return bCached ? TEXT("io-cache") : TEXT("io-http ");
	}();

	auto PrioToString = [](int32 Prio) -> const TCHAR*
	{
		if (Prio < IoDispatcherPriority_Low)
		{
			return TEXT("Min");
		}
		if (Prio < IoDispatcherPriority_Medium)
		{
			return TEXT("Low");
		}
		if (Prio < IoDispatcherPriority_High)
		{
			return TEXT("Medium");
		}
		if (Prio < IoDispatcherPriority_Max)
		{
			return TEXT("High");
		}

		return TEXT("Max");
	};

	UE_LOG(LogIas, VeryVerbose, TEXT("%s: %5" UINT64_FMT "ms %5" UINT64_FMT "KiB[%7" UINT64_FMT "] % s: % s | Range: %" UINT64_FMT "-%" UINT64_FMT "/%" UINT64_FMT " (%.2f%%) | Prio: %s"),
		Prefix,
		DurationMs,
		UncompressedSize >> 10,
		UncompressedOffset,
		*LexToString(ChunkId),
		*LexToString(UrlHash),
		ChunkRange.GetOffset(), (ChunkRange.GetOffset() + ChunkRange.GetLength() - 1), ChunkSize,
		100.0f * (float(ChunkRange.GetLength()) / float(ChunkSize)),
		PrioToString(Priority));
};

///////////////////////////////////////////////////////////////////////////////
class FOnDemandIoBackend final
	: public IOnDemandIoDispatcherBackend
{
	using FIoRequestQueue = TThreadSafeIntrusiveQueue<FIoRequestImpl>;
	using FChunkRequestQueue = TThreadSafeIntrusiveQueue<FChunkRequest>;

	struct FBackendData
	{
		static void Attach(FIoRequestImpl* Request, const FIoHash& ChunkKey)
		{
			check(Request->BackendData == nullptr);
			Request->BackendData = new FBackendData{ChunkKey};
		}

		static TUniquePtr<FBackendData> Detach(FIoRequestImpl* Request)
		{
			check(Request->BackendData != nullptr);
			void* BackendData = Request->BackendData;
			Request->BackendData = nullptr;
			return TUniquePtr<FBackendData>(static_cast<FBackendData*>(BackendData));
		}
		
		static FBackendData* Get(FIoRequestImpl* Request)
		{
			return static_cast<FBackendData*>(Request->BackendData);
		}

		FIoHash ChunkKey;
	};

	struct FChunkRequests
	{
		FChunkRequest* TryUpdatePriority(FIoRequestImpl* Request)
		{
			FScopeLock _(&Mutex);

			const FBackendData* BackendData = FBackendData::Get(Request);
			if (BackendData == nullptr)
			{
				return nullptr;
			}

			if (FChunkRequest** InflightRequest = Inflight.Find(BackendData->ChunkKey))
			{
				FChunkRequest* ChunkRequest = *InflightRequest;
				if (Request->Priority > ChunkRequest->Priority)
				{
					ChunkRequest->Priority = Request->Priority;
					return ChunkRequest;
				}
			}

			return nullptr;
		}

		FChunkRequest* Create(FIoRequestImpl* Request, const FChunkRequestParams& Params, bool& bOutPending, bool& bOutUpdatePriority)
		{
			FScopeLock _(&Mutex);
			
			FBackendData::Attach(Request, Params.ChunkKey);

			if (FChunkRequest** InflightRequest = Inflight.Find(Params.ChunkKey))
			{
				FChunkRequest* ChunkRequest = *InflightRequest;
				check(!ChunkRequest->bCancelled);
				bOutPending = true;
				bOutUpdatePriority = ChunkRequest->AddDispatcherRequest(Request);

				Trace(true, Request->ChunkId, &Params);

				return ChunkRequest;
			}

			bOutPending = bOutUpdatePriority = false;
			FChunkRequest* ChunkRequest = Allocator.Construct(Request, Params);
			ChunkRequestCount++;
			Inflight.Add(Params.ChunkKey, ChunkRequest);

			Trace(false, Request->ChunkId, &Params);

			// Paranoid check to make sure that no host group has currently been assigned
			check(ChunkRequest->HostGroup.GetHostUrls().IsEmpty());

			return ChunkRequest;
		}

		bool Cancel(FIoRequestImpl* Request, FOnDemandHttpThread& TheHttpClient, IIasCache* TheCache)
		{
#if UE_ALLOW_DISABLE_CANCELLING
			if (GIasCancelationEnabled == false)
			{
				return false;
			}
#endif //UE_ALLOW_DISABLE_CANCELLING

			FScopeLock _(&Mutex);

			const FBackendData* BackendData = FBackendData::Get(Request);
			if (BackendData == nullptr)
			{
				return false;
			}

			UE_LOG(LogIas, VeryVerbose, TEXT("%s"),
				*WriteToString<256>(TEXT("Cancelling I/O request ChunkId='"), Request->ChunkId, TEXT("' ChunkKey='"), BackendData->ChunkKey, TEXT("'")));

			if (FChunkRequest** InflightRequest = Inflight.Find(BackendData->ChunkKey))
			{
				FChunkRequest& ChunkRequest = **InflightRequest;
				const int32 RemainingCount = ChunkRequest.RemoveDispatcherRequest(Request);
				if (RemainingCount == INDEX_NONE)
				{
					// Not found
					// When a request A with ChunkKey X enters CompleteRequest its Inflight entry X->A is removed.
					// If a new request B with the same ChunkKey X is made, then Resolve will add a new Infligt entry X->B.
					// If we at this point cancel A, we will find the Inflight entry for B, which will not contain A, which is fine.
					return false;
				}

				check(Request->NextRequest == nullptr);

				if (RemainingCount == 0)
				{
					check(ChunkRequest.RequestTail == nullptr);

					ChunkRequest.bCancelled = true;
					TheHttpClient.CancelRequest(ChunkRequest.HttpRequest);

					if (TheCache != nullptr)
					{
						TheCache->Cancel(ChunkRequest.Chunk);
					}
					Inflight.Remove(BackendData->ChunkKey);
				}

				return true;
			}

			return false;
		}

		FIoChunkId GetChunkId(FChunkRequest* Request)
		{
			FScopeLock _(&Mutex);
			return Request->RequestHead ? Request->RequestHead->ChunkId : FIoChunkId::InvalidChunkId;
		}

		void Remove(FChunkRequest* Request)
		{
			FScopeLock _(&Mutex);
			Inflight.Remove(Request->Params.ChunkKey);
		}

		void Release(FChunkRequest* Request)
		{
			FScopeLock _(&Mutex);
			Destroy(Request);
		}
		
		int32 Num()
		{
			FScopeLock _(&Mutex);
			return ChunkRequestCount; 
		}

	private:
		void Destroy(FChunkRequest* Request)
		{
			Allocator.Destroy(Request);
			ChunkRequestCount--;
			check(ChunkRequestCount >= 0);
		}

		TSingleThreadedSlabAllocator<FChunkRequest, 128> Allocator;
		TMap<FIoHash, FChunkRequest*> Inflight;
		FCriticalSection Mutex;
		int32 ChunkRequestCount = 0;
	};
public:

	FOnDemandIoBackend(const FOnDemandEndpointConfig& InConfig, FOnDemandIoStore& InIoStore, FOnDemandHttpThread& InHttpClient, TUniquePtr<IIasCache>&& InCache);
	virtual ~FOnDemandIoBackend();

	// I/O dispatcher backend
	virtual void Initialize(TSharedRef<const FIoDispatcherBackendContext> Context) override;
	virtual void Shutdown() override;
	virtual void ResolveIoRequests(FIoRequestList Requests, FIoRequestList& OutUnresolved) override;
	virtual void CancelIoRequest(FIoRequestImpl* Request) override;
	virtual void UpdatePriorityForIoRequest(FIoRequestImpl* Request) override;
	virtual bool DoesChunkExist(const FIoChunkId& ChunkId) const override;
	virtual bool DoesChunkExist(const FIoChunkId& ChunkId, const FIoOffsetAndLength& ChunkRange) const override;
	virtual TIoStatusOr<uint64> GetSizeForChunk(const FIoChunkId& ChunkId) const override;
	virtual TIoStatusOr<uint64> GetSizeForChunk(const FIoChunkId& ChunkId, const FIoOffsetAndLength& ChunkRange, uint64& OutAvailable) const;
	virtual FIoRequestImpl* GetCompletedIoRequests() override;
	virtual TIoStatusOr<FIoMappedRegion> OpenMapped(const FIoChunkId& ChunkId, const FIoReadOptions& Options) override;
	virtual const TCHAR* GetName() const override;

	// I/O Http backend
	virtual void SetBulkOptionalEnabled(bool bEnabled) override;
	virtual void AbandonCache() override;
	
	virtual void ReportAnalytics(TArray<FAnalyticsEventAttribute>& OutAnalyticsArray) const override;
	virtual TUniquePtr<IAnalyticsRecording> StartAnalyticsRecording() const override;
	
	virtual FOnDemandStreamingCacheUsage GetCacheUsage() const override;

private:
	bool Resolve(FIoRequestImpl* Request);

	void BeginCreateDefaultHostGroup();
	void IssueHttpRequest(FChunkRequest* ChunkRequest);
	void CompleteRequest(FChunkRequest* ChunkRequest);
	void CompleteCacheRead(FChunkRequest* ChunkRequest);
	bool ResolveDefaultHostGroup();
	bool CreateDefaultHostGroup();
	bool ResolveDistributedEndpoint(const FDistributedEndpointUrl& Url, TArray<FString>& OutUrls);
	int32 WaitForCompleteRequestTasks(float WaitTimeSeconds, float PollTimeSeconds);
	void OnThreadTickIdle();

#if UE_IAS_DEBUG_CONSOLE_CMDS
	void RunRequestTest(const TArray<FString>& Args) const;
#endif // UE_IAS_DEBUG_CONSOLE_CMDS

	FOnDemandIoStore& IoStore;
	FOnDemandHttpThread& HttpClient;
	TUniquePtr<IIasCache> Cache;
	TSharedPtr<const FIoDispatcherBackendContext> BackendContext;
	FChunkRequests ChunkRequests;
	FIoRequestQueue CompletedRequests;
	FBackendStatus BackendStatus;
	TArray<FString> DefaultUrls;
	FOnDemandIoBackendStats Stats;
	FDistributedEndpointUrl DistributionUrl;
	FEventRef DistributedEndpointEvent;
	FIASHostGroup DefaultHostGroup;
	FDelegateHandle OnThreadTickIdleHandle;
	mutable FRWLock Lock;
	std::atomic_uint32_t InflightCacheRequestCount{0};
	std::atomic_bool bStopRequested{false};

#if UE_IAS_DEBUG_CONSOLE_CMDS
	TArray<IConsoleCommand*> DynamicConsoleCommands;
#endif // UE_IAS_DEBUG_CONSOLE_CMDS
};

///////////////////////////////////////////////////////////////////////////////
FOnDemandIoBackend::FOnDemandIoBackend(const FOnDemandEndpointConfig& Config, FOnDemandIoStore& InIoStore, FOnDemandHttpThread& InHttpClient, TUniquePtr<IIasCache>&& InCache)
	: IoStore(InIoStore)
	, HttpClient(InHttpClient)
	, Cache(MoveTemp(InCache))
	, Stats(BackendStatus)
{
	FAnsiString EndpointTestPath = StringCast<ANSICHAR>(*Config.TocPath).Get();

	DefaultHostGroup = FHostGroupManager::Get().Register(FName("DefaultOnDemand"), EndpointTestPath).ConsumeValueOrDie();

	if (Config.DistributionUrl.IsEmpty() == false)
	{
		DistributionUrl = { Config.DistributionUrl, Config.FallbackUrl };
	}
	else
	{
		DefaultUrls = Config.ServiceUrls;
	}

	// Don't enable HTTP until the background thread has been started
	BackendStatus.SetHttpEnabled(false);
	BackendStatus.SetCacheEnabled(Cache.IsValid());

#if UE_IAS_DEBUG_CONSOLE_CMDS
	DynamicConsoleCommands.Emplace(
	IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("ias.InvokeHttpFailure"),
		TEXT("Marks the current ias http connections as failed and forcing them  to try to reconnect"),
		FConsoleCommandDelegate::CreateLambda([this]()
			{
				UE_LOG(LogIas, Display, TEXT("User invoked http error via 'ias.InvokeHttpFailure'"));
				FHostGroupManager::Get().DisconnectAll();
			}),
		ECVF_Default)
	);

	DynamicConsoleCommands.Emplace(
		IConsoleManager::Get().RegisterConsoleCommand(
			TEXT("ias.AbandonCache"),
			TEXT("Simulate code calling FOnDemandIoBackend::AbandonCache()"),
			FConsoleCommandDelegate::CreateLambda([this]()
				{
					UE_LOG(LogIas, Display, TEXT("User invoked a cache abandon via 'ias.AbandonCache' (disable the IAS cache)"));
					AbandonCache();
				}),
			ECVF_Default)
	);

	DynamicConsoleCommands.Emplace(
		IConsoleManager::Get().RegisterConsoleCommand(
			TEXT("ias.RunRequestTest"),
			TEXT("[optional Number of requests to make] Creates a number of requests for chunks that can be found in that IAS system"),
			FConsoleCommandWithArgsDelegate::CreateRaw(this, &FOnDemandIoBackend::RunRequestTest),
			ECVF_Default)
	);

#endif // UE_IAS_DEBUG_CONSOLE_CMDS
}

FOnDemandIoBackend::~FOnDemandIoBackend()
{
#if UE_IAS_DEBUG_CONSOLE_CMDS
	for (IConsoleCommand* Cmd : DynamicConsoleCommands)
	{
		IConsoleManager::Get().UnregisterConsoleObject(Cmd);
	}
#endif // UE_IAS_DEBUG_CONSOLE_CMDS

	Shutdown();
}

void FOnDemandIoBackend::Initialize(TSharedRef<const FIoDispatcherBackendContext> Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(IasBackend::Initialize);
	LLM_SCOPE_BYTAG(Ias);
	UE_LOG(LogIas, Log, TEXT("Initializing on demand I/O dispatcher backend"));
	BackendContext = Context;
	BeginCreateDefaultHostGroup();
	OnThreadTickIdleHandle = HttpClient.OnTickIdle().AddRaw(this, &FOnDemandIoBackend::OnThreadTickIdle);
}

void FOnDemandIoBackend::Shutdown()
{
	if (bStopRequested)
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(IasBackend::Shutdown);

	UE_LOG(LogIas, Log, TEXT("Shutting down on demand I/O dispatcher backend"));

	if (OnThreadTickIdleHandle.IsValid())
	{
		HttpClient.OnTickIdle().Remove(OnThreadTickIdleHandle);
		OnThreadTickIdleHandle.Reset();
	}

	// The CompleteRequest tasks may still be executing a while after the IoDispatcher has been notified about the completed io requests.
	const int32 NumPending = WaitForCompleteRequestTasks(5.0f, 0.1f);
	UE_CLOG(NumPending > 0, LogIas, Warning, TEXT("%d request(s) still pending after shutdown"), NumPending);

	BackendContext.Reset();
}

void FOnDemandIoBackend::OnThreadTickIdle()
{
	if (BackendStatus.ShouldAbandonCache())
	{
		BackendStatus.SetAbandonCache(false);
		check(BackendStatus.IsCacheEnabled() == false);
		if (Cache.IsValid())
		{
			UE_LOG(LogIas, Log, TEXT("Abandoning cache, local file cache is no longer available"));
			Cache.Release()->Abandon(); // Will delete its self
		}
	}
}

void FOnDemandIoBackend::BeginCreateDefaultHostGroup()
{
	UE::Tasks::Launch(UE_SOURCE_LOCATION, [this]()
		{
			if (CreateDefaultHostGroup())
			{
				BackendStatus.SetHttpEnabled(true);
			}
		});
}

void FOnDemandIoBackend::IssueHttpRequest(FChunkRequest* ChunkRequest)
{
	ChunkRequest->HttpRequest = HttpClient.IssueRequest(ChunkRequest->Params.ChunkInfo, ChunkRequest->Params.ChunkRange, ChunkRequest->Priority,
		[this, ChunkRequest](uint32 StatusCode, FStringView ErrorReason, FIoBuffer&& Data)
		{
			ChunkRequest->Chunk = MoveTemp(Data);

			if (IsHttpStatusOk(StatusCode))
			{
				UE::Tasks::Launch(UE_SOURCE_LOCATION, [this, ChunkRequest]()
					{
						CompleteRequest(ChunkRequest);
					}, GetRequestCompletionTaskPriority());
			}
			else
			{
				CompleteRequest(ChunkRequest);
			}
		}, EHttpRequestType::Streaming);

	check(ChunkRequest->HttpRequest != nullptr);
}

void FOnDemandIoBackend::CompleteRequest(FChunkRequest* ChunkRequest)
{
	ChunkRequest->LaneScope = FLaneTraceScope();

	LLM_SCOPE_BYTAG(Ias);
	TRACE_CPUPROFILER_EVENT_SCOPE(IasBackend::CompleteRequest);
	check(ChunkRequest != nullptr);

	if (ChunkRequest->bCancelled)
	{
		check(ChunkRequest->RequestHead == nullptr);
		check(ChunkRequest->RequestTail == nullptr);
		return ChunkRequests.Release(ChunkRequest);
	}

	ChunkRequests.Remove(ChunkRequest);
	
	FIoBuffer Chunk = MoveTemp(ChunkRequest->Chunk);
	FIoChunkDecodingParams DecodingParams = ChunkRequest->Params.GetDecodingParams();

	// Only cache chunks if HTTP streaming is enabled
	bool bCacheChunk = ChunkRequest->bCached == false && Chunk.GetSize() > 0;
	FIoRequestImpl* NextRequest = ChunkRequest->DeqeueDispatcherRequests();
	while (NextRequest)
	{
		FIoRequestImpl* Request = NextRequest;
		NextRequest = Request->NextRequest;
		Request->NextRequest = nullptr;

		bool bDecoded = false;
		if (Chunk.GetSize() > 0)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(IasBackend::DecodeBlocks);
			const uint64 RawSize = FMath::Min<uint64>(Request->Options.GetSize(), ChunkRequest->Params.ChunkInfo.RawSize());
			Request->CreateBuffer(RawSize);
			DecodingParams.RawOffset = Request->Options.GetOffset();

			const EIoDecodeFlags Options = GIasEnableWriteOnlyDecoding && EnumHasAnyFlags(Request->Options.GetFlags(), EIoReadOptionsFlags::HardwareTargetBuffer) ? EIoDecodeFlags::WriteOnly : EIoDecodeFlags::None;

			bDecoded = FIoChunkEncoding::Decode(DecodingParams, Chunk.GetView(), Request->GetBuffer().GetMutableView(), Options);

			if (!bDecoded)
			{
				if (ChunkRequest->bCached)
				{
					Stats.OnCacheDecodeError();

					check(Cache.IsValid());
					Cache->Evict(ChunkRequest->Params.ChunkKey);
				}
				else
				{
					// Currently not being cached implies that the request was made via http
					Stats.OnHttpDecodeError(EHttpRequestType::Streaming);
				}
			}
		}
		
		const uint64 DurationMs = Request->GetStartTime() > 0 ?
			(uint64)FPlatformTime::ToMilliseconds64(FPlatformTime::Cycles64() - Request->GetStartTime()) : 0;

		if (bDecoded)
		{
			Stats.OnIoRequestComplete(Request->GetBuffer().GetSize(), DurationMs);
			LogIoResult(Request->ChunkId, ChunkRequest->Params.GetUrlHash(), DurationMs,
				Request->GetBuffer().DataSize(), Request->Options.GetOffset(),
				ChunkRequest->Params.ChunkRange, ChunkRequest->Params.ChunkInfo.EncodedSize(),
				ChunkRequest->Priority, ChunkRequest->bCached);
			TRACE_IOSTORE_BACKEND_REQUEST_COMPLETED(Request, Request->GetBuffer().GetSize());
		}
		else
		{
			bCacheChunk = false;
			Request->SetFailed();

			Stats.OnIoRequestError();
			LogIoResult(Request->ChunkId, ChunkRequest->Params.GetUrlHash(), DurationMs,
				0, Request->Options.GetOffset(),
				ChunkRequest->Params.ChunkRange, ChunkRequest->Params.ChunkInfo.EncodedSize(),
				ChunkRequest->Priority, ChunkRequest->bCached);
			TRACE_IOSTORE_BACKEND_REQUEST_FAILED(Request);
		}

		CompletedRequests.Enqueue(Request);
		BackendContext->WakeUpDispatcherThreadDelegate.Execute();
	}

	if (bCacheChunk && BackendStatus.IsCacheWriteable())
	{
		Cache->Put(ChunkRequest->Params.ChunkKey, Chunk);
	}

	ChunkRequests.Release(ChunkRequest);
}

void FOnDemandIoBackend::CompleteCacheRead(FChunkRequest* ChunkRequest)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(IasBackend::CompleteCacheRead);

	bool bWasCancelled = false;
	switch (ChunkRequest->CacheGetStatus)
	{
	case EIoErrorCode::Ok:
		check(ChunkRequest->Chunk.GetData() != nullptr);
#if !UE_BUILD_SHIPPING
		if (GIasPoisonCache)
		{
			FIoBuffer& Data = ChunkRequest->Chunk;
			for (uint64 i = Data.GetSize(); i--;)
			{
				Data.GetData()[i] = 0x4d;
			}
		}
#endif
		ChunkRequest->bCached = true;
		CompleteRequest(ChunkRequest);
		return;

	case EIoErrorCode::ReadError:
		Stats.OnCacheError();
		break;

	case EIoErrorCode::Cancelled:
		bWasCancelled = true;
		break;

	case EIoErrorCode::NotFound:
		break;
	}

	if (bWasCancelled || (!BackendStatus.IsHttpEnabled() || !ChunkRequest->HostGroup.IsConnected()))
	{
		UE_CLOG(!bWasCancelled, LogIas, Log, TEXT("Chunk was not found in the cache and HTTP is disabled"));
		CompleteRequest(ChunkRequest);
		return;
	}

	static uint32 ScopeId = LaneTrace_NewScope("Iax/HttpGetAgain");
	ChunkRequest->LaneScope.Change(ScopeId);

	IssueHttpRequest(ChunkRequest);
}

bool FOnDemandIoBackend::Resolve(FIoRequestImpl* Request)
{
	using namespace UE::Tasks;

	FOnDemandChunkInfo ChunkInfo = IoStore.GetStreamingChunkInfo(Request->ChunkId);
	if (!ChunkInfo.IsValid())
	{
		return false;
	}

	FChunkRequestParams RequestParams = FChunkRequestParams::Create(Request, ChunkInfo);

	if (BackendStatus.IsHttpEnabled(Request->ChunkId.GetChunkType()) == false || ChunkInfo.HostGroup().IsConnected() == false)
	{ 
		// If the cache is not readonly the chunk may get evicted before the request is completed
		if (BackendStatus.IsCacheReadOnly() == false || Cache->ContainsChunk(RequestParams.ChunkKey) == false)
		{
			return false;
		}
	}

	TRACE_IOSTORE_BACKEND_REQUEST_STARTED(Request, this);

	Stats.OnIoRequestEnqueue();
	bool bPending = false;
	bool bUpdatePriority = false;
	FChunkRequest* ChunkRequest = ChunkRequests.Create(Request, RequestParams, bPending, bUpdatePriority);

	if (bPending)
	{
		if (bUpdatePriority)
		{
			HttpClient.ReprioritizeRequest(ChunkRequest->HttpRequest, ChunkRequest->Priority);
		}
		// The chunk for the request is already inflight 
		return true;
	}

	if (Cache.IsValid())
	{
		const FIoHash& Key = ChunkRequest->Params.ChunkKey;
		FIoBuffer& Buffer = ChunkRequest->Chunk;

		//TODO: Pass priority to cache
		EIoErrorCode GetStatus = Cache->Get(Key, Buffer);

		if (GetStatus == EIoErrorCode::Ok)
		{
			check(Buffer.GetData() != nullptr);
			ChunkRequest->bCached = true;

			Launch(UE_SOURCE_LOCATION, [this, ChunkRequest] {
				CompleteRequest(ChunkRequest);
			}, GetRequestCompletionTaskPriority());
			return true;
		}

		if (GetStatus == EIoErrorCode::FileNotOpen)
		{
			static uint32 ScopeId = LaneTrace_NewScope("Iax/CacheRead");
			FLaneTrace* Lane = LaneEstate_Lookup(GRequestLaneEstate, ChunkRequest);
			ChunkRequest->LaneScope = FLaneTraceScope(Lane, ScopeId);

			InflightCacheRequestCount.fetch_add(1, std::memory_order_relaxed);

			FTaskEvent OnReadyEvent(TEXT("IasCacheMaterializeDone"));

			Launch(UE_SOURCE_LOCATION, [this, ChunkRequest] {
				InflightCacheRequestCount.fetch_sub(1, std::memory_order_relaxed);
				CompleteCacheRead(ChunkRequest);
			}, OnReadyEvent, GetRequestCompletionTaskPriority());

			EIoErrorCode& OutStatus = ChunkRequest->CacheGetStatus;
			Cache->Materialize(Key, Buffer, OutStatus, MoveTemp(OnReadyEvent));
			return true;
		}

		check(GetStatus == EIoErrorCode::NotFound);
	}

	ChunkRequest->HostGroup = ChunkInfo.HostGroup();

	FLaneTrace* Lane = LaneEstate_Lookup(GRequestLaneEstate, ChunkRequest);
	static uint32 ScopeId = LaneTrace_NewScope("Iax/HttpGet");
	ChunkRequest->LaneScope = FLaneTraceScope(Lane, ScopeId);

	IssueHttpRequest(ChunkRequest);

	return true;
}

void FOnDemandIoBackend::ResolveIoRequests(FIoRequestList Requests, FIoRequestList& OutUnresolved)
{
	while (FIoRequestImpl* Request = Requests.PopHead())
	{
		if (Resolve(Request) == false)
		{
			OutUnresolved.AddTail(Request);
		}
	}
}

void FOnDemandIoBackend::CancelIoRequest(FIoRequestImpl* Request)
{
	if (ChunkRequests.Cancel(Request, HttpClient, Cache.Get()))
	{
		CompletedRequests.Enqueue(Request);
		BackendContext->WakeUpDispatcherThreadDelegate.Execute();
	}
}

void FOnDemandIoBackend::UpdatePriorityForIoRequest(FIoRequestImpl* Request)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(IasBackend::UpdatePriorityForIoRequest);
	if (FChunkRequest* ChunkRequest = ChunkRequests.TryUpdatePriority(Request))
	{
		HttpClient.ReprioritizeRequest(ChunkRequest->HttpRequest, ChunkRequest->Priority);
	}
}

bool FOnDemandIoBackend::DoesChunkExist(const FIoChunkId& ChunkId) const
{
	const TIoStatusOr<uint64> ChunkSize = GetSizeForChunk(ChunkId);
	return ChunkSize.IsOk();
}

bool FOnDemandIoBackend::DoesChunkExist(const FIoChunkId& ChunkId, const FIoOffsetAndLength& ChunkRange) const
{
	uint64 Unused = 0;
	const TIoStatusOr<uint64> ChunkSize = GetSizeForChunk(ChunkId, ChunkRange, Unused);
	return ChunkSize.IsOk();
}

TIoStatusOr<uint64> FOnDemandIoBackend::GetSizeForChunk(const FIoChunkId& ChunkId) const
{
	uint64 Unused = 0;
	const FIoOffsetAndLength ChunkRange(0, MAX_uint64);
	return GetSizeForChunk(ChunkId, ChunkRange, Unused);
}

TIoStatusOr<uint64> FOnDemandIoBackend::GetSizeForChunk(const FIoChunkId& ChunkId, const FIoOffsetAndLength& ChunkRange, uint64& OutAvailable) const
{
	OutAvailable = 0;

	const FOnDemandChunkInfo ChunkInfo = IoStore.GetStreamingChunkInfo(ChunkId);
	if (ChunkInfo.IsValid() == false)
	{
		return FIoStatus(EIoErrorCode::UnknownChunkID);
	}

	FIoOffsetAndLength RequestedRange(ChunkRange.GetOffset(), FMath::Min<uint64>(ChunkInfo.RawSize(), ChunkRange.GetLength()));
	OutAvailable = ChunkInfo.RawSize();

	if (!BackendStatus.IsHttpEnabled(ChunkId.GetChunkType()))
	{
		// If the cache is not readonly the chunk may get evicted before the request is resolved
		if (BackendStatus.IsCacheReadOnly() == false)
		{
			return FIoStatus(EIoErrorCode::UnknownChunkID);
		}

		check(Cache.IsValid());
		const FChunkRequestParams RequestParams = FChunkRequestParams::Create(RequestedRange, ChunkInfo);
		if (Cache->ContainsChunk(RequestParams.ChunkKey) == false)
		{
			return FIoStatus(EIoErrorCode::UnknownChunkID);
		}

		// Only the specified chunk range is available 
		OutAvailable = RequestedRange.GetLength();
	}

	return TIoStatusOr<uint64>(ChunkInfo.RawSize());
}

FIoRequestImpl* FOnDemandIoBackend::GetCompletedIoRequests()
{
	FIoRequestImpl* Requests = CompletedRequests.Dequeue();

	for (FIoRequestImpl* It = Requests; It != nullptr; It = It->NextRequest)
	{
		TUniquePtr<FBackendData> BackendData = FBackendData::Detach(It);
		check(It->BackendData == nullptr);
	}

	return Requests;
}

TIoStatusOr<FIoMappedRegion> FOnDemandIoBackend::OpenMapped(const FIoChunkId& ChunkId, const FIoReadOptions& Options)
{
	return FIoStatus::Unknown;
}

const TCHAR* FOnDemandIoBackend::GetName() const
{
	return TEXT("OnDemand");
}

bool FOnDemandIoBackend::CreateDefaultHostGroup()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(IasBackend::ResolveDefaultHostGroup);

	const double InitStart = FPlatformTime::Seconds();

	if (!ResolveDefaultHostGroup())
	{
		return false;
	}

	if (ConnectionTest(DefaultHostGroup.GetPrimaryHostUrl(), DefaultHostGroup.GetTestPath(), GIasHttpTimeOutMs))
	{
		Stats.OnHttpConnected();
	}
	else
	{
		DefaultHostGroup.Disconnect();
	}

	const double InitTime = FPlatformTime::Seconds() - InitStart;
	UE_LOG(LogIas, Display, TEXT("HostGroup init took %.3f seconds"), InitTime);

	return true;
}

bool FOnDemandIoBackend::ResolveDefaultHostGroup()
{
	if (DistributionUrl.IsValid())
	{
		DefaultUrls.Empty(); // We don't want any pre-existing urls if we are getting them from the distributed endpoint

		if (ResolveDistributedEndpoint(DistributionUrl, DefaultUrls) == false)
		{
			// ResolveDistributedEndpoint should spin forever until either a valid url is found or
			// we give up and use a predetermined fallback url. If this returned false then we didn't
			// have a fallback url but the current process is shutting down so we might as well just
			// exist the thread early.
			UE_LOG(LogIas, Error, TEXT("Failed to resolve CDN endpoints from distribution URL"));
			return false;
		}
	}

	if (DefaultUrls.IsEmpty())
	{
		UE_LOG(LogIas, Error, TEXT("HTTP streaming disabled, no valid urls"));
		return false;
	}

	// Sanitize the urls
	for (FString& Url : DefaultUrls)
	{
		Url.ReplaceInline(TEXT("https"), TEXT("http"));
		Url.ToLowerInline();
	}

	if (GIasHttpPrimaryEndpoint > 0)
	{
		// Rotate the list of urls so that the primary endpoint is the first element
		Algo::Rotate(DefaultUrls, GIasHttpPrimaryEndpoint);
	}

	FIoStatus HostResult = DefaultHostGroup.Resolve(DefaultUrls);
	DefaultUrls.Empty(); // No longer needed
	
	if (!HostResult.IsOk())
	{
		UE_LOG(LogIas, Error, TEXT("HTTP streaming disabled, could not create the default host group (%s)"), *HostResult.ToString());
		return false;
	}

	return true;
}

bool FOnDemandIoBackend::ResolveDistributedEndpoint(const FDistributedEndpointUrl& DistributedEndpointUrl, TArray<FString>& OutUrls)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(IasBackend::ResolveDistributedEndpoint);

	check(DistributedEndpointUrl.IsValid());

	// We need to resolve the end point in this method which occurs after the config system has initialized
	// rather than in ::Mount which can occur before that.
	// Without the config system initialized the http module will not work properly and we will always fail
	// to resolve and the OnDemand system will not recover.
	check(GConfig->IsReadyForUse());

	int32 NumAttempts = 0;

	while (!bStopRequested)
	{
		FDistributionEndpoints Resolver;
		FDistributionEndpoints::EResult Result = Resolver.ResolveEndpoints(DistributedEndpointUrl.EndpointUrl, OutUrls, *DistributedEndpointEvent.Get());
		if (Result == FDistributionEndpoints::EResult::Success)
		{
			Stats.OnHttpDistributedEndpointResolved();
			return true;
		}

		if (DistributedEndpointUrl.HasFallbackUrl() && ++NumAttempts == GDistributedEndpointAttemptCount)
		{
			FString FallbackUrl = DistributedEndpointUrl.FallbackUrl.Replace(TEXT("https"), TEXT("http"));
			UE_LOG(LogIas, Warning, TEXT("Failed to resolve the distributed endpoint %d times. Fallback CDN '%s' will be used instead"), GDistributedEndpointAttemptCount , *FallbackUrl);

			OutUrls.Emplace(MoveTemp(FallbackUrl));
			return true;
		}

		if (!bStopRequested)
		{
			const uint32 WaitTime = GDistributedEndpointRetryWaitTime >= 0 ? (static_cast<uint32>(GDistributedEndpointRetryWaitTime) * 1000) : MAX_uint32;
			DistributedEndpointEvent->Wait(WaitTime);
		}
	}

	return false;
}

void FOnDemandIoBackend::SetBulkOptionalEnabled(bool bEnabled)
{
	BackendStatus.SetHttpOptionalBulkEnabled(bEnabled);
}

void FOnDemandIoBackend::AbandonCache()
{
	BackendStatus.SetCacheEnabled(false);
	BackendStatus.SetAbandonCache(true);
}

void FOnDemandIoBackend::ReportAnalytics(TArray<FAnalyticsEventAttribute>& OutAnalyticsArray) const
{
	// If we got this far we know that IAS is enabled for the current process as it has a valid backend.
	// However just because IAS is enabled does not mean we have managed to make a valid connection yet.

	if (!GIasReportAnalyticsEnabled)
	{
		return;
	}

	Stats.ReportGeneralAnalytics(OutAnalyticsArray);

	if (BackendStatus.IsHttpEnabled())
	{
		Stats.ReportEndPointAnalytics(OutAnalyticsArray);
	}
}

TUniquePtr<IAnalyticsRecording> FOnDemandIoBackend::StartAnalyticsRecording() const
{
	if (GIasReportAnalyticsEnabled)
	{
		return Stats.StartAnalyticsRecording();
	}

	return TUniquePtr<IAnalyticsRecording>();
}

FOnDemandStreamingCacheUsage FOnDemandIoBackend::GetCacheUsage() const
{
	FOnDemandStreamingCacheUsage Usage;
	if (Cache.IsValid())
	{
		Cache->GetCacheUsage(Usage.TotalSize, Usage.MaxSize);
	}

	return Usage; 
}

int32 FOnDemandIoBackend::WaitForCompleteRequestTasks(float WaitTimeSeconds, float PollTimeSeconds)
{
	const double StartTime = FPlatformTime::Seconds();
	while (ChunkRequests.Num() > 0 && float(FPlatformTime::Seconds() - StartTime) < WaitTimeSeconds)
	{
		FPlatformProcess::SleepNoStats(PollTimeSeconds);
	}

	return ChunkRequests.Num();
}

#if UE_IAS_DEBUG_CONSOLE_CMDS
void FOnDemandIoBackend::RunRequestTest(const TArray<FString>& Args) const
{
	int32 NumToRequest = 1000;

	if (Args.Num() > 1)
	{
		UE_LOG(LogIas, Error, TEXT("Too many args for 'ias.RequestTestCount'"));
		return;
	}
	else if (Args.Num() == 1)
	{
		int32 ArgValue = TCString<TCHAR>::Atoi(*Args[0]);

		if (!UE::String::IsNumericOnlyDigits(Args[0]) || ArgValue <= 0)
		{
			UE_LOG(LogIas, Error, TEXT("Invalid arg for 'ias.RequestTestCount', value must be a postive integer"));
			return;
		}

		NumToRequest = ArgValue;
	}

	UE_LOG(LogIas, Display, TEXT("Running IAS Request Test with %d requests..."), NumToRequest);

	TArray<FIoChunkId> RequestIds = IoStore.DebugFindStreamingChunkIds(NumToRequest);

	FIoBatch IoBatch;
	for (const FIoChunkId& Id : RequestIds)
	{
		IoBatch.Read(Id, FIoReadOptions(), IoDispatcherPriority_Medium);
	}

	const double StartTime = FPlatformTime::Seconds();

	IoBatch.IssueWithCallback([StartTime]()
		{
			const double TotalTime = FPlatformTime::Seconds() - StartTime;
			UE_LOG(LogIas, Display, TEXT("IAS Request Test took %.3f(s)"), TotalTime);
		});
}
#endif // UE_IAS_DEBUG_CONSOLE_CMDS

TSharedPtr<IOnDemandIoDispatcherBackend> MakeOnDemandIoDispatcherBackend(
	const FOnDemandEndpointConfig& Config,
	FOnDemandIoStore& IoStore,
	FOnDemandHttpThread& HttpClient,
	TUniquePtr<IIasCache>&& Cache)
{
	return MakeShareable<IOnDemandIoDispatcherBackend>(
		new FOnDemandIoBackend(Config, IoStore, HttpClient, MoveTemp(Cache)));
}

///////////////////////////////////////////////////////////////////////////////
#if WITH_IOSTORE_ONDEMAND_TESTS

TEST_CASE("IoStore::OnDemand::Ias::Misc", "[IoStoreOnDemand][Ias]")
{
	SECTION("AddRemoveDispatcherRequest")
	{
		// We are not allowed to create FIoRequestImpl directly due to it's api, however we don't need the full functionality
		// in order to test FChunkRequest so we can fake it with a malloced version of the struct, mem set to zero.
		FIoRequestImpl* IoFirstRequest = (FIoRequestImpl*)FMemory::MallocZeroed(sizeof(FIoRequestImpl));
		FIoRequestImpl* IoSecondRequest = (FIoRequestImpl*)FMemory::MallocZeroed(sizeof(FIoRequestImpl));
		FIoRequestImpl* IoThirdRequest = (FIoRequestImpl*)FMemory::MallocZeroed(sizeof(FIoRequestImpl));

		FChunkRequestParams Params;
		FChunkRequest ChunkRequest(IoFirstRequest, Params);
		ChunkRequest.AddDispatcherRequest(IoSecondRequest);
		ChunkRequest.AddDispatcherRequest(IoThirdRequest);

		CHECK(ChunkRequest.RequestHead == IoFirstRequest);
		CHECK(ChunkRequest.RequestHead->NextRequest == IoSecondRequest);
		CHECK(ChunkRequest.RequestHead->NextRequest->NextRequest == IoThirdRequest);
		CHECK(ChunkRequest.RequestTail == IoThirdRequest);

		CHECK(ChunkRequest.RequestCount == 3);

		ChunkRequest.RemoveDispatcherRequest(IoThirdRequest);
		
		CHECK(ChunkRequest.RequestCount == 2);
		CHECK(ChunkRequest.RequestHead == IoFirstRequest);
		CHECK(ChunkRequest.RequestTail == IoSecondRequest);

		ChunkRequest.RemoveDispatcherRequest(IoSecondRequest);
		
		CHECK(ChunkRequest.RequestCount == 1);
		CHECK(ChunkRequest.RequestHead == IoFirstRequest);
		CHECK(ChunkRequest.RequestTail == IoFirstRequest);

		ChunkRequest.RemoveDispatcherRequest(IoFirstRequest);
		CHECK(ChunkRequest.RequestCount == 0);

		CHECK(ChunkRequest.RequestHead == nullptr);
		CHECK(ChunkRequest.RequestTail == nullptr);

		FMemory::Free(IoFirstRequest);
		FMemory::Free(IoSecondRequest);
		FMemory::Free(IoThirdRequest);
	}
}

struct FTestRequest
{
	FTestRequest() = default;
	explicit FTestRequest(int32 InValue)
		: Value(InValue)
	{ }
	explicit FTestRequest(int32 InValue, int32 InPriority)
		: Value(InValue)
		, Priority(InPriority)
	{ }

	int32			Value = 0;
	int32			Priority = 0;
	FTestRequest*	NextRequest = nullptr;
};

TEST_CASE("IoStore::OnDemand::Ias::BasicQueue", "[IoStoreOnDemand][Ias]")
{
	const int32 NumRequests = 10;
	FTestRequest Requests[NumRequests];

	for (int32 Index = 0; Index < NumRequests; ++Index)
	{
		Requests[Index].Value = Index;
	}

	SECTION("Basic")
	{
		TThreadSafeIntrusiveQueue<FTestRequest> Queue;

		for (int32 Index = 0; Index < NumRequests; ++Index)
		{
			Queue.Enqueue(&Requests[Index]);
		}

		REQUIRE(Queue.Num() == NumRequests);

		FTestRequest* ReturnedRequests = Queue.Dequeue();

		REQUIRE(ReturnedRequests != nullptr);
		REQUIRE(Queue.Num() == 0);

		for (int32 Index = 0; Index < NumRequests; ++Index)
		{
			REQUIRE(ReturnedRequests->Value == Index);
			ReturnedRequests = ReturnedRequests->NextRequest;
		}

		REQUIRE(ReturnedRequests == nullptr);

		// Try removing from the empty list
		FTestRequest* EmptyRequests = Queue.Dequeue();
		REQUIRE(EmptyRequests == nullptr);
		REQUIRE(Queue.Num() == 0);
	}

	SECTION("Advanced Dequeue")
	{
		TThreadSafeIntrusiveQueue<FTestRequest> Queue;

		for (int32 Index = 0; Index < NumRequests; ++Index)
		{
			Queue.Enqueue(&Requests[Index]);
		}

		int32 NumRemaining = NumRequests;

		// Remove a number of items from the list
		{
			const int32 NumToRemove = 3;
			const int32 ValueOffset = NumRequests - NumRemaining;

			FTestRequest* ReturnedRequests = Queue.Dequeue(NumToRemove);
			NumRemaining -= NumToRemove;

			REQUIRE(ReturnedRequests != nullptr);
			REQUIRE(Queue.Num() == NumRemaining);

			for (int32 Index = 0; Index < NumToRemove; ++Index)
			{
				REQUIRE(ReturnedRequests->Value == ValueOffset + Index);
				ReturnedRequests = ReturnedRequests->NextRequest;
			}

			REQUIRE(ReturnedRequests == nullptr);
		}

		// Remove one more item from the list
		{
			const int32 NumToRemove = 1;
			const int32 ValueOffset = NumRequests - NumRemaining;

			FTestRequest* ReturnedRequests = Queue.Dequeue(NumToRemove);
			NumRemaining -= NumToRemove;

			REQUIRE(ReturnedRequests != nullptr);
			REQUIRE(Queue.Num() == NumRemaining);

			for (int32 Index = 0; Index < NumToRemove; ++Index)
			{
				REQUIRE(ReturnedRequests->Value == ValueOffset + Index);
				ReturnedRequests = ReturnedRequests->NextRequest;
			}

			REQUIRE(ReturnedRequests == nullptr);
		}

		// Remove nothing from the list
		{
			const int32 OriginalSize = Queue.Num();
			FTestRequest* ReturnedRequests = Queue.Dequeue(0);
			REQUIRE(ReturnedRequests == nullptr);
			REQUIRE(Queue.Num() == OriginalSize);
		}

		// Remove invalid value from the list (should do nothing)
		{
			const int32 OriginalSize = Queue.Num();
			FTestRequest * ReturnedRequests = Queue.Dequeue(MIN_int32);
			REQUIRE(ReturnedRequests == nullptr);
			REQUIRE(Queue.Num() == OriginalSize);
		}

		// Now remove the remaining items in the list
		{
			const int32 ValueOffset = NumRequests - NumRemaining;

			FTestRequest* ReturnedRequests = Queue.Dequeue();
			REQUIRE(ReturnedRequests != nullptr);
			REQUIRE(Queue.Num() == 0);

			for (int32 Index = 0; Index < NumRemaining; ++Index)
			{
				REQUIRE(ReturnedRequests->Value == ValueOffset + Index);
				ReturnedRequests = ReturnedRequests->NextRequest;
			}

			REQUIRE(ReturnedRequests == nullptr);
		}

		// Try removing from the empty list
		FTestRequest* EmptyRequests = Queue.Dequeue();
		REQUIRE(EmptyRequests == nullptr);
		REQUIRE(Queue.Num() == 0);
	}

	SECTION("Precise Dequeue")
	{
		TThreadSafeIntrusiveQueue<FTestRequest> Queue;

		Queue.Enqueue(&Requests[0]);
		Queue.Enqueue(&Requests[1]);

		// Dequeue the exact number of items that we have in the list
		FTestRequest* ReturnedRequests = Queue.Dequeue(5);
		REQUIRE(ReturnedRequests != nullptr);
		REQUIRE(Queue.Num() == 0);

		for (int32 Index = 0; Index < 2; ++Index)
		{
			REQUIRE(ReturnedRequests->Value == Index);
			ReturnedRequests = ReturnedRequests->NextRequest;
		}

		REQUIRE(ReturnedRequests == nullptr);
	}

	SECTION("Greedy Dequeue")
	{
		TThreadSafeIntrusiveQueue<FTestRequest> Queue;

		Queue.Enqueue(&Requests[0]);
		Queue.Enqueue(&Requests[1]);

		// Attempt to dequeue more items than we have in the list
		FTestRequest* ReturnedRequests = Queue.Dequeue(5);
		REQUIRE(ReturnedRequests != nullptr);
		REQUIRE(Queue.Num() == 0);

		for (int32 Index = 0; Index < 2; ++Index)
		{
			REQUIRE(ReturnedRequests->Value == Index);
			ReturnedRequests = ReturnedRequests->NextRequest;
		}

		REQUIRE(ReturnedRequests == nullptr);
	}
}

TEST_CASE("IoStore::OnDemand::Ias::PriorityQueue", "[IoStoreOnDemand][Ias]")
{
	using FTestRequestAlloc = TSingleThreadedSlabAllocator<FTestRequest>;
	using FTestRequestQueue = TThreadSafeIntrusiveQueue<FTestRequest>;

	auto Dequeue = [](FTestRequestQueue& Queue) -> TArray<FTestRequest*>
	{
		TArray<FTestRequest*> Out; 
		FTestRequest* NextRequest = Queue.Dequeue();
		while (NextRequest != nullptr)
		{
			FTestRequest* Request	= NextRequest; 
			NextRequest				= Request->NextRequest;
			Request->NextRequest	= nullptr;
			Out.Add(Request);
		}
		return Out;
	};

	auto Destroy = [](TArray<FTestRequest*>& Requests, FTestRequestAlloc& Alloc)
	{
		for (FTestRequest* R : Requests)
		{
			Alloc.Destroy(R);
		}
		Requests.Empty();
	};

	SECTION("EnqueueDequeue")
	{
		// Arrange
		FTestRequestAlloc Alloc;
		FTestRequestQueue Queue;
		const int32 ExpectedCount = 100;

		// Act
		for (int32 Idx = 0; Idx < ExpectedCount; ++Idx)
		{
			Queue.Enqueue(Alloc.Construct(Idx));
		}

		TArray<FTestRequest*> Requests = Dequeue(Queue);

		// Assert
		CHECK(Requests.Num() == ExpectedCount);
		for (int32 Idx = 0; Idx < ExpectedCount; ++Idx)
		{
			CHECK(Requests[Idx]->Value == Idx);
			Alloc.Destroy(Requests[Idx]);
		}
	}

	SECTION("EnqueueByPriority")
	{
		// Arrange
		FTestRequestAlloc Alloc;
		FTestRequestQueue Queue;
		const int32 ExpectedCount = 100;

		// Act
		for (int32 Idx = 0; Idx < ExpectedCount; ++Idx)
		{
			Queue.EnqueueByPriority(Alloc.Construct(Idx, Idx));
		}

		TArray<FTestRequest*> Requests = Dequeue(Queue);

		// Assert
		CHECK(Requests.Num() == ExpectedCount);
		for (int32 Idx = 0; Idx < ExpectedCount; ++Idx)
		{
			CHECK(Requests[Idx]->Value == (ExpectedCount - Idx - 1));
			Alloc.Destroy(Requests[Idx]);
		}
	}

	SECTION("UpdateWithHighestPriority")
	{
		// Arrange
		FTestRequestAlloc Alloc;
		FTestRequestQueue Queue;

		// Act
		FTestRequest* Request = Alloc.Construct(1, 1);
		Queue.EnqueueByPriority(Request);
		Queue.EnqueueByPriority(Alloc.Construct(2, 2));
		Queue.EnqueueByPriority(Alloc.Construct(3, 3));
		Queue.Reprioritize(Request, 4);

		TArray<FTestRequest*> Requests = Dequeue(Queue);

		// Assert
		CHECK(Requests.Num() == 3); 
		CHECK(Requests[0]->Value == 1);
		CHECK(Requests[1]->Value == 3);
		CHECK(Requests[2]->Value == 2);
		Destroy(Requests, Alloc);
	}

	SECTION("UpdateWithHigherPriority")
	{
		// Arrange
		FTestRequestAlloc Alloc;
		FTestRequestQueue Queue;

		// Act
		FTestRequest* Request = Alloc.Construct(1, 1);
		Queue.EnqueueByPriority(Request);
		Queue.EnqueueByPriority(Alloc.Construct(2, 2));
		Queue.EnqueueByPriority(Alloc.Construct(3, 4));
		Queue.Reprioritize(Request, 3);

		TArray<FTestRequest*> Requests = Dequeue(Queue);

		// Assert
		CHECK(Requests.Num() == 3); 
		CHECK(Requests[0]->Value == 3);
		CHECK(Requests[1]->Value == 1);
		CHECK(Requests[2]->Value == 2);
		Destroy(Requests, Alloc);
	}

	SECTION("UpdateWithLowestPriority")
	{
		// Arrange
		FTestRequestAlloc Alloc;
		FTestRequestQueue Queue;

		// Act
		Queue.EnqueueByPriority(Alloc.Construct(1, 1));
		Queue.EnqueueByPriority(Alloc.Construct(2, 2));
		FTestRequest* Request = Alloc.Construct(3, 3);
		Queue.EnqueueByPriority(Request);
		Queue.Reprioritize(Request, 0);

		TArray<FTestRequest*> Requests = Dequeue(Queue);

		// Assert
		CHECK(Requests.Num() == 3); 
		CHECK(Requests[0]->Value == 2);
		CHECK(Requests[1]->Value == 1);
		CHECK(Requests[2]->Value == 3);
		Destroy(Requests, Alloc);
	}
}

#endif // WITH_IOSTORE_ONDEMAND_TESTS



#if UE_TRACE_ENABLED

////////////////////////////////////////////////////////////////////////////////
UE_TRACE_EVENT_BEGIN(Ias, ChunkRequest, NoSync)
	UE_TRACE_EVENT_FIELD(uint64, Timestamp)
	UE_TRACE_EVENT_FIELD(uint32, Offset)
	UE_TRACE_EVENT_FIELD(uint32, Length)
	UE_TRACE_EVENT_FIELD(uint64, Hash_A)
	UE_TRACE_EVENT_FIELD(uint64, Hash_B)
	UE_TRACE_EVENT_FIELD(uint32, Hash_C)
	UE_TRACE_EVENT_FIELD(uint64, Id_A)
	UE_TRACE_EVENT_FIELD(uint32, Id_B)
	UE_TRACE_EVENT_FIELD(bool, IsPiggyback)
UE_TRACE_EVENT_END()

////////////////////////////////////////////////////////////////////////////////
static void Trace(
	bool bIsPiggyback,
	const FIoChunkId& ChunkId,
	const FChunkRequestParams* Params)
{
	struct FChunkIdDecomp
	{
		uint64 A;
		uint32 B;
		uint32 Pad;
	};
	static_assert(sizeof(FChunkIdDecomp) - sizeof(FChunkIdDecomp::Pad) == sizeof(FIoChunkId));
	const auto& IdDecomp = *(FChunkIdDecomp*)(ChunkId.GetData());

	struct FIoHashDecomp
	{
		uint64 A;
		uint64 B;
		uint32 C;
		uint32 Pad;
	};
	static_assert(sizeof(FIoHashDecomp) - sizeof(FIoHashDecomp::Pad) == sizeof(FIoHash::ByteArray));
	const auto& HashDecomp = *(FIoHashDecomp*)(Params->ChunkInfo.Hash().GetBytes());

	UE_TRACE_LOG(Ias, ChunkRequest, HTTP::GetIaxTraceChannel())
		<< ChunkRequest.Timestamp(FPlatformTime::Cycles64())
		<< ChunkRequest.Offset(uint32(Params->ChunkRange.GetOffset()))
		<< ChunkRequest.Length(uint32(Params->ChunkRange.GetLength()))
		<< ChunkRequest.Hash_A(HashDecomp.A)
		<< ChunkRequest.Hash_B(HashDecomp.B)
		<< ChunkRequest.Hash_C(HashDecomp.C)
		<< ChunkRequest.Id_A(IdDecomp.A)
		<< ChunkRequest.Id_B(IdDecomp.B)
		<< ChunkRequest.IsPiggyback(bIsPiggyback);
}

#endif // UE_TRACE_ENABLED

} // namespace UE::IoStore

#undef UE_ALLOW_DISABLE_CANCELLING
#undef UE_ENABLE_IAS_TESTING
#undef UE_IAS_DEBUG_CONSOLE_CMDS
