// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VerseVM/VVMBytecodeAnalysis.h"

#include "Algo/BinarySearch.h"
#include "Misc/ReverseIterate.h"
#include "VerseVM/Inline/VVMBytecodeInline.h"
#include "VerseVM/VVMBytecodeDispatcher.h"
#include "VerseVM/VVMBytecodesAndCaptures.h"

namespace Verse
{

namespace BytecodeAnalysis
{

static TSet<uint32> ComputeJumpTargets(VProcedure& Procedure)
{
	TSet<uint32> Targets;

	DispatchOps(Procedure, [&](FOp& CurrentOp) {
		bool bHandled = true;

		EOpcode Opcode = CurrentOp.Opcode;

		auto AddOffset = [&](FLabelOffset& LabelOffset, const TCHAR* Name) {
			// Make sure this analysis and IsBranch stay in sync.
			V_DIE_UNLESS(IsBranch(Opcode) || Opcode == EOpcode::BeginFailureContext || Opcode == EOpcode::EndFailureContext || Opcode == EOpcode::BeginTask);
			Targets.Add(Procedure.BytecodeOffset(LabelOffset.GetLabeledPC()));
		};

		switch (Opcode)
		{
			case EOpcode::Jump:
				static_cast<FOpJump&>(CurrentOp).ForEachJump(AddOffset);
				break;
			case EOpcode::JumpIfInitialized:
				static_cast<FOpJumpIfInitialized&>(CurrentOp).ForEachJump(AddOffset);
				break;
			case EOpcode::JumpIfArchetype:
				static_cast<FOpJumpIfArchetype&>(CurrentOp).ForEachJump(AddOffset);
				break;
			case EOpcode::Switch:
				static_cast<FOpSwitch&>(CurrentOp).ForEachJump([&](TOperandRange<FLabelOffset> LabelOffsets, const TCHAR* Name) {
					for (uint32 I = 0; I < LabelOffsets.Num; ++I)
					{
						AddOffset(Procedure.GetLabelsBegin()[LabelOffsets.Index + I], Name);
					}
				});
				break;
			case EOpcode::BeginFailureContext:
				// We treat the failure PC as a jump target.
				static_cast<FOpBeginFailureContext&>(CurrentOp).ForEachJump(AddOffset);
				break;
			case EOpcode::EndFailureContext:
				// The label for this opcode is just to branch around the then/else during leniency.
				// We don't model this.
				break;
			case EOpcode::BeginTask:
				// The yield PC is jumped to by something, even though we don't know what will jump there.
				static_cast<FOpBeginTask&>(CurrentOp).ForEachJump(AddOffset);
				break;
			default:
				bHandled = false;
				break;
		}

		if (!bHandled)
		{
			CurrentOp.ForEachJump(Procedure, [](auto&, const TCHAR*) {
				V_DIE("Jump should be handled above.");
			});
		}
	});

	return Targets;
}

TUniquePtr<FCFG> MakeBytecodeCFG(VProcedure& Procedure)
{
	TSet<uint32> JumpTargets = ComputeJumpTargets(Procedure);
	TUniquePtr<FCFG> CFG(new FCFG);
	FBasicBlock* CurrentBlock = nullptr;
	bool bNextInstructionStartsNewBlock = true; // 0 is the entrypoint.
	DispatchOps(Procedure, [&](FOp& Op) {
		uint32 Offset = Procedure.BytecodeOffset(Op);
		if (bNextInstructionStartsNewBlock || JumpTargets.Contains(Offset))
		{
			if (CurrentBlock)
			{
				CFG->Blocks.Push(TUniquePtr<FBasicBlock>(CurrentBlock));
			}
			CurrentBlock = new FBasicBlock;
			bNextInstructionStartsNewBlock = false;
		}

		CurrentBlock->Bytecodes.Push(Offset);

		if (IsBranch(Op.Opcode) || IsTerminal(Op.Opcode))
		{
			bNextInstructionStartsNewBlock = true;
		}
	});

	CFG->Blocks.Push(TUniquePtr<FBasicBlock>(CurrentBlock));

	for (FBlockIndex I = 0; I < CFG->NumBlocks(); ++I)
	{
		FBasicBlock* Block = CFG->Blocks[I].Get();
		Block->Index = I;
	}

	auto FindBlock = [&](FOp* Op) -> FBasicBlock& {
		return CFG->GetJumpTarget(Procedure.BytecodeOffset(Op));
	};

	// Compute successors
	for (FBlockIndex I = 0; I < CFG->NumBlocks(); ++I)
	{
		FBasicBlock* Block = CFG->Blocks[I].Get();

		auto AppendSuccessor = [&](FBasicBlock& Successor) {
			if (!Block->Successors.Contains(&Successor))
			{
				Block->Successors.Push(&Successor);
			}

			if (!Successor.Predecessors.Contains(Block))
			{
				Successor.Predecessors.Push(Block);
			}
		};

		FOp* LastOp = Procedure.GetPCForOffset(Block->Last());
		switch (LastOp->Opcode)
		{
			case EOpcode::Jump:
				AppendSuccessor(FindBlock(static_cast<FOpJump*>(LastOp)->JumpOffset.GetLabeledPC()));
				break;
			case EOpcode::JumpIfInitialized:
				AppendSuccessor(FindBlock(static_cast<FOpJumpIfInitialized*>(LastOp)->JumpOffset.GetLabeledPC()));
				AppendSuccessor(*CFG->Blocks[I + 1]); // Fall through to the next one.
				break;
			case EOpcode::JumpIfArchetype:
				AppendSuccessor(FindBlock(static_cast<FOpJumpIfArchetype*>(LastOp)->JumpOffset.GetLabeledPC()));
				AppendSuccessor(*CFG->Blocks[I + 1]); // Fall through to the next one.
				break;
			case EOpcode::Switch:
			{
				TOperandRange<FLabelOffset> LabelOffsets = static_cast<FOpSwitch*>(LastOp)->JumpOffsets;
				for (uint32 J = 0; J < LabelOffsets.Num; ++J)
				{
					AppendSuccessor(FindBlock(Procedure.GetLabelsBegin()[LabelOffsets.Index + J].GetLabeledPC()));
				}
				break;
			}
			case EOpcode::EndTask:
				break;
			default:
				if (MightFallThrough(LastOp->Opcode))
				{
					AppendSuccessor(*CFG->Blocks[I + 1]);
				}
				break;
		}
	}

	// Compute the mapping from bytecode offset to failure context. We require that the bytecode is constructed in such a way
	// so we can validate and produce the list of failure contexts in a single pass starting at the root block. The rule we validate
	// against is that every incoming edge to a basic block must contain the same failure context stack.
	{
		FBitArray VisitedBlocks;
		VisitedBlocks.Init(false, CFG->NumBlocks());

		TArray<TOptional<TArray<FFailureContext*>>> FailureContextsAtHead;
		FailureContextsAtHead.SetNum(CFG->NumBlocks());
		FailureContextsAtHead[0] = TArray<FFailureContext*>(); // The root starts without a failure context.

		TArray<FBasicBlock*> Worklist;
		Worklist.Push(CFG->Blocks[0].Get());

		TMap<FFailureContextId, FFailureContext*> AllocatedFailureContexts;

		auto GetOrCreateFailureContext = [&](FFailureContextId Id, FOp* FailurePC, FFailureContext* Parent) -> FFailureContext* {
			if (FFailureContext** Result = AllocatedFailureContexts.Find(Id))
			{
				V_DIE_UNLESS((*Result)->Parent == Parent);
				V_DIE_UNLESS((*Result)->FailurePC == FailurePC);
				return *Result;
			}
			FFailureContext* Result = new FFailureContext{Id, FailurePC, Parent};
			AllocatedFailureContexts.Add(Id, Result);
			CFG->FailureContexts.Push(TUniquePtr<FFailureContext>(Result));
			return Result;
		};

		auto MergeInto = [&](const TArray<FFailureContext*>& FailureContexts, FBasicBlock* Block) {
			Worklist.Push(Block);
			TOptional<TArray<FFailureContext*>>& IncomingFailureContexts = FailureContextsAtHead[Block->Index];
			if (!IncomingFailureContexts)
			{
				IncomingFailureContexts = FailureContexts;
			}
			else
			{
				V_DIE_UNLESS(IncomingFailureContexts.GetValue() == FailureContexts);
			}
		};

		while (Worklist.Num())
		{
			FBasicBlock* Block = Worklist.Pop();
			if (VisitedBlocks[Block->Index])
			{
				continue;
			}
			VisitedBlocks[Block->Index] = true;

			TOptional<TArray<FFailureContext*>>& OptionalFailureContexts = FailureContextsAtHead[Block->Index];
			V_DIE_UNLESS(OptionalFailureContexts);

			TArray<FFailureContext*> FailureContexts = OptionalFailureContexts.GetValue();

			for (uint32 InstOffset : Block->Bytecodes)
			{
				FOp* Op = Procedure.GetPCForOffset(InstOffset);

				auto AddInstToFailureContextMap = [&] {
					if (FailureContexts.Num())
					{
						CFG->BytecodeOffsetToFailureContext.Add(InstOffset, FailureContexts.Last());
					}
				};

				// BytecodeOffsetToFailureContext is exclusive of both BeginFailureContext and
				// EndFailureContext because neither of those opcodes can branch to FailurePC. Only things
				// inside the BeginFailureContext/EndFailureContext instruction range can.

				switch (Op->Opcode)
				{
					case EOpcode::BeginFailureContext:
					{
						AddInstToFailureContextMap();
						FOpBeginFailureContext* BeginOp = static_cast<FOpBeginFailureContext*>(Op);

						// We model the branches in this failure context to the "else" target. The failure context we enter
						// the BeginFailureContext opcode with is the same that it is at the "else".
						MergeInto(FailureContexts, &CFG->GetJumpTarget(Procedure.BytecodeOffset(BeginOp->OnFailure.GetLabeledPC())));

						FFailureContext* Parent = FailureContexts.Num() ? FailureContexts.Last() : nullptr;
						FailureContexts.Push(GetOrCreateFailureContext(BeginOp->Id, BeginOp->OnFailure.GetLabeledPC(), Parent));
						break;
					}
					case EOpcode::EndFailureContext:
					{
						FOpEndFailureContext* EndOp = static_cast<FOpEndFailureContext*>(Op);
						V_DIE_UNLESS(FailureContexts.Last()->Id == EndOp->Id);
						FailureContexts.Pop();

						AddInstToFailureContextMap();
						break;
					}
					case EOpcode::BeginTask:
					{
						AddInstToFailureContextMap();
						FOpBeginTask* BeginOp = static_cast<FOpBeginTask*>(Op);
						MergeInto(FailureContexts, &CFG->GetJumpTarget(Procedure.BytecodeOffset(BeginOp->OnYield.GetLabeledPC())));
						break;
					}
					default:
						AddInstToFailureContextMap();
						break;
				}
			}

			for (FBasicBlock* Successor : Block->Successors)
			{
				MergeInto(FailureContexts, Successor);
			}
		}
	}

	// Compute the mapping of bytecode offset to task. This is exclusive of BeginTask
	// but inclusive of EndTask because BeginTask can't branch to YieldPC but EndTask can.
	{
		TArray<FTask> Tasks;
		DispatchOps(Procedure, [&](FOp& Op) {
			if (Tasks.Num())
			{
				CFG->BytecodeOffsetToTask.Add(Procedure.BytecodeOffset(Op), Tasks.Last());
			}
			if (Op.Opcode == EOpcode::BeginTask)
			{
				FOpBeginTask& BeginOp = static_cast<FOpBeginTask&>(Op);
				Tasks.Push(FTask{BeginOp.OnYield.GetLabeledPC()});
			}
			if (Op.Opcode == EOpcode::EndTask)
			{
				Tasks.Pop();
			}
		});
	}

	return CFG;
}

FBasicBlock& FCFG::GetJumpTarget(uint32 BytecodeOffset)
{
	uint32 Index = Algo::LowerBound(Blocks, BytecodeOffset, [&](const TUniquePtr<FBasicBlock>& Block, uint32 Offset) {
		return Block->Last() < Offset;
	});
	FBasicBlock* Result = Blocks[Index].Get();
	V_DIE_UNLESS(Result->First() == BytecodeOffset);
	return *Result;
}

FFailureContext* FCFG::FindCurrentFailureContext(uint32 BytecodeOffset)
{
	if (FFailureContext** Result = BytecodeOffsetToFailureContext.Find(BytecodeOffset))
	{
		return *Result;
	}
	return nullptr;
}

FTask* FCFG::FindCurrentTask(uint32 BytecodeOffset)
{
	return BytecodeOffsetToTask.Find(BytecodeOffset);
}

template <typename FunctionType>
static void ForEachDef(VProcedure& Procedure, FOp* Op, FunctionType&& Function)
{
	Op->ForEachReg(Procedure, [&](EOperandRole Role, FRegisterIndex Register) {
		if (IsAnyDef(Role))
		{
			Function(Register);
		}
	});
}

template <typename FunctionType>
static void ForEachUse(VProcedure& Procedure, FOp* Op, FunctionType&& Function)
{
	Op->ForEachReg(Procedure, [&](EOperandRole Role, FRegisterIndex Register) {
		if (IsAnyUse(Role))
		{
			Function(Register);
		}
	});
}

FLiveness::FLocalCalc::FLocalCalc(FLiveness* Liveness, FBasicBlock* Block, VProcedure& Procedure)
	: Live(Liveness->LiveOut[Block->Index])
	, Liveness(Liveness)
	, Procedure(Procedure)
{
}

void FLiveness::FLocalCalc::Step(FOp* Op)
{
	// TODO SOL-7792: We also need an implicit edge to the defer for any suspends calls.

	ForEachDef(Procedure, Op, [&](FRegisterIndex Register) {
		Live[Register] = false;
	});
	ForEachUse(Procedure, Op, [&](FRegisterIndex Register) {
		Live[Register] = true;
	});

	// Everything live at the failure PC is live throughout the body of the failure context.
	if (FFailureContext* FailureContext = Liveness->CFG.FindCurrentFailureContext(Procedure.BytecodeOffset(Op)))
	{
		uint32 FailureBCOffset = Procedure.BytecodeOffset(FailureContext->FailurePC);
		FBasicBlock& FailureBlock = Liveness->CFG.GetJumpTarget(FailureBCOffset);
		Live.Union(Liveness->LiveIn[FailureBlock.Index]);
	}
	if (FTask* Task = Liveness->CFG.FindCurrentTask(Procedure.BytecodeOffset(Op)))
	{
		uint32 YieldBCOffset = Procedure.BytecodeOffset(Task->YieldPC);
		FBasicBlock& YieldBlock = Liveness->CFG.GetJumpTarget(YieldBCOffset);
		Live.Union(Liveness->LiveIn[YieldBlock.Index]);
	}
}

TUniquePtr<FLiveness> ComputeBytecodeLiveness(FCFG& CFG, VProcedure& Procedure)
{
	TUniquePtr<FLiveness> Result(new FLiveness{CFG});

	Result->LiveOut.Init(FLiveSet(Procedure.NumRegisters), CFG.NumBlocks());
	Result->LiveIn.Init(FLiveSet(Procedure.NumRegisters), CFG.NumBlocks());

	bool bChanged;
	do
	{
		bChanged = false;
		for (FBlockIndex BlockIndex = CFG.NumBlocks(); BlockIndex--;)
		{
			FBasicBlock* Block = CFG.Blocks[BlockIndex].Get();
			FLiveness::FLocalCalc LocalCalc(Result.Get(), Block, Procedure);
			for (uint32 InstOffset : ReverseIterate(Block->Bytecodes))
			{
				FOp* Op = Procedure.GetPCForOffset(InstOffset);
				LocalCalc.Step(Op);
			}

			if (LocalCalc.Live != Result->LiveIn[BlockIndex])
			{
				bChanged = true;
				Result->LiveIn[BlockIndex] = LocalCalc.Live;
			}

			for (FBasicBlock* Predecessor : Block->Predecessors)
			{
				bChanged |= Result->LiveOut[Predecessor->Index].Union(LocalCalc.Live);
			}
		}
	}
	while (bChanged);

	return Result;
}

struct FInterferenceGraph
{
	TArray<TSet<FRegisterIndex>> InterferenceEdges;

