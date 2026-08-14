// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMBytecodePrinting.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "VerseVM/Inline/VVMValueInline.h"
#include "VerseVM/VVMBytecode.h"
#include "VerseVM/VVMBytecodeAnalysis.h"
#include "VerseVM/VVMBytecodeDispatcher.h"
#include "VerseVM/VVMCVars.h"
#include "VerseVM/VVMLog.h"
#include "VerseVM/VVMPackage.h"
#include "VerseVM/VVMProcedure.h"
#include "VerseVM/VVMValuePrinting.h"
#include <inttypes.h>

namespace Verse
{
namespace
{
struct FJumpTargetHandler
{
	VProcedure& Procedure;
	TMap<const FOp*, FString> JumpTargetToLabelIndexMap;

	template <typename OpType>
	void operator()(OpType& Op)
	{
		Op.ForEachJump(*this);
	}

	void operator()(FLabelOffset& LabelOffset, const TCHAR* Name)
	{
		const FOp* TargetOp = LabelOffset.GetLabeledPC();
		if (!JumpTargetToLabelIndexMap.Contains(TargetOp))
		{
			JumpTargetToLabelIndexMap.Add(TargetOp, FString::Printf(TEXT("L%u"), JumpTargetToLabelIndexMap.Num()));
		}
	}

	void operator()(TOperandRange<FLabelOffset> LabelOffsets, const TCHAR* Name)
	{
		for (int32 Index = 0; Index < LabelOffsets.Num; ++Index)
		{
			(*this)(Procedure.GetLabelsBegin()[LabelOffsets.Index + Index], Name);
		}
	}
};

struct FBytecodePrinter
{
	FBytecodePrinter(FAllocationContext Context, VProcedure& Procedure)
		: Context(Context)
		, Procedure(Procedure)
		, JumpTargetHandler{Procedure}
	{
		JumpTargetHandler.JumpTargetToLabelIndexMap.Add(Procedure.GetOpsBegin(), TEXT("Entry"));
	}

