// Copyright Epic Games, Inc. All Rights Reserved.

#include "Statistics.h"

#include "Async/Mutex.h"
#include "Async/UniqueLock.h"
#include "Algo/MaxElement.h"
#include "AnalyticsEventAttribute.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "IO/IoStatus.h"
#include "IO/IoStoreOnDemand.h"
#include "IasHostGroup.h"
#include "Internationalization/Internationalization.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/CoreDelegates.h"
#include "OnDemandHttpThread.h"
#include "Templates/Requires.h"

LLM_DEFINE_TAG(Ias);

#if IAS_WITH_STATISTICS

#define UE_ENABLE_ANALYTICS_RECORDING 0
#define UE_ENABLE_ONSCREEN_STATISTICS !UE_BUILD_SHIPPING

namespace UE::IoStore
{

float GIasStatisticsLogInterval = 30.f;
static FAutoConsoleVariableRef CVar_StatisticsLogInterval(
	TEXT("ias.StatisticsLogInterval"),
	GIasStatisticsLogInterval,
	TEXT("Enables and sets interval for periodic logging of statistics"));

bool GIasReportHttpAnalyticsEnabled = true;
static FAutoConsoleVariableRef CVar_ReportHttpAnalytics(
	TEXT("ias.ReportHttpAnalytics"),
	GIasReportHttpAnalyticsEnabled,
	TEXT("Enables reporting statics on our http traffic to the analytics system"));

bool GIasReportCacheAnalyticsEnabled = true;
static FAutoConsoleVariableRef CVar_ReportCacheAnalytics(
	TEXT("ias.ReportCacheAnalytics"),
	GIasReportCacheAnalyticsEnabled,
	TEXT("Enables reporting statics on our file cache usage to the analytics system"));

bool GIadReportAnalyticsEnabled = true;
static FAutoConsoleVariableRef CVar_ReportIadAnalyticsEnabled(
	TEXT("iad.ReportAnalytics"),
	GIadReportAnalyticsEnabled,
	TEXT("Enables reporting analytics for individual asset downloads."));

#if UE_ENABLE_ONSCREEN_STATISTICS

bool GIasDisplayOnScreenStatistics = false;
static FAutoConsoleVariableRef CVar_DisplayOnScreenStatistics(
	TEXT("ias.DisplayOnScreenStatistics"),
	GIasDisplayOnScreenStatistics,
	TEXT("Enables display of Ias on screen statistics"));

#endif // UE_ENABLE_ONSCREEN_STATISTICS

////////////////////////////////////////////////////////////////////////////////
static int32 BytesToApproxMB(uint64 Bytes) { return int32(Bytes >> 20); }
static int32 BytesToApproxKB(uint64 Bytes) { return int32(Bytes >> 10); }

/**
 * Code taken from SummarizeTraceCommandlet.cpp pending discussion on moving it
 * somewhere for general use.
 * Currently not thread safe!
 */
class FIncrementalVariance
{
public:
	FIncrementalVariance()
		: Count(0)
		, Mean(0.0)
		, VarianceAccumulator(0.0)
	{

	}

	uint64 GetCount() const
	{
		return Count;
	}

	double GetMean() const
	{
		return Mean;
	}

	/**
	* Compute the variance given Welford's accumulator and the overall count
	*
	* @return The variance in sample units squared
	*/
	double GetVariance() const
	{
		double Result = 0.0;

		if (Count > 1)
		{
			// Welford's final step, dependent on sample count
			Result = VarianceAccumulator / double(Count - 1);
		}

		return Result;
	}

	/**
	* Compute the standard deviation given Welford's accumulator and the overall count
	*
	* @return The standard deviation in sample units
	*/
	double GetDeviation() const
	{
		double Result = 0.0;

		if (Count > 1)
		{
			// Welford's final step, dependent on sample count
			double DeviationSqrd = VarianceAccumulator / double(Count - 1);

			// stddev is sqrt of variance, to restore to units (vs. units squared)
			Result = sqrt(DeviationSqrd);
		}

		return Result;
	}

	/**
	* Perform an increment of work for Welford's variance, from which we can compute variation and standard deviation
	*
	* @param InSample	The new sample value to operate on
	*/
	void Increment(const double InSample)
	{
		Count++;
		const double OldMean = Mean;
		Mean += ((InSample - Mean) / double(Count));
		VarianceAccumulator += ((InSample - Mean) * (InSample - OldMean));
	}

	/**
	* Merge with another IncrementalVariance series in progress
	*
	* @param Other	The other variance incremented from another mutually exclusive population of analogous data.
	*/
	void Merge(const FIncrementalVariance& Other)
	{
		// empty other, nothing to do
		if (Other.Count == 0)
		{
			return;
		}

		// empty this, just copy other
		if (Count == 0)
		{
			Count = Other.Count;
			Mean = Other.Mean;
			VarianceAccumulator = Other.VarianceAccumulator;
			return;
		}

		const double TotalPopulation = static_cast<double>(Count + Other.Count);
		const double MeanDifference = Mean - Other.Mean;
		const double A = (double(Count - 1) * GetVariance()) + (double(Other.Count - 1) * Other.GetVariance());
		const double B = (MeanDifference) * (MeanDifference) * (double(Count) * double(Other.Count) / TotalPopulation);
		const double MergedVariance = (A + B) / (TotalPopulation - 1);

		const uint64 NewCount = Count + Other.Count;
		const double NewMean = ((Mean * double(Count)) + (Other.Mean * double(Other.Count))) / double(NewCount);
		const double NewVarianceAccumulator = MergedVariance * double(NewCount - 1);

		Count = NewCount;
		Mean = NewMean;
		VarianceAccumulator = NewVarianceAccumulator;
	}

	/**
	* Reset state back to initialized.
	*/
	void Reset()
	{
		Count = 0;
		Mean = 0.0;
		VarianceAccumulator = 0.0;
	}

private:
	uint64 Count;
	double Mean;
	double VarianceAccumulator;
};

class FDeltaTracking
{
public:
	int64 Get(FStringView Name, int64 Value)
	{
		if (int64* PrevValue = IntTotals.FindByHash(GetTypeHash(Name), Name))
		{
			const int64 Delta = Value - *PrevValue;
			*PrevValue = Value;

			return Delta;
		}
		else
		{
			IntTotals.Add(FString(Name), Value);
			return Value;
		}
	}

	uint32 Get(FStringView Name, uint32 Value)
	{
		return static_cast<uint32>(Get(Name, static_cast<int64>(Value)));
	}

	double Get(FStringView Name, double Value)
	{
		if (double* PrevValue = RealTotals.FindByHash(GetTypeHash(Name), Name))
		{
			const double Delta = Value - *PrevValue;
			*PrevValue = Value;

			return Delta;
		}
		else
		{
			RealTotals.Add(FString(Name), 0.0);
			return Value;
		}
	}

private:

