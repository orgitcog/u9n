// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 
#include "Async/Fundamental/Task.h"
#include "Async/Fundamental/TaskShared.h"
#include "Async/Fundamental/TaskDelegate.h"
#include "Async/Fundamental/WaitingQueue.h"
#include "Async/Mutex.h"
#include "Async/UniqueLock.h"
#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/List.h"
#include "Containers/ContainerAllocationPolicies.h"
#include "HAL/Event.h"
#include "HAL/PlatformAffinity.h"
#include "HAL/PlatformMutex.h"
#include "HAL/PlatformProcess.h"
#include "HAL/Thread.h"
#include "LocalQueue.h"
#include "Misc/AssertionMacros.h"
#include "Templates/Function.h"
#include "Templates/IsInvocable.h"
#include "Templates/UniquePtr.h"
#include "Templates/UnrealTemplate.h"
#include "Templates/UnrealTypeTraits.h"

#include <atomic>

namespace LowLevelTasks
{
	enum class EQueuePreference
	{
		GlobalQueuePreference,
		LocalQueuePreference,
		DefaultPreference = LocalQueuePreference,
	};

	//implementation of a treiber stack
	//(https://en.wikipedia.org/wiki/Treiber_stack)
	template<typename NodeType>
	class UE_DEPRECATED(5.5, "This class will be removed.") TEventStack
	{
		static constexpr uint32 EVENT_INDEX_NONE = ~0u;

		struct FTopNode
		{
			uint32 EventIndex;		// All events are stored in an array, so this allows us to pack node info into 32 bit as array index
			uint32 Revision;		// Tagging is used to avoid ABA (https://en.wikipedia.org/wiki/ABA_problem#Tagged_state_reference)
		};
		std::atomic<FTopNode> Top { FTopNode{EVENT_INDEX_NONE, 0} };
		TAlignedArray<NodeType>& NodesArray;

		uint32 GetNodeIndex(const NodeType* Node) const
		{
			return (Node == nullptr) ? EVENT_INDEX_NONE : uint32(Node - NodesArray.GetData());
		}

	public:
		TEventStack(TAlignedArray<NodeType>& NodesArray)
			: NodesArray(NodesArray)
		{
		}

		NodeType* Pop()
		{
			FTopNode LocalTop = Top.load(std::memory_order_acquire);
			while (true) 
			{			
				if (LocalTop.EventIndex == EVENT_INDEX_NONE)
				{
					return nullptr;
				}
#if DO_CHECK
				const int64 LastRevision = int64(LocalTop.Revision);
#endif
				NodeType* Item = &NodesArray[LocalTop.EventIndex];
				if (Top.compare_exchange_weak(LocalTop, FTopNode { GetNodeIndex(Item->Next.load(std::memory_order_relaxed)), uint32(LocalTop.Revision + 1) }, std::memory_order_relaxed, std::memory_order_relaxed))
				{
					Item->Next.store(nullptr, std::memory_order_relaxed);
					return Item;
				}
#if DO_CHECK
				const int64 NewRevision = int64(LocalTop.Revision) < LastRevision ? (int64(UINT32_MAX) + int64(LocalTop.Revision)) : int64(LocalTop.Revision);
				ensureMsgf((NewRevision - LastRevision) < (1ll << 31), TEXT("Dangerously close to the wraparound: %" INT64_FMT ", %" INT64_FMT), LastRevision, NewRevision);
#endif
			}
		}

