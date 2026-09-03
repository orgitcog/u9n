// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaBinaryReaderWriter.h"
#include "UbaEvent.h"

#define UBA_TRACK_THREADS 0

namespace uba
{
	class Event;
	struct GroupAffinity;
	using TraverseThreadFunc = Function<void(u32 tid, void** callstack, u32 callstackCount, const tchar* description)>;
	using TraverseThreadErrorFunc = Function<void(const StringView& error)>;

	class Thread
	{
	public:
		Thread();
		Thread(Function<u32()>&& func, const tchar* description = nullptr);
		~Thread();
		void Start(Function<u32()>&& func, const tchar* description = nullptr);
		bool Wait(u32 milliseconds = ~0u, Event* wakeupEvent = nullptr);
		bool GetGroupAffinity(GroupAffinity& out);
		bool IsInside() const;

	private:
		Function<u32()>	m_func;
		void* m_handle = nullptr;
		ReaderWriterLock m_funcLock;

		#if !PLATFORM_WINDOWS
		Event m_finished;
		#endif

		#if UBA_TRACK_THREADS
		Thread* m_next = nullptr;
		Thread* m_prev = nullptr;
		#endif

		Thread(const Thread&) = delete;
		void operator=(const Thread&) = delete;

		friend bool TraverseAllThreads(const TraverseThreadFunc& func, const TraverseThreadErrorFunc& errorFunc);
	};


	struct GroupAffinity
	{
		u64 mask = 0;
		u16 group = 0;
	};
	bool SetThreadGroupAffinity(void* nativeThreadHandle, const GroupAffinity& affinity);
	bool AlternateThreadGroupAffinity(void* nativeThreadHandle);
	bool TraverseAllThreads(const TraverseThreadFunc& func, const TraverseThreadErrorFunc& errorFunc);
}
