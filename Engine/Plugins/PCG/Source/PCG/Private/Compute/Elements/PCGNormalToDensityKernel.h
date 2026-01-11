// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Compute/PCGComputeKernel.h"
#include "Compute/PCGPinPropertiesGPU.h"

#include "PCGNormalToDensityKernel.generated.h"

class UPCGNormalToDensitySettings;

UCLASS()
class UPCGNormalToDensityKernel : public UPCGComputeKernel
{	
	GENERATED_BODY()

public:
	virtual bool ComputeOutputBindingDataDesc(const UPCGComputeGraph* InGraph, FName InOutputPinLabel, UPCGDataBinding* InBinding, FPCGDataCollectionDesc& OutDataDesc) const override;
	virtual int ComputeThreadCount(const UPCGDataBinding* Binding) const override;

#if WITH_EDITOR
	virtual FString GetCookedSource(FPCGGPUCompilationContext& InOutContext) const override;
	virtual FString GetEntryPoint() const override { return TEXT("PCGNormalToDensityCS"); }
	void CreateAdditionalInputDataInterfaces(FPCGGPUCompilationContext& InOutContext, UObject* InObjectOuter, TArray<TObjectPtr<UComputeDataInterface>>& OutDataInterfaces) const override;
#endif
	virtual void GetInputPins(TArray<FPCGPinProperties>& OutPins) const override;
	virtual void GetOutputPins(TArray<FPCGPinPropertiesGPU>& OutPins) const override;
};

