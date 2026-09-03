// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/RunnableThread.h"
#include "HAL/Runnable.h"
#include "Containers/Ticker.h"
#include <TimerManager.h> // Angled brackets to pick up UE's TimerManager.h and not this file.

class FCPSTimerManager
{
public:

	using FTimerHandle = FTSTicker::FDelegateHandle;

	static const float IdealTimeResolution;

	FCPSTimerManager(const float Resolution = IdealTimeResolution);

	static FCPSTimerManager& Get();
	~FCPSTimerManager();

	FTimerHandle AddTimer(FTimerDelegate InDelegate, float InRate, bool InbLoop = false, float InFirstDelay = 0.f);
	void RemoveTimer(FTimerHandle Handle);

private:

	class FTimerManagerRunnable final : public FRunnable
	{
	public:

		FTimerManagerRunnable(FCPSTimerManager* InOwner, const float InResolution);

		virtual uint32 Run() override;
		virtual void Stop() override;

	private:

		FCPSTimerManager* Owner;
		std::atomic_bool bShouldRun = true;
		const float IdealTimeResolution;
	};

	FTSTicker Ticker;
	TUniquePtr<FRunnableThread> Thread;
	TUniquePtr<FTimerManagerRunnable> Runnable;
};