		void Push(NodeType* Item)
		{
			checkSlow(Item != nullptr);
			checkSlow(Item->Next.load(std::memory_order_relaxed) == nullptr);
			checkfSlow((Item >= NodesArray.GetData()) && (Item < (NodesArray.GetData() + NodesArray.Num())), TEXT("Item doesn't belong to a Nodes Array"));

			FTopNode LocalTop = Top.load(std::memory_order_relaxed);
			while (true) 
			{
#if DO_CHECK
				const int64 LastRevision = int64(LocalTop.Revision);
#endif
				Item->Next.store((LocalTop.EventIndex == EVENT_INDEX_NONE) ? nullptr : &NodesArray[LocalTop.EventIndex], std::memory_order_relaxed);
				if (Top.compare_exchange_weak(LocalTop, FTopNode { GetNodeIndex(Item), uint32(LocalTop.Revision + 1) }, std::memory_order_release, std::memory_order_relaxed))
				{
					return;
				}
#if DO_CHECK
				const int64 NewRevision = int64(LocalTop.Revision) < LastRevision ? (int64(UINT32_MAX) + int64(LocalTop.Revision)) : int64(LocalTop.Revision);
				ensureMsgf((NewRevision - LastRevision) < (1ll << 31), TEXT("Dangerously close to the wraparound: %" INT64_FMT ", %" INT64_FMT), LastRevision, NewRevision);
#endif
			}
		}
	};

	class FSchedulerTls
	{
	protected:
		class FImpl;

		using FQueueRegistry	= Private::TLocalQueueRegistry<>;
		using FLocalQueueType	= FQueueRegistry::TLocalQueue;

		enum class EWorkerType
		{
			None,
			Background,
			Foreground,
		};

		struct FTlsValues : public TIntrusiveLinkedList<FTlsValues>
		{
			FSchedulerTls* ActiveScheduler = nullptr;
			FLocalQueueType* LocalQueue = nullptr;
			EWorkerType WorkerType = EWorkerType::None;
			std::atomic<bool> bPendingWakeUp = false;
			bool        bIsStandbyWorker = false;

			inline bool IsBackgroundWorker()
			{
				return WorkerType == EWorkerType::Background;
			}

			inline bool IsStandbyWorker()
			{
				return bIsStandbyWorker;
			}

			inline void SetStandbyWorker(bool bInIsStandbyWorker)
			{
				bIsStandbyWorker = bInIsStandbyWorker;
			}

			static void* operator new(size_t Size);
			static void operator delete(void* Ptr);
		};

		struct FTlsValuesHolder
		{
			CORE_API FTlsValuesHolder();
			CORE_API ~FTlsValuesHolder();
			
			FTlsValues* TlsValues = nullptr;
		};

	private:
		static thread_local FTlsValuesHolder TlsValuesHolder;

	public:
		CORE_API bool IsWorkerThread() const;

		// returns true if the current thread execution is in the context of busy-waiting
		CORE_API static bool IsBusyWaiting();

	protected:
		CORE_API bool HasPendingWakeUp() const;

		static FTlsValues& GetTlsValuesRef();
	};

	class FScheduler final : public FSchedulerTls
	{
		UE_NONCOPYABLE(FScheduler);
		static constexpr uint32 WorkerSpinCycles = 53;

		static CORE_API FScheduler Singleton;

		// using 16 bytes here because it fits the vtable and one additional pointer
		using FConditional = TTaskDelegate<bool(), 16>;

	public: // Public Interface of the Scheduler
		FORCEINLINE_DEBUGGABLE static FScheduler& Get();

		//start number of workers where 0 is the system default
		CORE_API void StartWorkers(uint32 NumForegroundWorkers = 0, uint32 NumBackgroundWorkers = 0, FThread::EForkable IsForkable = FThread::NonForkable, EThreadPriority InWorkerPriority = EThreadPriority::TPri_Normal, EThreadPriority InBackgroundPriority = EThreadPriority::TPri_BelowNormal, uint64 InWorkerAffinity = 0, uint64 InBackgroundAffinity = 0);
		CORE_API void StopWorkers(bool DrainGlobalQueue = true);
		CORE_API void RestartWorkers(uint32 NumForegroundWorkers = 0, uint32 NumBackgroundWorkers = 0, FThread::EForkable IsForkable = FThread::NonForkable, EThreadPriority WorkerPriority = EThreadPriority::TPri_Normal, EThreadPriority BackgroundPriority = EThreadPriority::TPri_BelowNormal, uint64 InWorkerAffinity = 0, uint64 InBackgroundAffinity = 0);

