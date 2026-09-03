// Copyright Epic Games, Inc. All Rights Reserved.

#include "SkeletalMeshUpdater.h"
#include "GPUSkinCache.h"
#include "SceneInterface.h"
#include "Rendering/RenderCommandPipes.h"
#include "RenderGraphBuilder.h"

bool GUseSkeletalMeshUpdater = true;
static FAutoConsoleVariableRef CVarUseSkeletalMeshUpdater(
	TEXT("r.GPUSkin.UpdateMethod"),
	GUseSkeletalMeshUpdater,
	TEXT("Controls how skeletal mesh updates are pushed to the renderer.\n")
	TEXT(" 0: Use the skeletal mesh render commands. This is the legacy path, which is simpler but can become a bottleneck with large workloads.\n")
	TEXT(" 1: Use the skeletal mesh updater system, which processes and parallelizes the skeletal mesh work more efficiently. (default)\n"),
	ECVF_Default);

///////////////////////////////////////////////////////////////////////////////

void FSkeletalMeshUpdatePacket::Init(FSceneInterface* InScene, FGPUSkinCache* InGPUSkinCache, ERHIPipeline InGPUSkinCachePipeline, const FInitializer& Initializer)
{
	GPUSkinCache = InGPUSkinCache;
	GPUSkinCachePipeline = InGPUSkinCachePipeline;
	Scene = InScene;

#if RHI_RAYTRACING
	bSkinCacheForRayTracingSupported = GPUSkinCache && GEnableGPUSkinCache && FGPUSkinCache::IsGPUSkinCacheRayTracingSupported();
#endif

	Init(Initializer);
}

void FSkeletalMeshUpdatePacket::Finalize()
{
#if RHI_RAYTRACING
	if (bInvalidatePathTracedOutput)
	{
		Scene->InvalidatePathTracedOutput();
	}
#endif
}

///////////////////////////////////////////////////////////////////////////////

FSkeletalMeshUpdateChannel::FBackend::FGlobalList& FSkeletalMeshUpdateChannel::FBackend::GetGlobalList()
{
	static FGlobalList GlobalList;
	return GlobalList;
}

FSkeletalMeshUpdateChannel::FBackend::FBackend()
{
	FGlobalList& GlobalList = GetGlobalList();
	checkf(!GlobalList.NumPipeRefs, TEXT("A backend is being registered while a SkeletalMeshUpdateChannel is active. Only use DEFINE_SKELETAL_MESH_UPDATE_BACKEND to create backends statically."));
	GlobalList.List.AddTail(this);
	GlobalListIndex = GlobalList.Num++;
}

FSkeletalMeshUpdateChannel::FBackend::~FBackend()
{
	checkf(!GetGlobalList().NumPipeRefs, TEXT("A backend is being unregistered while a SkeletalMeshUpdateChannel is still alive."));
	Reset();
}

///////////////////////////////////////////////////////////////////////////////

int32 FSkeletalMeshUpdateChannel::FIndexAllocator::Allocate()
{
	UE::TScopeLock Lock(Mutex);
	if (!FreeList.IsEmpty())
	{
		return FreeList.Pop(EAllowShrinking::No);
	}
	return Max++;
}

void FSkeletalMeshUpdateChannel::FIndexAllocator::Free(int32 Index)
{
	UE::TScopeLock Lock(Mutex);
	FreeList.Push(Index);
}

///////////////////////////////////////////////////////////////////////////////

TArray<FSkeletalMeshUpdateChannel> FSkeletalMeshUpdateChannel::GetChannels()
{
	FBackend::FGlobalList& GlobalList = FBackend::GetGlobalList();

	TArray<FSkeletalMeshUpdateChannel> Channels;
	Channels.Reserve(GlobalList.Num);

	int32 ChannelIndex = 0;
	for (FBackend& Backend : GlobalList.List)
	{
		// Hitting this means something went wrong with static initialization.
		check(ChannelIndex++ == Backend.GlobalListIndex);

		Channels.Emplace(&Backend);
	}

	return Channels;
}

FSkeletalMeshUpdateChannel::FSkeletalMeshUpdateChannel(const FBackend* InBackend)
	: OpQueue(new FOpQueue)
	, Backend(InBackend)
{
	++FBackend::GetGlobalList().NumPipeRefs;
}

