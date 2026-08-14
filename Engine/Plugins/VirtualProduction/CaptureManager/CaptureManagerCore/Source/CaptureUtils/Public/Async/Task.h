// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/Function.h"
#include "Async/StopToken.h"

template<typename TTask>
class FAsyncTask;

namespace UE::CaptureManager
{

class CAPTUREUTILS_API FCancelableAsyncTask
{
public:
	using FTaskFunction = TFunction<void(const FStopToken&)>;

	explicit FCancelableAsyncTask(FTaskFunction InTaskFunction);

	~FCancelableAsyncTask();

	bool IsDone();

	void StartSync();
	void StartAsync();

	void Cancel();

private:
	class FAsyncTaskInternal;

	FStopRequester StopRequester;
	TUniquePtr<FAsyncTask<FAsyncTaskInternal>> AsyncTask;
};

}
