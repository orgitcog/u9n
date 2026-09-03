// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpImageCompose.h"

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "HAL/PlatformMath.h"
#include "MuR/Layout.h"
#include "MuR/ModelPrivate.h"
#include "MuR/MutableMath.h"
#include "MuR/MutableTrace.h"
#include "MuR/ImagePrivate.h"
#include "MuR/RefCounted.h"
#include "MuR/Types.h"
#include "MuT/ASTOpConstantResource.h"
#include "MuT/ASTOpImagePixelFormat.h"
#include "MuT/ASTOpImageResize.h"


namespace mu
{


//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
ASTOpImageCompose::ASTOpImageCompose()
    : Layout(this)
    , Base(this)
	, BlockImage(this)
	, Mask(this)
{
}


//-------------------------------------------------------------------------------------------------
ASTOpImageCompose::~ASTOpImageCompose()
{
    // Explicit call needed to avoid recursive destruction
    ASTOp::RemoveChildren();
}


//-------------------------------------------------------------------------------------------------
bool ASTOpImageCompose::IsEqual(const ASTOp& otherUntyped) const
{
	if (otherUntyped.GetOpType() == GetOpType())
    {
		const ASTOpImageCompose* other = static_cast<const ASTOpImageCompose*>(&otherUntyped);
		return Layout == other->Layout &&
			Base ==other->Base &&
			BlockImage == other->BlockImage &&
			Mask == other->Mask &&
			BlockId == other->BlockId;
    }
    return false;
}


//-------------------------------------------------------------------------------------------------
uint64 ASTOpImageCompose::Hash() const
{
	uint64 res = std::hash<EOpType>()(EOpType::IM_COMPOSE);
    hash_combine( res, Layout.child().get() );
	hash_combine(res, Base.child().get());
	hash_combine(res, BlockImage.child().get());
	hash_combine(res, Mask.child().get());
	hash_combine(res, BlockId);
	return res;
}


//-------------------------------------------------------------------------------------------------
mu::Ptr<ASTOp> ASTOpImageCompose::Clone(MapChildFuncRef mapChild) const
{
	mu::Ptr<ASTOpImageCompose> n = new ASTOpImageCompose();
    n->Layout = mapChild(Layout.child());
	n->Base = mapChild(Base.child());
	n->BlockImage = mapChild(BlockImage.child());
	n->Mask = mapChild(Mask.child());
	n->BlockId = BlockId;
    return n;
}


//-------------------------------------------------------------------------------------------------
void ASTOpImageCompose::ForEachChild(const TFunctionRef<void(ASTChild&)> f )
{
	f(Layout);
	f(Base);
	f(BlockImage);
	f(Mask);
}


//-------------------------------------------------------------------------------------------------
void ASTOpImageCompose::Link( FProgram& program, FLinkerOptions*)
{
    // Already linked?
    if (!linkedAddress)
    {
        OP::ImageComposeArgs Args;
		FMemory::Memzero(Args);

		if (Layout) Args.layout = Layout->linkedAddress;
		if (Base) Args.base = Base->linkedAddress;
		if (BlockImage) Args.blockImage = BlockImage->linkedAddress;
		if (Mask) Args.mask = Mask->linkedAddress;
        Args.BlockId = BlockId;

        linkedAddress = (OP::ADDRESS)program.OpAddress.Num();
        program.OpAddress.Add((uint32_t)program.ByteCode.Num());
        AppendCode(program.ByteCode,EOpType::IM_COMPOSE);
        AppendCode(program.ByteCode,Args);
    }

}


FImageDesc ASTOpImageCompose::GetImageDesc( bool returnBestOption, FGetImageDescContext* context ) const
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

    // Actual work
    if (Base)
    {
        res = Base->GetImageDesc( returnBestOption, context );
    }

	if (BlockImage)
	{
		FImageDesc BlockDesc = BlockImage->GetImageDesc(returnBestOption, context);
		res.m_format = GetMostGenericFormat(res.m_format,BlockDesc.m_format);
	}

    // Cache th result
    if (context)
    {
		context->m_results.Add(this, res);
	}

    return res;
}


mu::Ptr<ImageSizeExpression> ASTOpImageCompose::GetImageSizeExpression() const
{
    if ( Base )
    {
        return Base->GetImageSizeExpression();
    }

    return nullptr;
}


bool ASTOpImageCompose::IsImagePlainConstant(FVector4f& colour) const
{
	bool res = false;

	if (BlockImage.child())
	{
		BlockImage->IsImagePlainConstant(colour);
	}

	if (res && Base.child())
	{
		FVector4f baseColour;
		res = Base->IsImagePlainConstant(baseColour);
		res &= (colour == baseColour);
	}

	return res;
}


