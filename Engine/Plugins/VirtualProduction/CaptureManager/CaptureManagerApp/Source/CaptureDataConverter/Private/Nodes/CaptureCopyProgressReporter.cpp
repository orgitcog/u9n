// Copyright Epic Games, Inc. All Rights Reserved.

#include "CaptureCopyProgressReporter.h"

FCopyProgressReporter::FCopyProgressReporter(UE::CaptureManager::FTaskProgress::FTask& InTask,
											 UE::CaptureManager::FStopToken InStopToken)
	: Task(InTask)
	, StopToken(MoveTemp(InStopToken))
{
}

bool FCopyProgressReporter::Poll(float InProgress)
{
	Task.Update(InProgress);

	return !StopToken.IsStopRequested();
}