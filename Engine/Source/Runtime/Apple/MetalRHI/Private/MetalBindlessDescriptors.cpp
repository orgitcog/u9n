// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetalBindlessDescriptors.h"
#include "GlobalShader.h"
#include "PipelineStateCache.h"
#include "MetalRHIContext.h"
#include "UpdateDescriptorHandle.h"

#if PLATFORM_SUPPORTS_BINDLESS_RENDERING

#include "MetalDevice.h"
#include "MetalCommandEncoder.h"
#include "MetalDynamicRHI.h"

int32 GBindlessResourceDescriptorHeapSize = 2048 * 1024;
static FAutoConsoleVariableRef CVarBindlessResourceDescriptorHeapSize(
	TEXT("Metal.Bindless.ResourceDescriptorHeapSize"),
	GBindlessResourceDescriptorHeapSize,
	TEXT("Bindless resource descriptor heap size"),
	ECVF_ReadOnly
);

int32 GBindlessSamplerDescriptorHeapSize = 64 << 10; // TODO: We should be able to reduce the size of the sampler heap if we fix static sampler creation.
static FAutoConsoleVariableRef CVarBindlessSamplerDescriptorHeapSize(
	TEXT("Metal.Bindless.SamplerDescriptorHeapSize"),
	GBindlessSamplerDescriptorHeapSize,
	TEXT("Bindless sampler descriptor heap size"),
	ECVF_ReadOnly
);

static int32_t GMetalBindlessUpdateDescriptorOnCompute = 1;
static FAutoConsoleVariableRef CVarBindlessUpdateDescriptorOnCompute(
	TEXT("Metal.Bindless.UpdateDescriptorOnCompute"),
	GMetalBindlessUpdateDescriptorOnCompute,
	TEXT("How to update descriptors on the GPU")
	TEXT("0 = Use Copy (slow), 1 = Use Compute shader (default)"),
	ECVF_ReadOnly
);

FMetalDescriptorHeap::FMetalDescriptorHeap(FMetalDevice& MetalDevice, const ERHIDescriptorHeapType DescriptorType)
	: Device(MetalDevice)
	, ResourceHeap(nullptr)
	, Type(DescriptorType)
{
}

FMetalDescriptorHeap::~FMetalDescriptorHeap()
{
}

void FMetalDescriptorHeap::Init(FRHICommandListBase& RHICmdList, const int32 HeapSize)
{
	const FRHIBufferCreateDesc CreateDesc =
		FRHIBufferCreateDesc::CreateStructured(TEXT("ResourceHeap"), HeapSize, 1)
		.AddUsage(EBufferUsageFlags::Dynamic | EBufferUsageFlags::KeepCPUAccessible | EBufferUsageFlags::UnorderedAccess | EBufferUsageFlags::StructuredBuffer)
		.DetermineInitialState();
	
	ResourceHeapLength = HeapSize;
	ResourceHeap = new FMetalRHIBuffer(RHICmdList, Device, CreateDesc, nullptr);
	FMetalRHIBuffer* Buffer = ResourceCast(ResourceHeap.GetReference());
	
	Descriptors = reinterpret_cast<IRDescriptorTableEntry*>(Buffer->GetCurrentBuffer()->Contents());
}

void FMetalDescriptorHeap::FreeDescriptor(FRHIDescriptorHandle DescriptorHandle)
{
	FMetalDynamicRHI::Get().DeferredDelete([this, DescriptorHandle]()
	{
		FScopeLock ScopeLock(&FreeListCS);	
		FreeList.Enqueue(DescriptorHandle.GetIndex());
	});
}

uint32 FMetalDescriptorHeap::GetFreeResourceIndex()
{
	{
		FScopeLock ScopeLock(&FreeListCS);
		if (!FreeList.IsEmpty())
		{
			uint32 FreeIndex;
			FreeList.Dequeue(FreeIndex);
			return FreeIndex;
		}
	}

	NSUInteger MaxDescriptorCount = ResourceHeapLength / sizeof(IRDescriptorTableEntry);
	checkf((PeakDescriptorCount + 1) < MaxDescriptorCount, TEXT("Reached Heap Max Capacity (%u/%u)"), PeakDescriptorCount + 1, MaxDescriptorCount);

	const uint32 ResourceIndex = PeakDescriptorCount++;
	return ResourceIndex;
}