void ASTOpImageCompose::GetLayoutBlockSize(int32* pBlockX, int32* pBlockY)
{
	// Try to follow the base image of the compose, which is the most stable.
	if (Base)
	{
		Base->GetLayoutBlockSize(pBlockX,pBlockY);
	}

	// We can only follow the block if the base is empty, since the first block will set the block size.	
	if (*pBlockX == 0)
	{
		// Let's try the block approach: We need the block size and the layout blocks
		int32 layoutBlocksX = 0;
		int32 layoutBlocksY = 0;
		if (Layout.child())
		{
			MUTABLE_CPUPROFILER_SCOPE(GetLayoutBlockSize_GetBlockLayoutSize);
			FBlockLayoutSizeCache cache;
			Layout->GetBlockLayoutSizeCached(BlockId, &layoutBlocksX, &layoutBlocksY, &cache);
		}

		if (layoutBlocksX > 0 && layoutBlocksY > 0 && BlockImage.child())
		{
			FImageDesc blockDesc = BlockImage->GetImageDesc();

			*pBlockX = blockDesc.m_size[0] / layoutBlocksX;
			*pBlockY = blockDesc.m_size[1] / layoutBlocksY;
		}
		else
		{
			*pBlockX = 0;
			*pBlockY = 0;
		}
	}
}


mu::Ptr<ASTOp> ASTOpImageCompose::OptimiseSemantic(const FModelOptimizationOptions& options, int32 Pass) const
{
	mu::Ptr<ASTOp> at;

	mu::Ptr<ASTOp> baseAt = Base.child();
	mu::Ptr<ASTOp> blockAt = BlockImage.child();
	mu::Ptr<ASTOp> layoutAt = Layout.child();
	if (layoutAt
		&&
		layoutAt->GetOpType() == EOpType::LA_CONSTANT
		&&
		baseAt
		&&
		blockAt)
	{
		const ASTOpConstantResource* typedLayout = static_cast<const ASTOpConstantResource*>(layoutAt.get());
		const mu::FLayout* pLayout = static_cast<const mu::FLayout*>(typedLayout->GetValue().Get());

		// Constant single-block full layout?
		if (pLayout->GetBlockCount() == 1
			&&
			pLayout->Blocks[0].Min == FIntVector2(0, 0)
			&&
			pLayout->Blocks[0].Size == pLayout->Size
			&&
			pLayout->Blocks[0].Id == BlockId
			)
		{
			// We could only take the block, but we must make sure it will have the format
			// and size of the base.
			mu::FImageDesc BaseDesc = baseAt->GetImageDesc(true);
			mu::FImageDesc BlockDesc = blockAt->GetImageDesc(true);

			at = blockAt;

			// \todo isn't this a common operation?
			if (BaseDesc.m_format != BlockDesc.m_format
				&&
				BaseDesc.m_format != EImageFormat::None)
			{
				mu::Ptr<ASTOpImagePixelFormat> Reformat = new ASTOpImagePixelFormat;
				Reformat->Format = BaseDesc.m_format;
				Reformat->FormatIfAlpha = EImageFormat::None;
				Reformat->Source = at;
				at = Reformat;
			}

			if (BaseDesc.m_size != BlockDesc.m_size
				&&
				BaseDesc.m_size[0] != 0
				&&
				BaseDesc.m_size[1] != 0)
			{
				mu::Ptr<ASTOpImageResize> Resize = new ASTOpImageResize;
				Resize->Size[0] = BaseDesc.m_size[0];
				Resize->Size[1] = BaseDesc.m_size[1];
				Resize->Source = at;
				at = Resize;
			}
		}
	}

	return at;
}


FSourceDataDescriptor ASTOpImageCompose::GetSourceDataDescriptor(FGetSourceDataDescriptorContext* Context) const
{
	// Cache management
	TUniquePtr<FGetSourceDataDescriptorContext> LocalContext;
	if (!Context)
	{
		LocalContext.Reset(new FGetSourceDataDescriptorContext);
		Context = LocalContext.Get();
	}

	FSourceDataDescriptor* Found = Context->Cache.Find(this);
	if (Found)
	{
		return *Found;
	}

	// Not cached: calculate
	FSourceDataDescriptor Result;

	if (Base)
	{
		FSourceDataDescriptor SourceDesc = Base->GetSourceDataDescriptor(Context);
		Result.CombineWith(SourceDesc);
	}

	if (BlockImage)
	{
		FSourceDataDescriptor SourceDesc = BlockImage->GetSourceDataDescriptor(Context);
		Result.CombineWith(SourceDesc);
	}

	if (Mask)
	{
		FSourceDataDescriptor SourceDesc = Mask->GetSourceDataDescriptor(Context);
		Result.CombineWith(SourceDesc);
	}

	Context->Cache.Add(this, Result);

	return Result;
}


}