	FString Print()
	{
		// Do a pre-pass over the procedure's ops to find jump targets.
		DispatchOps(Procedure, JumpTargetHandler);

		// Print the procedure definition.
		String += FString::Printf(
			TEXT("%s = procedure(%p):\n"),
			*Procedure.Name->AsString(),
			&Procedure);

		// Print the procedure constant table.
		for (uint32 ConstantIndex = 0; ConstantIndex < Procedure.NumConstants; ++ConstantIndex)
		{
			String += FString::Printf(TEXT("    c%u = %s\n"),
				ConstantIndex,
				*WriteToString<64>(Procedure.GetConstant(FConstantIndex{ConstantIndex}).ToString(Context, EValueStringFormat::CellsWithAddresses)));
		}

		// Print info about the procedure's frame.
		if (Procedure.NumRegisters)
		{
			String += FString::Printf(TEXT("    # Frame contains %u registers: r0..r%u\n"),
				Procedure.NumRegisters,
				Procedure.NumRegisters - 1);
		}

		String += FString::Printf(TEXT("    # Frame contains %u positional parameters\n"), Procedure.NumPositionalParameters);
		String += FString::Printf(TEXT("    # Frame contains %u named parameters\n"), Procedure.NumNamedParameters);

		if (Procedure.NumRegisterNames)
		{
			FRegisterName* RegisterNames = Procedure.GetRegisterNamesBegin();
			String += FString::Printf(TEXT("    # Frame contains %u named registers:\n"), Procedure.NumRegisterNames);
			for (uint32 i = 0; i < Procedure.NumRegisterNames; ++i)
			{
				String += FString::Printf(TEXT("    #   r%u => %s\n"),
					RegisterNames[i].Index.Index,
					*RegisterNames[i].Name->AsString());
			}
		}

		// Print the procedure's ops.
		if (CVarDumpBytecodeAsCFG.GetValueOnAnyThread())
		{
			TUniquePtr<BytecodeAnalysis::FCFG> CFG = BytecodeAnalysis::MakeBytecodeCFG(Procedure);
			TUniquePtr<BytecodeAnalysis::FLiveness> Liveness = BytecodeAnalysis::ComputeBytecodeLiveness(*CFG, Procedure);
			for (uint32 BlockIndex = 0; BlockIndex < CFG->Blocks.Num(); ++BlockIndex)
			{
				BytecodeAnalysis::FBasicBlock* Block = CFG->Blocks[BlockIndex].Get();
				String += FString::Printf(TEXT("bb#%u:\n"), BlockIndex);

				auto GetLiveness = [](const BytecodeAnalysis::FLiveSet& Live) -> FString {
					bool bDidPrint = false;
					FString Result = TEXT("[");
					Live.ForEach([&](FRegisterIndex Register) {
						if (bDidPrint)
						{
							Result += TEXT(", ");
						}
						Result += FString::Printf(TEXT("r%u"), Register.Index);
						bDidPrint = true;
					});
					Result += TEXT("]");
					return Result;
				};

				auto GetBlocks = [](const TArray<BytecodeAnalysis::FBasicBlock*>& Blocks) -> FString {
					FString Result = TEXT("[");
					bool bDidPrint = false;
					for (BytecodeAnalysis::FBasicBlock* Block : Blocks)
					{
						if (bDidPrint)
						{
							Result += TEXT(", ");
						}
						Result += FString::Printf(TEXT("bb#%u"), Block->Index);
						bDidPrint = true;
					}
					Result += TEXT("]");
					return Result;
				};

				String += FString::Printf(TEXT("  live in: %s\n"), *GetLiveness(Liveness->LiveIn[BlockIndex]));
				String += FString::Printf(TEXT("  Predecessors: %s\n"), *GetBlocks(Block->Predecessors));

				FOp* Op = Procedure.GetPCForOffset(Block->First());
				FOp* End = Procedure.GetPCForOffset(Block->Last());
				while (Op <= End)
				{
					Op = DispatchOp(Op, *this);
				}

				String += FString::Printf(TEXT("  live out: %s\n"), *GetLiveness(Liveness->LiveOut[BlockIndex]));
				String += FString::Printf(TEXT("  Successors: %s\n\n"), *GetBlocks(Block->Successors));
			}
		}
		else
		{
			DispatchOps(Procedure, *this);
		}
		PrintLabelIfNeeded(Procedure.GetOpsEnd());

		return MoveTemp(String);
	}

	void PrintLabelIfNeeded(const FOp* Op)
	{
		// If this op is the target of a jump, print a label before it.
		if (FString* Label = JumpTargetHandler.JumpTargetToLabelIndexMap.Find(Op))
		{
			String += TEXT("   ");
			String += *Label;
			String += TEXT(":\n");
		}
	}

	template <typename OpType>
	void operator()(OpType& Op)
	{
		PrintLabelIfNeeded(&Op);

		String += FString::Printf(TEXT("    %5u | "), Procedure.BytecodeOffset(Op));

		PrintOpWithOperands(Op);

		String += TEXT('\n');
	}

private:
	FAllocationContext Context;
	VProcedure& Procedure;
	FString String;

	FJumpTargetHandler JumpTargetHandler;

	void PrintRegister(FRegisterIndex Register)
	{
		if (Register.Index == FRegisterIndex::UNINITIALIZED)
		{
			String += FString::Printf(TEXT("r(UNINITIALIZED)"));
		}
		else
		{
			String += FString::Printf(TEXT("r%u"), Register.Index);
		}
	}

	void PrintValueOperand(FRegisterIndex Operand)
	{
		PrintRegister(Operand);
	}

	void PrintValueOperand(FValueOperand ValueOperand)
	{
		if (ValueOperand.IsRegister())
		{
			PrintRegister(ValueOperand.AsRegister());
		}
		else if (ValueOperand.IsConstant())
		{
			FConstantIndex ConstantIndex = ValueOperand.AsConstant();
			String += FString::Printf(TEXT("c%u="), ConstantIndex.Index);
			String += Procedure.GetConstant(ConstantIndex).ToString(Context, EValueStringFormat::CellsWithAddresses);
		}
		else
		{
			String += "Empty";
		}
	}

