// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Data/PCGSurfaceData.h"

#include "RendererInterface.h"
#include "RHI.h"

#include "PCGTextureData.generated.h"

#define UE_API PCG_API

class UPCGSpatialData;
class UTexture;
class UTexture2D;

UENUM(BlueprintType)
enum class EPCGTextureColorChannel : uint8
{
	Red,
	Green,
	Blue,
	Alpha
};

UENUM(BlueprintType)
enum class UE_DEPRECATED(5.5, "EPCGTextureDensityFunction has been deprecated.") EPCGTextureDensityFunction : uint8
{
	Ignore,
	Multiply
};

/** Method used to determine the value for a sample based on the value of nearby texels. */
UENUM(BlueprintType)
enum class EPCGTextureFilter : uint8
{
	Point UMETA(Tooltip="Takes the value of whatever texel the sample lands in."),
	Bilinear UMETA(Tooltip="Bilinearly interpolates the values of the four nearest texels to the sample location.")
};

UENUM()
enum class EPCGTextureAddressMode : uint8
{
	Clamp UMETA(ToolTip = "Clamps UV to 0-1."),
	Wrap UMETA(ToolTip = "Tiles the texture to fit.")
};

UENUM()
enum class EPCGTextureResourceType : uint8
{
	TextureObject UMETA(ToolTip = "UObject texture such as UTexture2D or UTextureRenderTarget2D."),
	ExportedTexture UMETA(ToolTip = "Texture handle exported from a texture on the GPU."),
	Invalid UMETA(Hidden)
};

namespace PCGTextureSamplingHelpers
{
	/** Returns true if a texture is CPU-accessible. */
	TOptional<bool> IsTextureCPUAccessible(UTexture2D* Texture);

	/** Returns true if a texture is both GPU-accessible and reachable from CPU memory. */
	TOptional<bool> CanGPUTextureBeCPUAccessed(UTexture2D* Texture);
}

UCLASS(MinimalAPI, Abstract)
class UPCGBaseTextureData : public UPCGSurfaceData
{
	GENERATED_BODY()

public:
	// ~Being UObject interface
	UE_API virtual void PostLoad() override;
	// ~End UObject interface

	// ~Begin UPCGData interface
	virtual EPCGDataType GetDataType() const override { return EPCGDataType::BaseTexture; }
	// ~End UPCGData interface

