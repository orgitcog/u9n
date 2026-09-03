// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ArrayView.h"
#include "CoreMinimal.h"
#include "HAL/Platform.h"
#include "Internationalization/Text.h"
#include "Math/Range.h"

/** Enumeration of pre-defined Unicode block ranges that can be used to access entries from FUnicodeBlockRange */
enum class EUnicodeBlockRange : uint16
{
#define REGISTER_UNICODE_BLOCK_RANGE(LOWERBOUND, UPPERBOUND, SYMBOLNAME, LOCTEXT_NAMESPACE, LOCTEXT_KEY, LOCTEXT_LITERAL) SYMBOLNAME,
#include "UnicodeBlockRange.inl"
#undef REGISTER_UNICODE_BLOCK_RANGE
};

/** Pre-defined Unicode block ranges that can be used with the character ranges in sub-fonts */
struct FUnicodeBlockRange
{
	/** Index enum of this block */
	EUnicodeBlockRange Index;

	/** Display name of this block */
	FText DisplayName;

	/** Range of this block */
	FInt32Range Range;

	/** Returns an array containing all of the pre-defined block ranges */
	static SLATECORE_API TArrayView<const FUnicodeBlockRange> GetUnicodeBlockRanges();

	/** Returns the block corresponding to the given enum */
	static SLATECORE_API FUnicodeBlockRange GetUnicodeBlockRange(const EUnicodeBlockRange InBlockIndex);

	/** Ctor. Forced to not be inlined to improve compile times */
	FORCENOINLINE FUnicodeBlockRange(EUnicodeBlockRange Index, const TCHAR* Namespace, const TCHAR* Key, const TCHAR* Literal, int32 Lower, int32 Upper);
};
