// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpImagePixelFormat.h"

#include "Containers/Map.h"
#include "HAL/PlatformMath.h"
#include "HAL/UnrealMemory.h"
#include "Misc/AssertionMacros.h"
#include "MuR/ImagePrivate.h"
#include "MuR/ModelPrivate.h"
#include "MuR/RefCounted.h"
#include "MuR/Types.h"
#include "MuT/ASTOpConditional.h"
#include "MuT/ASTOpImageCompose.h"
#include "MuT/ASTOpImageMipmap.h"
#include "MuT/ASTOpImagePatch.h"
#include "MuT/ASTOpImageLayer.h"
#include "MuT/ASTOpImageLayerColor.h"
#include "MuT/ASTOpImageRasterMesh.h"
#include "MuT/ASTOpImageBlankLayout.h"
#include "MuT/ASTOpImageInterpolate.h"
#include "MuT/ASTOpImagePlainColor.h"
#include "MuT/ASTOpImageDisplace.h"
#include "MuT/ASTOpImageInvert.h"
#include "MuT/ASTOpSwitch.h"
#include "MuT/CompilerPrivate.h"

namespace mu
{

	template <class SCALAR> class vec4;

	//-------------------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------------------
	//-------------------------------------------------------------------------------------------------
	ASTOpImagePixelFormat::ASTOpImagePixelFormat()
		: Source(this)
	{
	}


	ASTOpImagePixelFormat::~ASTOpImagePixelFormat()
	{
		// Explicit call needed to avoid recursive destruction
		ASTOp::RemoveChildren();
	}


	bool ASTOpImagePixelFormat::IsEqual(const ASTOp& otherUntyped) const
	{
		if (otherUntyped.GetOpType()==GetOpType())
		{
			const ASTOpImagePixelFormat* other = static_cast<const ASTOpImagePixelFormat*>(&otherUntyped);
			return Source == other->Source && Format == other->Format && FormatIfAlpha == other->FormatIfAlpha;
		}
		return false;
	}


	uint64 ASTOpImagePixelFormat::Hash() const
	{
		uint64 res = std::hash<void*>()(Source.child().get());
		hash_combine(res, Format);
		return res;
	}


	mu::Ptr<ASTOp> ASTOpImagePixelFormat::Clone(MapChildFuncRef mapChild) const
	{
		mu::Ptr<ASTOpImagePixelFormat> n = new ASTOpImagePixelFormat();
		n->Source = mapChild(Source.child());
		n->Format = Format;
		n->FormatIfAlpha = FormatIfAlpha;
		return n;
	}


	void ASTOpImagePixelFormat::ForEachChild(const TFunctionRef<void(ASTChild&)> f)
	{
		f(Source);
	}


	void ASTOpImagePixelFormat::Link(FProgram& program, FLinkerOptions*)
	{
		// Already linked?
		if (!linkedAddress)
		{
			OP::ImagePixelFormatArgs Args;
			FMemory::Memzero(Args);

			Args.format = Format;
			Args.formatIfAlpha = FormatIfAlpha;
			if (Source) Args.source = Source->linkedAddress;

			linkedAddress = (OP::ADDRESS)program.OpAddress.Num();
			//program.m_code.push_back(op);
			program.OpAddress.Add((uint32_t)program.ByteCode.Num());
			AppendCode(program.ByteCode, EOpType::IM_PIXELFORMAT);
			AppendCode(program.ByteCode, Args);
		}

	}


	Ptr<ASTOp> ASTOpImagePixelFormat::OptimiseSemantic(const FModelOptimizationOptions& options, int32 Pass) const
	{
		mu::Ptr<ASTOp> NewOp;

		// Skip this operation if the source op format is already the one we want.
		if (Source)
		{
			FImageDesc SourceDesc = Source->GetImageDesc();
			if (SourceDesc.m_format==Format)
			{
				NewOp = Source.child();
			}
		}

		return NewOp;
	}