	//~Begin UPCGSpatialData interface
	UE_API virtual FBox GetBounds() const override;
	UE_API virtual FBox GetStrictBounds() const override;
	UE_API virtual bool SamplePoint(const FTransform& Transform, const FBox& Bounds, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const override;
	//~End UPCGSpatialData interface

	//~Begin UPCGSpatialDataWithPointCache interface
	UE_API virtual const UPCGPointData* CreatePointData(FPCGContext* Context) const override;
	UE_API virtual const UPCGPointArrayData* CreatePointArrayData(FPCGContext* Context, const FBox& InBounds) const override;
	//~End UPCGSpatialDataWithPointCache interface

	/** Sample using a local space 'UV' position. */
	UE_API bool SamplePointLocal(const FVector2D& LocalPosition, FVector4& OutColor, float& OutDensity) const;

	UE_API virtual bool IsValid() const;

	UE_API virtual UTexture* GetTexture() const PURE_VIRTUAL(UPCGBaseTextureData::GetTexture, return nullptr;)
	UE_API virtual FTextureRHIRef GetTextureRHI() const PURE_VIRTUAL(UPCGBaseTextureData::GetTextureResource, return nullptr;);
	UE_API virtual EPCGTextureResourceType GetTextureResourceType() const PURE_VIRTUAL(UPCGBaseTextureData::GetTextureResourceType, return EPCGTextureResourceType::Invalid;);
	virtual TRefCountPtr<IPooledRenderTarget> GetRefCountedTexture() const { return nullptr; }
	virtual int GetTextureSlice() const { return 0; }
	virtual FIntPoint GetTextureSize() const { return FIntPoint(Width, Height); }

protected:
	UE_API const UPCGBasePointData* CreateBasePointData(FPCGContext* Context, TSubclassOf<UPCGBasePointData> PointDataClass) const;

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UFUNCTION(BlueprintGetter, meta = (BlueprintInternalUseOnly = "true"))
	UE_API EPCGTextureDensityFunction GetDensityFunctionEquivalent() const;

	UFUNCTION(BlueprintSetter, meta = (BlueprintInternalUseOnly = "true"))
	UE_API void SetDensityFunctionEquivalent(EPCGTextureDensityFunction DensityFunction);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

public:
#if WITH_EDITORONLY_DATA
	UE_DEPRECATED(5.5, "DensityFunction has been deprecated in favor of bUseDensitySourceChannel.")
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UPROPERTY(BlueprintGetter = GetDensityFunctionEquivalent, BlueprintSetter = SetDensityFunctionEquivalent, Category = SpatialData, meta = (DeprecatedProperty, DeprecatedMessage = "Density function on GetTextureData is deprecated in favor of bUseDensitySourceChannel."))
	EPCGTextureDensityFunction DensityFunction = EPCGTextureDensityFunction::Multiply; 
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (InlineEditConditionToggle))
	bool bUseDensitySourceChannel = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (DisplayName = "Density Source Channel", EditCondition = "bUseDensitySourceChannel"))
	EPCGTextureColorChannel ColorChannel = EPCGTextureColorChannel::Alpha;

	/** Method used to determine the value for a sample based on the value of nearby texels. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGTextureFilter Filter = EPCGTextureFilter::Bilinear;

	/** The size of one texel in cm, used when calling ToPointData. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (UIMin = "1.0", ClampMin = "1.0"))
	float TexelSize = 50.0f;

	/** Whether to tile the source or to stretch it to fit target area. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bUseAdvancedTiling = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tiling", meta = (EditCondition = "bUseAdvancedTiling"))
	FVector2D Tiling = FVector2D(1.0, 1.0);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tiling", meta = (EditCondition = "bUseAdvancedTiling"))
	FVector2D CenterOffset = FVector2D::ZeroVector;

	/** Rotation to apply when sampling texture. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (UIMin = -360, ClampMin = -360, UIMax = 360, ClampMax = 360, Units = deg, EditCondition = "bUseAdvancedTiling"))
	float Rotation = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tiling", meta = (EditionCondition = "bUseAdvancedTiling"))
	bool bUseTileBounds = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tiling", meta = (EditCondition = "bUseAdvancedTiling && bUseTileBounds"))
	FBox2D TileBounds = FBox2D(FVector2D(-0.5, -0.5), FVector2D(0.5, 0.5));

protected:
	UPROPERTY()
	TArray<FLinearColor> ColorData;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = SpatialData)
	FBox Bounds = FBox(EForceInit::ForceInit);

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = SpatialData)
	int32 Height = 0;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = SpatialData)
	int32 Width = 0;

	UPROPERTY()
	bool bSkipReadbackToCPU = false;

	/** Used to make sure errors are only logged once when trying to sample points from a data which hasn't been read back into a CPU buffer. */
	mutable bool bEmittedNoReadbackDataError = false;

	UE_API void CopyBaseTextureData(UPCGBaseTextureData* NewTextureData) const;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup=(Procedural))
class UPCGTextureData : public UPCGBaseTextureData
{
	GENERATED_BODY()

public:
	/**
	 * Initialize this data. Can depend on async texture operations / async GPU readbacks. Should be polled until it returns true signaling completion,
	 * and then IsInitialized() is used to verify the initialization was successful and data is ready to use.
	 */
	PCG_API bool Initialize(UTexture* InTexture, uint32 InTextureIndex, const FTransform& InTransform, bool bCreateCPUDuplicateEditorOnly = false, bool bInSkipReadbackToCPU = false);
	PCG_API bool Initialize(TRefCountPtr<IPooledRenderTarget> InTextureHandle, uint32 InTextureIndex, const FTransform& InTransform, bool bInSkipReadbackToCPU = false);

	/** Data is successfully initialized and is ready to use. */
	bool IsSuccessfullyInitialized() const { return bSuccessfullyInitialized; }

