// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpColorFromScalars.h"

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

	ASTOpColorFromScalars::ASTOpColorFromScalars()
		: V{ ASTChild(this),ASTChild(this),ASTChild(this),ASTChild(this) }
	{
	}


	ASTOpColorFromScalars::~ASTOpColorFromScalars()
	{
		// Explicit call needed to avoid recursive destruction
		ASTOp::RemoveChildren();
	}


	bool ASTOpColorFromScalars::IsEqual(const ASTOp& otherUntyped) const
	{
		if (otherUntyped.GetOpType()==GetOpType())
		{
			const ASTOpColorFromScalars* Other = static_cast<const ASTOpColorFromScalars*>(&otherUntyped);
			for (int32 i = 0; i<MUTABLE_OP_MAX_SWIZZLE_CHANNELS; ++i)
			{
				if (!(V[i] == Other->V[i]))
				{
					return false;
				}
			}
			return true;
		}
		return false;
	}


	uint64 ASTOpColorFromScalars::Hash() const
	{
		uint64 res = std::hash<void*>()(V[0].child().get());
		hash_combine(res, std::hash<void*>()(V[1].child().get()));
		hash_combine(res, std::hash<void*>()(V[2].child().get()));
		hash_combine(res, std::hash<void*>()(V[3].child().get()));
		return res;
	}


	mu::Ptr<ASTOp> ASTOpColorFromScalars::Clone(MapChildFuncRef mapChild) const
	{
		mu::Ptr<ASTOpColorFromScalars> n = new ASTOpColorFromScalars();
		for (int32 i = 0; i < MUTABLE_OP_MAX_SWIZZLE_CHANNELS; ++i)
		{
			n->V[i] = mapChild(V[i].child());
		}
		return n;
	}


	void ASTOpColorFromScalars::ForEachChild(const TFunctionRef<void(ASTChild&)> f)
	{
		for (int32 i = 0; i < MUTABLE_OP_MAX_SWIZZLE_CHANNELS; ++i)
		{
			f(V[i]);
		}
	}


	void ASTOpColorFromScalars::Link(FProgram& program, FLinkerOptions*)
	{
		// Already linked?
		if (!linkedAddress)
		{
			OP::ColourFromScalarsArgs Args;
			FMemory::Memzero(Args);

			for (int32 i = 0; i < MUTABLE_OP_MAX_SWIZZLE_CHANNELS; ++i)
			{
				if (V[i]) Args.V[i] = V[i]->linkedAddress;
			}			

			linkedAddress = (OP::ADDRESS)program.OpAddress.Num();
			program.OpAddress.Add((uint32)program.ByteCode.Num());
			AppendCode(program.ByteCode, GetOpType());
			AppendCode(program.ByteCode, Args);
		}
	}

}
