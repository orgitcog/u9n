// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Compute/PCGDataForGPU.h"
#include "Compute/DataInterfaces/PCGComputeDataInterface.h"
#include "Data/PCGTextureData.h"

#include "RHIResources.h"
#include "ComputeFramework/ComputeDataProvider.h"

#include "PCGTextureDataInterface.generated.h"

class FPCGTextureDataInterfaceParameters;
class UPCGBaseTextureData;

namespace PCGTextureDataInterfaceConstants
{
	/**
	 * The maximum number of texture SRVs and UAVs which can be bound to a PCGTextureDataInterface.
	 * We can't use full bindless on all platforms, so fallback to emulating.
	 * If you change this number, make sure update PCGTextureDataInterface.ush as well.
	 */
	constexpr uint32 MAX_NUM_SRV_BINDINGS = 8;
	constexpr uint32 MAX_NUM_UAV_BINDINGS = 1;
}

struct FPCGTextureBindingInfo
{
	explicit FPCGTextureBindingInfo(const UPCGBaseTextureData* InTextureData);
	FPCGTextureBindingInfo(const FPCGDataDesc& InDataDesc, const FTransform& InTransform);
	bool operator==(const FPCGTextureBindingInfo& Other) const;

	EPCGTextureResourceType ResourceType = EPCGTextureResourceType::TextureObject;
	FTextureRHIRef Texture = nullptr;
	TRefCountPtr<IPooledRenderTarget> ExportedTexture = nullptr;
	FIntPoint Size = FIntPoint::ZeroValue;
	ETextureDimension Dimension = ETextureDimension::Texture2D;
	bool bPointSample = false;

	// @todo_pcg: Eventually replace TextureBounds with only the Transform
	FTransform Transform = FTransform::Identity;
	FBox TextureBounds = FBox(EForceInit::ForceInit);
};

struct FPCGTextureInfo
{
	int BindingIndex = 0;
	int SliceIndex = 0;
};

/** Data Interface allowing sampling of a texture. */
UCLASS(ClassGroup = (Procedural))
class UPCGTextureDataInterface : public UPCGExportableDataInterface
{
	GENERATED_BODY()

public:
	//~ Begin UComputeDataInterface Interface
	TCHAR const* GetClassName() const override { return TEXT("PCGTexture"); }
	void GetSupportedInputs(TArray<FShaderFunctionDefinition>& OutFunctions) const override;
	void GetSupportedOutputs(TArray<FShaderFunctionDefinition>& OutFunctions) const override;
	void GetShaderParameters(TCHAR const* UID, FShaderParametersMetadataBuilder& InOutBuilder, FShaderParametersMetadataAllocations& InOutAllocations) const override;
	TCHAR const* GetShaderVirtualPath() const override;
	void GetShaderHash(FString& InOutKey) const override;
	void GetHLSL(FString& OutHLSL, FString const& InDataInterfaceName) const override;
	void GetDefines(FComputeKernelDefinitionSet& OutDefinitionSet) const override;
	void GetPermutations(FComputeKernelPermutationVector& OutPermutationVector) const override;
	UComputeDataProvider* CreateDataProvider() const override;
	//~ End UComputeDataInterface Interface

	void SetInitializeFromDataCollection(bool bInInitializeFromDataCollection) { bInitializeFromDataCollection = bInInitializeFromDataCollection; }
	bool GetInitializeFromDataCollection() const { return bInitializeFromDataCollection; }

protected:
	UPROPERTY()
	bool bInitializeFromDataCollection = false;

private:
	static TCHAR const* TemplateFilePath;
};

/** Compute Framework Data Provider for reading a texture. */
UCLASS()
class UPCGTextureDataProvider : public UPCGExportableDataProvider
{
	GENERATED_BODY()

public:
	//~ Begin UComputeDataProvider Interface
	virtual void Initialize(const UComputeDataInterface* InDataInterface, UObject* InBinding, uint64 InInputMask, uint64 InOutputMask) override;
	FComputeDataProviderRenderProxy* GetRenderProxy() override;
	virtual void Reset() override;
	//~ End UComputeDataProvider Interface

	//~ Begin UPCGComputeDataProvider Interface
	virtual bool PrepareForExecute_GameThread(UPCGDataBinding* InBinding) override;
	//~ End UPCGComputeDataProvider Interface

	const TArray<FPCGTextureBindingInfo>& GetBindingInfos() const { return BindingInfos; }
	const TArray<FPCGTextureInfo>& GetTextureInfos() const { return TextureInfos; }

protected:
	void BuildInfosFromDataCollection(UPCGDataBinding* InBinding);
	void BuildInfosFromDataDescription(UPCGDataBinding* InBinding);

public:
	TArray<FPCGTextureBindingInfo> BindingInfos;
	TArray<FPCGTextureInfo> TextureInfos;
	bool bInitializeFromDataCollection = false;
};

class FPCGTextureDataProviderProxy : public FComputeDataProviderRenderProxy
{
public:
	explicit FPCGTextureDataProviderProxy(TWeakObjectPtr<UPCGTextureDataProvider> InDataProvider);

	//~ Begin FComputeDataProviderRenderProxy Interface
	bool IsValid(FValidationData const& InValidationData) const override;
	void GatherPermutations(FPermutationData& InOutPermutationData) const override;
	void GatherDispatchData(FDispatchData const& InDispatchData) override;
	void AllocateResources(FRDGBuilder& GraphBuilder, FAllocationData const& InAllocationData) override;
	//~ End FComputeDataProviderRenderProxy Interface

	void CreateDefaultTextures(FRDGBuilder& GraphBuilder);
	void CreateTextures(FRDGBuilder& GraphBuilder);
	void ExportTextureUAVs(const TArray<TRefCountPtr<IPooledRenderTarget>>& ExportedTextures);
	void PackTextureInfos(FRDGBuilder& GraphBuilder);

protected:
	using FParameters = FPCGTextureDataInterfaceParameters;

	TArray<FPCGTextureBindingInfo> BindingInfos;
	TArray<FPCGTextureInfo> TextureInfos;

	// For export
	EPCGExportMode ExportMode = EPCGExportMode::NoExport;
	FPCGDataCollectionDesc PinDesc;
	FName OutputPinLabel;
	FName OutputPinLabelAlias;

	/** Generation count of the data provider when the proxy was created. */
	uint64 OriginatingGenerationCount = 0;

	// Weak pointer useful for passing back texture handles. Do not access this directly from the render thread.
	TWeakObjectPtr<UPCGTextureDataProvider> DataProviderWeakPtr_GT;

	// Allocated resources
	FRDGTextureSRVRef TextureSRV[PCGTextureDataInterfaceConstants::MAX_NUM_SRV_BINDINGS];
	FRDGTextureSRVRef TextureArraySRV[PCGTextureDataInterfaceConstants::MAX_NUM_SRV_BINDINGS];
	FRDGTextureUAVRef TextureUAV[PCGTextureDataInterfaceConstants::MAX_NUM_UAV_BINDINGS];
	FRDGTextureUAVRef TextureArrayUAV[PCGTextureDataInterfaceConstants::MAX_NUM_UAV_BINDINGS];
	FRDGBufferSRVRef TextureInfosBufferSRV = nullptr;
};
