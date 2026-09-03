// Copyright Epic Games, Inc. All Rights Reserved.

#include "D3D12CommandList.h"
#include "D3D12RHIPrivate.h"
#include "RHIValidation.h"

static int32 GD3D12BatchResourceBarriers = 1;
static FAutoConsoleVariableRef CVarD3D12BatchResourceBarriers(
	TEXT("d3d12.BatchResourceBarriers"),
	GD3D12BatchResourceBarriers,
	TEXT("Whether to allow batching resource barriers"));

static int32 GD3D12ExtraDepthTransitions = 0;
static FAutoConsoleVariableRef CVarD3D12ExtraDepthTransitions(
	TEXT("d3d12.ExtraDepthTransitions"),
	GD3D12ExtraDepthTransitions,
	TEXT("Adds extra transitions for the depth buffer to fix validation issues. However, this currently breaks async compute"));

void FD3D12CommandList::UpdateResidency(const FD3D12Resource* Resource)
{
#if ENABLE_RESIDENCY_MANAGEMENT
	if (Resource->NeedsDeferredResidencyUpdate())
	{
		State.DeferredResidencyUpdateSet.Add(Resource);
	}
	else
	{
		AddToResidencySet(Resource->GetResidencyHandles());
	}
#endif // ENABLE_RESIDENCY_MANAGEMENT
}

#if ENABLE_RESIDENCY_MANAGEMENT
FD3D12ResidencySet* FD3D12CommandList::CloseResidencySet()
{
	for (const FD3D12Resource* Resource : State.DeferredResidencyUpdateSet)
	{
		AddToResidencySet(Resource->GetResidencyHandles());
	}

	if (State.DeferredResidencyUpdateSet.Num() > 0)
	{
		D3DX12Residency::Close(ResidencySet);
	}

	return ResidencySet;
}

void FD3D12CommandList::AddToResidencySet(TConstArrayView<FD3D12ResidencyHandle*> ResidencyHandles)
{
	for (FD3D12ResidencyHandle* Handle : ResidencyHandles)
	{
		if (D3DX12Residency::IsInitialized(Handle))
		{
#if DO_CHECK
			check(Device->GetGPUMask() == Handle->GPUObject->GetGPUMask());
#endif
			D3DX12Residency::Insert(*ResidencySet, *Handle);
		}
	}
}
#endif // ENABLE_RESIDENCY_MANAGEMENT


void FD3D12ContextCommon::AddTransitionBarrier(FD3D12Resource* pResource, D3D12_RESOURCE_STATES Before, D3D12_RESOURCE_STATES After, uint32 Subresource)
{
	if (Before != After)
	{
		ResourceBarrierBatcher.AddTransition(pResource, Before, After, Subresource);

		UpdateResidency(pResource);

		if (!GD3D12BatchResourceBarriers)
		{
			FlushResourceBarriers();
		}
	}
	else
	{
		ensureMsgf(0, TEXT("AddTransitionBarrier: Before == After (%d)"), (uint32)Before);
	}
}

void FD3D12ContextCommon::AddUAVBarrier()
{
	ResourceBarrierBatcher.AddUAV();

	if (!GD3D12BatchResourceBarriers)
	{
		FlushResourceBarriers();
	}
}

void FD3D12ContextCommon::AddAliasingBarrier(ID3D12Resource* InResourceBefore, ID3D12Resource* InResourceAfter)
{
	ResourceBarrierBatcher.AddAliasingBarrier(InResourceBefore, InResourceAfter);

	if (!GD3D12BatchResourceBarriers)
	{
		FlushResourceBarriers();
	}
}


void FD3D12ContextCommon::FlushResourceBarriers()
{
	if (ResourceBarrierBatcher.Num())
	{
		ResourceBarrierBatcher.FlushIntoCommandList(GetCommandList(), TimestampQueries);
	}
}

FD3D12CommandAllocator::FD3D12CommandAllocator(FD3D12Device* Device, ED3D12QueueType QueueType)
	: Device(Device)
	, QueueType(QueueType)
{
	VERIFYD3D12RESULT(Device->GetDevice()->CreateCommandAllocator(GetD3DCommandListType(QueueType), IID_PPV_ARGS(CommandAllocator.GetInitReference())));
	INC_DWORD_STAT(STAT_D3D12NumCommandAllocators);
}

FD3D12CommandAllocator::~FD3D12CommandAllocator()
{
	CommandAllocator.SafeRelease();
	DEC_DWORD_STAT(STAT_D3D12NumCommandAllocators);
}

