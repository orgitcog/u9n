// Copyright Epic Games, Inc. All Rights Reserved.

#include "GpuProfilerTrace.h"

#include "CborWriter.h"
#include "GPUProfiler.h"
#include "HAL/IConsoleManager.h"
#include "Serialization/MemoryWriter.h"
#include "ProfilingDebugging/MiscTrace.h"
#include "Trace/Trace.inl"
#include "RHI.h"
#include "Trace/Detail/Field.h"
#include "Trace/Detail/Important/ImportantLogScope.h"
#include "Trace/Detail/LogScope.h"

// Both (old and new) GPU profilers uses same "GPU" trace channel.
#if GPUPROFILERTRACE_ENABLED || UE_TRACE_GPU_PROFILER_ENABLED
	UE_TRACE_CHANNEL_EXTERN(GpuChannel, RHI_API)
	UE_TRACE_CHANNEL_DEFINE(GpuChannel)
#endif

// The old GpuProfilerTrace is deprecated in UE 5.6
#if GPUPROFILERTRACE_ENABLED

namespace GpuProfilerTrace
{

static TAutoConsoleVariable<int32> CVarGpuProfilerMaxEventBufferSizeKB(
	TEXT("r.GpuProfilerMaxEventBufferSizeKB"),
	32,
	TEXT("Size of the scratch buffer in kB."),
	ECVF_Default);


struct FGpuTraceFrame
{
	int64							CalibrationBias;
	FGPUTimingCalibrationTimestamp	Calibration;
	uint64							TimestampBase;
	uint64							LastTimestamp;
	uint32							RenderingFrameNumber;
	uint32							EventBufferSize;
	bool							bActive;
	uint8*							EventBuffer = nullptr;
	uint32							MaxEventBufferSize = 0;
};

#if RHI_NEW_GPU_PROFILER
FGpuTraceFrame GCurrentFrames[2];
#else
FGpuTraceFrame GCurrentFrame;
#endif

static TSet<uint32> GEventNames;

// deprecated in UE 5.6
UE_TRACE_EVENT_BEGIN(GpuProfiler, EventSpec, NoSync|Important)
	UE_TRACE_EVENT_FIELD(uint32, EventType)
	UE_TRACE_EVENT_FIELD(uint16[], Name)
UE_TRACE_EVENT_END()

// GPU Index 0 - deprecated in UE 5.6
UE_TRACE_EVENT_BEGIN(GpuProfiler, Frame)
	UE_TRACE_EVENT_FIELD(uint64, CalibrationBias)
	UE_TRACE_EVENT_FIELD(uint64, TimestampBase)
	UE_TRACE_EVENT_FIELD(uint32, RenderingFrameNumber)
	UE_TRACE_EVENT_FIELD(uint8[], Data)
UE_TRACE_EVENT_END()

// GPU Index 1 - deprecated in UE 5.6
UE_TRACE_EVENT_BEGIN(GpuProfiler, Frame2)
	UE_TRACE_EVENT_FIELD(uint64, CalibrationBias)
	UE_TRACE_EVENT_FIELD(uint64, TimestampBase)
	UE_TRACE_EVENT_FIELD(uint32, RenderingFrameNumber)
	UE_TRACE_EVENT_FIELD(uint8[], Data)
UE_TRACE_EVENT_END()

} // namespace GpuProfilerTrace

#if RHI_NEW_GPU_PROFILER
#define GPU_TRACE_ARG , uint32 GPUIndex
#else
#define GPU_TRACE_ARG
#endif

