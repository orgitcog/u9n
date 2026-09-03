// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/Delegate.h"
#include "Async/Async.h"

enum class EDelegateExecutionThread
{
	GameThread = 0,
	InternalThread // Thread on which the function invoking the delegate is being run on
};

namespace details
{
template<typename ... Args>
static void ExecuteDelegate(TDelegate<void(Args...)> InDelegate, EDelegateExecutionThread InThread, Args&&... InArgs)
{
	if (InThread == EDelegateExecutionThread::GameThread)
	{
		TDelegate<void()> Task = TDelegate<void()>::CreateLambda([InDelegate = (MoveTemp(InDelegate))](Args... InArgs)
		{
			InDelegate.Execute(Forward<Args>(InArgs)...);
		}, Forward<Args>(InArgs)...);

		AsyncTask(ENamedThreads::GameThread, [InTask = MoveTemp(Task)]()
		{
			InTask.Execute();
		});
	}
	else
	{
		InDelegate.Execute(Forward<Args>(InArgs)...);
	}
}
}

template<typename ... Args>
class TManagedDelegate
{
public:
	using Type = TDelegate<void(Args...)>;

	TManagedDelegate() = default;

	template<typename Func>
	TManagedDelegate(Func InFunc, EDelegateExecutionThread InThread = EDelegateExecutionThread::GameThread)
		: TManagedDelegate(Type::CreateLambda(InFunc), InThread)
	{
	}

	TManagedDelegate(Type InDelegate, EDelegateExecutionThread InThread = EDelegateExecutionThread::GameThread)
		: Delegate(MoveTemp(InDelegate))
		, ExecutionThread(InThread)
	{
	}

	void operator()(Args... InArgs)
	{
		if (Delegate.IsBound())
		{
			details::ExecuteDelegate(Delegate, ExecutionThread, Forward<Args>(InArgs)...);
		}
	}

	void operator()(Args... InArgs) const
	{
		if (Delegate.IsBound())
		{
			details::ExecuteDelegate(Delegate, ExecutionThread, Forward<Args>(InArgs)...);
		}
	}

private:

	Type Delegate;
	EDelegateExecutionThread ExecutionThread;
};

template<typename ... Args>
class TManagedMulticastDelegate
{
public:
	using DelegateType = TDelegate<void(Args...)>;

	TManagedMulticastDelegate() = default;

	template<typename Func>
	void Add(Func InFunc, EDelegateExecutionThread InThread = EDelegateExecutionThread::GameThread)
	{
		Add(DelegateType::CreateLambda(MoveTemp(InFunc)), InThread);
	}

	void Add(DelegateType InDelegate, EDelegateExecutionThread InThread = EDelegateExecutionThread::GameThread)
	{
		Delegate.AddLambda([InDelegate = MoveTemp(InDelegate), InThread](Args... InArgs) mutable
		{
			details::ExecuteDelegate(InDelegate, InThread, Forward<Args>(InArgs)...);
		});
	}

	void operator()(Args... InArgs)
	{
		Delegate.Broadcast(Forward<Args>(InArgs)...);
	}

	void operator()(Args... InArgs) const
	{
		Delegate.Broadcast(Forward<Args>(InArgs)...);
	}

private:

	using Type = TMulticastDelegate<void(Args...)>;

	Type Delegate;
};