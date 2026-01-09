// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpParameter.h"

#include "MuT/ASTOpBoolEqualIntConst.h"
#include "HAL/PlatformMath.h"
#include "Misc/AssertionMacros.h"
#include "MuR/ModelPrivate.h"
#include "MuR/Parameters.h"
#include "MuR/RefCounted.h"
#include "MuR/Types.h"


namespace mu
{


	ASTOpParameter::~ASTOpParameter()
	{
		// Explicit call needed to avoid recursive destruction
		ASTOp::RemoveChildren();
	}


	void ASTOpParameter::ForEachChild(const TFunctionRef<void(ASTChild&)> f)
	{
		for (FRangeData& c : Ranges)
		{
			f(c.rangeSize);
		}
	}


	bool ASTOpParameter::IsEqual(const ASTOp& OtherUntyped) const
	{
		if (OtherUntyped.GetOpType() == GetOpType())
		{
			const ASTOpParameter* Other = static_cast<const ASTOpParameter*>(&OtherUntyped);
			return Type == Other->Type &&
				Parameter == Other->Parameter &&
				LODIndex == Other->LODIndex &&
				SectionIndex == Other->SectionIndex &&
				MeshID == Other->MeshID &&
				Ranges == Other->Ranges;
		}
		return false;
	}


	mu::Ptr<ASTOp> ASTOpParameter::Clone(MapChildFuncRef MapChild) const
	{
		Ptr<ASTOpParameter> n = new ASTOpParameter();
		n->Type = Type;
		n->Parameter = Parameter;
		n->LODIndex = LODIndex;
		n->SectionIndex = SectionIndex;
		n->MeshID = MeshID;
		for (const FRangeData& c : Ranges)
		{
			n->Ranges.Emplace(n.get(), MapChild(c.rangeSize.child()), c.rangeName, c.rangeUID);
		}
		return n;
	}


	EOpType ASTOpParameter::GetOpType() const 
	{ 
		return Type; 
	}


	uint64 ASTOpParameter::Hash() const
	{
		uint64 res = std::hash<uint64>()(uint64(Type));
		hash_combine(res, Parameter.Type);
		hash_combine(res, LODIndex);
		hash_combine(res, SectionIndex);
		hash_combine(res, Parameter.Name.Len());
		return res;
	}


	void ASTOpParameter::Assert()
	{
		ASTOp::Assert();
	}


	void ASTOpParameter::Link(FProgram& Program, FLinkerOptions*)
	{
		// Already linked?
		if (!linkedAddress)
		{

			LinkedParameterIndex = Program.Parameters.Find(Parameter);

			// If this fails, it means an ASTOpParameter was created at code generation time, but not registered into the
			// parameters map in the CodeGenerator_FirstPass.
			check(LinkedParameterIndex != INDEX_NONE);

			linkedAddress = (OP::ADDRESS)Program.OpAddress.Num();
			Program.OpAddress.Add((uint32)Program.ByteCode.Num());
			AppendCode(Program.ByteCode, Type);

			if (Type == EOpType::ME_PARAMETER)
			{
				OP::MeshParameterArgs Args;
				FMemory::Memzero(Args);
				Args.variable = (OP::ADDRESS)LinkedParameterIndex;
				Args.LOD = LODIndex;
				Args.Section = SectionIndex;
				Args.MeshID = MeshID;

				for (const auto& d : Ranges)
				{
					OP::ADDRESS sizeAt = 0;
					uint16 rangeId = 0;
					LinkRange(Program, d, sizeAt, rangeId);
					Program.Parameters[LinkedParameterIndex].Ranges.Add(rangeId);
				}

				AppendCode(Program.ByteCode, Args);
			}
			else
			{
				OP::ParameterArgs Args;
				FMemory::Memzero(Args);
				Args.variable = (OP::ADDRESS)LinkedParameterIndex;

				for (const auto& d : Ranges)
				{
					OP::ADDRESS sizeAt = 0;
					uint16 rangeId = 0;
					LinkRange(Program, d, sizeAt, rangeId);
					Program.Parameters[LinkedParameterIndex].Ranges.Add(rangeId);
				}

				AppendCode(Program.ByteCode, Args);
			}

		}
	}


	int32 ASTOpParameter::EvaluateInt(ASTOpList& facts, bool& bOutUnknown) const
	{
		bOutUnknown = true;

		// Check the facts, in case we have the value for our parameter.
		for (const auto& f : facts)
		{
			if (f->GetOpType() == EOpType::BO_EQUAL_INT_CONST)
			{
				const ASTOpBoolEqualIntConst* typedFact = static_cast<const ASTOpBoolEqualIntConst*>(f.get());
				Ptr<ASTOp> Value = typedFact->Value.child();
				if (Value.get() == this)
				{
					bOutUnknown = false;
					return typedFact->Constant;
				}
				else
				{
					// We could try something more if it was an expression and it had the parameter
					// somewhere in it.
				}
			}
		}

		return 0;
	}


	ASTOp::FBoolEvalResult ASTOpParameter::EvaluateBool(ASTOpList& /*facts*/, FEvaluateBoolCache*) const
	{
		check(Type == EOpType::BO_PARAMETER);
		return BET_UNKNOWN;
	}


	FImageDesc ASTOpParameter::GetImageDesc(bool, FGetImageDescContext*) const
	{
		check(Type == EOpType::IM_PARAMETER);
		return FImageDesc();
	}


	bool ASTOpParameter::IsColourConstant(FVector4f&) const
	{
		return false;
	}

}
