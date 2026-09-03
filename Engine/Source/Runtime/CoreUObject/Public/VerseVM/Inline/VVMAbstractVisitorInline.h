// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "Templates/TypeCompatibleBytes.h"
#include "VerseVM/VVMAbstractVisitor.h"
#include "VerseVM/VVMCppClassInfo.h"

class UObject;

namespace Verse
{

inline FAbstractVisitor::FReferrerContext::FReferrerContext(FAbstractVisitor& InVisitor, FReferenceToken InReferrer)
	: Visitor(InVisitor)
	, Referrer(InReferrer)
{
	Previous = Visitor.Context;
	Visitor.Context = this;
}

inline FAbstractVisitor::FReferrerContext::~FReferrerContext()
{
	Visitor.Context = Previous;
}

template <typename T>
inline void FAbstractVisitor::Visit(T& Value, const TCHAR* ElementName)
{
	using Verse::Visit;
	Visit(*this, Value);
}

// Simple arrays
template <typename T>
inline void FAbstractVisitor::Visit(T Begin, T End, const TCHAR* ElementName)
{
	for (; Begin != End; ++Begin)
	{
		auto&& Element = *Begin;
		Visit(Element, TEXT(""));
	}
}

// Arrays
template <typename ElementType, typename AllocatorType>
inline void FAbstractVisitor::Visit(TArray<ElementType, AllocatorType>& Values, const TCHAR* ElementName)
{
	Visit(Values.begin(), Values.end(), ElementName);
}

// Sets
template <typename ElementType, typename KeyFuncs, typename Allocator>
inline void FAbstractVisitor::Visit(TSet<ElementType, KeyFuncs, Allocator>& Values, const TCHAR* ElementName)
{
	for (ElementType& Value : Values)
	{
		Visit(Value, TEXT(""));
	}
}

// Maps
template <typename KeyType, typename ValueType, typename SetAllocator, typename KeyFuncs>
inline void FAbstractVisitor::Visit(TMap<KeyType, ValueType, SetAllocator, KeyFuncs>& Values, const TCHAR* ElementName)
{
	for (TPair<KeyType, ValueType>& Pair : Values)
	{
		Visit(Pair.Key, TEXT("Key"));
		Visit(Pair.Value, TEXT("Value"));
	}
}

} // namespace Verse
#endif // WITH_VERSE_VM
