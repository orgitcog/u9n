// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpMeshMerge.h"
#include "MuT/ASTOpMeshAddTags.h"

#include "HAL/PlatformMath.h"
#include "MuR/ModelPrivate.h"
#include "MuR/RefCounted.h"
#include "MuR/Types.h"

namespace mu
{

	ASTOpMeshMerge::ASTOpMeshMerge()
		: Base(this)
		, Added(this)
	{
	}


	ASTOpMeshMerge::~ASTOpMeshMerge()
	{
		// Explicit call needed to avoid recursive destruction
		ASTOp::RemoveChildren();
	}


	bool ASTOpMeshMerge::IsEqual(const ASTOp& OtherUntyped) const
	{
		if (OtherUntyped.GetOpType()==GetOpType())
		{
			const ASTOpMeshMerge* Other = static_cast<const ASTOpMeshMerge*>(&OtherUntyped);
			return Base == Other->Base &&
				Added == Other->Added &&
				NewSurfaceID == Other->NewSurfaceID;
		}
		return false;
	}


	uint64 ASTOpMeshMerge::Hash() const
	{
		uint64 Result = std::hash<void*>()(Base.child().get());
		hash_combine(Result, Added.child().get());
		hash_combine(Result, NewSurfaceID);
		return Result;
	}


	Ptr<ASTOp> ASTOpMeshMerge::Clone(MapChildFuncRef MapChild) const
	{
		Ptr<ASTOpMeshMerge> New = new ASTOpMeshMerge();
		New->Base = MapChild(Base.child());
		New->Added = MapChild(Added.child());
		New->NewSurfaceID = NewSurfaceID;
		return New;
	}


	void ASTOpMeshMerge::ForEachChild(const TFunctionRef<void(ASTChild&)> Func)
	{
		Func(Base);
		Func(Added);
	}


	void ASTOpMeshMerge::Link(FProgram& Program, FLinkerOptions*)
	{
		// Already linked?
		if (!linkedAddress)
		{
			OP::MeshMergeArgs Args;
			FMemory::Memzero(Args);

			if (Base) Args.Base = Base->linkedAddress;
			if (Added) Args.Added = Added->linkedAddress;
			Args.NewSurfaceID = NewSurfaceID;

			linkedAddress = (OP::ADDRESS)Program.OpAddress.Num();
			Program.OpAddress.Add(Program.ByteCode.Num());
			AppendCode(Program.ByteCode, GetOpType());
			AppendCode(Program.ByteCode, Args);
		}
	}


	FSourceDataDescriptor ASTOpMeshMerge::GetSourceDataDescriptor(FGetSourceDataDescriptorContext* Context) const
	{
		if (Base)
		{
			return Base->GetSourceDataDescriptor(Context);
		}

		return {};
	}
}
