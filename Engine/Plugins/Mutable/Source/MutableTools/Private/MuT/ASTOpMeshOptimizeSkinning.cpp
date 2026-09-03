// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpMeshOptimizeSkinning.h"

#include "MuT/ASTOpConditional.h"
#include "MuT/ASTOpMeshMorph.h"
#include "MuT/ASTOpMeshAddTags.h"
#include "MuT/ASTOpMeshApplyLayout.h"
#include "MuT/ASTOpSwitch.h"
#include "MuR/ModelPrivate.h"
#include "MuR/RefCounted.h"
#include "MuR/Types.h"
#include "HAL/PlatformMath.h"


namespace mu
{

	ASTOpMeshOptimizeSkinning::ASTOpMeshOptimizeSkinning()
		: Source(this)
	{
	}


	ASTOpMeshOptimizeSkinning::~ASTOpMeshOptimizeSkinning()
	{
		// Explicit call needed to avoid recursive destruction
		ASTOp::RemoveChildren();
	}


	bool ASTOpMeshOptimizeSkinning::IsEqual(const ASTOp& OtherUntyped) const
	{
		if (OtherUntyped.GetOpType() == GetOpType())
		{
			const ASTOpMeshOptimizeSkinning* Other = static_cast<const ASTOpMeshOptimizeSkinning*>(&OtherUntyped);
			return Source == Other->Source;
		}
		return false;
	}


	uint64 ASTOpMeshOptimizeSkinning::Hash() const
	{
		uint64 Res = std::hash<void*>()(Source.child().get());
		return Res;
	}


	mu::Ptr<ASTOp> ASTOpMeshOptimizeSkinning::Clone(MapChildFuncRef MapChild) const
	{
		Ptr<ASTOpMeshOptimizeSkinning> n = new ASTOpMeshOptimizeSkinning();
		n->Source = MapChild(Source.child());
		return n;
	}


	void ASTOpMeshOptimizeSkinning::ForEachChild(const TFunctionRef<void(ASTChild&)> Func)
	{
		Func(Source);
	}


	void ASTOpMeshOptimizeSkinning::Link(FProgram& Program, FLinkerOptions*)
	{
		// Already linked?
		if (!linkedAddress)
		{
			OP::MeshOptimizeSkinningArgs Args;
			FMemory::Memzero(Args);

			if (Source)
			{
				Args.source = Source->linkedAddress;
			}

			linkedAddress = OP::ADDRESS(Program.OpAddress.Num());
			Program.OpAddress.Add((uint32)Program.ByteCode.Num());
			AppendCode(Program.ByteCode, EOpType::ME_OPTIMIZESKINNING);
			AppendCode(Program.ByteCode, Args);
		}
	}


	mu::Ptr<ASTOp> ASTOpMeshOptimizeSkinning::OptimiseSink(const FModelOptimizationOptions&, FOptimizeSinkContext& Context) const
	{
		return Context.MeshOptimizeSkinningSinker.Apply(this);
	}


	FSourceDataDescriptor ASTOpMeshOptimizeSkinning::GetSourceDataDescriptor(FGetSourceDataDescriptorContext* Context) const
	{
		if (Source)
		{
			return Source->GetSourceDataDescriptor(Context);
		}

		return {};
	}




	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	mu::Ptr<ASTOp> Sink_MeshOptimizeSkinningAST::Apply(const ASTOpMeshOptimizeSkinning* InRoot)
	{
		Root = InRoot;

		OldToNew.Reset();

		InitialSource = Root->Source.child();
		mu::Ptr<ASTOp> NewSource = Visit(InitialSource, Root);

		// If there is any change, it is the new root.
		if (NewSource != InitialSource)
		{
			return NewSource;
		}

		return nullptr;
	}


	mu::Ptr<ASTOp> Sink_MeshOptimizeSkinningAST::Visit(const mu::Ptr<ASTOp>& at, const ASTOpMeshOptimizeSkinning* CurrentSinkOp)
	{
		if (!at)
		{
			return nullptr;
		}

		// Move the operation down the conditionals and harmless operations to try to optimize it into a constant if possible
		// and also try to move the conditionals to theroot so that may be moved out of the mesh operation graph entirely.

		// Already visited?
		const Ptr<ASTOp>* Cached = OldToNew.Find({ at, CurrentSinkOp });
		if (Cached)
		{
			return *Cached;
		}

		mu::Ptr<ASTOp> NewAt = at;
		switch (at->GetOpType())
		{

		case EOpType::ME_APPLYLAYOUT:
		{
			Ptr<ASTOpMeshApplyLayout> NewOp = mu::Clone<ASTOpMeshApplyLayout>(at);
			NewOp->Mesh = Visit(NewOp->Mesh.child(), CurrentSinkOp);
			NewAt = NewOp;
			break;
		}

		case EOpType::ME_ADDTAGS:
		{
			Ptr<ASTOpMeshAddTags> NewOp = mu::Clone<ASTOpMeshAddTags>(at);
			NewOp->Source = Visit(NewOp->Source.child(), CurrentSinkOp);
			NewAt = NewOp;
			break;
		}

		case EOpType::ME_CONDITIONAL:
		{
			Ptr<ASTOpConditional> NewOp = mu::Clone<ASTOpConditional>(at);
			NewOp->yes = Visit(NewOp->yes.child(), CurrentSinkOp);
			NewOp->no = Visit(NewOp->no.child(), CurrentSinkOp);
			NewAt = NewOp;
			break;
		}

		case EOpType::ME_SWITCH:
		{
			Ptr<ASTOpSwitch> NewOp = mu::Clone<ASTOpSwitch>(at);
			NewOp->Default = Visit(NewOp->Default.child(), CurrentSinkOp);
			for (ASTOpSwitch::FCase& c : NewOp->Cases)
			{
				c.Branch = Visit(c.Branch.child(), CurrentSinkOp);
			}
			NewAt = NewOp;
			break;
		}

		// If we reach here it means the operation type has not been optimized.
		default:
			if (at != InitialSource)
			{
				mu::Ptr<ASTOpMeshOptimizeSkinning> NewOp = mu::Clone<ASTOpMeshOptimizeSkinning>(CurrentSinkOp);
				NewOp->Source = at;
				NewAt = NewOp;
			}
			break;

		}

		OldToNew.Add({ at, CurrentSinkOp }, NewAt);

		return NewAt;
	}

}
