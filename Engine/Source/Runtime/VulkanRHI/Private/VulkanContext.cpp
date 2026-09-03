// Copyright Epic Games, Inc. All Rights Reserved.

#include "VulkanContext.h"
#include "VulkanCommandBuffer.h"
#include "VulkanMemory.h"
#include "VulkanResources.h"
#include "VulkanRHIPrivate.h"
#include "VulkanDevice.h"
#include "VulkanPendingState.h"
#include "VulkanQuery.h"
#include "DynamicRHI.h"



FVulkanContextCommon::FVulkanContextCommon(FVulkanDevice& InDevice, FVulkanQueue& InQueue, EVulkanCommandBufferType InCommandBufferType)
	: Device(InDevice)
	, Queue(InQueue)
	, Pool(*InQueue.AcquireCommandBufferPool(InCommandBufferType))
{
}

FVulkanContextCommon::~FVulkanContextCommon()
{
	Queue.ReleaseCommandBufferPool(&Pool);
}

void FVulkanContextCommon::NewPayload()
{
	EndPayload();
	Payloads.Add(new FVulkanPayload(Queue));
	CurrentPhase = EPhase::Wait;
}

void FVulkanContextCommon::EndPayload()
{
	if (Payloads.Num() > 0)
	{
		FlushPendingSyncPoints();

		FVulkanPayload* Payload = Payloads.Last();
		if (Payload->CommandBuffers.Num() > 0)
		{
			FVulkanCommandBuffer* CommandBuffer = Payload->CommandBuffers.Last();
			checkSlow(!CommandBuffer->IsSubmitted() && CommandBuffer->HasBegun());

			const bool bIsPrimaryCommandBuffer = (CommandBuffer->GetCommandBufferType() == EVulkanCommandBufferType::Primary);
			if (!CommandBuffer->IsOutsideRenderPass() && bIsPrimaryCommandBuffer)
			{
				UE_LOG(LogVulkanRHI, Warning, TEXT("Forcing EndRenderPass() for submission"));
				CommandBuffer->EndRenderPass();
			}

			// Only record begin/end timestamps on primary command buffers
			FVulkanQueryPool* TimestampQueryPool = 
				GRHIGlobals.SupportsTimestampRenderQueries && bIsPrimaryCommandBuffer ?
				GetCurrentTimestampQueryPool(*Payload) : nullptr;
			CommandBuffer->End(TimestampQueryPool);
		}
	}
}

// Complete recording of the current command list set, and appends the resulting
// payloads to the given array. Resets the context so new commands can be recorded.
void FVulkanContextCommon::Finalize(TArray<FVulkanPayload*>& OutPayloads)
{
	FlushProfilerStats();

	FlushPendingSyncPoints();

	if (ContextSyncPoint.IsValid())
	{
		SignalSyncPoint(ContextSyncPoint);
		ContextSyncPoint = nullptr;
	}

	EndPayload();

	OutPayloads.Append(MoveTemp(Payloads));
}

void FVulkanContextCommon::FlushCommands(EVulkanFlushFlags FlushFlags)
{
	FVulkanSyncPointRef SyncPoint;
	if (EnumHasAnyFlags(FlushFlags, EVulkanFlushFlags::WaitForCompletion))
	{
		SyncPoint = GetContextSyncPoint();
	}

	FGraphEventRef SubmissionEvent;
	if (EnumHasAnyFlags(FlushFlags, EVulkanFlushFlags::WaitForSubmission))
	{
		SubmissionEvent = FGraphEvent::CreateGraphEvent();
		AddSubmissionEvent(SubmissionEvent);
	}

	FVulkanPlatformCommandList* FinalizedPayloads = new FVulkanPlatformCommandList;
	Finalize(*FinalizedPayloads);

	FDynamicRHI::FRHISubmitCommandListsArgs Args;
	Args.CommandLists.Add(FinalizedPayloads);
	FVulkanDynamicRHI::Get().RHISubmitCommandLists(MoveTemp(Args));

	if (SyncPoint)
	{
		FVulkanDynamicRHI::Get().ProcessInterruptQueueUntil(SyncPoint);
	}

	if (SubmissionEvent && !SubmissionEvent->IsComplete())
	{
		SCOPED_NAMED_EVENT_TEXT("Submission_Wait", FColor::Turquoise);
		SubmissionEvent->Wait();
	}
}

#if VULKAN_DELETE_STALE_CMDBUFFERS
struct FRHICommandFreeUnusedCmdBuffers final : public FRHICommand<FRHICommandFreeUnusedCmdBuffers>
{
	FVulkanCommandBufferPool* Pool;
	FVulkanQueue* Queue;
	bool bTrimMemory;

	FRHICommandFreeUnusedCmdBuffers(FVulkanCommandBufferPool* InPool, FVulkanQueue* InQueue, bool bInTrimMemory)
		: Pool(InPool)
		, Queue(InQueue)
		, bTrimMemory(bInTrimMemory)
	{
	}