void FD3D12CommandAllocator::Reset()
{
	VERIFYD3D12RESULT(CommandAllocator->Reset());
}

FD3D12CommandList::FD3D12CommandList(FD3D12CommandAllocator* CommandAllocator, FD3D12QueryAllocator* TimestampAllocator, FD3D12QueryAllocator* PipelineStatsAllocator)
	: Device(CommandAllocator->Device)
	, QueueType(CommandAllocator->QueueType)
	, ResidencySet(D3DX12Residency::CreateResidencySet(Device->GetResidencyManager()))
	, State(CommandAllocator, TimestampAllocator, PipelineStatsAllocator)
{
	switch (QueueType)
	{
	case ED3D12QueueType::Direct:
	case ED3D12QueueType::Async:
		VERIFYD3D12RESULT(Device->CreateCommandList(
			Device->GetGPUMask().GetNative(),
			GetD3DCommandListType(QueueType),
			*CommandAllocator,
			nullptr,
			IID_PPV_ARGS(Interfaces.GraphicsCommandList.GetInitReference())
		));
		Interfaces.CommandList = Interfaces.GraphicsCommandList;

		Interfaces.CommandList->QueryInterface(IID_PPV_ARGS(Interfaces.CopyCommandList.GetInitReference()));

		// Optionally obtain the versioned ID3D12GraphicsCommandList[0-9]+ interfaces, we don't check the HRESULT.
#if D3D12_MAX_COMMANDLIST_INTERFACE >= 1
		Interfaces.CommandList->QueryInterface(IID_PPV_ARGS(Interfaces.GraphicsCommandList1.GetInitReference()));
#endif
#if D3D12_MAX_COMMANDLIST_INTERFACE >= 2
		Interfaces.CommandList->QueryInterface(IID_PPV_ARGS(Interfaces.GraphicsCommandList2.GetInitReference()));
#endif
#if D3D12_MAX_COMMANDLIST_INTERFACE >= 3
		Interfaces.CommandList->QueryInterface(IID_PPV_ARGS(Interfaces.GraphicsCommandList3.GetInitReference()));
#endif
#if D3D12_MAX_COMMANDLIST_INTERFACE >= 4
		Interfaces.CommandList->QueryInterface(IID_PPV_ARGS(Interfaces.GraphicsCommandList4.GetInitReference()));
#endif
#if D3D12_MAX_COMMANDLIST_INTERFACE >= 5
		Interfaces.CommandList->QueryInterface(IID_PPV_ARGS(Interfaces.GraphicsCommandList5.GetInitReference()));
#endif
#if D3D12_MAX_COMMANDLIST_INTERFACE >= 6
		Interfaces.CommandList->QueryInterface(IID_PPV_ARGS(Interfaces.GraphicsCommandList6.GetInitReference()));
#endif
#if D3D12_MAX_COMMANDLIST_INTERFACE >= 7
		Interfaces.CommandList->QueryInterface(IID_PPV_ARGS(Interfaces.GraphicsCommandList7.GetInitReference()));
#endif
#if D3D12_MAX_COMMANDLIST_INTERFACE >= 8
		Interfaces.CommandList->QueryInterface(IID_PPV_ARGS(Interfaces.GraphicsCommandList8.GetInitReference()));
#endif
#if D3D12_MAX_COMMANDLIST_INTERFACE >= 9
		Interfaces.CommandList->QueryInterface(IID_PPV_ARGS(Interfaces.GraphicsCommandList9.GetInitReference()));
#endif
#if D3D12_MAX_COMMANDLIST_INTERFACE >= 10
		Interfaces.CommandList->QueryInterface(IID_PPV_ARGS(Interfaces.GraphicsCommandList10.GetInitReference()));
#endif

#if D3D12_SUPPORTS_DEBUG_COMMAND_LIST
		Interfaces.CommandList->QueryInterface(IID_PPV_ARGS(Interfaces.DebugCommandList.GetInitReference()));
#endif
		break;

	case ED3D12QueueType::Copy:
		VERIFYD3D12RESULT(Device->GetDevice()->CreateCommandList(
			Device->GetGPUMask().GetNative(),
			GetD3DCommandListType(QueueType),
			*CommandAllocator,
			nullptr,
			IID_PPV_ARGS(Interfaces.CopyCommandList.GetInitReference())
		));
		Interfaces.CommandList = Interfaces.CopyCommandList;

		break;

	default:
		checkNoEntry();
		return;
	}

	INC_DWORD_STAT(STAT_D3D12NumCommandLists);

#if NV_AFTERMATH
	Interfaces.AftermathHandle = UE::RHICore::Nvidia::Aftermath::D3D12::RegisterCommandList(Interfaces.CommandList);
#endif

#if INTEL_GPU_CRASH_DUMPS
	Interfaces.IntelCommandListHandle = UE::RHICore::Intel::GPUCrashDumps::D3D12::RegisterCommandList( Interfaces.GraphicsCommandList );
#endif

#if NAME_OBJECTS
	FString Name = FString::Printf(TEXT("FD3D12CommandList (GPU %d)"), Device->GetGPUIndex());
	SetName(Interfaces.CommandList, Name.GetCharArray().GetData());
#endif

	D3DX12Residency::Open(ResidencySet);
	BeginLocalQueries();
}

