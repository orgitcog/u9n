// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ArrayView.h"
#include "HAL/Platform.h"
#include "ScratchBuffer.h"
#include "Templates/RetainedRef.h"
#include "TypeFamily/ChannelTypeFamily.h"

namespace Audio
{
	class FChannelTypeFamily;
	class FSimpleAllocBase;

	class AUDIOEXPERIMENTALRUNTIME_API FChannelAgnosticType
	{
	public:
		static FSimpleAllocBase& GetDefaultAllocator();

		explicit FChannelAgnosticType(const TRetainedRef<const FChannelTypeFamily> InType, const int32 InNumFrames, FSimpleAllocBase* InAllocator = &GetDefaultAllocator());

		/**
		 * 
		 * @param InType 
		 * @param InNumFrames 
		 * @param InNumChannels 
		 * @param InAllocator 
		 */
		explicit FChannelAgnosticType(const TRetainedRef<const FChannelTypeFamily> InType, const int32 InNumFrames, const int32 InNumChannels, FSimpleAllocBase* InAllocator = &GetDefaultAllocator());
	
		// To be a Variable it *must* be copyable.
		FChannelAgnosticType(const FChannelAgnosticType& Other) = default;
		FChannelAgnosticType(FChannelAgnosticType&& Other) = default;
		FChannelAgnosticType& operator=(FChannelAgnosticType&& Other) = default;
		FChannelAgnosticType& operator=(const FChannelAgnosticType& Other) = default;

		[[nodiscard]]
		int32 NumFrames() const { return NumFramesPrivate; }

		[[nodiscard]]
		int32 NumChannels() const { return NumChannelsPrivate; }

		[[nodiscard]]
		TArrayView<const float> GetChannel(const int32 InChannelIndex) const
		{
			check(InChannelIndex >= 0 && InChannelIndex < NumChannelsPrivate);
			const TArrayView<const float> AllChannels = Buffer.GetView();
			return MakeArrayView(AllChannels.GetData() + (NumFramesPrivate * InChannelIndex), NumFramesPrivate);
		}

		[[nodiscard]]
		TArrayView<float> GetChannel(const int32 InChannelIndex)
		{
			const TArrayView<const float> ConstView = std::as_const(*this).GetChannel(InChannelIndex);
			return MakeArrayView<float>(const_cast<float*>(ConstView.GetData()), ConstView.Num());
		}

		bool IsA(const FChannelAgnosticType& InOther) const;
		bool IsA(const FName& InTypeName) const;
		FName GetTypeName() const;
		const FChannelTypeFamily& GetType() const { return *Type; }

		/**
		 * For fast raw DSP access to the buffer.
		 * @return View of entire multi-mono buffer.
		 */
		[[nodiscard]]
		TArrayView<float> GetRawMultiMono()
		{
			return Buffer.GetView();
		}
		/**
		 * For fast raw DSP access to the buffer.  
		 * @return View of entire multi-mono buffer.
		 */
		[[nodiscard]]
		TArrayView<const float> GetRawMultiMono() const
		{
			return Buffer.GetView();	
		}
		
	private:
		friend class FCatUtils;
		TScratchBuffer<float> Buffer;
		const FChannelTypeFamily* Type = nullptr;	// Keep this as pointer internally for easy of copying.
		int32 NumFramesPrivate = 0;
		int32 NumChannelsPrivate = 0;
	};
}
