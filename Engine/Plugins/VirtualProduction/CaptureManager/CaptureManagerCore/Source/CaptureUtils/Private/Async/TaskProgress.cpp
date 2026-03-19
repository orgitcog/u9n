// Copyright Epic Games, Inc. All Rights Reserved.

#include "Async/TaskProgress.h"

#include "Math/UnrealMathUtility.h"

namespace UE::CaptureManager
{

FTaskProgress::FTask::FTask()
	: TaskProgress(nullptr)
{
}

FTaskProgress::FTask::FTask(FTask&& InTask)
{
	*this = MoveTemp(InTask);
}

FTaskProgress::FTask& FTaskProgress::FTask::operator=(FTask&& InTask)
{
	if (this == &InTask)
	{
		return *this;
	}

	TaskProgress = MoveTemp(InTask.TaskProgress);
	Id = InTask.Id;

	InTask.TaskProgress = nullptr;
	InTask.Id = INDEX_NONE;

	return *this;
}

FTaskProgress::FTask::FTask(const FTask& InTask)
{
	*this = InTask;
}

FTaskProgress::FTask& FTaskProgress::FTask::operator=(const FTask& InTask)
{
	if (this == &InTask)
	{
		return *this;
	}

	TaskProgress = InTask.TaskProgress;
	Id = InTask.Id;

	return *this;
}

FTaskProgress::FTask::FTask(TWeakPtr<FTaskProgress> InTaskProgress, int32 InId)
	: TaskProgress(MoveTemp(InTaskProgress))
	, Id(InId)
{
}

void FTaskProgress::FTask::Update(double InProgress)
{
	if (TSharedPtr<FTaskProgress> TaskProgressShared = TaskProgress.Pin())
	{
		TaskProgressShared->Update(Id, InProgress);
		TaskProgressShared->Report();
	}
}

FTaskProgress::FTaskProgress(uint32 InAmountOfWork, FProgressReporter InReporter)
	: Reporter(MoveTemp(InReporter))
	, CurrentTask(-1)
{
	checkf(InAmountOfWork != 0, TEXT("Number of tasks must NOT be 0"));
	CurrentProgressValues.Lock()->AddZeroed(InAmountOfWork);
}

FTaskProgress::FTask FTaskProgress::StartTask()
{
	int32 NewTask = ++CurrentTask;
	checkf(CurrentProgressValues.Lock()->IsValidIndex(NewTask), TEXT("Unexpected start task"));

	return FTaskProgress::FTask(AsWeak(), NewTask);
}

double FTaskProgress::GetTotalProgress() const
{
	FMonitor<TArray<double>>::FHelper ProgressValues = CurrentProgressValues.Lock();

	double TotalProgress = 0.0;

	for (double TaskProgress : *ProgressValues)
	{
		TotalProgress += TaskProgress;
	}

	return TotalProgress / ProgressValues->Num();
}

void FTaskProgress::Update(int32 InTaskToUpdate, double InProgress)
{
	FMonitor<TArray<double>>::FHelper ProgressValues = CurrentProgressValues.Lock();

	checkf(InTaskToUpdate < ProgressValues->Num(), TEXT("Current task exceeds the number of expected tasks"));

	InProgress = FMath::Clamp(InProgress, 0.0f, 1.0f);

	(*ProgressValues)[InTaskToUpdate] = InProgress;
}

void FTaskProgress::Report() const
{
	float TotalProgress = GetTotalProgress();
	Reporter.ExecuteIfBound(TotalProgress);
}

}