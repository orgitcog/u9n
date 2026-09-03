// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreTypes.h"
#include "Misc/AutomationTest.h"
#include "Tests/Benchmark.h"
#include "Tasks/Pipe.h"
#include "HAL/Thread.h"
#include "Misc/SpinLock.h"
#include "Misc/ScopeLock.h"
#include "Async/Mutex.h"
#include "Async/UniqueLock.h"
#include "Async/ParallelFor.h"
#include "Tests/TestHarnessAdapter.h"
#include "HAL/ThreadHeartBeat.h"
#include "HAL/PlatformManualResetEvent.h"
#include "ProfilingDebugging/ScopedTimers.h"
#include "Containers/UnrealString.h"
#include "Sanitizer/RaceDetector.h"

#include <atomic>
#include <thread>

#if WITH_TESTS && USING_INSTRUMENTATION

namespace UE { namespace TasksTests
{
	using namespace Tasks;
	using namespace UE::Sanitizer;
	using namespace UE::Sanitizer::RaceDetector;

	// A "fake" spinning wait that makes sure a "Wait" doesn't make the caller thread
	// retract the task. Used to test different scenarios.
	void TestWait(FTask& Task)
	{
		while (!Task.IsCompleted())
		{
			FPlatformProcess::Yield();
		}
	}

	void TestWait(FGraphEventRef& GraphEvent)
	{
		while (!GraphEvent->IsComplete())
		{
			FPlatformProcess::Yield();
		}
	}

	struct FDataRace {
		void* Address;
		FString FirstThreadName;
		FString SecondThreadName;
		FFullLocation FirstLocation;
		FFullLocation SecondLocation;

		bool operator==(const FDataRace& Other) const
		{
			return Address == Other.Address &&
				((FirstThreadName == Other.FirstThreadName && SecondThreadName == Other.SecondThreadName) ||
				(SecondThreadName == Other.FirstThreadName && FirstThreadName == Other.SecondThreadName));
		}
	};

	class FRaceCollectorBase {
	public:
	
		bool IsEmpty()
		{
			return Races.IsEmpty();
		}

		void Reset()
		{
			Races.Reset();
		}

		bool Contains(void* RaceAddress)
		{
			for (auto& Race : Races)
			{
				if (Race.Address == RaceAddress)
				{
					return true;
				}
			}
			return false;
		}

		bool Contains(void* RaceAddress, const FString& FirstTaskName, const FString& SecondTaskName)
		{
			FDataRace Check{ RaceAddress, FirstTaskName, SecondTaskName };
			for (auto& Race : Races)
			{
				if (Race == Check)
					return true;
			}
			return false;
		}

		uint32 NumRacesForAddress(void* RaceAddress)
		{
			uint32 Num = 0;
			for (auto& Race : Races)
			{
				if (Race.Address == RaceAddress)
				{
					Num++;
				}
			}
			return Num;
		}

	protected:
		UE::FSpinLock RaceLock;
		TArray<FDataRace> Races;
	};

	class FThreadRaceCollector : public FRaceCollectorBase {
	public:
		FThreadRaceCollector()
		{
			SetRaceCallbackFn(*this);
		}

		~FThreadRaceCollector()
		{
			ResetRaceCallbackFn();
		}

		void operator()(uint64 RaceAddress, uint32 FirstThreadId, uint32 SecondThreadId, const FFullLocation& FirstLocation, const FFullLocation& SecondLocation)
		{
			UE::TUniqueLock Lock(RaceLock);
			Races.Add(FDataRace{ (void*)RaceAddress, FString::Printf(TEXT("%d"), FirstThreadId), FString::Printf(TEXT("%d"), SecondThreadId), FirstLocation, SecondLocation });
		}
	};

	void Test();
	void TestLockFree(int32 OuterIters = 3);

