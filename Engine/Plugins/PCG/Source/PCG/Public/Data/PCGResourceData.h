// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGData.h"

#include "Engine/StreamableManager.h"

#include "PCGResourceData.generated.h"

#define UE_API PCG_API

/** Data that wrap/represent an asset, like a Static Mesh or Texture. */
UCLASS(MinimalAPI, Abstract, BlueprintType, ClassGroup = (Procedural))
class UPCGResourceData : public UPCGData
{
	GENERATED_BODY()

public:
	//~ Begin UPCGData interface
	virtual EPCGDataType GetDataType() const override { return EPCGDataType::Resource; }
	//~ End UPCGData interface

	UE_API virtual FSoftObjectPath GetResourcePath() const PURE_VIRTUAL(UPCGResourceData::GetResourcePath, return FSoftObjectPath(););

	UE_API TSharedPtr<FStreamableHandle> RequestResourceLoad(bool bAsynchronous = true) const;
};

#undef UE_API