		//try to launch the task, the return value will specify if the task was in the ready state and has been launched
		inline bool TryLaunch(FTask& Task, EQueuePreference QueuePreference = EQueuePreference::DefaultPreference, bool bWakeUpWorker = true);

		//tries to do some work until the Task is completed
		template<typename TaskType>
		UE_DEPRECATED(5.5, "This method will be removed.")
		inline void BusyWait(const TaskType& Task, bool ForceAllowBackgroundWork = false);

		//tries to do some work until the Conditional return true
		template<typename Conditional>
		UE_DEPRECATED(5.5, "This method will be removed.")
		inline void BusyWaitUntil(Conditional&& Cond, bool ForceAllowBackgroundWork = false);

		//tries to do some work until all the Tasks are completed
		//the template parameter can be any Type that has a const conversion operator to FTask
		template<typename TaskType>
		UE_DEPRECATED(5.5, "This method will be removed.")
		inline void BusyWait(const TArrayView<const TaskType>& Tasks, bool ForceAllowBackgroundWork = false);

		//number of instantiated workers
		inline uint32 GetNumWorkers() const;

		//get the worker priority set when workers were started
		inline EThreadPriority GetWorkerPriority() const { return WorkerPriority; }

		//get the background priority set when workers were started
		inline EThreadPriority GetBackgroundPriority() const { return BackgroundPriority; }

		//determine if we're currently out of workers for a given task priority
		CORE_API bool IsOversubscriptionLimitReached(ETaskPriority TaskPriority) const;

		//event that will fire when the scheduler has reached its oversubscription limit (all threads are waiting).
		//note: This event can be broadcasted from any thread so the receiver needs to be thread-safe
		//      For optimal performance, avoid binding UObjects to this event and use AddRaw/AddLambda instead.
		//      Also, what's happening inside that callback should be as brief and simple as possible (i.e. raising an event)
		CORE_API FOversubscriptionLimitReached& GetOversubscriptionLimitReachedEvent();
	public:
		FScheduler() = default;
		~FScheduler();

	private: 
		[[nodiscard]] FTask* ExecuteTask(FTask* InTask);
		TUniquePtr<FThread> CreateWorker(uint32 WorkerId, const TCHAR* Name, bool bPermitBackgroundWork = false, FThread::EForkable IsForkable = FThread::NonForkable, Private::FWaitEvent* ExternalWorkerEvent = nullptr, FSchedulerTls::FLocalQueueType* ExternalWorkerLocalQueue = nullptr, EThreadPriority Priority = EThreadPriority::TPri_Normal, uint64 InAffinity = 0);
		void WorkerMain(Private::FWaitEvent* WorkerEvent, FSchedulerTls::FLocalQueueType* ExternalWorkerLocalQueue, uint32 WaitCycles, bool bPermitBackgroundWork);
		void StandbyLoop(Private::FWaitEvent* WorkerEvent, FSchedulerTls::FLocalQueueType* ExternalWorkerLocalQueue, uint32 WaitCycles, bool bPermitBackgroundWork);
		void WorkerLoop(Private::FWaitEvent* WorkerEvent, FSchedulerTls::FLocalQueueType* ExternalWorkerLocalQueue, uint32 WaitCycles, bool bPermitBackgroundWork);
		CORE_API void LaunchInternal(FTask& Task, EQueuePreference QueuePreference, bool bWakeUpWorker);
		CORE_API void BusyWaitInternal(const FConditional& Conditional, bool ForceAllowBackgroundWork);
		inline bool WakeUpWorker(bool bBackgroundWorker);
		CORE_API void IncrementOversubscription();
		CORE_API void DecrementOversubscription();
		template<typename QueueType, FTask* (QueueType::*DequeueFunction)(bool), bool bIsStandbyWorker>
		bool TryExecuteTaskFrom(Private::FWaitEvent* WaitEvent, QueueType* Queue, Private::FOutOfWork& OutOfWork, bool bPermitBackgroundWork);

