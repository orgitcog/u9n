// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaEvent.h"
#include "UbaStringBuffer.h"
#include "UbaTimer.h"

#define UBA_TRACK_WORK 1

namespace uba
{
	struct StringView;
	struct TrackWorkScope;

	using Color = u32;
	constexpr inline Color ToColor(u8 r, u8 g, u8 b) { return (r << 16) + (g << 8) + b; }
	constexpr Color ColorWhite = ToColor(255, 255, 255);
	constexpr Color ColorWork = ToColor(70, 70, 100);

	class WorkTracker
	{
	public:
		virtual u32 TrackWorkStart(const StringView& desc, const Color& color) = 0;
		virtual void TrackWorkHint(u32 id, const StringView& hint, u64 startTime = 0) = 0;
		virtual void TrackWorkEnd(u32 id) = 0;
	};

	struct WorkContext
	{
		TrackWorkScope& tracker;
	};

	class WorkManager : public WorkTracker
	{
	public:
		using WorkFunction = Function<void(const WorkContext& context)>;
		virtual void AddWork(const WorkFunction& work, u32 count, const tchar* desc, const Color& color = ColorWork, bool highPriority = false) = 0;
		virtual u32 GetWorkerCount() = 0;
		virtual void DoWork(u32 count = 1) = 0;
		virtual bool FlushWork(u32 timeoutMs = 0) = 0;

		u32 TrackWorkStart(const StringView& desc, const Color& color);
		void TrackWorkHint(u32 id, const StringView& hint, u64 startTime = 0);
		void TrackWorkEnd(u32 id);

		void SetWorkTracker(WorkTracker* workTracker) { m_workTracker = workTracker; }
		WorkTracker* GetWorkTracker() { return m_workTracker; }

		template<u32 BatchSize = 1, typename TContainer, typename TFunc>
		void ParallelFor(u32 workCount, TContainer& container, TFunc&& func, const StringView& description = AsView(TC("")), bool highPriority = false);

	protected:
		Atomic<WorkTracker*> m_workTracker;
	};


	class WorkManagerImpl : public WorkManager
	{
	public:
		WorkManagerImpl(u32 workerCount, const tchar* workerDesc = TC("UbaWrk"));
		virtual ~WorkManagerImpl();
		virtual void AddWork(const WorkFunction& work, u32 count, const tchar* desc, const Color& color = ColorWork, bool highPriority = false) override final;
		virtual u32 GetWorkerCount() override final;
		virtual void DoWork(u32 count = 1) override final;
		virtual bool FlushWork(u32 timeoutMs = 0) override final;

	private:
		struct Worker;
		void PushWorker(Worker* worker);
		void PushWorkerNoLock(Worker* worker);
		Worker* PopWorkerNoLock();

		Vector<Worker*> m_workers;
		struct Work { WorkFunction func; TString desc; };
		Futex m_workLock;
		List<Work> m_work;
		Atomic<u32> m_activeWorkerCount;
		Atomic<u32> m_workCounter;

		Futex m_availableWorkersLock;
		Worker* m_firstAvailableWorker = nullptr;
	};

	struct TrackWorkScope
	{
		TrackWorkScope() : tracker(nullptr), id(0) {}
		TrackWorkScope(WorkTracker& t, const StringView& desc, const Color& color = ColorWork) : tracker(&t), id(t.TrackWorkStart(desc, color)) {}
		void AddHint(const StringView& hint, u64 startTime = 0) { if (tracker) tracker->TrackWorkHint(id, hint, startTime); }
		~TrackWorkScope() { if (tracker) tracker->TrackWorkEnd(id); }
		WorkTracker* tracker;
		u32 id;
	};

	struct TrackHintScope
	{
		TrackHintScope(TrackWorkScope& t, const StringView& h) : tws(t), hint(h), startTime(GetTime()) {}
		~TrackHintScope() { tws.AddHint(hint, startTime); }
		TrackWorkScope& tws;
		const StringView& hint;
		u64 startTime;
	};

	template<u32 BatchSize, typename TContainer, typename TFunc>
	void WorkManager::ParallelFor(u32 workCount, TContainer& container, TFunc&& func, const StringView& description, bool highPriority)
	{
		#if !defined( __clang_analyzer__ ) // Static analyzer claims context can leak but it can only leak when process terminates (and then it doesn't matter)

		typedef typename TContainer::iterator iterator;

		auto size = container.size();

		if (size <= BatchSize)
		{
			#if UBA_TRACK_WORK
			TrackWorkScope tws(*this, description, ColorWork);
			#else
			TrackWorkScope tws;
			#endif
			WorkContext wc{tws};
			for (iterator i=container.begin(), e=container.end(); i!=e; i++)
				func(wc, i);
			return;
		}

		workCount = Min(workCount, u32(size - 1)/BatchSize);

		Event doneEvent(true);

		struct Context
		{
			iterator it;
			iterator end;
			u32 refCount;
			u32 activeCount;
			bool isDone;
			Futex lock;
			Event* doneEvent;
		};

		auto context = new Context();
		context->it = container.begin();
		context->end = container.end();
		context->refCount = workCount + 1;
		context->activeCount = 0;
		context->isDone = false;
		context->doneEvent = &doneEvent;

		auto work = [context, funcCopy = func](const WorkContext& wc) mutable
			{
				iterator itBatch[BatchSize];

				u32 active = 0;
				while (true)
				{
					SCOPED_FUTEX(context->lock, l);
					context->activeCount -= active;
					context->isDone = context->it == context->end;
					if (context->isDone)
					{
						if (context->activeCount == 0 && context->doneEvent)
						{
							context->doneEvent->Set();
							context->doneEvent = nullptr;
						}
						if (--context->refCount)
							return;
						l.Leave();
						delete context;
						return;
					}

					itBatch[0] = context->it++;
					active = 1;

					if constexpr (BatchSize > 1)
						for (;active < BatchSize && context->it != context->end;)
							itBatch[active++] = context->it++;

					context->activeCount += active;
					l.Leave();

					funcCopy(wc, itBatch[0]);

					if constexpr (BatchSize > 1)
						for (u32 i=1; i!=active; ++i)
							funcCopy(wc, itBatch[i]);
				}
			};
		
		AddWork(work, workCount, description.data, ColorWork, highPriority);

		{
			#if UBA_TRACK_WORK
			TrackWorkScope tws(*this, description, ColorWork);
			#else
			TrackWorkScope tws;
			#endif
			work({tws});
		}

		doneEvent.IsSet();

		#endif
	}
}
