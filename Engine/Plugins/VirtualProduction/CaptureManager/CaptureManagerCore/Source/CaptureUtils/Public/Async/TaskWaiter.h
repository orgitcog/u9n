// Copyright Epic Games, Inc. All Rights Reserved.

#include <atomic>
#include "HAL/Platform.h"

namespace UE::CaptureManager
{

class CAPTUREUTILS_API FTaskWaiter
{
public:

	FTaskWaiter();
	~FTaskWaiter();

	bool CreateTask();
	void FinishTask();
	void WaitForAll();

private:

	std::atomic<uint32> TaskCounter;
	const uint32 CanCreateTaskFlag = 0x80000000;
};

}