FRHIDescriptorHandle FMetalDescriptorHeap::ReserveDescriptor()
{
	const uint32 ResourceIndex = GetFreeResourceIndex();
	return FRHIDescriptorHandle(Type, ResourceIndex);
}

void FMetalDescriptorHeap::UpdateDescriptor(FRHIDescriptorHandle DescriptorHandle, IRDescriptorTableEntry DescriptorData)
{
	checkf(DescriptorHandle.IsValid(), TEXT("Attemping to update invalid descriptor handle!"));
	
	uint32 DescriptorIndex = DescriptorHandle.GetIndex();
	Descriptors[DescriptorIndex] = DescriptorData;
}

void FMetalDescriptorHeap::BindHeap(FMetalCommandEncoder* Encoder, MTL::FunctionType FunctionType, const uint32 BindIndex)
{
	uint32 DescriptorCount = PeakDescriptorCount.load();
	const uint64 HeapSize = DescriptorCount * sizeof(IRDescriptorTableEntry);

	FMetalRHIBuffer* Buffer = ResourceCast(ResourceHeap.GetReference());
	Encoder->SetShaderBuffer(FunctionType, Buffer->GetCurrentBuffer(), 0, HeapSize, BindIndex, MTL::ResourceUsageRead);
}

FMetalBindlessDescriptorManager::FMetalBindlessDescriptorManager(FMetalDevice& MetalDevice)
	: Device(MetalDevice)
	, StandardResources(Device, ERHIDescriptorHeapType::Standard)
	, SamplerResources(Device, ERHIDescriptorHeapType::Sampler)
{
}

FMetalBindlessDescriptorManager::~FMetalBindlessDescriptorManager()
{

}

void FMetalBindlessDescriptorManager::Init()
{
	FRHICommandListImmediate& RHICmdList = FRHICommandListImmediate::Get();
	
	StandardResources.Init(RHICmdList, GBindlessResourceDescriptorHeapSize);
	SamplerResources.Init(RHICmdList, GBindlessSamplerDescriptorHeapSize);
	
	bIsSupported = true;
}

FRHIDescriptorHandle FMetalBindlessDescriptorManager::ReserveDescriptor(ERHIDescriptorHeapType InType)
{
	switch (InType)
	{
	case ERHIDescriptorHeapType::Standard:
		return StandardResources.ReserveDescriptor();
	case ERHIDescriptorHeapType::Sampler:
		return SamplerResources.ReserveDescriptor();
	default:
		checkNoEntry();
	};

	return FRHIDescriptorHandle();
}

void FMetalBindlessDescriptorManager::FreeDescriptor(FRHIDescriptorHandle DescriptorHandle)
{
	check(DescriptorHandle.IsValid());
	switch (DescriptorHandle.GetType())
	{
	case ERHIDescriptorHeapType::Standard:
		StandardResources.FreeDescriptor(DescriptorHandle);
		break;
	case ERHIDescriptorHeapType::Sampler:
		SamplerResources.FreeDescriptor(DescriptorHandle);
		break;
	default:
		checkNoEntry();
	};
}

void FMetalDescriptorHeap::BindSampler(FRHIDescriptorHandle DescriptorHandle, MTL::SamplerState* Sampler)
{
	IRDescriptorTableEntry DescriptorData = {0};
	IRDescriptorTableSetSampler(&DescriptorData, Sampler, 0.0f);

	UpdateDescriptor(DescriptorHandle, DescriptorData);
}

void FMetalDescriptorHeap::BindBufferImmediate(FRHIDescriptorHandle DescriptorHandle, FMetalBufferPtr Resource)
{
	IRDescriptorTableEntry DescriptorData = {0};
	
	IRDescriptorTableSetBuffer(&DescriptorData, Resource->GetGPUAddress(), Resource->GetLength());
	
	UpdateDescriptor(DescriptorHandle, DescriptorData);
}

