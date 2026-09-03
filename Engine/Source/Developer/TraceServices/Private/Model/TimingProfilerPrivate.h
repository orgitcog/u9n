// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TraceServices/Model/TimingProfiler.h"
#include "Common/SlabAllocator.h"
#include "Model/MonotonicTimeline.h"
#include "Model/Tables.h"

namespace TraceServices
{

class FAnalysisSessionLock;
class FStringStore;

struct FGpuQueueData
{
	TArray<FGpuSignalFence> SignalFenceArray;
	TArray<FGpuWaitFence> WaitFenceArray;
};

class FTimingProfilerProvider
	: public ITimingProfilerProvider
	, public ITimingProfilerTimerReader
	, public IEditableTimingProfilerProvider
{
public:
	typedef TMonotonicTimeline<FTimingProfilerEvent> TimelineInternal;

	explicit FTimingProfilerProvider(IAnalysisSession& InSession);
	virtual ~FTimingProfilerProvider();

	TimelineInternal& EditGpuTimeline();
	TimelineInternal& EditGpu2Timeline();

	// ITimingProfilerProvider
	virtual bool GetCpuThreadTimelineIndex(uint32 ThreadId, uint32& OutTimelineIndex) const override;
	virtual bool GetGpuTimelineIndex(uint32& OutTimelineIndex) const override;
	virtual bool GetGpu2TimelineIndex(uint32& OutTimelineIndex) const override;
	virtual void EnumerateGpuQueues(TFunctionRef<void(const FGpuQueueInfo&)> Callback) const override;
	virtual bool GetGpuQueueTimelineIndex(uint32 QueueId, uint32& OutTimelineIndex) const override;
	virtual bool GetVerseTimelineIndex(uint32& OutTimelineIndex) const override;

	virtual void EnumerateGpuSignalFences(uint32 QueueId, double StartTime, double EndTime, EnumerateGpuSignalFencesCallback Callback) const override;
	virtual void EnumerateGpuWaitFences(uint32 QueueId, double StartTime, double EndTime, EnumerateGpuWaitFencesCallback Callback) const override;
	virtual void EnumerateGpuFences(uint32 QueueId, double StartTime, double EndTime, EnumerateGpuFencesCallback Callback) const override;
	virtual void EnumerateResolvedGpuFences(uint32 QueueId, double StartTime, double EndTime, EnumerateResolvedGpuFencesCallback Callback) const override;

	virtual bool ReadTimeline(uint32 Index, TFunctionRef<void(const Timeline&)> Callback) const override;
	virtual uint32 GetTimelineCount() const override { return Timelines.Num(); }
	virtual void EnumerateTimelines(TFunctionRef<void(const Timeline&)> Callback) const override;
	virtual void ReadTimers(TFunctionRef<void(const ITimingProfilerTimerReader&)> Callback) const override;
	virtual uint32 GetOriginalTimerIdFromMetadata(uint32 MetadataTimerId) const override;
	virtual TArrayView<const uint8> GetMetadata(uint32 MetadataTimerId) const override;
	virtual const FMetadataSpec* GetMetadataSpec(uint32 MetadataSpecId) const;
	virtual ITable<FTimingProfilerAggregatedStats>* CreateAggregation(const FCreateAggregationParams& Params) const override;
	virtual ITimingProfilerButterfly* CreateButterfly(const FCreateButterflyParams& Params) const override;

	// ITimingProfilerTimerReader
	virtual const FTimingProfilerTimer* GetTimer(uint32 TimerId) const override;
	virtual uint32 GetTimerCount() const override;
	//virtual uint32 GetOriginalTimerIdFromMetadata(uint32 MetadataTimerId) const override;
	//virtual TArrayView<const uint8> GetMetadata(uint32 MetadataTimerId) const override;

	// IEditableTimingProfilerProvider
	virtual uint32 AddCpuTimer(FStringView Name, const TCHAR* File = nullptr, uint32 Line = 0) override;
	virtual uint32 AddGpuTimer(FStringView Name, const TCHAR* File = nullptr, uint32 Line = 0) override;
	virtual uint32 AddVerseTimer(FStringView Name, const TCHAR* File = nullptr, uint32 Line = 0) override;
	virtual void SetTimerName(uint32 TimerId, FStringView Name) override;
	virtual void SetTimerNameAndLocation(uint32 TimerId, FStringView Name, const TCHAR* File, uint32 Line) override;
	virtual uint32 AddMetadata(uint32 OriginalTimerId, TArray<uint8>&& Metadata) override;
	virtual void SetMetadata(uint32 MetadataTimerId, TArray<uint8>&& Metadata) override;
	virtual void SetMetadata(uint32 MetadataTimerId, TArray<uint8>&& Metadata, uint32 NewTimerId) override;
	virtual TArrayView<uint8> GetEditableMetadata(uint32 MetadataTimerId) override;

	virtual uint32 AddMetadataSpec(FMetadataSpec&& Metadata) override;
	virtual void SetMetadataSpec(uint32 TimerId, uint32 MetadataSpecId) override;

	virtual void AddGpuQueue(uint32 QueueId, uint8 GPU, uint8 Index, uint8 Type, const TCHAR* Name) override;
	virtual void AddGpuSignalFence(uint32 QueueId, const FGpuSignalFence& SignalFence) override;
	virtual void AddGpuWaitFence(uint32 QueueId, const FGpuWaitFence& WaitFence) override;

	virtual IEditableTimeline<FTimingProfilerEvent>& GetCpuThreadEditableTimeline(uint32 ThreadId) override;
	virtual IEditableTimeline<FTimingProfilerEvent>* GetGpuQueueEditableTimeline(uint32 QueueId) override;
	virtual IEditableTimeline<FTimingProfilerEvent>* GetGpuQueueWorkEditableTimeline(uint32 QueueId) override;
	virtual IEditableTimeline<FTimingProfilerEvent>* GetVerseEditableTimeline() override;

	virtual const ITimingProfilerProvider* GetReadProvider() const override { return this; }

private:
	enum class ETimingProfilerTimerType : uint32
	{
		Cpu,
		Gpu,
		Verse
	};

	FTimingProfilerTimer& AddTimerInternal(FStringView Name, const TCHAR* File, uint32 Line, ETimingProfilerTimerType TimerType);

	struct FMetadata
	{
		TArray<uint8> Payload;
		uint32 TimerId;
	};

	IAnalysisSession& Session;
	TArray<FMetadata> Metadatas;
	TArray<FMetadataSpec> MetadataSpecs;
	TArray<FTimingProfilerTimer> Timers;
	TArray<TSharedRef<TimelineInternal>> Timelines;
	TMap<uint32, uint32> CpuThreadTimelineIndexMap; // CPU Thread Id --> timeline index, in Timelines
	static const uint32 GpuTimelineIndex = 0;
	static const uint32 Gpu2TimelineIndex = 1;
	static const uint32 VerseTimelineIndex = 2;
	TArray<FGpuQueueInfo> GpuQueues;
	TArray<FGpuQueueData> GpuQueueData;
	TMap<uint32, uint32> GpuQueueIdToQueueIndexMap; // GPU Queue Id --> queue index, in GpuQueues
	TTableLayout<FTimingProfilerAggregatedStats> AggregatedStatsTableLayout;

};

} // namespace TraceServices
