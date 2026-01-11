// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpConstantColor.h"

#include "HAL/PlatformMath.h"
#include "MuR/ModelPrivate.h"
#include "MuR/RefCounted.h"
#include "MuR/Types.h"


namespace mu
{

	void ASTOpConstantColor::ForEachChild(const TFunctionRef<void(ASTChild&)>)
	{
	}


	bool ASTOpConstantColor::IsEqual(const ASTOp& OtherUntyped) const
	{
		if (OtherUntyped.GetOpType() == GetOpType())
		{
			const ASTOpConstantColor* other = static_cast<const ASTOpConstantColor*>(&OtherUntyped);

			// HACK: We encode an invalid value (Nan) for table option "None".
			// This nan comparison is needed because some compilers will return that 0.0f is equal to Nan...
			if (FMath::IsNaN(Value[0]) || FMath::IsNaN(other->Value[0]))
			{
				return FMath::IsNaN(Value[0]) == FMath::IsNaN(other->Value[0]);
			}

			return Value == other->Value;
		}
		return false;
	}


	uint64 ASTOpConstantColor::Hash() const
	{
		uint64 Result = std::hash<uint64>()(uint64(GetOpType()));
		hash_combine(Result, Value[0]);
		hash_combine(Result, Value[1]);
		hash_combine(Result, Value[2]);
		hash_combine(Result, Value[3]);
		return Result;
	}


	mu::Ptr<ASTOp> ASTOpConstantColor::Clone(MapChildFuncRef) const
	{
		Ptr<ASTOpConstantColor> New = new ASTOpConstantColor();
		New->Value = Value;
		return New;
	}


	void ASTOpConstantColor::Link(FProgram& Program, FLinkerOptions*)
	{
		if (!linkedAddress)
		{
			OP::ColorConstantArgs Args;
			FMemory::Memzero(Args);
			Args.Value = Value;

			linkedAddress = (OP::ADDRESS)Program.OpAddress.Num();
			Program.OpAddress.Add(Program.ByteCode.Num());
			AppendCode(Program.ByteCode, GetOpType());
			AppendCode(Program.ByteCode, Args);
		}
	}


	bool ASTOpConstantColor::IsColourConstant(FVector4f& OutColour) const
	{
		OutColour = Value;
		return true;
	}

}