	TMap<FString, int64> IntTotals;
	TMap<FString, double> RealTotals;

} static GDeltaTracking;

////////////////////////////////////////////////////////////////////////////////
// TRACE STATS

#if COUNTERSTRACE_ENABLED
	using FCounterInt		= FCountersTrace::FCounterInt;
	using FCounterAtomicInt = FCountersTrace::FCounterAtomicInt;
#else
	template <typename Type>
	struct TCounterInt
	{
		TCounterInt(...)  {}
		void Set(int64 i) { V = i; }
		void Add(int64 d) { V += d; }
		void Increment() { V++; }
		void Decrement() { --V; }
		int64 Get() const { return V;}
		Type V = 0;
	};
	using FCounterInt		= TCounterInt<int64>;
	using FCounterAtomicInt = TCounterInt<std::atomic<int64>>;
#endif

#define UE_IAX_COUNTER(Name, Type) Type G##Name[(uint8)EHttpRequestType::NUM_SOURCES] = {{TEXT(UE_STRINGIZE(Ias/Name)), TraceCounterDisplayHint_None}, {TEXT(UE_STRINGIZE(Iad/Name)), TraceCounterDisplayHint_None}};
#define UE_IAX_MEMORY_COUNTER(Name, Type) Type G##Name[(uint8)EHttpRequestType::NUM_SOURCES] = {{TEXT(UE_STRINGIZE(Ias/Name)), TraceCounterDisplayHint_None}, {TEXT(UE_STRINGIZE(Iad/Name)), TraceCounterDisplayHint_Memory}};
	
// iorequest stats
FCounterInt				GIoRequestCount(TEXT("Ias/IoRequestCount"), TraceCounterDisplayHint_None);
FCounterAtomicInt		GIoRequestReadCount(TEXT("Ias/IoRequestReadCount"), TraceCounterDisplayHint_None);
FCounterAtomicInt		GIoRequestReadBytes(TEXT("Ias/IoRequestReadBytes"), TraceCounterDisplayHint_Memory);
FCounterInt				GIoRequestCancelCount(TEXT("Ias/IoRequestCancelCount"), TraceCounterDisplayHint_None);
FCounterAtomicInt		GIoRequestErrorCount(TEXT("Ias/IoRequestErrorCount"), TraceCounterDisplayHint_None);
// cache stats
FCounterAtomicInt		GCacheErrorCount(TEXT("Ias/CacheErrorCount"), TraceCounterDisplayHint_None);
FCounterAtomicInt		GCacheDecodeErrorCount(TEXT("Ias/CacheDecodeErrorCount"), TraceCounterDisplayHint_None);
FCounterAtomicInt		GCacheGetCount(TEXT("Ias/CacheGetCount"), TraceCounterDisplayHint_None);
FCounterAtomicInt		GCachePutCount(TEXT("Ias/CachePutCount"), TraceCounterDisplayHint_None);
FCounterAtomicInt		GCachePutExistingCount(TEXT("Ias/CachePutExistingCount"), TraceCounterDisplayHint_None);
FCounterAtomicInt		GCachePutRejectCount(TEXT("Ias/CachePutRejectCount"), TraceCounterDisplayHint_None);
FCounterAtomicInt		GCacheCachedBytes(TEXT("Ias/CacheCachedBytes"), TraceCounterDisplayHint_Memory);
FCounterAtomicInt		GCacheWrittenBytes(TEXT("Ias/CacheWrittenBytes"), TraceCounterDisplayHint_Memory);
int64					GCacheMaxBytes = 0;
FCounterAtomicInt		GCachePendingBytes(TEXT("Ias/CachePendingBytes"), TraceCounterDisplayHint_Memory);
FCounterAtomicInt		GCacheReadBytes(TEXT("Ias/CacheReadBytes"), TraceCounterDisplayHint_Memory);
FCounterAtomicInt		GCacheRejectBytes(TEXT("Ias/CachePutRejectBytes"), TraceCounterDisplayHint_Memory);

// http stats
bool					GHttpDistributedEndpointResolved = false;
FCounterInt				GHttpConnectCount(TEXT("Ias/HttpConnectCount"), TraceCounterDisplayHint_None);
FCounterInt				GHttpDisconnectCount(TEXT("Ias/HttpDisconnectCount"), TraceCounterDisplayHint_None);
UE_IAX_COUNTER(HttpGetCount, FCounterInt);
UE_IAX_COUNTER(HttpErrorCount, FCounterInt);
UE_IAX_COUNTER(HttpDecodeErrorCount, FCounterAtomicInt);
UE_IAX_COUNTER(HttpRetryCount, FCounterInt);
UE_IAX_COUNTER(HttpCancelCount, FCounterInt);
UE_IAX_COUNTER(HttpPendingCount, FCounterAtomicInt);
UE_IAX_COUNTER(HttpInflightCount, FCounterInt);
UE_IAX_MEMORY_COUNTER(HttpDownloadedBytes, FCounterInt);
UE_IAX_COUNTER(HttpDurationMs, FCounterInt);
UE_IAX_COUNTER(HttpBandwidthMbps, FCounterInt);
double GHttpDurationMsAvg[(uint8)EHttpRequestType::NUM_SOURCES] = { 0.0 };
int32 GHttpDurationMsMax[(uint8)EHttpRequestType::NUM_SOURCES] = { 0 };
int64 GHttpDurationMsSum[(uint8)EHttpRequestType::NUM_SOURCES] = { 0 };

struct FHttpRecentHistoryStatistics
{
	static const int64 HistoryCount = 16;

	int64			Duration[HistoryCount] = {};
	int64			Bytes[HistoryCount] = {};
	int64			TotalDuration = 0;
	int64			MaxDuration = 0;
	int64			TotalBytes = 0;
	int64 			Index = 0;

	void OnGet(uint64 SizeBytes, uint64 DurationMs)
	{
		int64 OldDuration = Duration[Index];
		int64 NewDuration = (int64)DurationMs;

		TotalDuration -= OldDuration;
		TotalDuration += NewDuration;
		Duration[Index] = NewDuration;

		TotalBytes -= Bytes[Index];
		TotalBytes += SizeBytes;
		Bytes[Index] = SizeBytes;

		MaxDuration = FMath::Max(NewDuration, MaxDuration);

		Index = (Index + 1) % HistoryCount;
	}

	int64 GetBandwidthMbps() const
	{
		return (TotalBytes * 8) / (TotalDuration + 1) / 1000;
	}

	double GetAverage() const
	{
		return static_cast<double>(TotalDuration) / static_cast<double>(HistoryCount);
	}

	int64 GetMaxDuration() const
	{
		return MaxDuration;
	}
};

FHttpRecentHistoryStatistics GHttpHistory[(uint8)EHttpRequestType::NUM_SOURCES];

// Experimental Http Stats
static uint32			GHttpCdnCacheHit = 0;
static uint32			GHttpCdnCacheMiss = 0;
static uint32			GHttpCdnCacheUnknown = 0;

////////////////////////////////////////////////////////////////////////////////
// CSV STATS

#if CSV_PROFILER && !CSV_PROFILER_MINIMAL
	#define UE_IAX_CSV_DEFINE_STAT(StatName) FCsvDeclaredStat _GCsvStat_##StatName[(uint8)EHttpRequestType::NUM_SOURCES] = {{TEXT(#StatName),CSV_CATEGORY_INDEX(Ias)},{TEXT(#StatName), CSV_CATEGORY_INDEX(Iad)}}
	#define UE_IAX_CSV_CUSTOM_STAT_DEFINED(RequestType, StatName, Value, Op) FCsvProfiler::RecordCustomStat(_GCsvStat_##StatName[(uint8)RequestType].Name, _GCsvStat_##StatName[(uint8)RequestType].CategoryIndex, Value, Op);
#else
	#define UE_IAX_CSV_DEFINE_STAT(StatName)
	#define UE_IAX_CSV_CUSTOM_STAT_DEFINED(RequestType, StatName, Value, Op)
#endif

CSV_DEFINE_CATEGORY(Ias, true);
CSV_DEFINE_CATEGORY(Iad, true);
// iorequest per frame stats
CSV_DEFINE_STAT(Ias, FrameIoRequestCount);
CSV_DEFINE_STAT(Ias, FrameIoRequestReadCount);
CSV_DEFINE_STAT(Ias, FrameIoRequestReadMB);
CSV_DEFINE_STAT(Ias, FrameIoRequestCancelCount);
CSV_DEFINE_STAT(Ias, FrameIoRequestErrorCount);
// cache stat totals
CSV_DEFINE_STAT(Ias, CacheGetCount);
CSV_DEFINE_STAT(Ias, CacheErrorCount);
CSV_DEFINE_STAT(Ias, CachePutCount);
CSV_DEFINE_STAT(Ias, CachePutExistingCount);
CSV_DEFINE_STAT(Ias, CachePutRejectCount);
CSV_DEFINE_STAT(Ias, CacheCachedMB);
CSV_DEFINE_STAT(Ias, CacheWrittenMB);
CSV_DEFINE_STAT(Ias, CacheReadMB);
CSV_DEFINE_STAT(Ias, CacheRejectedMB);
// http stat totals
UE_IAX_CSV_DEFINE_STAT(HttpGetCount);
UE_IAX_CSV_DEFINE_STAT(HttpRetryCount);
UE_IAX_CSV_DEFINE_STAT(HttpCancelCount);
UE_IAX_CSV_DEFINE_STAT(HttpErrorCount);
UE_IAX_CSV_DEFINE_STAT(HttpPendingCount);
UE_IAX_CSV_DEFINE_STAT(HttpDownloadedMB);
UE_IAX_CSV_DEFINE_STAT(HttpBandwidthMpbs);
UE_IAX_CSV_DEFINE_STAT(HttpDurationMsAvg);
UE_IAX_CSV_DEFINE_STAT(HttpDurationMsMax);

////////////////////////////////////////////////////////////////////////////////
#if COUNTERSTRACE_ENABLED
struct FInstallerTraceCounters
{
	using FCounterFloat = FCountersTrace::FCounterFloat;

