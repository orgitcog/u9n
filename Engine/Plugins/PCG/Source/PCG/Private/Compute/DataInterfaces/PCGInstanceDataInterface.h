// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Compute/DataInterfaces/PCGComputeDataInterface.h"

#include "RHIResources.h"

#include "PCGInstanceDataInterface.generated.h"

class FPCGInstanceDataInterfaceParameters;
class FPrimitiveSceneProxy;
class UPCGSettings;
class UComputeKernel;

using FPCGPrimitiveComponentAndNumInstances = TPair<TWeakObjectPtr<UPrimitiveComponent>, int>;

/** Data Interface for writing instance data (transform and custom floats) to an intermediate buffer, which is then injected into the GPU Scene. */
UCLASS(ClassGroup = (Procedural))
class UPCGInstanceDataInterface : public UPCGComputeDataInterface
{
	GENERATED_BODY()

public:
	//~ Begin UComputeDataInterface Interface
	TCHAR const* GetClassName() const override { return TEXT("PCGInstanceData"); }
	void GetSupportedOutputs(TArray<FShaderFunctionDefinition>& OutFunctions) const override;
	void GetShaderParameters(TCHAR const* UID, FShaderParametersMetadataBuilder& InOutBuilder, FShaderParametersMetadataAllocations& InOutAllocations) const override;
	TCHAR const* GetShaderVirtualPath() const override;
	void GetShaderHash(FString& InOutKey) const override;
	void GetHLSL(FString& OutHLSL, FString const& InDataInterfaceName) const override;
	void GetDefines(FComputeKernelDefinitionSet& OutDefinitionSet) const override;
	bool GetRequiresPostSubmitCall() const override { return true; }
	UComputeDataProvider* CreateDataProvider() const override;
	//~ End UComputeDataInterface Interface

public:
	UPROPERTY()
	FName InputPinProvidingData = NAME_None;

private:
	static TCHAR const* TemplateFilePath;
};

/** Compute Framework Data Provider for writing to the GPU Scene. */
UCLASS()
class UPCGInstanceDataProvider : public UPCGComputeDataProvider
{
	GENERATED_BODY()

public:
	//~ Begin UComputeDataProvider Interface
	virtual bool PrepareForExecute_GameThread(UPCGDataBinding* InBinding) override;
	virtual bool PostExecute(UPCGDataBinding* InBinding) override;
	FComputeDataProviderRenderProxy* GetRenderProxy() override;
	virtual void Reset() override;
	//~ End UComputeDataProvider Interface

	TArray<FPCGPrimitiveComponentAndNumInstances> Primitives;

	uint32 NumInstancesAllPrimitives = 0;

	uint32 NumCustomFloatsPerInstance = 0;

	/** Whether instance data has been applied to scene and operation is complete. */
	bool bWroteInstances = false;

	/** Bumped each time the data provider is initialized or reset, so that async callbacks can detect if they originated from a previous usage of the data provider and no-op. */
	std::atomic<uint64> GenerationCounter = 0;
};

class FPCGInstanceDataProviderProxy : public FComputeDataProviderRenderProxy
{
public:
	FPCGInstanceDataProviderProxy(TArray<FPCGPrimitiveComponentAndNumInstances> InPrimitives, uint32 InNumInstancesAllPrimitives, uint32 InNumCustomFloatsPerInstance, TWeakObjectPtr<UPCGInstanceDataProvider> InDataProvider/*, uint64 InDataProviderGeneration*/);

	//~ Begin FComputeDataProviderRenderProxy Interface
	bool IsValid(FValidationData const& InValidationData) const override;
	void AllocateResources(FRDGBuilder& GraphBuilder, FAllocationData const& InAllocationData);
	void GatherDispatchData(FDispatchData const& InDispatchData) override;
	void PostSubmit(FRDGBuilder& InGraphBuilder) const override;
	//~ End FComputeDataProviderRenderProxy Interface

protected:
	using FParameters = FPCGInstanceDataInterfaceParameters;

	bool bIsValid = false;

	TArray<FPCGPrimitiveComponentAndNumInstances> Primitives;

	/** Instance transforms of all instances across all primitives. Three float4s per instance. */
	FRDGBufferRef InstanceData = nullptr;
	FRDGBufferSRVRef InstanceDataSRV = nullptr;
	FRDGBufferUAVRef InstanceDataUAV = nullptr;

	/** Custom float data for all instances across all primitives. N per instance. Number of custom floats N comes from inputs (and is static). */
	FRDGBufferRef InstanceCustomFloatData = nullptr;
	FRDGBufferSRVRef InstanceCustomFloatDataSRV = nullptr;
	FRDGBufferUAVRef InstanceCustomFloatDataUAV = nullptr;

	/** A per-primitive instance counter. Updated atomically and used to place instances into a segmented array (one segment per primitive). */
	FRDGBufferRef WriteCounters = nullptr;
	FRDGBufferSRVRef WriteCountersSRV = nullptr;
	FRDGBufferUAVRef WriteCountersUAV = nullptr;

	uint32 NumInstancesAllPrimitives = 0;
	uint32 NumCustomFloatsPerInstance = 0;

	TWeakObjectPtr<UPCGInstanceDataProvider> DataProvider;
	uint64 DataProviderGeneration = 0;
};