void FMetalDescriptorHeap::BindResource(FRHIDescriptorHandle DescriptorHandle, FMetalResourceViewBase* Resource, FMetalRHICommandContext* Context, const EDescriptorUpdateType UpdateType)
{
	check(IsInRHIThread() || IsInRenderingThread());
	
	IRDescriptorTableEntry DescriptorData = {0};

	switch (Resource->GetMetalType())
	{
	case FMetalResourceViewBase::EMetalType::TextureView:
		{
			auto const& View = Resource->GetTextureView();

			IRDescriptorTableSetTexture(&DescriptorData, View.get(), 0.0f, 0u);
		}
		break;
	case FMetalResourceViewBase::EMetalType::BufferView:
		{
			auto const& View = Resource->GetBufferView();

			IRDescriptorTableSetBuffer(&DescriptorData, View.Buffer->GetGPUAddress() + View.Offset, View.Size);
		}
		break;
	case FMetalResourceViewBase::EMetalType::TextureBufferBacked:
		{
			auto const& View = Resource->GetTextureBufferBacked();

			IRBufferView BufferView;
			BufferView.buffer = View.Buffer->GetMTLBuffer();
			BufferView.bufferOffset = View.Buffer->GetOffset() + View.Offset;
			BufferView.bufferSize = View.Size;
			BufferView.typedBuffer = true;
			BufferView.textureBufferView = View.Texture.get();

			uint32 Stride = GPixelFormats[View.Format].BlockBytes;
			uint32 FirstElement = View.Offset / Stride;
			uint32 NumElement = View.Size / Stride;

			uint64 BufferVA              = View.Buffer->GetGPUAddress() + View.Offset;
			uint64_t ExtraElement        = (BufferVA % 16) / Stride;

			BufferView.textureViewOffsetInElements = ExtraElement;

			IRDescriptorTableSetBufferView(&DescriptorData, &BufferView);
		}
		break;
#if METAL_RHI_RAYTRACING
	case FMetalResourceViewBase::EMetalType::AccelerationStructure:
		{
			MTL::AccelerationStructure const& AccelerationStructure = Resource->GetAccelerationStructure();

			IRDescriptorTableSetAccelerationStructure(&DescriptorData, [AccelerationStructure.GetPtr() gpuResourceID]._impl);
		}
		break;
#endif
	default:
		checkNoEntry();
		return;
	};

	{
		if(Context && UpdateType != EDescriptorUpdateType_Immediate)
		{
			check(GIsRHIInitialized);
			Context->EnqueueDescriptorUpdate(DescriptorData, DescriptorHandle.GetIndex());
		}
		else
		{
			UpdateDescriptor(DescriptorHandle, DescriptorData);
		}
	}
}

void FMetalBindlessDescriptorManager::UpdateDescriptorsWithGPU(FMetalRHICommandContext* Context, TResourceArray<IRDescriptorTableEntry>&& InComputeDescriptorEntries,
															   TResourceArray<uint32_t>&& InComputeDescriptorIndices)
{
	if(GMetalBindlessUpdateDescriptorOnCompute)
	{
		UpdateDescriptorsWithCompute(Context, MoveTemp(InComputeDescriptorEntries), MoveTemp(InComputeDescriptorIndices));
	}
	else
	{
		UpdateDescriptorsWithCopy(Context, MoveTemp(InComputeDescriptorEntries), MoveTemp(InComputeDescriptorIndices));
	}
}

