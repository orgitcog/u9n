// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "Templates/TypeCompatibleBytes.h"
#include "VVMBytecodeOps.h"
#include "VVMLocation.h"
#include "VVMMarkStackVisitor.h"

namespace Verse
{
struct VProcedure;

using FOpcodeInt = uint16_t;

enum class EOpcode : FOpcodeInt
{
#define VISIT_OP(Name) Name,
	VERSE_ENUM_OPS(VISIT_OP)
#undef VISIT_OP
};

COREUOBJECT_API const char* ToString(EOpcode Opcode);

inline bool IsBranch(EOpcode Opcode)
{
	switch (Opcode)
	{
		case EOpcode::Jump:
		case EOpcode::JumpIfInitialized:
		case EOpcode::JumpIfArchetype:
		case EOpcode::Switch:
		case EOpcode::EndTask:
			return true;
		default:
			return false;
	}
}

inline bool IsTerminal(EOpcode Opcode)
{
	switch (Opcode)
	{
		case EOpcode::Err:
		case EOpcode::ResumeUnwind:
		case EOpcode::Return:
			return true;
		default:
			return false;
	}
}

inline bool MightFallThrough(EOpcode Opcode)
{
	if (IsTerminal(Opcode))
	{
		return false;
	}

	if (!IsBranch(Opcode))
	{
		return true;
	}

	switch (Opcode)
	{
		case EOpcode::Jump:
		case EOpcode::EndTask:
		case EOpcode::Switch:
			return false;
		case EOpcode::JumpIfInitialized:
		case EOpcode::JumpIfArchetype:
			return true;
		default:
			break;
	}

	VERSE_UNREACHABLE();
	return false;
}

/// This _must_ match up with the codegen in `VerseVMBytecodeGenerator.cs`.
enum class EOperandRole : uint8
{
	Use,
	Immediate,
	ClobberDef,
	UnifyDef,
};

inline bool IsAnyDef(EOperandRole Role)
{
	switch (Role)
	{
		case EOperandRole::Use:
		case EOperandRole::Immediate:
			return false;
		case EOperandRole::ClobberDef:
		case EOperandRole::UnifyDef:
			return true;
	}
}

inline bool IsAnyUse(EOperandRole Role)
{
	switch (Role)
	{
		case EOperandRole::Use:
		case EOperandRole::UnifyDef:
			return true;
		case EOperandRole::Immediate:
		case EOperandRole::ClobberDef:
			return false;
	}
}

struct FRegisterIndex
{
	static constexpr uint32 UNINITIALIZED = INT32_MAX;

	/// These are hardcoded register indices that we will always place the operands in by convention.
	static constexpr uint32 SELF = 0;  // for `Self`.
	static constexpr uint32 SCOPE = 1; // for `(super:)` and other generic captures in the future.
	static constexpr uint32 PARAMETER_START = 2;

	// Unsigned, but must be less than INT32_MAX
	uint32 Index{UNINITIALIZED};

	explicit operator bool() const { return Index < UNINITIALIZED; }
	friend bool operator==(FRegisterIndex Left, FRegisterIndex Right) { return Left.Index == Right.Index; }
	friend bool operator!=(FRegisterIndex Left, FRegisterIndex Right) { return Left.Index != Right.Index; }
	bool operator<(const FRegisterIndex& Other) const { return Index < Other.Index; }
	bool operator>=(const FRegisterIndex& Other) const { return Index >= Other.Index; }
	FRegisterIndex& operator++()
	{
		++Index;
		return *this;
	}

	template <typename TVisitor>
	friend void Visit(TVisitor& Visitor, FRegisterIndex& Value)
	{
		Visitor.Visit(Value.Index, TEXT("Index"));
	}
};

inline uint32 GetTypeHash(FRegisterIndex Register)
{
	return ::GetTypeHash(Register.Index);
}

struct FConstantIndex
{
	// Unsigned, but must be less than or equal to INT32_MAX
	uint32 Index;
};

struct FValueOperand
{
	static constexpr uint32 UNINITIALIZED = INT32_MAX;

	union
	{
		uint32 Index;
		FRegisterIndex Register;
	};

	static_assert(sizeof(Index) == sizeof(Register));

	FValueOperand()
		: Index(UNINITIALIZED)
	{
	}

	FValueOperand(FRegisterIndex Register)
		: Index(Register.Index)
	{
		check(Register.Index < UNINITIALIZED);
		check(IsRegister());
	}
	FValueOperand(FConstantIndex Constant)
		: Index{~Constant.Index}
	{
		check(Constant.Index <= UNINITIALIZED);
		check(IsConstant());
	}

	FORCEINLINE bool IsRegister() const { return Index < UNINITIALIZED; }
	FORCEINLINE bool IsConstant() const { return UNINITIALIZED < Index; }

	FORCEINLINE FRegisterIndex& AsRegister()
	{
		checkSlow(IsRegister());
		return Register;
	}

	FORCEINLINE FConstantIndex AsConstant() const
	{
		checkSlow(IsConstant());
		return FConstantIndex{~Index};
	}

	template <typename TVisitor>
	friend void Visit(TVisitor& Visitor, FValueOperand& Value)
	{
		Visitor.Visit(Value.Index, TEXT("Index"));
	}
};

template <typename OperandType>
struct TOperandRange
{
	int32 Index;
	int32 Num;

