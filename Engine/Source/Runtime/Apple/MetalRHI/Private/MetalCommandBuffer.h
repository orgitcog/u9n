// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetalThirdParty.h"
#include "MetalResources.h"
#include "MetalShaderResources.h"
#include "RHIBreadcrumbs.h"
#include "MetalProfiler.h"
#include "MetalCommandQueue.h"

class FMetalCommandBufferTimer;
struct FMetalCounterSample;
class FMetalEventNode;

class FMetalCommandBuffer
{
public:
    FMetalCommandBuffer(MTL::CommandBuffer* InCommandBuffer, FMetalCommandQueue& Queue) 
#if RHI_NEW_GPU_PROFILER
	: EventStream(Queue.GetProfilerQueue())
#endif
    {
        CommandBuffer = InCommandBuffer;
		CommandBuffer->retain();
    }
	
	~FMetalCommandBuffer()
	{
		CommandBuffer->release();
	}
    
    FORCEINLINE MTL::CommandBuffer*& GetMTLCmdBuffer() {return CommandBuffer;}
	
#if RHI_NEW_GPU_PROFILER == 0
	FORCEINLINE void SetTimer(FMetalCommandBufferTimer* InTimer)
	{
		Timer = InTimer;
	}
	
	FORCEINLINE FMetalCommandBufferTimer* GetTimer()
	{
		return Timer;
	}
#endif
	
#if WITH_RHI_BREADCRUMBS
#if RHI_NEW_GPU_PROFILER
	void BeginBreadcrumb(FRHIBreadcrumbNode* Node)
	{
		FMetalBreadcrumbTrackerObject TrackerObject;
		TrackerObject.Type = EMetalBreadcrumbTrackerType::Begin;
		TrackerObject.CmdBuffer = this;
		TrackerObject.Node = Node;
		BreadcrumbTrackerStream.Add(TrackerObject);
	}
	
	void EndBreadcrumb(FRHIBreadcrumbNode* Node)
	{
		FMetalBreadcrumbTrackerObject TrackerObject;
		TrackerObject.Type = EMetalBreadcrumbTrackerType::End;
		TrackerObject.CmdBuffer = this;
		TrackerObject.Node = Node;
		BreadcrumbTrackerStream.Add(TrackerObject);
	}
#endif
#endif
    
	void AddCounterSample(FMetalCounterSamplePtr CounterSample)
	{
		if(!CounterSample)
		{
			return;
		}
		
#if RHI_NEW_GPU_PROFILER == 0
		for(FMetalEventNode* Node : ActiveEventNodes)
		{
			if(EventSampleCounters.Contains(Node))
			{
				EventSampleCounters[Node].Add(CounterSample);
			}
			else
			{
				EventSampleCounters.Add(Node, {CounterSample});
			}
		}
#elif WITH_RHI_BREADCRUMBS
		FMetalBreadcrumbTrackerObject TrackerObject;
		TrackerObject.Type = EMetalBreadcrumbTrackerType::Encode;
		TrackerObject.CmdBuffer = this;
		TrackerObject.CounterSample = CounterSample;
		BreadcrumbTrackerStream.Add(TrackerObject);
#endif
		
		CounterSamples.Add(CounterSample);
	}
	
#if RHI_NEW_GPU_PROFILER
	void SetBeginWorkTimestamp(uint64_t* Timestamp)
	{
		BeginWorkTimestamp = Timestamp;
	}
	
	void SetEndWorkTimestamp(uint64_t* Timestamp)
	{
		EndWorkTimestamp = Timestamp;
	}

	template <typename TEventType, typename... TArgs>
	TEventType& EmplaceProfilerEvent(TArgs&&... Args)
	{
		TEventType& Data = EventStream.Emplace<TEventType>(Forward<TArgs>(Args)...);

		if constexpr (std::is_same_v<UE::RHI::GPUProfiler::FEvent::FBeginWork, TEventType>)
		{
			// Store BeginEvents in a separate array as the CPUTimestamp field needs updating at submit time.
			BeginEvents.Add(&Data);
		}
		
		// Not sure what to do here, ask Luke!
		return Data;
	}
	
	void FlushProfilerEvents(UE::RHI::GPUProfiler::FEventStream& Destination, uint64 CPUTimestamp)
	{
		for (UE::RHI::GPUProfiler::FEvent::FBeginWork* BeginEvent : BeginEvents)
		{
			BeginEvent->CPUTimestamp = CPUTimestamp;
		}
		BeginEvents.Reset();
		Destination.Append(MoveTemp(EventStream));
	}
#endif
	
	TArray<FMetalRHIRenderQuery*> TimestampQueries;
	TArray<FMetalRHIRenderQuery*> OcclusionQueries;
	TArray<FMetalCounterSamplePtr> CounterSamples;
	
#if RHI_NEW_GPU_PROFILER == 0
	TMap<FMetalEventNode*, TArray<FMetalCounterSamplePtr>> EventSampleCounters;
	TArray<FMetalEventNode*> ActiveEventNodes;
#else
	UE::RHI::GPUProfiler::FEventStream EventStream;
	TArray<UE::RHI::GPUProfiler::FEvent::FBeginWork*, TInlineAllocator<8>> BeginEvents;
	
#if WITH_RHI_BREADCRUMBS
	TArray<FMetalBreadcrumbTrackerObject> BreadcrumbTrackerStream;
#endif
	
	uint64_t* BeginWorkTimestamp;
	uint64_t* EndWorkTimestamp;
#endif
	
private:
    MTL::CommandBuffer* CommandBuffer;	
	FMetalCommandBufferTimer* Timer = nullptr;
};
