// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaEvent.h"
#include "UbaPlatform.h"
#include "UbaStringBuffer.h"
#include "UbaSynchronization.h"

#if !PLATFORM_WINDOWS
#include <sys/time.h>
#include <new>
#endif

#define UBA_TEST_WAIT_QUALITY 0

namespace uba
{
#if !PLATFORM_WINDOWS

	u64 GetMonoticTimeNs();

	struct EventImpl
	{
		EventImpl() = default;
		~EventImpl() { Destroy(); }

		bool Create(bool mr, bool shared = false)
		{
			static_assert(Atomic<u32>::is_always_lock_free, "atomic<T> not lock free, can't work in shared mem");
			static_assert(Atomic<TriggerType>::is_always_lock_free, "atomic<T> not lock free, can't work in shared mem");
			static_assert(Atomic<bool>::is_always_lock_free, "atomic<T> not lock free, can't work in shared mem");

			UBA_ASSERTF(!m_initialized, "Can't create already created Event");
			m_manualReset = mr;

			pthread_mutexattr_t attrmutex;
			if (pthread_mutexattr_init(&attrmutex) != 0)
			{
				UBA_ASSERTF(false, "pthread_mutexattr_init failed");
				return false;
			}

			if (shared)
			{
				if (pthread_mutexattr_setpshared(&attrmutex, PTHREAD_PROCESS_SHARED) != 0)
				{
					UBA_ASSERTF(false, "pthread_mutexattr_setpshared failed");
					return false;
				}
				#if PLATFORM_LINUX
				if (pthread_mutexattr_setrobust(&attrmutex, PTHREAD_MUTEX_ROBUST) != 0)
				{
					UBA_ASSERTF(false, "pthread_mutexattr_setrobust failed");
					return false;
				}
				#endif
			}

			if (pthread_mutex_init(&m_mutex, &attrmutex) != 0)
			{
				UBA_ASSERTF(false, "pthread_mutex_init failed");
				return false;
			}
			pthread_mutexattr_destroy(&attrmutex);

			pthread_condattr_t attrcond;
			if (pthread_condattr_init(&attrcond) != 0)
			{
				UBA_ASSERTF(false, "pthread_condattr_init failed");
				return false;
			}

			#if PLATFORM_LINUX
			if (pthread_condattr_setclock(&attrcond, CLOCK_MONOTONIC) != 0)
			{
				UBA_ASSERTF(false, "pthread_condattr_setclock failed");
				return false;
			}
			#endif

			if (shared)
			{
				if (pthread_condattr_setpshared(&attrcond, PTHREAD_PROCESS_SHARED) != 0)
				{
					UBA_ASSERTF(false, "pthread_condattr_setpshared failed");
					return false;
				}
			}

			if (pthread_cond_init(&m_condition, &attrcond) != 0)
			{
				UBA_ASSERTF(false, "pthread_cond_init failed");
				pthread_mutex_destroy(&m_mutex);
				return false;
			}
			pthread_condattr_destroy(&attrcond);

			m_initialized = true;
			return true;
		}

		void Destroy()
		{
			if (!m_initialized)
				return;

			LockEventMutex();
			m_manualReset = true;
			UnlockEventMutex();
			Set();

			LockEventMutex();
			m_initialized = false;
			while (m_waitingThreads)
			{
				UnlockEventMutex();
				LockEventMutex();
			}

			pthread_cond_destroy(&m_condition);
			UnlockEventMutex();
			pthread_mutex_destroy(&m_mutex);
		}

		void Set()
		{
			if (!m_initialized)
				return;
			LockEventMutex();

			if (m_manualReset)
			{
				m_triggered = TriggerType_All;
				if (pthread_cond_broadcast(&m_condition) != 0)
					UBA_ASSERTF(false, "pthread_cond_broadcast failed");
			}
			else
			{
				m_triggered = TriggerType_One;
				if (pthread_cond_signal(&m_condition) != 0)  // may release multiple threads anyhow!
					UBA_ASSERTF(false, "pthread_cond_signal failed");
			}

			UnlockEventMutex();
		}

		void Reset()
		{
			if (!m_initialized)
				return;
			LockEventMutex();
			m_triggered = TriggerType_None;
			UnlockEventMutex();
		}

