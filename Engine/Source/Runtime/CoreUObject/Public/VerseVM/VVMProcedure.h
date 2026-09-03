// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "Misc/StructBuilder.h"
#include "Templates/TypeCompatibleBytes.h"
#include "VVMBytecode.h"
#include "VVMGlobalTrivialEmergentTypePtr.h"
#include "VVMType.h"
#include "VVMUniqueString.h"
#include "VVMWriteBarrier.h"

namespace Verse
{
/*
This is laid out in memory with (64-bit) pointers followed by (8-byte aligned) instructions followed by (32-bit) integers:
VProcedure
FNamedParam                    NamedParam[0]
FNamedParam                    NamedParam[1]
...
FNamedParam                    NamedParam[NumNamedParameters - 1]
TWriteBarrier<VValue>          Constant  [0]
TWriteBarrier<VValue>          Constant  [1]
...
TWriteBarrier<VValue>          Constant  [NumConstants - 1]
FOp                            Ops
  + NumOpBytes
FValueOperand                  Operand   [0]
FValueOperand                  Operand   [1]
...
FValueOperand                  Operand   [NumOperands - 1]
FLabelOffset                   Label     [0]
FLabelOffset                   Label     [1]
...
FLabelOffset                   Label     [NumLabels - 1]
FUnwindEdge                    UnwindEdge[0]
FUnwindEdge                    UnwindEdge[1]
...
FUnwindEdge                    UnwindEdge[NumUnwindEdges - 1]
FOpLocation                    OpLocation[0]
FOpLocation                    OpLocation[1]
...
FOpLocation                    OpLocation[NumOpLocations - 1]
FRegisterName                  RegisterName[0]
FRegisterName                  RegisterName[1]
...
FRegisterName                  RegisterName[NumRegisterNames - 1]
*/
struct VProcedure : VCell
{
	DECLARE_DERIVED_VCPPCLASSINFO(COREUOBJECT_API, VCell);
	COREUOBJECT_API static TGlobalTrivialEmergentTypePtr<&StaticCppClassInfo> GlobalTrivialEmergentType;

	// Used by the debugger when checking breakpoints
	TWriteBarrier<VUniqueString> FilePath;
	// Used by the debugger when showing stack frames
	TWriteBarrier<VUniqueString> Name;

	uint32 NumRegisters;
	uint32 NumPositionalParameters;

	// Sizes of trailing arrays
	uint32 NumNamedParameters;
	uint32 NumConstants;
	uint32 NumOpBytes;
	uint32 NumOperands;
	uint32 NumLabels;
	uint32 NumUnwindEdges;
	uint32 NumOpLocations;
	uint32 NumRegisterNames;

	TWriteBarrier<VCell> Trailing[];

	// Trailing array layout computation

	FNamedParam* GetNamedParamsBegin() { return BitCast<FNamedParam*>(BitCast<std::byte*>(this) + GetLayout().NamedParamsOffset); }
	FNamedParam* GetNamedParamsEnd() { return GetNamedParamsBegin() + NumNamedParameters; }

	TWriteBarrier<VValue>* GetConstantsBegin() { return BitCast<TWriteBarrier<VValue>*>(BitCast<std::byte*>(this) + GetLayout().ConstantsOffset); }
	TWriteBarrier<VValue>* GetConstantsEnd() { return GetConstantsBegin() + NumConstants; }

	FOp* GetPCForOffset(uint32 Offset) { return BitCast<FOp*>(BitCast<std::byte*>(this) + GetLayout().OpsOffset + Offset); }
	FOp* GetOpsBegin() { return GetPCForOffset(0); }
	FOp* GetOpsEnd() { return GetPCForOffset(NumOpBytes); }
	bool ContainsPC(FOp* PC)
	{
		return uintptr_t(BitCast<std::byte*>(PC) - BitCast<std::byte*>(this)) - uintptr_t(GetLayout().OpsOffset) < uintptr_t(NumOpBytes);
	}

	FValueOperand* GetOperandsBegin() { return BitCast<FValueOperand*>(BitCast<std::byte*>(this) + GetLayout().OperandsOffset); }
	FValueOperand* GetOperandsEnd() { return GetOperandsBegin() + NumOperands; }

