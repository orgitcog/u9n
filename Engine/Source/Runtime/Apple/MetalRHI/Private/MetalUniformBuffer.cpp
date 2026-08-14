// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalConstantBuffer.cpp: Metal Constant buffer implementation.
=============================================================================*/

#include "MetalUniformBuffer.h"
#include "MetalRHIPrivate.h"
#include "MetalDynamicRHI.h"
#include "MetalTempAllocator.h"
#include "MetalDevice.h"
#include "ShaderParameterStruct.h"
#include "RHIUniformBufferDataShared.h"
#include "Misc/ScopeLock.h"

#pragma mark Suballocated Uniform Buffer Implementation

FMetalSuballocatedUniformBuffer::FMetalSuballocatedUniformBuffer(FMetalDevice& InDevice, const void *Contents, const FRHIUniformBufferLayout* Layout,
																EUniformBufferUsage Usage, EUniformBufferValidation InValidation)
    : FRHIUniformBuffer(Layout)
    , LastFrameUpdated(0)
    , Shadow(FMemory::Malloc(GetSize()))
	, Device(InDevice)
#if METAL_UNIFORM_BUFFER_VALIDATION
    , Validation(InValidation)
#endif // METAL_UNIFORM_BUFFER_VALIDATION
{
	if (Contents)
	{
        UE::RHICore::UpdateUniformBufferConstants(Shadow, Contents, GetLayout());
		CopyResourceTable(Contents, ResourceTable);
		PushToGPUBacking(Shadow);
	}
}

FMetalSuballocatedUniformBuffer::~FMetalSuballocatedUniformBuffer()
{
	if(BackingBuffer)
	{
		FMetalDynamicRHI::Get().DeferredDelete(BackingBuffer);
		BackingBuffer.Reset();
	}
	
	FMemory::Free(Shadow);
}

void FMetalSuballocatedUniformBuffer::Update(const void* Contents)
{
    UE::RHICore::UpdateUniformBufferConstants(Shadow, Contents, GetLayout());
	CopyResourceTable(Contents, ResourceTable);
	PushToGPUBacking(Shadow);
}

// Acquires a region in the current frame's uniform buffer and
// pushes the data in Contents into that GPU backing store
// The amount of data read from Contents is given by the Layout
void FMetalSuballocatedUniformBuffer::PushToGPUBacking(const void* Contents)
{    
	FMetalTempAllocator* Allocator = Device.GetUniformAllocator();
	
	if(BackingBuffer)
	{
		FMetalDynamicRHI::Get().DeferredDelete(BackingBuffer);
		BackingBuffer.Reset();
	}
	
	FMetalPooledBufferArgs ArgsCPU(&Device, GetSize(), BUF_Static, MTL::StorageModeShared);
	BackingBuffer = Device.CreatePooledBuffer(ArgsCPU);
	
	if(!BackingBuffer)
	{
		UE_LOG(LogMetal, Fatal, TEXT("BackingBuffer returned from MetalTempAllocator::Allocate is nullptr"));
	}
	
	uint8* ConstantSpace = reinterpret_cast<uint8*>(BackingBuffer->Contents());
	FMemory::Memcpy(ConstantSpace, Contents, GetSize());
	LastFrameUpdated = Device.GetFrameNumberRHIThread();
}

void FMetalSuballocatedUniformBuffer::CopyResourceTable(const void* Contents, TArray<TRefCountPtr<FRHIResource> >& OutResourceTable) const
{
#if METAL_UNIFORM_BUFFER_VALIDATION
	if (Validation == EUniformBufferValidation::ValidateResources)
	{
		ValidateShaderParameterResourcesRHI(Contents, GetLayout());
	}
#endif // METAL_UNIFORM_BUFFER_VALIDATION

	const FRHIUniformBufferLayout& Layout = GetLayout();
    const uint32 NumResources = Layout.Resources.Num();
    if (NumResources > 0)
    {
		OutResourceTable.Empty(NumResources);
		OutResourceTable.AddZeroed(NumResources);
        
        for (uint32 Index = 0; Index < NumResources; ++Index)
		{
			OutResourceTable[Index] = GetShaderParameterResourceRHI(Contents, Layout.Resources[Index].MemberOffset, Layout.Resources[Index].MemberType);
		}
    }
}
