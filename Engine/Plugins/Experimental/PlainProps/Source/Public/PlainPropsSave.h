// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PlainPropsBind.h"

namespace PlainProps 
{

struct FSaveContext : FBindContext
{
	FScratchAllocator&			Scratch;
	IDefaultStructs*			Defaults = nullptr;
};

template<typename Runtime>
FSaveContext MakeSaveContext(FScratchAllocator& Scratch)
{
	return { Runtime::GetTypes(), Runtime::GetSchemas(), Runtime::GetCustoms(), Scratch, Runtime::GetDefaults() };
}

////////////////////////////////////////////////////////////////////////////////////////////////

[[nodiscard]] PLAINPROPS_API FBuiltStruct*	SaveStruct(const void* Struct, FBindId BindId, const FSaveContext& Context);
[[nodiscard]] PLAINPROPS_API FBuiltStruct*	SaveStructDelta(const void* Struct, const void* Default, FBindId BindId, const FSaveContext& Context);
[[nodiscard]] PLAINPROPS_API FBuiltStruct*	SaveStructDeltaIfDiff(const void* Struct, const void* Default, FBindId BindId, const FSaveContext& Context);
[[nodiscard]] PLAINPROPS_API FBuiltRange*	SaveRange(const void* Range, FRangeMemberBinding Member, const FSaveContext& Ctx);
[[nodiscard]] PLAINPROPS_API FBuiltRange*	SaveLeafRange(const void* Range, const ILeafRangeBinding& Binding, FUnpackedLeafType Leaf, const FSaveContext& Ctx);

} // namespace PlainProps