void FGpuProfilerTrace::BeginFrame(FGPUTimingCalibrationTimestamp& Calibration GPU_TRACE_ARG)
{
	using namespace GpuProfilerTrace;

#if RHI_NEW_GPU_PROFILER
	FGpuTraceFrame& GCurrentFrame = GCurrentFrames[GPUIndex];
#endif

	if (!bool(GpuChannel))
	{
		return;
	}

	GCurrentFrame.Calibration = Calibration;
	ensure(GCurrentFrame.Calibration.CPUMicroseconds > 0 && GCurrentFrame.Calibration.GPUMicroseconds > 0);
	GCurrentFrame.TimestampBase = 0;
	GCurrentFrame.EventBufferSize = 0;
	GCurrentFrame.bActive = true;

	int32 NeededSize = CVarGpuProfilerMaxEventBufferSizeKB.GetValueOnAnyThread() * 1024;
	if ((GCurrentFrame.MaxEventBufferSize != NeededSize) && (NeededSize > 0))
	{
		FMemory::Free(GCurrentFrame.EventBuffer);
		GCurrentFrame.EventBuffer = (uint8*)FMemory::Malloc(NeededSize);
		GCurrentFrame.MaxEventBufferSize = NeededSize;
	}
}

void FGpuProfilerTrace::SpecifyEventByName(const FName& Name GPU_TRACE_ARG)
{
	using namespace GpuProfilerTrace;

#if RHI_NEW_GPU_PROFILER
	FGpuTraceFrame& GCurrentFrame = GCurrentFrames[GPUIndex];
#endif

	if (!GCurrentFrame.bActive)
	{
		return;
	}

	// This function is only called from FRealtimeGPUProfilerFrame::UpdateStats
	// at the end of the frame, so the access to this container is thread safe

	uint32 Index = Name.GetComparisonIndex().ToUnstableInt();
	if (!GEventNames.Contains(Index))
	{
		GEventNames.Add(Index);

		FString String = Name.ToString();
		uint32 NameLength = String.Len() + 1;
		static_assert(sizeof(TCHAR) == sizeof(uint16), "");

		UE_TRACE_LOG(GpuProfiler, EventSpec, GpuChannel, NameLength * sizeof(uint16))
			<< EventSpec.EventType(Index)
			<< EventSpec.Name((const uint16*)(*String), NameLength);
	}
}

void FGpuProfilerTrace::BeginEventByName(const FName& Name, uint32 FrameNumber, uint64 TimestampMicroseconds GPU_TRACE_ARG)
{
	using namespace GpuProfilerTrace;

#if RHI_NEW_GPU_PROFILER
	FGpuTraceFrame& GCurrentFrame = GCurrentFrames[GPUIndex];
#endif

	if (!GCurrentFrame.bActive)
	{
		return;
	}

	// Prevent buffer overrun
	if (GCurrentFrame.EventBufferSize + 10 + sizeof(uint32) > GCurrentFrame.MaxEventBufferSize) // 10 is the max size that FTraceUtils::Encode7bit might use + some space for the FName index (uint32)
	{
		UE_LOG(LogRHI, Error, TEXT("GpuProfiler's scratch buffer is out of space for this frame (current size : %d kB). Dropping this frame. The size can be increased dynamically with the console variable r.GpuProfilerMaxEventBufferSizeKB"), GCurrentFrame.MaxEventBufferSize / 1024);

		// Deactivate for the current frame to avoid errors while decoding an incomplete trace
		GCurrentFrame.bActive = false;
		return;
	}

	if (GCurrentFrame.TimestampBase == 0)
	{
		GCurrentFrame.TimestampBase = TimestampMicroseconds;
		GCurrentFrame.LastTimestamp = GCurrentFrame.TimestampBase;
		GCurrentFrame.RenderingFrameNumber = FrameNumber;
		if (!GCurrentFrame.Calibration.GPUMicroseconds)
		{
			GCurrentFrame.Calibration.GPUMicroseconds = TimestampMicroseconds;
		}
	}
	uint8* BufferPtr = GCurrentFrame.EventBuffer + GCurrentFrame.EventBufferSize;
	uint64 TimestampDelta = TimestampMicroseconds - GCurrentFrame.LastTimestamp;
	GCurrentFrame.LastTimestamp = TimestampMicroseconds;
	FTraceUtils::Encode7bit((TimestampDelta << 1ull) | 0x1, BufferPtr);
	*reinterpret_cast<uint32*>(BufferPtr) = uint32(Name.GetComparisonIndex().ToUnstableInt());
	GCurrentFrame.EventBufferSize = BufferPtr - GCurrentFrame.EventBuffer + sizeof(uint32);
}

