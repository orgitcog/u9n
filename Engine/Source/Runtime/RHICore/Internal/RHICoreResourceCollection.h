// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RHIResources.h"
#include "RHIResourceCollection.h"
#include "RHICommandList.h"
#include "DynamicRHI.h"
#include "Containers/ResourceArray.h"

#if PLATFORM_SUPPORTS_BINDLESS_RENDERING

namespace UE::RHICore
{
	template<typename TType>
	inline size_t CalculateResourceCollectionMemorySize(TConstArrayView<TType> InValues)
	{
		return (1 + InValues.Num()) * sizeof(uint32);
	}

	inline FRHIDescriptorHandle GetHandleForResourceCollectionValue(const FRHIResourceCollectionMember& Member)
	{
		switch (Member.Type)
		{
		case FRHIResourceCollectionMember::EType::Texture:
			return static_cast<const FRHITexture*>(Member.Resource)->GetDefaultBindlessHandle();
		case FRHIResourceCollectionMember::EType::TextureReference:
			return static_cast<const FRHITextureReference*>(Member.Resource)->GetBindlessHandle();
		case FRHIResourceCollectionMember::EType::ShaderResourceView:
			return static_cast<const FRHIShaderResourceView*>(Member.Resource)->GetBindlessHandle();
		}

		return FRHIDescriptorHandle();
	}

	inline FRHIDescriptorHandle GetHandleForResourceCollectionValue(const FRHIDescriptorHandle& Handle)
	{
		return Handle;
	}

	inline void FillResourceCollectionMemory(TRHIBufferInitializer<uint32>& Destination, TConstArrayView<FRHIResourceCollectionMember> InValues)
	{
		int32 WriteIndex = 0;

		Destination[WriteIndex] = static_cast<uint32>(InValues.Num());
		++WriteIndex;

		for (const FRHIResourceCollectionMember& Value : InValues)
		{
			const FRHIDescriptorHandle Handle = GetHandleForResourceCollectionValue(Value);
			check(Handle.IsValid());

			const uint32 BindlessIndex = Handle.IsValid() ? Handle.GetIndex() : 0;

			Destination[WriteIndex] = BindlessIndex;
			++WriteIndex;
		}
	}

	inline FRHIBuffer* CreateResourceCollectionBuffer(FRHICommandListBase& RHICmdList, TConstArrayView<FRHIResourceCollectionMember> InMembers)
	{
		const size_t BufferSize = UE::RHICore::CalculateResourceCollectionMemorySize(InMembers);

		const FRHIBufferCreateDesc CreateDesc =
			FRHIBufferCreateDesc::CreateByteAddress(TEXT("ResourceCollection"), BufferSize, sizeof(uint32))
			.AddUsage(EBufferUsageFlags::Static)
			.SetInitialState(ERHIAccess::SRVMask)
			.SetInitActionInitializer();

		TRHIBufferInitializer<uint32> Initializer = RHICmdList.CreateBufferInitializer(CreateDesc);
		UE::RHICore::FillResourceCollectionMemory(Initializer, InMembers);

		return Initializer.Finalize();
	}

	class FGenericResourceCollection : public FRHIResourceCollection
	{
	public:
		FGenericResourceCollection(FRHICommandListBase& RHICmdList, TConstArrayView<FRHIResourceCollectionMember> InMembers)
			: FRHIResourceCollection(InMembers)
			, Buffer(CreateResourceCollectionBuffer(RHICmdList, InMembers))
		{
			FRHIViewDesc::FBufferSRV::FInitializer ViewDesc = FRHIViewDesc::CreateBufferSRV().SetType(FRHIViewDesc::EBufferType::Raw);
			ShaderResourceView = RHICmdList.CreateShaderResourceView(Buffer, ViewDesc);
		}

		~FGenericResourceCollection() = default;

		// FRHIResourceCollection
		virtual FRHIDescriptorHandle GetBindlessHandle() const final
		{
			return ShaderResourceView->GetBindlessHandle();
		}
		//~FRHIResourceCollection

		FRHIShaderResourceView* GetShaderResourceView() const
		{
			return ShaderResourceView;
		}

		TRefCountPtr<FRHIBuffer> Buffer;
		TRefCountPtr<FRHIShaderResourceView> ShaderResourceView;
	};

	inline FRHIResourceCollectionRef CreateGenericResourceCollection(FRHICommandListBase& RHICmdList, TConstArrayView<FRHIResourceCollectionMember> InMembers)
	{
		return new FGenericResourceCollection(RHICmdList, InMembers);
	}
}

#endif // PLATFORM_SUPPORTS_BINDLESS_RENDERING
