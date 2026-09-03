// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpScalarArithmetic.h"
#include "MuT/ASTOpMeshAddTags.h"

#include "HAL/PlatformMath.h"
#include "MuR/ModelPrivate.h"
#include "MuR/RefCounted.h"
#include "MuR/Types.h"

namespace mu
{

	ASTOpScalarArithmetic::ASTOpScalarArithmetic()
		: A(this)
		, B(this)
	{
	}


	ASTOpScalarArithmetic::~ASTOpScalarArithmetic()
	{
		// Explicit call needed to avoid recursive destruction
		ASTOp::RemoveChildren();
	}


	bool ASTOpScalarArithmetic::IsEqual(const ASTOp& OtherUntyped) const
	{
		if (OtherUntyped.GetOpType()==GetOpType())
		{
			const ASTOpScalarArithmetic* Other = static_cast<const ASTOpScalarArithmetic*>(&OtherUntyped);
			return A == Other->A &&
				B == Other->B &&
				Operation == Other->Operation;
		}
		return false;
	}


	uint64 ASTOpScalarArithmetic::Hash() const
	{
		uint64 Result = std::hash<void*>()(A.child().get());
		hash_combine(Result, B.child().get());
		hash_combine(Result, Operation);
		return Result;
	}


	Ptr<ASTOp> ASTOpScalarArithmetic::Clone(MapChildFuncRef MapChild) const
	{
		Ptr<ASTOpScalarArithmetic> New = new ASTOpScalarArithmetic();
		New->A = MapChild(A.child());
		New->B = MapChild(B.child());
		New->Operation = Operation;
		return New;
	}


	void ASTOpScalarArithmetic::ForEachChild(const TFunctionRef<void(ASTChild&)> Func)
	{
		Func(A);
		Func(B);
	}


	void ASTOpScalarArithmetic::Link(FProgram& Program, FLinkerOptions*)
	{
		// Already linked?
		if (!linkedAddress)
		{
			OP::ArithmeticArgs Args;
			FMemory::Memzero(Args);

			if (A) Args.A = A->linkedAddress;
			if (B) Args.B = B->linkedAddress;
			Args.Operation = Operation;

			linkedAddress = (OP::ADDRESS)Program.OpAddress.Num();
			Program.OpAddress.Add(Program.ByteCode.Num());
			AppendCode(Program.ByteCode, GetOpType());
			AppendCode(Program.ByteCode, Args);
		}

	}
}