void FGpuProfilerTrace::EndEvent(uint64 TimestampMicroseconds GPU_TRACE_ARG)
{
	using namespace GpuProfilerTrace;

#if RHI_NEW_GPU_PROFILER
	FGpuTraceFrame& GCurrentFrame = GCurrentFrames[GPUIndex];
#endif

	if (!GCurrentFrame.bActive)
	{
		return;
	}

	// Prevent buffer overrun
	if (GCurrentFrame.EventBufferSize + 10 > GCurrentFrame.MaxEventBufferSize) // 10 is the max size that FTraceUtils::Encode7bit might use
	{
		UE_LOG(LogRHI, Error, TEXT("GpuProfiler's scratch buffer is out of space for this frame (current size : %d kB). Dropping this frame. The size can be increased dynamically with the console variable r.GpuProfilerMaxEventBufferSizeKB"), GCurrentFrame.MaxEventBufferSize / 1024);

		// Deactivate for the current frame to avoid errors while decoding an incomplete trace
		GCurrentFrame.bActive = false;
		return;
	}

	uint64 TimestampDelta = TimestampMicroseconds - GCurrentFrame.LastTimestamp;
	GCurrentFrame.LastTimestamp = TimestampMicroseconds;
	uint8* BufferPtr = GCurrentFrame.EventBuffer + GCurrentFrame.EventBufferSize;
	FTraceUtils::Encode7bit(TimestampDelta << 1ull, BufferPtr);
	GCurrentFrame.EventBufferSize = BufferPtr - GCurrentFrame.EventBuffer;
}

void FGpuProfilerTrace::EndFrame(uint32 GPUIndex)
{
	using namespace GpuProfilerTrace;

#if RHI_NEW_GPU_PROFILER
	FGpuTraceFrame& GCurrentFrame = GCurrentFrames[GPUIndex];
#endif

	if (GCurrentFrame.bActive && GCurrentFrame.EventBufferSize)
	{
		// This subtraction is intended to be performed on uint64 to leverage the wrap around behavior defined by the standard
		uint64 Bias = GCurrentFrame.Calibration.CPUMicroseconds - GCurrentFrame.Calibration.GPUMicroseconds;

		if (GPUIndex == 0)
		{
			UE_TRACE_LOG(GpuProfiler, Frame, GpuChannel)
				<< Frame.CalibrationBias(Bias)
				<< Frame.TimestampBase(GCurrentFrame.TimestampBase)
				<< Frame.RenderingFrameNumber(GCurrentFrame.RenderingFrameNumber)
				<< Frame.Data(GCurrentFrame.EventBuffer, GCurrentFrame.EventBufferSize);
		}
		else if (GPUIndex == 1)
		{
			UE_TRACE_LOG(GpuProfiler, Frame2, GpuChannel)
				<< Frame2.CalibrationBias(Bias)
				<< Frame2.TimestampBase(GCurrentFrame.TimestampBase)
				<< Frame2.RenderingFrameNumber(GCurrentFrame.RenderingFrameNumber)
				<< Frame2.Data(GCurrentFrame.EventBuffer, GCurrentFrame.EventBufferSize);
		}
	}

	GCurrentFrame.EventBufferSize = 0;
	GCurrentFrame.bActive = false;
}

#undef GPU_TRACE_ARG

void FGpuProfilerTrace::Deinitialize()
{
	using namespace GpuProfilerTrace;

#if RHI_NEW_GPU_PROFILER
	for (FGpuTraceFrame& GCurrentFrame : GCurrentFrames)
#endif
	{
		FMemory::Free(GCurrentFrame.EventBuffer);
		GCurrentFrame.EventBuffer = nullptr;
		GCurrentFrame.MaxEventBufferSize = 0;
	}
}

