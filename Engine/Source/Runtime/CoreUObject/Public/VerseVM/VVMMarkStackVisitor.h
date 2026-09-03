// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "UObject/GarbageCollection.h"
#include "VVMMarkStack.h"
#include "VVMRestValue.h"

namespace Verse
{
struct VNativeStruct;

struct FMarkStackVisitor
{
	UE_NONCOPYABLE(FMarkStackVisitor);

	static constexpr bool bIsAbstractVisitor = false;

	FMarkStackVisitor(FMarkStack& InMarkStack)
		: MarkStack(InMarkStack)
	{
	}

	void VisitNonNull(VCell* InCell, const TCHAR*)
	{
		MarkStack.MarkNonNull(InCell);
	}

	void VisitNonNull(UObject* InObject, const TCHAR*)
	{
		MarkStack.MarkNonNull(InObject);
	}

	void VisitAuxNonNull(void* InAux, const TCHAR*)
	{
		MarkStack.MarkAuxNonNull(InAux);
	}

	void Visit(VCell* InCell, const TCHAR* ElementName)
	{
		if (InCell != nullptr)
		{
			VisitNonNull(InCell, ElementName);
		}
	}

	void Visit(UObject* InObject, const TCHAR* ElementName)
	{
		if (InObject != nullptr)
		{
			VisitNonNull(InObject, ElementName);
		}
	}

	void VisitAux(void* Aux, const TCHAR*)
	{
		if (Aux != nullptr)
		{
			VisitAuxNonNull(Aux, TEXT(""));
		}
	}

	void Visit(VValue Value, const TCHAR*)
	{
		if (VCell* Cell = Value.ExtractCell())
		{
			Visit(Cell, TEXT(""));
		}
		else if (UObject* Object = Value.ExtractUObject())
		{
			Visit(Object, TEXT(""));
		}
	}

	// NOTE: The Value parameter can not be passed by value.

	template <typename T>
	void Visit(TWriteBarrier<T>& Value, const TCHAR*)
	{
		Visit(Value.Get(), TEXT(""));
	}

	template <typename T>
	void Visit(T& Value, const TCHAR*);

	template <typename T>
	void Visit(T* Values, uint64 Count, const TCHAR*)
	{
		Visit(Values, Values + Count, TEXT(""));
	}

	template <typename T>
	void Visit(T Begin, T End, const TCHAR*);

	// Arrays
	template <typename ElementType, typename AllocatorType>
	void Visit(TArray<ElementType, AllocatorType>& Values, const TCHAR*);

	// Sets
	template <typename ElementType, typename KeyFuncs, typename Allocator>
	void Visit(TSet<ElementType, KeyFuncs, Allocator>& Values, const TCHAR*);

	// Maps
	template <typename KeyType, typename ValueType, typename SetAllocator, typename KeyFuncs>
	void Visit(TMap<KeyType, ValueType, SetAllocator, KeyFuncs>& Values, const TCHAR*);

	bool IsMarked(VCell* InCell, const TCHAR*)
	{
		return FHeap::IsMarked(InCell);
	}

	void ReportNativeBytes(size_t Bytes)
	{
		MarkStack.ReportNativeBytes(Bytes);
	}

	void MarkNativeStructAsReachable(VNativeStruct* NativeStruct)
	{
		UE::GC::MarkNativeStructAsReachable(NativeStruct);
	}

	// Mimic FStructuredArchiveVisitor so Visit overloads can call these unconditionally.

	void Visit(uint8 Value, const TCHAR* ElementName) {}
	void Visit(uint32 Value, const TCHAR* ElementName) {}

private:
	FMarkStack& MarkStack;
};

} // namespace Verse
#endif // WITH_VERSE_VM