FD3D12CommandList::~FD3D12CommandList()
{
	D3DX12Residency::DestroyResidencySet(Device->GetResidencyManager(), ResidencySet);

#if NV_AFTERMATH
	UE::RHICore::Nvidia::Aftermath::D3D12::UnregisterCommandList(Interfaces.AftermathHandle);
#endif

	DEC_DWORD_STAT(STAT_D3D12NumCommandLists);
}

void FD3D12CommandList::Reset(FD3D12CommandAllocator* NewCommandAllocator, FD3D12QueryAllocator* TimestampAllocator, FD3D12QueryAllocator* PipelineStatsAllocator)
{
	check(IsClosed());
	check(NewCommandAllocator->Device == Device && NewCommandAllocator->QueueType == QueueType);
	if (Interfaces.CopyCommandList)
	{
		VERIFYD3D12RESULT(Interfaces.CopyCommandList->Reset(*NewCommandAllocator, nullptr));
	}
	else
	{
		VERIFYD3D12RESULT(Interfaces.GraphicsCommandList->Reset(*NewCommandAllocator, nullptr));
	}
	D3DX12Residency::Open(ResidencySet);

	(&State)->~FState();
	new (&State) FState(NewCommandAllocator, TimestampAllocator, PipelineStatsAllocator);

	BeginLocalQueries();
}

void FD3D12CommandList::Close()
{
	check(IsOpen());
	EndLocalQueries();

	HRESULT hr;
	if (Interfaces.CopyCommandList)
	{
		hr = Interfaces.CopyCommandList->Close();
	}
	else
	{
		hr = Interfaces.GraphicsCommandList->Close();
	}

#if DEBUG_RESOURCE_STATES
	if (hr != S_OK)
		LogResourceBarriers(State.ResourceBarriers, Interfaces.CommandList.GetReference(), ED3D12QueueType::Direct , FString(DX12_RESOURCE_NAME_TO_LOG));
#endif

	VERIFYD3D12RESULT(hr);

	if (State.DeferredResidencyUpdateSet.Num() == 0)
	{
		D3DX12Residency::Close(ResidencySet);
	}

	State.IsClosed = true;
}

void FD3D12CommandList::BeginLocalQueries()
{
#if DO_CHECK
	check(!State.bLocalQueriesBegun);
	State.bLocalQueriesBegun = true;
#endif

	if (State.BeginTimestamp)
	{
#if RHI_NEW_GPU_PROFILER
		// CPUTimestamp is filled in at submission time in FlushProfilerEvents
		auto& Event = EmplaceProfilerEvent<UE::RHI::GPUProfiler::FEvent::FBeginWork>(0);
		State.BeginTimestamp.Target = &Event.GPUTimestampTOP;
#endif

		EndQuery(State.BeginTimestamp);
	}

	if (State.PipelineStats)
	{
		BeginQuery(State.PipelineStats);
	}
}

void FD3D12CommandList::EndLocalQueries()
{
#if DO_CHECK
	check(!State.bLocalQueriesEnded);
	State.bLocalQueriesEnded = true;
#endif

	if (State.PipelineStats)
	{
		EndQuery(State.PipelineStats);
	}

	if (State.EndTimestamp)
	{
#if RHI_NEW_GPU_PROFILER
		auto& Event = EmplaceProfilerEvent<UE::RHI::GPUProfiler::FEvent::FEndWork>();
		State.EndTimestamp.Target = &Event.GPUTimestampBOP;
#endif

		EndQuery(State.EndTimestamp);
	}
}

void FD3D12CommandList::BeginQuery(FD3D12QueryLocation const& Location)
{
	check(Location);
	check(Location.Heap->QueryType == D3D12_QUERY_TYPE_OCCLUSION || Location.Heap->QueryType == D3D12_QUERY_TYPE_PIPELINE_STATISTICS);

	GraphicsCommandList()->BeginQuery(
		Location.Heap->GetD3DQueryHeap(),
		Location.Heap->QueryType,
		Location.Index
	);
}

