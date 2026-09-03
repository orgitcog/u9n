// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/RigVMTrait_AnimNextPublicVariables.h"
#include "AnimNextRigVMAsset.h"
#include "Logging/StructuredLog.h"

#if WITH_EDITOR
FString (*FRigVMTrait_AnimNextPublicVariables::GetDisplayNameFunc)(const FRigVMTrait_AnimNextPublicVariables& InTrait);
void (*FRigVMTrait_AnimNextPublicVariables::GetProgrammaticPinsFunc)(const FRigVMTrait_AnimNextPublicVariables& InTrait, URigVMController* InController, int32 InParentPinIndex, const FString& InDefaultValue, FRigVMPinInfoArray& OutPinArray);
bool (*FRigVMTrait_AnimNextPublicVariables::ShouldCreatePinForPropertyFunc)(const FRigVMTrait_AnimNextPublicVariables& InTrait, const FProperty* InProperty);

FString FRigVMTrait_AnimNextPublicVariables::GetDisplayName() const
{
	check(GetDisplayNameFunc);
	return GetDisplayNameFunc(*this);
}

void FRigVMTrait_AnimNextPublicVariables::GetProgrammaticPins(URigVMController* InController, int32 InParentPinIndex, const URigVMPin* InTraitPin, const FString& InDefaultValue, FRigVMPinInfoArray& OutPinArray) const
{
	check(GetProgrammaticPinsFunc);
	GetProgrammaticPinsFunc(*this, InController, InParentPinIndex, InDefaultValue, OutPinArray);
}

bool FRigVMTrait_AnimNextPublicVariables::ShouldCreatePinForProperty(const FProperty* InProperty) const
{
	check(ShouldCreatePinForPropertyFunc);
	return ShouldCreatePinForPropertyFunc(*this, InProperty);
}
#endif

namespace UE::AnimNext
{

const UAnimNextDataInterface* FPublicVariablesTraitToDataInterfaceHostAdapter::GetDataInterface() const
{
	return Trait.InternalAsset;
}

uint8* FPublicVariablesTraitToDataInterfaceHostAdapter::GetMemoryForVariable(int32 InVariableIndex, FName InVariableName, const FProperty* InVariableProperty) const
{
	// Note we dont use InVariableIndex here as we may not have bound all variables in an interface

	int32 TraitVariableIndex = Trait.InternalVariableNames.Find(InVariableName);
	if(TraitVariableIndex == INDEX_NONE)
	{
		// Variable not bound here
		return nullptr;
	}

	if(!TraitScope.GetAdditionalMemoryHandles().IsValidIndex(TraitVariableIndex))
	{
		// Memory handle is out of bounds
		// If this ensure fires then we have a mismatch between the variable names and the compiled memory handles, indicating a bug with the
		// compilation of trait additional memory handles (programmatic pins)
		ensure(false);
		return nullptr;
	}

	const FRigVMMemoryHandle& MemoryHandle = TraitScope.GetAdditionalMemoryHandles()[TraitVariableIndex];
	if(InVariableProperty->GetClass() != MemoryHandle.GetProperty()->GetClass())
	{
		UE_LOGFMT(LogAnimation, Error, "FPublicVariablesTraitToDataInterfaceHostAdapter::GetMemoryForVariable: Mismatched variable types: {Name}:{Type} vs {OtherType}", InVariableName, InVariableProperty->GetFName(), MemoryHandle.GetProperty()->GetFName());
		return nullptr;
	}

	return const_cast<uint8*>(MemoryHandle.GetData());
}

}