	FLabelOffset* GetLabelsBegin() { return BitCast<FLabelOffset*>(BitCast<std::byte*>(this) + GetLayout().LabelsOffset); }
	FLabelOffset* GetLabelsEnd() { return GetLabelsBegin() + NumLabels; }

	FUnwindEdge* GetUnwindEdgesBegin() { return BitCast<FUnwindEdge*>(BitCast<std::byte*>(this) + GetLayout().UnwindEdgesOffset); }
	FUnwindEdge* GetUnwindEdgesEnd() { return GetUnwindEdgesBegin() + NumUnwindEdges; }

	FOpLocation* GetOpLocationsBegin() { return BitCast<FOpLocation*>(BitCast<std::byte*>(this) + GetLayout().OpLocationsOffset); }
	FOpLocation* GetOpLocationsEnd() { return GetOpLocationsBegin() + NumOpLocations; }

	FRegisterName* GetRegisterNamesBegin() { return BitCast<FRegisterName*>(BitCast<std::byte*>(this) + GetLayout().RegisterNamesOffset); }
	FRegisterName* GetRegisterNamesEnd() { return GetRegisterNamesBegin() + NumRegisterNames; }

	// In bytes.
	uint32 BytecodeOffset(const FOp& Bytecode)
	{
		return BytecodeOffset(&Bytecode);
	}

	uint32 BytecodeOffset(const void* Data)
	{
		checkSlow(GetOpsBegin() <= Data && Data < GetOpsEnd());
		return static_cast<uint32>(BitCast<char*>(Data) - BitCast<char*>(GetOpsBegin()));
	}

	const FLocation* GetLocation(const FOp& Op)
	{
		return GetLocation(BytecodeOffset(Op));
	}

	const FLocation* GetLocation(int32 OpOffset)
	{
		return Verse::GetLocation(GetOpLocationsBegin(), GetOpLocationsEnd(), OpOffset);
	}

	void SetConstant(FAllocationContext Context, FConstantIndex ConstantIndex, VValue Value)
	{
		checkSlow(ConstantIndex.Index < NumConstants);
		GetConstantsBegin()[ConstantIndex.Index].Set(Context, Value);
	}

	VValue GetConstant(FConstantIndex ConstantIndex)
	{
		checkSlow(ConstantIndex.Index < NumConstants);
		return GetConstantsBegin()[ConstantIndex.Index].Get();
	}

	static VProcedure& NewUninitialized(
		FAllocationContext Context,
		uint32 NumNamedParameters,
		uint32 NumConstants,
		uint32 NumOpBytes,
		uint32 NumOperands,
		uint32 NumLabels,
		uint32 NumUnwindEdges,
		uint32 NumOpLocations,
		uint32 NumRegisterNames)
	{
		FLayout Layout = CalcLayout(NumNamedParameters, NumConstants, NumOpBytes, NumOperands, NumLabels, NumUnwindEdges, NumOpLocations, NumRegisterNames);

		return *new (Context.Allocate(FHeap::CensusSpace, Layout.TotalSize)) VProcedure(
			Context,
			NumNamedParameters,
			NumConstants,
			NumOpBytes,
			NumOperands,
			NumLabels,
			NumUnwindEdges,
			NumOpLocations,
			NumRegisterNames);
	}

	void ConductCensusImpl();

	static void SerializeLayout(FAllocationContext Context, VProcedure*& This, FStructuredArchiveVisitor& Visitor);
	void SerializeImpl(FAllocationContext Context, FStructuredArchiveVisitor& Visitor);

private:
	TArray<uint8> SanitizeOpCodes();

	struct FLayout
	{
		int32 NamedParamsOffset;
		int32 ConstantsOffset;
		int32 OpsOffset;
		int32 OperandsOffset;
		int32 LabelsOffset;
		int32 UnwindEdgesOffset;
		int32 OpLocationsOffset;
		int32 RegisterNamesOffset;
		int32 TotalSize;
	};

