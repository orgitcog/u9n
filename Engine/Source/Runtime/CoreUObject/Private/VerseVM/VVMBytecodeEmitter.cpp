// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMBytecodeEmitter.h"
#include "VerseVM/VVMBytecodeAnalysis.h"
#include "VerseVM/VVMCVars.h"
#include "VerseVM/VVMDebugger.h"
#include "VerseVM/VVMProcedure.h"
#include "VerseVM/VVMValue.h"
#include <string.h>

Verse::VProcedure& Verse::FOpEmitter::MakeProcedure(FAllocationContext Context)
{
	VProcedure& Procedure = VProcedure::NewUninitialized(
		Context,
		NumNamedParameters,
		Constants.Num(),
		OpBytes.Num(),
		Operands.Num(),
		Labels.Num(),
		UnwindEdges.Num(),
		OpLocations.Num(),
		RegisterNames.Num());

	Procedure.FilePath.Set(Context, *FilePath);
	Procedure.Name.Set(Context, *ProcedureName);

	Procedure.NumRegisters = NumRegisters;
	Procedure.NumPositionalParameters = NumPositionalParameters;

	FNamedParam* ProcNamedParams = Procedure.GetNamedParamsBegin();
	for (int32 I = 0; I < NamedParameters.Num(); ++I)
	{
		ProcNamedParams[I] = NamedParameters[I];
	}

	for (int32 I = 0; I < Constants.Num(); ++I)
	{
		Procedure.SetConstant(Context, FConstantIndex{uint32(I)}, Constants[I]);
	}

	check(OpBytes.Num());
	memcpy(Procedure.GetOpsBegin(), OpBytes.GetData(), OpBytes.Num());

	FValueOperand* ProcOperands = Procedure.GetOperandsBegin();
	for (int32 I = 0; I < Operands.Num(); ++I)
	{
		ProcOperands[I] = Operands[I];
	}

	FLabelOffset* ProcLabels = Procedure.GetLabelsBegin();
	for (int32 I = 0; I < Labels.Num(); ++I)
	{
		ProcLabels[I] = CoerceOperand(Labels[I]);
	}

	FUnwindEdge* ProcUnwindEdges = Procedure.GetUnwindEdgesBegin();
	for (int32 I = 0; I < UnwindEdges.Num(); ++I)
	{
		ProcUnwindEdges[I] = UnwindEdges[I];
	}

	for (auto I = Procedure.GetOpLocationsBegin(), J = OpLocations.GetData(), Last = J + OpLocations.Num(); J != Last; ++I, ++J)
	{
		new (I) FOpLocation(*J);
	}

	if (FDebugger* Debugger = GetDebugger())
	{
		for (auto I = Procedure.GetOpLocationsBegin(), Last = Procedure.GetOpLocationsEnd(); I != Last; ++I)
		{
			Debugger->AddLocation(Context, *FilePath, I->Location);
		}
	}

	for (auto I = Procedure.GetRegisterNamesBegin(), J = RegisterNames.GetData(), Last = J + RegisterNames.Num(); J != Last; ++I, ++J)
	{
		new (I) FRegisterName(*J);
	}

	// Fixup label indices to label offsets.
	auto FixupLabel = [&](FLabelOffset& LabelOffset, uint32 LabelOffsetOffset) {
		const int32 LabelIndex = LabelOffset.Offset;
		check(LabelIndex >= 0 && LabelIndex < LabelOffsets.Num());

		const uint32 TargetLabelOffset = LabelOffsets[LabelIndex];
		checkf(TargetLabelOffset != UINT32_MAX, TEXT("Label was emitted but not bound"));

		const int64 RelativeOffset = static_cast<int64>(TargetLabelOffset) - LabelOffsetOffset;
		check(RelativeOffset >= INT32_MIN && RelativeOffset <= INT32_MAX);
		LabelOffset.Offset = static_cast<int32>(RelativeOffset);
	};
	for (uint32 LabelOffsetOffset : LabelOffsetOffsets)
	{
		check(static_cast<size_t>(LabelOffsetOffset) < static_cast<size_t>(OpBytes.Num()));
		FLabelOffset& LabelOffset = *BitCast<FLabelOffset*>(BitCast<uint8*>(Procedure.GetOpsBegin()) + LabelOffsetOffset);
		FixupLabel(LabelOffset, LabelOffsetOffset);
	}
	for (FLabelOffset* LabelOffset = Procedure.GetLabelsBegin(); LabelOffset != Procedure.GetLabelsEnd(); ++LabelOffset)
	{
		uint32 LabelOffsetOffset = BitCast<uint8*>(LabelOffset) - BitCast<uint8*>(Procedure.GetOpsBegin());
		FixupLabel(*LabelOffset, LabelOffsetOffset);
	}
	for (FUnwindEdge* UnwindEdge = Procedure.GetUnwindEdgesBegin(); UnwindEdge != Procedure.GetUnwindEdgesEnd(); ++UnwindEdge)
	{
		uint32 LabelOffsetOffset = BitCast<uint8*>(&UnwindEdge->OnUnwind) - BitCast<uint8*>(Procedure.GetOpsBegin());
		FixupLabel(UnwindEdge->OnUnwind, LabelOffsetOffset);
	}

	if (bEnableRegisterAllocation && CVarDoBytecodeRegisterAllocation.GetValueOnAnyThread())
	{
		BytecodeAnalysis::AllocateRegisters(Procedure);
	}

	return Procedure;
}

#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
