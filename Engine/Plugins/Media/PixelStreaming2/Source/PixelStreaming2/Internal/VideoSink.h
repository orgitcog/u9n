// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Set.h"
#include "CoreTypes.h"
#include "HAL/CriticalSection.h"
#include "IPixelStreaming2VideoSink.h"

namespace UE::PixelStreaming2
{
	class PIXELSTREAMING2_API FVideoSink : public IPixelStreaming2VideoSink
	{
	public:
		virtual ~FVideoSink();

		virtual void AddVideoConsumer(const TWeakPtrVariant<IPixelStreaming2VideoConsumer>& VideoConsumer) override;
		virtual void RemoveVideoConsumer(const TWeakPtrVariant<IPixelStreaming2VideoConsumer>& VideoConsumer) override;

		bool HasVideoConsumers();

		void SetMuted(bool bIsMuted);

		void OnVideoData(FTextureRHIRef Frame);

	protected:
		FCriticalSection									   VideoConsumersCS;
		TArray<TWeakPtrVariant<IPixelStreaming2VideoConsumer>> VideoConsumers;

		bool bIsMuted = false;
	};

} // namespace UE::PixelStreaming2