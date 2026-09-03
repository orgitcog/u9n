// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpImageInterpolate.h"

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
#include "MuT/ASTOpImageMultiLayer.h"
#include "MuT/ASTOpImageLayer.h"
#include "MuT/ASTOpImageLayerColor.h"
#include "MuT/ASTOpImageRasterMesh.h"
#include "MuT/ASTOpImageTransform.h"
#include "MuT/ASTOpImageMipmap.h"
#include "MuT/ASTOpSwitch.h"

namespace mu
{

	ASTOpImageInterpolate::ASTOpImageInterpolate()
		: Factor(this), Targets{ ASTChild(this),ASTChild(this),ASTChild(this),ASTChild(this),ASTChild(this),ASTChild(this) }
	{
	}


	ASTOpImageInterpolate::~ASTOpImageInterpolate()
	{
		// Explicit call needed to avoid recursive destruction
		ASTOp::RemoveChildren();
	}


	bool ASTOpImageInterpolate::IsEqual(const ASTOp& OtherUntyped) const
	{
		if (OtherUntyped.GetOpType()==GetOpType())
		{
			const ASTOpImageInterpolate* Other = static_cast<const ASTOpImageInterpolate*>(&OtherUntyped);
			for (int32 i = 0; i<MUTABLE_OP_MAX_INTERPOLATE_COUNT; ++i)
			{
				if (!(Targets[i] == Other->Targets[i]))
				{
					return false;
				}
			}
			return Factor == Other->Factor;
		}
		return false;
	}


	uint64 ASTOpImageInterpolate::Hash() const
	{
		uint64 res = std::hash<void*>()(Targets[0].child().get());
		hash_combine(res, std::hash<void*>()(Targets[1].child().get()));
		hash_combine(res, std::hash<void*>()(Targets[2].child().get()));
		hash_combine(res, std::hash<void*>()(Targets[3].child().get()));
		hash_combine(res, std::hash<void*>()(Targets[4].child().get()));
		hash_combine(res, std::hash<void*>()(Targets[5].child().get()));
		hash_combine(res, std::hash<void*>()(Factor.child().get()));
		return res;
	}


	mu::Ptr<ASTOp> ASTOpImageInterpolate::Clone(MapChildFuncRef MapChild) const
	{
		mu::Ptr<ASTOpImageInterpolate> n = new ASTOpImageInterpolate();
		n->Factor = MapChild(Factor.child());
		for (int32 i = 0; i < MUTABLE_OP_MAX_INTERPOLATE_COUNT; ++i)
		{
			n->Targets[i] = MapChild(Targets[i].child());
		}
		n->Factor = Factor.child();
		return n;
	}


	void ASTOpImageInterpolate::ForEachChild(const TFunctionRef<void(ASTChild&)> Func)
	{
		Func(Factor);
		for (int32 i = 0; i < MUTABLE_OP_MAX_INTERPOLATE_COUNT; ++i)
		{
			Func(Targets[i]);
		}
	}


	void ASTOpImageInterpolate::Link(FProgram& program, FLinkerOptions*)
	{
		// Already linked?
		if (!linkedAddress)
		{
			OP::ImageInterpolateArgs Args;
			FMemory::Memzero(Args);

			if (Factor)
			{
				Args.Factor = Factor->linkedAddress;
			}
			for (int32 i = 0; i < MUTABLE_OP_MAX_INTERPOLATE_COUNT; ++i)
			{
				if (Targets[i]) Args.Targets[i] = Targets[i]->linkedAddress;
			}			

			linkedAddress = (OP::ADDRESS)program.OpAddress.Num();
			program.OpAddress.Add((uint32)program.ByteCode.Num());
			AppendCode(program.ByteCode, GetOpType());
			AppendCode(program.ByteCode, Args);
		}
	}


	//!
	FImageDesc ASTOpImageInterpolate::GetImageDesc(bool bReturnBestOption, FGetImageDescContext* Context) const
	{
		FImageDesc res;

		// Local context in case it is necessary
		FGetImageDescContext localContext;
		if (!Context)
		{
			Context = &localContext;
		}
		else
		{
			// Cached result?
			FImageDesc* PtrValue = Context->m_results.Find(this);
			if (PtrValue)
			{
				return *PtrValue;
			}
		}

		if (Targets[0])
		{
			res = Targets[0]->GetImageDesc(bReturnBestOption, Context);
		}

		// Cache the result
		if (Context)
		{
			Context->m_results.Add(this, res);
		}

		return res;
	}


	void ASTOpImageInterpolate::GetLayoutBlockSize(int32* pBlockX, int32* pBlockY)
	{
		if (Targets[0].child())
		{
			// Assume the block size of the biggest mip
			Targets[0].child()->GetLayoutBlockSize(pBlockX, pBlockY);
		}
	}


	mu::Ptr<ImageSizeExpression> ASTOpImageInterpolate::GetImageSizeExpression() const
	{
		mu::Ptr<ImageSizeExpression> pRes;

		if (Targets[0].child())
		{
			pRes = Targets[0].child()->GetImageSizeExpression();
		}
		else
		{
			pRes = new ImageSizeExpression;
		}

		return pRes;
	}


	FSourceDataDescriptor ASTOpImageInterpolate::GetSourceDataDescriptor(FGetSourceDataDescriptorContext* Context) const
	{
		// Cache management
		TUniquePtr<FGetSourceDataDescriptorContext> LocalContext;
		if (!Context)
		{
			LocalContext.Reset(new FGetSourceDataDescriptorContext);
			Context = LocalContext.Get();
		}

		FSourceDataDescriptor* Found= Context->Cache.Find(this);
		if (Found)
		{
			return *Found;
		}

		// Not cached: calculate
		FSourceDataDescriptor Result;

		for( int32 TargetIndex=0; TargetIndex<MUTABLE_OP_MAX_INTERPOLATE_COUNT; ++TargetIndex)
		{ 
			if (Targets[TargetIndex])
			{
				FSourceDataDescriptor TargetDesc = Targets[TargetIndex]->GetSourceDataDescriptor(Context);
				Result.CombineWith(TargetDesc);
			}
		}

		Context->Cache.Add(this,Result);

		return Result;
	}


}
