// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PlainPropsBind.h"

namespace PlainProps 
{

union FDiffMetadata
{
	FOptionalEnumId			Leaf;
	FRangeBinding			Range;
	FBindId					Struct;
};

// Currently lacking range indices
struct FDiffNode
{
	FMemberBindType			Type;
	FOptionalMemberId		Name;
	FDiffMetadata			Meta;
	const void*				A;
	const void*				B;
};

struct FDiffPath : public TArray<FDiffNode, TInlineAllocator<16>> {};

// Tracks diff path for diff tools unlike the const FBindContext& overloads for delta saving
struct FDiffContext : FBindContext
{
	FDiffPath Out;
};

////////////////////////////////////////////////////////////////////////////////
// Tracking and non-tracking methods to diff member leaves/ranges/structs
//
// FDiffContext& overloads track inner FDiffPath, caller must add outermost FDiffNode

[[nodiscard]] PLAINPROPS_API bool DiffStructs(const void* A, const void* B, FBindId Id, const FBindContext& Ctx);
[[nodiscard]] PLAINPROPS_API bool DiffStructs(const void* A, const void* B, FBindId Id, FDiffContext& Ctx);

[[nodiscard]] PLAINPROPS_API bool DiffLeaves(float A, float B);
[[nodiscard]] PLAINPROPS_API bool DiffLeaves(double A, double B);
template<Arithmetic T>		 bool DiffLeaves(T A, T B) { return A != B; }

[[nodiscard]] PLAINPROPS_API bool DiffRanges(const void* A, const void* B, const IItemRangeBinding& Binding, FUnpackedLeafType ItemType);
[[nodiscard]] PLAINPROPS_API bool DiffRanges(const void* A, const void* B, const IItemRangeBinding& Binding, FBindId ItemType, const FBindContext& Ctx);
[[nodiscard]] PLAINPROPS_API bool DiffRanges(const void* A, const void* B, const IItemRangeBinding& Binding, FBindId ItemType, FDiffContext& Ctx);
[[nodiscard]] PLAINPROPS_API bool DiffRanges(const void* A, const void* B, const IItemRangeBinding& Binding, FRangeMemberBinding ItemType, const FBindContext& Ctx);
[[nodiscard]] PLAINPROPS_API bool DiffRanges(const void* A, const void* B, const IItemRangeBinding& Binding, FRangeMemberBinding ItemType, FDiffContext& Ctx);

////////////////////////////////////////////////////////////////////////////////

struct FReadDiffNode
{
	FMemberType				Type;
	FOptionalStructSchemaId	Struct;
	FOptionalMemberId		Name;
	uint64					RangeIdx = ~0u; 
};

struct FReadDiffPath : public TArray<FReadDiffNode, TInlineAllocator<16>> {};

bool PLAINPROPS_API DiffStruct(FStructView A, FStructView B, FReadDiffPath& Out);
bool PLAINPROPS_API DiffSchemas(FSchemaBatchId A, FSchemaBatchId B);

} // namespace PlainProps
