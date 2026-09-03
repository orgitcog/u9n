// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/ASTOpAddExtensionData.h"

#include "HAL/PlatformMath.h"
#include "MuR/ModelPrivate.h"
#include "MuR/RefCounted.h"


namespace mu
{


//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
ASTOpAddExtensionData::ASTOpAddExtensionData()
	: Instance(this)
	, ExtensionData(this)
{
}


ASTOpAddExtensionData::~ASTOpAddExtensionData()
{
	// Explicit call needed to avoid recursive destruction
	ASTOp::RemoveChildren();
}

bool ASTOpAddExtensionData::IsEqual(const ASTOp& OtherUntyped) const
{
	if (OtherUntyped.GetOpType() == GetOpType())
	{
		const ASTOpAddExtensionData* Other = static_cast<const ASTOpAddExtensionData*>(&OtherUntyped);
		return Instance == Other->Instance
			&& ExtensionData == Other->ExtensionData
			&& ExtensionDataName == Other->ExtensionDataName;
	}

	return false;
}

uint64 ASTOpAddExtensionData::Hash() const
{
	uint64 Result = std::hash<uint64>()(uint64(EOpType::IN_ADDEXTENSIONDATA));
	
	hash_combine(Result, Instance.child().get());
	hash_combine(Result, ExtensionData.child().get());
	
	return Result;
}

mu::Ptr<ASTOp> ASTOpAddExtensionData::Clone(MapChildFuncRef MapChild) const
{
	Ptr<ASTOpAddExtensionData> NewInstance = new ASTOpAddExtensionData();

	NewInstance->Instance = MapChild(Instance.child());
	NewInstance->ExtensionData = MapChild(ExtensionData.child());
	NewInstance->ExtensionDataName = ExtensionDataName;

	return NewInstance;
}

void ASTOpAddExtensionData::ForEachChild(const TFunctionRef<void(ASTChild&)> F)
{
	F(Instance);
	F(ExtensionData);
}

void ASTOpAddExtensionData::Link(FProgram& Program, FLinkerOptions*)
{
	// Already linked?
	if (linkedAddress)
	{
		return;
	}

	OP::InstanceAddExtensionDataArgs Args;
	FMemory::Memzero(Args);

	if (!Instance || !Instance->linkedAddress)
	{
		// Can happen if there's no reference skeletal mesh in the first component
		return;
	}

	Args.Instance = Instance->linkedAddress;

	check(ExtensionData->linkedAddress);
	Args.ExtensionData = ExtensionData->linkedAddress;

	check(ExtensionDataName.Len() > 0);
	Args.ExtensionDataName = Program.AddConstant(ExtensionDataName);

	linkedAddress = (OP::ADDRESS)Program.OpAddress.Num();
	Program.OpAddress.Add((uint32_t)Program.ByteCode.Num());
	AppendCode(Program.ByteCode, EOpType::IN_ADDEXTENSIONDATA);
	AppendCode(Program.ByteCode, Args);
}

}