	template <typename CellType>
	void PrintValueOperand(TWriteBarrier<CellType>& ValueOperand)
	{
		if constexpr (TWriteBarrier<CellType>::bIsVValue)
		{
			String += ValueOperand.Get().ToString(Context, EValueStringFormat::CellsWithAddresses);
		}
		else
		{
			String += ValueOperand->ToString(Context, EValueStringFormat::CellsWithAddresses);
		}
	}

	void PrintValueOperand(TOperandRange<FValueOperand> ValueOperands)
	{
		String += TEXT("(");
		const TCHAR* Separator = TEXT("");
		for (int32 Index = 0; Index < ValueOperands.Num; ++Index)
		{
			String += Separator;
			Separator = TEXT(", ");
			PrintValueOperand(Procedure.GetOperandsBegin()[ValueOperands.Index + Index]);
		}
		String += TEXT(")");
	}

	template <typename CellType>
	void PrintValueOperand(TOperandRange<TWriteBarrier<CellType>> ValueOperands)
	{
		TWriteBarrier<CellType>* Constants = BitCast<TWriteBarrier<CellType>*>(Procedure.GetConstantsBegin());
		String += TEXT("(");
		const TCHAR* Separator = TEXT("");
		for (int32 Index = 0; Index < ValueOperands.Num; ++Index)
		{
			String += Separator;
			Separator = TEXT(", ");
			PrintValueOperand(Constants[ValueOperands.Index + Index]);
		}
		String += TEXT(")");
	}

	void PrintJumpOperand(FLabelOffset& Label)
	{
		FString* TargetLabel = JumpTargetHandler.JumpTargetToLabelIndexMap.Find(Label.GetLabeledPC());
		check(TargetLabel);
		String += *TargetLabel;
	}

	void PrintJumpOperand(TOperandRange<FLabelOffset> Labels)
	{
		String += TEXT("(");
		const TCHAR* Separator = TEXT("");
		for (int32 Index = 0; Index < Labels.Num; ++Index)
		{
			String += Separator;
			Separator = TEXT(", ");
			PrintJumpOperand(Procedure.GetLabelsBegin()[Labels.Index + Index]);
		}
		String += TEXT(")");
	}

	template <typename OpType>
	void PrintOpWithOperands(OpType& Op)
	{
		const TCHAR* Separator = TEXT("");
		auto PrintSeparator = [&] {
			String += Separator;
			Separator = TEXT(", ");
		};

		bool bPrintedOp = false;
		auto PrintOp = [&] {
			if (!bPrintedOp)
			{
				String += ToString(Op.Opcode);
				String += TEXT("(");
				bPrintedOp = true;
			}
		};

		// Right now we just assume that Defs come before Uses, but we could rework this
		// if this ever breaks printing.
		Op.ForEachOperand([&](EOperandRole Role, auto& Operand, const TCHAR* Name) {
			// NOTE: (yiliang.siew) Account for optional operands.
			switch (Role)
			{
				case EOperandRole::ClobberDef:
					PrintValueOperand(Operand);
					String += TEXT(" <- ");
					break;
				case EOperandRole::UnifyDef:
					PrintValueOperand(Operand);
					String += TEXT(" = ");
					break;
				case EOperandRole::Use:
				case EOperandRole::Immediate:
					PrintOp();
					PrintSeparator();
					String.Append(Name).Append(TEXT(": "));
					PrintValueOperand(Operand);
					break;
				default:
					VERSE_UNREACHABLE();
			}
		});

		PrintOp();

		Op.ForEachJump([&](auto& Label, const TCHAR* Name) {
			PrintSeparator();
			String.Append(Name).Append(": ");
			PrintJumpOperand(Label);
		});

		String += TEXT(")");
	}
};
} // namespace
} // namespace Verse

FString Verse::PrintProcedure(FAllocationContext Context, VProcedure& Procedure)
{
	FBytecodePrinter Printer{Context, Procedure};
	return Printer.Print();
}
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
