// Copyright Epic Games, Inc. All Rights Reserved.
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetalRHIPrivate.h"

#if PLATFORM_SUPPORTS_BINDLESS_RENDERING
#include "MetalResources.h"
#include "MetalShaderResources.h"
#include "Containers/DynamicRHIResourceArray.h"

class FMetalCommandEncoder;
class FMetalDevice;

enum EDescriptorUpdateType
{
	EDescriptorUpdateType_Immediate,
	EDescriptorUpdateType_GPU
};

struct FMetalDescriptorHeap
{
	FMetalDescriptorHeap(FMetalDevice& MetalDevice, const ERHIDescriptorHeapType DescriptorType);
	~FMetalDescriptorHeap();
	
    void                    Init(FRHICommandListBase& RHICmdList, const int32 HeapSize);

    FRHIDescriptorHandle    ReserveDescriptor();
    void                    FreeDescriptor(FRHIDescriptorHandle DescriptorHandle);
    uint32                  GetFreeResourceIndex();

    void                    UpdateDescriptor(FRHIDescriptorHandle DescriptorHandle, struct IRDescriptorTableEntry DescriptorData);
    void                    BindHeap(FMetalCommandEncoder* Encoder, MTL::FunctionType FunctionType, const uint32 BindIndex);
    
	void                   	BindSampler(FRHIDescriptorHandle DescriptorHandle, MTL::SamplerState* Sampler);
	void                    BindBufferImmediate(FRHIDescriptorHandle DescriptorHandle, FMetalBufferPtr Resource);
	void                    BindResource(FRHIDescriptorHandle DescriptorHandle, FMetalResourceViewBase* Resource,
												 FMetalRHICommandContext* Context, const EDescriptorUpdateType UpdateType = EDescriptorUpdateType_GPU);
	void                    BindTexture(FRHICommandListBase* RHICmdList, FRHIDescriptorHandle DescriptorHandle, MTL::Texture* Texture, const EDescriptorUpdateType UpdateType);
	
    //
	FMetalDevice& 					Device;
	
    FCriticalSection                FreeListCS;
    TQueue<uint32>                  FreeList;

    std::atomic<uint32>             PeakDescriptorCount;
    struct IRDescriptorTableEntry*  Descriptors;
    uint32_t                        ResourceHeapLength;
    FBufferRHIRef			        ResourceHeap;
    
    const ERHIDescriptorHeapType    Type;
};

class FMetalBindlessDescriptorManager
{
public:
                            FMetalBindlessDescriptorManager(FMetalDevice& MetalDevice);
                            ~FMetalBindlessDescriptorManager();

    void                    Init();

    FRHIDescriptorHandle    ReserveDescriptor(ERHIDescriptorHeapType InType);
    void                    FreeDescriptor(FRHIDescriptorHandle DescriptorHandle);

    void                    BindSampler(FRHIDescriptorHandle DescriptorHandle, MTL::SamplerState* Sampler);
    void                    BindResource(FRHIDescriptorHandle DescriptorHandle, FMetalResourceViewBase* Resource,
										 FMetalRHICommandContext* Context, const EDescriptorUpdateType UpdateType = EDescriptorUpdateType_GPU);
    void                    BindTexture(FRHICommandListBase* RHICmdList, FRHIDescriptorHandle DescriptorHandle, MTL::Texture* Texture, EDescriptorUpdateType UpdateType);

    void                    BindDescriptorHeapsToEncoder(FMetalCommandEncoder* Encoder, MTL::FunctionType FunctionType, EMetalShaderStages Frequency);

	bool					IsSupported() {return bIsSupported;}
    
	void 					UpdateDescriptorsWithGPU(FMetalRHICommandContext* Context,
													 TResourceArray<IRDescriptorTableEntry>&& ComputeDescriptorEntries,
													 TResourceArray<uint32_t>&& ComputeDescriptorIndices);
    
private:
	void                    UpdateDescriptorsWithCompute(FMetalRHICommandContext* Context, TResourceArray<IRDescriptorTableEntry>&& ComputeDescriptorEntries,
														 TResourceArray<uint32_t>&& ComputeDescriptorIndices);
	
	void                    UpdateDescriptorsWithCopy(FMetalRHICommandContext* Context, TResourceArray<IRDescriptorTableEntry>&& ComputeDescriptorEntries,
														 TResourceArray<uint32_t>&& ComputeDescriptorIndices);
	
	
	
	bool 					bIsSupported = false;
	FMetalDevice& 			Device;
    FMetalDescriptorHeap    StandardResources;
    FMetalDescriptorHeap    SamplerResources;
};

#endif
