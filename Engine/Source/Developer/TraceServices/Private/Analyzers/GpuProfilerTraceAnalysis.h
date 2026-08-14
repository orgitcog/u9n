// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Trace/Analyzer.h"
#include "Model/TimingProfilerPrivate.h"

namespace TraceServices
{
	class FAnalysisSession;
	class IEditableCounter;
	class IEditableCounterProvider;

class FGpuProfilerAnalyzer
	: public UE::Trace::IAnalyzer
{
private:
	enum : uint16
	{
		// The New GPU Profiler
		RouteId_Init,
		RouteId_QueueSpec,
		RouteId_EventFrameBoundary,
		RouteId_EventBreadcrumbSpec,
		RouteId_EventBeginBreadcrumb,
		RouteId_EventEndBreadcrumb,
		RouteId_EventBeginWork,
		RouteId_EventEndWork,
		RouteId_EventWait,
		RouteId_EventStats,
		RouteId_SignalFence,
		RouteId_WaitFence,

		// The Old GPU Profiler
		RouteId_EventSpec,
		RouteId_Frame, // GPU Index 0
		RouteId_Frame2, // GPU Index 1
	};

	struct FOpenEvent
	{
		double Time;
		uint32 TimerId;
	};

	struct FQueue
	{
		uint32 Id = 0;
		uint32 FrameNumber = 0;
		TArray<FOpenEvent> Stack[2]; // [0] = breadcrumbs, [1] = work
		IEditableCounter* NumDrawsCounter;
		IEditableCounter* NumPrimitivesCounter;
		uint64 NumDraws = 0;
		uint64 NumPrimitives = 0;
		double LastTime = 0.0f;
	};

	struct FErrorData
	{
		const uint32 NumMaxErrors = 100;

		uint32 NumInterleavedEvents = 0;
		uint32 NumInterleavedAndReversedEvents = 0;
		uint32 NumMismatchedEvents = 0;
		uint32 NumNegativeDurationEvents = 0;

		double InterleavedEventsMaxDelta = 0.0f;
		double InterleavedAndReversedEventsMaxDelta = 0.0f;
		double NegativeDurationEventsMaxDelta = 0.0f;
	};

public:
	FGpuProfilerAnalyzer(FAnalysisSession& Session, FTimingProfilerProvider& TimingProfilerProvider, IEditableCounterProvider& EditableCounterProvider);
	virtual void OnAnalysisBegin(const FOnAnalysisContext& Context) override;
	virtual void OnAnalysisEnd() override;
	virtual bool OnEvent(uint16 RouteId, EStyle Style, const FOnEventContext& Context) override;

private:
	uint32 GetOrAddTimerId(const FString& Breadcrumb);
	const TCHAR* GetTimerName(uint32 TimerId) const;
	FQueue& GetOrAddQueue(uint32 QueueId);
	void BeginEvent(FQueue& Queue, int32 StackIndex, double Time, uint32 TimerId);
	void EndEvent(FQueue& Queue, int32 StackIndex, double Time, uint32 TimerId);

	void InitCounters(FQueue& FoundQueue);
	void InitCountersDesc(FQueue& FoundQueue, uint8 Gpu, uint8 Index, const TCHAR* Name);

private:
	FAnalysisSession& Session;
	FTimingProfilerProvider& TimingProfilerProvider;
	IEditableCounterProvider& EditableCounterProvider;

	//////////////////////////////////////////////////
	// The New GPU Profiler

	uint32 Version = 0;
	uint32 GpuWorkTimerId = ~0;
	uint32 GpuWaitTimerId = ~0;
	TMap<uint32, uint32> BreadcrumbSpecMap; // breadcrumb spec id --> GPU timer id
	TMap<FString, uint32> BreadcrumbMap; // breadcrumb name --> GPU timer id
	TMap<uint32, const TCHAR*> TimerMap; // GPU timer id --> persistent timer name
	TMap<uint32, FQueue> Queues; // QueueId --> FQueue
	FErrorData ErrorData;

	//////////////////////////////////////////////////
	// The Old GPU Profiler

	TMap<uint64, uint32> EventTypeMap;
	double MinTime = DBL_MIN;
	double MinTime2 = DBL_MIN;
	uint32 NumFrames = 0;
	uint32 NumFramesWithErrors = 0;
};

} // namespace TraceServices
