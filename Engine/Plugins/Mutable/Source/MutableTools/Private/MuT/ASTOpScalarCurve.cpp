// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpScalarCurve.h"

#include "HAL/PlatformMath.h"
#include "MuR/ModelPrivate.h"
#include "MuR/RefCounted.h"
#include "MuR/Types.h"


namespace mu
{


	ASTOpScalarCurve::ASTOpScalarCurve()
		: time(this)
	{
	}


	ASTOpScalarCurve::~ASTOpScalarCurve()
	{
		// Explicit call needed to avoid recursive destruction
		ASTOp::RemoveChildren();
	}


	void ASTOpScalarCurve::ForEachChild(const TFunctionRef<void(ASTChild&)> f)
	{
		f(time);
	}


	uint64 ASTOpScalarCurve::Hash() const
	{
		uint64 res = std::hash<uint64>()(size_t(EOpType::SC_CURVE));
		hash_combine(res, Curve.Keys.Num());
		return res;
	}


	bool ASTOpScalarCurve::IsEqual(const ASTOp& otherUntyped) const
	{
		if (otherUntyped.GetOpType() == GetOpType())
		{
			const ASTOpScalarCurve* other = static_cast<const ASTOpScalarCurve*>(&otherUntyped);
			return time == other->time && Curve == other->Curve;
		}
		return false;
	}


	mu::Ptr<ASTOp> ASTOpScalarCurve::Clone(MapChildFuncRef mapChild) const
	{
		Ptr<ASTOpScalarCurve> n = new ASTOpScalarCurve();
		n->Curve = Curve;
		n->time = mapChild(time.child());
		return n;
	}


	void ASTOpScalarCurve::Link(FProgram& program, FLinkerOptions*)
	{
		if (!linkedAddress)
		{
			OP::ScalarCurveArgs Args;
			FMemory::Memzero(Args);
			Args.time = time ? time->linkedAddress : 0;
			Args.curve = program.AddConstant(Curve);

			linkedAddress = (OP::ADDRESS)program.OpAddress.Num();
			//program.m_code.push_back(op);
			program.OpAddress.Add((uint32_t)program.ByteCode.Num());
			AppendCode(program.ByteCode, EOpType::SC_CURVE);
			AppendCode(program.ByteCode, Args);
		}

	}

}

