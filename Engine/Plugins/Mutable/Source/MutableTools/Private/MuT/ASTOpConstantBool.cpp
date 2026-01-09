// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpConstantBool.h"

#include "HAL/PlatformMath.h"
#include "MuR/ModelPrivate.h"
#include "MuR/RefCounted.h"
#include "MuR/Types.h"


namespace mu
{

	ASTOpConstantBool::ASTOpConstantBool(bool InValue)
	{
		bValue = InValue;
	}


	void ASTOpConstantBool::ForEachChild(const TFunctionRef<void(ASTChild&)>)
	{
	}


	bool ASTOpConstantBool::IsEqual(const ASTOp& OtherUntyped) const
	{
		if (OtherUntyped.GetOpType() == GetOpType())
		{
			const ASTOpConstantBool* other = static_cast<const ASTOpConstantBool*>(&OtherUntyped);
			return bValue == other->bValue;
		}
		return false;
	}


	uint64 ASTOpConstantBool::Hash() const
	{
		uint64 Result = std::hash<uint64>()(uint64(EOpType::BO_CONSTANT));
		hash_combine(Result, bValue);
		return Result;
	}


	mu::Ptr<ASTOp> ASTOpConstantBool::Clone(MapChildFuncRef) const
	{
		Ptr<ASTOpConstantBool> New = new ASTOpConstantBool();
		New->bValue = bValue;
		return New;
	}


	void ASTOpConstantBool::Link(FProgram& Program, FLinkerOptions*)
	{
		if (!linkedAddress)
		{
			OP::BoolConstantArgs Args;
			FMemory::Memzero(Args);
			Args.bValue = bValue;

			linkedAddress = (OP::ADDRESS)Program.OpAddress.Num();
			Program.OpAddress.Add(Program.ByteCode.Num());
			AppendCode(Program.ByteCode, GetOpType());
			AppendCode(Program.ByteCode, Args);
		}
	}


	ASTOp::FBoolEvalResult ASTOpConstantBool::EvaluateBool(ASTOpList&, FEvaluateBoolCache*) const
	{
		return bValue ? BET_TRUE : BET_FALSE;
	}

}
