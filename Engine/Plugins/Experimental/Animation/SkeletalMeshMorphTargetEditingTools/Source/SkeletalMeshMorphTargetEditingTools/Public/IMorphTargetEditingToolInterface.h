// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "IMorphTargetEditingToolInterface.generated.h"

class FMorphTargetEditChange;
class UMorphTargetEditingToolProperties;

UINTERFACE(MinimalAPI)
class UMorphTargetEditingToolInterface : public UInterface
{
	GENERATED_BODY()
};

class IMorphTargetEditingToolInterface : public IInterface
{
	GENERATED_BODY()
public:
	virtual void SetupMorphEditingToolCommon();
	virtual void ShutdownMorphEditingToolCommon();
	virtual void SetupCommonProperties(const TFunction<void(UMorphTargetEditingToolProperties*)>& InSetupFunction) = 0;
	// virtual void Apply(const FMorphTargetEditChange& Change) = 0;
	// virtual void Revert(const FMorphTargetEditChange& Change) = 0;
};

