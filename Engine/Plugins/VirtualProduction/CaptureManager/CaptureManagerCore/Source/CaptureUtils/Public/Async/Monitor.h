// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/ScopeLock.h"

namespace UE::CaptureManager
{

template<typename T>
class FMonitor
{
public:
	template<typename ...Args>
	FMonitor(Args&&... InArgs)
		: Object(Forward<Args>(InArgs)...)
	{
	}

	FMonitor(T InObject)
		: Object(MoveTemp(InObject))
	{
	}

	class FHelper
	{
	public:
		FHelper(FMonitor* InOwner)
			: Owner(InOwner)
			, ScopeLock(&InOwner->Mutex)
		{
		}

		T* operator->()
		{
			return &Owner->Object;
		}

		T& operator*()
		{
			return Owner->Object;
		}

	private:

		FMonitor* Owner;
		FScopeLock ScopeLock;
	};

	FHelper operator->()
	{
		return FHelper(this);
	}

	FHelper Lock()
	{
		return FHelper(this);
	}

	T& GetUnsafe()
	{
		return Object;
	}

	T Claim()
	{
		return MoveTemp(Object);
	}

private:

	FCriticalSection Mutex;
	T Object;
};

}