		inline u64 ToNanoSeconds(const struct timespec& ts)
		{
			return u64(ts.tv_sec)*1'000'000'000 + ts.tv_nsec;
		}

		inline struct timespec ToTimeSpec(u64 nanoSeconds)
		{
			struct timespec ts;
			ts.tv_sec = nanoSeconds / 1'000'000'000;
			ts.tv_nsec = nanoSeconds % 1'000'000'000;
			return ts;
		}

		bool IsCreated()
		{
			return m_initialized;
		}

		bool IsSet(u32 timeoutMs = ~0u)
		{
			if (!m_initialized)
				return false;

			u64 startTimeNs = 0;

			// We need to know the start time if we're going to do a timed wait.
			if ((timeoutMs > 0) && (timeoutMs != ~0u))  // not polling and not infinite wait.
				startTimeNs = GetMonoticTimeNs();

			u64 timeoutNs = u64(timeoutMs) * 1'000'000;

			#if UBA_TEST_WAIT_QUALITY
			u32 loop = 0;
			bool timedOut = false;
			u64 initialStartTimeNs = startTimeNs;
			u64 initialTimeoutNs = timeoutNs;
			auto qg = MakeGuard([&]()
				{
					if (startTimeNs && timedOut)
					{
						u64 overtimeNs = GetMonoticTimeNs() - initialStartTimeNs - initialTimeoutNs;
						printf("Loops: %u TimeOut: %ums Over-wait: %lluus\n", loop, timeoutMs, overtimeNs/1000);
					}
				});
			#endif

			LockEventMutex();
			auto unlock = MakeGuard([this]() { UnlockEventMutex(); });

			// loop in case we fall through the Condition signal but someone else claims the event.
			do
			{
				if (m_triggered == TriggerType_One)
				{
					m_triggered = TriggerType_None;
					return true;
				}

				if (m_triggered == TriggerType_All)
				{
					return true;
				}

				// No event signalled yet.
				if (timeoutNs != 0)  // not just polling, wait on the condition variable.
				{
					++m_waitingThreads;
					if (timeoutMs == ~0u) // infinite wait?
					{
						if (pthread_cond_wait(&m_condition, &m_mutex) != 0)  // unlocks Mutex while blocking...
							UBA_ASSERTF(false, "pthread_cond_wait failed");
					}
					else  // timed wait.
					{
#if PLATFORM_MAC
						struct timespec timeout = ToTimeSpec(timeoutNs);
						int rc = pthread_cond_timedwait_relative_np(&m_condition, &m_mutex, &timeout); // unlocks Mutex while blocking...
						UBA_ASSERTF((rc == 0) || (rc == ETIMEDOUT), "pthread_cond_timedwait_relative_np failed"); (void)rc;
#else
						struct timespec timeout = ToTimeSpec(startTimeNs + timeoutNs);
						int rc = pthread_cond_timedwait(&m_condition, &m_mutex, &timeout); // unlocks Mutex while blocking...
						UBA_ASSERTF((rc == 0) || (rc == ETIMEDOUT), "pthread_cond_timedwait failed"); (void)rc;
#endif
						#if UBA_TEST_WAIT_QUALITY
						++loop;
						#endif

						u64 nowNs = GetMonoticTimeNs();
						u64 diffNs = nowNs - startTimeNs;

						timeoutNs = diffNs >= timeoutNs ? 0 : (timeoutNs - diffNs);
						startTimeNs = nowNs;
					}
					--m_waitingThreads;
					UBA_ASSERTF(m_waitingThreads >= 0, "m_waitingThreads less than 0");
				}

			} while (timeoutNs != 0);

			#if UBA_TEST_WAIT_QUALITY
			timedOut = true;
			#endif
			return false;
		}

		bool GetManualReset() { return m_manualReset; }
		void ResetManualReset(bool mr) {}


		EventImpl* next;

	private:
		enum TriggerType : u8 { TriggerType_None, TriggerType_One, TriggerType_All };

		Atomic<bool> m_initialized;
		Atomic<bool> m_manualReset;
		Atomic<TriggerType> m_triggered;
		Atomic<u32> m_waitingThreads;
		pthread_mutex_t m_mutex;
		pthread_cond_t m_condition;