FSkeletalMeshUpdateChannel::~FSkeletalMeshUpdateChannel()
{
	--FBackend::GetGlobalList().NumPipeRefs;
	check(!OpQueue);
	check(OpStream.Ops.IsEmpty());

	const int32 NumAllocatedHandles = IndexAllocator.NumAllocated();
	checkf(NumAllocatedHandles == 0, TEXT("FSkeletalMeshUpdateChannel is destructing but still has %d valid handles!"), NumAllocatedHandles);
}

FSkeletalMeshUpdateHandle FSkeletalMeshUpdateChannel::Create(FSkeletalMeshObject* MeshObject)
{
	check(IsInGameThread() || IsInParallelGameThread());

	FSkeletalMeshUpdateHandle Handle;
	Handle.Index = IndexAllocator.Allocate();
	Handle.Channel = this;

	FOp Op;
	Op.HandleIndex = Handle.Index;
	Op.Type = FOp::EType::Add;
	Op.Data_Add.MeshObject = MeshObject;

	check(OpQueue);
	OpQueue->Queue.Enqueue(Op);
	OpQueue->NumAdds.fetch_add(1, std::memory_order_relaxed);
	OpQueue->Num.fetch_add(1, std::memory_order_relaxed);

	return Handle;
}

void FSkeletalMeshUpdateChannel::Release(FSkeletalMeshUpdateHandle&& Handle)
{
	check(IsInGameThread() || IsInParallelGameThread());
	check(Handle.Channel == this);

	FOp Op;
	Op.HandleIndex = Handle.Index;
	Op.Type = FOp::EType::Remove;

	check(OpQueue);
	OpQueue->Queue.Enqueue(Op);
	OpQueue->NumRemoves.fetch_add(1, std::memory_order_relaxed);
	OpQueue->Num.fetch_add(1, std::memory_order_relaxed);

	IndexAllocator.Free(Handle.Index);

	// Clear the channel so that the handle can destruct cleanly.
	Handle.Channel = nullptr;
}

bool FSkeletalMeshUpdateChannel::Update(const FSkeletalMeshUpdateHandle& Handle, FSkeletalMeshDynamicData* MeshDynamicData)
{
	check(IsInGameThread() || IsInParallelGameThread());
	check(MeshDynamicData);
	check(Handle.Channel == this);

	if (GUseSkeletalMeshUpdater)
	{
		FOp Op;
		Op.HandleIndex = Handle.Index;
		Op.Type = FOp::EType::Update;
		Op.Data_Update.MeshDynamicData = MeshDynamicData;
	
		check(OpQueue);
		OpQueue->Queue.Enqueue(Op);
		OpQueue->NumUpdates.fetch_add(1, std::memory_order_relaxed);
		OpQueue->Num.fetch_add(1, std::memory_order_relaxed);
		return true;
	}
	return false;
}

void FSkeletalMeshUpdateChannel::Shutdown()
{
	OpQueue = nullptr;
}

TUniquePtr<FSkeletalMeshUpdateChannel::FOpQueue> FSkeletalMeshUpdateChannel::PopFromQueue()
{
	check(IsInGameThread());

	if (OpQueue->Num.load(std::memory_order_relaxed) == 0)
	{
		return {};
	}

	TUniquePtr<FOpQueue> Data(MoveTemp(OpQueue));
	OpQueue = MakeUnique<FOpQueue>();
	return Data;
}

void FSkeletalMeshUpdateChannel::PushToStream(TUniquePtr<FSkeletalMeshUpdateChannel::FOpQueue>&& InOps)
{
	check(IsInRenderingThread());

	OpStream.NumAdds    += InOps->NumAdds.load(std::memory_order_relaxed);
	OpStream.NumRemoves += InOps->NumRemoves.load(std::memory_order_relaxed);
	OpStream.NumUpdates += InOps->NumUpdates.load(std::memory_order_relaxed);
	OpStream.Num        += InOps->Num.load(std::memory_order_relaxed);
	OpStream.Ops.Reserve(OpStream.Num);

	InOps->Queue.Close([&] (const FOp& Op)
	{
		OpStream.Ops.Emplace(Op);
	});
}

