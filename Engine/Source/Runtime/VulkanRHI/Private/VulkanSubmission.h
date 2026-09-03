// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanContext.h: Class to generate Vulkan command buffers from RHI CommandLists
=============================================================================*/

#pragma once 

#include "VulkanRHIPrivate.h"
#include "VulkanQuery.h"

class FVulkanDevice;
class FVulkanDynamicRHI;
class FVulkanPendingGfxState;
class FVulkanPendingComputeState;
class FVulkanQueue;
class FVulkanSwapChain;
class FVulkanTiming;

// NOTE: typedef to FGraphEvent for now, but eventually move all syncs inside a FVulkanSyncPoint class
using FVulkanSyncPoint = FGraphEvent;
using FVulkanSyncPointRef = TRefCountPtr<FVulkanSyncPoint>;

static FVulkanSyncPointRef CreateVulkanSyncPoint()
{
	return FGraphEvent::CreateGraphEvent();
}

class FVulkanPayload
{
	friend class FVulkanCommandListContext;
	friend class FVulkanContextCommon;
	friend class FVulkanDynamicRHI;
	friend class FVulkanQueue;

public:
	FVulkanPayload(FVulkanQueue& InQueue)
		: Queue(InQueue)
#if RHI_NEW_GPU_PROFILER
		, EventStream(InQueue.GetProfilerQueue())
#endif
	{}

	~FVulkanPayload();

protected:

	void PreExecute();

	FVulkanQueue& Queue;
	TArray<VkPipelineStageFlags> WaitFlags;  // flags that match 1:1 with WaitSemaphores
	TArray<VulkanRHI::FSemaphore*> WaitSemaphores; // wait before command buffers
	TArray<FVulkanCommandBuffer*> CommandBuffers;
	TArray<VulkanRHI::FSemaphore*> SignalSemaphores; // signaled after command buffers

	// Signaled when the payload has been submitted to the GPU queue
	TArray<FGraphEventRef> SubmissionEvents;

	// For internal completion tracking of the payload
	uint64 TimelineSemaphoreValue = 0;
	VulkanRHI::FFence* Fence = nullptr;

	// Used to sync other CPU work to payload completion
	TArray<FVulkanSyncPointRef> SyncPoints;

	// Queries used in the command lists of this payload
	TStaticArray<TArray<FVulkanQueryPool*>, (int32)EVulkanQueryPoolType::Count> QueryPools;

	// Used by RHIRunOnQueue
	TFunction<void(VkQueue)> PreExecuteCallback;

#if WITH_RHI_BREADCRUMBS
	FRHIBreadcrumbRange BreadcrumbRange{};
	TSharedPtr<FRHIBreadcrumbAllocatorArray> BreadcrumbAllocators{};
#endif

#if RHI_NEW_GPU_PROFILER
	TOptional<FVulkanTiming*> Timing;
	TOptional<UE::RHI::GPUProfiler::FEvent::FFrameBoundary> EndFrameEvent;
	UE::RHI::GPUProfiler::FEventStream EventStream;
#else
	bool bEndFrame = false;
#endif
};

struct FVulkanPlatformCommandList : public IRHIPlatformCommandList, public TArray<FVulkanPayload*>
{
};

template<>
struct TVulkanResourceTraits<IRHIPlatformCommandList>
{
	typedef FVulkanPlatformCommandList TConcreteType;
};