	TEST_CASE_NAMED(FRaceDetectorTasksWithPrereqTest, "System::Core::Sanitizer::RaceDetector::TasksWithPrereq", "[ApplicationContextMask][EngineFilter]")
	{
		CHECK(UE::Sanitizer::RaceDetector::Initialize());

		ToggleRaceDetection(false);
		FThreadRaceCollector Collector;

		for (uint32 i = 0; i < 100; ++i)
		{
			Collector.Reset();
			ToggleRaceDetection(true);

			int x = 0, y = 0;

			FGraphEventRef LegacyTask = FFunctionGraphTask::CreateAndDispatchWhenReady([&]
			{
				x = 1;
				y = 1;
			}, TStatId(), nullptr, ENamedThreads::AnyHiPriThreadHiPriTask);

			{
				FGraphEventArray Prereqs;
				Prereqs.Add(LegacyTask);
				LegacyTask = FFunctionGraphTask::CreateAndDispatchWhenReady([&]
				{
					x = 2;
					y = 2;
				}, TStatId(), &Prereqs, ENamedThreads::AnyHiPriThreadHiPriTask);
			}

			LegacyTask->Wait();

			ToggleRaceDetection(false);

			CHECK(!Collector.Contains(&x));
			CHECK(!Collector.Contains(&y));
		}
	}

	TEST_CASE_NAMED(FRaceDetectorStdThreadRacesTest, "System::Core::Sanitizer::RaceDetector::StdThreadRaces", "[ApplicationContextMask][EngineFilter]")
	{
		CHECK(UE::Sanitizer::RaceDetector::Initialize());

		ToggleRaceDetection(false);

		FThreadRaceCollector Collector;

		ToggleRaceDetection(true);

		int x = 0, y = 0, z = 0, w = 0;
		std::atomic<int> sync = 0;
		std::thread t1{ [&]() {
			x = 1;
			y = 1;
			z = 1;
		} };

		w = 2;

		std::thread t2{ [&]() {
			w = 3;
			z = 3;
		} };

		x = 2;

		t1.join();
		t2.join();

		y = 2;

		ToggleRaceDetection(false);

		CHECK(Collector.Contains(&x));
		CHECK(!Collector.Contains(&y));
		CHECK(Collector.Contains(&z));
		CHECK(!Collector.Contains(&w));
	}

	TEST_CASE_NAMED(FRaceDetectorStdThreadAtomicSyncTest, "System::Core::Sanitizer::RaceDetector::StdThreadAtomicSync", "[ApplicationContextMask][EngineFilter]")
	{
		CHECK(UE::Sanitizer::RaceDetector::Initialize());

		ToggleRaceDetection(false);
		FThreadRaceCollector Collector;

		ToggleRaceDetection(true);

		int x = 0, y = 0, result = 0;
		std::atomic<int> sync = 0;
		std::thread t1{ [&]() {
			x = 10;

			int expected = 0;
			if (!sync.compare_exchange_strong(expected, 1))
			{
				result = y;
			}
		} };

		y = 20;

		int expected = 0;
		if (!sync.compare_exchange_strong(expected, 1))
		{
			result = x;
		}

		t1.join();

		ToggleRaceDetection(false);

		CHECK(!Collector.Contains(&x));
		CHECK(!Collector.Contains(&y));
		CHECK(!Collector.Contains(&result));
	}

	TEST_CASE_NAMED(FRaceDetectorStdThreadSynchEventTest, "System::Core::Sanitizer::RaceDetector::StdThreadSynchEvent", "[ApplicationContextMask][EngineFilter]")
	{
		CHECK(UE::Sanitizer::RaceDetector::Initialize());

		ToggleRaceDetection(false);

		FThreadRaceCollector Collector;

		ToggleRaceDetection(true);

		FEvent* Event = FPlatformProcess::GetSynchEventFromPool(false);
		Event->Reset();

		int x = 0, y = 0, result = 0;

		ToggleFilterDetailedLogOnAddress(&y);
		ToggleGlobalDetailedLog(true);

		std::thread t1{ [&]() {
			x = 10;
			Event->Trigger();
			y = 20;
		} };

		Event->Wait();
		result = x;
		result += y;

		t1.join();

		ToggleFilterDetailedLogOnAddress(nullptr);
		ToggleGlobalDetailedLog(false);

		ToggleRaceDetection(false);

		CHECK(!Collector.Contains(&x));
		CHECK(Collector.Contains(&y));
		CHECK(result); // avoid the result and its operations to be optimized out.
	}

