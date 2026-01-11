// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpConstantScalar.h"

#include "HAL/PlatformMath.h"
#include "MuR/ModelPrivate.h"
#include "MuR/RefCounted.h"
#include "MuR/Types.h"


namespace mu
{

	ASTOpConstantScalar::ASTOpConstantScalar(float InValue)
	{
		Value = InValue;
	}


	void ASTOpConstantScalar::ForEachChild(const TFunctionRef<void(ASTChild&)>)
	{
	}


	bool ASTOpConstantScalar::IsEqual(const ASTOp& OtherUntyped) const
	{
		if (OtherUntyped.GetOpType() == GetOpType())
		{
			const ASTOpConstantScalar* other = static_cast<const ASTOpConstantScalar*>(&OtherUntyped);
			return Value == other->Value;
		}
		return false;
	}


	uint64 ASTOpConstantScalar::Hash() const
	{
		uint64 Result = std::hash<uint64>()(uint64(GetOpType()));
		hash_combine(Result, Value);
		return Result;
	}


	mu::Ptr<ASTOp> ASTOpConstantScalar::Clone(MapChildFuncRef) const
	{
		Ptr<ASTOpConstantScalar> New = new ASTOpConstantScalar();
		New->Value = Value;
		return New;
	}


	void ASTOpConstantScalar::Link(FProgram& Program, FLinkerOptions*)
	{
		if (!linkedAddress)
		{
			OP::ScalarConstantArgs Args;
			FMemory::Memzero(Args);
			Args.Value = Value;

			linkedAddress = (OP::ADDRESS)Program.OpAddress.Num();
			Program.OpAddress.Add(Program.ByteCode.Num());
			AppendCode(Program.ByteCode, GetOpType());
			AppendCode(Program.ByteCode, Args);
		}
	}


	int32 ASTOpConstantScalar::EvaluateInt(ASTOpList&, bool& bOutUnknown) const
	{
		bOutUnknown =  false;
		return Value;
	}

}
