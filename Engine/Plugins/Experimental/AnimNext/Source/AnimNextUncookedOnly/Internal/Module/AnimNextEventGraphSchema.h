// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimNextRigVMAssetSchema.h"
#include "AnimNextEventGraphSchema.generated.h"

UCLASS(MinimalAPI)
class UAnimNextEventGraphSchema : public UAnimNextRigVMAssetSchema
{
	GENERATED_BODY()

	// URigVMSchema interface
	virtual bool SupportsUnitFunction(URigVMController* InController, const FRigVMFunction* InUnitFunction) const override;
};