	TEST_CASE_NAMED(FRaceDetectorStdThreadManualResetEventTest, "System::Core::Sanitizer::RaceDetector::StdThreadManualResetEvent", "[ApplicationContextMask][EngineFilter]")
	{
		CHECK(UE::Sanitizer::RaceDetector::Initialize());

		ToggleRaceDetection(false);
		FThreadRaceCollector Collector;

		ToggleRaceDetection(true);

		UE::FPlatformManualResetEvent Event;
		Event.Reset();

		int x = 0, y = 0, result = 0;
		std::thread t1{ [&]() {
			x = 10;
			Event.Notify();
			y = 20;
		} };

		Event.Wait();
		result = x;
		result += y;

		t1.join();

		ToggleRaceDetection(false);

		CHECK(!Collector.Contains(&x));
		CHECK(Collector.Contains(&y));
		CHECK(result); // avoid the result and its operations to be optimized out.
	}

	TEST_CASE_NAMED(FRaceDetectorModernTasksTest, "System::Core::Sanitizer::RaceDetector::ModernTasks", "[ApplicationContextMask][EngineFilter]")
	{
		CHECK(UE::Sanitizer::RaceDetector::Initialize());

		ToggleRaceDetection(false);

		FThreadRaceCollector Collector;

		ToggleRaceDetection(true);

		int x = 0, y = 0;
		FTask TaskA = UE::Tasks::Launch(TEXT("A"), [&]() {
			x = 3;
			y = 3;
		});
		x = 5;
		TestWait(TaskA);

		y = 5;

		ToggleRaceDetection(false);
		CHECK(Collector.Contains(&x));
		CHECK(!Collector.Contains(&y));
	}

	TEST_CASE_NAMED(FRaceDetectorLegacyTasksTest, "System::Core::Sanitizer::RaceDetector::LegacyTasks", "[ApplicationContextMask][EngineFilter]")
	{
		CHECK(UE::Sanitizer::RaceDetector::Initialize());

		ToggleRaceDetection(false);

		FThreadRaceCollector Collector;

		ToggleRaceDetection(true);

		int x = 0, y = 0;
		FGraphEventRef LegacyTask = FFunctionGraphTask::CreateAndDispatchWhenReady([&]
		{
			x = 3;
			y = 3;
		}, TStatId(), nullptr, ENamedThreads::AnyHiPriThreadHiPriTask);

		x = 5;

		TestWait(LegacyTask);

		y = 5;

		ToggleRaceDetection(false);

		CHECK(Collector.Contains(&x));
		CHECK(!Collector.Contains(&y));
	}

	TEST_CASE_NAMED(FRaceDetectorTwoAsyncThreadRaceTest, "System::Core::Sanitizer::RaceDetector::TwoAsyncThreadRace", "[ApplicationContextMask][EngineFilter]")
	{
		CHECK(UE::Sanitizer::RaceDetector::Initialize());

		ToggleRaceDetection(false);

		FThreadRaceCollector Collector;

		ToggleRaceDetection(true);

		int x = 0;
		TFuture<void> FutureA = AsyncThread([&x]() {
			x = 1;
		});

		x = 2;

		FutureA.Get();

		ToggleRaceDetection(false);

		CHECK(Collector.Contains(&x));
	}

	TEST_CASE_NAMED(FRaceDetectorAsyncThreadAtomicSyncTest, "System::Core::Sanitizer::RaceDetector::AsyncThreadAtomicSync", "[ApplicationContextMask][EngineFilter]")
	{
		CHECK(UE::Sanitizer::RaceDetector::Initialize());

		ToggleRaceDetection(false);

		FThreadRaceCollector Collector;

		int x = 0;
		std::atomic<bool> sync = false;
		TFuture<void> FutureA = AsyncThread([&x, &sync]() {
			ToggleRaceDetection(true);
			x = 1;
			sync.store(true);
			ToggleRaceDetection(false);
		});

		FutureA.Get();

		TFuture<void> FutureB = AsyncThread([&x, &sync]() {
			ToggleRaceDetection(true);
			if (sync.load())
			{
				x = 2;
			}
			ToggleRaceDetection(false);
		});


		FutureB.Get();

		CHECK(!Collector.Contains(&x));
	}

