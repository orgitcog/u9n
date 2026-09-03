// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VerseVM/VVMCell.h"
#include "VerseVM/VVMGlobalTrivialEmergentTypePtr.h"
#include "VerseVM/VVMWriteBarrier.h"

namespace Verse
{
struct VTask;
struct FAccessContext;
struct VEmergentType;

struct VTaskNativeHook : VCell
{
	DECLARE_DERIVED_VCPPCLASSINFO(COREUOBJECT_API, VCell);
	COREUOBJECT_API static TGlobalTrivialEmergentTypePtr<&StaticCppClassInfo> GlobalTrivialEmergentType;

	// If desired, this can point to the hook added after this one
	TWriteBarrier<VTaskNativeHook> Next;

	// The lambda can be mutable, therefore invocation is not const
	void Invoke(FAccessContext Context, VTask* Task)
	{
		Invoker(Context, this, Task);
	}

	// WARNING 1: The destructor of the lambda passed in must be thread safe since it can be invoked on the GC thread
	// WARNING 2: Any VCells or UObjects captured by the lambda will not be visited during GC
	template <
		typename FunctorType,
		typename FunctorTypeDecayed = std::decay_t<FunctorType>
			UE_REQUIRES(
				!TIsTFunction<FunctorTypeDecayed>::Value && std::is_invocable_r_v<void, FunctorTypeDecayed, FAccessContext, VTask*>)>
	static VTaskNativeHook& New(FAllocationContext Context, FunctorType&& Func);

protected:
	using InvokerType = void (*)(FAccessContext, VTaskNativeHook*, VTask*);
	using DestructorType = void (*)(VTaskNativeHook*);

	VTaskNativeHook(FAllocationContext Context, InvokerType InInvoker, DestructorType InDestructor)
		: VCell(Context, &GlobalTrivialEmergentType.Get(Context))
		, Invoker(InInvoker)
		, Destructor(InDestructor)
	{
	}

	~VTaskNativeHook()
	{
		Destructor(this);
	}

private:
	InvokerType Invoker;
	DestructorType Destructor;
};

template <typename FunctorTypeDecayed>
struct TTaskNativeHook : VTaskNativeHook
{
	TTaskNativeHook(FAllocationContext Context, FunctorTypeDecayed&& InFunc)
		: VTaskNativeHook(
			Context,
			[](FAccessContext Context, VTaskNativeHook* This, VTask* Task) {
				static_cast<TTaskNativeHook*>(This)->Func(Context, Task);
			},
			[](VTaskNativeHook* This) {
				// Destroy only what's not already contained in the base class
				static_cast<TTaskNativeHook*>(This)->Func.~FunctorTypeDecayed();
			})
		, Func(Forward<FunctorTypeDecayed>(InFunc))
	{
	}

	FunctorTypeDecayed Func;
};

template <
	typename FunctorType,
	typename FunctorTypeDecayed
		UE_REQUIRES(
			!TIsTFunction<FunctorTypeDecayed>::Value && std::is_invocable_r_v<void, FunctorTypeDecayed, FAccessContext, VTask*>)>
VTaskNativeHook& VTaskNativeHook::New(FAllocationContext Context, FunctorType&& Func)
{
	std::byte* Allocation = std::is_trivially_destructible_v<FunctorTypeDecayed>
							  ? Context.AllocateFastCell(sizeof(TTaskNativeHook<FunctorTypeDecayed>))
							  : Context.Allocate(FHeap::DestructorSpace, sizeof(TTaskNativeHook<FunctorTypeDecayed>));
	return *new (Allocation) TTaskNativeHook<FunctorTypeDecayed>(Context, Forward<FunctorTypeDecayed>(Func));
}

} // namespace Verse

#endif