	template <typename TVisitor>
	friend void Visit(TVisitor& Visitor, TOperandRange& Value) {}
};

// We align the bytecode stream to 8 bytes so we don't see tearing from the collector,
// and in the future other concurrent threads, when writing to a VValue/pointer sized
// entry.
constexpr uint8 OpAlignment = alignof(void*);

struct FLabelOffset;

struct alignas(OpAlignment) FOp
{
	EOpcode Opcode;

	explicit FOp(const EOpcode InOpcode)
		: Opcode(InOpcode) {}

	// Function should take as parameters (EOperandRole, FRegisterIndex) where FRegisterIndex can be a reference or a value.
	// This vends the registers used in this FOp.
	template <typename FunctionType>
	void ForEachReg(VProcedure& Procedure, FunctionType&& Function);

	// Function should take as parameters (FLabelOffset, const TCHAR*) where FLabelOffset can be a reference or a value.
	// This vends the jumps used in this FOp.
	template <typename FunctionType>
	void ForEachJump(VProcedure&, FunctionType&& Function);

private:
	template <typename FunctionType>
	static void ForEachRegImpl(VProcedure&, FRegisterIndex& Register, FunctionType&& Function);

	template <typename FunctionType>
	static void ForEachRegImpl(VProcedure&, FValueOperand& Operand, FunctionType&& Function);

	template <typename CellType, typename FunctionType>
	static void ForEachRegImpl(VProcedure&, TWriteBarrier<CellType>&, FunctionType&&);

	template <typename FunctionType>
	static void ForEachRegImpl(VProcedure& Procedure, TOperandRange<FValueOperand> ValueOperands, FunctionType&& Function);

	template <typename CellType, typename FunctionType>
	static void ForEachRegImpl(VProcedure&, TOperandRange<TWriteBarrier<CellType>>, FunctionType&&);

	template <typename FunctionType>
	static void ForEachJumpImpl(VProcedure&, TOperandRange<FLabelOffset> LabelOffsets, const TCHAR* Name, FunctionType&&);

	template <typename FunctionType>
	static void ForEachJumpImpl(VProcedure&, FLabelOffset&, const TCHAR* Name, FunctionType&&);
};

struct FLabelOffset
{
	int32 Offset; // In bytes, relative to the address of this FLabelOffset

	FOp* GetLabeledPC() const
	{
		return const_cast<FOp*>(BitCast<const FOp*>(BitCast<const uint8*>(this) + Offset));
	}

	template <typename TVisitor>
	friend void Visit(TVisitor& Visitor, FLabelOffset& Value)
	{
		Visitor.Visit(Value.Offset, TEXT("Index"));
	}
};

// A range of opcode bytes, with a target label for unwinding from calls within that range.
// VProcedure holds a sorted array of non-overlapping unwind edges.
struct FUnwindEdge
{
	int32 Begin;
	int32 End;
	FLabelOffset OnUnwind;

	template <typename TVisitor>
	friend void Visit(TVisitor& Visitor, FUnwindEdge& Value)
	{
		Visitor.Visit(Value.Begin, TEXT("Begin"));
		Visitor.Visit(Value.End, TEXT("End"));
		Visitor.Visit(Value.OnUnwind, TEXT("OnUnwind"));
	}
};

// Mapping of a parameter name to its corresponding register. VProcedures hold an array of such mappings.
struct FNamedParam
{
	TWriteBarrier<VUniqueString> Name;
	FRegisterIndex Index;

	FNamedParam() = default;

	FNamedParam(FAccessContext Context, VUniqueString& InName, FRegisterIndex InIndex)
		: Name(Context, InName)
		, Index(InIndex)
	{
	}

	template <typename TVisitor>
	friend void Visit(TVisitor& Visitor, FNamedParam& Value)
	{
		Visitor.Visit(Value.Name, TEXT("Name"));
		Visitor.Visit(Value.Index, TEXT("Index"));
	}
};

// Mapping from an opcode offset to a location.  VProcedure holds a sorted array of such
// mappings where an op's location is the latest entry with an equal or lesser offset.
struct FOpLocation
{
	int32 Begin;
	FLocation Location;

	template <typename TVisitor>
	friend void Visit(TVisitor& Visitor, FOpLocation& Value)
	{
		Visitor.Visit(Value.Begin, TEXT("Begin"));
		Visitor.Visit(Value.Location, TEXT("Location"));
	}
};

COREUOBJECT_API const FLocation* GetLocation(FOpLocation* First, FOpLocation* Last, uint32 OpOffset);

// Mapping from register index to name. VProcedures hold an array of such mappings.
struct FRegisterName
{
	FRegisterIndex Index;
	TWriteBarrier<VUniqueString> Name;

	FRegisterName() = default;

	FRegisterName(FAccessContext InContext, FRegisterIndex InIndex, VUniqueString& InName)
		: Index(InIndex)
		, Name(InContext, InName)
	{
	}

	template <typename TVisitor>
	friend void Visit(TVisitor& Visitor, FRegisterName& Value)
	{
		Visitor.Visit(Value.Index, TEXT("Index"));
		Visitor.Visit(Value.Name, TEXT("Name"));
	}
};

struct FFailureContextId
{
private:
	uint32 Id;

public:
	FFailureContextId(uint32 Id)
		: Id(Id)
	{
	}
	bool operator==(const FFailureContextId& Other) const
	{
		return Id == Other.Id;
	}
	uint32 GetTypeHash() const
	{
		using ::GetTypeHash;
		return GetTypeHash(Id);
	}
};

inline uint32 GetTypeHash(const FFailureContextId& Id)
{
	return Id.GetTypeHash();
}

} // namespace Verse
#endif // WITH_VERSE_VM
