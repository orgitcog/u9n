// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpImageLuminance.h"

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

	ASTOpImageLuminance::ASTOpImageLuminance()
		: Base(this)
	{
	}


	ASTOpImageLuminance::~ASTOpImageLuminance()
	{
		// Explicit call needed to avoid recursive destruction
		ASTOp::RemoveChildren();
	}


	bool ASTOpImageLuminance::IsEqual(const ASTOp& InOther) const
	{
		if (InOther.GetOpType()==GetOpType())
		{
			const ASTOpImageLuminance* Other = static_cast<const ASTOpImageLuminance*>(&InOther);
			return Base == Other->Base;
		}
		return false;
	}


	uint64 ASTOpImageLuminance::Hash() const
	{
		uint64 Res = std::hash<EOpType>()(GetOpType());
		hash_combine(Res, Base.child().get());
		return Res;
	}


	Ptr<ASTOp> ASTOpImageLuminance::Clone(MapChildFuncRef MapChild) const
	{
		Ptr<ASTOpImageLuminance> New = new ASTOpImageLuminance();
		New->Base = MapChild(Base.child());
		return New;
	}


	void ASTOpImageLuminance::ForEachChild(const TFunctionRef<void(ASTChild&)> Func)
	{
		Func(Base);
	}


	void ASTOpImageLuminance::Link(FProgram& Program, FLinkerOptions*)
	{
		// Already linked?
		if (!linkedAddress)
		{
			OP::ImageLuminanceArgs Args;
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


	FImageDesc ASTOpImageLuminance::GetImageDesc(bool bReturnBestOption, FGetImageDescContext* Context) const
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


	Ptr<ImageSizeExpression> ASTOpImageLuminance::GetImageSizeExpression() const
	{
		if (Base)
		{
			return Base->GetImageSizeExpression();
		}

		return new ImageSizeExpression;
	}


	bool ASTOpImageLuminance::IsImagePlainConstant(FVector4f& OutColour) const
	{
		bool bResult = true;
		OutColour = FVector4f(0.0f,0.0f,0.0f,1.0f);

		if (Base)
		{
			bResult = Base->IsImagePlainConstant(OutColour);
			if (bResult)
			{
				float Luminance = OutColour[0] * 77.0f + OutColour[1] * 150.0f + OutColour[2] * 29.0f;
				Luminance /= 255.0f;
				OutColour = FVector4f(Luminance, Luminance, Luminance, OutColour[3]);
			}
		}

		return bResult;
	}


	FSourceDataDescriptor ASTOpImageLuminance::GetSourceDataDescriptor(FGetSourceDataDescriptorContext* Context) const
	{
		if (Base)
		{
			return Base->GetSourceDataDescriptor(Context);
		}

		return {};
	}

}
