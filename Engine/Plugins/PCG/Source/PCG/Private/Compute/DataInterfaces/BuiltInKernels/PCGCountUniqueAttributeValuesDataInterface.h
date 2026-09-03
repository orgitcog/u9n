// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Compute/DataInterfaces/PCGComputeDataInterface.h"
#include "ComputeFramework/ComputeDataProvider.h"

#include "PCGCountUniqueAttributeValuesDataInterface.generated.h"

class FPCGCountUniqueAttributeValuesDataInterfaceParameters;

/** Data Interface to marshal Count Unique Values kernel data to GPU. */
UCLASS(ClassGroup = (Procedural))
class UPCGCountUniqueAttributeValuesDataInterface : public UPCGComputeDataInterface
{
	GENERATED_BODY()

public:
	//~ Begin UComputeDataInterface Interface
	TCHAR const* GetClassName() const override { return TEXT("PCGCountUniqueAttributeValues"); }
	void GetSupportedInputs(TArray<FShaderFunctionDefinition>& OutFunctions) const override;
	void GetShaderParameters(TCHAR const* UID, FShaderParametersMetadataBuilder& InOutBuilder, FShaderParametersMetadataAllocations& InOutAllocations) const override;
	void GetHLSL(FString& OutHLSL, FString const& InDataInterfaceName) const override;
	UComputeDataProvider* CreateDataProvider() const override;
	//~ End UComputeDataInterface Interface

public:
	UPROPERTY()
	FName AttributeToCountName;
};

UCLASS()
class UPCGCountUniqueAttributeValuesDataProvider : public UPCGComputeDataProvider
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
	FName AttributeToCountName;

	UPROPERTY()
	int32 AttributeToCountId = INDEX_NONE;

	UPROPERTY()
	int32 OutputValueAttributeId = INDEX_NONE;

	UPROPERTY()
	int32 OutputCountAttributeId = INDEX_NONE;

	UPROPERTY()
	int32 MaxAttributeValue = INDEX_NONE;

	UPROPERTY()
	TArray<int32> UniqueStringKeyValues;
};

class FPCGCountUniqueAttributeValuesProviderProxy : public FComputeDataProviderRenderProxy
{
public:
	struct FCountUniqueValuesData_RenderThread
	{
		int32 AttributeToCountId = INDEX_NONE;
		int32 OutputValueAttributeId = INDEX_NONE;
		int32 OutputCountAttributeId = INDEX_NONE;
		int32 MaxAttributeValue = INDEX_NONE;
		TArray<int32> UniqueStringKeyValues;
	};

	FPCGCountUniqueAttributeValuesProviderProxy(FCountUniqueValuesData_RenderThread InData)
		: Data(MoveTemp(InData))
	{}

	//~ Begin FComputeDataProviderRenderProxy Interface
	bool IsValid(FValidationData const& InValidationData) const override;
	void AllocateResources(FRDGBuilder& GraphBuilder, FAllocationData const& InAllocationData);
	void GatherDispatchData(FDispatchData const& InDispatchData) override;
	//~ End FComputeDataProviderRenderProxy Interface

protected:
	using FParameters = FPCGCountUniqueAttributeValuesDataInterfaceParameters;

	FCountUniqueValuesData_RenderThread Data;

	FRDGBufferRef AttributeValueToValueIndex = nullptr;
	FRDGBufferSRVRef AttributeValueToValueIndexSRV = nullptr;
};
