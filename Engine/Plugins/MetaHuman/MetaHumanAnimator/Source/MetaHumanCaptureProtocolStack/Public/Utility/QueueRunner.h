// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Queue.h"

#include "HAL/Event.h"
#include "HAL/PlatformProcess.h"
#include "HAL/RunnableThread.h"
#include "HAL/Runnable.h"
#include "Delegates/Delegate.h"

template<class T>
class TQueueRunner : public FRunnable
{
public:

    DECLARE_DELEGATE_OneParam(FOnProcess, T InElem)

    TQueueRunner(FOnProcess InProcess)
        : bRunning(true)
        , OnProcess(MoveTemp(InProcess))
    {
        Event = FGenericPlatformProcess::GetSynchEventFromPool(false);
        check(Event);

        Thread.Reset(FRunnableThread::Create(this, TEXT("Queue Runner"), 128 * 1024, TPri_Normal, FPlatformAffinity::GetPoolThreadMask()));
    }

    ~TQueueRunner()
    {
        Thread->Kill(true);

		FGenericPlatformProcess::ReturnSynchEventToPool(Event);
    }

    void Add(T InElement)
    {
		{
			FScopeLock lock(&Mutex);
			Queue.Enqueue(MoveTemp(InElement));
		}

		Event->Trigger();
    }

protected:

    virtual uint32 Run() override
    {
        T Elem;

		while (bRunning.load())
        {
            Event->Wait();

            for (;;)
            {
				{
		            FScopeLock Lock(&Mutex);
					if (!Queue.Dequeue(Elem))
					{
						break;
					}
				}

                OnProcess.ExecuteIfBound(MoveTemp(Elem));
            }
        }

		return 0;
    }

    virtual void Stop() override
    {
        bRunning.store(false);

        Event->Trigger();
    }

private:

    std::atomic<bool> bRunning;
    FOnProcess OnProcess;

    FCriticalSection Mutex;
    FEvent* Event;
    TQueue<T> Queue;
    TUniquePtr<FRunnableThread> Thread;
};
