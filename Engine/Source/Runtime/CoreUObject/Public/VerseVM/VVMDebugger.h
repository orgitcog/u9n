// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VerseVM/VVMContext.h"
#include "VerseVM/VVMLocation.h"
#include "VerseVM/VVMUniqueString.h"
#include "VerseVM/VVMValue.h"

namespace Verse
{
struct FLocation;
struct FOp;
struct VFrame;
struct VUniqueString;

struct FDebugger
{
	virtual ~FDebugger() = default;
	virtual void Notify(FRunningContext, const FOp&, VFrame&, VTask&) = 0;
	virtual void AddLocation(FAllocationContext, VUniqueString& FilePath, const FLocation&) = 0;
	virtual void AddTask(FAccessContext, VTask&) = 0;
};

COREUOBJECT_API FDebugger* GetDebugger();

COREUOBJECT_API void SetDebugger(FDebugger*);

namespace Debugger
{
using FRegisters = TArray<TTuple<TWriteBarrier<VUniqueString>, VValue>>;

struct FFrame
{
	explicit FFrame(FAccessContext Context, VUniqueString& Name)
		: Name{Context, &Name}
	{
	}

	explicit FFrame(FAccessContext Context, VUniqueString& Name, VUniqueString& FilePath, FRegisters Registers)
		: Name{Context, &Name}
		, FilePath{Context, &FilePath}
		, Registers{::MoveTemp(Registers)}
	{
	}

	template <typename TVisitor>
	friend void Visit(TVisitor& Visitor, FFrame& Value)
	{
		Visitor.Visit(Value.Name, TEXT("Name"));
		Visitor.Visit(Value.FilePath, TEXT("FilePath"));
		Visitor.Visit(Value.Registers, TEXT("Registers"));
	}

	TWriteBarrier<VUniqueString> Name;
	TWriteBarrier<VUniqueString> FilePath;
	FRegisters Registers;
};

COREUOBJECT_API void ForEachStackFrame(
	FRunningContext,
	const FOp&,
	VFrame&,
	VTask&,
	const FNativeFrame*,
	TFunctionRef<void(const FLocation*, FFrame)>);
} // namespace Debugger
} // namespace Verse

#endif