	TEST_CASE_NAMED(FRaceDetectorAsyncThreadAtomicAccessTest, "System::Core::Sanitizer::RaceDetector::AsyncThreadAtomicAccess", "[ApplicationContextMask][EngineFilter]")
	{
		CHECK(UE::Sanitizer::RaceDetector::Initialize());

		ToggleRaceDetection(false);

		FThreadRaceCollector Collector;

		ToggleRaceDetection(true);

		std::atomic<uint32> x = 0;
		TFuture<void> FutureA = AsyncThread([&x]() {
			x = 1;
		});

		TFuture<void> FutureB = AsyncThread([&x]() {
			x = 2;
		});

		FutureA.Get();
		FutureB.Get();

		ToggleRaceDetection(false);

		CHECK(!Collector.Contains(&x));
	}

	TEST_CASE_NAMED(FRaceDetectorVirtualPointerHarmfulTest, "System::Core::Sanitizer::RaceDetector::VirtualPointerHarmful", "[ApplicationContextMask][EngineFilter]")
	{
		CHECK(UE::Sanitizer::RaceDetector::Initialize());

		ToggleRaceDetection(false);

		FThreadRaceCollector Collector;

		ToggleRaceDetection(true);

		class FBase
		{
		public:
			virtual void Function() {}
			void Done() { Event.Notify(); }
			virtual ~FBase() { Event.Wait(); }
		private:
			UE::FManualResetEvent Event;
		};

		class FDerived : public FBase
		{
		public:
			virtual void Function() {}
			virtual ~FDerived() {}
		};

		FBase* Base = new FDerived; 
		TFuture<void> FutureA = AsyncThread([Base] { 
			Base->Function(); 
			Base->Done(); 
		});

		// This is a race because the function called could be the one of FDerived or FBase
		// depending if the call is made before or after we enter the destructor and the vptr
		// is rewritten to point to the base functions.
		delete Base;
		FutureA.Get();

		ToggleRaceDetection(false);

		// For this test, assume the vptr is stored as the first member of the instance.
		CHECK(Collector.Contains(Base));
	}

	TEST_CASE_NAMED(FRaceDetectorVirtualPointerBenignTest, "System::Core::Sanitizer::RaceDetector::VirtualPointerBenign", "[ApplicationContextMask][EngineFilter]")
	{
		CHECK(UE::Sanitizer::RaceDetector::Initialize());

		ToggleRaceDetection(false);

		FThreadRaceCollector Collector;

		ToggleRaceDetection(true);

		class FBase
		{
		public:
			virtual void Function() {}
			void Done() { Event.Notify(); }
			virtual ~FBase() { Event.Wait(); }
		private:
			UE::FManualResetEvent Event;
		};

		FBase* Base = new FBase;
		TFuture<void> FutureA = AsyncThread([Base] {
			Base->Function();
			Base->Done();
		});

		// This race is considered benign since the vptr can only point on the base class
		// so the racedetector won't report it.
		delete Base;
		FutureA.Get();

		ToggleRaceDetection(false);

		// For this test, assume the vptr is stored as the first member of the instance.
		CHECK(!Collector.Contains(Base));
	}

	struct FTestStruct
	{
		int32 Index;
		int32 Constant;
		FTestStruct(int32 InIndex)
			: Index(InIndex)
			, Constant(0xfe05abcd)
		{
		}
	};

	struct FTestRigFIFO
	{
		FLockFreePointerFIFOBase<FTestStruct, PLATFORM_CACHE_LINE_SIZE> Test1;
		FLockFreePointerFIFOBase<FTestStruct, 8> Test2;
		FLockFreePointerFIFOBase<FTestStruct, 8, 1 << 4> Test3;
	};

