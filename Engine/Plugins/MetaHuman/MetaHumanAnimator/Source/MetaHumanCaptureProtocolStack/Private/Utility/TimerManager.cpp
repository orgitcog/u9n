// Copyright Epic Games, Inc. All Rights Reserved.

#include "TimerManager.h"

const float FCPSTimerManager::IdealTimeResolution = 0.1f;

FCPSTimerManager& FCPSTimerManager::Get()
{
	static FCPSTimerManager Manager;
	return Manager;
}

FCPSTimerManager::FCPSTimerManager(const float Resolution)
{
	Runnable = MakeUnique<FTimerManagerRunnable>(this, Resolution);
	Thread.Reset(FRunnableThread::Create(Runnable.Get(), TEXT("Timer Manager"), 128 * 1024, TPri_Normal, FPlatformAffinity::GetPoolThreadMask()));
}

FCPSTimerManager::~FCPSTimerManager()
{
	Thread->Kill(true);

	Thread = nullptr;
	Runnable = nullptr;
}

FCPSTimerManager::FTimerHandle FCPSTimerManager::AddTimer(FTimerDelegate InDelegate, float InRate, bool InbLoop, float InFirstDelay)
{
	FCPSTimerManager::FTimerHandle Handle = Ticker.AddTicker(TEXT("Timer"), InRate, [InbLoop, Delegate = MoveTemp(InDelegate)](float InDelay)
	{
		Delegate.Execute();

		return InbLoop;
	});

	Handle.Pin()->FireTime -= InRate - InFirstDelay;

	return Handle;
}

void FCPSTimerManager::RemoveTimer(FTimerHandle Handle)
{
	Ticker.RemoveTicker(MoveTemp(Handle));
}

FCPSTimerManager::FTimerManagerRunnable::FTimerManagerRunnable(FCPSTimerManager* InOwner, const float InResolution)
	: Owner(InOwner)
	, IdealTimeResolution(InResolution)
{
}

uint32 FCPSTimerManager::FTimerManagerRunnable::Run()
{
	double LastTime = FPlatformTime::Seconds();
	
	while (bShouldRun.load())
	{
		const double CurrentTime = FPlatformTime::Seconds();
		const double DeltaTime = CurrentTime - LastTime;

		Owner->Ticker.Tick(DeltaTime);

		FPlatformProcess::Sleep(FMath::Max<float>(0.0f, IdealTimeResolution - (FPlatformTime::Seconds() - LastTime)));

		LastTime = CurrentTime;
	}

	Owner->Ticker.Reset();

	return 0;
}

void FCPSTimerManager::FTimerManagerRunnable::Stop()
{
	bShouldRun.store(false);
}