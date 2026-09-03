// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SimpleAllocBase.h"
#include "Containers/ArrayView.h"

namespace Audio
{
	class FSimpleLinearAllocator : public FSimpleAllocBase
	{
	public:
		explicit FSimpleLinearAllocator(const TArrayView<uint8> InPage)
			: Page(InPage)
		{
			check(SimpleAllocBasePrivate::IsAligned(InPage.GetData(),SimpleAllocBasePrivate::GetDefaultSizeToAlignment(InPage.Num())));
		}
	
		virtual void* Malloc(const SIZE_T InSizeBytes, const uint32 InAlignment=DEFAULT_ALIGNMENT) override
		{
			const uint32 Alignment = InAlignment == DEFAULT_ALIGNMENT ? SimpleAllocBasePrivate::GetDefaultSizeToAlignment(InSizeBytes) : InAlignment;
			const uint32 AlignedNewTop = SimpleAllocBasePrivate::RoundUpToAlignment(Top, Alignment);
		
			if (AlignedNewTop+InSizeBytes < Page.Num())
			{
				Top = AlignedNewTop;
				void* Ret = Page.GetData() + Top;
				Top += InSizeBytes;
				return Ret;
			}
			return nullptr;
		}
		virtual uint32 GetCurrentLifetime() const override
		{
			return CurrentLifetime;
		}
		virtual void Reset() override
		{
			Top = 0;
			CurrentLifetime++;
		}
	
	protected:	
		uint32 Top = 0;					// Top of all allocations, climbs upwards towards end of page.
		uint32 CurrentLifetime = 0;		// Each reset the lifetime is increased
		TArrayView<uint8> Page;			// One page for now.
	};


	class FSimpleLinearAllocatorFromHeap final : public FSimpleLinearAllocator
	{
	public:
		explicit FSimpleLinearAllocatorFromHeap(const SIZE_T InPageSize, const uint32 InPageAlignment = DEFAULT_ALIGNMENT, FMalloc& InHeap = *UE::Private::GMalloc)
			: FSimpleLinearAllocator(MakeArrayView(static_cast<uint8*>(InHeap.Malloc(InPageSize,InPageAlignment)), IntCastChecked<int32>(InPageSize)))
			, Heap(InHeap)
		{}
		virtual ~FSimpleLinearAllocatorFromHeap() override
		{
			Heap.Free(Page.GetData());
		}
	private:
		FMalloc& Heap;
	};
}
