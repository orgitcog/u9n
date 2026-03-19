// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PlainPropsBuild.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"

namespace PlainProps
{

struct FBuiltStruct
{
	~FBuiltStruct() = delete; // Allocated in FScratchAllocator 
	
	uint16				NumMembers;
	FBuiltMember		Members[0];
};

struct FBuiltRange
{
	~FBuiltRange() = delete; // Allocated in FScratchAllocator

	[[nodiscard]] static FBuiltRange*					Create(FScratchAllocator& Allocator, uint64 NumItems, SIZE_T ItemSize);

	uint64												Num;
	uint8												Data[0];
	
	TConstArrayView64<const FBuiltRange*>				AsRanges() const	{ return { reinterpret_cast<FBuiltRange const* const*>(Data),	static_cast<int64>(Num) }; }
	TConstArrayView64<const FBuiltStruct*>				AsStructs() const	{ return { reinterpret_cast<FBuiltStruct const* const*>(Data),	static_cast<int64>(Num) }; }
};


} // namespace PlainProps