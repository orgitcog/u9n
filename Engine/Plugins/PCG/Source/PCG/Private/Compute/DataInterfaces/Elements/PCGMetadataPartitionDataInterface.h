// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Compute/DataInterfaces/PCGComputeDataInterface.h"
#include "ComputeFramework/ComputeDataProvider.h"

#include "PCGMetadataPartitionDataInterface.generated.h"

class FPCGMetadataPartitionDataInterfaceParameters;

/** Data Interface to marshal Attribute Partition settings to the GPU. */
UCLASS(ClassGroup = (Procedural))
class UPCGMetadataPartitionDataInterface : public UPCGComputeDataInterface
{
	GENERATED_BODY()

public:
	//~ Begin UComputeDataInterface Interface
	TCHAR const* GetClassName() const override { return TEXT("PCGMetadataPartition"); }
	void GetSupportedInputs(TArray<FShaderFunctionDefinition>& OutFunctions) const override;
	void GetShaderParameters(TCHAR const* UID, FShaderParametersMetadataBuilder& InOutBuilder, FShaderParametersMetadataAllocations& InOutAllocations) const override;
	void GetHLSL(FString& OutHLSL, FString const& InDataInterfaceName) const override;
	UComputeDataProvider* CreateDataProvider() const override;
	//~ End UComputeDataInterface Interface
};

UCLASS()
class UPCGMetaDataPartitionDataProvider : public UPCGComputeDataProvider
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

public:
	UPROPERTY()
	TObjectPtr<const UPCGComputeKernel> Kernel;

	UPROPERTY()
	int32 PartitionAttributeId = INDEX_NONE;

	UPROPERTY()
	int32 NumInputData = 0;

	UPROPERTY()
	uint32 NumPartitions = 0;

	UPROPERTY()
	int32 MaxAttributeValue = INDEX_NONE;

	UPROPERTY()
	TArray<int32> UniqueStringKeyValues;
};

class FPCGMetaDataPartitionProviderProxy : public FComputeDataProviderRenderProxy
{
public:
	FPCGMetaDataPartitionProviderProxy(int32 InPartitionAttributeId, int32 InNumInputData, uint32 InNumPartitions, int32 InMaxAttributeValue, TArray<int32> InUniqueStringKeyValues)
		: PartitionAttributeId(InPartitionAttributeId)
		, NumInputData(InNumInputData)
		, NumPartitions(InNumPartitions)
		, MaxAttributeValue(InMaxAttributeValue)
		, UniqueStringKeyValues(MoveTemp(InUniqueStringKeyValues))
	{}

	//~ Begin FComputeDataProviderRenderProxy Interface
	bool IsValid(FValidationData const& InValidationData) const override;
	void AllocateResources(FRDGBuilder& GraphBuilder, FAllocationData const& InAllocationData);
	void GatherDispatchData(FDispatchData const& InDispatchData) override;
	//~ End FComputeDataProviderRenderProxy Interface

protected:
	using FParameters = FPCGMetadataPartitionDataInterfaceParameters;

	int32 PartitionAttributeId = INDEX_NONE;

	uint32 NumInputData = 0;

	uint32 NumPartitions = 0;

	int32 MaxAttributeValue = INDEX_NONE;

	TArray<int32> UniqueStringKeyValues;

	FRDGBufferRef AttributeValueToPartitionIndex = nullptr;
	FRDGBufferSRVRef AttributeValueToPartitionIndexSRV = nullptr;
};