		friend class FOversubscriptionScope;
	private:
		Private::FWaitingQueue                         WaitingQueue[2] = { { WorkerEvents, OversubscriptionLimitReachedEvent }, { WorkerEvents, OversubscriptionLimitReachedEvent } };
		FSchedulerTls::FQueueRegistry                  QueueRegistry;
		UE::FPlatformRecursiveMutex                    WorkerCreationCS;
		UE::FPlatformRecursiveMutex                    WorkerThreadsCS;
		TArray<std::atomic<FThread*>>                  WorkerThreads;
		TAlignedArray<FSchedulerTls::FLocalQueueType>  WorkerLocalQueues;
		TAlignedArray<Private::FWaitEvent>             WorkerEvents;
		TUniquePtr<FSchedulerTls::FLocalQueueType>     GameThreadLocalQueue;
		std::atomic_uint                               ActiveWorkers { 0 };
		std::atomic_uint                               NextWorkerId { 0 };
		std::atomic<int32>                             ForegroundCreationIndex{ 0 };
		std::atomic<int32>                             BackgroundCreationIndex{ 0 };
		uint64                                         WorkerAffinity = 0;
		uint64                                         BackgroundAffinity = 0;
		EThreadPriority                                WorkerPriority = EThreadPriority::TPri_Normal;
		EThreadPriority                                BackgroundPriority = EThreadPriority::TPri_BelowNormal;
		std::atomic_bool                               TemporaryShutdown{ false };
		FOversubscriptionLimitReached                  OversubscriptionLimitReachedEvent;
	};

	namespace Private
	{
		class FOversubscriptionTls
		{
			static thread_local bool bIsOversubscriptionAllowed;

			friend class FOversubscriptionAllowedScope;

			CORE_API static bool& GetIsOversubscriptionAllowedRef();

		public:
			static bool IsOversubscriptionAllowed() { return bIsOversubscriptionAllowed; }
		};

		class FOversubscriptionAllowedScope
		{
			UE_NONCOPYABLE(FOversubscriptionAllowedScope);

		public:
			explicit FOversubscriptionAllowedScope(bool bIsOversubscriptionAllowed)
				: bValue(FOversubscriptionTls::GetIsOversubscriptionAllowedRef())
				, bPreviousValue(bValue)
			{
				bValue = bIsOversubscriptionAllowed;
			}

			~FOversubscriptionAllowedScope()
			{
				bValue = bPreviousValue;
			}

		private:
			bool& bValue;
			bool bPreviousValue;
		};
	}

	class FOversubscriptionScope
	{
		UE_NONCOPYABLE(FOversubscriptionScope);

	public:
		FOversubscriptionScope(bool bCondition = true)
		{
			if (bCondition && Private::FOversubscriptionTls::IsOversubscriptionAllowed())
			{
				bIncrementOversubscriptionEmitted = true;

#if CPUPROFILERTRACE_ENABLED
				if (CpuChannel)
				{
					static uint32 OversubscriptionTraceId = FCpuProfilerTrace::OutputEventType("Oversubscription");
					FCpuProfilerTrace::OutputBeginEvent(OversubscriptionTraceId);
					bCpuBeginEventEmitted = true;
				}
#endif
				FScheduler::Get().IncrementOversubscription();
			}
		}

		~FOversubscriptionScope()
		{
			if (bIncrementOversubscriptionEmitted)
			{
				FScheduler::Get().DecrementOversubscription();

#if CPUPROFILERTRACE_ENABLED
				if (bCpuBeginEventEmitted)
				{
					FCpuProfilerTrace::OutputEndEvent();
					bCpuBeginEventEmitted = false;
				}
#endif
			}
		}
	private:
		bool bIncrementOversubscriptionEmitted = false;
		bool bCpuBeginEventEmitted = false;
	};

