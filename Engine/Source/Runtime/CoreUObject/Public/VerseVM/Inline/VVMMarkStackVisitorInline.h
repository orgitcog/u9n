// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VerseVM/VVMCppClassInfo.h"
#include "VerseVM/VVMMarkStackVisitor.h"

namespace Verse
{

template <typename T>
inline void FMarkStackVisitor::Visit(T& Value, const TCHAR*)
{
	using Verse::Visit;
	Visit(*this, Value);
}

template <typename T>
inline void FMarkStackVisitor::Visit(T Begin, T End, const TCHAR*)
{
	for (; Begin != End; ++Begin)
	{
		auto&& Element = *Begin;
		Visit(Element, TEXT(""));
	}
}

// Arrays
template <typename ElementType, typename AllocatorType>
inline void FMarkStackVisitor::Visit(TArray<ElementType, AllocatorType>& Values, const TCHAR*)
{
	Visit(Values.begin(), Values.end(), TEXT(""));
}

// Sets
template <typename ElementType, typename KeyFuncs, typename Allocator>
inline void FMarkStackVisitor::Visit(TSet<ElementType, KeyFuncs, Allocator>& Values, const TCHAR*)
{
	for (ElementType& Value : Values)
	{
		Visit(Value, TEXT(""));
	}
}

// Maps
template <typename KeyType, typename ValueType, typename SetAllocator, typename KeyFuncs>
inline void FMarkStackVisitor::Visit(TMap<KeyType, ValueType, SetAllocator, KeyFuncs>& Values, const TCHAR*)
{
	for (TPair<KeyType, ValueType>& Pair : Values)
	{
		Visit(Pair.Key, TEXT(""));
		Visit(Pair.Value, TEXT(""));
	}
}

} // namespace Verse
#endif // WITH_VERSE_VM