	struct FTestRigLIFO
	{
		FLockFreePointerListLIFOBase<FTestStruct, PLATFORM_CACHE_LINE_SIZE> Test1;
		FLockFreePointerListLIFOBase<FTestStruct, 8> Test2;
		FLockFreePointerListLIFOBase<FTestStruct, 8, 1 << 4> Test3;
	};


	void Test()
	{
		ToggleRaceDetection(false);
		FThreadRaceCollector Collector;
		ToggleRaceDetection(true);

		TestLockFree();

		ToggleRaceDetection(false);
		Collector.Contains(nullptr);
	}

	void TestLockFree(int32 OuterIters)
	{
		if (!FTaskGraphInterface::IsMultithread())
		{
			UE_LOG(LogConsoleResponse, Display, TEXT("WARNING: TestLockFree disabled for non multi-threading platforms"));
			return;
		}

		const int32 NumWorkers = FTaskGraphInterface::Get().GetNumWorkerThreads();
		// If we have too many threads active at once, they become too slow due to contention.  Set a reasonable maximum for how many are required to guarantee correctness of our LockFreePointers.
		const int32 MaxWorkersForTest = 5;
		const int32 MinWorkersForTest = 2; // With less than two threads we're not testing threading at all, so the test is pointless.
		if (NumWorkers < MinWorkersForTest)
		{
			UE_LOG(LogConsoleResponse, Display, TEXT("WARNING: TestLockFree disabled for current machine because of not enough worker threads.  Need %d, have %d."), MinWorkersForTest, NumWorkers);
			return;
		}

		const uint32 NumWorkersForTest = static_cast<uint32>(FMath::Clamp(NumWorkers, MinWorkersForTest, MaxWorkersForTest));
		auto RunWorkersSynchronous = [NumWorkersForTest](const TFunction<void(uint32)>& WorkerTask)
		{
			FGraphEventArray Tasks;
			for (uint32 Index = 0; Index < NumWorkersForTest; Index++)
			{
				TUniqueFunction<void()> WorkerTaskWithIndex{ [Index, &WorkerTask] { WorkerTask(Index); } };
				Tasks.Add(FFunctionGraphTask::CreateAndDispatchWhenReady(MoveTemp(WorkerTaskWithIndex), TStatId{}, nullptr, ENamedThreads::AnyNormalThreadHiPriTask));
			}
			FTaskGraphInterface::Get().WaitUntilTasksComplete(MoveTemp(Tasks));
		};

		for (int32 Iter = 0; Iter < OuterIters; Iter++)
		{
			{
				UE_LOG(LogTemp, Display, TEXT("******************************* Iter FIFO %d"), Iter);
				FTestRigFIFO Rig;
				for (int32 Index = 0; Index < 1000; Index++)
				{
					Rig.Test1.Push(new FTestStruct(Index));
				}
				TFunction<void(uint32)> Broadcast =
					[&Rig](uint32 WorkerIndex)
				{
					FRandomStream Stream(((int32)WorkerIndex) * 7 + 13);
					for (int32 Index = 0; Index < 100000; Index++)
					{
						if (Stream.FRand() < .03f)
						{
							TArray<FTestStruct*> Items;
							{
								float r = Stream.FRand();
								if (r < .33f)
								{
									Rig.Test1.PopAll(Items);
								}
								else if (r < .66f)
								{
									Rig.Test2.PopAll(Items);
								}
								else
								{
									Rig.Test3.PopAll(Items);
								}
							}
							for (FTestStruct* Item : Items)
							{
								float r = Stream.FRand();
								if (r < .33f)
								{
									Rig.Test1.Push(Item);
								}
								else if (r < .66f)
								{
									Rig.Test2.Push(Item);
								}
								else
								{
									Rig.Test3.Push(Item);
								}
							}
						}
						else
						{
							FTestStruct* Item;
							{
								float r = Stream.FRand();
								if (r < .33f)
								{
									Item = Rig.Test1.Pop();
								}
								else if (r < .66f)
								{
									Item = Rig.Test2.Pop();
								}
								else
								{
									Item = Rig.Test3.Pop();
								}
							}
							if (Item)
							{
								float r = Stream.FRand();
								if (r < .33f)
								{
									Rig.Test1.Push(Item);
								}
								else if (r < .66f)
								{
									Rig.Test2.Push(Item);
								}
								else
								{
									Rig.Test3.Push(Item);
								}
							}
						}
					}
				};
				RunWorkersSynchronous(Broadcast);

				TArray<FTestStruct*> Items;
				Rig.Test1.PopAll(Items);
				Rig.Test2.PopAll(Items);
				Rig.Test3.PopAll(Items);

				checkf(Items.Num() == 1000, TEXT("Items %d"), Items.Num());

				for (int32 LookFor = 0; LookFor < 1000; LookFor++)
				{
					bool bFound = false;
					for (int32 Index = 0; Index < 1000; Index++)
					{
						if (Items[Index]->Index == LookFor && Items[Index]->Constant == 0xfe05abcd)
						{
							check(!bFound);
							bFound = true;
						}
					}
					check(bFound);
				}
				for (FTestStruct* Item : Items)
				{
					delete Item;
				}

				UE_LOG(LogTemp, Display, TEXT("******************************* Pass FTestRigFIFO"));

			}
			{
				UE_LOG(LogTemp, Display, TEXT("******************************* Iter LIFO %d"), Iter);
				FTestRigLIFO Rig;
				for (int32 Index = 0; Index < 1000; Index++)
				{
					Rig.Test1.Push(new FTestStruct(Index));
				}
				TFunction<void(uint32)> Broadcast =
					[&Rig](uint32 WorkerIndex)
				{
					FRandomStream Stream(((int32)WorkerIndex) * 7 + 13);
					for (int32 Index = 0; Index < 100000; Index++)
					{
						if (Index % 200000 == 1)
						{
							//UE_LOG(LogTemp, Log, TEXT("%8d iters thread=%d"), Index, int32(WorkerIndex));
						}
						if (Stream.FRand() < .03f)
						{
							TArray<FTestStruct*> Items;
							{
								float r = Stream.FRand();
								if (r < .33f)
								{
									Rig.Test1.PopAll(Items);
								}
								else if (r < .66f)
								{
									Rig.Test2.PopAll(Items);
								}
								else
								{
									Rig.Test3.PopAll(Items);
								}
							}
							for (FTestStruct* Item : Items)
							{
								float r = Stream.FRand();
								if (r < .33f)
								{
									Rig.Test1.Push(Item);
								}
								else if (r < .66f)
								{
									Rig.Test2.Push(Item);
								}
								else
								{
									Rig.Test3.Push(Item);
								}
							}
						}
						else
						{
							FTestStruct* Item;
							{
								float r = Stream.FRand();
								if (r < .33f)
								{
									Item = Rig.Test1.Pop();
								}
								else if (r < .66f)
								{
									Item = Rig.Test2.Pop();
								}
								else
								{
									Item = Rig.Test3.Pop();
								}
							}
							if (Item)
							{
								float r = Stream.FRand();
								if (r < .33f)
								{
									Rig.Test1.Push(Item);
								}
								else if (r < .66f)
								{
									Rig.Test2.Push(Item);
								}
								else
								{
									Rig.Test3.Push(Item);
								}
							}
						}
					}
				};
				RunWorkersSynchronous(Broadcast);

				TArray<FTestStruct*> Items;
				Rig.Test1.PopAll(Items);
				Rig.Test2.PopAll(Items);
				Rig.Test3.PopAll(Items);

				checkf(Items.Num() == 1000, TEXT("Items %d"), Items.Num());

				for (int32 LookFor = 0; LookFor < 1000; LookFor++)
				{
					bool bFound = false;
					for (int32 Index = 0; Index < 1000; Index++)
					{
						if (Items[Index]->Index == LookFor && Items[Index]->Constant == 0xfe05abcd)
						{
							check(!bFound);
							bFound = true;
						}
					}
					check(bFound);
				}
				for (FTestStruct* Item : Items)
				{
					delete Item;
				}

				UE_LOG(LogTemp, Display, TEXT("******************************* Pass FTestRigLIFO"));
			}
		}
	}
}}

#endif // WITH_TESTS
