// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Compute/PCGDataForGPU.h"
#include "Compute/DataInterfaces/PCGComputeDataInterface.h"

#include "RenderGraphResources.h"
#include "ComputeFramework/ComputeDataProvider.h"

#include "PCGDataCollectionDataInterface.generated.h"

struct FPCGComputeGraphContext;
class FPCGDataCollectionDataInterfaceParameters;
class UPCGDataBinding;

/** Compute Framework Data Interface for reading PCG data. */
UCLASS(ClassGroup = (Procedural))
class UPCGDataCollectionDataInterface : public UPCGExportableDataInterface
{
	GENERATED_BODY()

public:
	//~ Begin UComputeDataInterface Interface
	TCHAR const* GetClassName() const override { return TEXT("PCGDataCollection"); }
	/** Return true if the associated UComputeDataProvider holds data that can be combined into a single dispatch invocation. */
	bool CanSupportUnifiedDispatch() const override { return false; } // I think this means compute shader can produce multiple buffers simultaneously?
	// TODO don't allow writing to an input!
	void GetSupportedInputs(TArray<FShaderFunctionDefinition>& OutFunctions) const override;
	// TODO could differentiate later for SRV vs UAV.
	void GetSupportedOutputs(TArray<FShaderFunctionDefinition>& OutFunctions) const override { GetSupportedInputs(OutFunctions); }
	void GetShaderParameters(TCHAR const* UID, FShaderParametersMetadataBuilder& InOutBuilder, FShaderParametersMetadataAllocations& InOutAllocations) const override;
	TCHAR const* GetShaderVirtualPath() const override; // TODO probably easier to just inline rather than external source?
	void GetShaderHash(FString& InOutKey) const override;
	void GetHLSL(FString& OutHLSL, FString const& InDataInterfaceName) const override;
	void GetDefines(FComputeKernelDefinitionSet& OutDefinitionSet) const override;
	// This data interface only uses the CF readback functionality for the element counters.
	bool GetRequiresReadback() const override { return bExposeElementCounters; }
	UComputeDataProvider* CreateDataProvider() const override;
	//~ End UComputeDataInterface Interface

	void SetRequiresZeroInitialization(bool bInZeroInit) { bRequiresZeroInitialization = bInZeroInit; }

	void SetElementCountMultiplier(uint32 InElementCountMultiplier) { ElementCountMultiplier = InElementCountMultiplier; }

	void SetExposeElementCounters(bool bInExposeElementCounters) { bExposeElementCounters = bInExposeElementCounters; }

public:
	/** Whether to perform full 0-initialization of the buffer. */
	UPROPERTY()
	bool bRequiresZeroInitialization = false;

	UPROPERTY()
	uint32 ElementCountMultiplier = 0;

	UPROPERTY()
	bool bExposeElementCounters = false;

protected:
	static TCHAR const* TemplateFilePath;
};

/** Compute Framework Data Provider for reading a PCG Data Collection. */
UCLASS()
class UPCGDataCollectionDataProvider : public UPCGExportableDataProvider
{
	GENERATED_BODY()

public:
	//~ Begin UPCGComputeDataProvider Interface
	virtual bool PrepareForExecute_GameThread(UPCGDataBinding* InBinding) override;
	//~ End UPCGComputeDataProvider Interface

	//~ Begin UComputeDataProvider Interface
	virtual void Initialize(const UComputeDataInterface* InDataInterface, UObject* InBinding, uint64 InInputMask, uint64 InOutputMask) override;
	FComputeDataProviderRenderProxy* GetRenderProxy() override;
	virtual void Reset() override;
	//~ End UComputeDataProvider Interface

	DECLARE_EVENT(UPCGDataCollectionDataProvider, FOnCountersReadBack);
	FOnCountersReadBack& OnCountersReadBack_GameThread() { return OnCountersReadBack; }

public:
	UPROPERTY()
	uint32 ElementCountMultiplier = 0;

	UPROPERTY()
	bool bRequiresZeroInitialization = false;

	UPROPERTY()
	bool bExposeElementCounters = false;

private:
	FOnCountersReadBack OnCountersReadBack;

	std::atomic<bool> bReadbackComplete = false;

	TArray<uint32> ReadbackCounters;
};

class FPCGDataCollectionDataProviderProxy : public FComputeDataProviderRenderProxy
{
public:
	FPCGDataCollectionDataProviderProxy(
		TWeakObjectPtr<UPCGDataCollectionDataProvider> InDataProvider,
		const FPCGDataCollectionDesc* InPinDesc,
		EPCGExportMode InExportMode,
		bool bInZeroInitialize,
		uint32 InElementCountMultiplier,
		bool bInExposeElementCounters,
		FName InOutputPinLabel,
		FName InOutputPinLabelAlias);

	//~ Begin FComputeDataProviderRenderProxy Interface
	bool IsValid(FValidationData const& InValidationData) const override;
	void GatherDispatchData(FDispatchData const& InDispatchData) override;
	void AllocateResources(FRDGBuilder& GraphBuilder, FAllocationData const& InAllocationData) override;
	void GetReadbackData(TArray<FReadbackData>& OutReadbackData) const override;
	//~ End FComputeDataProviderRenderProxy Interface

protected:
	virtual FRDGBufferSRVRef GetBufferToGraphStringKeySRV(FRDGBuilder& GraphBuilder, int32& OutNumRemappedStringKeys) const;

public:
	FReadbackCallback AsyncReadbackCallback_RenderThread;

	/** Generation count of the data provider when the proxy was created. */
	uint64 OriginatingGenerationCount = 0;

protected:
	uint32 SizeBytes = 0;

	EPCGExportMode ExportMode = EPCGExportMode::NoExport;

	bool bZeroInitialize = false;

	uint32 ElementCountMultiplier = 0;

	bool bExposeElementCounters = false;

	using FParameters = FPCGDataCollectionDataInterfaceParameters;

	FPCGDataCollectionDesc PinDesc;

	FRDGBufferRef Buffer = nullptr;
	FRDGBufferUAVRef BufferUAV = nullptr;

	FRDGBufferRef Counters = nullptr;
	FRDGBufferUAVRef CountersUAV = nullptr;

	// Weak pointer useful for passing back buffer handles. Do not access this directly from the render thread.
	TWeakObjectPtr<UPCGDataCollectionDataProvider> DataProviderWeakPtr;

	FName OutputPinLabel;
	FName OutputPinLabelAlias;

	FRDGBufferSRVRef BufferToGraphStringKeySRV = nullptr;

	int32 NumRemappedStringKeys = INDEX_NONE;
};
