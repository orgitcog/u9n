// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseHistoryChooserParameter.h"
#include "DataInterface/AnimNextDataInterfaceInstance.h"
#include "StructUtils/PropertyBag.h"

namespace 
{

// Helper function to find the first data interface instance in the context 
FAnimNextDataInterfaceInstance* GetFirstAnimNextDataInterfaceInstance(FChooserEvaluationContext& Context)
{
	for(const FStructView& Param : Context.Params)
	{
		if(Param.GetScriptStruct() == FAnimNextDataInterfaceInstance::StaticStruct())
		{
			return Param.GetPtr<FAnimNextDataInterfaceInstance>();
		}
	}

	return nullptr;
}

}

bool FPoseHistoryAnimProperty::GetValue(FChooserEvaluationContext& Context, FPoseHistoryReference& OutResult) const
{
	if(FAnimNextDataInterfaceInstance* Instance = GetFirstAnimNextDataInterfaceInstance(Context))
	{
		return Instance->GetVariable(VariableName, OutResult) == EPropertyBagResult::Success;
	}
	return false;
}