		void LockEventMutex()
		{
			int res = pthread_mutex_lock(&m_mutex);(void)res;
			UBA_ASSERTF(res == 0, "pthread_mutex_lock failed (error code %i)", res);
		}

		void UnlockEventMutex()
		{
			int res = pthread_mutex_unlock(&m_mutex);
			UBA_ASSERTF(res == 0, "pthread_mutex_unlock failed (error code %i)", res);
		}
	};

#else
	struct EventImpl
	{
		HANDLE handle;
		union
		{
			EventImpl* next;
			bool manualReset;
		};

		bool Create(bool mr) { handle = CreateEvent(nullptr, mr, false, NULL); return handle != 0; }
		void Set() { SetEvent(handle); }
		void Reset() { ResetEvent(handle); }
		bool IsSet(u32 timeOutMs) { return WaitForSingleObject(handle, timeOutMs) == 0;} 
		bool GetManualReset() { return manualReset; }
		void ResetManualReset(bool mr) { manualReset = mr; }
	};
#endif

	Futex g_firstEventLock[2];
	EventImpl* g_firstEvent[2];


	Event::Event(bool manualReset)
	{
		Create(manualReset);
	}

	Event::~Event()
	{
		Destroy();
	}

	bool Event::Create(bool manualReset)
	{
		EventImpl* impl = nullptr;
		SCOPED_FUTEX(g_firstEventLock[manualReset], lock);
		auto*& first = g_firstEvent[manualReset];
		if (first)
		{
			impl = first;
			first = impl->next;
		}
		lock.Leave();

		if (!impl)
		{
			impl = new EventImpl();
			impl->Create(manualReset);
		}
		else
			impl->Reset();

		impl->ResetManualReset(manualReset);
		m_impl = impl;
		return true;
	}

	void Event::Destroy()
	{
		if (!m_impl)
			return;
		auto impl = m_impl;
		m_impl = nullptr;
		bool manualReset = impl->GetManualReset();
		auto*& first = g_firstEvent[manualReset];
		SCOPED_FUTEX(g_firstEventLock[manualReset], l);
		impl->next = first;
		first = impl;
	}

	void Event::Set()
	{
		if (m_impl)
			m_impl->Set();
	}

	void Event::Reset()
	{
		if (m_impl)
			m_impl->Reset();
	}

	bool Event::IsCreated()
	{
		return m_impl != nullptr;
	}

	bool Event::IsSet(u32 timeOutMs)
	{
		if (m_impl)
			return m_impl->IsSet(timeOutMs);
		return false;
	}

	void* Event::GetHandle()
	{
		#if PLATFORM_WINDOWS
		if (!m_impl)
			return nullptr;
		return m_impl->handle;
		#else
		UBA_ASSERTF(false, "Event::GetHandle not available");
		return 0;
		#endif
	}


#if !PLATFORM_WINDOWS
	SharedEvent::SharedEvent()
	{
		static_assert(sizeof(m_data) >= sizeof(EventImpl));
		new (m_data) EventImpl();
	}

	SharedEvent::SharedEvent(bool manualReset)
	{
		new (m_data) EventImpl();
		Create(manualReset);
	}

	SharedEvent::~SharedEvent()
	{
		Destroy();
		((EventImpl&)m_data).~EventImpl();
	}

	bool SharedEvent::Create(bool manualReset)//, bool shared)
	{
		return ((EventImpl&)m_data).Create(manualReset, true);
	}

	void SharedEvent::Destroy()
	{
		((EventImpl&)m_data).Destroy();
	}

	void SharedEvent::Set()
	{
		((EventImpl&)m_data).Set();
	}

	void SharedEvent::Reset()
	{
		((EventImpl&)m_data).Reset();
	}

	bool SharedEvent::IsCreated()
	{
		return ((EventImpl&)m_data).IsCreated();
	}

	bool SharedEvent::IsSet(u32 timeOutMs)
	{
		return ((EventImpl&)m_data).IsSet(timeOutMs);
	}

	void* SharedEvent::GetHandle()
	{
		UBA_ASSERTF(false, "Event::GetHandle not available");
		return 0;
	}
#endif
}
