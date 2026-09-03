// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Data/PCGTextureData.h"

#include "RHI.h"

#include "PCGRenderTargetData.generated.h"

class UTexture;
class UTextureRenderTarget2D;

//TODO: It's possible that caching the result in this class is not as efficient as it could be
// if we expect to sample in different ways (e.g. channel) in the same render target
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class UPCGRenderTargetData : public UPCGBaseTextureData
{
	GENERATED_BODY()

public:
	// ~Begin UPCGData interface
	virtual EPCGDataType GetDataType() const override { return EPCGDataType::RenderTarget; }
	virtual void AddToCrc(FArchiveCrc32& Ar, bool bFullDataCrc) const override;
	virtual bool HoldsTransientResources() const override { return bOwnsRenderTarget; }
	virtual bool IsCacheable() const { return Super::IsCacheable() && !bOwnsRenderTarget; }
	virtual void ReleaseTransientResources(const TCHAR* InReason = nullptr) override;
	// ~End UPCGData interface

	//~Begin UPCGBaseTextureData interface
	virtual UTexture* GetTexture() const override;
	virtual FTextureRHIRef GetTextureRHI() const override;
	virtual EPCGTextureResourceType GetTextureResourceType() const override { return EPCGTextureResourceType::TextureObject; }
	//~End UPCGBaseTextureData interface

	//~Begin UPCGSpatialData interface
protected:
	virtual UPCGSpatialData* CopyInternal(FPCGContext* Context) const override;
	//~End UPCGSpatialData interface

public:
	UFUNCTION(BlueprintCallable, Category = RenderTarget)
	PCG_API void Initialize(UTextureRenderTarget2D* InRenderTarget, const FTransform& InTransform, bool bInSkipReadbackToCPU = false, bool bInTakeOwnershipOfRenderTarget = false);

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = Properties)
	TObjectPtr<UTextureRenderTarget2D> RenderTarget = nullptr;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = Properties)
	bool bOwnsRenderTarget = false;
};
