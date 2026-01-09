// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpImageMakeGrowMap.h"

#include "Containers/Map.h"
#include "HAL/UnrealMemory.h"
#include "Math/UnrealMathSSE.h"
#include "Misc/AssertionMacros.h"
#include "MuR/ImagePrivate.h"
#include "MuR/ModelPrivate.h"
#include "MuR/MutableMath.h"
#include "MuR/RefCounted.h"
#include "MuR/Types.h"
#include "MuT/ASTOpConditional.h"
#include "MuT/ASTOpImageCompose.h"
#include "MuT/ASTOpImagePatch.h"
#include "MuT/ASTOpImagePixelFormat.h"
#include "MuT/ASTOpSwitch.h"


namespace mu
{

	ASTOpImageMakeGrowMap::ASTOpImageMakeGrowMap()
		: Mask(this)
	{
	}


	ASTOpImageMakeGrowMap::~ASTOpImageMakeGrowMap()
	{
		// Explicit call needed to avoid recursive destruction
		ASTOp::RemoveChildren();
	}


	bool ASTOpImageMakeGrowMap::IsEqual(const ASTOp& otherUntyped) const
	{
		if (otherUntyped.GetOpType()==GetOpType())
		{
			const ASTOpImageMakeGrowMap* other = static_cast<const ASTOpImageMakeGrowMap*>(&otherUntyped);
			return Mask == other->Mask &&
				Border == other->Border;
		}
		return false;
	}


	uint64 ASTOpImageMakeGrowMap::Hash() const
	{
		uint64 res = std::hash<void*>()(Mask.child().get());
		hash_combine(res, Border);
		return res;
	}


	mu::Ptr<ASTOp> ASTOpImageMakeGrowMap::Clone(MapChildFuncRef mapChild) const
	{
		mu::Ptr<ASTOpImageMakeGrowMap> n = new ASTOpImageMakeGrowMap();
		n->Mask = mapChild(Mask.child());
		n->Border = Border;
		return n;
	}


	void ASTOpImageMakeGrowMap::ForEachChild(const TFunctionRef<void(ASTChild&)> f)
	{
		f(Mask);
	}


	void ASTOpImageMakeGrowMap::Link(FProgram& program, FLinkerOptions*)
	{
		// Already linked?
		if (!linkedAddress)
		{
			OP::ImageMakeGrowMapArgs Args;
			FMemory::Memzero(Args);

			Args.border = Border;
			if (Mask) Args.mask = Mask->linkedAddress;

			linkedAddress = (OP::ADDRESS)program.OpAddress.Num();
			//program.m_code.push_back(op);
			program.OpAddress.Add((uint32_t)program.ByteCode.Num());
			AppendCode(program.ByteCode, GetOpType());
			AppendCode(program.ByteCode, Args);
		}
	}


	mu::Ptr<ASTOp> ASTOpImageMakeGrowMap::OptimiseSink(const FModelOptimizationOptions& options, FOptimizeSinkContext& context) const
	{
		mu::Ptr<ASTOp> at;

		switch (Mask.child()->GetOpType())
		{

		case EOpType::IM_CONDITIONAL:
		{
			// We move the format down the two paths
			Ptr<ASTOpConditional> nop = mu::Clone<ASTOpConditional>(Mask.child());

			Ptr<ASTOpImageMakeGrowMap> aOp = mu::Clone<ASTOpImageMakeGrowMap>(this);
			aOp->Mask = nop->yes.child();
			nop->yes = aOp;

			Ptr<ASTOpImageMakeGrowMap> bOp = mu::Clone<ASTOpImageMakeGrowMap>(this);
			bOp->Mask = nop->no.child();
			nop->no = bOp;

			at = nop;
			break;
		}

		case EOpType::IM_SWITCH:
		{
			// Move the format down all the paths
			Ptr<ASTOpSwitch> nop = mu::Clone<ASTOpSwitch>(Mask.child());

			if (nop->Default)
			{
				Ptr<ASTOpImageMakeGrowMap> defOp = mu::Clone<ASTOpImageMakeGrowMap>(this);
				defOp->Mask = nop->Default.child();
				nop->Default = defOp;
			}

			// We need to copy the options because we change them
			for (size_t v = 0; v < nop->Cases.Num(); ++v)
			{
				if (nop->Cases[v].Branch)
				{
					Ptr<ASTOpImageMakeGrowMap> bOp = mu::Clone<ASTOpImageMakeGrowMap>(this);
					bOp->Mask = nop->Cases[v].Branch.child();
					nop->Cases[v].Branch = bOp;
				}
			}

			at = nop;
			break;
		}

		default:
			break;
		}


		return at;
	}


	//!
	FImageDesc ASTOpImageMakeGrowMap::GetImageDesc(bool returnBestOption, FGetImageDescContext* context) const
	{
		FImageDesc res;

		// Local context in case it is necessary
		FGetImageDescContext localContext;
		if (!context)
		{
			context = &localContext;
		}
		else
		{
			// Cached result?
			FImageDesc* PtrValue = context->m_results.Find(this);
			if (PtrValue)
			{
				return *PtrValue;
			}
		}

		if (Mask.child())
		{
			res = Mask.child()->GetImageDesc(returnBestOption, context);
		}

		// Cache the result
		context->m_results.Add(this, res);

		return res;
	}


	void ASTOpImageMakeGrowMap::GetLayoutBlockSize(int* pBlockX, int* pBlockY)
	{
		if (Mask.child())
		{
			// Assume the block size of the biggest mip
			Mask.child()->GetLayoutBlockSize(pBlockX, pBlockY);
		}
	}


	bool ASTOpImageMakeGrowMap::IsImagePlainConstant(FVector4f& colour) const
	{
		bool res = false;
		if (Mask.child())
		{
			Mask.child()->IsImagePlainConstant(colour);
		}
		return res;
	}


	mu::Ptr<ImageSizeExpression> ASTOpImageMakeGrowMap::GetImageSizeExpression() const
	{
		mu::Ptr<ImageSizeExpression> pRes;

		if (Mask.child())
		{
			pRes = Mask.child()->GetImageSizeExpression();
		}
		else
		{
			pRes = new ImageSizeExpression;
		}

		return pRes;
	}


	FSourceDataDescriptor ASTOpImageMakeGrowMap::GetSourceDataDescriptor(FGetSourceDataDescriptorContext* Context) const
	{
		if (Mask)
		{
			return Mask->GetSourceDataDescriptor(Context);
		}

		return {};
	}

}