	FInterferenceGraph(VProcedure& Procedure)
	{
		InterferenceEdges.SetNum(Procedure.NumRegisters);
	}

	void AddEdge(FRegisterIndex A, FRegisterIndex B)
	{
		if (A != B)
		{
			InterferenceEdges[A.Index].Add(B);
			InterferenceEdges[B.Index].Add(A);
		}
	}
};

struct FRegisterAllocator
{
	VProcedure& Procedure;
	TUniquePtr<FCFG> CFG;
	TUniquePtr<FLiveness> Liveness;
	FInterferenceGraph InterferenceGraph;
	TArray<FRegisterIndex> RegisterAssignments;

	FRegisterAllocator(VProcedure& Procedure)
		: Procedure(Procedure)
		, CFG(MakeBytecodeCFG(Procedure))
		, Liveness(ComputeBytecodeLiveness(*CFG, Procedure))
		, InterferenceGraph(Procedure)
	{
	}

	// We allocate registers using a simple first fit allocator. We start by performing a liveness analysis and building
	// an interference graph between registers. Two registers interfere if they're live at the same time. If two registers
	// interfere, they can't be allocated to the same register. If they don't interfere, they can be allocated to the
	// same register.
	//
	// Once we have an interference graph, we walk each registers and assign it the lowest register that isn't used by
	// any of the registers it interferes with.
	void Allocate()
	{
		// TODO SOL-7793: Make this work with the debugger's register names.

		bool bUsesTasks = false;
		for (FBlockIndex BlockIndex = 0; BlockIndex < CFG->NumBlocks(); ++BlockIndex)
		{
			FBasicBlock* Block = CFG->Blocks[BlockIndex].Get();
			FLiveness::FLocalCalc LocalCalc(Liveness.Get(), Block, Procedure);
			for (uint32 InstOffset : ReverseIterate(Block->Bytecodes))
			{
				FOp* Op = Procedure.GetPCForOffset(InstOffset);

				if (Op->Opcode == EOpcode::BeginTask)
				{
					bUsesTasks = true;
				}

				ForEachDef(Procedure, Op, [&](FRegisterIndex Register) {
					LocalCalc.Live.ForEach([&](FRegisterIndex LiveRegister) {
						InterferenceGraph.AddEdge(Register, LiveRegister);
					});
				});

				LocalCalc.Step(Op);
			}
		}

		RegisterAssignments.SetNum(Procedure.NumRegisters);
		FRegisterIndex PinnedEnd = FRegisterIndex{FRegisterIndex::PARAMETER_START + Procedure.NumPositionalParameters + Procedure.NumNamedParameters};
		for (FRegisterIndex I = FRegisterIndex{0}; I < PinnedEnd; ++I)
		{
			RegisterAssignments[I.Index] = I;
		}

#if DO_GUARD_SLOW
		for (FRegisterIndex I = PinnedEnd; I < FRegisterIndex{Procedure.NumRegisters}; ++I)
		{
			V_DIE_IF(RegisterAssignments[I.Index]);
		}
#endif

		if (bUsesTasks)
		{
			TSet<FRegisterIndex> RegistersUsedInTasks;
			uint32 TaskCount = 0;
			DispatchOps(Procedure, [&](FOp& Op) {
				if (Op.Opcode == EOpcode::BeginTask)
				{
					++TaskCount;
				}
				if (TaskCount)
				{
					Op.ForEachReg(Procedure, [&](EOperandRole, FRegisterIndex Register) {
						if (Register >= PinnedEnd)
						{
							RegistersUsedInTasks.Add(Register);
						}
					});
				}
				if (Op.Opcode == EOpcode::EndTask)
				{
					V_DIE_UNLESS(TaskCount);
					--TaskCount;
				}
			});
			V_DIE_UNLESS(TaskCount == 0);

			FRegisterIndex NextToAssign = PinnedEnd;
			for (FRegisterIndex UsedInTask : RegistersUsedInTasks)
			{
				for (FRegisterIndex I = FRegisterIndex{0}; I < FRegisterIndex{Procedure.NumRegisters}; ++I)
				{
					InterferenceGraph.AddEdge(UsedInTask, I);
				}
				RegisterAssignments[UsedInTask.Index] = NextToAssign;
				++NextToAssign;
			}
		}

		for (FRegisterIndex ToAssign = PinnedEnd; ToAssign < FRegisterIndex{Procedure.NumRegisters}; ++ToAssign)
		{
			if (RegisterAssignments[ToAssign.Index])
			{
				continue;
			}
			TSet<FRegisterIndex> Disallowed;
			for (FRegisterIndex Interference : InterferenceGraph.InterferenceEdges[ToAssign.Index])
			{
				if (RegisterAssignments[Interference.Index])
				{
					Disallowed.Add(RegisterAssignments[Interference.Index]);
				}
			}

			for (FRegisterIndex J = PinnedEnd; true; ++J)
			{
				if (!Disallowed.Contains(J))
				{
					RegisterAssignments[ToAssign.Index] = J;
					break;
				}
			}
		}

		uint32 MaxRegister = 0;
		for (uint32 I = 0; I < Procedure.NumRegisters; ++I)
		{
			MaxRegister = std::max(RegisterAssignments[I].Index, MaxRegister);
		}

		if (false)
		{
			UE_LOG(LogVerseVM, Display, TEXT("OldSize: %u NewSize: %u"), Procedure.NumRegisters, MaxRegister + 1);
			if (true)
			{
				UE_LOG(LogVerseVM, Display, TEXT("Allocation:"));
				for (uint32 I = 0; I < Procedure.NumRegisters; ++I)
				{
					UE_LOG(LogVerseVM, Display, TEXT("\tr%u->r%u"), I, RegisterAssignments[I].Index);
				}
			}
		}

		Procedure.NumRegisters = MaxRegister + 1;

		DispatchOps(Procedure, [&](FOp& Op) {
			Op.ForEachReg(Procedure, [&](EOperandRole, FRegisterIndex& Register) {
				Register = RegisterAssignments[Register.Index];
			});
		});
	}
};

void AllocateRegisters(VProcedure& Procedure)
{
	FRegisterAllocator Allocator(Procedure);
	Allocator.Allocate();
}

} // namespace BytecodeAnalysis

} // namespace Verse

#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