///////////////////////////////////////////////////////////////////////////////

FSkeletalMeshUpdater::FSkeletalMeshUpdater(FSceneInterface* InScene, FGPUSkinCache* InGPUSkinCache)
	: Scene(InScene)
	, GPUSkinCache(InGPUSkinCache)
	, Channels(FSkeletalMeshUpdateChannel::GetChannels())
{
	DelegateHandle = UE::RenderCommandPipe::GetStopRecordingDelegate().AddLambda([this] (const FRenderCommandPipeBitArray&)
	{
		TArray<TPair<FSkeletalMeshUpdateChannel*, TUniquePtr<FSkeletalMeshUpdateChannel::FOpQueue>>, FConcurrentLinearArrayAllocator> ChannelsToPush;
		ChannelsToPush.Reserve(Channels.Num());

		for (FSkeletalMeshUpdateChannel& Channel : Channels)
		{
			if (TUniquePtr<FSkeletalMeshUpdateChannel::FOpQueue> Ops{ Channel.PopFromQueue() }; Ops.IsValid())
			{
				ChannelsToPush.Emplace(&Channel, MoveTemp(Ops));
			}
		}

		if (!Channels.IsEmpty())
		{
			ENQUEUE_RENDER_COMMAND(FSkeletalMeshUpdater_PopFromQueues)([this, ChannelsToPush = MoveTemp(ChannelsToPush)](FRHICommandList& RHICmdList) mutable
			{
				for (auto& KeyValue : ChannelsToPush)
				{
					KeyValue.Key->PushToStream(MoveTemp(KeyValue.Value));
				}
			});
		}
	});
}

void FSkeletalMeshUpdater::Shutdown()
{
	UE::RenderCommandPipe::GetStopRecordingDelegate().Remove(DelegateHandle);

	for (FSkeletalMeshUpdateChannel& Channel : Channels)
	{
		Channel.Shutdown();
	}
}

RDG_REGISTER_BLACKBOARD_STRUCT(FSkeletalMeshUpdater::FSubmitTasks);

struct FSkeletalMeshUpdater::FTaskData
{
	FTaskData(FRDGBuilder& GraphBuilder, ERHIPipeline InGPUSkinCachePipeline)
		: GPUSkinCachePipeline(InGPUSkinCachePipeline)
		, bAsyncCommandList(GraphBuilder.IsParallelSetupEnabled())
	{
		if (bAsyncCommandList)
		{
			RHICmdList = new FRHICommandList;

			FRHICommandListScopedPipeline ScopedPipeline(GraphBuilder.RHICmdList, GPUSkinCachePipeline);
			GraphBuilder.RHICmdList.QueueAsyncCommandListSubmit(RHICmdList);
		}
		else
		{
			RHICmdList = &GraphBuilder.RHICmdList;
		}
	}

	void Begin(int32 NumChannels)
	{
		Packets.Reserve(NumChannels);

		if (bAsyncCommandList)
		{
			RHICmdList->SwitchPipeline(ERHIPipeline::Graphics);
		}

		RHICmdListScopedFence.Emplace(*RHICmdList);
	}

	void End()
	{
		RHICmdListScopedFence.Reset();

		if (bAsyncCommandList)
		{
			RHICmdList->FinishRecording();
		}

		Packets.Empty();
	}

