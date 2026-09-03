// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace UE::PixelStreaming2
{
	/**
	 * The thread. Wraps both the RunnableThread and Runnable into a single point
	 */
	class FPixelStreamingThread
	{
	public:
		FPixelStreamingThread();
		~FPixelStreamingThread();

	private:
		TSharedPtr<class FRunnableThread>		  Thread;
		TSharedPtr<class FPixelStreamingRunnable> Runnable;
	};
} // namespace UE::PixelStreaming2
