// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGContext.h"
#include "PCGSettings.h"

#include "RendererInterface.h"

#include "PCGGenerateGrassMaps.generated.h"

class ALandscapeProxy;
class FLandscapeGrassWeightExporter;
class ULandscapeComponent;
class ULandscapeGrassType;
class UPCGTextureData;
class UTexture;
struct IPooledRenderTarget;

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class UPCGGenerateGrassMapsSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("GenerateGrassMaps")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGGenerateGrassMapsElement", "NodeTitle", "Generate Grass Maps"); }
	virtual FText GetNodeTooltipText() const override { return NSLOCTEXT("PCGGenerateGrassMapsElement", "NodeTooltip", "Generates landscape grass maps on the GPU."); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::GPU; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual bool IsInputPinRequiredByExecution(const UPCGPin* InPin) const override { return true; }
	virtual FPCGElementPtr CreateElement() const override;
#if WITH_EDITOR
	virtual EPCGChangeType GetChangeTypeForProperty(const FName& InPropertyName) const override;
#endif
	//~End UPCGSettings interface

public:
	/** Select which grass types to generate. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "!bOverrideFromInput"))
	TArray<FString> SelectedGrassTypes;

	/** Override grass types from input. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bOverrideFromInput = false;

	/** Input attribute to pull grass type strings from. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "bOverrideFromInput", PCG_DiscardPropertySelection, PCG_DiscardExtraSelection))
	FPCGAttributePropertyInputSelector GrassTypesAttribute;

	/** If toggled, will only generate grass types which are not selected. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bExcludeSelectedGrassTypes = true;

	/** Skip CPU readback of emitted textures during initialization of the texture datas. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bSkipReadbackToCPU = false;
};

struct FPCGGenerateGrassMapsContext : public FPCGContext
{
public:
	~FPCGGenerateGrassMapsContext();

	FLandscapeGrassWeightExporter* LandscapeGrassWeightExporter = nullptr;

	/** Output texture data objects. */
	TArray<TObjectPtr<UPCGTextureData>> TextureDatas;

	/** Exported result texture array. */
	TRefCountPtr<IPooledRenderTarget> GrassMapHandle = nullptr;

	/** List of the grass types selected for generation. We have to hold their texture index as well, since
	 * this array could be sparse, but the actual texture array we produce will always have all the grass types.
	 */
	TArray<TTuple<TWeakObjectPtr<ULandscapeGrassType>, /*TextureIndex=*/int32>> SelectedGrassTypes;

	/** Total number of grass types used by the landscape component. Includes grass types which were not selected for generation. */
	int32 NumGrassTypes = 0;

	TWeakObjectPtr<ALandscapeProxy> LandscapeProxy = nullptr;
	TArray<TWeakObjectPtr<ULandscapeComponent>> LandscapeComponents;

	/** World-space bounds containing all of the landscape components given to the grass weight exporter. */
	FBox GrassMapBounds = FBox(EForceInit::ForceInit);

	/** Extent (side length) of each landscape component. */
	double LandscapeComponentExtent = 0.0;

	/** True when we have filtered all of the incoming landscape components down to the ones which overlap the given bounds. */
	bool bLandscapeComponentsFiltered = false;

	/** True when the landscape components are ready for rendering. */
	bool bReadyToRender = false;

	/** Textures that we wait to be streamed before generating the grass maps. */
	TArray<TObjectPtr<UTexture>> TexturesToStream;

	/** True when streaming has been requested on landscape textures. */
	bool bTextureStreamingRequested = false;

	/** True when grass map generation has been scheduled on the render thread. */
	bool bGenerationScheduled = false;

protected:
	virtual void AddExtraStructReferencedObjects(FReferenceCollector& Collector) override;
};

class FPCGGenerateGrassMapsElement : public IPCGElement
{
public:
	virtual void GetDependenciesCrc(const FPCGGetDependenciesCrcParams& InParams, FPCGCrc& OutCrc) const override;
	/** FLandscapeGrassWeightExporter expects to exist in scope only on the game thread. */
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const { return true; }

protected:
	virtual FPCGContext* CreateContext() override;
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};
