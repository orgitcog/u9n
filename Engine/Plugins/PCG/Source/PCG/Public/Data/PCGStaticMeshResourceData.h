// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Data/PCGResourceData.h"

#include "PCGStaticMeshResourceData.generated.h"

#define UE_API PCG_API

class UStaticMesh;

/** Data that wraps a Static Mesh soft object path. */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class UPCGStaticMeshResourceData : public UPCGResourceData
{
	GENERATED_BODY()

public:
	UE_API void Initialize(TSoftObjectPtr<UStaticMesh> InStaticMesh);

	//~ Begin UPCGData interface
	virtual EPCGDataType GetDataType() const override { return EPCGDataType::StaticMeshResource; }
	//~ End UPCGData interface

	//~ Begin UPCGResourceData interface
	UE_API virtual FSoftObjectPath GetResourcePath() const override;
	//~ End UPCGResourceData interface

protected:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = StaticMesh)
	TSoftObjectPtr<UStaticMesh> StaticMesh = nullptr;
};

#undef UE_API