void FD3D12CommandList::EndQuery(FD3D12QueryLocation const& Location)
{
	check(Location);
	switch (Location.Heap->QueryType)
	{
	default:
		checkNoEntry();
		break;

	case D3D12_QUERY_TYPE_PIPELINE_STATISTICS:
		GraphicsCommandList()->EndQuery(
			Location.Heap->GetD3DQueryHeap(),
			Location.Heap->QueryType,
			Location.Index
		);
		State.PipelineStatsQueries.Add(Location);
		break;

	case D3D12_QUERY_TYPE_OCCLUSION:
		GraphicsCommandList()->EndQuery(
			Location.Heap->GetD3DQueryHeap(),
			Location.Heap->QueryType,
			Location.Index
		);
		State.OcclusionQueries.Add(Location);
		break;

	case D3D12_QUERY_TYPE_TIMESTAMP:
		{
			ED3D12QueryPosition Position;
			switch (Location.Type)
			{
			default:
				checkf(false, TEXT("Query location type is not a top or bottom of pipe timestamp."));
				Position = ED3D12QueryPosition::BottomOfPipe;
				break;

#if RHI_NEW_GPU_PROFILER
			case ED3D12QueryType::ProfilerTimestampTOP:
#else
			case ED3D12QueryType::CommandListBegin:
			case ED3D12QueryType::IdleBegin:
#endif
				Position = ED3D12QueryPosition::TopOfPipe;
				break;

			case ED3D12QueryType::TimestampMicroseconds:
			case ED3D12QueryType::TimestampRaw:
#if RHI_NEW_GPU_PROFILER
			case ED3D12QueryType::ProfilerTimestampBOP:
#else
			case ED3D12QueryType::CommandListEnd:
			case ED3D12QueryType::IdleEnd:
#endif
				Position = ED3D12QueryPosition::BottomOfPipe;
				break;
			}

			WriteTimestamp(Location, Position);

#if RHI_NEW_GPU_PROFILER == 0
			// Command list begin/end timestamps are handled separately by the 
			// submission thread, so shouldn't be in the TimestampQueries array.
			if (Location.Type != ED3D12QueryType::CommandListBegin && Location.Type != ED3D12QueryType::CommandListEnd)
#endif
			{
				State.TimestampQueries.Add(Location);
			}
		}
		break;
	}
}

#if D3D12RHI_PLATFORM_USES_TIMESTAMP_QUERIES
void FD3D12CommandList::WriteTimestamp(FD3D12QueryLocation const& Location, ED3D12QueryPosition Position)
{
	GraphicsCommandList()->EndQuery(
		Location.Heap->GetD3DQueryHeap(),
		Location.Heap->QueryType,
		Location.Index
	);
}
#endif // D3D12RHI_PLATFORM_USES_TIMESTAMP_QUERIES

FD3D12CommandList::FState::FState(FD3D12CommandAllocator* CommandAllocator, FD3D12QueryAllocator* TimestampAllocator, FD3D12QueryAllocator* PipelineStatsAllocator)
	: CommandAllocator(CommandAllocator)
#if RHI_NEW_GPU_PROFILER
	, EventStream(CommandAllocator->Device->GetQueue(CommandAllocator->QueueType).GetProfilerQueue())
#endif
{
	if (TimestampAllocator)
	{
#if RHI_NEW_GPU_PROFILER
		BeginTimestamp = TimestampAllocator->Allocate(ED3D12QueryType::ProfilerTimestampTOP, nullptr);
		EndTimestamp   = TimestampAllocator->Allocate(ED3D12QueryType::ProfilerTimestampBOP, nullptr);
#else
		BeginTimestamp = TimestampAllocator->Allocate(ED3D12QueryType::CommandListBegin, nullptr);
		EndTimestamp   = TimestampAllocator->Allocate(ED3D12QueryType::CommandListEnd  , nullptr);
#endif
	}

	if (PipelineStatsAllocator)
	{
		PipelineStats = PipelineStatsAllocator->Allocate(ED3D12QueryType::PipelineStats, nullptr);
	}
}

