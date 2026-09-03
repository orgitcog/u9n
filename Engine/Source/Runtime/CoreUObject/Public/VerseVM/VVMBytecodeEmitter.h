// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "Containers/Array.h"
#include "Misc/AssertionMacros.h"
#include "VerseVM/VVMBytecode.h"
#include "VerseVM/VVMBytecodeOps.h"
#include "VerseVM/VVMBytecodesAndCaptures.h"
#include "VerseVM/VVMFalse.h"
#include "VerseVM/VVMLog.h"
#include "VerseVM/VVMValue.h"

namespace Verse
{
struct VProcedure;
struct FAllocationContext;

struct FOpEmitter
{
	struct FLabel
	{
	private:
		friend struct FOpEmitter;
		uint32 Index;
		FLabel(uint32 InIndex)
			: Index(InIndex)
		{
		}
	};

	FOpEmitter(FAllocationContext Context, VUniqueString& FilePath, VUniqueString& ProcedureName, uint32 NumPositionalParameters, uint32 NumNamedParameters)
		: FilePath(Context, FilePath)
		, ProcedureName(Context, ProcedureName)
		, NumRegisters(FRegisterIndex::PARAMETER_START + NumPositionalParameters + NumNamedParameters)
		, NumPositionalParameters(NumPositionalParameters)
		, NumNamedParameters(NumNamedParameters)
	{
		// Sentinel unwind region covering the function body.
		UnwindStack.Push(FUnwindRegion{FLabel(0), 0, false});
	}

	bool IsEmpty() const { return OpBytes.IsEmpty(); }

	FConstantIndex AllocateConstant(VValue Constant)
	{
		const uint32 ConstantIndex = Constants.Add(Constant);
		return FConstantIndex{ConstantIndex};
	}

	FRegisterIndex NoRegister()
	{
		return FRegisterIndex{FRegisterIndex::UNINITIALIZED};
	}

	FRegisterIndex Self()
	{
		return FRegisterIndex{FRegisterIndex::SELF};
	}

	FRegisterIndex Scope()
	{
		return FRegisterIndex{FRegisterIndex::SCOPE};
	}

	FRegisterIndex Argument(uint32 ArgumentIndex)
	{
		V_DIE_UNLESS(ArgumentIndex < NumPositionalParameters + NumNamedParameters);
		return FRegisterIndex{FRegisterIndex::PARAMETER_START + ArgumentIndex};
	}

	void AddRegisterName(FAccessContext Context, FRegisterIndex Index, VUniqueString& Name)
	{
		RegisterNames.Emplace(Context, Index, Name);
	}

	FRegisterIndex AllocateRegisterNoReset()
	{
		const uint32 RegisterIndex = NumRegisters++;
		return FRegisterIndex{RegisterIndex};
	}

	FRegisterIndex AllocateRegisterNoReset(FAccessContext Context, VUniqueString& Name)
	{
		FRegisterIndex Result = AllocateRegisterNoReset();
		AddRegisterName(Context, Result, Name);
		return Result;
	}

	FRegisterIndex AllocateRegister(const FLocation& Location)
	{
		const FRegisterIndex Register = AllocateRegisterNoReset();
		Reset(Location, Register);
		return Register;
	}

	FRegisterIndex AllocateRegister(FAccessContext Context, VUniqueString& Name, const FLocation& Location)
	{
		const FRegisterIndex Register = AllocateRegisterNoReset(Context, Name);
		Reset(Location, Register);
		return Register;
	}

	FLabel AllocateLabel()
	{
		const uint32 LabelIndex = LabelOffsets.Add(UINT32_MAX);
		return FLabel(LabelIndex);
	}

	void BindLabel(FLabel Label)
	{
		LabelOffsets[Label.Index] = OpBytes.Num();
	}

	FLabel AllocateAndBindLabel()
	{
		FLabel Label = AllocateLabel();
		BindLabel(Label);
		return Label;
	}

	uint32 GetOffsetForLabel(FLabel Label) const
	{
		return LabelOffsets[Label.Index];
	}

	void EnterUnwindRegion(FLabel OnUnwind)
	{
		int32 Offset = OpBytes.Num();

		FUnwindRegion& Outer = UnwindStack.Top();
		if (Outer.bHandlesUnwind)
		{
			UnwindEdges.Add(FUnwindEdge{Outer.Begin, Offset, CoerceOperand(Outer.OnUnwind)});
		}

		FUnwindRegion& Inner = UnwindStack.Emplace_GetRef(OnUnwind);
		Inner.Begin = Offset;
		Inner.bHandlesUnwind = false;
	}

	void NoteUnwind()
	{
		// The outer sentinel region never handles an unwind.
		if (UnwindStack.Num() > 1)
		{
			UnwindStack.Top().bHandlesUnwind = true;
		}
	}

	void LeaveUnwindRegion()
	{
		int32 Offset = OpBytes.Num();

		FUnwindRegion Inner = UnwindStack.Pop();
		if (Inner.bHandlesUnwind)
		{
			UnwindEdges.Add(FUnwindEdge{Inner.Begin, Offset, CoerceOperand(Inner.OnUnwind)});
		}

		FUnwindRegion& Outer = UnwindStack.Top();
		Outer.Begin = Offset;
		Outer.bHandlesUnwind = false;
	}

	COREUOBJECT_API VProcedure& MakeProcedure(FAllocationContext Context);

#define VISIT_OP(Name)                                                  \
	template <typename... ArgumentTypes>                                \
	int32 Name(ArgumentTypes&&... Arguments)                            \
	{                                                                   \
		return EmitOp<FOp##Name>(Forward<ArgumentTypes>(Arguments)...); \
	}
	VERSE_ENUM_OPS(VISIT_OP)
#undef VISIT_OP

