// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataInterface/DataInterfaceStructAdapter.h"

#include "Logging/StructuredLog.h"

namespace UE::AnimNext
{

const UAnimNextDataInterface* FDataInterfaceStructAdapter::GetDataInterface() const
{
	return DataInterface;
}

uint8* FDataInterfaceStructAdapter::GetMemoryForVariable(int32 InVariableIndex, FName InVariableName, const FProperty* InVariableProperty) const
{
	const UScriptStruct* Struct = StructView.GetScriptStruct();
	if(Struct == nullptr)
	{
		return nullptr;
	}

	const FProperty* Property = Struct->FindPropertyByName(InVariableName);
	if(Property == nullptr)
	{
		return nullptr;
	}

	if(Property->GetClass() != InVariableProperty->GetClass())
	{
		UE_LOGFMT(LogAnimation, Error, "FDataInterfaceStructAdapter::GetMemoryForVariable: Mismatched variable types: {Name}:{Type} vs {OtherType} in '{Host}'", InVariableName, Property->GetFName(), InVariableProperty->GetFName(), GetDataInterfaceName());
		return nullptr;
	}

	return Property->ContainerPtrToValuePtr<uint8>(StructView.GetMemory());
}

}
