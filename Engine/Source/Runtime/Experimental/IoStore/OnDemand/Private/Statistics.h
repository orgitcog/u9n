// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/LowLevelMemTracker.h"
#include "OnDemandBackendStatus.h"
#include "ProfilingDebugging/CountersTrace.h"
#include "ProfilingDebugging/CsvProfiler.h"

#if COUNTERSTRACE_ENABLED || CSV_PROFILER_STATS
#	define IAS_WITH_STATISTICS 1
#else
#	define IAS_WITH_STATISTICS 0
#endif

struct FAnalyticsEventAttribute;
enum class EIoErrorCode;

LLM_DECLARE_TAG(Ias);

namespace UE::IoStore
{

class IAnalyticsRecording;
enum class EHttpRequestType : uint8;
struct FAvailableEndPoints;

#if IAS_WITH_STATISTICS
#	define IAS_STATISTICS_IMPL(...) ;
#else
#	define IAS_STATISTICS_IMPL(...) { return __VA_ARGS__; }
#endif

////////////////////////////////////////////////////////////////////////////////
class FOnDemandIoBackendStats
{
public:
	static FOnDemandIoBackendStats* Get() IAS_STATISTICS_IMPL(nullptr)

	FOnDemandIoBackendStats(FBackendStatus& InBackendStatus) IAS_STATISTICS_IMPL()
	~FOnDemandIoBackendStats() IAS_STATISTICS_IMPL()

	/** Report analytics not directly associated with a specific endpoint */
	void ReportGeneralAnalytics(TArray<FAnalyticsEventAttribute>& OutAnalyticsArray) const IAS_STATISTICS_IMPL()
	/** Report analytics for the current endpoint */
	void ReportEndPointAnalytics(TArray<FAnalyticsEventAttribute>& OutAnalyticsArray) const IAS_STATISTICS_IMPL()

	TUniquePtr<IAnalyticsRecording> StartAnalyticsRecording() const IAS_STATISTICS_IMPL(TUniquePtr<IAnalyticsRecording>())

	void OnIoRequestEnqueue() IAS_STATISTICS_IMPL()
	void OnIoRequestComplete(uint64 Size, uint64 DurationMs) IAS_STATISTICS_IMPL()
	void OnIoRequestCancel() IAS_STATISTICS_IMPL()
	void OnIoRequestError() IAS_STATISTICS_IMPL()

	void OnCacheError() IAS_STATISTICS_IMPL()
	void OnCacheDecodeError() IAS_STATISTICS_IMPL()
	void OnCacheGet(uint64 DataSize) IAS_STATISTICS_IMPL()
	void OnCachePut() IAS_STATISTICS_IMPL()
	void OnCachePutExisting(uint64 DataSize) IAS_STATISTICS_IMPL()
	void OnCachePutReject(uint64 DataSize) IAS_STATISTICS_IMPL()
	void OnCachePendingBytes(uint64 TotalSize) IAS_STATISTICS_IMPL()
	void OnCachePersistedBytes(uint64 TotalSize) IAS_STATISTICS_IMPL()
	void OnCacheWriteBytes(uint64 WriteSize) IAS_STATISTICS_IMPL()
	void OnCacheSetMaxBytes(uint64 TotalSize) IAS_STATISTICS_IMPL()

	// Per session
	void OnHttpDistributedEndpointResolved() IAS_STATISTICS_IMPL()

	// Per hostgroup
	void OnHttpConnected() IAS_STATISTICS_IMPL()
	void OnHttpDisconnected() IAS_STATISTICS_IMPL()

	// From FOnDemandHttpThread
	void OnHttpEnqueue(EHttpRequestType Type) IAS_STATISTICS_IMPL()
	void OnHttpCancel(EHttpRequestType Type) IAS_STATISTICS_IMPL()
	void OnHttpDequeue(EHttpRequestType Type) IAS_STATISTICS_IMPL()
	void OnHttpGet(EHttpRequestType Type, uint64 SizeBytes, uint64 DurationMs) IAS_STATISTICS_IMPL()
	void OnHttpRetry(EHttpRequestType Type) IAS_STATISTICS_IMPL()
	void OnHttpError(EHttpRequestType Type) IAS_STATISTICS_IMPL()
	void OnHttpDecodeError(EHttpRequestType Type) IAS_STATISTICS_IMPL()
	void OnHttpCdnCacheReply(EHttpRequestType Type,int32 Reply) IAS_STATISTICS_IMPL()

private:

#if IAS_WITH_STATISTICS
	const FBackendStatus& BackendStatus;
#endif //IAS_WITH_STATISTICS
};

////////////////////////////////////////////////////////////////////////////////
class FOnDemandContentInstallerStats
{
public:
	static void OnRequestEnqueued() IAS_STATISTICS_IMPL()
	static void OnRequestCompleted(
		uint64 RequestedChunkCount,
		uint64 RequestedBytes,
		uint64 DownloadedChunkCount,
		uint64 DownloadedBytes,
		double CacheHitRatio,
		uint64 DurationCycles,
		EIoErrorCode ErrorCode) IAS_STATISTICS_IMPL()
	static void ReportAnalytics(TArray<FAnalyticsEventAttribute>& OutAnalyticsArray) IAS_STATISTICS_IMPL()
};

////////////////////////////////////////////////////////////////////////////////
class FOnDemandInstallCacheStats
{
public:
	static void OnStartupError(EIoErrorCode ErrorCode) IAS_STATISTICS_IMPL()
	static void OnFlush(EIoErrorCode ErrorCode, int64 ByteCount) IAS_STATISTICS_IMPL()
	static void OnJournalCommit(EIoErrorCode ErrorCode, int64 ByteCount) IAS_STATISTICS_IMPL()
	static void OnCasVerificationError(int32 RemoveChunks) IAS_STATISTICS_IMPL()
	static void OnPurge(
		EIoErrorCode ErrorCode,
		uint64 MaxCacheSize,
		uint64 NewCacheSize,
		uint64 BytesToPurge,
		uint64 PurgedBytes) IAS_STATISTICS_IMPL()
	static void OnDefrag(EIoErrorCode ErrorCode, uint64 FragmentedBytes) IAS_STATISTICS_IMPL()
	static void OnCacheUsage(
		uint64 MaxCacheSize,
		uint64 CacheSize,
		uint64 ReferencedBlockSize,
		uint64 ReferencedSize,
		uint64 FragmentedSize,
		int64 OldestBlockAccess) IAS_STATISTICS_IMPL()
	static void OnReadCompleted(EIoErrorCode ErrorCode) IAS_STATISTICS_IMPL()
};

#undef IAS_STATISTICS_IMPL

} // namespace UE::IoStore
