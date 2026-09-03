// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaEvent.h"
#include "UbaPlatform.h"
#include "UbaSynchronization.h"
#include "UbaTimer.h"

namespace uba
{
	struct BottleneckTicket
	{
		Event ev;
		BottleneckTicket* next = nullptr;
	};

	struct Bottleneck
	{
		Bottleneck(u32 mc) : m_maxCount(mc) {}

		void Enter(BottleneckTicket& ticket, Timer& timer)
		{
			SCOPED_FUTEX(m_lock, lock);
			if (m_activeCount < m_maxCount)
			{
				UBA_ASSERT(!m_first);
				++m_activeCount;
				return;
			}

			if (!m_first)
				m_first = &ticket;
			else
				m_last->next = &ticket;
			m_last = &ticket;

			if (!ticket.ev.IsCreated())
				ticket.ev.Create(true);

			lock.Leave();

			TimerScope ts(timer);
			ticket.ev.IsSet();
		}

		void Leave(BottleneckTicket& ticket)
		{
			SCOPED_FUTEX(m_lock, lock);
			BottleneckTicket* first = m_first;
			if (!first)
			{
				UBA_ASSERT(m_activeCount > 0);
				--m_activeCount;
				return;
			}
			m_first = first->next;
			if (!m_first)
				m_last = nullptr;
			first->ev.Set();
		}

	private:
		Futex m_lock;
		BottleneckTicket* m_first = nullptr;
		BottleneckTicket* m_last = nullptr;
		u32 m_activeCount = 0;
		u32 m_maxCount;
	};

	struct BottleneckScope
	{
		BottleneckScope(Bottleneck& b, Timer& timer) : bottleneck(b) { b.Enter(ticket, timer); }
		~BottleneckScope() { bottleneck.Leave(ticket); }

		Bottleneck& bottleneck;
		BottleneckTicket ticket;
	};

}
