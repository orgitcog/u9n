// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaBase.h"

namespace uba
{
	class Event
	{
	public:
		Event() = default;
		Event(bool manualReset);
		~Event();
		bool Create(bool manualReset);
		void Destroy();
		void Set();
		void Reset();
		bool IsCreated();
		bool IsSet(u32 timeOutMs = ~0u);
		void* GetHandle();
	private:
		struct EventImpl* m_impl = nullptr;
		Event(const Event&) = delete;
		void operator=(const Event&) = delete;
	};

#if !PLATFORM_WINDOWS
	class SharedEvent
	{
	public:
		SharedEvent();
		SharedEvent(bool manualReset);
		~SharedEvent();
		bool Create(bool manualReset);
		void Destroy();
		void Set();
		void Reset();
		bool IsCreated();
		bool IsSet(u32 timeOutMs = ~0u);
		void* GetHandle();
	private:
		#if PLATFORM_MAC || defined(__aarch64__)
		u64 m_data[16];
		#else
		u64 m_data[13];
		#endif
		SharedEvent(const SharedEvent&) = delete;
		void operator=(const SharedEvent&) = delete;
	};
#endif
}
