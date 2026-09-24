// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "ControlRigDefines.h"
#include "RigVMCore/RigVM.h"
#include "RigVMCore/RigVMGraphFunctionHost.h"
#include "RigVMBlueprintGeneratedClass.h"
#include "ControlRigBlueprintGeneratedClass.generated.h"

#define UE_API CONTROLRIG_API

UCLASS(MinimalAPI)
class UControlRigBlueprintGeneratedClass : public URigVMBlueprintGeneratedClass
{
	GENERATED_UCLASS_BODY()

public:

	// UObject interface
	UE_API void Serialize(FArchive& Ar);
};

#undef UE_API
