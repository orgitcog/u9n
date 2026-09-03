// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/StringFwd.h"
#include "VerseVM/VVMValue.h"

class FUtf8String;

namespace Verse
{
struct FAllocationContext;
struct VCell;
struct VValue;

enum class EValueStringFormat
{
	Verse,              // Verse syntax
	Diagnostic,         // For Verse's ToDiagnostic function
	Cells,              // Low-level VM cell information
	CellsWithAddresses, // Low-level VM cell information with addresses (non-deterministic).
	JSON                // JSON syntax
};

constexpr bool IsCellFormat(EValueStringFormat Format)
{
	return Format == EValueStringFormat::Cells || Format == EValueStringFormat::CellsWithAddresses;
}

COREUOBJECT_API void AppendToString(VCell* Cell, FUtf8StringBuilderBase& Builder, FAllocationContext Context, EValueStringFormat Format, uint32 RecursionDepth = 0);
COREUOBJECT_API void AppendToString(VValue Value, FUtf8StringBuilderBase& Builder, FAllocationContext Context, EValueStringFormat Format, uint32 RecursionDepth = 0);

COREUOBJECT_API FUtf8String ToString(VCell* Cell, FAllocationContext Context, EValueStringFormat Format, uint32 RecursionDepth = 0);
COREUOBJECT_API FUtf8String ToString(VValue Value, FAllocationContext Context, EValueStringFormat Format, uint32 RecursionDepth = 0);

} // namespace Verse