	Ptr<ASTOp> ASTOpImagePixelFormat::OptimiseSink(const FModelOptimizationOptions& options, FOptimizeSinkContext& context) const
	{
		Ptr<ASTOp> at;

		Ptr<ASTOp> sourceAt = Source.child();

		EImageFormat format = Format;
		bool bIsCompressedFormat = IsCompressedFormat(Format);
		//bool isBlockFormat = GetImageFormatData( format ).PixelsPerBlockX!=0;

		// The instruction can be sunk
		EOpType sourceType = sourceAt ? sourceAt->GetOpType() : EOpType::NONE;
		switch (sourceType)
		{
		case EOpType::IM_PIXELFORMAT:
		{
			// Keep only the top pixel format
			const ASTOpImagePixelFormat* typedSource = static_cast<const ASTOpImagePixelFormat*>(sourceAt.get());
			mu::Ptr<ASTOpImagePixelFormat> formatOp = mu::Clone<ASTOpImagePixelFormat>(this);
			formatOp->Source = typedSource->Source.child();
			at = formatOp;
			break;
		}

		case EOpType::IM_DISPLACE:
		{
			// This op doesn't support compressed formats
			if (!bIsCompressedFormat)
			{
				mu::Ptr<ASTOpImageDisplace> newOp = mu::Clone<ASTOpImageDisplace>(sourceAt);

				mu::Ptr<ASTOpImagePixelFormat> fop = mu::Clone<ASTOpImagePixelFormat>(this);
				fop->Source = newOp->Source.child();
				newOp->Source = fop;

				at = newOp;
			}
			break;
		}

		case EOpType::IM_INVERT:
		{
			// This op doesn't support compressed formats
			if (!bIsCompressedFormat)
			{
				mu::Ptr<ASTOpImageInvert> newOp = mu::Clone<ASTOpImageInvert>(sourceAt);

				mu::Ptr<ASTOpImagePixelFormat> fop = mu::Clone<ASTOpImagePixelFormat>(this);
				fop->Source = newOp->Base.child();
				newOp->Base = fop;

				at = newOp;
			}
			break;
		}

		case EOpType::IM_RASTERMESH:
		{
			// This op doesn't support compressed formats
			if (!bIsCompressedFormat
				&&
				static_cast<const ASTOpImageRasterMesh*>(sourceAt.get())->image)
			{
				Ptr<ASTOpImageRasterMesh> newOp = mu::Clone<ASTOpImageRasterMesh>(sourceAt);

				mu::Ptr<ASTOpImagePixelFormat> fop = mu::Clone<ASTOpImagePixelFormat>(this);
				fop->Source = newOp->image.child();
				newOp->image = fop;

				at = newOp;
			}
			break;
		}

		case EOpType::IM_BLANKLAYOUT:
		{
			// Just make sure the layout format is the right one and forget the op
			Ptr<ASTOpImageBlankLayout> NewOp = mu::Clone<ASTOpImageBlankLayout>(sourceAt);

			EImageFormat layoutFormat = NewOp->Format;
			if (FormatIfAlpha != EImageFormat::None
				&&
				GetImageFormatData(layoutFormat).Channels > 3)
			{
				format = FormatIfAlpha;
			}

			NewOp->Format = format;
			at = NewOp;
			break;
		}

		case EOpType::IM_PLAINCOLOUR:
		{
			// Just make sure the format is the right one and forget the op
			Ptr<ASTOpImagePlainColor> NewOp = mu::Clone<ASTOpImagePlainColor>(sourceAt);

			EImageFormat LayoutFormat = NewOp->Format;
			if (FormatIfAlpha != EImageFormat::None
				&&
				GetImageFormatData(LayoutFormat).Channels > 3)
			{
				format = FormatIfAlpha;
			}

			NewOp->Format = format;
			at = NewOp;
			break;
		}

		default:
		{

			at = context.ImagePixelFormatSinker.Apply(this);

			break;
		} // pixelformat source default

		}

		return at;
	}


