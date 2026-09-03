// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaWorkManager.h"
#include "UbaEvent.h"
#include "UbaPlatform.h"
#include "UbaThread.h"

namespace uba
{
	u32 WorkManager::TrackWorkStart(const StringView& desc, const Color& color)
	{
		if (auto t = m_workTracker.load())
			return t->TrackWorkStart(desc, color);
		return 0;
	}

	void WorkManager::TrackWorkHint(u32 id, const StringView& hint, u64 startTime)
	{
		if (auto t = m_workTracker.load())
			return t->TrackWorkHint(id, hint, startTime);
	}

	void WorkManager::TrackWorkEnd(u32 id)
	{
		if (auto t = m_workTracker.load())
			return t->TrackWorkEnd(id);
	}


	struct WorkManagerImpl::Worker
	{
		Worker(WorkManagerImpl& manager, const tchar* workerDesc) : m_workAvailable(false)
		{
			m_loop = true;
			manager.PushWorker(this);
			m_thread.Start([&]() { ThreadWorker(manager); return 0; }, workerDesc);
		}
		~Worker()
		{
			m_thread.Wait();
		}

		void Stop()
		{
			m_loop = false;
			m_workAvailable.Set();
		}

		void ThreadWorker(WorkManagerImpl& manager)
		{
			while (true)
			{
				if (!m_workAvailable.IsSet())
					break;
				if (!m_loop)
					break;

				while (true)
				{
					while (true)
					{
						SCOPED_FUTEX(manager.m_workLock, lock);
						if (manager.m_work.empty())
							break;
						Work work = manager.m_work.front();
						manager.m_work.pop_front();
						lock.Leave();

						#if UBA_TRACK_WORK
						TrackWorkScope tws(manager, work.desc);
						#else
						TrackWorkScope tws;
						#endif
						work.func({tws});
					}

					SCOPED_FUTEX(manager.m_availableWorkersLock, lock1);
					SCOPED_FUTEX_READ(manager.m_workLock, lock2);
					if (!manager.m_work.empty())
						continue;

					manager.PushWorkerNoLock(this);
					break;
				}
			}
		}

		Worker* m_nextWorker = nullptr;
		Worker* m_prevWorker = nullptr;
		Atomic<bool> m_loop;
		Event m_workAvailable;
		Thread m_thread;
	};

	WorkManagerImpl::WorkManagerImpl(u32 workerCount, const tchar* workerDesc)
	{
		m_workers.resize(workerCount);
		m_activeWorkerCount = workerCount;
		for (u32 i = 0; i != workerCount; ++i)
			m_workers[i] = new Worker(*this, workerDesc);
	}

	WorkManagerImpl::~WorkManagerImpl()
	{
		for (auto worker : m_workers)
			worker->Stop();
		for (auto worker : m_workers)
			delete worker;
	}


	void WorkManagerImpl::AddWork(const WorkFunction& work, u32 count, const tchar* desc, const Color& color, bool highPriority)
	{
		UBA_ASSERT(*desc);
		UBA_ASSERT(!m_workers.empty());
		SCOPED_FUTEX(m_workLock, lock);
		bool trackWork = m_workTracker.load();
		for (u32 i = 0; i != count; ++i)
		{
			if (highPriority)
			{
				m_work.push_front({ work });
				if (trackWork)
					m_work.front().desc = desc;
			}
			else
			{
				m_work.push_back({ work });
				if (trackWork)
					m_work.back().desc = desc;
			}
		}
		lock.Leave();

		SCOPED_FUTEX(m_availableWorkersLock, lock2);
		while (count--)
		{
			Worker* worker = PopWorkerNoLock();
			if (!worker)
				break;
			worker->m_workAvailable.Set();
		}
	}

	u32 WorkManagerImpl::GetWorkerCount()
	{
		return u32(m_workers.size());
	}

	void WorkManagerImpl::PushWorker(Worker* worker)
	{
		SCOPED_FUTEX(m_availableWorkersLock, lock);
		PushWorkerNoLock(worker);
	}

	void WorkManagerImpl::PushWorkerNoLock(Worker* worker)
	{
		if (m_firstAvailableWorker)
			m_firstAvailableWorker->m_prevWorker = worker;
		worker->m_prevWorker = nullptr;
		worker->m_nextWorker = m_firstAvailableWorker;
		m_firstAvailableWorker = worker;
		--m_activeWorkerCount;
	}

	void WorkManagerImpl::DoWork(u32 count)
	{
		while (count--)
		{
			SCOPED_FUTEX(m_workLock, lock);
			if (m_work.empty())
				break;
			Work work = m_work.front();
			m_work.pop_front();
			lock.Leave();

			#if UBA_TRACK_WORK
			TrackWorkScope tws(*this, work.desc);
			#else
			TrackWorkScope tws;
			#endif
			work.func({tws});
		}
	}

	bool WorkManagerImpl::FlushWork(u32 timeoutMs)
	{
		u64 startTime = GetTime();
		auto hasTimedOut = [&]() { return timeoutMs ? (timeoutMs < TimeToMs(GetTime() - startTime)) : false; };

		while (true)
		{
			SCOPED_FUTEX_READ(m_workLock, lock);
			bool workEmpty = m_work.empty();
			lock.Leave();
			if (workEmpty)
				break;
			if (hasTimedOut())
				return false;
			Sleep(5);
		}

		while (m_activeWorkerCount)
		{
			if (hasTimedOut())
				return false;
			Sleep(5);
		}
		return true;
	}

	WorkManagerImpl::Worker* WorkManagerImpl::PopWorkerNoLock()
	{
		Worker* worker = m_firstAvailableWorker;
		if (!worker)
			return nullptr;
		m_firstAvailableWorker = worker->m_nextWorker;
		if (m_firstAvailableWorker)
			m_firstAvailableWorker->m_prevWorker = nullptr;
		++m_activeWorkerCount;
		return worker;
	}
}
