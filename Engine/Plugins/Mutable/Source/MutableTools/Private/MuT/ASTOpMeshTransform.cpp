// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpMeshTransform.h"

#include "HAL/PlatformMath.h"
#include "MuR/ModelPrivate.h"
#include "MuR/RefCounted.h"
#include "MuR/Types.h"


namespace mu
{

	ASTOpMeshTransform::ASTOpMeshTransform()
		: source(this)
	{
	}


	ASTOpMeshTransform::~ASTOpMeshTransform()
	{
		// Explicit call needed to avoid recursive destruction
		ASTOp::RemoveChildren();
	}


	bool ASTOpMeshTransform::IsEqual(const ASTOp& otherUntyped) const
	{
		if (otherUntyped.GetOpType() == GetOpType())
		{
			const ASTOpMeshTransform* other = static_cast<const ASTOpMeshTransform*>(&otherUntyped);
			return source == other->source &&
				matrix == other->matrix;
		}
		return false;
	}


	uint64 ASTOpMeshTransform::Hash() const
	{
		uint64 res = std::hash<EOpType>()(EOpType::ME_TRANSFORM);
		hash_combine(res, source.child().get());
		return res;
	}


	mu::Ptr<ASTOp> ASTOpMeshTransform::Clone(MapChildFuncRef mapChild) const
	{
		Ptr<ASTOpMeshTransform> n = new ASTOpMeshTransform();
		n->matrix = matrix;
		n->source = mapChild(source.child());
		return n;
	}


	void ASTOpMeshTransform::ForEachChild(const TFunctionRef<void(ASTChild&)> f)
	{
		f(source);
	}


	void ASTOpMeshTransform::Link(FProgram& program, FLinkerOptions*)
	{
		// Already linked?
		if (!linkedAddress)
		{
			OP::MeshTransformArgs Args;
			FMemory::Memzero(Args);

			if (source) Args.source = source->linkedAddress;

			Args.matrix = program.AddConstant(matrix);

			linkedAddress = (OP::ADDRESS)program.OpAddress.Num();
			//program.m_code.push_back(op);
			program.OpAddress.Add((uint32_t)program.ByteCode.Num());
			AppendCode(program.ByteCode, EOpType::ME_TRANSFORM);
			AppendCode(program.ByteCode, Args);
		}

	}


	FSourceDataDescriptor ASTOpMeshTransform::GetSourceDataDescriptor(FGetSourceDataDescriptorContext* Context) const
	{
		if (source)
		{
			return source->GetSourceDataDescriptor(Context);
		}

		return {};
	}


	ASTOp::EClosedMeshTest ASTOpMeshTransform::IsClosedMesh(TMap<const ASTOp*, EClosedMeshTest>* Cache) const
	{
		if (source)
		{
			return source->IsClosedMesh(Cache);
		}
		return EClosedMeshTest::Unknown;
	}

}
