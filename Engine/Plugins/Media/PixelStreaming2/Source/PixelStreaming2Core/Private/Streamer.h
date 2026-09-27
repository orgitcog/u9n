// Copyright Epic Games, Inc. All Rights Reserved.

#include "IPixelStreaming2Streamer.h"

namespace UE::PixelStreaming2
{
	class FDummyStreamerFactory : public IPixelStreaming2StreamerFactory
	{
	public:
		virtual FString								 GetStreamType() override;
		virtual TSharedPtr<IPixelStreaming2Streamer> CreateNewStreamer(const FString& StreamerId) override;
	};
} // namespace UE::PixelStreaming2