// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VVMGlobalTrivialEmergentTypePtr.h"
#include "VVMNativeFunction.h"
#include "VVMType.h"

namespace Verse
{

struct VProgram;
struct VTask;

// A special heap value to store all intrinsic VNativeFunction objects
struct VIntrinsics : VCell
{
	DECLARE_DERIVED_VCPPCLASSINFO(COREUOBJECT_API, VCell);
	COREUOBJECT_API static TGlobalTrivialEmergentTypePtr<&StaticCppClassInfo> GlobalTrivialEmergentType;

	TWriteBarrier<VNativeFunction> Abs;
	TWriteBarrier<VNativeFunction> Ceil;
	TWriteBarrier<VNativeFunction> Floor;
	TWriteBarrier<VNativeFunction> ConcatenateMaps;

	static VIntrinsics& New(FAllocationContext Context, VPackage& BuiltInPackage)
	{
		return *new (Context.AllocateFastCell(sizeof(VIntrinsics))) VIntrinsics(Context, BuiltInPackage);
	}

	static void Initialize(FAllocationContext Context);

private:
	COREUOBJECT_API static FNativeCallResult AbsImpl(FRunningContext Context, VValue Scope, VNativeFunction::Args Arguments);
	COREUOBJECT_API static FNativeCallResult CeilImpl(FRunningContext Context, VValue Scope, VNativeFunction::Args Arguments);
	COREUOBJECT_API static FNativeCallResult FloorImpl(FRunningContext Context, VValue Scope, VNativeFunction::Args Arguments);
	COREUOBJECT_API static FNativeCallResult ConcatenateMapsImpl(FRunningContext Context, VValue Scope, VNativeFunction::Args Arguments);

	VIntrinsics(FAllocationContext Context, VPackage& BuiltInPackage);
};

} // namespace Verse
#endif // WITH_VERSE_VM
