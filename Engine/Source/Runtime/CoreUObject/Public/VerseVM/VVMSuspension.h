// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VVMBytecode.h"
#include "VVMCell.h"
#include "VVMGlobalTrivialEmergentTypePtr.h"
#include "VVMProcedure.h"
#include <array>
#include <type_traits>

namespace Verse
{
struct FOp;
struct VFailureContext;
struct VProcedure;
struct VTask;

struct VSuspension : VCell
{
	DECLARE_DERIVED_VCPPCLASSINFO(COREUOBJECT_API, VCell);

	TWriteBarrier<VFailureContext> FailureContext;
	TWriteBarrier<VTask> Task;
	TWriteBarrier<VSuspension> Next;
#if WITH_EDITORONLY_DATA
	TWriteBarrier<VPackage> CurrentPackage;
#endif

	VSuspension& Tail()
	{
		VSuspension* Current = this;
		while (Current->Next)
		{
			Current = Current->Next.Get();
		}
		return *Current;
	}

protected:
	VSuspension(FAllocationContext Context, VEmergentType* EmergentType, VFailureContext* FailureContext, VTask* Task)
		: VCell(Context, EmergentType)
		, FailureContext(Context, FailureContext)
		, Task(Context, Task)
#if WITH_EDITORONLY_DATA
		, CurrentPackage(Context, Context.GetCurrentPackage())
#endif
	{
	}
};

struct VBytecodeSuspension : public VSuspension
{
	DECLARE_DERIVED_VCPPCLASSINFO(COREUOBJECT_API, VSuspension);
	COREUOBJECT_API static TGlobalTrivialEmergentTypePtr<&StaticCppClassInfo> GlobalTrivialEmergentType;

	template <typename Captures>
	static VBytecodeSuspension& New(FAllocationContext Context, VFailureContext& FailureContext, VTask& Task, VProcedure& Procedure, FOp* PC, const Captures& TheCaptures)
	{
		static_assert(std::is_same_v<Captures, std::decay_t<Captures>>);
		std::byte* Allocation = std::is_trivially_destructible_v<Captures>
								  ? Context.AllocateFastCell(CapturesOffset<Captures>() + sizeof(Captures))
								  : Context.Allocate(FHeap::DestructorSpace, CapturesOffset<Captures>() + sizeof(Captures));
		return *new (Allocation) VBytecodeSuspension(Context, FailureContext, Task, Procedure, PC, TheCaptures);
	}

	TWriteBarrier<VProcedure> Procedure;
	const EOpcode Opcode;
	const uint32 BytecodeOffset;

	template <typename Captures>
	Captures& GetCaptures()
	{
		static_assert(std::is_same_v<Captures, std::decay_t<Captures>>);
		return *BitCast<Captures*>(BitCast<char*>(this) + CapturesOffset<Captures>());
	}

	template <typename TFunc>
	void CaptureSwitch(const TFunc& Func);

private:
	template <typename Captures>
	static size_t CapturesOffset()
	{
		return Align(sizeof(VBytecodeSuspension), alignof(Captures));
	}

	template <typename Captures>
	VBytecodeSuspension(FAllocationContext Context, VFailureContext& FailureContext, VTask& Task, VProcedure& Procedure, FOp* PC, const Captures& TheCaptures)
		: VSuspension(Context, &GlobalTrivialEmergentType.Get(Context), &FailureContext, &Task)
		, Procedure(Context, &Procedure)
		, Opcode(PC->Opcode)
		, BytecodeOffset(Procedure.BytecodeOffset(*PC))
	{
		static_assert(std::is_same_v<Captures, std::decay_t<Captures>>);
		new (&GetCaptures<Captures>()) Captures(Context, TheCaptures);
	}

	~VBytecodeSuspension();
};

// This is used when you want to run a callback when a placeholder resolves its value.
// The Args array is there to capture and mark the VValue arguments that the lambda
// wants to use.
struct VLambdaSuspension : public VSuspension
{
	DECLARE_DERIVED_VCPPCLASSINFO(COREUOBJECT_API, VSuspension);
	COREUOBJECT_API static TGlobalTrivialEmergentTypePtr<&StaticCppClassInfo> GlobalTrivialEmergentType;

	typedef void (*CallbackType)(FRunningContext, VLambdaSuspension& This, VSuspension*& ToFire);

	TWriteBarrier<VValue>* Args()
	{
		return BitCast<TWriteBarrier<VValue>*>(BitCast<char*>(this) + ArgsOffset());
	}

	uint32 NumValues;
	CallbackType Callback;

	template <typename... Args>
	static VLambdaSuspension& New(FAllocationContext Context, VFailureContext& FailureContext, VTask& Task, CallbackType Callback, Args&&... TheArgs)
	{
		return *new (Context.AllocateFastCell(AllocationSize(sizeof...(Args)))) VLambdaSuspension(Context, FailureContext, Task, Callback, std::forward<Args>(TheArgs)...);
	}

private:
	static size_t ArgsOffset()
	{
		return Align(sizeof(VLambdaSuspension), alignof(TWriteBarrier<VValue>));
	}

	static size_t AllocationSize(uint32 NumValues)
	{
		return ArgsOffset() + NumValues * sizeof(TWriteBarrier<VValue>);
	}

	template <typename... ArgsType>
	VLambdaSuspension(FAllocationContext Context, VFailureContext& FailureContext, VTask& Task, CallbackType Callback, ArgsType&&... TheArgs)
		: VSuspension(Context, &GlobalTrivialEmergentType.Get(Context), &FailureContext, &Task)
		, NumValues(sizeof...(ArgsType))
		, Callback(Callback)
	{
		new (Args()) TWriteBarrier<VValue>[] {
			TWriteBarrier<VValue>(Context, std::forward<ArgsType>(TheArgs))...
		};
	}
};

} // namespace Verse
#endif // WITH_VERSE_VM
