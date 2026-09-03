// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VerseVM/VVMTransaction.h"
#include "VerseVM/VVMVar.h"
#include "VerseVM/VVMWriteBarrier.h"

namespace Verse
{
template <typename T>
template <typename TResult>
inline auto TWriteBarrier<T>::SetTransactionally(FAllocationContext Context, TValue NewValue) -> std::enable_if_t<bIsVValue || bIsAux, TResult>
{
	RunBarrier(Context, NewValue);
	Context.CurrentTransaction()->LogBeforeWrite(Context, *this);
	Value = NewValue;
}

inline void VRestValue::SetTransactionally(FAllocationContext Context, VValue NewValue)
{
	checkSlow(!NewValue.IsRoot());
	Value.SetTransactionally(Context, NewValue);
}

inline void VVar::Set(FAllocationContext Context, VValue NewValue)
{
	return Value.SetTransactionally(Context, NewValue);
}
} // namespace Verse
#endif // WITH_VERSE_VM
