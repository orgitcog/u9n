// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpMeshClipWithMesh.h"
#include "MuT/ASTOpMeshAddTags.h"

#include "HAL/PlatformMath.h"
#include "MuR/ModelPrivate.h"
#include "MuR/RefCounted.h"
#include "MuR/Types.h"

namespace mu
{

	ASTOpMeshClipWithMesh::ASTOpMeshClipWithMesh()
		: Source(this)
		, ClipMesh(this)
	{
	}


	ASTOpMeshClipWithMesh::~ASTOpMeshClipWithMesh()
	{
		// Explicit call needed to avoid recursive destruction
		ASTOp::RemoveChildren();
	}


	bool ASTOpMeshClipWithMesh::IsEqual(const ASTOp& OtherUntyped) const
	{
		if (OtherUntyped.GetOpType()==GetOpType())
		{
			const ASTOpMeshClipWithMesh* Other = static_cast<const ASTOpMeshClipWithMesh*>(&OtherUntyped);
			return Source == Other->Source &&
				ClipMesh == Other->ClipMesh;
		}
		return false;
	}


	uint64 ASTOpMeshClipWithMesh::Hash() const
	{
		uint64 Result = std::hash<void*>()(Source.child().get());
		hash_combine(Result, ClipMesh.child().get());
		return Result;
	}


	Ptr<ASTOp> ASTOpMeshClipWithMesh::Clone(MapChildFuncRef MapChild) const
	{
		Ptr<ASTOpMeshClipWithMesh> New = new ASTOpMeshClipWithMesh();
		New->Source = MapChild(Source.child());
		New->ClipMesh = MapChild(ClipMesh.child());
		return New;
	}


	void ASTOpMeshClipWithMesh::ForEachChild(const TFunctionRef<void(ASTChild&)> Func)
	{
		Func(Source);
		Func(ClipMesh);
	}


	void ASTOpMeshClipWithMesh::Link(FProgram& Program, FLinkerOptions*)
	{
		// Already linked?
		if (!linkedAddress)
		{
			OP::MeshApplyPoseArgs Args;
			FMemory::Memzero(Args);

			if (Source) Args.base = Source->linkedAddress;
			if (ClipMesh) Args.pose = ClipMesh->linkedAddress;

			linkedAddress = (OP::ADDRESS)Program.OpAddress.Num();
			Program.OpAddress.Add(Program.ByteCode.Num());
			AppendCode(Program.ByteCode, GetOpType());
			AppendCode(Program.ByteCode, Args);
		}

	}


	Ptr<ASTOp> ASTOpMeshClipWithMesh::OptimiseSink(const FModelOptimizationOptions&, FOptimizeSinkContext&) const
	{
		Ptr<ASTOp> NewOp;

		Ptr<ASTOp> MeshAt = Source.child();

		if (!MeshAt)
		{
			return nullptr;
		}

		EOpType MeshType = MeshAt->GetOpType();

		switch (MeshType)
		{
			case EOpType::ME_ADDTAGS:
			{
				Ptr<ASTOpMeshAddTags> New = mu::Clone<ASTOpMeshAddTags>(MeshAt);
				if (New->Source)
				{
					Ptr<ASTOpMeshClipWithMesh> NewApplyPose = mu::Clone<ASTOpMeshClipWithMesh>(this);
					NewApplyPose->Source = New->Source.child();
					New->Source = NewApplyPose;
				}

				NewOp = New;
				break;
			}

			default:
				break;
		}

		return NewOp;
	}


	FSourceDataDescriptor ASTOpMeshClipWithMesh::GetSourceDataDescriptor(FGetSourceDataDescriptorContext* Context) const
	{
		if (Source)
		{
			return Source->GetSourceDataDescriptor(Context);
		}

		return {};
	}
}
