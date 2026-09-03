// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "Containers/BitArray.h"
#include "Templates/UniquePtr.h"
#include "VerseVM/VVMProcedure.h"

namespace Verse
{

namespace BytecodeAnalysis
{

using FBlockIndex = uint32;

// A basic block is a list of bytecode instructions with a set of predecessors and successors.
struct FBasicBlock
{
	FBlockIndex Index{std::numeric_limits<uint32>::max()};

	// These First/Last bytecodes are inclusive.
	uint32 First() const { return Bytecodes[0]; }
	uint32 Last() const { return Bytecodes.Last(); }

	bool Contains(uint32 BytecodeOffset) const { return First() <= BytecodeOffset && BytecodeOffset <= Last(); }

	// For now, we store these just as offsets from the procedures start bytecode to make it easy to walk backwards.
	// In the future we could compact this by storing it as offsets between instructions.
	TArray<uint32> Bytecodes;

	TArray<FBasicBlock*> Predecessors;
	TArray<FBasicBlock*> Successors;
};

struct FFailureContext
{
	FFailureContext(FFailureContextId Id, FOp* FailurePC, FFailureContext* Parent)
		: Id(Id)
		, FailurePC(FailurePC)
		, Parent(Parent)
	{
	}
	FFailureContextId Id;
	FOp* FailurePC;
	FFailureContext* Parent;
};

struct FTask
{
	FTask(FOp* YieldPC)
		: YieldPC(YieldPC)
	{
	}
	FOp* YieldPC;
};

// A CFG is composed of a list of basic blocks and some metadata to tell us about which instructions
// belong in which tasks and/or failure contexts.
struct FCFG
{
	TArray<TUniquePtr<FBasicBlock>> Blocks;
	TArray<TUniquePtr<FFailureContext>> FailureContexts;

	// Maps bytecode offset to innermost failure context. This is exclusive of both
	// BeginFailureContext and EndFailureContext because those instructions can't jump
	// to the FailurePC.
	TMap<uint32, FFailureContext*> BytecodeOffsetToFailureContext;
	// Maps bytecode offset to innermost task. This is exclusive of BeginTask but inclusive of EndTask.
	// The reason for this is BeginTask won't branch to YieldPC, but any bytecode within the range, including
	// EndTask, might.
	TMap<uint32, FTask> BytecodeOffsetToTask;

	FBasicBlock& GetJumpTarget(uint32 BytecodeOffset);
	// Returns the innermost failure context we're in, if we're in one.
	FFailureContext* FindCurrentFailureContext(uint32 BytecodeOffset);
	// Returns the innermost task we're in, if we're in one.
	FTask* FindCurrentTask(uint32 BytecodeOffset);

	uint32 NumBlocks() const
	{
		return Blocks.Num();
	}
};

using FBitArray = TBitArray<>;

// Bitset used to represent the live set of registers at a point in the program.
struct FLiveSet
{
	FLiveSet(uint32 Num)
	{
		Live.Init(false, Num);
	}
	FLiveSet(const FLiveSet&) = default;

	// Returns true if this changed.
	bool Union(const FLiveSet& Other)
	{
		V_DIE_UNLESS(Other.Live.Num() == Live.Num());
		FBitArray NewLive = Live;
		NewLive.CombineWithBitwiseOR(Other.Live, EBitwiseOperatorFlags::MaintainSize);
		if (NewLive != Live)
		{
			Live = NewLive;
			return true;
		}
		return false;
	}

	bool operator==(const FLiveSet& Other) const
	{
		return Live == Other.Live;
	}
	bool operator!=(const FLiveSet& Other) const
	{
		return !(*this == Other);
	}

	template <typename FunctionType>
	void ForEach(FunctionType&& Function) const
	{
		for (TConstSetBitIterator<> Iter(Live); Iter; ++Iter)
		{
			Function(FRegisterIndex{static_cast<uint32>(Iter.GetIndex())});
		}
	}

	FBitReference operator[](FRegisterIndex Register)
	{
		return Live[Register.Index];
	}

private:
	FBitArray Live;
};

// This data structure caches liveness at basic block boundaries.
// To get the liveness in at an individual instruction, use FLocalCalc
// and walk the basic block backwards.
struct FLiveness
{
	struct FLocalCalc
	{
		FLocalCalc(FLiveness*, FBasicBlock*, VProcedure&);
		void Step(FOp*);
		FLiveSet Live;
		FLiveness* Liveness;
		VProcedure& Procedure;
	};

	FCFG& CFG;
	TArray<FLiveSet> LiveOut;
	TArray<FLiveSet> LiveIn;
};

COREUOBJECT_API TUniquePtr<FCFG> MakeBytecodeCFG(VProcedure&);
COREUOBJECT_API TUniquePtr<FLiveness> ComputeBytecodeLiveness(FCFG&, VProcedure&);
void AllocateRegisters(VProcedure&);

} // namespace BytecodeAnalysis

} // namespace Verse

#endif
