// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Data/PCGSurfaceData.h"

#include "PCGVirtualTextureData.generated.h"

class URuntimeVirtualTexture;
class URuntimeVirtualTextureComponent;

UCLASS()
class UPCGVirtualTextureData : public UPCGSurfaceData
{
	GENERATED_BODY()

public:
	void Initialize(const URuntimeVirtualTextureComponent* InVirtualTextureComponent);

	//~Begin UPCGData interface
	virtual EPCGDataType GetDataType() const override { return EPCGDataType::VirtualTexture; }
	//~End UPCGData interface

	//~Begin UPCGSpatialData interface
	virtual FBox GetBounds() const override { return FBox(EForceInit::ForceInit); }
	virtual bool SamplePoint(const FTransform& InTransform, const FBox& InBounds, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const override { return false; }
protected:
	virtual UPCGSpatialData* CopyInternal(FPCGContext* Context) const override;
	//~End UPCGSpatialData interface

public:
	//~Begin UPCGSpatialDataWithPointCache interface
	virtual const UPCGPointData* CreatePointData(FPCGContext* Context) const override;
	virtual const UPCGPointArrayData* CreatePointArrayData(FPCGContext* Context, const FBox& InBounds) const override;
	//~End UPCGSpatialDataWithPointCache interface

	TObjectPtr<const URuntimeVirtualTexture> GetRuntimeVirtualTexture() const { return RuntimeVirtualTexture; }

protected:
	UPROPERTY()
	TObjectPtr<URuntimeVirtualTexture> RuntimeVirtualTexture = nullptr;

	// @todo_pcg: Support SVT as well?
	//UPROPERTY()
	//TObjectPtr<class UVirtualTextureBuilder> StreamingVirtualTexture = nullptr;
};
