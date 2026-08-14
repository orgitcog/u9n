// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ResourceArray.h"
#include "RHIBufferInitializer.h"
#include "RHICore.h"
#include "RHICommandList.h"

namespace UE::RHICore
{
	// Buffer initializer that just returns the buffer on finalize.
	struct FDefaultBufferInitializer : public FRHIBufferInitializer
	{
		FDefaultBufferInitializer(FRHICommandListBase& RHICmdList, FRHIBuffer* Buffer, void* InWritableData, uint64 InWritableSize)
			: FRHIBufferInitializer(RHICmdList, Buffer, InWritableData, InWritableSize,
				[Buffer = TRefCountPtr<FRHIBuffer>(Buffer)](FRHICommandListBase&) mutable
				{
					return MoveTemp(Buffer);
				}
			)
		{
		}

		FDefaultBufferInitializer(FRHICommandListBase& RHICmdList, FRHIBuffer* Buffer)
			: FDefaultBufferInitializer(RHICmdList, Buffer, nullptr, 0)
		{
		}
	};

	// Buffer initializer that calls Lock on creation and Unlock on finalize.
	struct FLockBufferInitializer : public FRHIBufferInitializer
	{
		FLockBufferInitializer(FRHICommandListBase& RHICmdList, FRHIBuffer* Buffer)
			: FRHIBufferInitializer(
				RHICmdList,
				Buffer,
				RHICmdList.LockBuffer(Buffer, 0, Buffer->GetDesc().Size, RLM_WriteOnly),
				Buffer->GetDesc().Size,
				[Buffer = TRefCountPtr<FRHIBuffer>(Buffer)](FRHICommandListBase& RHICmdList) mutable
				{
					RHICmdList.UnlockBuffer(Buffer);
					return MoveTemp(Buffer);
				})
		{
		}
	};

	// Utility class for RHIs to use when they need to malloc aligned data up-front and make sure it's freed correctly by the Buffer Initializer lambda destruction
	struct FScopedMemory : public FNoncopyable
	{
		FScopedMemory(void* InPointer)
			: Pointer(InPointer)
		{
		}
		FScopedMemory(size_t InSize, uint32 InAlignment)
			: FScopedMemory(FMemory::Malloc(InSize, InAlignment))
		{
		}
		FScopedMemory(FScopedMemory&& InOther)
			: Pointer(InOther.Pointer)
		{
			InOther.Pointer = nullptr;
		}
		~FScopedMemory()
		{
			if (Pointer)
			{
				FMemory::Free(Pointer);
			}
		}

		void* Pointer{};
	};

	// Buffer initializer with custom implementation. This type is necessary for access to the FRHIBufferInitializer protected constructor.
	struct FCustomBufferInitializer : public FRHIBufferInitializer
	{
		FCustomBufferInitializer(FRHICommandListBase& RHICmdList, FRHIBuffer* Buffer, void* InWritableData, uint64 InWritableSize, FFinalizeCallback&& Func)
			: FRHIBufferInitializer(RHICmdList, Buffer, InWritableData, InWritableSize, Forward<FFinalizeCallback>(Func))
		{
		}
	};

	static FRHIBufferInitializer HandleUnknownBufferInitializerInitAction(FRHICommandListBase& RHICmdList, const FRHIBufferCreateDesc& CreateDesc)
	{
		UE_LOG(LogRHICore, Fatal, TEXT("Unknown or unhandled ERHIBufferInitAction: %d"), static_cast<uint32>(CreateDesc.InitAction));

		FRHIBuffer* Buffer = new FRHIBuffer(CreateDesc);
		return UE::RHICore::FDefaultBufferInitializer(RHICmdList, Buffer);
	}

	// Create a buffer initializer for a unified memory platform. Any init actions not handled before here will use default implementations.
	static FRHIBufferInitializer CreateUnifiedMemoryBufferInitializer(
		FRHICommandListBase& RHICmdList
		, const FRHIBufferCreateDesc& CreateDesc
		, FRHIBuffer* Buffer
		, void* WritableData
	)
	{
		if (CreateDesc.InitAction == ERHIBufferInitAction::Default)
		{
			// Use the default buffer implementation, so just return the buffer on finalize.
			return UE::RHICore::FDefaultBufferInitializer(RHICmdList, Buffer);
		}

		if (CreateDesc.InitAction == ERHIBufferInitAction::ResourceArray)
		{
			// Write the data from the resource array immediately, discard the resource array and then return the buffer on finalize.
			check(CreateDesc.InitialData);

			FMemory::Memcpy(WritableData, CreateDesc.InitialData->GetResourceData(), CreateDesc.InitialData->GetResourceDataSize());

			// Discard the resource array's contents.
			CreateDesc.InitialData->Discard();

			return UE::RHICore::FDefaultBufferInitializer(RHICmdList, Buffer);
		}

		if (CreateDesc.InitAction == ERHIBufferInitAction::Zeroed)
		{
			// Zero memory immediately and return the buffer on finalize
			FMemory::Memzero(WritableData, CreateDesc.Size);
			return UE::RHICore::FDefaultBufferInitializer(RHICmdList, Buffer);
		}

		// Let the caller fill in the writable data.
		if (CreateDesc.InitAction == ERHIBufferInitAction::Initializer)
		{
			return UE::RHICore::FDefaultBufferInitializer(RHICmdList, Buffer, WritableData, CreateDesc.Size);
		}

		return UE::RHICore::HandleUnknownBufferInitializerInitAction(RHICmdList, CreateDesc);
	}
}
