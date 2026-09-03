// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpConstantExtensionData.h"

#include "HAL/UnrealMemory.h"
#include "MuR/ModelPrivate.h"


namespace mu
{


//-------------------------------------------------------------------------------------------------
bool ASTOpConstantExtensionData::IsEqual(const ASTOp& OtherUntyped) const
{
	if (OtherUntyped.GetOpType() == GetOpType())
	{
		const ASTOpConstantExtensionData* Other = static_cast<const ASTOpConstantExtensionData*>(&OtherUntyped);
		return Other->Value == Value;
	}

	return false;
}


//-------------------------------------------------------------------------------------------------
mu::Ptr<ASTOp> ASTOpConstantExtensionData::Clone(MapChildFuncRef MapChild) const
{
	Ptr<ASTOpConstantExtensionData> Result = new ASTOpConstantExtensionData();

	Result->Value = Value;

	return Result;
}


//-------------------------------------------------------------------------------------------------
uint64 ASTOpConstantExtensionData::Hash() const
{
	return std::hash<uint64>()(Value != nullptr ? Value->Hash() : 0ll);
}


//-------------------------------------------------------------------------------------------------
void ASTOpConstantExtensionData::Link(FProgram& Program, FLinkerOptions* Options)
{
	if (!linkedAddress)
	{
		OP::ResourceConstantArgs Args;
		FMemory::Memzero(Args);

		Args.value = Program.AddConstant(Value);

		linkedAddress = (OP::ADDRESS)Program.OpAddress.Num();
		Program.OpAddress.Add((uint32_t)Program.ByteCode.Num());
		AppendCode(Program.ByteCode, EOpType::ED_CONSTANT);
		AppendCode(Program.ByteCode, Args);
	}
}

}