	//!
	FImageDesc ASTOpImagePixelFormat::GetImageDesc(bool returnBestOption, FGetImageDescContext* context) const
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

		if (Source.child())
		{
			res = Source.child()->GetImageDesc(returnBestOption, context);
		}

		if (FormatIfAlpha != EImageFormat::None
			&&
			GetImageFormatData(res.m_format).Channels > 3)
		{
			res.m_format = FormatIfAlpha;
		}
		else
		{
			res.m_format = Format;
		}
		check(res.m_format != EImageFormat::None);


		// Cache the result
		if (context)
		{
			context->m_results.Add(this, res);
		}

		return res;
	}


	void ASTOpImagePixelFormat::GetLayoutBlockSize(int* pBlockX, int* pBlockY)
	{
		if (Source.child())
		{
			Source.child()->GetLayoutBlockSize(pBlockX, pBlockY);
		}
	}


	bool ASTOpImagePixelFormat::IsImagePlainConstant(FVector4f& colour) const
	{
		bool res = false;
		if (Source.child())
		{
			Source.child()->IsImagePlainConstant(colour);
		}
		return res;
	}


	mu::Ptr<ImageSizeExpression> ASTOpImagePixelFormat::GetImageSizeExpression() const
	{
		mu::Ptr<ImageSizeExpression> pRes;

		if (Source.child())
		{
			pRes = Source.child()->GetImageSizeExpression();
		}
		else
		{
			pRes = new ImageSizeExpression;
		}

		return pRes;
	}


	FSourceDataDescriptor ASTOpImagePixelFormat::GetSourceDataDescriptor(FGetSourceDataDescriptorContext* Context) const
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
	mu::Ptr<ASTOp> Sink_ImagePixelFormatAST::Apply(const ASTOpImagePixelFormat* root)
	{
		Root = root;
		OldToNew.Reset();

		check(root->GetOpType() == EOpType::IM_PIXELFORMAT);

		InitialSource = Root->Source.child();
		mu::Ptr<ASTOp> newSource = Visit(InitialSource, Root);

		Root = nullptr;

		// If there is any change, it is the new root.
		if (newSource != InitialSource)
		{
			return newSource;
		}

		return nullptr;
	}


	//---------------------------------------------------------------------------------------------
	mu::Ptr<ASTOp> Sink_ImagePixelFormatAST::Visit(mu::Ptr<ASTOp> at, const ASTOpImagePixelFormat* currentFormatOp)
	{
		if (!at) return nullptr;

		EImageFormat format = currentFormatOp->Format;
		bool bIsCompressedFormat = IsCompressedFormat(format);
		bool isBlockFormat = GetImageFormatData(format).PixelsPerBlockX != 0;

		// Already visited?
		const Ptr<ASTOp>* Cached = OldToNew.Find({ at,currentFormatOp });
		if (Cached)
		{
			return *Cached;
		}

		mu::Ptr<ASTOp> newAt = at;
		switch (at->GetOpType())
		{

		case EOpType::IM_CONDITIONAL:
		{
			// We move the op down the two paths
			Ptr<ASTOpConditional> newOp = mu::Clone<ASTOpConditional>(at);
			newOp->yes = Visit(newOp->yes.child(), currentFormatOp);
			newOp->no = Visit(newOp->no.child(), currentFormatOp);
			newAt = newOp;
			break;
		}

		case EOpType::IM_SWITCH:
		{
			// We move the op down all the paths
			Ptr<ASTOpSwitch> newOp = mu::Clone<ASTOpSwitch>(at);
			newOp->Default = Visit(newOp->Default.child(), currentFormatOp);
			for (ASTOpSwitch::FCase& c : newOp->Cases)
			{
				c.Branch = Visit(c.Branch.child(), currentFormatOp);
			}
			newAt = newOp;
			break;
		}

		case EOpType::IM_COMPOSE:
		{
			if (isBlockFormat)
			{
				// Since blocks can be resized at runtime anyway, push the format down and rely on reformatting on the fly if necessary.

				// We move the format down the two paths
				Ptr<ASTOpImageCompose> newOp = mu::Clone<ASTOpImageCompose>(at);

				// TODO: We have to make sure we don't end up with two different formats if
				// there is an formatIfAlpha

				Ptr<ASTOp> baseOp = newOp->Base.child();
				newOp->Base = Visit(baseOp, currentFormatOp);

				Ptr<ASTOp> blockOp = newOp->BlockImage.child();
				newOp->BlockImage = Visit(blockOp, currentFormatOp);

				newAt = newOp;
			}

			break;
		}

		case EOpType::IM_PATCH:
		{
			if (isBlockFormat)
			{
				// We move the format down the two paths
				Ptr<ASTOpImagePatch> newOp = mu::Clone<ASTOpImagePatch>(at);

				newOp->base = Visit(newOp->base.child(), currentFormatOp);
				newOp->patch = Visit(newOp->patch.child(), currentFormatOp);

				newAt = newOp;
			}

			break;
		}

		case EOpType::IM_MIPMAP:
		{
			const ASTOpImageMipmap* typedSource = static_cast<const ASTOpImageMipmap*>(at.get());

			// If its a compressed format, only sink formats on mipmap operations that
			// generate the tail. To avoid optimization loop.
			if (!bIsCompressedFormat || typedSource->bOnlyTail)
			{
				Ptr<ASTOpImageMipmap> newOp = mu::Clone<ASTOpImageMipmap>(typedSource);

				Ptr<ASTOp> baseOp = newOp->Source.child();
				newOp->Source = Visit(baseOp, currentFormatOp);

				newAt = newOp;
			}
			break;
		}

		case EOpType::IM_INTERPOLATE:
		{
			// This op doesn't support compressed formats
			if (!bIsCompressedFormat)
			{
				// Move the format down all the paths
				Ptr<ASTOpImageInterpolate> newOp = mu::Clone<ASTOpImageInterpolate>(at);

				for (int v = 0; v < MUTABLE_OP_MAX_INTERPOLATE_COUNT; ++v)
				{
					Ptr<ASTOp> Child = newOp->Targets[v].child();
					Ptr<ASTOp> FormattedChild = Visit(Child, currentFormatOp);
					newOp->Targets[v] = FormattedChild;
				}

				newAt = newOp;
			}
			break;
		}

		case EOpType::IM_LAYER:
		{
			if (GetOpToolsDesc(at->GetOpType()).bSupportedBasePixelFormats[(size_t)format])
			{
				// We move the format down the two paths
				Ptr<ASTOpImageLayer> nop = mu::Clone<ASTOpImageLayer>(at);

				nop->base = Visit(nop->base.child(), currentFormatOp);
				nop->blend = Visit(nop->blend.child(), currentFormatOp);

				newAt = nop;
			}
			break;
		}

		case EOpType::IM_LAYERCOLOUR:
		{
			if (GetOpToolsDesc(at->GetOpType()).bSupportedBasePixelFormats[(size_t)format])
			{
				// We move the format down the base
				Ptr<ASTOpImageLayerColor> nop = mu::Clone<ASTOpImageLayerColor>(at);

				nop->base = Visit(nop->base.child(), currentFormatOp);

				newAt = nop;
			}
			break;
		}


		default:
			break;
		}

		// end on tree branch, replace with format
		if (at == newAt && at != InitialSource)
		{
			mu::Ptr<ASTOpImagePixelFormat> newOp = mu::Clone<ASTOpImagePixelFormat>(currentFormatOp);
			check(newOp->GetOpType() == EOpType::IM_PIXELFORMAT);

			newOp->Source = at;

			newAt = newOp;
		}

		OldToNew.Add({ at, currentFormatOp }, newAt);

		return newAt;
	}

}
