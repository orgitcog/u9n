// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Event.h"
#include "HAL/IConsoleManager.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Misc/SingleThreadRunnable.h"
#include "Templates/SharedPointer.h"
#include "VideoCapturer.h"

namespace UE::PixelStreaming2
{
	class PIXELSTREAMING2_API FVideoSource
	{
	public:
		virtual ~FVideoSource() = default;

		virtual void PushFrame();
		virtual void ForceKeyFrame();
		virtual void SetMuted(bool bIsMuted);

	protected:
		bool bIsMuted = false;
	};
} // namespace UE::PixelStreaming2
