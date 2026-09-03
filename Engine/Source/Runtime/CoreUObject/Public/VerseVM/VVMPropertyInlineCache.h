// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VVMCell.h"
#include "VVMGlobalTrivialEmergentTypePtr.h"

namespace Verse
{

struct VFunction;
struct VNativeFunction;

struct FCacheCase
{
	FCacheCase() = default;
	FCacheCase(const FCacheCase& Other)
		: Kind(Other.Kind)
		, EmergentTypeOffset(Other.EmergentTypeOffset)
	{
		U.Offset = Other.U.Offset;
	}

	FCacheCase& operator=(const FCacheCase& Other)
	{
		Kind = Other.Kind;
		EmergentTypeOffset = Other.EmergentTypeOffset;
		U.Offset = Other.U.Offset;
		return *this;
	}

	enum class EKind : uint8
	{
		Offset,
		ConstantValue,
		ConstantFunction,
		ConstantNativeFunction,
		Invalid
	};

	static FCacheCase Offset(VEmergentType* EmergentType, uint64 Offset)
	{
		FCacheCase Result;
		Result.Kind = EKind::Offset;
		Result.EmergentTypeOffset = FHeap::EmergentTypePtrToOffset(EmergentType);
		Result.U.Offset = Offset;
		return Result;
	}

	static FCacheCase Constant(VEmergentType* EmergentType, VValue Value)
	{
		FCacheCase Result;
		Result.Kind = EKind::ConstantValue;
		Result.EmergentTypeOffset = FHeap::EmergentTypePtrToOffset(EmergentType);
		Result.U.Value = Value;
		return Result;
	}

	static FCacheCase Function(VEmergentType* EmergentType, VFunction* Function)
	{
		FCacheCase Result;
		Result.Kind = EKind::ConstantFunction;
		Result.EmergentTypeOffset = FHeap::EmergentTypePtrToOffset(EmergentType);
		Result.U.Function = Function;
		V_DIE_UNLESS(Function);
		return Result;
	}

	static FCacheCase NativeFunction(VEmergentType* EmergentType, VNativeFunction* NativeFunction)
	{
		FCacheCase Result;
		Result.Kind = EKind::ConstantNativeFunction;
		Result.EmergentTypeOffset = FHeap::EmergentTypePtrToOffset(EmergentType);
		Result.U.NativeFunction = NativeFunction;
		V_DIE_UNLESS(NativeFunction);
		return Result;
	}

	explicit operator bool() { return Kind != EKind::Invalid; }

	EKind Kind = EKind::Invalid;
	uint32 EmergentTypeOffset = 0;
	union
	{
		uint64 Offset = 0;
		VValue Value;
		VFunction* Function;
		VNativeFunction* NativeFunction;
	} U;
};

} // namespace Verse
#endif // WITH_VERSE_VM
