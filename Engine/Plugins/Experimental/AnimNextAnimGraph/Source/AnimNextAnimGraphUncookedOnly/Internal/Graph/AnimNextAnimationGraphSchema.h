// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimNextRigVMAssetSchema.h"
#include "AnimNextAnimationGraphSchema.generated.h"

UCLASS(MinimalAPI)
class UAnimNextAnimationGraphSchema : public UAnimNextRigVMAssetSchema
{
	GENERATED_BODY()

public:
	// Schema reserved for future filtering of units etc.

	// Returns true if a node supports setting a node title
	virtual bool CanSetNodeTitle(URigVMController* InController, const URigVMNode* InNode) const override;

	// Returns true if a pin should be unfolded (represented by its subpins)
	virtual bool CanUnfoldPin(URigVMController* InController, const URigVMPin* InPinToUnfold) const override;
};