bool FD3D12ContextCommon::TransitionResource(FD3D12Resource* Resource, D3D12_RESOURCE_STATES Before, D3D12_RESOURCE_STATES After, uint32 Subresource)
{
	check(Resource);
	check(Resource->RequiresResourceStateTracking());
	check(!((After & (D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)) && (Resource->GetDesc().Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE)));
	check(Before != D3D12_RESOURCE_STATE_TBD);
	check(After != D3D12_RESOURCE_STATE_TBD);

#ifdef PLATFORM_SUPPORTS_RESOURCE_COMPRESSION
	After |= Resource->GetCompressedState();
#endif
#if DEBUG_RESOURCE_STATES
	FString IncompatibilityReason;
	if (CheckResourceStateCompatibility(After, Resource->GetDesc().Flags, IncompatibilityReason) == false)
	{
		UE_LOG(LogRHI, Error, TEXT("Incompatible Transition State for Resource %s - %s"), *Resource->GetName().ToString(), *IncompatibilityReason);
	}
#endif
	UpdateResidency(Resource);

	bool bRequireUAVBarrier = false;

	bRequireUAVBarrier = TransitionResource(Resource, Subresource, Before, After);

	return bRequireUAVBarrier;
}

static inline bool IsTransitionNeeded(D3D12_RESOURCE_STATES Before, D3D12_RESOURCE_STATES& After, FD3D12Resource* InResource = nullptr)
{
	check(Before != D3D12_RESOURCE_STATE_CORRUPT && After != D3D12_RESOURCE_STATE_CORRUPT);
	check(Before != D3D12_RESOURCE_STATE_TBD && After != D3D12_RESOURCE_STATE_TBD);

	// COMMON is an oddball state that doesn't follow the RESOURE_STATE pattern of 
	// having exactly one bit set so we need to special case these
	if (After == D3D12_RESOURCE_STATE_COMMON)
	{
		// Before state should not have the common state otherwise it's invalid transition
		check(Before != D3D12_RESOURCE_STATE_COMMON);
		return true;
	}

	return Before != After;
}

bool FD3D12ContextCommon::TransitionResource(FD3D12Resource* InResource, uint32 InSubresourceIndex, D3D12_RESOURCE_STATES InBeforeState, D3D12_RESOURCE_STATES InAfterState)
{
	check(InBeforeState != D3D12_RESOURCE_STATE_TBD);
	check(InAfterState != D3D12_RESOURCE_STATE_TBD);

	D3D12_RESOURCE_STATES BeforeState = InBeforeState;
	D3D12_RESOURCE_STATES AfterState = InAfterState;

	bool bRequireUAVBarrier = false;

	// Require UAV barrier when before and after are UAV
	if (BeforeState == D3D12_RESOURCE_STATE_UNORDERED_ACCESS && InAfterState == D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
	{
		bRequireUAVBarrier = true;
	}
	// Special case for UAV access resources
	else if (InResource->GetUAVAccessResource() && EnumHasAnyFlags(BeforeState | InAfterState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS))
	{
		// We are issuing an Aliasing Barrier from or to GetUAVAccessResource
		// while we are transitioning to a known resource state (D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) the original resource since it can't go int UAV
		// the transition into D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE is not needed but in this way the tracking of the resource will be consistent at the higher level

		// inject an aliasing barrier
		const bool bFromUAV = EnumHasAnyFlags(BeforeState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		const bool bToUAV = EnumHasAnyFlags(InAfterState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		check(bFromUAV != bToUAV);

		if (bToUAV)
		{
			AddAliasingBarrier(InResource->GetResource(), InResource->GetUAVAccessResource());
			AfterState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE; //original resource CAN'T go into D3D12_RESOURCE_STATE_UNORDERED_ACCESS so we transition it in a known state D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
		}
		else
		{
			AddAliasingBarrier(InResource->GetUAVAccessResource(), InResource->GetResource());
			BeforeState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE; //original resource CAN'T be into D3D12_RESOURCE_STATE_UNORDERED_ACCESS so it was transitioned to D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
		}
	}

	if (IsTransitionNeeded(BeforeState, AfterState, InResource))
	{
		AddTransitionBarrier(InResource, BeforeState, AfterState, InSubresourceIndex);
	}

	return bRequireUAVBarrier;
}

namespace D3D12RHI
{
	void GetGfxCommandListAndQueue(FRHICommandList& RHICmdList, void*& OutGfxCmdList, void*& OutCommandQueue)
	{
		IRHICommandContext& RHICmdContext = RHICmdList.GetContext();
		FD3D12CommandContext& CmdContext = static_cast<FD3D12CommandContext&>(RHICmdContext);
		check(CmdContext.IsDefaultContext());

		OutGfxCmdList = CmdContext.GraphicsCommandList().Get();
		OutCommandQueue = CmdContext.Device->GetQueue(CmdContext.QueueType).D3DCommandQueue;
	}
}