void FMetalBindlessDescriptorManager::UpdateDescriptorsWithCopy(FMetalRHICommandContext* Context, 
																   TResourceArray<IRDescriptorTableEntry>&& ComputeDescriptorEntriesCopy,
																   TResourceArray<uint32_t>&& ComputeDescriptorIndicesCopy)
{
	const uint32_t NumDescriptors = ComputeDescriptorIndicesCopy.Num();
	
	if(!NumDescriptors)
	{
		return;
	}
	
	for(uint32_t Idx = 0; Idx < NumDescriptors; ++Idx)
	{
		FMetalBufferPtr SourceBuffer = Device.GetTransferAllocator()->Allocate(sizeof(IRDescriptorTableEntry));
		
		FMetalRHIBuffer* DestBuffer = ResourceCast(StandardResources.ResourceHeap.GetReference());
		IRDescriptorTableEntry* DescriptorCopy = (IRDescriptorTableEntry*)SourceBuffer->Contents();
		
		IRDescriptorTableEntry Data = ComputeDescriptorEntriesCopy[Idx];
		FMemory::Memcpy(DescriptorCopy, &Data, sizeof(IRDescriptorTableEntry));
		
		uint32_t DescIdx = ComputeDescriptorIndicesCopy[Idx];
		Context->CopyFromBufferToBuffer(SourceBuffer, 0, DestBuffer->GetCurrentBuffer(), DescIdx*sizeof(IRDescriptorTableEntry), sizeof(IRDescriptorTableEntry));
	}
}


void FMetalBindlessDescriptorManager::UpdateDescriptorsWithCompute(FMetalRHICommandContext* Context, 
																   TResourceArray<IRDescriptorTableEntry>&& ComputeDescriptorEntriesCopy,
																   TResourceArray<uint32_t>&& ComputeDescriptorIndicesCopy)
{
	const uint32_t NumDescriptors = ComputeDescriptorIndicesCopy.Num();
	
	if(!NumDescriptors)
	{
		return;
	}

	FMetalDescriptorHeap* Heap = new FMetalDescriptorHeap(Device, ERHIDescriptorHeapType::Standard);
	
	TRHICommandList_RecursiveHazardous<FMetalRHICommandContext> RHICmdList(Context);
	Heap->Init(RHICmdList, 256);
	
	TShaderMapRef<FUpdateDescriptorHandleCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FRHIComputeShader* ShaderRHI = ComputeShader.GetComputeShader();
	
	FMetalTempAllocator* Allocator = Device.GetUniformAllocator();
	
	// Create the two temporary buffers for descriptor entries and indices
	FMetalBufferPtr DescriptorEntriesBuffer = Allocator->Allocate(NumDescriptors * sizeof(IRDescriptorTableEntry));
	FMemory::Memcpy(DescriptorEntriesBuffer->Contents(), ComputeDescriptorEntriesCopy.GetResourceData(), ComputeDescriptorEntriesCopy.GetResourceDataSize());

	FMetalBufferPtr DescriptorIndiciesBuffer = Allocator->Allocate(NumDescriptors * sizeof(uint));
	FMemory::Memcpy(DescriptorIndiciesBuffer->Contents(), ComputeDescriptorIndicesCopy.GetResourceData(), ComputeDescriptorIndicesCopy.GetResourceDataSize());
	
	// Create descriptor handles and bind buffers to descriptor heap
	FRHIDescriptorHandle DescriptorEntriesHandle = Heap->ReserveDescriptor();
	Heap->BindBufferImmediate(DescriptorEntriesHandle, DescriptorEntriesBuffer);
	
	FRHIDescriptorHandle DescriptorIndicesHandle = Heap->ReserveDescriptor();
	Heap->BindBufferImmediate(DescriptorIndicesHandle, DescriptorIndiciesBuffer);
	
	FRHIDescriptorHandle DestBufferHandle = Heap->ReserveDescriptor();
	
	FMetalRHIBuffer* DestBuffer = (FMetalRHIBuffer*)StandardResources.ResourceHeap.GetReference();
	Heap->BindBufferImmediate(DestBufferHandle, DestBuffer->GetCurrentBuffer());
	
	// Create a temporary uniform buffer
	uint32_t* UBDataPtr = (uint32_t*)FMemory::Malloc(16);
	
	UBDataPtr[0] = NumDescriptors;
	UBDataPtr[1] = DescriptorIndicesHandle.GetIndex();
	UBDataPtr[2] = DescriptorEntriesHandle.GetIndex();
	UBDataPtr[3] = DestBufferHandle.GetIndex();
	
	{
		Context->GetStateCache().SetComputeShader((FMetalComputeShader*)ShaderRHI);
		
		FMetalRHIBuffer* Dest = (FMetalRHIBuffer*)StandardResources.ResourceHeap.GetReference();
		
		Context->GetStateCache().CacheOrSkipResourceResidencyUpdate(DescriptorEntriesBuffer->GetMTLBuffer(), EMetalShaderStages::Compute, true, true);
		Context->GetStateCache().CacheOrSkipResourceResidencyUpdate(DescriptorIndiciesBuffer->GetMTLBuffer(), EMetalShaderStages::Compute, true, true);
		Context->GetStateCache().CacheOrSkipResourceResidencyUpdate(Dest->GetCurrentBuffer()->GetMTLBuffer(), EMetalShaderStages::Compute, false, true);
		
		Context->GetStateCache().SetOverriddenDescriptorHeap(Heap);
		
		FMetalBufferPtr Buffer;
		Context->GetStateCache().IRBindPackedUniforms(EMetalShaderStages::Compute, 0, (const uint8*)UBDataPtr, 16, Buffer);
		Context->GetStateCache().GetShaderParameters(EMetalShaderStages::Compute).MarkAllDirty();
		
		Context->RHIDispatchComputeShader(1, 1, 1);
		Context->GetStateCache().SetOverriddenDescriptorHeap(nullptr);
		
		FMetalDynamicRHI::Get().DeferredDelete([DescriptorEntriesBuffer, DescriptorIndiciesBuffer, Heap, UBDataPtr](){
			delete Heap;
			FMemory::Free(UBDataPtr);
		});
	}
}