	FORCEINLINE_DEBUGGABLE bool TryLaunch(FTask& Task, EQueuePreference QueuePreference = EQueuePreference::DefaultPreference, bool bWakeUpWorker = true)
	{
		return FScheduler::Get().TryLaunch(Task, QueuePreference, bWakeUpWorker);
	}

	UE_DEPRECATED(5.5, "This method will be removed.")
	FORCEINLINE_DEBUGGABLE void BusyWaitForTask(const FTask& Task, bool ForceAllowBackgroundWork = false)
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		FScheduler::Get().BusyWait(Task, ForceAllowBackgroundWork);
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	template<typename Conditional>
	UE_DEPRECATED(5.5, "This method will be removed.")
	FORCEINLINE_DEBUGGABLE void BusyWaitUntil(Conditional&& Cond, bool ForceAllowBackgroundWork = false)
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		FScheduler::Get().BusyWaitUntil<Conditional>(Forward<Conditional>(Cond), ForceAllowBackgroundWork);
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	template<typename TaskType>
	UE_DEPRECATED(5.5, "This method will be removed.")
	FORCEINLINE_DEBUGGABLE void BusyWaitForTasks(const TArrayView<const TaskType>& Tasks, bool ForceAllowBackgroundWork = false)
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		FScheduler::Get().BusyWait<TaskType>(Tasks, ForceAllowBackgroundWork);
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	/******************
	* IMPLEMENTATION *
	******************/
	inline bool FScheduler::TryLaunch(FTask& Task, EQueuePreference QueuePreference, bool bWakeUpWorker)
	{
		if(Task.TryPrepareLaunch())
		{
			LaunchInternal(Task, QueuePreference, bWakeUpWorker);
			return true;
		}
		return false;
	}

	inline uint32 FScheduler::GetNumWorkers() const
	{
		return ActiveWorkers.load(std::memory_order_relaxed);
	}

	template<typename TaskType>
	inline void FScheduler::BusyWait(const TaskType& Task, bool ForceAllowBackgroundWork)
	{
		if(!Task.IsCompleted())
		{
			FScheduler::BusyWaitInternal([&Task](){ return Task.IsCompleted(); }, ForceAllowBackgroundWork);
		}
	}

	template<typename Conditional>
	inline void FScheduler::BusyWaitUntil(Conditional&& Cond, bool ForceAllowBackgroundWork)
	{
		static_assert(TIsInvocable<Conditional>::Value, "Conditional is not invocable");
		static_assert(std::is_same_v<decltype(Cond()), bool>, "Conditional must return a boolean");
		
		if(!Cond())
		{
			FScheduler::BusyWaitInternal(Forward<Conditional>(Cond), ForceAllowBackgroundWork);
		}
	}

	template<typename TaskType>
	inline void FScheduler::BusyWait(const TArrayView<const TaskType>& Tasks, bool ForceAllowBackgroundWork)
	{
		auto AllTasksCompleted = [Index(0), &Tasks]() mutable
		{
			while (Index < Tasks.Num())
			{
				if (!Tasks[Index].IsCompleted())
				{
					return false;
				}
				Index++;
			}
			return true;
		};

		if (!AllTasksCompleted())
		{
			FScheduler::BusyWaitInternal([&AllTasksCompleted](){ return AllTasksCompleted(); }, ForceAllowBackgroundWork);
		}
	}

	inline bool FScheduler::WakeUpWorker(bool bBackgroundWorker)
	{
		return WaitingQueue[bBackgroundWorker].Notify() != 0;
	}

	inline FScheduler& FScheduler::Get()
	{
		return Singleton;
	}

	inline FScheduler::~FScheduler()
	{
		StopWorkers();
	}
}

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_6
#include "HAL/CriticalSection.h"
#endif
