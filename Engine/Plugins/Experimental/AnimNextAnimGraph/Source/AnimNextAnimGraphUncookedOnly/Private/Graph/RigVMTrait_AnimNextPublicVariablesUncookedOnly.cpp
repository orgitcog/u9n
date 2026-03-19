// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMTrait_AnimNextPublicVariablesUncookedOnly.h"
#include "AnimNextRigVMAssetEditorData.h"
#include "UncookedOnlyUtils.h"
#include "Entries/AnimNextVariableEntry.h"
#include "Graph/RigVMTrait_AnimNextPublicVariables.h"
#include "RigVMModel/RigVMController.h"
#include "RigVMModel/RigVMPin.h"

namespace UE::AnimNext::UncookedOnly
{

void FPublicVariablesImpl::Register()
{
	FRigVMTrait_AnimNextPublicVariables::GetDisplayNameFunc = GetDisplayName;
	FRigVMTrait_AnimNextPublicVariables::GetProgrammaticPinsFunc = GetProgrammaticPins;
	FRigVMTrait_AnimNextPublicVariables::ShouldCreatePinForPropertyFunc = ShouldCreatePinForProperty;
}

FString FPublicVariablesImpl::GetDisplayName(const FRigVMTrait_AnimNextPublicVariables& InTrait)
{
	TStringBuilder<256> StringBuilder;
	StringBuilder.Appendf(TEXT("Variables: %s"), InTrait.InternalAsset ? *InTrait.InternalAsset->GetFName().ToString() : TEXT("None"));
	return StringBuilder.ToString();
}

void FPublicVariablesImpl::GetProgrammaticPins(const FRigVMTrait_AnimNextPublicVariables& InTrait, URigVMController* InController, int32 InParentPinIndex, const FString& InDefaultValue, FRigVMPinInfoArray& OutPinArray)
{
	if (InTrait.InternalAsset == nullptr)
	{
		return;
	}

	UAnimNextRigVMAssetEditorData* EditorData = UE::AnimNext::UncookedOnly::FUtils::GetEditorData<UAnimNextRigVMAssetEditorData>(InTrait.InternalAsset.Get());
	if(EditorData == nullptr)
	{
		return;
	}

	TArray<UAnimNextVariableEntry*> PublicVariables;
	EditorData->GetPublicVariables(PublicVariables);
	if(PublicVariables.Num() == 0)
	{
		return;
	}

	TArray<FPropertyBagPropertyDesc> Descs;
	Descs.Reserve(PublicVariables.Num());
	TArray<TConstArrayView<uint8>> Values;
	Values.Reserve(PublicVariables.Num());

	// Maintain trait sorted order
	for (FName VariableName : InTrait.InternalVariableNames)
	{
		auto FindMatchingName = [&](const UAnimNextVariableEntry* InVariable)
		{
			if (!InVariable)
			{
				return false;
			}

			return InVariable->GetEntryName() == VariableName;
		};

		if (UAnimNextVariableEntry** PublicVariableItr = PublicVariables.FindByPredicate(FindMatchingName))
		{
			if (UAnimNextVariableEntry* PublicVariable = *PublicVariableItr)
			{
				const FProperty* Property;
				TConstArrayView<uint8> Value;
				PublicVariable->GetDefaultValue(Property, Value);
				Descs.Emplace(PublicVariable->GetEntryName(), Property);
				Values.Emplace(Value);
			}
		}

	}

	ensure(Values.Num() == InTrait.InternalVariableNames.Num());

	FInstancedPropertyBag Defaults;
	Defaults.ReplaceAllPropertiesAndValues(Descs, Values);

	const TFunction<ERigVMPinDefaultValueType(const FName&)> DefaultValueTypeGetter = [](const FName& InPropertyName)
	{
		return ERigVMPinDefaultValueType::AutoDetect;
	};

	OutPinArray.AddPins(const_cast<UPropertyBag*>(Defaults.GetPropertyBagStruct()), InController, ERigVMPinDirection::Input, InParentPinIndex, DefaultValueTypeGetter, Defaults.GetValue().GetMemory(), true);
}

bool FPublicVariablesImpl::ShouldCreatePinForProperty(const FRigVMTrait_AnimNextPublicVariables& InTrait, const FProperty* InProperty)
{
	return
		InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(FRigVMTrait_AnimNextPublicVariables, InternalAsset) ||
		InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(FRigVMTrait_AnimNextPublicVariables, InternalVariableNames) ||
		InTrait.InternalVariableNames.Contains(InProperty->GetFName());
}

}