void FMetalDescriptorHeap::BindTexture(FRHICommandListBase* RHICmdList, FRHIDescriptorHandle DescriptorHandle, MTL::Texture* Texture, EDescriptorUpdateType UpdateType)
{
	IRDescriptorTableEntry DescriptorData = {0};
	IRDescriptorTableSetTexture(&DescriptorData, Texture, 0.0f, 0u);
	
	UpdateType = !GIsRHIInitialized ? EDescriptorUpdateType_Immediate : UpdateType;
	
	if(RHICmdList)
	{
		RHICmdList->EnqueueLambda([this, InUpdateType=UpdateType, Data=DescriptorData, Handle=DescriptorHandle](FRHICommandListBase& RHICmdList)
		{
			if(InUpdateType == EDescriptorUpdateType_Immediate)
			{
				UpdateDescriptor(Handle, Data);
			}
			else
			{	
				FMetalRHICommandContext* Context = &FMetalRHICommandContext::Get(RHICmdList);
				Context->EnqueueDescriptorUpdate(Data, Handle.GetIndex());
			}
		});
		
		RHICmdList->RHIThreadFence(true);
	}
	else
	{
		check(!IsRunningRHIInSeparateThread() || IsInRHIThread() || IsInParallelRHIThread());
		UpdateDescriptor(DescriptorHandle, DescriptorData);
	}
}

void FMetalBindlessDescriptorManager::BindDescriptorHeapsToEncoder(FMetalCommandEncoder* Encoder, MTL::FunctionType FunctionType, EMetalShaderStages Frequency)
{
	StandardResources.BindHeap(Encoder, FunctionType, kIRStandardHeapBindPoint);
	SamplerResources.BindHeap(Encoder, FunctionType, kIRSamplerHeapBindPoint);
}

void FMetalBindlessDescriptorManager::BindSampler(FRHIDescriptorHandle DescriptorHandle, MTL::SamplerState* Sampler)
{
	SamplerResources.BindSampler(DescriptorHandle, Sampler);
}

void FMetalBindlessDescriptorManager::BindResource(FRHIDescriptorHandle DescriptorHandle, FMetalResourceViewBase* Resource,
												   FMetalRHICommandContext* Context, const EDescriptorUpdateType UpdateType)
{
	StandardResources.BindResource(DescriptorHandle, Resource, Context, UpdateType);
}

void FMetalBindlessDescriptorManager::BindTexture(FRHICommandListBase* RHICmdList, FRHIDescriptorHandle DescriptorHandle, MTL::Texture* Texture, EDescriptorUpdateType UpdateType)
{
	StandardResources.BindTexture(RHICmdList, DescriptorHandle, Texture, UpdateType);
}

#endif //PLATFORM_SUPPORTS_BINDLESS_RENDERING
