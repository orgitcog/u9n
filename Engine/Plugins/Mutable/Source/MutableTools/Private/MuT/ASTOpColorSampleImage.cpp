// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpColorSampleImage.h"

#include "HAL/PlatformMath.h"
#include "MuR/ModelPrivate.h"
#include "MuR/RefCounted.h"
#include "MuR/Types.h"

namespace mu
{

	ASTOpColorSampleImage::ASTOpColorSampleImage()
		: Image(this)
		, X(this)
		, Y(this)
	{
	}


	ASTOpColorSampleImage::~ASTOpColorSampleImage()
	{
		// Explicit call needed to avoid recursive destruction
		ASTOp::RemoveChildren();
	}


	bool ASTOpColorSampleImage::IsEqual(const ASTOp& OtherUntyped) const
	{
		if (OtherUntyped.GetOpType()==GetOpType())
		{
			const ASTOpColorSampleImage* Other = static_cast<const ASTOpColorSampleImage*>(&OtherUntyped);
			return Image == Other->Image &&
				X == Other->X &&
				Y == Other->Y &&
				Filter == Other->Filter;
		}
		return false;
	}


	uint64 ASTOpColorSampleImage::Hash() const
	{
		uint64 Result = std::hash<void*>()(Image.child().get());
		hash_combine(Result, X.child().get());
		hash_combine(Result, Y.child().get());
		hash_combine(Result, Filter);
		return Result;
	}


	Ptr<ASTOp> ASTOpColorSampleImage::Clone(MapChildFuncRef MapChild) const
	{
		Ptr<ASTOpColorSampleImage> New = new ASTOpColorSampleImage();
		New->Image = MapChild(Image.child());
		New->X = MapChild(X.child());
		New->Y = MapChild(Y.child());
		New->Filter = Filter;
		return New;
	}


	void ASTOpColorSampleImage::ForEachChild(const TFunctionRef<void(ASTChild&)> Func)
	{
		Func(Image);
		Func(X);
		Func(Y);
	}


	void ASTOpColorSampleImage::Link(FProgram& Program, FLinkerOptions*)
	{
		// Already linked?
		if (!linkedAddress)
		{
			OP::ColourSampleImageArgs Args;
			FMemory::Memzero(Args);

			if (Image) Args.Image = Image->linkedAddress;
			if (X) Args.X = X->linkedAddress;
			if (Y) Args.Y = Y->linkedAddress;
			Args.Filter = Filter;

			linkedAddress = (OP::ADDRESS)Program.OpAddress.Num();
			Program.OpAddress.Add(Program.ByteCode.Num());
			AppendCode(Program.ByteCode, GetOpType());
			AppendCode(Program.ByteCode, Args);
		}
	}
}