	static FLayout CalcLayout(
		uint32 NumNamedParameters,
		uint32 NumConstants,
		uint32 NumOpBytes,
		uint32 NumOperands,
		uint32 NumLabels,
		uint32 NumUnwindEdges,
		uint32 NumOpLocations,
		uint32 NumRegisterNames)
	{
		FStructBuilder StructBuilder;
		StructBuilder.AddMember(offsetof(VProcedure, Trailing), alignof(VProcedure));

		FLayout Layout;
		Layout.NamedParamsOffset = StructBuilder.AddMember(sizeof(FNamedParam) * NumNamedParameters, alignof(FNamedParam));
		Layout.ConstantsOffset = StructBuilder.AddMember(sizeof(TWriteBarrier<VValue>) * NumConstants, alignof(TWriteBarrier<VValue>));
		Layout.OpsOffset = StructBuilder.AddMember(NumOpBytes, OpAlignment);
		Layout.OperandsOffset = StructBuilder.AddMember(sizeof(FValueOperand) * NumOperands, alignof(FValueOperand));
		Layout.LabelsOffset = StructBuilder.AddMember(sizeof(FLabelOffset) * NumLabels, alignof(FLabelOffset));
		Layout.UnwindEdgesOffset = StructBuilder.AddMember(sizeof(FUnwindEdge) * NumUnwindEdges, alignof(FUnwindEdge));
		Layout.OpLocationsOffset = StructBuilder.AddMember(sizeof(FOpLocation) * NumOpLocations, alignof(FOpLocation));
		Layout.RegisterNamesOffset = StructBuilder.AddMember(sizeof(FRegisterName) * NumRegisterNames, alignof(FRegisterName));
		Layout.TotalSize = StructBuilder.GetSize();

		return Layout;
	}

	FLayout GetLayout() const
	{
		return CalcLayout(NumNamedParameters, NumConstants, NumOpBytes, NumOperands, NumLabels, NumUnwindEdges, NumOpLocations, NumRegisterNames);
	}

	VProcedure(
		FAllocationContext Context,
		uint32 InNumNamedParameters,
		uint32 InNumConstants,
		uint32 InNumOpBytes,
		uint32 InNumOperands,
		uint32 InNumLabels,
		uint32 InNumUnwindEdges,
		uint32 InNumOpLocations,
		uint32 InNumRegisterNames)
		: VCell(Context, &GlobalTrivialEmergentType.Get(Context))
		, NumNamedParameters(InNumNamedParameters)
		, NumConstants(InNumConstants)
		, NumOpBytes(InNumOpBytes)
		, NumOperands(InNumOperands)
		, NumLabels(InNumLabels)
		, NumUnwindEdges(InNumUnwindEdges)
		, NumOpLocations(InNumOpLocations)
		, NumRegisterNames(InNumRegisterNames)
	{
		for (FNamedParam* NamedParam = GetNamedParamsBegin(); NamedParam != GetNamedParamsEnd(); ++NamedParam)
		{
			new (NamedParam) FNamedParam{};
		}
		for (TWriteBarrier<VValue>* Constant = GetConstantsBegin(); Constant != GetConstantsEnd(); ++Constant)
		{
			new (Constant) TWriteBarrier<VValue>{};
		}
		for (FValueOperand* Operand = GetOperandsBegin(); Operand != GetOperandsEnd(); ++Operand)
		{
			new (Operand) FValueOperand{};
		}
		for (FLabelOffset* Label = GetLabelsBegin(); Label != GetLabelsEnd(); ++Label)
		{
			new (Label) FLabelOffset{};
		}
		for (FUnwindEdge* UnwindEdge = GetUnwindEdgesBegin(); UnwindEdge != GetUnwindEdgesEnd(); ++UnwindEdge)
		{
			new (UnwindEdge) FUnwindEdge{};
		}
		for (FOpLocation* OpLocation = GetOpLocationsBegin(); OpLocation != GetOpLocationsEnd(); ++OpLocation)
		{
			new (OpLocation) FOpLocation{};
		}
		for (FRegisterName* RegisterName = GetRegisterNamesBegin(); RegisterName != GetRegisterNamesEnd(); ++RegisterName)
		{
			new (RegisterName) FRegisterName{};
		}
	}

	template <typename FuncType>
	void ForEachOpCode(FuncType&& Func);
};
} // namespace Verse
#endif // WITH_VERSE_VM