	EPCGTextureResourceType GetTextureResourceType() const override { return ResourceType; }

	//~Begin UPCGData interface
	virtual EPCGDataType GetDataType() const override { return EPCGDataType::Texture; }
	virtual void AddToCrc(FArchiveCrc32& Ar, bool bFullDataCrc) const override;
	// Don't hold onto exported buffers currently as graphics memory usage (and lifetimes) may cause issues.
	virtual bool HoldsTransientResources() const override { return ResourceType == EPCGTextureResourceType::ExportedTexture; }
	virtual bool IsCacheable() const { return Super::IsCacheable() && ResourceType != EPCGTextureResourceType::ExportedTexture; }
	virtual void ReleaseTransientResources(const TCHAR* InReason = nullptr) override { TextureHandle = nullptr; }
	//~End UPCGData interface

	//~Begin UPCGBaseTextureData interface
	virtual UTexture* GetTexture() const override { return Texture.IsValid() ? Texture.Get() : nullptr; }
	virtual FTextureRHIRef GetTextureRHI() const override;
	virtual TRefCountPtr<IPooledRenderTarget> GetRefCountedTexture() const { return TextureHandle; }
	virtual int GetTextureSlice() const override { return TextureIndex; }
	//~End UPCGBaseTextureData interface
	
	//~Begin UPCGSpatialData interface
protected:
	virtual UPCGSpatialData* CopyInternal(FPCGContext* Context) const override;
	//~End UPCGSpatialData interface

	void InitializeInternal(UTexture* InTexture, uint32 InTextureIndex, const FTransform& InTransform, bool* bOutInitializeDone, bool bCreateCPUDuplicateEditorOnly, bool bInSkipReadbackToCPU);

private:
	/** Attempts to initialize the UPCGTextureData from a CPU-accessible texture. Returns true if CPU initialization succeeds. */
	TOptional<bool> InitializeFromCPUTexture();

	/** Attempts to read back from a GPU-accessible texture. Returns true if GPU texture readback can be dispatched. */
	bool ReadbackFromGPUTexture();

#if WITH_EDITOR
	/** Attempts to initialize the UPCGTextureData from a GPU-accessible texture, but with CPU-accessible memory. Returns true if initialization succeeds. */
	TOptional<bool> InitializeGPUTextureFromCPU();
#endif

public:
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = Properties)
	TWeakObjectPtr<UTexture> Texture = nullptr;

#if WITH_EDITORONLY_DATA
	/** Transient CPU visible duplicate of Texture created and used only when initialized with bCreateCPUDuplicateEditorOnly. */
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> DuplicateTexture = nullptr;

	UPROPERTY(Transient)
	bool bDuplicateTextureInitialized = false;
#endif

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = Properties)
	int TextureIndex = 0;

	UPROPERTY(Transient, BlueprintReadOnly, VisibleAnywhere, Category = Properties)
	bool bSuccessfullyInitialized = false;

	bool bReadbackFromGPUInitiated = false;

	// Added to help deprecation in 5.5. To be removed when the deprecated Initialized function is removed.
	TFunction<void()> PostInitializeCallback;

protected:
	/** The type of underlying resource that this texture data represents. */
	UPROPERTY()
	EPCGTextureResourceType ResourceType = EPCGTextureResourceType::TextureObject;

	/** If initialized from an exported texture this holds a reference to the resource. */
	TRefCountPtr<IPooledRenderTarget> TextureHandle = nullptr;

	bool bUpdatedReadbackTextureResource = false;

public:
	UE_DEPRECATED(5.5, "Will be removed. Poll the alternate Initialize API until it returns true instead of passing in a callback.")
	PCG_API void Initialize(UTexture* InTexture, uint32 InTextureIndex, const FTransform& InTransform, const TFunction<void()>& InPostInitializeCallback, bool bCreateCPUDuplicateEditorOnly = false);

	UE_DEPRECATED(5.6, "Internal object state removed from blueprint.")
	UPROPERTY()
	bool bReadbackFromGPUInitiated_DEPRECATED = false;
};

#undef UE_API
