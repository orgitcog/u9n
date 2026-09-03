// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpMeshMaskDiff.h"
#include "MuT/ASTOpMeshAddTags.h"
#include "MuT/ASTOpSwitch.h"
#include "MuT/ASTOpMeshPrepareLayout.h"
#include "MuR/ModelPrivate.h"
#include "MuR/RefCounted.h"
#include "MuR/Types.h"
#include "HAL/PlatformMath.h"

namespace mu
{

	ASTOpMeshMaskDiff::ASTOpMeshMaskDiff()
		: Source(this)
		, Fragment(this)
	{
	}


	ASTOpMeshMaskDiff::~ASTOpMeshMaskDiff()
	{
		// Explicit call needed to avoid recursive destruction
		ASTOp::RemoveChildren();
	}


	bool ASTOpMeshMaskDiff::IsEqual(const ASTOp& OtherUntyped) const
	{
		if (OtherUntyped.GetOpType()==GetOpType())
		{
			const ASTOpMeshMaskDiff* Other = static_cast<const ASTOpMeshMaskDiff*>(&OtherUntyped);
			return Source == Other->Source &&
				Fragment == Other->Fragment;
		}
		return false;
	}


	uint64 ASTOpMeshMaskDiff::Hash() const
	{
		uint64 Result = std::hash<void*>()(Source.child().get());
		hash_combine(Result, Fragment.child().get());
		return Result;
	}


	Ptr<ASTOp> ASTOpMeshMaskDiff::Clone(MapChildFuncRef MapChild) const
	{
		Ptr<ASTOpMeshMaskDiff> New = new ASTOpMeshMaskDiff();
		New->Source = MapChild(Source.child());
		New->Fragment = MapChild(Fragment.child());
		return New;
	}


	void ASTOpMeshMaskDiff::ForEachChild(const TFunctionRef<void(ASTChild&)> Func)
	{
		Func(Source);
		Func(Fragment);
	}


	void ASTOpMeshMaskDiff::Link(FProgram& Program, FLinkerOptions*)
	{
		// Already linked?
		if (!linkedAddress)
		{
			OP::MeshMaskDiffArgs Args;
			FMemory::Memzero(Args);

			if (Source) Args.Source = Source->linkedAddress;
			if (Fragment) Args.Fragment = Fragment->linkedAddress;

			linkedAddress = (OP::ADDRESS)Program.OpAddress.Num();
			Program.OpAddress.Add(Program.ByteCode.Num());
			AppendCode(Program.ByteCode, GetOpType());
			AppendCode(Program.ByteCode, Args);
		}
	}


	mu::Ptr<ASTOp> ASTOpMeshMaskDiff::OptimiseSemantic(const FModelOptimizationOptions&, int32 Pass) const
	{
		if (!Fragment)
		{
			return nullptr;
		}

		mu::Ptr<ASTOp> ResultOp;

		EOpType FragmentType = Fragment->GetOpType();
		switch (FragmentType)
		{

		case EOpType::ME_ADDTAGS:
		{
			// Tags in the fragment can be ignored.
			const ASTOpMeshAddTags* Add = static_cast<const ASTOpMeshAddTags*>(Fragment.child().get());

			Ptr<ASTOpMeshMaskDiff> NewAt = mu::Clone<ASTOpMeshMaskDiff>(this);
			NewAt->Fragment = Add->Source.child();
			ResultOp = NewAt;
			break;
		}

		case EOpType::ME_PREPARELAYOUT:
		{
			// Ignore the prepare op in the source: it doesn't contribute to the diff generation.
			const ASTOpMeshPrepareLayout* Prepare = static_cast<const ASTOpMeshPrepareLayout*>(Source.child().get());

			Ptr<ASTOpMeshMaskDiff> NewAt = mu::Clone<ASTOpMeshMaskDiff>(this);
			NewAt->Source = Prepare->Mesh.child();
			ResultOp = NewAt;
			break;
		}

		case EOpType::ME_SWITCH:
		{
			// Move the mask diff down all the paths
			Ptr<ASTOpSwitch> NewOp = mu::Clone<ASTOpSwitch>(Fragment.child().get());

			if (NewOp->Default)
			{
				Ptr<ASTOpMeshMaskDiff> defOp = mu::Clone<ASTOpMeshMaskDiff>(this);
				defOp->Fragment = NewOp->Default.child();
				NewOp->Default = defOp;
			}

			// We need to copy the options because we change them
			for (int32 v = 0; v < NewOp->Cases.Num(); ++v)
			{
				if (NewOp->Cases[v].Branch)
				{
					Ptr<ASTOpMeshMaskDiff> bOp = mu::Clone<ASTOpMeshMaskDiff>(this);
					bOp->Fragment = NewOp->Cases[v].Branch.child();
					NewOp->Cases[v].Branch = bOp;
				}
			}

			ResultOp = NewOp;
			break;
		}

		default:
			break;
		}

		// If we didn't optimize the fragment, see ifg we can optimize the source.
		if (!ResultOp)
		{
			EOpType SourceType = Source->GetOpType();
			switch (SourceType)
			{

			case EOpType::ME_ADDTAGS:
			{
				// Tags in the source can be ignored for mask generation.
				const ASTOpMeshAddTags* Add = static_cast<const ASTOpMeshAddTags*>(Source.child().get());

				Ptr<ASTOpMeshMaskDiff> NewAt = mu::Clone<ASTOpMeshMaskDiff>(this);
				NewAt->Source = Add->Source.child();
				ResultOp = NewAt;
				break;
			}

			case EOpType::ME_PREPARELAYOUT:
			{
				// Ignore the prepare op in the source: it doesn't contribute to the diff generation.
				const ASTOpMeshPrepareLayout* Prepare = static_cast<const ASTOpMeshPrepareLayout*>(Source.child().get());

				Ptr<ASTOpMeshMaskDiff> NewAt = mu::Clone<ASTOpMeshMaskDiff>(this);
				NewAt->Source = Prepare->Mesh.child();
				ResultOp = NewAt;
				break;
			}

			default:
				break;
			}
		}

		return ResultOp;
	}


	FSourceDataDescriptor ASTOpMeshMaskDiff::GetSourceDataDescriptor(FGetSourceDataDescriptorContext* Context) const
	{
		if (Source)
		{
			return Source->GetSourceDataDescriptor(Context);
		}

		return {};
	}
}
