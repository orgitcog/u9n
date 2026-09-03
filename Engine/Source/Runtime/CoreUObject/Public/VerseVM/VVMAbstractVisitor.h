// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "UObject/ReferenceToken.h"
#include "VVMCell.h"
#include "VVMValue.h"
#include "VVMWriteBarrier.h"

class UObject;

namespace Verse
{

struct VNativeStruct;

struct FAbstractVisitor
{
	UE_NONCOPYABLE(FAbstractVisitor);

	static constexpr bool bIsAbstractVisitor = true;

	// A stack based context to maintain the chain of referrers
	struct FReferrerContext
	{
		FReferrerContext(FAbstractVisitor& InVisitor, FReferenceToken InReferrer);
		~FReferrerContext();

		FReferenceToken GetReferrer() const { return Referrer; }
		FReferrerContext* GetPrevious() const { return Previous; }

	private:
		FAbstractVisitor& Visitor;
		FReferenceToken Referrer;
		FReferrerContext* Previous;
	};

	virtual ~FAbstractVisitor() = default;

	// The context provides information about the current cell being visited
	FReferrerContext* GetContext() const
	{
		return Context;
	}

	// Canonical visit methods. Visitors should override these for the types they care about.

	COREUOBJECT_API virtual void VisitNonNull(VCell* InCell, const TCHAR* ElementName);
	COREUOBJECT_API virtual void VisitNonNull(UObject* InObject, const TCHAR* ElementName);
	COREUOBJECT_API virtual void VisitAuxNonNull(void* InAux, const TCHAR* ElementName);

	// Convenience methods. These forward to the canonical methods above.

	COREUOBJECT_API void Visit(VCell* InCell, const TCHAR* ElementName);
	COREUOBJECT_API void Visit(UObject* InObject, const TCHAR* ElementName);
	COREUOBJECT_API void VisitAux(void* InAux, const TCHAR* ElementName);

	COREUOBJECT_API void Visit(VValue Value, const TCHAR* ElementName);

	template <typename T>
	void Visit(TWriteBarrier<T>& Value, const TCHAR* ElementName)
	{
		Visit(Value.Get(), ElementName);
	}

	template <typename T>
	void Visit(T& Value, const TCHAR* ElementName);

	// Simple arrays
	template <typename T>
	void Visit(T Begin, T End, const TCHAR* ElementName);

	template <typename T>
	void Visit(T* Values, uint64 Count, const TCHAR* ElementName)
	{
		Visit(Values, Values + Count, ElementName);
	}

	// Arrays
	template <typename ElementType, typename AllocatorType>
	void Visit(TArray<ElementType, AllocatorType>& Values, const TCHAR* ElementName);

	// Sets
	template <typename ElementType, typename KeyFuncs, typename Allocator>
	void Visit(TSet<ElementType, KeyFuncs, Allocator>& Values, const TCHAR* ElementName);

	// Maps
	template <typename KeyType, typename ValueType, typename SetAllocator, typename KeyFuncs>
	void Visit(TMap<KeyType, ValueType, SetAllocator, KeyFuncs>& Values, const TCHAR* ElementName);

	// Mimic FMarkStackVisitor so VisitReferencesImpl can call these unconditionally.

	bool IsMarked(VCell* InCell, const TCHAR* ElementName) { return true; }
	void ReportNativeBytes(size_t Bytes) {}
	void MarkNativeStructAsReachable(VNativeStruct* NativeStruct) {}

	// Mimic FStructuredArchiveVisitor so Visit overloads can call these unconditionally.

	void Visit(uint8 Value, const TCHAR* ElementName) {}
	void Visit(uint32 Value, const TCHAR* ElementName) {}

protected:
	FAbstractVisitor() = default;

private:
	FReferrerContext* Context{nullptr};
};

} // namespace Verse
#endif // WITH_VERSE_VM
