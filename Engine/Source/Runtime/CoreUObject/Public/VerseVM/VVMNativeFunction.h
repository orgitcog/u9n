// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "Containers/ArrayView.h"
#include "Containers/StringFwd.h"
#include "VVMFalse.h"
#include "VVMGlobalTrivialEmergentTypePtr.h"
#include "VVMScope.h"
#include "VVMType.h"
#include "VerseVM/VVMNames.h"

namespace Verse
{
struct FOpResult;
struct VPackage;
struct VTask;
struct VUniqueString;

using FNativeCallResult = FOpResult;

// A function that is implemented in C++
struct VNativeFunction : VHeapValue
{
	DECLARE_DERIVED_VCPPCLASSINFO(COREUOBJECT_API, VHeapValue);
	COREUOBJECT_API static TGlobalTrivialEmergentTypePtr<&StaticCppClassInfo> GlobalTrivialEmergentType;

	static constexpr char DecoratorString[] = "Native";

	// Interface between VerseVM and C++
	using Args = TArrayView<VValue>;
	using FThunkFn = FNativeCallResult (*)(FRunningContext, VValue Scope, Args Arguments);

	uint32 NumPositionalParameters;

	// The C++ function to call
	FThunkFn Thunk;

	TWriteBarrier<VUniqueString> Name;
	TWriteBarrier<VValue> Self;

	static VNativeFunction& New(FAllocationContext Context, uint32 NumPositionalParameters, FThunkFn Thunk, VUniqueString& InName, VValue InSelf)
	{
		return *new (Context.AllocateFastCell(sizeof(VNativeFunction))) VNativeFunction(Context, NumPositionalParameters, Thunk, &InName, InSelf);
	}

	VNativeFunction& Bind(FAllocationContext Context, VValue InSelf)
	{
		checkf(!HasSelf(), TEXT("Attempting to bind `Self` to a `VNativeFunction` that already has it set; this is probably a mistake in the code generation."));
		return VNativeFunction::New(Context, NumPositionalParameters, Thunk, *Name, InSelf);
	}

	// Lookup a native function and set it's thunk to a C++ function
	static COREUOBJECT_API void SetThunk(Verse::VPackage* Package, FUtf8StringView VerseScopePath, FUtf8StringView DecoratedName, FThunkFn NativeFuncPtr);

	COREUOBJECT_API bool HasSelf() const;

	void AppendToStringImpl(FUtf8StringBuilderBase& Builder, FAllocationContext Context, EValueStringFormat Format, uint32 RecursionDepth);
	static void SerializeLayout(FAllocationContext Context, VNativeFunction*& This, FStructuredArchiveVisitor& Visitor);
	void SerializeImpl(FAllocationContext Context, FStructuredArchiveVisitor& Visitor);

private:
	VNativeFunction(FAllocationContext Context, uint32 InNumPositionalParameters, FThunkFn InThunk, VUniqueString* InName, VValue InSelf)
		: VHeapValue(Context, &GlobalTrivialEmergentType.Get(Context))
		, NumPositionalParameters(InNumPositionalParameters)
		, Thunk(InThunk)
		, Name(Context, InName)
		, Self(Context, InSelf)
	{
	}
};

} // namespace Verse
#endif // WITH_VERSE_VM
