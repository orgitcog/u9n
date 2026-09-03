// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimNextUnitNode.h"
#include "AnimNextTraitStackUnitNode.generated.h"

/**
  * Implements AnimNext RigVM unit node extensions for Trait Stacks
  */
UCLASS(MinimalAPI)
class UAnimNextTraitStackUnitNode : public URigVMUnitNode
{
	GENERATED_BODY()

public:
	// Override node functions
	virtual FString GetNodeTitle() const override
	{
		return !NodeTitle.IsEmpty() ? NodeTitle : Super::GetNodeTitle();
	}

	bool IsExposedToManifest() const
	{
		return bIncludedInManifest;
	}

	void SetManifestState(bool bInIncludedInManifest)
	{
		bIncludedInManifest = bInIncludedInManifest;
	}
private:

	UPROPERTY()
	bool bIncludedInManifest = false;
};
