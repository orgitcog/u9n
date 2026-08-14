// Copyright Epic Games, Inc. All Rights Reserved.

#include "Async/Mutex.h"

#include "Async/Fundamental/Scheduler.h"
#include "Async/ParkingLot.h"
#include "HAL/PlatformProcess.h"

namespace UE
{

void FMutex::LockSlow()
{
	constexpr int32 SpinLimit = 40;
	int32 SpinCount = 0;
	for (uint8 CurrentState = State.load(std::memory_order_relaxed);;)
	{
		// Try to acquire the lock if it was unlocked, even if there are waiting threads.
		// Acquiring the lock despite the waiting threads means that this lock is not FIFO and thus not fair.
		if (LIKELY(!(CurrentState & IsLockedFlag)))
		{
			if (LIKELY(State.compare_exchange_weak(CurrentState, CurrentState | IsLockedFlag, std::memory_order_acquire, std::memory_order_relaxed)))
			{
				return;
			}
			continue;
		}

		// Spin up to the spin limit while there are no waiting threads.
		if (LIKELY(!(CurrentState & MayHaveWaitingLockFlag) && SpinCount < SpinLimit))
		{
			FPlatformProcess::YieldThread();
			++SpinCount;
			CurrentState = State.load(std::memory_order_relaxed);
			continue;
		}

		// Store that there are waiting threads. Restart if the state has changed since it was loaded.
		if (LIKELY(!(CurrentState & MayHaveWaitingLockFlag)))
		{
			if (UNLIKELY(!State.compare_exchange_weak(CurrentState, CurrentState | MayHaveWaitingLockFlag, std::memory_order_relaxed)))
			{
				continue;
			}
			CurrentState |= MayHaveWaitingLockFlag;
		}

		// Do not enter oversubscription during a wait on a mutex since the wait is generally too short
		// for it to matter and it can worsen performance a lot for heavily contended locks.
		LowLevelTasks::Private::FOversubscriptionAllowedScope _(false);

		// Wait if the state has not changed. Either way, loop back and try to acquire the lock after trying to wait.
		ParkingLot::Wait(&State, [this, CurrentState] { return State.load(std::memory_order_relaxed) == CurrentState; }, []{});
		CurrentState = State.load(std::memory_order_relaxed);
	}
}

FORCENOINLINE void FMutex::WakeWaitingThread()
{
	if constexpr (!bUnlockImmediately)
	{
		uint8 CurrentState = State.load(std::memory_order_relaxed);
		checkSlow(CurrentState & IsLockedFlag);

		// Spin on the fast path because there may be spurious failures.
		while (LIKELY(!(CurrentState & MayHaveWaitingLockFlag)))
		{
			if (LIKELY(State.compare_exchange_weak(CurrentState, 0, std::memory_order_release, std::memory_order_relaxed)))
			{
				return;
			}
		}
	}

	ParkingLot::WakeOne(&State, [this](ParkingLot::FWakeState WakeState) -> uint64
	{
		if constexpr (bUnlockImmediately)
		{
			if (!WakeState.bHasWaitingThreads)
			{
				State.fetch_and(~MayHaveWaitingLockFlag, std::memory_order_relaxed);
			}
		}
		else
		{
			const uint8 NewState = WakeState.bHasWaitingThreads ? MayHaveWaitingLockFlag : 0;
			const uint8 OldState = State.exchange(NewState, std::memory_order_release);
			checkSlow((OldState & IsLockedFlag) && (OldState & MayHaveWaitingLockFlag));
		}
		return 0;
	});
}

} // UE