	void Execute(FRHICommandListBase& CmdList)
	{
		Pool->FreeUnusedCmdBuffers(Queue, bTrimMemory);
	}
};
#endif

void FVulkanContextCommon::FreeUnusedCmdBuffers(bool bTrimMemory)
{
#if VULKAN_DELETE_STALE_CMDBUFFERS
	FRHICommandList& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
	if (!IsInRenderingThread() || (RHICmdList.Bypass() || !IsRunningRHIInSeparateThread()))
	{
		Pool.FreeUnusedCmdBuffers(&Queue, bTrimMemory);
	}
	else
	{
		check(IsInRenderingThread());
		ALLOC_COMMAND_CL(RHICmdList, FRHICommandFreeUnusedCmdBuffers)(&Pool, &Queue, bTrimMemory);
	}
#endif
}

void FVulkanContextCommon::PrepareNewCommandBuffer(FVulkanPayload& Payload)
{
	FScopeLock ScopeLock(&Pool.CS);

	FVulkanCommandBuffer* NewCommandBuffer = nullptr;

	for (int32 Index = 0; Index < Pool.CmdBuffers.Num(); ++Index)
	{
		FVulkanCommandBuffer* CmdBuffer = Pool.CmdBuffers[Index];
		if (CmdBuffer->State == FVulkanCommandBuffer::EState::ReadyForBegin || CmdBuffer->State == FVulkanCommandBuffer::EState::NeedReset)
		{
			NewCommandBuffer = CmdBuffer;
			break;
		}
		else
		{
			check(CmdBuffer->IsSubmitted() || CmdBuffer->HasEnded());
		}
	}

	// All cmd buffers are being executed still, create a new one
	if (!NewCommandBuffer)
	{
		NewCommandBuffer = Pool.Create();
	}

	Payload.CommandBuffers.Add(NewCommandBuffer);

	// Only record begin/end timestamps on primary command buffers
	const bool bIsPrimaryCommandBuffer = (NewCommandBuffer->GetCommandBufferType() == EVulkanCommandBufferType::Primary);
	FVulkanQueryPool* TimestampQueryPool = GRHIGlobals.SupportsTimestampRenderQueries && bIsPrimaryCommandBuffer ?
		GetCurrentTimestampQueryPool(Payload) : nullptr;
	VkRenderPass RenderPassHandle = GetParallelRenderPassInfo() ? GetParallelRenderPassInfo()->RenderPassHandle : VK_NULL_HANDLE;
	NewCommandBuffer->Begin(TimestampQueryPool, RenderPassHandle);
}


FVulkanCommandListContext::FVulkanCommandListContext(FVulkanDevice& InDevice, ERHIPipeline InPipeline, FVulkanCommandListContext* InImmediate)
	: FVulkanContextCommon(InDevice, *InDevice.GetQueue(InPipeline), EVulkanCommandBufferType::Primary)
	, Immediate(InImmediate)
	, RHIPipeline(InPipeline)
	, bSupportsBreadcrumbs(GRHIGlobals.SupportsTimestampRenderQueries)
#if (RHI_NEW_GPU_PROFILER == 0)
	, GpuProfiler(this, &InDevice)
#endif
{
#if (RHI_NEW_GPU_PROFILER == 0)
	FrameTiming = new FVulkanGPUTiming(this, &InDevice);
	FrameTiming->Initialize();
#endif

	// Create Pending state, contains pipeline states such as current shader and etc..
	PendingGfxState = new FVulkanPendingGfxState(Device);
	PendingComputeState = new FVulkanPendingComputeState(Device);

	GlobalUniformBuffers.AddZeroed(FUniformBufferStaticSlotRegistry::Get().GetSlotCount());
}

FVulkanCommandListContext::FVulkanCommandListContext(FVulkanDevice& InDevice, FVulkanCommandListContext* InImmediate, FVulkanParallelRenderPassInfo* InParallelRenderPassInfo)
	: FVulkanContextCommon(InDevice, *InDevice.GetQueue(ERHIPipeline::Graphics), EVulkanCommandBufferType::Secondary)
	, Immediate(InImmediate)
	, RHIPipeline(ERHIPipeline::Graphics)
	, bSupportsBreadcrumbs(false)
	, CurrentParallelRenderPassInfo(InParallelRenderPassInfo)
#if (RHI_NEW_GPU_PROFILER == 0)
	, GpuProfiler(this, &InDevice)
#endif
{
	checkf(CurrentParallelRenderPassInfo, TEXT("Secondary command buffers should be created with a FVulkanParallelRenderPassInfo."));

	// Only graphic commands can be used
	PendingGfxState = new FVulkanPendingGfxState(Device);

	GlobalUniformBuffers.AddZeroed(FUniformBufferStaticSlotRegistry::Get().GetSlotCount());
}