	void Lock()		{ Mutex.Lock(); }
	void Unlock()	{ Mutex.Unlock(); }
	static FMutex Mutex; 

	FInstallerTraceCounters()
		: InstallCount(TEXT("Iad/InstallCount"), TraceCounterDisplayHint_None)
		, InflightInstallCount(TEXT("Iad/InflightInstallCount"), TraceCounterDisplayHint_None)
		, DownloadedBytes(TEXT("Iad/DownloadedBytes"), TraceCounterDisplayHint_Memory)
		, AvgInstallDurationMs(TEXT("Iad/AvgInstallDurationMs"), TraceCounterDisplayHint_None)
		, AvgCacheHitRatio(TEXT("Iad/AvgCacheHitRatio"), TraceCounterDisplayHint_None)
	{ }

	FCounterInt				InstallCount;
	FCounterInt				InflightInstallCount;
	FCounterInt				DownloadedBytes; 
	FCounterFloat			AvgInstallDurationMs;
	FCounterFloat			AvgCacheHitRatio;
	FIncrementalVariance	InstallDurationMs;
	FIncrementalVariance	CacheHitRatio;
};
FMutex FInstallerTraceCounters::Mutex;
FInstallerTraceCounters InstallerTraceCounters;
#endif // COUNTERSTRACE_ENABLED

////////////////////////////////////////////////////////////////////////////////
struct FInstallerAnalytics
{
	void Lock()		{ Mutex.Lock(); }
	void Unlock()	{ Mutex.Unlock(); }
	static FMutex Mutex; 

	uint64 InstallCount = 0;
	uint64 InstallErrorCount = 0;
	uint64 DownloadedBytes = 0;
	uint64 TotalInstallDurationMs = 0;
	double TotalCacheHitRatio = 0;
};
FMutex FInstallerAnalytics::Mutex;
static FInstallerAnalytics InstallerAnalytics;

////////////////////////////////////////////////////////////////////////////////
struct FInstallCacheAnalytics
{
	void Lock()		{ Mutex.Lock(); }
	void Unlock()	{ Mutex.Unlock(); }
	static FMutex Mutex; 