	void ProcessForeground()
	{
		Tasks.Filter.Trigger();

		{
			TRACE_CPUPROFILER_EVENT_SCOPE(Inline);
			for (auto& UpdatePacket : Packets)
			{
				UpdatePacket->ProcessStage_Inline(*RHICmdList);
			}
			Tasks.Inline.Trigger();
		}

		{
			TRACE_CPUPROFILER_EVENT_SCOPE(MeshDeformer);
			for (auto& UpdatePacket : Packets)
			{
				UpdatePacket->ProcessStage_MeshDeformer(*RHICmdList);
			}
			Tasks.MeshDeformer.Trigger();
		}
	
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(GPUSkinCache);
			for (auto& UpdatePacket : Packets)
			{
				UpdatePacket->ProcessStage_SkinCache(*RHICmdList);
			}
		}
	}

	void ProcessUpload()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FSkeletalMeshUpdater::ProcessUpload);

		for (auto& UpdatePacket : Packets)
		{
			UpdatePacket->ProcessStage_Upload(*RHICmdList);
		}
	}

	TArray<TUniquePtr<FSkeletalMeshUpdatePacket>, FConcurrentLinearArrayAllocator> Packets;
	TOptional<FRHICommandListScopedFence> RHICmdListScopedFence;
	FRHICommandList* RHICmdList;
	const ERHIPipeline GPUSkinCachePipeline;
	const bool bAsyncCommandList;

	struct
	{
		UE::Tasks::FTaskEvent Filter{ UE_SOURCE_LOCATION };
		UE::Tasks::FTaskEvent Inline{ UE_SOURCE_LOCATION };
		UE::Tasks::FTaskEvent MeshDeformer{ UE_SOURCE_LOCATION };

	} Tasks;
};

FSkeletalMeshUpdater::FSubmitTasks FSkeletalMeshUpdater::Submit(FRDGBuilder& GraphBuilder, ERHIPipeline GPUSkinCachePipeline)
{
	bool bAllOpsEmpty = true;

	for (FSkeletalMeshUpdateChannel& Channel : Channels)
	{
		if (Channel.OpStream.Num > 0)
		{
			bAllOpsEmpty = false;
			break;
		}
	}

	if (bAllOpsEmpty)
	{
		return {};
	}

	checkf(!bSubmitting, TEXT("Submit was called twice on the same RDG builder. This is not allowed."));
	bSubmitting = true;

	FTaskData& TaskData = *GraphBuilder.AllocObject<FTaskData>(GraphBuilder, GPUSkinCachePipeline);

	UE::Tasks::FTask ForegroundTask = GraphBuilder.AddSetupTask([this, &TaskData]
	{
		TaskData.Begin(Channels.Num());

		for (FSkeletalMeshUpdateChannel& Channel : Channels)
		{
			TUniquePtr<FSkeletalMeshUpdatePacket> Packet = Channel.CreatePacket();

			Packet->Init(Scene, GPUSkinCache, TaskData.GPUSkinCachePipeline, Channel.GetPacketInitializer());
			Channel.Replay(*TaskData.RHICmdList, *Packet);
			Packet->Finalize();

			TaskData.Packets.Emplace(MoveTemp(Packet));
		}

		TaskData.ProcessForeground();

	}, UE::Tasks::ETaskPriority::High);

	GraphBuilder.AddSetupTask([this, &TaskData]
	{
		TaskData.ProcessUpload();
		TaskData.End();

		bSubmitting = false;

	}, ForegroundTask, UE::Tasks::ETaskPriority::BackgroundHigh);

	return GraphBuilder.Blackboard.Create<FSubmitTasks>(FSubmitTasks
	{
		  .Filter       = TaskData.Tasks.Filter
		, .Inline       = TaskData.Tasks.Inline
		, .MeshDeformer = TaskData.Tasks.MeshDeformer
		, .SkinCache    = MoveTemp(ForegroundTask)
	});
}

void FSkeletalMeshUpdater::WaitForStage(FRDGBuilder& GraphBuilder, ESkeletalMeshUpdateStage Stage)
{
	if (FSubmitTasks* SubmitTasks = GraphBuilder.Blackboard.GetMutable<FSubmitTasks>())
	{
		switch (Stage)
		{
		default: checkNoEntry();
		case ESkeletalMeshUpdateStage::Filter:
			SubmitTasks->Filter.Wait();
			SubmitTasks->Filter = {};
			break;
		case ESkeletalMeshUpdateStage::Inline:
			SubmitTasks->Inline.Wait();
			SubmitTasks->Inline = {};
			break;
		case ESkeletalMeshUpdateStage::MeshDeformer:
			SubmitTasks->MeshDeformer.Wait();
			SubmitTasks->MeshDeformer = {};
			break;
		case ESkeletalMeshUpdateStage::SkinCache:
			SubmitTasks->SkinCache.Wait();
			SubmitTasks->SkinCache = {};
			break;
		}
	}
}