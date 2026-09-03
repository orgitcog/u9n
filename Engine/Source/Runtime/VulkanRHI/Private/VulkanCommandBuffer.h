// Copyright Epic Games, Inc. All Rights Reserved..

/*=============================================================================
	VulkanCommandBuffer.h: Private Vulkan RHI definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "VulkanConfiguration.h"
#include "VulkanBarriers.h"

class FVulkanDevice;
class FVulkanCommandBufferPool;
class FVulkanContextCommon;
class FVulkanRenderTargetLayout;
class FVulkanQueue;
class FVulkanDescriptorPoolSetContainer;
class FVulkanQueryPool;
struct FVulkanBeginRenderPassInfo;

namespace VulkanRHI
{
	class FFence;
	class FSemaphore;
}

enum class EVulkanCommandBufferType : uint8
{
	Primary,
	Secondary,
	Count
};

class FVulkanCommandBuffer
{
protected:
	friend class FVulkanCommandBufferPool;
	friend class FVulkanQueue;

	FVulkanCommandBuffer(FVulkanDevice& InDevice, FVulkanCommandBufferPool& InCommandBufferPool);
	~FVulkanCommandBuffer();

public:
	bool IsInsideRenderPass() const
	{
		return State == EState::IsInsideRenderPass;
	}

	bool IsOutsideRenderPass() const
	{
		return State == EState::IsInsideBegin;
	}

	bool HasBegun() const
	{
		return State == EState::IsInsideBegin || State == EState::IsInsideRenderPass;
	}

	bool HasEnded() const
	{
		return State == EState::HasEnded;
	}

	bool IsSubmitted() const
	{
		return State == EState::Submitted;
	}

	bool IsAllocated() const
	{
		return State != EState::NotAllocated;
	}

	VkCommandBuffer GetHandle() const
	{
		return CommandBufferHandle;
	}

	EVulkanCommandBufferType GetCommandBufferType() const;

	void SetSubmitted();

	void Begin(FVulkanQueryPool* QueryPool, VkRenderPass RenderPassHandle);
	void End(FVulkanQueryPool* QueryPool);

	enum class EState : uint8
	{
		ReadyForBegin,
		IsInsideBegin,
		IsInsideRenderPass,
		HasEnded,
		Submitted,
		NotAllocated,
		NeedReset,
	};

	TArray<VkViewport, TInlineAllocator<2>> CurrentViewports;
	TArray<VkRect2D, TInlineAllocator<2>> CurrentScissors;
	uint32 CurrentStencilRef = 0;
	EState State = EState::NotAllocated;
	uint8 bNeedsDynamicStateSet			: 1 = 1;
	uint8 bHasPipeline					: 1 = 0;
	uint8 bHasViewport					: 1 = 0;
	uint8 bHasScissor					: 1 = 0;
	uint8 bHasStencilRef				: 1 = 0;

	// Every secondary command buffer executed from this one with tied lifetimes
	TArray<FVulkanCommandBuffer*> ExecutedSecondaryCommandBuffers;

	// You never want to call Begin/EndRenderPass directly as it will mess up the layout manager.
	void BeginRenderPass(const FVulkanBeginRenderPassInfo& BeginRenderPassInfo, const VkClearValue* AttachmentClearValues);
	void EndRenderPass();

	// Split barriers
	void BeginSplitBarrier(VkEvent BarrierEvent, const VkDependencyInfo& DependencyInfo);
	void EndSplitBarrier(VkEvent BarrierEvent, const VkDependencyInfo& DependencyInfo);

	//#todo-rco: Hide this
	FVulkanDescriptorPoolSetContainer* CurrentDescriptorPoolSetContainer = nullptr;

	bool AcquirePoolSetAndDescriptorsIfNeeded(const class FVulkanDescriptorSetsLayout& Layout, bool bNeedDescriptors, VkDescriptorSet* OutDescriptors);

#if RHI_NEW_GPU_PROFILER
	template <typename TEventType, typename... TArgs>
	TEventType& EmplaceProfilerEvent(TArgs&&... Args)
	{
		TEventType& Data = EventStream.Emplace<TEventType>(Forward<TArgs>(Args)...);

		if constexpr (std::is_same_v<UE::RHI::GPUProfiler::FEvent::FBeginWork, TEventType>)
		{
			// Store BeginEvents in a separate array as the CPUTimestamp field needs updating at submit time.
			BeginEvents.Add(&Data);
		}

		return Data;
	}

	void FlushProfilerEvents(UE::RHI::GPUProfiler::FEventStream& Destination, uint64 CPUTimestamp)
	{
		for (UE::RHI::GPUProfiler::FEvent::FBeginWork* BeginEvent : BeginEvents)
		{
			checkSlow((BeginEvent->CPUTimestamp == 0) && (BeginEvent->GPUTimestampTOP == UINT64_MAX));
			BeginEvent->CPUTimestamp = CPUTimestamp;
		}
		BeginEvents.Reset();

		Destination.Append(MoveTemp(EventStream));
	}
#else
	uint64 GetBusyCycles() const
	{
		if (EndTimestamp > StartTimestamp)
		{
			return (EndTimestamp - StartTimestamp);
		}
		return 0;
	}
#endif

	// Public immutable reference to be accessed directly
	FVulkanDevice& Device;

private:
	VkCommandBuffer CommandBufferHandle = VK_NULL_HANDLE;
	double SubmittedTime = 0.0f;

	void Reset();

	FVulkanCommandBufferPool& CommandBufferPool;

	TArray<VkEvent> EndedBarrierEvents;

	void AcquirePoolSetContainer();

	void AllocMemory();
	void FreeMemory();

#if RHI_NEW_GPU_PROFILER
	UE::RHI::GPUProfiler::FEventStream EventStream;
	TArray<UE::RHI::GPUProfiler::FEvent::FBeginWork*, TInlineAllocator<8>> BeginEvents;
#else
	uint64 StartTimestamp = 0;
	uint64 EndTimestamp = 0;
#endif

public:
	//#todo-rco: Hide this
	TMap<uint32, class FVulkanTypedDescriptorPoolSet*> TypedDescriptorPoolSets;

	friend class FVulkanDynamicRHI;
	friend FVulkanContextCommon;
};

class FVulkanCommandBufferPool
{
public:
	FVulkanCommandBufferPool(FVulkanDevice& InDevice, FVulkanQueue& InQueue, EVulkanCommandBufferType InCommandBufferType);
	~FVulkanCommandBufferPool();

	VkCommandPool GetHandle() const
	{
		return Handle;
	}

	FCriticalSection* GetCS()
	{
		return &CS;
	}

	FVulkanQueue& GetQueue()
	{
		return Queue;
	}

	EVulkanCommandBufferType GetCommandBufferType() const
	{
		return CommandBufferType;
	}

	void FreeUnusedCmdBuffers(FVulkanQueue* Queue, bool bTrimMemory);

private:
	FVulkanDevice& Device;
	FVulkanQueue& Queue;

	const EVulkanCommandBufferType CommandBufferType;

	VkCommandPool Handle = VK_NULL_HANDLE;

	TArray<FVulkanCommandBuffer*> CmdBuffers;
	TArray<FVulkanCommandBuffer*> FreeCmdBuffers;

	FCriticalSection CS;

	FVulkanCommandBuffer* Create();

	friend FVulkanContextCommon;
};


inline EVulkanCommandBufferType FVulkanCommandBuffer::GetCommandBufferType() const
{
	return CommandBufferPool.GetCommandBufferType();
}