	int32 Fail(const FLocation& Location)
	{
		return Query(Location, AllocateRegister(Location), AllocateConstant(VValue(GlobalFalse())));
	}

	template <typename Opcode, typename... ArgumentTypes>
	int32 EmitOp(const FLocation& Location, ArgumentTypes&&... Arguments)
	{
		const int32 Offset = OpBytes.AddUninitialized(sizeof(Opcode));
		if (OpLocations.IsEmpty() || OpLocations.Top().Location != Location)
		{
			OpLocations.Emplace(Offset, Location);
		}
		Opcode* NewOp = new (std::launder(OpBytes.GetData() + Offset)) Opcode(CoerceOperand(Forward<ArgumentTypes>(Arguments))...);
		checkSlow((BitCast<uint64>(NewOp) % alignof(Opcode)) == 0);
		RegisterJumpRelocation(*NewOp);
		return Offset;
	}

	inline uint32 GetNumPositionalParameters() const { return NumPositionalParameters; }
	inline uint32 GetNumNamedParameters() const { return NumNamedParameters; }

	TArray<FNamedParam> NamedParameters;

	FFailureContextId GetNextFailureContextId() { return FFailureContextId(NextFailureContextId++); }

	void DisableRegisterAllocation() { bEnableRegisterAllocation = false; }

private:
	TWriteBarrier<VUniqueString> FilePath;
	TWriteBarrier<VUniqueString> ProcedureName;
	uint32 NumRegisters;
	uint32 NumPositionalParameters;
	uint32 NumNamedParameters;
	TArray<VValue> Constants;
	TArray<uint8> OpBytes;
	TArray<FValueOperand> Operands;
	TArray<FLabel> Labels;
	TArray<FUnwindEdge> UnwindEdges;
	uint32 NextFailureContextId{0};
	bool bEnableRegisterAllocation{true};

	// An open region of the program with an associated unwind label. Nested regions wrap this label with their own.
	// As code generation proceeds, this stack is flattened into a series of non-overlapping "unwind edges."
	struct FUnwindRegion
	{
		FLabel OnUnwind;
		int32 Begin;
		bool bHandlesUnwind;
	};
	TArray<FUnwindRegion> UnwindStack;

	TArray<uint32> LabelOffsets;
	TArray<uint32> LabelOffsetOffsets;

	TArray<FOpLocation> OpLocations;

	// Elements are not guaranteed to be unique and should not be relied on outside of debugging purposes.
	TArray<FRegisterName> RegisterNames;

	template <typename ArgumentType>
	decltype(auto) CoerceOperand(ArgumentType&& Operand)
	{
		return Forward<ArgumentType>(Operand);
	}

	template <typename CellType, typename AllocatorType>
	TOperandRange<TWriteBarrier<CellType>> CoerceOperand(TArray<TWriteBarrier<CellType>, AllocatorType> InOperands)
	{
		int32 Index = Constants.Num();
		for (TWriteBarrier<CellType>& Operand : InOperands)
		{
			if constexpr (TWriteBarrier<CellType>::bIsVValue)
			{
				Constants.Add(Operand.Get());
			}
			else
			{
				Constants.Add(*Operand);
			}
		}
		return TOperandRange<TWriteBarrier<CellType>>{Index, InOperands.Num()};
	}

	template <typename AllocatorType>
	TOperandRange<FValueOperand> CoerceOperand(TArray<FValueOperand, AllocatorType> InOperands)
	{
		int32 Index = Operands.Num();
		Operands.Append(InOperands);
		return TOperandRange<FValueOperand>{Index, InOperands.Num()};
	}

	FLabelOffset CoerceOperand(FLabel Label)
	{
		// Store the label index as an offset until it can be replaced with the actual label offset.
		check(Label.Index < INT32_MAX);
		return FLabelOffset{static_cast<int32>(Label.Index)};
	}

	template <typename AllocatorType>
	TOperandRange<FLabelOffset> CoerceOperand(TArray<FLabel, AllocatorType> InLabels)
	{
		for (FLabel Label : InLabels)
		{
			check(Label.Index < INT32_MAX);
		}
		int32 Index = Labels.Num();
		Labels.Append(InLabels);
		return TOperandRange<FLabelOffset>{Index, InLabels.Num()};
	}

	// Don't allow emitting label offsets directly.
	FLabelOffset CoerceOperand(FLabelOffset LabelOffset) = delete;
	FLabelOffset CoerceOperand(const TArrayView<FLabel>& LabelOffset) = delete;

	template <typename OpType>
	void RegisterJumpRelocation(OpType& Op)
	{
		Op.ForEachJump([&](auto& Label, const TCHAR* Name) {
			RegisterLabelRelocation(Label);
		});
	}

	void RegisterLabelRelocation(FLabelOffset& LabelOffset)
	{
		// Store the offset of this jump for fixup once we know the location of all labels.
		const size_t LabelOffsetOffset = BitCast<uint8*>(&LabelOffset) - OpBytes.GetData();
		check(LabelOffsetOffset <= UINT32_MAX);
		LabelOffsetOffsets.Add(static_cast<uint32>(LabelOffsetOffset));
	}

	void RegisterLabelRelocation(TOperandRange<FLabelOffset>& InLabelOffsets)
	{
		// Variadic label operands live in one flat array; no need to remember their location here.
	}
};
} // namespace Verse

#endif
