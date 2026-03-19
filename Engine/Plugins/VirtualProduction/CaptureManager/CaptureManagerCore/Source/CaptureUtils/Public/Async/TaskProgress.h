// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "HAL/CriticalSection.h"
#include "Delegates/Delegate.h"

#include "Monitor.h"

namespace UE::CaptureManager
{

class CAPTUREUTILS_API FTaskProgress : public TSharedFromThis<FTaskProgress>
{
public:

	DECLARE_DELEGATE_OneParam(FProgressReporter, double InProgress);

	class CAPTUREUTILS_API FTask
	{
	public:

		FTask();

		FTask(const FTask& InTask);
		FTask& operator=(const FTask& InTask);

		FTask(FTask&& InTask);
		FTask& operator=(FTask&& InTask);

		void Update(double InProgress);

	private:

		FTask(TWeakPtr<FTaskProgress> InTaskProgress, int32 InId);

		TWeakPtr<FTaskProgress> TaskProgress;
		int32 Id;

		friend FTaskProgress;
	};

	FTaskProgress(uint32 InAmountOfWork, FProgressReporter InReport);

	FTask StartTask();

	double GetTotalProgress() const;

private:

	void Update(int32 InTaskToUpdate, double InProgress);
	void Report() const;

	FProgressReporter Reporter;

	std::atomic<int32> CurrentTask;

	mutable FMonitor<TArray<double>> CurrentProgressValues;
};

}