// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VerseVM/VVMBytecode.h"

#include "VerseVM/VVMBytecodesAndCaptures.h"

namespace Verse
{
template <typename FunctionType>
void FOp::ForEachReg(VProcedure& Procedure, FunctionType&& Function)
{
	switch (Opcode)
	{
#define VISIT_OP(Name)                                                                       \
	case EOpcode::Name:                                                                      \
	{                                                                                        \
		FOp##Name* DerivedOp = static_cast<FOp##Name*>(this);                                \
		DerivedOp->ForEachOperand([&](EOperandRole Role, auto& Operand, const TCHAR* Name) { \
			ForEachRegImpl(Procedure, Operand, [&](FRegisterIndex& Register) {               \
				Function(Role, Register);                                                    \
			});                                                                              \
		});                                                                                  \
		break;                                                                               \
	}

		VERSE_ENUM_OPS(VISIT_OP)
#undef VISIT_OP
	}
}

template <typename FunctionType>
void FOp::ForEachRegImpl(VProcedure&, FRegisterIndex& Register, FunctionType&& Function)
{
	if (Register.Index != FRegisterIndex::UNINITIALIZED)
	{
		Function(Register);
	}
}

template <typename FunctionType>
void FOp::ForEachRegImpl(VProcedure&, FValueOperand& Operand, FunctionType&& Function)
{
	if (Operand.IsRegister() && Operand.AsRegister().Index != FRegisterIndex::UNINITIALIZED)
	{
		Function(Operand.AsRegister());
	}
}

template <typename CellType, typename FunctionType>
void FOp::ForEachRegImpl(VProcedure&, TWriteBarrier<CellType>&, FunctionType&&)
{
}

template <typename FunctionType>
void FOp::ForEachRegImpl(VProcedure& Procedure, TOperandRange<FValueOperand> ValueOperands, FunctionType&& Function)
{
	for (uint32 Index = 0; Index < ValueOperands.Num; ++Index)
	{
		FValueOperand& Operand = Procedure.GetOperandsBegin()[ValueOperands.Index + Index];
		if (Operand.IsRegister() && Operand.AsRegister().Index != FRegisterIndex::UNINITIALIZED)
		{
			Function(Operand.AsRegister());
		}
	}
}

template <typename CellType, typename FunctionType>
void FOp::ForEachRegImpl(VProcedure&, TOperandRange<TWriteBarrier<CellType>>, FunctionType&&)
{
}

template <typename FunctionType>
void FOp::ForEachJump(VProcedure& Procedure, FunctionType&& Function)
{
	switch (Opcode)
	{
#define VISIT_OP(Name)                                                                   \
	case EOpcode::Name:                                                                  \
	{                                                                                    \
		static_cast<FOp##Name*>(this)->ForEachJump([&](auto& Label, const TCHAR* Name) { \
			ForEachJumpImpl(Procedure, Label, Name, Forward<FunctionType>(Function));    \
		});                                                                              \
		break;                                                                           \
	}

		VERSE_ENUM_OPS(VISIT_OP)
#undef VISIT_OP

		default:
			V_DIE("Invalid opcode encountered: %u", static_cast<FOpcodeInt>(Opcode));
			break;
	}
}

template <typename FunctionType>
void FOp::ForEachJumpImpl(VProcedure& Procedure, TOperandRange<FLabelOffset> LabelOffsets, const TCHAR* Name, FunctionType&& Function)
{
	for (uint32 I = 0; I < LabelOffsets.Num; ++I)
	{
		Function(Procedure.GetLabelsBegin()[LabelOffsets.Index + I], Name);
	}
}

template <typename FunctionType>
void FOp::ForEachJumpImpl(VProcedure&, FLabelOffset& Label, const TCHAR* Name, FunctionType&& Function)
{
	Function(Label, Name);
}

} // namespace Verse
#endif // WITH_VERSE_VM