	uint64 VerificationRemovedBlockCount = 0;
	uint64 FlushCount = 0;
	uint64 FlushErrorCount = 0;
	uint64 FlushedBytes = 0;
	uint64 PurgedBytes = 0;
	uint64 PurgeCount = 0;
	uint64 PurgeErrorCount = 0;
	uint64 FragmentedBytes = 0;
	uint64 DefragCount = 0;
	uint64 DefragErrorCount = 0;
	uint64 JournalCommitCount = 0;
	uint64 JournalCommitErrorCount = 0;
	uint64 ReadCount = 0;
	uint64 ReadErrorCount = 0;
	uint64 MaxCacheSize = 0;
	uint64 MaxCacheUsageSize = 0;
	uint64 MaxReferencedBlockSize = 0;
	uint64 MaxReferencedSize = 0;
	uint64 MaxFragmentedSize = 0;
	int64 OldestBlockAccess = FDateTime::MaxValue().GetTicks();
	uint32 StartupErrorCode = 0;
};
FMutex FInstallCacheAnalytics::Mutex;
static FInstallCacheAnalytics InstallCacheAnalytics;

////////////////////////////////////////////////////////////////////////////////
static FOnDemandIoBackendStats* GStatistics = nullptr;
static FDelegateHandle GStatisticsEndFrameDelegateHandle;
static FDelegateHandle GStatisticsOnScreenDelegateHandle;

FOnDemandIoBackendStats::FOnDemandIoBackendStats(FBackendStatus& InStatus)
	: BackendStatus(InStatus)
{
	static constexpr float OneOver1024 = 1.0f / 1024.0f;

	check(GStatistics == nullptr);
	GStatistics = this;
	GStatisticsEndFrameDelegateHandle = FCoreDelegates::OnEndFrame.AddLambda([this]()
	{
		// cache stat totals
		int32 CGetCount = int32(GCacheGetCount.Get());
		int32 CErrorCount = int32(GCacheErrorCount.Get());
		int32 CPutCount = int32(GCachePutCount.Get());
		int32 CPutExistingCount = int32(GCachePutExistingCount.Get());
		int32 CPutRejectCount = int32(GCachePutRejectCount.Get());
		float CCachedKiB = (float)GCacheCachedBytes.Get() * OneOver1024;
		float CWrittenKiB = (float)GCacheWrittenBytes.Get() * OneOver1024;
		float CReadKiB = (float)GCacheReadBytes.Get() * OneOver1024;
		float CRejectedKiB = (float)GCacheRejectBytes.Get() * OneOver1024;
		CSV_CUSTOM_STAT_DEFINED(CacheGetCount, CGetCount, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_DEFINED(CacheErrorCount, CErrorCount, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_DEFINED(CachePutCount, CPutCount, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_DEFINED(CachePutExistingCount, CPutExistingCount, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_DEFINED(CachePutRejectCount, CPutRejectCount, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_DEFINED(CacheCachedMB, CCachedKiB * OneOver1024, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_DEFINED(CacheWrittenMB, CWrittenKiB * OneOver1024, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_DEFINED(CacheReadMB, CReadKiB * OneOver1024, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_DEFINED(CacheRejectedMB, CRejectedKiB * OneOver1024, ECsvCustomStatOp::Set);
		
		// http stat totals
		auto HttpCsv = [](EHttpRequestType Type)
			{
				UE_IAX_CSV_CUSTOM_STAT_DEFINED(Type, HttpGetCount, (int32)GHttpGetCount[(uint8)Type].Get(), ECsvCustomStatOp::Set);
				UE_IAX_CSV_CUSTOM_STAT_DEFINED(Type, HttpCancelCount, (int32)GHttpCancelCount[(uint8)Type].Get(), ECsvCustomStatOp::Set);
				UE_IAX_CSV_CUSTOM_STAT_DEFINED(Type, HttpErrorCount, (int32)GHttpErrorCount[(uint8)Type].Get(), ECsvCustomStatOp::Set);
				UE_IAX_CSV_CUSTOM_STAT_DEFINED(Type, HttpPendingCount, (int32)GHttpPendingCount[(uint8)Type].Get(), ECsvCustomStatOp::Set);
				UE_IAX_CSV_CUSTOM_STAT_DEFINED(Type, HttpDownloadedMB, (float)GHttpDownloadedBytes[(uint8)Type].Get() * OneOver1024 * OneOver1024, ECsvCustomStatOp::Set);
				UE_IAX_CSV_CUSTOM_STAT_DEFINED(Type, HttpBandwidthMpbs, (int32)GHttpBandwidthMbps[(uint8)Type].Get(), ECsvCustomStatOp::Set);
				UE_IAX_CSV_CUSTOM_STAT_DEFINED(Type, HttpDurationMsAvg, (int32)GHttpDurationMsAvg[(uint8)Type], ECsvCustomStatOp::Set);
				UE_IAX_CSV_CUSTOM_STAT_DEFINED(Type, HttpDurationMsMax, GHttpDurationMsMax[(uint8)Type], ECsvCustomStatOp::Set);
			};

		HttpCsv(EHttpRequestType::Streaming);
		HttpCsv(EHttpRequestType::Installed);

		if (GIasStatisticsLogInterval > 0.f)
		{
			static double LastLogTime = 0.0;
			if (double Time = FPlatformTime::Seconds(); Time - LastLogTime > (double)GIasStatisticsLogInterval)
			{
				if (BackendStatus.IsCacheEnabled())
				{
					UE_LOG(LogIas, Log, TEXT("CacheStats: CachedKiB=%d, WrittenKiB=%d, ReadKiB=%d, RejectedKiB=%d, Get=%d, Error=%d, Put=%d, PutReject=%d, PutExisting=%d"),
						(int32)CCachedKiB, (int32)CWrittenKiB, (int32)CReadKiB, (int32)CRejectedKiB, CGetCount, CErrorCount, CPutCount, CPutRejectCount, CPutExistingCount);
				}
				else
				{
					UE_LOG(LogIas, Log, TEXT("CacheStats: Disabled"));
				}

				auto HttpLog = [](const TCHAR* Title, EHttpRequestType Type)
				{
						UE_LOG(LogIas, Log, TEXT("%s - HttpStats: DownloadedKiB=%d, Get=%d, Retry=%d, Cancel=%d, Error=%d, CurPending=%d, CurDurationMsAvg=%d, CurDurationMsMax=%d"),
							Title, 
							(int32)((float)GHttpDownloadedBytes[(uint8)Type].Get() * OneOver1024),
							(int32)GHttpGetCount[(uint8)Type].Get(),
							(int32)GHttpRetryCount[(uint8)Type].Get(),
							(int32)GHttpCancelCount[(uint8)Type].Get(),
							(int32)GHttpErrorCount[(uint8)Type].Get(),
							(int32)GHttpPendingCount[(uint8)Type].Get(),
							(int32)GHttpDurationMsAvg[(uint8)Type],
							GHttpDurationMsMax[(uint8)Type]);
				};

				HttpLog(TEXT("IAS"), EHttpRequestType::Streaming);
				HttpLog(TEXT("IAD"), EHttpRequestType::Installed);

#if COUNTERSTRACE_ENABLED
				{
					TUniqueLock Lock(InstallerTraceCounters);
					UE_LOG(LogIoStoreOnDemand, Log, TEXT("IadStats: InstallCount=%llu, Downloaded=%d KiB, AvgInstallDuration=%dms, AvgCacheHitRatio=%d%%"),
						InstallerTraceCounters.InstallCount.Get(),
						(int32(InstallerTraceCounters.DownloadedBytes.Get()) / 1024),
						int32(InstallerTraceCounters.AvgInstallDurationMs.Get()),
						int32(InstallerTraceCounters.CacheHitRatio.GetMean() * 100.0));
				}
#endif // COUNTERSTRACE_ENABLED
				LastLogTime = Time;
			}
		}
	});

#if UE_ENABLE_ONSCREEN_STATISTICS
	GStatisticsOnScreenDelegateHandle = FCoreDelegates::OnGetOnScreenMessages.AddLambda(
		[this] (FCoreDelegates::FSeverityMessageMap& Out)
		{
			if (!GIasDisplayOnScreenStatistics)
			{
				return;
			}

			{
				TStringBuilder<256> BackendStatusText;
				BackendStatusText << TEXT("IAS Backend Status: ");
				BackendStatus.ToString(BackendStatusText);

				Out.Add(FCoreDelegates::EOnScreenMessageSeverity::Info, FText::FromStringView(BackendStatusText.ToView()));
			}

			FHostGroupManager::Get().ForEachHostGroup([&Out](const FIASHostGroup& HostGroup)
			{
				FCoreDelegates::EOnScreenMessageSeverity Verbosity = FCoreDelegates::EOnScreenMessageSeverity::Info;
				TStringBuilder<256> Text;
				Text << TEXT("IAS HostGroup [") << HostGroup.GetName() << TEXT("] ");

				if (HostGroup.IsConnected())
				{
					Text << HostGroup.GetPrimaryHostUrl();
					Text << TEXT(" (") << HostGroup.GetPrimaryHostIndex() << TEXT("/") << HostGroup.GetHostUrls().Num() << TEXT(")");
				}
				else if (HostGroup.IsResolved())
				{
					Text << TEXT("Resolving...");
				}
				else
				{
					Text << TEXT("Disconnected");
					Verbosity = FCoreDelegates::EOnScreenMessageSeverity::Error;
				}

				Out.Add(Verbosity, FText::FromStringView(Text.ToView()));
			});

			{
				TStringBuilder<256> CachingText;
				CachingText << TEXT("IAS Cache Stats: ");
				if (BackendStatus.IsCacheEnabled())
				{
					CachingText << TEXT("Cached: ") << FText::AsMemory(GCacheCachedBytes.Get()).ToString();
					CachingText << TEXT(" | Rejected: ") << FText::AsMemory(GCacheRejectBytes.Get()).ToString();
					CachingText << TEXT(" | Read: ") << FText::AsMemory(GCacheReadBytes.Get()).ToString();
					CachingText << TEXT(" (") << GCacheGetCount.Get() << TEXT(")");
				}
				else
				{
					CachingText << TEXTVIEW("Caching Disabled");
				}

				Out.Add(FCoreDelegates::EOnScreenMessageSeverity::Info, FText::FromStringView(CachingText.ToView()));
			}

			{
				auto HttpStats = [&Out](const TCHAR* Title, EHttpRequestType Type)
				{
					TStringBuilder<256> Builder;
					Builder.Appendf(
						TEXT("%s Backend Stats: Downloaded: %s (%d) Avg %d ms | Retries: %d | Pending: %d"),
						Title,
						*FText::AsMemory(GHttpDownloadedBytes[(uint8)Type].Get()).ToString(),
						GHttpGetCount[(uint8)Type].Get(),
						(int32)GHttpDurationMsAvg[(uint8)Type],
						GHttpRetryCount[(uint8)Type].Get(),
						GHttpPendingCount[(uint8)Type].Get()
					);

					Out.Add(FCoreDelegates::EOnScreenMessageSeverity::Info, FText::FromStringView(Builder.ToView()));
				};

				HttpStats(TEXT("IAS"), EHttpRequestType::Streaming);
				HttpStats(TEXT("IAD"), EHttpRequestType::Installed);
			}

			{
				TStringBuilder<256> Builder;
				Builder << TEXT("IAS CDN: Hit/Miss/NoHdr: ");

				Builder << GHttpCdnCacheHit;
				Builder << TEXT("/") << GHttpCdnCacheMiss;
				Builder << TEXT("/") << GHttpCdnCacheUnknown;

				if (uint32 Total = GHttpCdnCacheHit + GHttpCdnCacheMiss + GHttpCdnCacheUnknown; Total)
				{
					auto AsPercent = [Total] (uint32 Value) { return (Value * 100 + (Total >> 1)) / Total; };
					Builder << TEXT(" - ") << AsPercent(GHttpCdnCacheHit);
					Builder << TEXT("%/") << AsPercent(GHttpCdnCacheMiss);
					Builder << TEXT("%/") << AsPercent(GHttpCdnCacheUnknown);
					Builder << TEXT("%");
				}

				Out.Add(FCoreDelegates::EOnScreenMessageSeverity::Info, FText::FromStringView(Builder.ToView()));
			}

			if (GHttpDecodeErrorCount[(uint8)EHttpRequestType::Streaming].Get() > 0 || GCacheDecodeErrorCount.Get() > 0 || GHttpErrorCount[(uint8)EHttpRequestType::Streaming].Get() > 0)
			{
				TStringBuilder<256> Builder;
				Builder.Appendf(TEXT("IAS Errors: Cache Decode: %d | Http Decode: %d | Http: %d"),
					GCacheDecodeErrorCount.Get(),
					GHttpDecodeErrorCount[(uint8)EHttpRequestType::Streaming].Get(),
					GHttpErrorCount[(uint8)EHttpRequestType::Streaming].Get()
				);

				Out.Add(FCoreDelegates::EOnScreenMessageSeverity::Error, FText::FromStringView(Builder.ToView()));
			}
		});
#endif // UE_ENABLE_ONSCREEN_STATISTICS
}

FOnDemandIoBackendStats::~FOnDemandIoBackendStats()
{
	FCoreDelegates::OnEndFrame.Remove(GStatisticsEndFrameDelegateHandle);
	FCoreDelegates::OnGetOnScreenMessages.Remove(GStatisticsOnScreenDelegateHandle);
	GStatistics = nullptr;
}

FOnDemandIoBackendStats* FOnDemandIoBackendStats::Get()
{
	return GStatistics;
}

void FOnDemandIoBackendStats::ReportGeneralAnalytics(TArray<FAnalyticsEventAttribute>& OutAnalyticsArray) const
{
	// Note that this analytics section is not optional, if we are reporting analytics then we report this section
	// first. This means we can use the values here to determine if an analytics payload contains ondemand data or
	// not since with the current system we are unable to specify our own analytics payload.

	AppendAnalyticsEventAttributeArray(OutAnalyticsArray
		, TEXT("IasHttpDistributedEndpointResolved"), GHttpDistributedEndpointResolved
		, TEXT("IasHttpHasEverConnected"), GHttpConnectCount.Get() > 0 // Report if the system has ever actually managed to make a connection
	);
}

void FOnDemandIoBackendStats::ReportEndPointAnalytics(TArray<FAnalyticsEventAttribute>& OutAnalyticsArray) const
{


	if (GIasReportHttpAnalyticsEnabled)
	{
		auto ReportHttpStats = [&OutAnalyticsArray](const TCHAR* Prefix, EHttpRequestType Type)
		{
#define UE_PREFIX(Name) WriteToString<128>(Prefix, Name)
#define UE_TRACK_DELTA(Name, Value) *Name, GDeltaTracking.Get(Name, Value)

			const int64 ByteCount = GDeltaTracking.Get(UE_PREFIX(TEXT("HttpDownloadedBytes")), GHttpDownloadedBytes[(uint8)Type].Get());
			const int64 GetCount = GDeltaTracking.Get(UE_PREFIX(TEXT("HttpGetCount")), GHttpGetCount[(uint8)Type].Get());

			const double DataRateBPS = GHttpDurationMsSum[(uint8)Type] > 0 ? static_cast<double>(ByteCount) / (static_cast<double>(GHttpDurationMsSum[(uint8)Type]) / 1000.0) : 0.0;
			const double DurationMean = GetCount ? static_cast<double>(GHttpDurationMsSum[(uint8)Type]) / static_cast<double>(GetCount) : 0.0;

			AppendAnalyticsEventAttributeArray(OutAnalyticsArray
				, UE_TRACK_DELTA(UE_PREFIX("HttpErrorCount"), GHttpErrorCount[(uint8)Type].Get())
				, UE_TRACK_DELTA(UE_PREFIX("HttpDecodeErrors"), GHttpDecodeErrorCount[(uint8)Type].Get())
				, UE_TRACK_DELTA(UE_PREFIX("HttpRetryCount"), GHttpRetryCount[(uint8)Type].Get())
				, *UE_PREFIX(TEXT("HttpGetCount")), GetCount
				, *UE_PREFIX(TEXT("HttpDownloadedBytes")), ByteCount
				, *UE_PREFIX(TEXT("HttpDurationMean")), DurationMean
				, *UE_PREFIX(TEXT("HttpDurationSum")), GHttpDurationMsSum[(uint8)Type]
				, *UE_PREFIX(TEXT("HttpDataRateMean")), DataRateBPS
			);

			// These values we can just reset as they are only being used with analytics
			GHttpDurationMsSum[(uint8)Type] = 0;
#undef UE_TRACK_DELTA
#undef UE_PREFIX
		};

		ReportHttpStats(TEXT("Ias"), EHttpRequestType::Streaming);
		ReportHttpStats(TEXT("Iad"), EHttpRequestType::Installed);
	}

	if (GIasReportCacheAnalyticsEnabled)
	{
#define UE_TRACK_DELTA(Name, Value) TEXT(Name), GDeltaTracking.Get(TEXTVIEW(Name), Value)
		const int64 CacheTotalCount = GCacheGetCount.Get() + GCachePutCount.Get();
		const float CacheUsagePercent = GCacheMaxBytes > 0 ? 100.f * (float(GCacheCachedBytes.Get()) / float(GCacheMaxBytes)) : 0.f;

		AppendAnalyticsEventAttributeArray(OutAnalyticsArray

			, TEXT("IasCacheEnabled"), BackendStatus.IsCacheEnabled()

			, UE_TRACK_DELTA("IasCacheTotalCount", CacheTotalCount)
			, UE_TRACK_DELTA("IasCacheErrorCount", GCacheErrorCount.Get())
			, UE_TRACK_DELTA("IasCacheDecodeErrors", GCacheDecodeErrorCount.Get())
			, UE_TRACK_DELTA("IasCacheGetCount", GCacheGetCount.Get())
			, UE_TRACK_DELTA("IasCachePutCount", GCachePutCount.Get())

			, TEXT("IasCacheCachedBytes"), GCacheCachedBytes.Get()
			, TEXT("IasCacheMaxBytes"), GCacheMaxBytes
			, TEXT("IasCacheUsagePercent"), CacheUsagePercent

			, UE_TRACK_DELTA("IasCacheWriteBytes", GCacheWrittenBytes.Get())
			, UE_TRACK_DELTA("IasCacheReadBytes", GCacheReadBytes.Get())
			, UE_TRACK_DELTA("IasCacheRejectBytes", GCacheRejectBytes.Get())
		);
#undef UE_TRACK_DELTA
	}
}

#if UE_ENABLE_ANALYTICS_RECORDING

class FAnalyticsRecording : public IAnalyticsRecording
{
public:
	FAnalyticsRecording() = delete;

	FAnalyticsRecording(const FBackendStatus& InBackendStatus)
		: BackendStatus(InBackendStatus)
	{

	}

	virtual ~FAnalyticsRecording() = default;

private:

	void StopRecording() override
	{
		if (!bRecording)
		{
			return;
		}
	
		Http.ErrorCount.Stop();
		Http.DecodeErrorCount.Stop();
		Http.RetryCount.Stop();
		Http.GetCount.Stop();
		Http.DownloadedBytes.Stop();
		Http.TotalDuration.Stop();
		
		Cache.ErrorCount.Stop();
		Cache.DecodeErrorCount.Stop();
		Cache.GetCount.Stop();
		Cache.PutCount.Stop();

		Cache.WrittenBytes.Stop();
		Cache.ReadBytes.Stop();
		Cache.RejectBytes.Stop();

		bRecording = false;
	}

	void Report(TArray<FAnalyticsEventAttribute>& OutAnalyticsArray) const override
	{
		// Report if the system has ever actually managed to make a connection
		AppendAnalyticsEventAttributeArray(OutAnalyticsArray
			,TEXT("IasHttpDistributedEndpointResolved"), GHttpDistributedEndpointResolved
			,TEXT("IasHttpHasEverConnected"), GHttpConnectCount.Get() > 0
		);

		if (GIasReportHttpAnalyticsEnabled)
		{
			const double DataRateBPS = Http.TotalDuration.GetValue() > 0 ? static_cast<double>(Http.DownloadedBytes.GetValue()) / (static_cast<double>(Http.TotalDuration.GetValue()) / 1000.0) : 0.0;
			const double DurationMean = Http.GetCount.GetValue() ? static_cast<double>(Http.TotalDuration.GetValue()) / static_cast<double>(Http.GetCount.GetValue()) : 0.0;

			AppendAnalyticsEventAttributeArray(OutAnalyticsArray
				, TEXT("IasHttpErrorCount"), Http.ErrorCount.GetValue()
				, TEXT("IasHttpDecodeErrorCount"), Http.DecodeErrorCount.GetValue()
				, TEXT("IasHttpRetryCount"), Http.RetryCount.GetValue()
				, TEXT("IasHttpGetCount"), Http.GetCount.GetValue()
				, TEXT("IasHttpDownloadedBytes"), Http.DownloadedBytes.GetValue()
				, TEXT("IasHttpDurationMean"), DurationMean
				, TEXT("IasHttpDurationSum"), Http.TotalDuration.GetValue()
				, TEXT("IasHttpDataRateMean"), DataRateBPS
			);
		}

		if (GIasReportCacheAnalyticsEnabled)
		{
			const float CacheUsagePercent = GCacheMaxBytes > 0 ? 100.f * (float(GCacheCachedBytes.Get()) / float(GCacheMaxBytes)) : 0.f;

			AppendAnalyticsEventAttributeArray(OutAnalyticsArray

				, TEXT("IasCacheEnabled"), this->BackendStatus.IsCacheEnabled()
				, TEXT("IasCacheMaxBytes"), GCacheMaxBytes
				, TEXT("IasCacheCachedBytes"), GCacheCachedBytes.Get()
				, TEXT("IasCacheUsagePercent"), CacheUsagePercent

				, TEXT("IasCacheErrorCount"), Cache.ErrorCount.GetValue()
				, TEXT("IasCacheDecodeErrorCount"), Cache.DecodeErrorCount.GetValue()
				, TEXT("IasCacheGetCount"), Cache.GetCount.GetValue()
				, TEXT("IasCachePutCount"), Cache.PutCount.GetValue()

				, TEXT("IasCacheWriteBytes"), Cache.WrittenBytes.GetValue()
				, TEXT("IasCacheReadBytes"), Cache.ReadBytes.GetValue()
				, TEXT("IasCacheRejectBytes"), Cache.RejectBytes.GetValue()
			);
		}
	}

private:

	/** This wrapper class allows us to treat raw integer types and FCounter types  as the same thing */
	//template<typename CounterType, CounterType& Counter, int32 INDEX = -1>
	template<typename CounterType, const CounterType& Counter, int32 INDEX = -1>
	struct FTrackedValue
	{
		FTrackedValue()
		{
			Value = GetCurrentValue();
		}

		void Stop()
		{
			Value = GetCurrentValue() - Value;
			bRecording = false;
		}

		int64 GetValue() const
		{
			if (bRecording)
			{
				return GetCurrentValue() - Value;
			}
			else
			{
				return Value;
			}
		}

	private:

		template <
			typename U = CounterType
			UE_REQUIRES(!std::is_integral_v<U>)
		>
		int64 GetCurrentValue() const
		{
			return Counter.Get();
		}

		template <
			typename U = CounterType
			UE_REQUIRES(std::is_integral_v<U>)
		>
		int64 GetCurrentValue() const
		{
			return Counter;
		}

		int64 Value;
		bool bRecording = true;
	};

	/** Http Stats */
	struct FHttpStats
	{
		FTrackedValue<FCounterInt, GHttpErrorCount[0]>				ErrorCount;
		FTrackedValue<FCounterAtomicInt, GHttpDecodeErrorCount[0]>	DecodeErrorCount;
		FTrackedValue<FCounterInt, GHttpRetryCount[0]>				RetryCount;
		FTrackedValue<FCounterInt, GHttpGetCount[0]>				GetCount;
		FTrackedValue<FCounterInt, GHttpDownloadedBytes[0]>			DownloadedBytes;
		FTrackedValue<int64, GHttpDurationMsSum[0]>					TotalDuration;
		
	} Http;

	/** Cache Stats */
	struct FCacheStats
	{
		FTrackedValue<FCounterAtomicInt, GCacheErrorCount>			ErrorCount;
		FTrackedValue<FCounterAtomicInt, GCacheDecodeErrorCount>	DecodeErrorCount;
		FTrackedValue<FCounterAtomicInt, GCacheGetCount>			GetCount;
		FTrackedValue<FCounterAtomicInt, GCachePutCount>			PutCount;

		FTrackedValue<FCounterAtomicInt, GCacheWrittenBytes>		WrittenBytes;
		FTrackedValue<FCounterAtomicInt, GCacheReadBytes>			ReadBytes;
		FTrackedValue<FCounterAtomicInt, GCacheRejectBytes>			RejectBytes;
	} Cache;

	const FBackendStatus& BackendStatus;
	bool bRecording = true;
};

#endif //UE_ENABLE_ANALYTICS_RECORDING

TUniquePtr<IAnalyticsRecording> FOnDemandIoBackendStats::StartAnalyticsRecording() const
{
#if UE_ENABLE_ANALYTICS_RECORDING
	TUniquePtr<FAnalyticsRecording> Recording = MakeUnique<FAnalyticsRecording>(BackendStatus);
	return Recording;
#else
	return nullptr;
#endif // UE_ENABLE_ANALYTICS_RECORDING
}

void FOnDemandIoBackendStats::OnIoRequestEnqueue()
{
	GIoRequestCount.Increment();
	CSV_CUSTOM_STAT_DEFINED(FrameIoRequestCount, int32(GIoRequestCount.Get()), ECsvCustomStatOp::Set);
}

void FOnDemandIoBackendStats::OnIoRequestComplete(uint64 Size, uint64 Duration)
{
	GIoRequestReadCount.Increment();
	GIoRequestReadBytes.Add(Size);

	CSV_CUSTOM_STAT_DEFINED(FrameIoRequestReadCount, int32(GIoRequestReadCount.Get()), ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT_DEFINED(FrameIoRequestReadMB, BytesToApproxMB(GIoRequestReadBytes.Get()), ECsvCustomStatOp::Set);
}

void FOnDemandIoBackendStats::OnIoRequestCancel()
{
	GIoRequestCancelCount.Increment();
	CSV_CUSTOM_STAT_DEFINED(FrameIoRequestCancelCount, int32(GIoRequestCancelCount.Get()), ECsvCustomStatOp::Set);
}

void FOnDemandIoBackendStats::OnIoRequestError()
{
	GIoRequestErrorCount.Increment();
	CSV_CUSTOM_STAT_DEFINED(FrameIoRequestErrorCount, int32(GIoRequestErrorCount.Get()), ECsvCustomStatOp::Set);
}

void FOnDemandIoBackendStats::OnCacheError()
{
	GCacheErrorCount.Increment();
}

void FOnDemandIoBackendStats::OnCacheDecodeError()
{
	GCacheDecodeErrorCount.Increment();
}

void FOnDemandIoBackendStats::OnCacheGet(uint64 DataSize)
{
	GCacheGetCount.Increment();
	GCacheReadBytes.Add(DataSize);
}

void FOnDemandIoBackendStats::OnCachePut()
{
	GCachePutCount.Increment();
}

void FOnDemandIoBackendStats::OnCachePutExisting(uint64 /*DataSize*/)
{
	GCachePutExistingCount.Increment();
}

void FOnDemandIoBackendStats::OnCachePutReject(uint64 DataSize)
{
	GCachePutRejectCount.Increment();
	GCacheRejectBytes.Add(DataSize);
}

void FOnDemandIoBackendStats::OnCachePendingBytes(uint64 TotalSize)
{
	GCachePendingBytes.Set(TotalSize);
}

void FOnDemandIoBackendStats::OnCachePersistedBytes(uint64 TotalSize)
{
	GCacheCachedBytes.Set(TotalSize);
}

void FOnDemandIoBackendStats::OnCacheWriteBytes(uint64 WriteSize)
{
	GCacheWrittenBytes.Add(WriteSize);
}

void FOnDemandIoBackendStats::OnCacheSetMaxBytes(uint64 TotalSize)
{
	GCacheMaxBytes = TotalSize;
}

void FOnDemandIoBackendStats::OnHttpDistributedEndpointResolved()
{
	GHttpDistributedEndpointResolved = true;
}

void FOnDemandIoBackendStats::OnHttpConnected()
{
	GHttpConnectCount.Increment();
}

void FOnDemandIoBackendStats::OnHttpDisconnected()
{
	GHttpDisconnectCount.Increment();
}

void FOnDemandIoBackendStats::OnHttpEnqueue(EHttpRequestType Type)
{
	GHttpPendingCount[(uint8)Type].Increment();
}

void FOnDemandIoBackendStats::OnHttpDequeue(EHttpRequestType Type)
{
	GHttpInflightCount[(uint8)Type].Increment();
}

void FOnDemandIoBackendStats::OnHttpGet(EHttpRequestType Type, uint64 SizeBytes, uint64 DurationMs)
{
	GHttpPendingCount[(uint8)Type].Decrement();
	GHttpInflightCount[(uint8)Type].Decrement();
	GHttpGetCount[(uint8)Type].Increment();
	GHttpDownloadedBytes[(uint8)Type].Add(SizeBytes);
	GHttpDurationMsSum[(uint8)Type] += DurationMs;
	GHttpDurationMs[(uint8)Type].Set(DurationMs);

	GHttpHistory[(uint8)Type].OnGet(SizeBytes, DurationMs);

	GHttpBandwidthMbps[(uint8)Type].Set(GHttpHistory[(uint8)Type].GetBandwidthMbps());
	GHttpDurationMsAvg[(uint8)Type] = GHttpHistory[(uint8)Type].GetAverage();
	GHttpDurationMsMax[(uint8)Type] = (int32)GHttpHistory[(uint8)Type].GetMaxDuration();
}

void FOnDemandIoBackendStats::OnHttpCancel(EHttpRequestType Type)
{
	GHttpInflightCount[(uint8)Type].Decrement();
	GHttpPendingCount[(uint8)Type].Decrement();
	GHttpCancelCount[(uint8)Type].Increment();
}

void FOnDemandIoBackendStats::OnHttpRetry(EHttpRequestType Type)
{
	GHttpRetryCount[(uint8)Type].Increment();
}

void FOnDemandIoBackendStats::OnHttpError(EHttpRequestType Type)
{
	GHttpPendingCount[(uint8)Type].Decrement();
	GHttpInflightCount[(uint8)Type].Decrement();
	GHttpErrorCount[(uint8)Type].Increment();
}

void FOnDemandIoBackendStats::OnHttpDecodeError(EHttpRequestType Type)
{
	GHttpDecodeErrorCount[(uint8)Type].Increment();
}

void FOnDemandIoBackendStats::OnHttpCdnCacheReply(EHttpRequestType Type, int32 Reply)
{
	switch (Reply)
	{
	case -1:	GHttpCdnCacheUnknown += 1;	break;	
	case 0:		GHttpCdnCacheMiss += 1;		break;
	default:	GHttpCdnCacheHit += 1;		break;
	}
}

////////////////////////////////////////////////////////////////////////////////
void FOnDemandContentInstallerStats::OnRequestEnqueued()
{
#if COUNTERSTRACE_ENABLED
	TUniqueLock Lock(InstallerTraceCounters);
	InstallerTraceCounters.InflightInstallCount.Increment();
#endif
}

void FOnDemandContentInstallerStats::OnRequestCompleted(
		uint64 RequestedChunkCount,
		uint64 RequestedBytes,
		uint64 DownloadedChunkCount,
		uint64 DownloadedBytes,
		double CacheHitRatio,
		uint64 DurationCycles,
		EIoErrorCode ErrorCode) 
{
	if (RequestedChunkCount == 0)
	{
		return;
	}

#if COUNTERSTRACE_ENABLED
	{
		TUniqueLock Lock(InstallerTraceCounters);
		InstallerTraceCounters.InstallCount.Increment();
		InstallerTraceCounters.InflightInstallCount.Decrement();
		InstallerTraceCounters.DownloadedBytes.Add(DownloadedBytes);
		InstallerTraceCounters.InstallDurationMs.Increment(FPlatformTime::ToMilliseconds64(DurationCycles));
		InstallerTraceCounters.CacheHitRatio.Increment(CacheHitRatio);
		InstallerTraceCounters.AvgInstallDurationMs.Set(InstallerTraceCounters.InstallDurationMs.GetMean());
		InstallerTraceCounters.AvgCacheHitRatio.Set(InstallerTraceCounters.CacheHitRatio.GetMean());
	}
#endif // COUNTERSTRACE_ENABLED

	{
		TUniqueLock Lock(InstallerAnalytics);
		if (ErrorCode != EIoErrorCode::Ok && ErrorCode != EIoErrorCode::Cancelled)
		{
			InstallerAnalytics.InstallErrorCount++;
		}
		InstallerAnalytics.InstallCount++;
		InstallerAnalytics.DownloadedBytes += DownloadedBytes;
		InstallerAnalytics.TotalInstallDurationMs += uint64(FPlatformTime::ToMilliseconds64(DurationCycles));
		InstallerAnalytics.TotalCacheHitRatio += CacheHitRatio;
	}
}

void FOnDemandContentInstallerStats::ReportAnalytics(TArray<FAnalyticsEventAttribute>& OutAnalyticsArray)
{
	if (GIadReportAnalyticsEnabled == false)
	{
		return;
	}

	FInstallerAnalytics CurrentInstallerAnalytics;
	{
		TUniqueLock Lock(InstallerAnalytics);
		CurrentInstallerAnalytics	= InstallerAnalytics;
		InstallerAnalytics			= FInstallerAnalytics();
	}

	FInstallCacheAnalytics CurrentInstallCacheAnalytics;
	{
		TUniqueLock Lock(InstallCacheAnalytics);
		CurrentInstallCacheAnalytics	= InstallCacheAnalytics;
		InstallCacheAnalytics			= FInstallCacheAnalytics();
	}

	const FDateTime Now = FDateTime::UtcNow();
	const FDateTime OldestBlockAccess(CurrentInstallCacheAnalytics.OldestBlockAccess);
	const FTimespan OldestBlockAge = (Now >= OldestBlockAccess) ? (Now - OldestBlockAccess) : FTimespan::MaxValue();

	AppendAnalyticsEventAttributeArray(OutAnalyticsArray
		, TEXT("IadTotalInstallCount"), CurrentInstallerAnalytics.InstallCount
		, TEXT("IadTotalInstallErrorCount"), CurrentInstallerAnalytics.InstallErrorCount 
		, TEXT("IadTotalDownloadedBytes"), CurrentInstallerAnalytics.DownloadedBytes 
		, TEXT("IadTotalInstallDurationMs"), CurrentInstallerAnalytics.TotalInstallDurationMs 
		, TEXT("IadAvgInstallDurationMs"), (double(CurrentInstallerAnalytics.TotalInstallDurationMs) / double(CurrentInstallerAnalytics.InstallCount))
		, TEXT("IadAvgCacheHitRatio"), (CurrentInstallerAnalytics.TotalCacheHitRatio / double(CurrentInstallerAnalytics.InstallCount))
		, TEXT("IadInstallCacheStartupErrorCode"), CurrentInstallCacheAnalytics.StartupErrorCode
		, TEXT("IadInstallCacheVerificationRemovedBlockCount"), CurrentInstallCacheAnalytics.VerificationRemovedBlockCount 
		, TEXT("IadInstallCacheFlushCount"), CurrentInstallCacheAnalytics.FlushCount 
		, TEXT("IadInstallCacheFlushErrorCount"), CurrentInstallCacheAnalytics.FlushErrorCount 
		, TEXT("IadInstallCacheFlushedBytes"), CurrentInstallCacheAnalytics.FlushedBytes
		, TEXT("IadInstallCachePurgeCount"), CurrentInstallCacheAnalytics.PurgeCount
		, TEXT("IadInstallCachePurgeErrorCount"), CurrentInstallCacheAnalytics.PurgeErrorCount
		, TEXT("IadInstallCacheDefragCount"), CurrentInstallCacheAnalytics.DefragCount 
		, TEXT("IadInstallCacheDefragErrorCount"), CurrentInstallCacheAnalytics.DefragErrorCount 
		, TEXT("IadInstallCacheJournalCommitCount"), CurrentInstallCacheAnalytics.JournalCommitCount 
		, TEXT("IadInstallCacheJournalCommitErrorCount"), CurrentInstallCacheAnalytics.JournalCommitErrorCount
		, TEXT("IadInstallCacheMaxSize"), CurrentInstallCacheAnalytics.MaxCacheSize 
		, TEXT("IadInstallCacheMaxUsageSize"), CurrentInstallCacheAnalytics.MaxCacheUsageSize 
		, TEXT("IadInstallCacheMaxReferencedBlockSize"), CurrentInstallCacheAnalytics.MaxReferencedBlockSize
		, TEXT("IadInstallCacheMaxReferencedSize"), CurrentInstallCacheAnalytics.MaxReferencedSize
		, TEXT("IadInstallCacheMaxFragmentedSize"), CurrentInstallCacheAnalytics.MaxFragmentedSize
		, TEXT("IadInstallCacheOldestBlockAgeMinutes"), OldestBlockAge.GetTotalMinutes()
		, TEXT("IadInstallCacheReadCount"), CurrentInstallCacheAnalytics.ReadCount
		, TEXT("IadInstallCacheReadErrorCount"), CurrentInstallCacheAnalytics.ReadErrorCount
	);
}

////////////////////////////////////////////////////////////////////////////////
void FOnDemandInstallCacheStats::OnStartupError(EIoErrorCode ErrorCode)
{
	if (ErrorCode != EIoErrorCode::Ok)
	{
		UE::TUniqueLock Lock(InstallCacheAnalytics);
		InstallCacheAnalytics.StartupErrorCode = static_cast<uint32>(ErrorCode);
	}
}

void FOnDemandInstallCacheStats::OnFlush(EIoErrorCode ErrorCode, int64 ByteCount)
{
	if (ErrorCode == EIoErrorCode::Cancelled)
	{
		return;
	}

	UE::TUniqueLock Lock(InstallCacheAnalytics);
	if (ErrorCode != EIoErrorCode::Ok)
	{
		InstallCacheAnalytics.FlushErrorCount++;
	}
	InstallCacheAnalytics.FlushCount++;
	InstallCacheAnalytics.FlushedBytes += ByteCount;
}

void FOnDemandInstallCacheStats::OnJournalCommit(EIoErrorCode ErrorCode, int64 ByteCount)
{
	if (ErrorCode == EIoErrorCode::Cancelled)
	{
		return;
	}

	UE::TUniqueLock Lock(InstallCacheAnalytics);
	if (ErrorCode != EIoErrorCode::Ok)
	{
		InstallCacheAnalytics.JournalCommitErrorCount++;
	}
	InstallCacheAnalytics.JournalCommitCount++;
}

void FOnDemandInstallCacheStats::OnCasVerificationError(int32 RemoveChunks)
{
	UE::TUniqueLock Lock(InstallCacheAnalytics);
	InstallCacheAnalytics.VerificationRemovedBlockCount += RemoveChunks;
}

void FOnDemandInstallCacheStats::OnPurge(
	EIoErrorCode ErrorCode,
	uint64 MaxCacheSize,
	uint64 NewCacheSize,
	uint64 BytesToPurge,
	uint64 PurgedBytes)
{
	if (ErrorCode == EIoErrorCode::Cancelled)
	{
		return;
	}

	UE::TUniqueLock Lock(InstallCacheAnalytics);
	if (ErrorCode != EIoErrorCode::Ok)
	{
		InstallCacheAnalytics.PurgeErrorCount++;
	}
	InstallCacheAnalytics.PurgeCount++;
	InstallCacheAnalytics.PurgedBytes += PurgedBytes;
}

void FOnDemandInstallCacheStats::OnDefrag(EIoErrorCode ErrorCode, uint64 FragmentedBytes)
{
	if (ErrorCode == EIoErrorCode::Cancelled)
	{
		return;
	}

	UE::TUniqueLock Lock(InstallCacheAnalytics);
	if (ErrorCode != EIoErrorCode::Ok)
	{
		InstallCacheAnalytics.DefragErrorCount++;
	}
	InstallCacheAnalytics.DefragCount++;
	InstallCacheAnalytics.FragmentedBytes += FragmentedBytes;
}

void FOnDemandInstallCacheStats::OnCacheUsage(
	uint64 MaxCacheSize,
	uint64 CacheSize,
	uint64 ReferencedBlockSize,
	uint64 ReferencedSize,
	uint64 FragmentedSize,
	int64 OldestBlockAccess)
{
	UE::TUniqueLock Lock(InstallCacheAnalytics);
	InstallCacheAnalytics.MaxCacheSize				= MaxCacheSize; 
	InstallCacheAnalytics.MaxCacheUsageSize			= FMath::Max(InstallCacheAnalytics.MaxCacheUsageSize, CacheSize);
	InstallCacheAnalytics.MaxReferencedBlockSize	= FMath::Max(InstallCacheAnalytics.MaxReferencedBlockSize, ReferencedBlockSize);
	InstallCacheAnalytics.MaxReferencedSize			= FMath::Max(InstallCacheAnalytics.MaxReferencedSize, ReferencedSize);
	InstallCacheAnalytics.MaxFragmentedSize			= FMath::Max(InstallCacheAnalytics.MaxFragmentedSize, FragmentedSize);
	InstallCacheAnalytics.OldestBlockAccess			= FMath::Min(InstallCacheAnalytics.OldestBlockAccess, OldestBlockAccess);
}

void FOnDemandInstallCacheStats::OnReadCompleted(EIoErrorCode ErrorCode)
{
	if (ErrorCode == EIoErrorCode::Cancelled)
	{
		return;
	}

	UE::TUniqueLock Lock(InstallCacheAnalytics);
	if (ErrorCode != EIoErrorCode::Ok)
	{
		++InstallCacheAnalytics.ReadErrorCount;
	}
	++InstallCacheAnalytics.ReadCount;
}

} // namespace UE::IoStore

#undef UE_ENABLE_ONSCREEN_STATISTICS

#endif // IAS_WITH_STATISTICS
