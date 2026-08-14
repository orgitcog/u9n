// Copyright Epic Games, Inc. All Rights Reserved.

#include "VideoSource.h"

namespace UE::PixelStreaming2
{
	void FVideoSource::PushFrame()
	{
	}

	void FVideoSource::ForceKeyFrame()
	{
	}

	void FVideoSource::SetMuted(bool bInIsMuted)
	{
		bIsMuted = bInIsMuted;
	}
} // namespace UE::PixelStreaming2
