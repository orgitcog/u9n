// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Compute/PCGComputeKernel.h"

#include "PCGCountUniqueAttributeValuesKernel.generated.h"

namespace PCGCountUniqueAttributeValuesConstants
{
	const FName ValueAttributeName = TEXT("UniqueValue");
	const FName ValueCountAttributeName = TEXT("UniqueValueCount");
}

UCLASS()
class UPCGCountUniqueAttributeValuesKernel : public UPCGComputeKernel
{
	GENERATED_BODY()

public:
	void SetAttributeName(FName InAttributeName) { AttributeName = InAttributeName; }

	virtual bool IsKernelDataValid(FPCGContext* InContext) const override;
	virtual bool ComputeOutputBindingDataDesc(const UPCGComputeGraph* InGraph, FName InOutputPinLabel, UPCGDataBinding* InBinding, FPCGDataCollectionDesc& OutDataDesc) const override;
	virtual int ComputeThreadCount(const UPCGDataBinding* Binding) const override;
	virtual bool DoesOutputPinRequireZeroInitialization(FName InOutputPinLabel) const override;
#if WITH_EDITOR
	virtual FString GetCookedSource(FPCGGPUCompilationContext& InOutContext) const override;
	virtual FString GetEntryPoint() const override { return TEXT("PCGCountUniqueAttributeValuesCS"); }
	void CreateAdditionalInputDataInterfaces(FPCGGPUCompilationContext& InOutContext, UObject* InObjectOuter, TArray<TObjectPtr<UComputeDataInterface>>& OutDataInterfaces) const override;
	// Split graph to read back analysis results.
	virtual bool SplitGraphAtOutput() const override { return true; }
#endif
	virtual void GetKernelAttributeKeys(TArray<FPCGKernelAttributeKey>& OutKeys) const override;

	virtual void GetInputPins(TArray<FPCGPinProperties>& OutPins) const override;
	virtual void GetOutputPins(TArray<FPCGPinPropertiesGPU>& OutPins) const override;

public:
	UPROPERTY()
	FName AttributeName;
};
