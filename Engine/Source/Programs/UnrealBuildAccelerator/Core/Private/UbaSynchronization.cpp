// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaSynchronization.h"
#include "UbaPlatform.h"
#include "UbaStringBuffer.h"
#include <shared_mutex>

#define UBA_USE_SHARED_MUTEX 0

namespace uba
{
	CriticalSection::CriticalSection(bool recursive)
	{
		#if PLATFORM_WINDOWS
		static_assert(alignof(CRITICAL_SECTION) == alignof(CriticalSection));
		static_assert(sizeof(data) >= sizeof(CRITICAL_SECTION));
		InitializeCriticalSection((CRITICAL_SECTION*)&data);
		#else
		static_assert(alignof(pthread_mutex_t) == alignof(CriticalSection));
		static_assert(sizeof(data) >= sizeof(pthread_mutex_t));
		int res;
		if (recursive)
		{
			pthread_mutexattr_t attr;
			pthread_mutexattr_init(&attr);
			pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
			res = pthread_mutex_init((pthread_mutex_t*)data, &attr);
		}
		else
			res = pthread_mutex_init((pthread_mutex_t*)data, nullptr);

		UBA_ASSERTF(res == 0, TC("pthread_mutex_init failed: %i"), res);(void)res;
		#endif
	}

	CriticalSection::~CriticalSection()
	{
		#if PLATFORM_WINDOWS
		#if UBA_DEBUG
		if (TryEnterCriticalSection((CRITICAL_SECTION*)&data))
			LeaveCriticalSection((CRITICAL_SECTION*)&data);
		else
			UBA_ASSERT(false);
		#endif
		DeleteCriticalSection((CRITICAL_SECTION*)&data);
		#else
		int res = pthread_mutex_destroy((pthread_mutex_t*)data);(void)res;
		UBA_ASSERTF(res == 0, TC("pthread_mutex_destroy failed: %i"), res);
		#endif
	}

	void CriticalSection::Enter() const
	{
		#if PLATFORM_WINDOWS
		EnterCriticalSection((CRITICAL_SECTION*)&data);
		#else
		int res = pthread_mutex_lock((pthread_mutex_t*)data);(void)res;
		UBA_ASSERTF(res == 0, TC("pthread_mutex_lock failed: %i"), res);
		#endif
	}

	void CriticalSection::Leave() const
	{
		#if PLATFORM_WINDOWS
		LeaveCriticalSection((CRITICAL_SECTION*)&data);
		#else
		int res = pthread_mutex_unlock((pthread_mutex_t*)data);(void)res;
		UBA_ASSERTF(res == 0, TC("pthread_mutex_unlock failed: %i"), res);
		#endif
	}

	ReaderWriterLock::ReaderWriterLock()
	{
		#if PLATFORM_WINDOWS
		static_assert(sizeof(data) >= sizeof(SRWLOCK));
		static_assert(alignof(SRWLOCK) == alignof(ReaderWriterLock));
		*(SRWLOCK*)&data = SRWLOCK_INIT;
		#elif UBA_USE_SHARED_MUTEX
		static_assert(alignof(std::shared_mutex) == alignof(ReaderWriterLock));
		static_assert(sizeof(data) >= sizeof(std::shared_mutex));
		new (data) std::shared_mutex();
		#else
		static_assert(alignof(pthread_rwlock_t) == alignof(ReaderWriterLock));
		static_assert(sizeof(data) >= sizeof(pthread_rwlock_t));
		int res = pthread_rwlock_init((pthread_rwlock_t*)data, NULL);(void)res;
		UBA_ASSERTF(res == 0, TC("pthread_rwlock_init failed: %i"), res);
		#endif
	}

	ReaderWriterLock::~ReaderWriterLock()
	{
		#if PLATFORM_WINDOWS
		#if UBA_DEBUG
		if (TryAcquireSRWLockExclusive((SRWLOCK*)&data))
			ReleaseSRWLockExclusive((SRWLOCK*)&data);
		else
			UBA_ASSERT(false);
		#endif
		#elif UBA_USE_SHARED_MUTEX
		((std::shared_mutex*)data)->~shared_mutex();
		#else
		int res = pthread_rwlock_destroy((pthread_rwlock_t*)data);(void)res;
		UBA_ASSERTF(res == 0, TC("pthread_rwlock_destroy failed: %i"), res);
		#endif
	}

	void ReaderWriterLock::EnterRead() const
	{
		#if PLATFORM_WINDOWS
		AcquireSRWLockShared((SRWLOCK*)&data);
		#elif UBA_USE_SHARED_MUTEX
		((std::shared_mutex*)data)->lock_shared();
		#else
		int res = pthread_rwlock_rdlock((pthread_rwlock_t*)data);(void)res;
		UBA_ASSERTF(res == 0, TC("pthread_rwlock_rdlock failed: %i"), res);
		#endif
	}

	void ReaderWriterLock::LeaveRead() const
	{
		#if PLATFORM_WINDOWS
		ReleaseSRWLockShared((SRWLOCK*)&data);
		#elif UBA_USE_SHARED_MUTEX
		((std::shared_mutex*)data)->unlock_shared();
		#else
		int res = pthread_rwlock_unlock((pthread_rwlock_t*)data);(void)res;
		UBA_ASSERTF(res == 0, TC("pthread_rwlock_unlock failed: %i"), res);
		#endif
	}

	void ReaderWriterLock::Enter()
	{
		#if PLATFORM_WINDOWS
		AcquireSRWLockExclusive((SRWLOCK*)&data);
		#elif UBA_USE_SHARED_MUTEX
		((std::shared_mutex*)data)->lock();
		#else
		int res = pthread_rwlock_wrlock((pthread_rwlock_t*)data);(void)res;
		UBA_ASSERTF(res == 0, TC("pthread_rwlock_wrlock failed: %i"), res);
		#endif
	}

	bool ReaderWriterLock::TryEnter()
	{
		#if PLATFORM_WINDOWS
		return TryAcquireSRWLockExclusive((SRWLOCK*)&data) != 0;
		#elif UBA_USE_SHARED_MUTEX
		return ((std::shared_mutex*)data)->try_lock();
		#else
		return pthread_rwlock_trywrlock((pthread_rwlock_t*)data) == 0;
		#endif
	}

	void ReaderWriterLock::Leave()
	{
		#if PLATFORM_WINDOWS
		ReleaseSRWLockExclusive((SRWLOCK*)&data);
		#elif UBA_USE_SHARED_MUTEX
		((std::shared_mutex*)data)->unlock();
		#else
		int res = pthread_rwlock_unlock((pthread_rwlock_t*)data);(void)res;
		UBA_ASSERTF(res == 0, TC("pthread_rwlock_unlock failed: %i"), res);
		#endif
	}

	#if UBA_TRACK_CONTENTION

	List<ContentionTracker>& GetContentionTrackerList()
	{
		static List<ContentionTracker> trackers;
		return trackers;
	}


	ContentionTracker& GetContentionTracker(const char* file, u64 line)
	{
		static Futex rwl;
		ScopedFutex l(rwl);
		ContentionTracker& ct = GetContentionTrackerList().emplace_back();
		ct.file = file;
		ct.line = line;
		return ct;
	}
	#endif
}
