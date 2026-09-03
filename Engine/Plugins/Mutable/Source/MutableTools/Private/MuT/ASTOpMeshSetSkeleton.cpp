// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpMeshSetSkeleton.h"
#include "MuT/ASTOpMeshAddTags.h"

#include "HAL/PlatformMath.h"
#include "MuR/ModelPrivate.h"
#include "MuR/RefCounted.h"
#include "MuR/Types.h"

namespace mu
{

	ASTOpMeshSetSkeleton::ASTOpMeshSetSkeleton()
		: Source(this)
		, Skeleton(this)
	{
	}


	ASTOpMeshSetSkeleton::~ASTOpMeshSetSkeleton()
	{
		// Explicit call needed to avoid recursive destruction
		ASTOp::RemoveChildren();
	}


	bool ASTOpMeshSetSkeleton::IsEqual(const ASTOp& OtherUntyped) const
	{
		if (OtherUntyped.GetOpType()==GetOpType())
		{
			const ASTOpMeshSetSkeleton* Other = static_cast<const ASTOpMeshSetSkeleton*>(&OtherUntyped);
			return Source == Other->Source &&
				Skeleton == Other->Skeleton;
		}
		return false;
	}


	uint64 ASTOpMeshSetSkeleton::Hash() const
	{
		uint64 Result = std::hash<void*>()(Source.child().get());
		hash_combine(Result, Skeleton.child().get());
		return Result;
	}


	Ptr<ASTOp> ASTOpMeshSetSkeleton::Clone(MapChildFuncRef MapChild) const
	{
		Ptr<ASTOpMeshSetSkeleton> New = new ASTOpMeshSetSkeleton();
		New->Source = MapChild(Source.child());
		New->Skeleton = MapChild(Skeleton.child());
		return New;
	}


	void ASTOpMeshSetSkeleton::ForEachChild(const TFunctionRef<void(ASTChild&)> Func)
	{
		Func(Source);
		Func(Skeleton);
	}


	void ASTOpMeshSetSkeleton::Link(FProgram& Program, FLinkerOptions*)
	{
		// Already linked?
		if (!linkedAddress)
		{
			OP::MeshApplyPoseArgs Args;
			FMemory::Memzero(Args);

			if (Source) Args.base = Source->linkedAddress;
			if (Skeleton) Args.pose = Skeleton->linkedAddress;

			linkedAddress = (OP::ADDRESS)Program.OpAddress.Num();
			Program.OpAddress.Add(Program.ByteCode.Num());
			AppendCode(Program.ByteCode, GetOpType());
			AppendCode(Program.ByteCode, Args);
		}

	}


	FSourceDataDescriptor ASTOpMeshSetSkeleton::GetSourceDataDescriptor(FGetSourceDataDescriptorContext* Context) const
	{
		if (Source)
		{
			return Source->GetSourceDataDescriptor(Context);
		}

		return {};
	}
}
