// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpConstantProjector.h"

#include "HAL/PlatformMath.h"
#include "MuR/ModelPrivate.h"
#include "MuR/RefCounted.h"


namespace mu
{


	//-------------------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------------------
	void ASTOpConstantProjector::ForEachChild(const TFunctionRef<void(ASTChild&)>)
	{
	}


	uint64 ASTOpConstantProjector::Hash() const
	{
		uint64 res = std::hash<float>()(value.position[0]);
		hash_combine(res, value.direction[0]);
		return res;
	}


	bool ASTOpConstantProjector::IsEqual(const ASTOp& otherUntyped) const
	{
		if (otherUntyped.GetOpType() == GetOpType())
		{
			const ASTOpConstantProjector* other = static_cast<const ASTOpConstantProjector*>(&otherUntyped);
			return value == other->value;
		}
		return false;
	}


	mu::Ptr<ASTOp> ASTOpConstantProjector::Clone(MapChildFuncRef) const
	{
		Ptr<ASTOpConstantProjector> n = new ASTOpConstantProjector();
		n->value = value;
		return n;
	}


	void ASTOpConstantProjector::Link(FProgram& program, FLinkerOptions*)
	{
		if (!linkedAddress)
		{
			OP::ResourceConstantArgs Args;
			FMemory::Memzero(Args);
			Args.value = program.AddConstant(value);

			linkedAddress = (OP::ADDRESS)program.OpAddress.Num();
			//program.m_code.push_back(op);
			program.OpAddress.Add((uint32_t)program.ByteCode.Num());
			AppendCode(program.ByteCode, EOpType::PR_CONSTANT);
			AppendCode(program.ByteCode, Args);
		}
	}

}
