// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RigVMCore/RigVMStruct.h"
#include "AnimNextExecuteContext.h"
#include "RigUnit_AnimNextBase.generated.h"

USTRUCT(meta=(ExecuteContext="FAnimNextExecuteContext"))
struct FRigUnit_AnimNextBase : public FRigVMStruct
{
	GENERATED_BODY()
};
