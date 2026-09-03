// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpConstantInt.h"

#include "HAL/PlatformMath.h"
#include "MuR/ModelPrivate.h"
#include "MuR/RefCounted.h"
#include "MuR/Types.h"


namespace mu
{

	ASTOpConstantInt::ASTOpConstantInt(int32 InValue)
	{
		Value = InValue;
	}


	void ASTOpConstantInt::ForEachChild(const TFunctionRef<void(ASTChild&)>)
	{
	}


	bool ASTOpConstantInt::IsEqual(const ASTOp& OtherUntyped) const
	{
		if (OtherUntyped.GetOpType() == GetOpType())
		{
			const ASTOpConstantInt* Other = static_cast<const ASTOpConstantInt*>(&OtherUntyped);
			return Value == Other->Value;
		}
		return false;
	}


	uint64 ASTOpConstantInt::Hash() const
	{
		uint64 Result = std::hash<uint64>()(uint64(GetOpType()));
		hash_combine(Result, Value);
		return Result;
	}


	mu::Ptr<ASTOp> ASTOpConstantInt::Clone(MapChildFuncRef) const
	{
		Ptr<ASTOpConstantInt> New = new ASTOpConstantInt();
		New->Value = Value;
		return New;
	}


	void ASTOpConstantInt::Link(FProgram& Program, FLinkerOptions*)
	{
		if (!linkedAddress)
		{
			OP::IntConstantArgs Args;
			FMemory::Memzero(Args);
			Args.Value = Value;

			linkedAddress = (OP::ADDRESS)Program.OpAddress.Num();
			Program.OpAddress.Add(Program.ByteCode.Num());
			AppendCode(Program.ByteCode, GetOpType());
			AppendCode(Program.ByteCode, Args);
		}
	}


	int32 ASTOpConstantInt::EvaluateInt(ASTOpList&, bool& bOutUnknown) const
	{
		bOutUnknown =  false;
		return Value;
	}

}
