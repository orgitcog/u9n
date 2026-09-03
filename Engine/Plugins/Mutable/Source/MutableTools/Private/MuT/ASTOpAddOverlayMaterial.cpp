// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpAddOverlayMaterial.h"

#include "HAL/PlatformMath.h"
#include "MuR/ModelPrivate.h"
#include "MuR/RefCounted.h"


namespace mu
{


//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
	ASTOpAddOverlayMaterial::ASTOpAddOverlayMaterial()
	: Instance(this)
	, OverlayMaterialId(this)
{
}


	ASTOpAddOverlayMaterial::~ASTOpAddOverlayMaterial()
{
	// Explicit call needed to avoid recursive destruction
	ASTOp::RemoveChildren();
}

bool ASTOpAddOverlayMaterial::IsEqual(const ASTOp& OtherUntyped) const
{
	if (OtherUntyped.GetOpType() == GetOpType())
	{
		const ASTOpAddOverlayMaterial* Other = static_cast<const ASTOpAddOverlayMaterial*>(&OtherUntyped);
		return Instance == Other->Instance
			&& OverlayMaterialId == Other->OverlayMaterialId;
	}

	return false;
}

uint64 ASTOpAddOverlayMaterial::Hash() const
{
	uint64 Result = std::hash<uint64>()(uint64(EOpType::IN_ADDOVERLAYMATERIAL));
	
	hash_combine(Result, Instance.child().get());
	hash_combine(Result, OverlayMaterialId.child().get());
	
	return Result;
}

mu::Ptr<ASTOp> ASTOpAddOverlayMaterial::Clone(MapChildFuncRef MapChild) const
{
	Ptr<ASTOpAddOverlayMaterial> NewInstance = new ASTOpAddOverlayMaterial();

	NewInstance->Instance = MapChild(Instance.child());
	NewInstance->OverlayMaterialId = MapChild(OverlayMaterialId.child());

	return NewInstance;
}

void ASTOpAddOverlayMaterial::ForEachChild(const TFunctionRef<void(ASTChild&)> F)
{
	F(Instance);
	F(OverlayMaterialId);
}

void ASTOpAddOverlayMaterial::Link(FProgram& Program, FLinkerOptions*)
{
	// Already linked?
	if (linkedAddress)
	{
		return;
	}

	OP::InstanceAddOverlayMaterialArgs Args;
	FMemory::Memzero(Args);

	if (Instance)
	{
		Args.Instance = Instance->linkedAddress;
	}

	if (OverlayMaterialId)
	{
		Args.OverlayMaterialId = OverlayMaterialId->linkedAddress;
	}
	
	linkedAddress = (OP::ADDRESS)Program.OpAddress.Num();
	Program.OpAddress.Add((uint32_t)Program.ByteCode.Num());
	AppendCode(Program.ByteCode, EOpType::IN_ADDOVERLAYMATERIAL);
	AppendCode(Program.ByteCode, Args);
}

}
