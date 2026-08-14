// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "SKMMorphTargetBackedTarget.generated.h"

struct FMeshDescription;

UINTERFACE(MinimalAPI)
class USkeletalMeshMorphTargetBackedTarget : public UInterface
{
	GENERATED_BODY()
};

class ISkeletalMeshMorphTargetBackedTarget : public IInterface
{
	GENERATED_BODY()
public:
	virtual void SetDataToCommit(FMeshDescription&& InMeshDescription, const FName& ToolMorphTargetName) = 0;
	virtual void SetEditingMorphTargetName(const FName& InName) = 0;
	virtual TArray<FName> GetEditableMorphTargetNames() = 0;
	virtual FName GetValidNameForNewMorphTarget(const FName& InName) = 0;
};

