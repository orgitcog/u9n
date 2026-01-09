// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpImageInvert.h"

#include "Containers/Map.h"
#include "MuR/ModelPrivate.h"
#include "MuR/RefCounted.h"
#include "MuR/Types.h"
#include "MuR/ImagePrivate.h"
#include "MuT/ASTOpConditional.h"
#include "MuT/ASTOpSwitch.h"
#include "MuT/ASTOpImagePatch.h"
#include "MuT/ASTOpImagePixelFormat.h"
#include "MuT/ASTOpImageLayer.h"
#include "MuT/ASTOpImageLayerColor.h"
#include "MuT/ASTOpImageRasterMesh.h"
#include "MuT/ASTOpImageTransform.h"
#include "MuT/ASTOpImageSwizzle.h"
#include "MuT/ASTOpImageCompose.h"
#include "MuT/ASTOpImageMakeGrowMap.h"
#include "MuT/ASTOpImageResize.h"


namespace mu
{

	ASTOpImageInvert::ASTOpImageInvert()
		: Base(this)
	{
	}


	ASTOpImageInvert::~ASTOpImageInvert()
	{
		// Explicit call needed to avoid recursive destruction
		ASTOp::RemoveChildren();
	}


	bool ASTOpImageInvert::IsEqual(const ASTOp& InOther) const
	{
		if (InOther.GetOpType()==GetOpType())
		{
			const ASTOpImageInvert* Other = static_cast<const ASTOpImageInvert*>(&InOther);
			return Base == Other->Base;
		}
		return false;
	}


	uint64 ASTOpImageInvert::Hash() const
	{
		uint64 Res = std::hash<EOpType>()(GetOpType());
		hash_combine(Res, Base.child().get());
		return Res;
	}


	Ptr<ASTOp> ASTOpImageInvert::Clone(MapChildFuncRef MapChild) const
	{
		Ptr<ASTOpImageInvert> New = new ASTOpImageInvert();
		New->Base = MapChild(Base.child());
		return New;
	}


	void ASTOpImageInvert::ForEachChild(const TFunctionRef<void(ASTChild&)> Func)
	{
		Func(Base);
	}


	void ASTOpImageInvert::Link(FProgram& Program, FLinkerOptions*)
	{
		// Already linked?
		if (!linkedAddress)
		{
			OP::ImageInvertArgs Args;
			FMemory::Memzero(Args);

			if (Base)
			{
				Args.Base = Base->linkedAddress;
			}

			linkedAddress = (OP::ADDRESS)Program.OpAddress.Num();
			Program.OpAddress.Add(Program.ByteCode.Num());
			AppendCode(Program.ByteCode, GetOpType());
			AppendCode(Program.ByteCode, Args);
		}
	}


	FImageDesc ASTOpImageInvert::GetImageDesc(bool bReturnBestOption, FGetImageDescContext* Context) const
	{
		FImageDesc Result;

		// Local context in case it is necessary
		FGetImageDescContext LocalContext;
		if (!Context)
		{
			Context = &LocalContext;
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

		// Actual work
		if (Base)
		{
			Result = Base->GetImageDesc(bReturnBestOption, Context);
		}

		// Cache the result
		if (Context)
		{
			Context->m_results.Add(this, Result);
		}

		return Result;
	}


	Ptr<ImageSizeExpression> ASTOpImageInvert::GetImageSizeExpression() const
	{
		if (Base)
		{
			return Base->GetImageSizeExpression();
		}

		return new ImageSizeExpression;
	}


	bool ASTOpImageInvert::IsImagePlainConstant(FVector4f& OutColour) const
	{
		bool bResult = true;
		OutColour = FVector4f(1.0f,1.0f,1.0f,1.0f);

		if (Base)
		{
			bResult = Base->IsImagePlainConstant(OutColour);
			if (bResult)
			{
				OutColour[0] = 1.0f - OutColour[0];
				OutColour[1] = 1.0f - OutColour[1];
				OutColour[2] = 1.0f - OutColour[2];
			}
		}

		return bResult;
	}


	FSourceDataDescriptor ASTOpImageInvert::GetSourceDataDescriptor(FGetSourceDataDescriptorContext* Context) const
	{
		if (Base)
		{
			return Base->GetSourceDataDescriptor(Context);
		}

		return {};
	}

}
