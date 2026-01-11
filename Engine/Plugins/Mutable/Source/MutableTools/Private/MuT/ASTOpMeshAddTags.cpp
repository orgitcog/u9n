// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpMeshAddTags.h"

#include "MuT/ASTOpMeshMorph.h"
#include "HAL/PlatformMath.h"
#include "MuR/ModelPrivate.h"
#include "MuR/RefCounted.h"
#include "MuR/Types.h"


namespace mu
{

	ASTOpMeshAddTags::ASTOpMeshAddTags()
		: Source(this)
	{
	}


	ASTOpMeshAddTags::~ASTOpMeshAddTags()
	{
		// Explicit call needed to avoid recursive destruction
		ASTOp::RemoveChildren();
	}


	bool ASTOpMeshAddTags::IsEqual(const ASTOp& otherUntyped) const
	{
		if (otherUntyped.GetOpType()==GetOpType())
		{
			const ASTOpMeshAddTags* other = static_cast<const ASTOpMeshAddTags*>(&otherUntyped);
			return Source == other->Source && Tags == other->Tags;
		}
		return false;
	}


	mu::Ptr<ASTOp> ASTOpMeshAddTags::Clone(MapChildFuncRef mapChild) const
	{
		Ptr<ASTOpMeshAddTags> n = new ASTOpMeshAddTags();
		n->Source = mapChild(Source.child());
		n->Tags = Tags;
		return n;
	}


	void ASTOpMeshAddTags::ForEachChild(const TFunctionRef<void(ASTChild&)> f)
	{
		f(Source);
	}


	uint64 ASTOpMeshAddTags::Hash() const
	{
		uint64 res = std::hash<ASTOp*>()(Source.child().get());
		return res;
	}


	void ASTOpMeshAddTags::Link(FProgram& Program, FLinkerOptions*)
	{
		// Already linked?
		if (!linkedAddress)
		{
			linkedAddress = (OP::ADDRESS)Program.OpAddress.Num();

			Program.OpAddress.Add((uint32)Program.ByteCode.Num());
			AppendCode(Program.ByteCode, EOpType::ME_ADDTAGS);
			OP::ADDRESS SourceAt = Source ? Source->linkedAddress : 0;
			AppendCode(Program.ByteCode, SourceAt);
			AppendCode(Program.ByteCode, (uint16)Tags.Num());
			for (const FString& Tag : Tags)
			{
				OP::ADDRESS TagConstantAddress = Program.AddConstant(Tag);
				AppendCode(Program.ByteCode, TagConstantAddress);
			}
		}
	}


	FSourceDataDescriptor ASTOpMeshAddTags::GetSourceDataDescriptor(FGetSourceDataDescriptorContext* Context) const
	{
		if (Source)
		{
			return Source->GetSourceDataDescriptor(Context);
		}

		return {};
	}

}