#endif // GPUPROFILERTRACE_ENABLED

namespace UE::RHI::GPUProfiler
{
// Tracing for the new GPU Profiler
#if UE_TRACE_GPU_PROFILER_ENABLED

UE_TRACE_EVENT_BEGIN(GpuProfiler, Init, NoSync | Important)
	UE_TRACE_EVENT_FIELD(uint8, Version)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(GpuProfiler, QueueSpec, NoSync | Important)
	UE_TRACE_EVENT_FIELD(uint32, QueueId)
	UE_TRACE_EVENT_FIELD(UE::Trace::WideString, TypeString)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(GpuProfiler, EventFrameBoundary)
	UE_TRACE_EVENT_FIELD(uint32, QueueId)
	UE_TRACE_EVENT_FIELD(uint32, FrameNumber)
UE_TRACE_EVENT_END()

#if WITH_RHI_BREADCRUMBS

UE_TRACE_EVENT_BEGIN(GpuProfiler, EventBreadcrumbSpec, NoSync | Important)
	UE_TRACE_EVENT_FIELD(uint32, SpecId)
	UE_TRACE_EVENT_FIELD(UE::Trace::WideString, StaticName)
	UE_TRACE_EVENT_FIELD(UE::Trace::WideString, NameFormat)
	UE_TRACE_EVENT_FIELD(uint8[], FieldNames)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(GpuProfiler, EventBeginBreadcrumb)
	UE_TRACE_EVENT_FIELD(uint32, SpecId)
	UE_TRACE_EVENT_FIELD(uint32, QueueId)
	UE_TRACE_EVENT_FIELD(uint64, GPUTimestampTOP)
	UE_TRACE_EVENT_FIELD(uint8[], Metadata)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(GpuProfiler, EventEndBreadcrumb)
	UE_TRACE_EVENT_FIELD(uint32, QueueId)
	UE_TRACE_EVENT_FIELD(uint64, GPUTimestampBOP)
UE_TRACE_EVENT_END()
#endif // WITH_RHI_BREADCRUMBS

UE_TRACE_EVENT_BEGIN(GpuProfiler, EventBeginWork)
	UE_TRACE_EVENT_FIELD(uint32, QueueId)
	UE_TRACE_EVENT_FIELD(uint64, GPUTimestampTOP)
	UE_TRACE_EVENT_FIELD(uint64, CPUTimestamp)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(GpuProfiler, EventEndWork)
	UE_TRACE_EVENT_FIELD(uint32, QueueId)
	UE_TRACE_EVENT_FIELD(uint64, GPUTimestampBOP)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(GpuProfiler, EventWait)
	UE_TRACE_EVENT_FIELD(uint32, QueueId)
	UE_TRACE_EVENT_FIELD(uint64, StartTime)
	UE_TRACE_EVENT_FIELD(uint64, EndTime)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(GpuProfiler, EventStats)
	UE_TRACE_EVENT_FIELD(uint32, QueueId)
	UE_TRACE_EVENT_FIELD(uint32, NumDraws)
	UE_TRACE_EVENT_FIELD(uint32, NumPrimitives)
	// @todo add num dispatches / num vertices stats
	//UE_TRACE_EVENT_FIELD(uint32, NumDispatches)
	//UE_TRACE_EVENT_FIELD(uint32, NumVertices)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(GpuProfiler, SignalFence)
	UE_TRACE_EVENT_FIELD(uint32, QueueId)
	UE_TRACE_EVENT_FIELD(uint64, CPUTimestamp)
	UE_TRACE_EVENT_FIELD(uint64, Value)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(GpuProfiler, WaitFence)
	UE_TRACE_EVENT_FIELD(uint32, QueueId)
	UE_TRACE_EVENT_FIELD(uint64, CPUTimestamp)
	UE_TRACE_EVENT_FIELD(uint32, QueueToWaitForId)
	UE_TRACE_EVENT_FIELD(uint64, Value)
UE_TRACE_EVENT_END()

bool FGpuProfilerTrace::IsAvailable()
{
	return UE_TRACE_CHANNELEXPR_IS_ENABLED(GpuChannel);
}

void FGpuProfilerTrace::Initialize()
{
	static bool bInitialized = false;
	ensure(!bInitialized);

	constexpr uint8 Version = 2;

	UE_TRACE_LOG(GpuProfiler, Init, GpuChannel)
		<< Init.Version(Version);

	bInitialized = true;
}

void FGpuProfilerTrace::InitializeQueue(uint32 QueueId, const TCHAR* Name)
{
	FStringView TypeString(Name);
	UE_TRACE_LOG(GpuProfiler, QueueSpec, GpuChannel, TypeString.Len() * sizeof(TCHAR))
		<< QueueSpec.QueueId(QueueId)
		<< QueueSpec.TypeString(TypeString.GetData(), TypeString.Len());
}

void FGpuProfilerTrace::FrameBoundary(uint32 QueueId, uint32 FrameId)
{
	UE_TRACE_LOG(GpuProfiler, EventFrameBoundary, GpuChannel)
		<< EventFrameBoundary.QueueId(QueueId)
		<< EventFrameBoundary.FrameNumber(FrameId);
}

void FGpuProfilerTrace::BeginWork(uint32 QueueId, uint64 GPUTimestampTOP, uint64 CPUTimestamp)
{
	UE_TRACE_LOG(GpuProfiler, EventBeginWork, GpuChannel)
		<< EventBeginWork.QueueId(QueueId)
		<< EventBeginWork.GPUTimestampTOP(GPUTimestampTOP)
		<< EventBeginWork.CPUTimestamp(CPUTimestamp);
}

void FGpuProfilerTrace::EndWork(uint32 QueueId, uint64 GPUTimestampBOP)
{
	UE_TRACE_LOG(GpuProfiler, EventEndWork, GpuChannel)
		<< EventEndWork.QueueId(QueueId)
		<< EventEndWork.GPUTimestampBOP(GPUTimestampBOP);
}

void FGpuProfilerTrace::TraceWait(uint32 QueueId, uint64 StartTime, uint64 EndTime)
{
	UE_TRACE_LOG(GpuProfiler, EventWait, GpuChannel)
		<< EventWait.QueueId(QueueId)
		<< EventWait.StartTime(StartTime)
		<< EventWait.EndTime(EndTime);
}

void FGpuProfilerTrace::Stats(uint32 QueueId, uint32 NumDraws, uint32 NumPrimitives)
{
	UE_TRACE_LOG(GpuProfiler, EventStats, GpuChannel)
		<< EventStats.QueueId(QueueId)
		<< EventStats.NumDraws(NumDraws)
		<< EventStats.NumPrimitives(NumPrimitives);
}

void FGpuProfilerTrace::SignalFence(uint32 QueueId, uint64 ResolvedTimestamp, uint64 Value)
{
	UE_TRACE_LOG(GpuProfiler, SignalFence, GpuChannel)
		<< SignalFence.QueueId(QueueId)
		<< SignalFence.CPUTimestamp(ResolvedTimestamp)
		<< SignalFence.Value(Value);
}

void FGpuProfilerTrace::WaitFence(uint32 QueueId, uint64 ResolvedTimestamp, uint32 QueueToWaitForId, uint64 Value)
{
	UE_TRACE_LOG(GpuProfiler, WaitFence, GpuChannel)
		<< WaitFence.QueueId(QueueId)
		<< WaitFence.CPUTimestamp(ResolvedTimestamp)
		<< WaitFence.QueueToWaitForId(QueueToWaitForId)
		<< WaitFence.Value(Value);
}

std::atomic<uint32> FGpuProfilerTrace::NextSpecId = 1;

uint32 FGpuProfilerTrace::InternalBreadcrumbSpec(const TCHAR* StaticName, const TCHAR* NameFormat, const TArray<uint8>& FieldNames)
{
	if (!FGpuProfilerTrace::IsAvailable())
	{
		return 0;
	}

	uint32 SpecId = NextSpecId.fetch_add(1);

	uint32 DataSize = FCString::Strlen(StaticName) * sizeof(TCHAR);
	DataSize += FCString::Strlen(NameFormat) * sizeof(TCHAR);
	DataSize += FieldNames.Num() * sizeof(uint8);

#if WITH_RHI_BREADCRUMBS
	UE_TRACE_LOG(GpuProfiler, EventBreadcrumbSpec, GpuChannel, DataSize)
		<< EventBreadcrumbSpec.SpecId(SpecId)
		<< EventBreadcrumbSpec.StaticName(StaticName)
		<< EventBreadcrumbSpec.NameFormat(NameFormat)
		<< EventBreadcrumbSpec.FieldNames(FieldNames.GetData(), FieldNames.Num());
#endif
	return SpecId;
}

void FGpuProfilerTrace::BeginBreadcrumb(uint32 SpecId, uint32 QueueId, uint64 GPUTimestampTOP, const TArray<uint8>& CborData)
{
#if WITH_RHI_BREADCRUMBS
	UE_TRACE_LOG(GpuProfiler, EventBeginBreadcrumb, GpuChannel)
		<< EventBeginBreadcrumb.SpecId(SpecId)
		<< EventBeginBreadcrumb.QueueId(QueueId)
		<< EventBeginBreadcrumb.GPUTimestampTOP(GPUTimestampTOP)
		<< EventBeginBreadcrumb.Metadata(CborData.GetData(), CborData.Num());
#endif
}

void FGpuProfilerTrace::EndBreadcrumb(uint32 QueueId, uint64 GPUTimestampTOP)
{
#if WITH_RHI_BREADCRUMBS
	UE_TRACE_LOG(GpuProfiler, EventEndBreadcrumb, GpuChannel)
		<< EventEndBreadcrumb.QueueId(QueueId)
		<< EventEndBreadcrumb.GPUTimestampBOP(GPUTimestampTOP);
#endif
}

#endif // UE_TRACE_GPU_PROFILER_ENABLED

FMetadataSerializer::FMetadataSerializer()
{
	CborData.Reserve(128);
	MemoryWriter = new FMemoryWriter(CborData, false, true);
	CborWriter = new FCborWriter(MemoryWriter, ECborEndianness::StandardCompliant);
}

FMetadataSerializer::~FMetadataSerializer()
{
	delete CborWriter;
	delete MemoryWriter;
}

void FMetadataSerializer::AppendValue(const ANSICHAR* Value)
{
	CborWriter->WriteValue(Value, FCStringAnsi::Strlen(Value));
}

void FMetadataSerializer::AppendValue(const WIDECHAR* Value)
{
	CborWriter->WriteValue(FWideStringView(Value));
}

void FMetadataSerializer::AppendValue(const UTF8CHAR* Value)
{
	CborWriter->WriteValue((FUtf8StringView)Value);
}

void FMetadataSerializer::AppendValue(uint64 Value)
{
	CborWriter->WriteValue(Value);
}

void FMetadataSerializer::AppendValue(int64 Value)
{
	CborWriter->WriteValue(Value);
}

void FMetadataSerializer::AppendValue(double Value)
{
	CborWriter->WriteValue(Value);
}

void FMetadataSerializer::AppendValue(bool Value)
{
	CborWriter->WriteValue(Value);
}

void FMetadataSerializer::AppendValue(const FName& Value)
{
	CborWriter->WriteValue(Value.ToString());
}

void FMetadataSerializer::AppendValue(const FDebugName& Value)
{
	CborWriter->WriteValue(Value.ToString());
}

void FMetadataSerializer::AppendValue(const FString& Value)
{
	CborWriter->WriteValue(Value);
}

} // namespace UE::RHI::GPUProfiler

