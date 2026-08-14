// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/ObjectMacros.h"

#include "MetaHumanProcessingAsset.generated.h"

UCLASS(MinimalAPI, BlueprintType, Abstract)
class UMetaHumanProcessingAsset : public UObject
{
	GENERATED_BODY()

public:

	virtual bool CanProcess() const PURE_VIRTUAL(UMetaHumanProcessingAsset::CanProcess, return false;)
};
