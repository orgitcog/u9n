// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChooserParameters.h"
#include "DataInterface/AnimNextDataInterfaceInstance.h"
#include "StructUtils/PropertyBag.h"

namespace UE::AnimNext::Private
{

// Helper function to find the first data interface instance in the context 
FAnimNextDataInterfaceInstance* GetFirstDataInterfaceInstance(FChooserEvaluationContext& Context)
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

bool FBoolAnimProperty::GetValue(FChooserEvaluationContext& Context, bool& OutResult) const
{
	if(FAnimNextDataInterfaceInstance* Instance = UE::AnimNext::Private::GetFirstDataInterfaceInstance(Context))
	{
		return Instance->GetVariable(VariableName, OutResult) == EPropertyBagResult::Success;
	}
	return false;
}

bool FBoolAnimProperty::SetValue(FChooserEvaluationContext& Context, bool InValue) const
{
	if(FAnimNextDataInterfaceInstance* Instance = UE::AnimNext::Private::GetFirstDataInterfaceInstance(Context))
	{
		return Instance->SetVariable(VariableName, InValue) == EPropertyBagResult::Success;
	}
	return false;
}

void FBoolAnimProperty::GetDisplayName(FText& OutName) const
{
	FString DisplayName = VariableName.ToString();
	int Index = -1;
	DisplayName.FindLastChar(':', Index);
	if (Index>=0)
	{
		DisplayName = DisplayName.RightChop(Index + 1);
	}
	
	OutName = FText::FromString(DisplayName);
}


bool FFloatAnimProperty::GetValue(FChooserEvaluationContext& Context, double& OutResult) const
{
	if(FAnimNextDataInterfaceInstance* Instance = UE::AnimNext::Private::GetFirstDataInterfaceInstance(Context))
	{
		return Instance->GetVariable(VariableName, OutResult) == EPropertyBagResult::Success;
	}
	return false;
}

bool FFloatAnimProperty::SetValue(FChooserEvaluationContext& Context, double InValue) const
{
	if(FAnimNextDataInterfaceInstance* Instance = UE::AnimNext::Private::GetFirstDataInterfaceInstance(Context))
 	{
 		return Instance->SetVariable(VariableName, InValue) == EPropertyBagResult::Success;
 	}
 	return false;
}

void FFloatAnimProperty::GetDisplayName(FText& OutName) const
{
	FString DisplayName = VariableName.ToString();
	int Index = -1;
	DisplayName.FindLastChar(':', Index);
	if (Index>=0)
	{
		DisplayName = DisplayName.RightChop(Index + 1);
	}
	
	OutName = FText::FromString(DisplayName);
}

bool FEnumAnimProperty::GetValue(FChooserEvaluationContext& Context, uint8& OutResult) const
{
	if(FAnimNextDataInterfaceInstance* Instance = UE::AnimNext::Private::GetFirstDataInterfaceInstance(Context))
	{
		return Instance->GetVariable(VariableName, OutResult) == EPropertyBagResult::Success;
	}
	return false;
}

bool FEnumAnimProperty::SetValue(FChooserEvaluationContext& Context, uint8 InValue) const
{
	if(FAnimNextDataInterfaceInstance* Instance = UE::AnimNext::Private::GetFirstDataInterfaceInstance(Context))
	{
		return Instance->SetVariable(VariableName, InValue) == EPropertyBagResult::Success;
	}
	return false;
}

void FEnumAnimProperty::GetDisplayName(FText& OutName) const
{
	FString DisplayName = VariableName.ToString();
	int Index = -1;
	DisplayName.FindLastChar(':', Index);
	if (Index>=0)
	{
		DisplayName = DisplayName.RightChop(Index + 1);
	}
	
	OutName = FText::FromString(DisplayName);
}
