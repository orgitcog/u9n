// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpMeshClipMorphPlane.h"

#include "MuT/ASTOpMeshAddTags.h"
#include "MuR/ModelPrivate.h"
#include "MuR/RefCounted.h"
#include "MuR/Types.h"
#include "HAL/PlatformMath.h"


namespace mu
{


	//-------------------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------------------
	ASTOpMeshClipMorphPlane::ASTOpMeshClipMorphPlane()
		: Source(this)
	{
	}


	ASTOpMeshClipMorphPlane::~ASTOpMeshClipMorphPlane()
	{
		// Explicit call needed to avoid recursive destruction
		ASTOp::RemoveChildren();
	}


	bool ASTOpMeshClipMorphPlane::IsEqual(const ASTOp& otherUntyped) const
	{
		if (otherUntyped.GetOpType()==GetOpType())
		{
			const ASTOpMeshClipMorphPlane* other = static_cast<const ASTOpMeshClipMorphPlane*>(&otherUntyped);

			return Source == other->Source &&
				MorphShape == other->MorphShape &&
				SelectionShape == other->SelectionShape &&
				VertexSelectionBone == other->VertexSelectionBone &&
				VertexSelectionType == other->VertexSelectionType &&
				FaceCullStrategy == other->FaceCullStrategy &&
				Dist == other->Dist &&
				Factor == other->Factor &&
				VertexSelectionBoneMaxRadius == other->VertexSelectionBoneMaxRadius;
		}
		return false;
	}


	uint64 ASTOpMeshClipMorphPlane::Hash() const
	{
		uint64 res = std::hash<void*>()(Source.child().get());
		hash_combine(res, Factor);
		hash_combine(res, Dist);
		return res;
	}


	mu::Ptr<ASTOp> ASTOpMeshClipMorphPlane::Clone(MapChildFuncRef mapChild) const
	{
		Ptr<ASTOpMeshClipMorphPlane> n = new ASTOpMeshClipMorphPlane();
		n->Source = mapChild(Source.child());
		n->MorphShape = MorphShape;
		n->SelectionShape = SelectionShape;
		n->VertexSelectionBone = VertexSelectionBone;
		n->VertexSelectionType = VertexSelectionType;
		n->FaceCullStrategy = FaceCullStrategy;
		n->Dist = Dist;
		n->Factor = Factor;
		n->VertexSelectionBoneMaxRadius = VertexSelectionBoneMaxRadius;
		return n;
	}


	void ASTOpMeshClipMorphPlane::ForEachChild(const TFunctionRef<void(ASTChild&)> f)
	{
		f(Source);
	}


	void ASTOpMeshClipMorphPlane::Link(FProgram& Program, FLinkerOptions*)
	{
		// Already linked?
		if (!linkedAddress)
		{
			OP::MeshClipMorphPlaneArgs Args;
			FMemory::Memzero(Args);

			if (Source) Args.Source = Source->linkedAddress;

			Args.MorphShape = Program.AddConstant(MorphShape);

			Args.FaceCullStrategy = FaceCullStrategy;

			Args.VertexSelectionType = VertexSelectionType;
			if (VertexSelectionType == EClipVertexSelectionType::BoneHierarchy)
			{
				check(VertexSelectionBone.Id < MAX_uint32);
				Args.VertexSelectionShapeOrBone = VertexSelectionBone.Id;
			}
			else if (VertexSelectionType == EClipVertexSelectionType::Shape)
			{
				Args.VertexSelectionShapeOrBone = Program.AddConstant(SelectionShape);
			}

			Args.Dist = Dist;
			Args.Factor = Factor;
			Args.MaxBoneRadius = VertexSelectionBoneMaxRadius;

			linkedAddress = (OP::ADDRESS)Program.OpAddress.Num();
			Program.OpAddress.Add(Program.ByteCode.Num());
			AppendCode(Program.ByteCode, GetOpType());
			AppendCode(Program.ByteCode, Args);
		}

	}


	mu::Ptr<ASTOp> ASTOpMeshClipMorphPlane::OptimiseSink(const FModelOptimizationOptions&, FOptimizeSinkContext&) const
	{
		Ptr<ASTOp> NewOp;

		if (!Source.child())
		{
			return nullptr;
		}

		EOpType SourceType = Source.child()->GetOpType();

		// Optimize only the mesh parameter
		switch (SourceType)
		{

		case EOpType::ME_ADDTAGS:
		{
			Ptr<ASTOpMeshAddTags> NewAddTags = mu::Clone<ASTOpMeshAddTags>(Source.child());

			if (NewAddTags->Source)
			{
				Ptr<ASTOpMeshClipMorphPlane> New = mu::Clone<ASTOpMeshClipMorphPlane>(this);
				New->Source = NewAddTags->Source.child();
				NewAddTags->Source = New;
			}

			NewOp = NewAddTags;
			break;
		}

		default:
			break;

		}

		return NewOp;
	}


	FSourceDataDescriptor ASTOpMeshClipMorphPlane::GetSourceDataDescriptor(FGetSourceDataDescriptorContext* Context) const
	{
		if (Source)
		{
			return Source->GetSourceDataDescriptor(Context);
		}

		return {};
	}

}
