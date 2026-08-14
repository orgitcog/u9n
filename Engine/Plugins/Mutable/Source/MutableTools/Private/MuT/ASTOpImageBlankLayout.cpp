// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpImageBlankLayout.h"

#include "Containers/Map.h"
#include "MuR/ModelPrivate.h"
#include "MuR/RefCounted.h"
#include "MuR/Types.h"
#include "MuT/ASTOpConditional.h"
#include "MuT/ASTOpSwitch.h"
#include "MuT/ASTOpImagePatch.h"
#include "MuT/ASTOpImagePixelFormat.h"
#include "MuT/ASTOpImageLayer.h"
#include "MuT/ASTOpImageLayerColor.h"
#include "MuT/ASTOpImageRasterMesh.h"


namespace mu
{

	ASTOpImageBlankLayout::ASTOpImageBlankLayout()
		: Layout(this)
	{
	}


	ASTOpImageBlankLayout::~ASTOpImageBlankLayout()
	{
		// Explicit call needed to avoid recursive destruction
		ASTOp::RemoveChildren();
	}


	bool ASTOpImageBlankLayout::IsEqual(const ASTOp& InOther) const
	{
		if (InOther.GetOpType()==GetOpType())
		{
			const ASTOpImageBlankLayout* Other = static_cast<const ASTOpImageBlankLayout*>(&InOther);
			return Layout == Other->Layout &&
				BlockSize == Other->BlockSize &&
				Format == Other->Format &&
				GenerateMipmaps == Other->GenerateMipmaps &&
				MipmapCount == Other->MipmapCount;
		}
		return false;
	}


	uint64 ASTOpImageBlankLayout::Hash() const
	{
		uint64 Result = std::hash<EOpType>()(GetOpType());
		hash_combine(Result, Layout.child().get());
		hash_combine(Result, BlockSize[0]);
		hash_combine(Result, BlockSize[1]);
		hash_combine(Result, Format);
		hash_combine(Result, GenerateMipmaps);
		hash_combine(Result, MipmapCount);
		return Result;
	}


	Ptr<ASTOp> ASTOpImageBlankLayout::Clone(MapChildFuncRef MapChild) const
	{
		Ptr<ASTOpImageBlankLayout> n = new ASTOpImageBlankLayout();
		n->Layout = MapChild(Layout.child());
		n->BlockSize = BlockSize;
		n->Format = Format;
		n->GenerateMipmaps = GenerateMipmaps;
		n->MipmapCount = MipmapCount;
		return n;
	}


	void ASTOpImageBlankLayout::ForEachChild(const TFunctionRef<void(ASTChild&)> Func)
	{
		Func(Layout);
	}


	void ASTOpImageBlankLayout::Link(FProgram& Program, FLinkerOptions*)
	{
		// Already linked?
		if (!linkedAddress)
		{
			OP::ImageBlankLayoutArgs Args;
			FMemory::Memzero(Args);

			if (Layout)
			{
				Args.Layout = Layout->linkedAddress;
			}

			Args.BlockSize[0] = BlockSize[0];
			Args.BlockSize[1] = BlockSize[1];
			Args.Format = Format;
			Args.GenerateMipmaps = GenerateMipmaps;
			Args.MipmapCount = MipmapCount;

			linkedAddress = (OP::ADDRESS)Program.OpAddress.Num();
			Program.OpAddress.Add(Program.ByteCode.Num());
			AppendCode(Program.ByteCode, GetOpType());
			AppendCode(Program.ByteCode, Args);
		}

	}


	FImageDesc ASTOpImageBlankLayout::GetImageDesc(bool bReturnBestOption, FGetImageDescContext* Context) const
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
	   // TODO: We would need to process the layout to find the grid size, and then use
		// the block size with it.
		Result.m_size = FImageSize(0, 0);
		Result.m_format = Format;

		// Cache the result
		if (Context)
		{
			Context->m_results.Add(this, Result);
		}

		return Result;
	}


	Ptr<ImageSizeExpression> ASTOpImageBlankLayout::GetImageSizeExpression() const
	{
		Ptr<ImageSizeExpression> Res = new ImageSizeExpression;
		Res->type = ImageSizeExpression::ISET_LAYOUTFACTOR;
		Res->layout = Layout.child();
		Res->factor[0] = BlockSize[0];
		Res->factor[1] = BlockSize[1];
		return Res;
	}


	void ASTOpImageBlankLayout::GetLayoutBlockSize(int32* OutBlockX, int32* OutBlockY)
	{
		*OutBlockX = int32(BlockSize[0]);
		*OutBlockY = int32(BlockSize[1]);
	}


	bool ASTOpImageBlankLayout::IsImagePlainConstant(FVector4f& OutColour) const
	{
		OutColour[0] = 0;
		OutColour[1] = 0;
		OutColour[2] = 0;
		OutColour[3] = 0;
		return true;
	}

}
