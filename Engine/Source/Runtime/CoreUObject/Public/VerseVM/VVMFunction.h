// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VVMClass.h"
#include "VVMFalse.h"
#include "VVMGlobalTrivialEmergentTypePtr.h"
#include "VVMScope.h"

namespace Verse
{
enum class EValueStringFormat;
struct VProcedure;
struct VUniqueString;

struct VFunction : VHeapValue
{
	DECLARE_DERIVED_VCPPCLASSINFO(COREUOBJECT_API, VHeapValue);
	COREUOBJECT_API static TGlobalTrivialEmergentTypePtr<&StaticCppClassInfo> GlobalTrivialEmergentType;

	using Args = TArray<VValue, TInlineAllocator<8>>;

	TWriteBarrier<VProcedure> Procedure;

	/// If specified, the object instance that this function belongs to. Can either be a `VObject` or a `UObject`.
	/// When not bound, this should be an uninitialized `VValue` for methods and `VFalse` for functions. This is
	/// so we can differentiate between when we should bind `Self` lazily at runtime for calls to methods.
	TWriteBarrier<VValue> Self;

	/// The lexical scope that this function is allocated with. This includes all lexical captures, including `(super:)`.
	TWriteBarrier<VScope> ParentScope;

	FORCEINLINE FOpResult Invoke(FRunningContext Context, VValue Argument, TWriteBarrier<VUniqueString>* NamedArg = nullptr)
	{
		return InvokeWithSelf(Context, Self.Get(), Argument, NamedArg);
	}
	FORCEINLINE FOpResult Invoke(FRunningContext Context, Args&& Arguments, TArray<TWriteBarrier<VUniqueString>>* NamedArgs = nullptr, Args* NamedArgVals = nullptr)
	{
		return InvokeWithSelf(Context, Self.Get(), MoveTemp(Arguments), NamedArgs, NamedArgVals);
	}
	COREUOBJECT_API FOpResult InvokeWithSelf(FRunningContext Context, VValue Self, VValue Argument, TWriteBarrier<VUniqueString>* NamedArg = nullptr);
	COREUOBJECT_API FOpResult InvokeWithSelf(FRunningContext Context, VValue Self, Args&& Arguments, TArray<TWriteBarrier<VUniqueString>>* NamedArgs = nullptr, Args* NamedArgVals = nullptr);
	COREUOBJECT_API static FOpResult Spawn(FRunningContext Context, VValue Callee, Args&& Arguments, TArray<TWriteBarrier<VUniqueString>>* NamedArgs = nullptr, Args* NamedArgVals = nullptr);

	static VFunction& New(FAllocationContext Context, VProcedure& Procedure, VValue Self)
	{
		return *new (Context.AllocateFastCell(sizeof(VFunction))) VFunction(Context, &Procedure, Self, nullptr);
	}

	/// Use this when you're constructing a function where you passing captures to it for its lexical scope - i.e. a
	/// method that binds `Self` lazily at runtime, but captures `(super:)` in its current scope.
	static VFunction& NewUnbound(FAllocationContext Context, VProcedure& Procedure, VScope& InScope)
	{
		return *new (Context.AllocateFastCell(sizeof(VFunction))) VFunction(Context, &Procedure, VValue(), &InScope);
	}

	VFunction& Bind(FAllocationContext Context, VValue InSelf)
	{
		checkf(!HasSelf(), TEXT("Attempting to bind `Self` to a `VFunction` that already has it set; this is probably a mistake in the code generation."));
		checkf(ParentScope, TEXT("The function should already have had its scope set; this is probably a mistake in the code generation."));
		return *new (Context.AllocateFastCell(sizeof(VFunction))) VFunction(Context, Procedure.Get(), InSelf, ParentScope.Get());
	}

	VProcedure& GetProcedure() { return *Procedure.Get(); }

	/// Checks if the function is already bound.
	COREUOBJECT_API bool HasSelf() const;

	void AppendToStringImpl(FUtf8StringBuilderBase& Builder, FAllocationContext Context, EValueStringFormat Format, uint32 RecursionDepth);

	static void SerializeLayout(FAllocationContext Context, VFunction*& This, FStructuredArchiveVisitor& Visitor);
	void SerializeImpl(FAllocationContext Context, FStructuredArchiveVisitor& Visitor);

private:
	VFunction(FAllocationContext Context, VProcedure* InFunction, VValue InSelf, VScope* InParentScope)
		: VHeapValue(Context, &GlobalTrivialEmergentType.Get(Context))
		, Procedure(Context, InFunction)
		, Self(Context, InSelf)
		, ParentScope(Context, InParentScope)
	{
	}
};
} // namespace Verse
#endif // WITH_VERSE_VM
