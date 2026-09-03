// Copyright Epic Games, Inc. All Rights Reserved.

#include "InjectionDataInterfaceHostAdapter.h"
#include "Graph/AnimNextAnimationGraph.h"
#include "Graph/AnimNextAnimGraph.h"
#include "Graph/AnimNextGraphInstance.h"

namespace UE::AnimNext
{

const UAnimNextDataInterface* FInjectionDataInterfaceHostAdapter::GetDataInterface() const
{
	return HostInstance->GetAnimationGraph();
}

uint8* FInjectionDataInterfaceHostAdapter::GetMemoryForVariable(int32 InVariableIndex, FName InVariableName, const FProperty* InVariableProperty) const
{
	if(InVariableName != Name)
	{
		return nullptr;
	}

	const FStructProperty* StructProperty = CastField<FStructProperty>(InVariableProperty);
	if(StructProperty == nullptr || StructProperty->Struct != FAnimNextAnimGraph::StaticStruct())
	{
		return nullptr;
	}

	return GraphInstance.GetMemory();
}

}
