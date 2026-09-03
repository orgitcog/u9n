// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FRigVMTrait_AnimNextPublicVariables;
class URigVMController;
struct FRigVMPinInfoArray;

namespace UE::AnimNext::UncookedOnly
{

// UncookedOnly-side impl for FRigVMTrait_AnimNextPublicVariables
struct ANIMNEXTANIMGRAPHUNCOOKEDONLY_API FPublicVariablesImpl
{
	static void Register();
	static FString GetDisplayName(const FRigVMTrait_AnimNextPublicVariables& InTrait);
	static void GetProgrammaticPins(const FRigVMTrait_AnimNextPublicVariables& InTrait, URigVMController* InController, int32 InParentPinIndex, const FString& InDefaultValue, FRigVMPinInfoArray& OutPinArray);
	static bool ShouldCreatePinForProperty(const FRigVMTrait_AnimNextPublicVariables& InTrait, const FProperty* InProperty);
};

}