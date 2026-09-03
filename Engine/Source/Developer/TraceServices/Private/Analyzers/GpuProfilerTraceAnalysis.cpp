// Copyright Epic Games, Inc. All Rights Reserved.

#include "GpuProfilerTraceAnalysis.h"

#include "AnalysisServicePrivate.h"
#include "CborWriter.h"
#include "CborReader.h"
#include "Common/Utils.h"
#include "HAL/LowLevelMemTracker.h"
#include "Model/CountersPrivate.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

#define UE_INSIGHTS_MERGE_UNACCOUNTED_TIMERS 0

namespace TraceServices
{

FGpuProfilerAnalyzer::FGpuProfilerAnalyzer(FAnalysisSession& InSession, FTimingProfilerProvider& InTimingProfilerProvider, IEditableCounterProvider& InEditableCounterProvider)
	: Session(InSession)
	, TimingProfilerProvider(InTimingProfilerProvider)
	, EditableCounterProvider(InEditableCounterProvider)
{
}

void FGpuProfilerAnalyzer::OnAnalysisBegin(const FOnAnalysisContext& Context)
{
	auto& Builder = Context.InterfaceBuilder;

	// The New GPU Profiler
	Builder.RouteEvent(RouteId_Init, "GpuProfiler", "Init");
	Builder.RouteEvent(RouteId_QueueSpec, "GpuProfiler", "QueueSpec");
	Builder.RouteEvent(RouteId_EventFrameBoundary, "GpuProfiler", "EventFrameBoundary");
	Builder.RouteEvent(RouteId_EventBreadcrumbSpec, "GpuProfiler", "EventBreadcrumbSpec");
	Builder.RouteEvent(RouteId_EventBeginBreadcrumb, "GpuProfiler", "EventBeginBreadcrumb");
	Builder.RouteEvent(RouteId_EventEndBreadcrumb, "GpuProfiler", "EventEndBreadcrumb");
	Builder.RouteEvent(RouteId_EventBeginWork, "GpuProfiler", "EventBeginWork");
	Builder.RouteEvent(RouteId_EventEndWork, "GpuProfiler", "EventEndWork");
	Builder.RouteEvent(RouteId_EventWait, "GpuProfiler", "EventWait");
	Builder.RouteEvent(RouteId_EventStats, "GpuProfiler", "EventStats");
	Builder.RouteEvent(RouteId_SignalFence, "GpuProfiler", "SignalFence");
	Builder.RouteEvent(RouteId_WaitFence, "GpuProfiler", "WaitFence");

	// The Old GPU Profiler (deprecated in UE 5.6)
	// Analysis code is maintained for backward compatibility with old traces.
	Builder.RouteEvent(RouteId_EventSpec, "GpuProfiler", "EventSpec");
	Builder.RouteEvent(RouteId_Frame, "GpuProfiler", "Frame");
	Builder.RouteEvent(RouteId_Frame2, "GpuProfiler", "Frame2");
}

void FGpuProfilerAnalyzer::OnAnalysisEnd()
{
	//////////////////////////////////////////////////
	// Old GPU Profiler

	if (NumFramesWithErrors > 0)
	{
		UE_LOG(LogTraceServices, Error, TEXT("[GpuProfiler] Frames with errors: %u"), NumFramesWithErrors);
	}

	if (NumFrames > 0 || EventTypeMap.Num() > 0)
	{
		UE_LOG(LogTraceServices, Log, TEXT("[GpuProfiler] Analysis completed (%u frames, %d timers)."), NumFrames, EventTypeMap.Num());
	}

	//////////////////////////////////////////////////
	// New GPU Profiler

	if (ErrorData.NumInterleavedEvents > 0)
	{
		UE_LOG(LogTraceServices, Error, TEXT("[GpuProfiler] Number of interleaved events: %u, max error %f"), ErrorData.NumInterleavedEvents, ErrorData.InterleavedEventsMaxDelta);
	}

	if (ErrorData.NumInterleavedAndReversedEvents > 0)
	{
		UE_LOG(LogTraceServices, Error, TEXT("[GpuProfiler] Number of interleaved and reversed events: %u, max error %f"), ErrorData.NumInterleavedAndReversedEvents, ErrorData.InterleavedAndReversedEventsMaxDelta);
	}

	if (ErrorData.NumMismatchedEvents > 0)
	{
		UE_LOG(LogTraceServices, Error, TEXT("[GpuProfiler] Number of mismatched events: %u"), ErrorData.NumMismatchedEvents);
	}

	if (ErrorData.NumNegativeDurationEvents > 0)
	{
		UE_LOG(LogTraceServices, Error, TEXT("[GpuProfiler] Number of negative duration events: %u, max error %f"), ErrorData.NumNegativeDurationEvents, ErrorData.NegativeDurationEventsMaxDelta);
	}

	if (Queues.Num() > 0 || TimerMap.Num() > 0)
	{
		UE_LOG(LogTraceServices, Log, TEXT("[GpuProfiler] Analysis completed (%u queues, %d timers, %d breadcrumb specs, %d breadcrumb names)."),
			Queues.Num(), TimerMap.Num(), BreadcrumbSpecMap.Num(), BreadcrumbMap.Num());
	}
}

bool FGpuProfilerAnalyzer::OnEvent(uint16 RouteId, EStyle Style, const FOnEventContext& Context)
{
	LLM_SCOPE_BYNAME(TEXT("Insights/FGpuProfilerAnalyzer"));

	const auto& EventData = Context.EventData;

	switch (RouteId)
	{

	//////////////////////////////////////////////////
	// The New GPU Profiler

	case RouteId_Init:
	{
		Version = (uint32)EventData.GetValue<uint8>("Version");
		UE_LOG(LogTraceServices, Log, TEXT("[GPU] Init Version=%u"), Version);
		break;
	}
	case RouteId_QueueSpec:
	{
		const uint32 QueueId = EventData.GetValue<uint32>("QueueId");

		const uint8 GPU = (QueueId >> 8) & 0xFF;
		const uint8 Index = (QueueId >> 16) & 0xFF;
		const uint8 Type = QueueId & 0xFF;

		FString Name;
		EventData.GetString("TypeString", Name);

		UE_LOG(LogTraceServices, Log, TEXT("[GPU] QueueSpec QueueId=%u, GPU=%u, Index=%u, Type=%d \"%s\""), QueueId, (uint32)GPU, (uint32)Index, (uint32)Type, *Name);

		FAnalysisSessionEditScope _(Session);
		const TCHAR* PersistentName = Session.StoreString(Name);

		TimingProfilerProvider.AddGpuQueue(QueueId, GPU, Index, Type, PersistentName);

		FQueue& Queue = GetOrAddQueue(QueueId);
		InitCountersDesc(Queue, GPU, Index, *Name);

		break;
	}
	case RouteId_EventFrameBoundary:
	{
		const uint32 QueueId = EventData.GetValue<uint32>("QueueId");
		const uint32 FrameNumber = EventData.GetValue<uint32>("FrameNumber");

		UE_LOG(LogTraceServices, VeryVerbose, TEXT("[GPU] EventFrameBoundary QueueId=%u, FrameNumber=%u"), QueueId, FrameNumber);

		FQueue& Queue = GetOrAddQueue(QueueId);
		Queue.FrameNumber = FrameNumber;

		if (Queue.LastTime > 0)
		{
			FAnalysisSessionEditScope _(Session);

			Queue.NumDrawsCounter->SetValue(Queue.LastTime, (int64)Queue.NumDraws);
			Queue.NumPrimitivesCounter->SetValue(Queue.LastTime, (int64)Queue.NumPrimitives);
		}

		Queue.NumDraws = 0;
		Queue.NumPrimitives = 0;
		Queue.LastTime = 0;

		break;
	}
	case RouteId_EventBreadcrumbSpec:
	{
		FString Name, NameFormat;
		uint32 SpecId = EventData.GetValue<uint32>("SpecId");
		EventData.GetString("StaticName", Name);
		EventData.GetString("NameFormat", NameFormat);
		TArrayView<const uint8> FieldNames = EventData.GetArrayView<uint8>("FieldNames");

		if (Name.Compare(NameFormat) == 0)
		{
			Name.Empty();
		}

		int Index = 0;
		while (Index < NameFormat.Len() && Index < Name.Len())
		{
			if (Name[Index] != NameFormat[Index])
			{
				break;
			}

			++Index;
		}

		if (Index > 1)
		{
			NameFormat.MidInline(Index);
		}

		if (Name.IsEmpty())
		{
			const FString Separators = TEXT("% (=");
			Index = 0;
			bool bIsInFormatSpecifier = false;
			while (Index < NameFormat.Len())
			{
				int32 SpecIndex = -1;
				if (Separators.FindChar(NameFormat[Index], SpecIndex))
				{
					Name = NameFormat.Left(Index);
					NameFormat.MidInline(Index);
					break;
				}

				++Index;
			}

			if (Name.IsEmpty())
			{
				if (!NameFormat.IsEmpty())
				{
					Name = NameFormat;
					NameFormat.Empty();
				}
				else
				{
					Name = Session.StoreString(TEXT("Unknown"));
				}
			}
		}

		FMetadataSpec Spec;
		if (!NameFormat.IsEmpty())
		{
			Spec.Format = Session.StoreString(NameFormat);
		}

		if (FieldNames.Num() > 0)
		{
			FMemoryReaderView MemoryReader(FieldNames);
			FCborReader CborReader(&MemoryReader, ECborEndianness::StandardCompliant);
			FCborContext CborContext;

			while (CborReader.ReadNext(CborContext))
			{
				if (CborContext.MajorType() == ECborCode::TextString)
				{
					FString Field = CborContext.AsString();
					Spec.FieldNames.Add(Session.StoreString(Field));
				}
			}
		}

		FAnalysisSessionEditScope _(Session);
		uint32 TimerId = 0;
		uint32* FoundSpecIdPtr = BreadcrumbSpecMap.Find(SpecId);
		if (FoundSpecIdPtr == nullptr)
		{
			TimerId = GetOrAddTimerId(Name);
			BreadcrumbSpecMap.Add(SpecId, TimerId);
		}
		else
		{
			TimingProfilerProvider.SetTimerName(TimerId, Name);
		}

		if (Spec.FieldNames.Num() > 0 || Spec.Format != nullptr)
		{
			uint32 MetadataSpecId = TimingProfilerProvider.AddMetadataSpec(MoveTemp(Spec));
			TimingProfilerProvider.SetMetadataSpec(TimerId, MetadataSpecId);
		}

		break;
	}
	case RouteId_EventBeginBreadcrumb:
	{
		const uint32 SpecId = EventData.GetValue<uint32>("SpecId");
		const uint32 QueueId = EventData.GetValue<uint32>("QueueId");
		const uint64 GPUTimestampTOP = EventData.GetValue<uint64>("GPUTimestampTOP");

		// Ignore events for which the timestamp could not be determined.
		if (GPUTimestampTOP == 0)
		{
			break;
		}

		TArray<uint8> Metadata(EventData.GetArrayView<uint8>("Metadata"));
		const double Time = Context.EventTime.AsSeconds(GPUTimestampTOP);

		UE_LOG(LogTraceServices, VeryVerbose, TEXT("[GPU] EventBeginBreadcrumb QueueId=%u, Time=%f, SpecId=\"%u\""), QueueId, Time, SpecId);

		uint32 TimerId = 0;
		uint32* TimerIdPtr;
		TimerIdPtr = BreadcrumbSpecMap.Find(SpecId);

		{
			FAnalysisSessionEditScope _(Session);
			if (TimerIdPtr)
			{
				TimerId = *TimerIdPtr;
			}
			else
			{
				TimerId = GetOrAddTimerId(TEXT("Unknown"));
				BreadcrumbSpecMap.Add(SpecId, TimerId);
			}

			TimerId = TimingProfilerProvider.AddMetadata(TimerId, MoveTemp(Metadata));
		}

		FQueue& Queue = GetOrAddQueue(QueueId);
		BeginEvent(Queue, 0, Time, TimerId);
		break;
	}
	case RouteId_EventEndBreadcrumb:
	{
		const uint32 QueueId = EventData.GetValue<uint32>("QueueId");
		const uint64 GPUTimestampBOP = EventData.GetValue<uint64>("GPUTimestampBOP");

		// Ignore events for which the timestamp could not be determined.
		if (GPUTimestampBOP == 0)
		{
			break;
		}

		const double Time = Context.EventTime.AsSeconds(GPUTimestampBOP);

		FString Name;
		EventData.GetString("Name", Name);

		UE_LOG(LogTraceServices, VeryVerbose, TEXT("[GPU] EventEndBreadcrumb QueueId=%u, Time=%f, Name=\"%s\""), QueueId, Time, *Name);

		FQueue& Queue = GetOrAddQueue(QueueId);
		EndEvent(Queue, 0, Time, (uint32)- 1);
		break;
	}
	case RouteId_EventBeginWork:
	{
		const uint32 QueueId = EventData.GetValue<uint32>("QueueId");
		const uint64 GPUTimestampTOP = EventData.GetValue<uint64>("GPUTimestampTOP");
		double Time = Context.EventTime.AsSeconds(GPUTimestampTOP);
		//const uint64 CPUTimestamp = EventData.GetValue<uint64>("CPUTimestamp"); // not used

		UE_LOG(LogTraceServices, VeryVerbose, TEXT("[GPU] EventBeginWork QueueId=%u, GPUTimestampTOP=%f"), QueueId, Time);

		if (GpuWorkTimerId == ~0u)
		{
			const FString GpuWorkTimerName(TEXT("GpuWork"));
			GpuWorkTimerId = GetOrAddTimerId(GpuWorkTimerName);
		}

		FQueue& Queue = GetOrAddQueue(QueueId);
		TArray<uint8> CborData;
		{
			CborData.Reserve(256);
			FMemoryWriter MemoryWriter(CborData, false, true);
			FCborWriter CborWriter(&MemoryWriter, ECborEndianness::StandardCompliant);
			CborWriter.WriteContainerStart(ECborCode::Map, 1); // 1 is the FieldCount
			CborWriter.WriteValue("Frame", 5);
			CborWriter.WriteValue((int64)Queue.FrameNumber);
		}
		uint32 MetadataTimerId;
		{
			FAnalysisSessionEditScope _(Session);
			MetadataTimerId = TimingProfilerProvider.AddMetadata(GpuWorkTimerId, MoveTemp(CborData));
		}
		BeginEvent(Queue, 1, Time, MetadataTimerId);
		break;
	}
	case RouteId_EventEndWork:
	{
		uint32 QueueId = EventData.GetValue<uint32>("QueueId");
		const uint64 GPUTimestampBOP = EventData.GetValue<uint64>("GPUTimestampBOP");
		const double Time = Context.EventTime.AsSeconds(GPUTimestampBOP);

		UE_LOG(LogTraceServices, VeryVerbose, TEXT("[GPU] EventEndWork QueueId=%u, GPUTimestampBOP=%f"), QueueId, Time);

		FQueue& Queue = GetOrAddQueue(QueueId);
		EndEvent(Queue, 1, Time, GpuWorkTimerId);
		break;
	}
	case RouteId_EventWait:
	{
		uint32 QueueId = EventData.GetValue<uint32>("QueueId");
		const uint64 StartTimestamp = EventData.GetValue<uint64>("StartTime");
		const double StartTime = Context.EventTime.AsSeconds(StartTimestamp);

		const uint64 EndTimestamp = EventData.GetValue<uint64>("EndTime");
		const double EndTime = Context.EventTime.AsSeconds(EndTimestamp);

		UE_LOG(LogTraceServices, VeryVerbose, TEXT("[GPU] EventWait QueueId=%u, StartTime=%f, EndTime=%f"), QueueId, StartTime, EndTime);

		if (GpuWaitTimerId == ~0u)
		{
			const FString GpuWaitTimerName(TEXT("GpuWait"));
			GpuWaitTimerId = GetOrAddTimerId(GpuWaitTimerName);
		}

		FQueue& Queue = GetOrAddQueue(QueueId);
		TArray<uint8> CborData;
		{
			CborData.Reserve(256);
			FMemoryWriter MemoryWriter(CborData, false, true);
			FCborWriter CborWriter(&MemoryWriter, ECborEndianness::StandardCompliant);
			CborWriter.WriteContainerStart(ECborCode::Map, 1); // 1 is the FieldCount
			CborWriter.WriteValue("Frame", 5);
			CborWriter.WriteValue((int64)Queue.FrameNumber);
		}
		uint32 MetadataTimerId;
		{
			FAnalysisSessionEditScope _(Session);
			MetadataTimerId = TimingProfilerProvider.AddMetadata(GpuWaitTimerId, MoveTemp(CborData));
		}

		BeginEvent(Queue, 1, StartTime, MetadataTimerId);
		EndEvent(Queue, 1, EndTime, GpuWaitTimerId);
		break;
	}
	case RouteId_EventStats:
	{
		const uint32 QueueId = EventData.GetValue<uint32>("QueueId");
		const uint32 NumDraws = EventData.GetValue<uint32>("NumDraws");
		const uint32 NumPrimitives = EventData.GetValue<uint32>("NumPrimitives");

		FQueue& Queue = GetOrAddQueue(QueueId);

		Queue.NumDraws += NumDraws;
		Queue.NumPrimitives += NumPrimitives;

		break;
	}
	case RouteId_SignalFence:
	{
		uint32 QueueId = EventData.GetValue<uint32>("QueueId");
		uint64 Timestamp = EventData.GetValue<uint64>("CPUTimestamp");
		uint64 Value = EventData.GetValue<uint64>("Value");;

		FQueue& Queue = GetOrAddQueue(QueueId);
		FGpuSignalFence Fence;
		Fence.Timestamp = Context.EventTime.AsSeconds(Timestamp);
		Fence.Value = Value;

		FAnalysisSessionEditScope _(Session);
		TimingProfilerProvider.AddGpuSignalFence(QueueId, Fence);

		break;
	}
	case RouteId_WaitFence:
	{
		uint32 QueueId = EventData.GetValue<uint32>("QueueId");
		uint64 Timestamp = EventData.GetValue<uint64>("CPUTimestamp");
		uint32 QueueToWaitForId = EventData.GetValue<uint32>("QueueToWaitForId");
		uint64 Value = EventData.GetValue<uint64>("Value");;

		FGpuWaitFence Fence;
		Fence.Timestamp = Context.EventTime.AsSeconds(Timestamp);
		Fence.Value = Value;
		Fence.QueueToWaitForId = QueueToWaitForId;

		FAnalysisSessionEditScope _(Session);
		TimingProfilerProvider.AddGpuWaitFence(QueueId, Fence);

		break;
	}

	//////////////////////////////////////////////////
	// The Old GPU Profiler

	case RouteId_EventSpec:
	{
		uint32 EventType = EventData.GetValue<uint32>("EventType");
		const auto& Name = EventData.GetArray<UTF16CHAR>("Name");

		auto NameTChar = StringCast<TCHAR>(Name.GetData(), Name.Num());
		uint32* TimerIndexPtr = EventTypeMap.Find(EventType);
		if (!TimerIndexPtr)
		{
			FAnalysisSessionEditScope _(Session);
			uint32 TimerId;
#if UE_INSIGHTS_MERGE_UNACCOUNTED_TIMERS
			FStringView Unaccounted = TEXTVIEW("Unaccounted -");
			if (NameTChar.Length() > Unaccounted.Len() &&
				FCString::Strncmp(Unaccounted.GetData(), NameTChar.Get(), Unaccounted.Len()) == 0)
			{
				TimerId = TimingProfilerProvider.AddGpuTimer(TEXTVIEW("Unaccounted"));
			}
			else
#endif // UE_INSIGHTS_MERGE_UNACCOUNTED_TIMERS
			{
				TimerId = TimingProfilerProvider.AddGpuTimer(FStringView(NameTChar.Get(), NameTChar.Length()));
			}
			EventTypeMap.Add(EventType, TimerId);
		}
		else
		{
			FAnalysisSessionEditScope _(Session);
			TimingProfilerProvider.SetTimerName(*TimerIndexPtr, FStringView(NameTChar.Get(), NameTChar.Length()));
		}
		break;
	}
	case RouteId_Frame:
	case RouteId_Frame2:
	{
		FAnalysisSessionEditScope _(Session);

		TraceServices::FTimingProfilerProvider::TimelineInternal& ThisTimeline = (RouteId == RouteId_Frame) ?
			TimingProfilerProvider.EditGpuTimeline() :
			TimingProfilerProvider.EditGpu2Timeline();
		double& ThisMinTime = (RouteId == RouteId_Frame) ? MinTime : MinTime2;

		const auto& Data = EventData.GetArray<uint8>("Data");
		const uint8* BufferPtr = Data.GetData();
		const uint8* BufferEnd = BufferPtr + Data.Num();

		uint64 CalibrationBias = EventData.GetValue<uint64>("CalibrationBias");
		uint64 LastTimestamp = EventData.GetValue<uint64>("TimestampBase");
		uint32 RenderingFrameNumber = EventData.GetValue<uint32>("RenderingFrameNumber");

		++NumFrames;

		double LastTime = 0.0;
		uint32 CurrentDepth = 0;
		bool bHasErrors = false;

		while (BufferPtr < BufferEnd)
		{
			uint64 DecodedTimestamp = FTraceAnalyzerUtils::Decode7bit(BufferPtr);
			uint64 ActualTimestamp = (DecodedTimestamp >> 1) + LastTimestamp;
			LastTimestamp = ActualTimestamp;
			LastTime = double(ActualTimestamp + CalibrationBias) * 0.000001;
			LastTime += Context.EventTime.AsSeconds(0);

			if (LastTime < 0.0)
			{
				if (DecodedTimestamp & 1ull)
				{
					BufferPtr += sizeof(uint32);
				}
				bHasErrors = true;
				continue;
			}

			// If it advances with more than 1h, it is probably a wrong timestamp.
			if (LastTime > ThisMinTime + 3600.0 && ThisMinTime != DBL_MIN)
			{
				LastTime = ThisMinTime;
				bHasErrors = true;
			}

			// The monolithic timeline assumes that timestamps are ever increasing, but
			// with gpu/cpu calibration and drift there can be a tiny bit of overlap between
			// frames. So we just clamp.
			if (ThisMinTime > LastTime)
			{
				LastTime = ThisMinTime;
			}
			ThisMinTime = LastTime;

			if (DecodedTimestamp & 1ull)
			{
				uint32 EventType = *reinterpret_cast<const uint32*>(BufferPtr);
				BufferPtr += sizeof(uint32);
				if (EventTypeMap.Contains(EventType))
				{
					FTimingProfilerEvent Event;
					Event.TimerIndex = EventTypeMap[EventType];
					ThisTimeline.AppendBeginEvent(LastTime, Event);
				}
				else
				{
					FTimingProfilerEvent Event;
					Event.TimerIndex = TimingProfilerProvider.AddGpuTimer(TEXTVIEW("<unknown>"));
					EventTypeMap.Add(EventType, Event.TimerIndex);
					ThisTimeline.AppendBeginEvent(LastTime, Event);
				}
				++CurrentDepth;
			}
			else
			{
				if (CurrentDepth > 0)
				{
					--CurrentDepth;
				}
				ThisTimeline.AppendEndEvent(LastTime);
			}
		}
		check(BufferPtr == BufferEnd);
		check(CurrentDepth == 0);
		if (bHasErrors && ++NumFramesWithErrors <= 100)
		{
			UE_LOG(LogTraceServices, Error, TEXT("[GpuProfiler] The rendering frame %u has invalid timestamps!"), RenderingFrameNumber);
		}
		Session.UpdateDurationSeconds(LastTime);
		break;
	}

	//////////////////////////////////////////////////

	} // switch (RouteId)

	return true;
}

uint32 FGpuProfilerAnalyzer::GetOrAddTimerId(const FString& Breadcrumb)
{
	const uint32* FoundTimerId = BreadcrumbMap.Find(Breadcrumb);
	if (FoundTimerId)
	{
		return *FoundTimerId;
	}
	else
	{
		FAnalysisSessionEditScope _(Session);
		uint32 TimerId = TimingProfilerProvider.AddGpuTimer(Breadcrumb);
		const TCHAR* TimerName = nullptr;
		TimingProfilerProvider.ReadTimers([TimerId, &TimerName](const ITimingProfilerTimerReader& TimerReader)
			{
				const FTimingProfilerTimer* Timer = TimerReader.GetTimer(TimerId);
				if (Timer)
				{
					TimerName = Timer->Name;
				}
			});
		BreadcrumbMap.Add(Breadcrumb, TimerId);
		TimerMap.Add(TimerId, TimerName);
		return TimerId;
	}
}

const TCHAR* FGpuProfilerAnalyzer::GetTimerName(uint32 TimerId) const
{
	const TCHAR* const* TimerNamePtr = TimerMap.Find(TimerId);
	return TimerNamePtr ? *TimerNamePtr : TEXT("<unknown>");
}

FGpuProfilerAnalyzer::FQueue& FGpuProfilerAnalyzer::GetOrAddQueue(uint32 QueueId)
{
	FQueue* FoundQueue = Queues.Find(QueueId);
	if (FoundQueue)
	{
		return *FoundQueue;
	}

	FQueue& NewQueue = Queues.Add(QueueId);
	NewQueue.Id = QueueId;
	InitCounters(NewQueue);
	return NewQueue;
}

void FGpuProfilerAnalyzer::BeginEvent(FQueue& Queue, int32 StackIndex, double BeginEventTime, uint32 BeginEventTimerId)
{
	Queue.Stack[StackIndex].Push({ BeginEventTime, BeginEventTimerId });
	Queue.LastTime = BeginEventTime;

	FAnalysisSessionEditScope _(Session);
	IEditableTimeline<FTimingProfilerEvent>* Timeline = (StackIndex == 1) ?
		TimingProfilerProvider.GetGpuQueueWorkEditableTimeline(Queue.Id) :
		TimingProfilerProvider.GetGpuQueueEditableTimeline(Queue.Id);
	if (ensure(Timeline))
	{
		FTimingProfilerEvent Event;
		Event.TimerIndex = BeginEventTimerId;
		const double LastTimestamp = Timeline->GetLastTimestamp();
		if (BeginEventTime < LastTimestamp)
		{
			++ErrorData.NumInterleavedEvents;
			ErrorData.InterleavedEventsMaxDelta = FMath::Max(ErrorData.InterleavedEventsMaxDelta, LastTimestamp - BeginEventTime);

			if (ErrorData.NumInterleavedEvents < ErrorData.NumMaxErrors)
			{
				UE_LOG(LogTraceServices, Error, TEXT("[GPU] %s BEGIN %u \"%s\" : Time %f < %f !!!"),
					StackIndex == 1 ? TEXT("WORK") : TEXT("BREADCRUMB"),
					BeginEventTimerId, GetTimerName(BeginEventTimerId),
					BeginEventTime, LastTimestamp);
			}

			BeginEventTime = LastTimestamp;
		}
		Timeline->AppendBeginEvent(BeginEventTime, Event);
		Session.UpdateDurationSeconds(BeginEventTime);
	}
}

void FGpuProfilerAnalyzer::EndEvent(FQueue& Queue, int32 StackIndex, double EndEventTime, uint32 EndEventTimerId)
{
	Queue.LastTime = EndEventTime;

	if (Queue.Stack[StackIndex].IsEmpty())
	{
		return;
	}
	FOpenEvent BeginEvent = Queue.Stack[StackIndex].Pop(EAllowShrinking::No);
	uint32 BeginEventTimerId;
	{
		FAnalysisSessionReadScope _(Session);
		BeginEventTimerId = TimingProfilerProvider.GetOriginalTimerIdFromMetadata(BeginEvent.TimerId);
	}
	if (EndEventTimerId != (uint32)-1 && BeginEventTimerId != EndEventTimerId)
	{
		++ErrorData.NumMismatchedEvents;

		if (ErrorData.NumMismatchedEvents < ErrorData.NumMaxErrors)
		{
			UE_LOG(LogTraceServices, Error, TEXT("[GPU] %s END %u \"%s\" (%f) != BEGIN %u \"%s\" (%f) !!!"),
				StackIndex == 1 ? TEXT("WORK") : TEXT("BREADCRUMB"),
				EndEventTimerId, GetTimerName(EndEventTimerId), EndEventTime,
				BeginEventTimerId, GetTimerName(BeginEventTimerId), BeginEvent.Time);
		}
	}
	if (BeginEvent.Time > EndEventTime)
	{
		++ErrorData.NumNegativeDurationEvents;
		ErrorData.NegativeDurationEventsMaxDelta = FMath::Max(ErrorData.NegativeDurationEventsMaxDelta, BeginEvent.Time - EndEventTime);

		if (ErrorData.NumNegativeDurationEvents < ErrorData.NumMaxErrors)
		{
			UE_LOG(LogTraceServices, Error, TEXT("[GPU] %s END %u \"%s\" (%f) < BEGIN %u \"%s\" (%f) !!!"),
				StackIndex == 1 ? TEXT("WORK") : TEXT("BREADCRUMB"),
				EndEventTimerId, GetTimerName(EndEventTimerId), EndEventTime,
				BeginEventTimerId, GetTimerName(BeginEventTimerId), BeginEvent.Time);
		}
	}

	FAnalysisSessionEditScope _(Session);
	IEditableTimeline<FTimingProfilerEvent>* Timeline = (StackIndex == 1) ?
		TimingProfilerProvider.GetGpuQueueWorkEditableTimeline(Queue.Id) :
		TimingProfilerProvider.GetGpuQueueEditableTimeline(Queue.Id);
	if (ensure(Timeline))
	{
		const double LastTimestamp = Timeline->GetLastTimestamp();
		if (EndEventTime < LastTimestamp)
		{
			++ErrorData.NumInterleavedAndReversedEvents;
			ErrorData.InterleavedAndReversedEventsMaxDelta = FMath::Max(ErrorData.InterleavedAndReversedEventsMaxDelta, LastTimestamp - EndEventTime);

			if (ErrorData.NumInterleavedAndReversedEvents < ErrorData.NumMaxErrors)
			{
				UE_LOG(LogTraceServices, Error, TEXT("[GPU] %s END %u \"%s\" : Time %f < %f !!!"),
					StackIndex == 1 ? TEXT("WORK") : TEXT("BREADCRUMB"),
					EndEventTimerId, GetTimerName(EndEventTimerId),
					EndEventTime, LastTimestamp);
			}

			EndEventTime = LastTimestamp;
		}
		Timeline->AppendEndEvent(EndEventTime);
		Session.UpdateDurationSeconds(EndEventTime);
	}
}

void FGpuProfilerAnalyzer::InitCounters(FQueue& FoundQueue)
{
	FoundQueue.NumDrawsCounter = EditableCounterProvider.CreateEditableCounter();
	FoundQueue.NumDrawsCounter->SetIsFloatingPoint(false);

	FoundQueue.NumPrimitivesCounter = EditableCounterProvider.CreateEditableCounter();
	FoundQueue.NumPrimitivesCounter->SetIsFloatingPoint(false);
}

void FGpuProfilerAnalyzer::InitCountersDesc(FQueue& FoundQueue, uint8 Gpu, uint8 Index, const TCHAR* Name)
{
	const FString DrawsCounterName = FString::Printf(TEXT("NumDraws_GPU%u_%s_%u"), Gpu, Name, Index);
	const FString PrimitivesCounterName = FString::Printf(TEXT("NumPrimitives_GPU%u_%s_%u"), Gpu, Name, Index);

	FoundQueue.NumDrawsCounter->SetName(Session.StoreString(DrawsCounterName));
	FoundQueue.NumDrawsCounter->SetDescription(TEXT("The number of draw calls on the specified queue."));

	FoundQueue.NumPrimitivesCounter->SetName(Session.StoreString(PrimitivesCounterName));
	FoundQueue.NumPrimitivesCounter->SetDescription(TEXT("The number of primitives on the specified queue."));
}

} // namespace TraceServices

#undef UE_INSIGHTS_MERGE_UNACCOUNTED_TIMERS