void FVulkanCommandListContext::ReleasePendingState()
{
	delete PendingGfxState;
	PendingGfxState = nullptr;

	delete PendingComputeState;
	PendingComputeState = nullptr;
}

FVulkanCommandListContext::~FVulkanCommandListContext()
{
	if (GSupportsTimestampRenderQueries)
	{
#if (RHI_NEW_GPU_PROFILER == 0)
		if (FrameTiming)
		{
			FrameTiming->Release();
			delete FrameTiming;
			FrameTiming = nullptr;
		}
#endif
	}

	ReleasePendingState();
}

void FVulkanCommandListContext::RHIBeginParallelRenderPass(TSharedPtr<FRHIParallelRenderPassInfo> InInfo, const TCHAR* InName)
{
	checkf(CurrentParallelRenderPassInfo == nullptr, TEXT("There is already a parallel render pass in progress!"));
	CurrentParallelRenderPassInfo = new FVulkanParallelRenderPassInfo();

	RHIBeginRenderPass(*InInfo.Get(), InName);

	CurrentParallelRenderPassInfo->RenderPassHandle = CurrentRenderPass->GetHandle();
	InInfo->RHIPlatformData = CurrentParallelRenderPassInfo;
}

void FVulkanCommandListContext::RHIEndParallelRenderPass()
{
	if (CurrentParallelRenderPassInfo->SecondaryPayloads.Num())
	{
		FVulkanPayload& ParentPayload = GetPayload(EPhase::Execute);
		FVulkanCommandBuffer& ParentCommandBuffer = GetCommandBuffer();

		TArray<VkCommandBuffer> CommandBufferHandles;
		CommandBufferHandles.Reserve(CurrentParallelRenderPassInfo->SecondaryPayloads.Num()); // should be 1 cmdbuffer per payload
		for (FVulkanPayload* Payload : CurrentParallelRenderPassInfo->SecondaryPayloads)
		{
			check(Payload->SignalSemaphores.Num() == 0);
			check(Payload->WaitSemaphores.Num() == 0);

			for (int32 QueryPoolTypeIndex = 0; QueryPoolTypeIndex < (int32)EVulkanQueryPoolType::Count; ++QueryPoolTypeIndex)
			{
				ParentPayload.QueryPools[QueryPoolTypeIndex].Append(MoveTemp(Payload->QueryPools[QueryPoolTypeIndex]));
			}

			for (FVulkanCommandBuffer* SecondaryCommandBuffer : Payload->CommandBuffers)
			{
				CommandBufferHandles.Add(SecondaryCommandBuffer->GetHandle());

#if RHI_NEW_GPU_PROFILER
				// :todo: Flush the event stream if there is one (only use the parent timings for now)
				UE::RHI::GPUProfiler::FEventStream Dummy(Device.GetQueue(EVulkanQueueType::Graphics)->GetProfilerQueue());
				SecondaryCommandBuffer->FlushProfilerEvents(Dummy, FPlatformTime::Cycles64());
#endif
			}
			ParentCommandBuffer.ExecutedSecondaryCommandBuffers.Append(MoveTemp(Payload->CommandBuffers));

			for (const FVulkanSyncPointRef& Sync : Payload->SyncPoints)
			{
				AddPendingSyncPoint(Sync);
			}

			delete Payload;
		}

		if (CommandBufferHandles.Num())
		{
			VulkanRHI::vkCmdExecuteCommands(ParentCommandBuffer.GetHandle(), CommandBufferHandles.Num(), CommandBufferHandles.GetData());
		}
	}

	RHIEndRenderPass();

	delete CurrentParallelRenderPassInfo;
	CurrentParallelRenderPassInfo = nullptr;
}

TLockFreePointerListUnordered<FVulkanUploadContext, PLATFORM_CACHE_LINE_SIZE> FVulkanUploadContext::Pool;
void FVulkanUploadContext::DestroyPool()
{
	while (FVulkanUploadContext* Context = FVulkanUploadContext::Pool.Pop())
	{
		delete Context;
	}
}


FVulkanUploadContext::FVulkanUploadContext(FVulkanDevice& InDevice, FVulkanQueue& InQueue)
	: FVulkanContextCommon(InDevice, InQueue, EVulkanCommandBufferType::Primary)
{

}
FVulkanUploadContext::~FVulkanUploadContext()
{

}

IRHIUploadContext* FVulkanDynamicRHI::RHIGetUploadContext()
{
	FVulkanUploadContext* Context = FVulkanUploadContext::Pool.Pop();
	if (!Context)
	{
		// :todo-jn: locked to graphics queue for now
		Context = new FVulkanUploadContext(*Device, *Device->GetGraphicsQueue());
	}
	return Context;
}