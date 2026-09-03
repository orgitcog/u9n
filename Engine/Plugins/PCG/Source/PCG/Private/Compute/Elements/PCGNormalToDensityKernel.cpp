// Copyright Epic Games, Inc. All Rights Reserved.

#include "Compute/Elements/PCGNormalToDensityKernel.h"

#include "PCGContext.h"
#include "Graph/PCGGPUGraphCompilationContext.h"
#include "Compute/Elements/PCGComputeGraphElement.h"
#include "Compute/DataInterfaces/Elements/PCGNormalToDensityDataInterface.h"

#include "ShaderCompilerCore.h"

#define LOCTEXT_NAMESPACE "PCGTemplateKernel"

bool UPCGNormalToDensityKernel::ComputeOutputBindingDataDesc(const UPCGComputeGraph* InGraph, FName InOutputPinLabel, UPCGDataBinding* InBinding, FPCGDataCollectionDesc& OutDataDesc) const
{
	check(InGraph);
	check(InBinding);

	// Code assumes single output pin.
	if (!ensure(InOutputPinLabel == PCGPinConstants::DefaultOutputLabel))
	{
		return false;
	}

	const FPCGKernelPin SourceKernelPin(KernelIndex, PCGPinConstants::DefaultInputLabel, /*bIsInput=*/true);

	ensure(InBinding->ComputeKernelPinDataDesc(SourceKernelPin, &OutDataDesc));

	return true;
}

int UPCGNormalToDensityKernel::ComputeThreadCount(const UPCGDataBinding* InBinding) const
{
	int ThreadCount = 0;

	const FPCGDataCollectionDesc* OutputPinDesc = InBinding->GetCachedKernelPinDataDesc(this, PCGPinConstants::DefaultOutputLabel, /*bIsInput=*/false);

	if (ensure(OutputPinDesc))
	{
		for (const FPCGDataDesc& DataDesc : OutputPinDesc->DataDescs)
		{
			ThreadCount += DataDesc.ElementCount;
		}
	}

	return ThreadCount;
}

#if WITH_EDITOR
FString UPCGNormalToDensityKernel::GetCookedSource(FPCGGPUCompilationContext& InOutContext) const
{
	FString TemplateFile;
	ensure(LoadShaderSourceFile(TEXT("/Plugin/PCG/Private/Elements/PCGNormalToDensity.usf"), EShaderPlatform::SP_PCD3D_SM5, &TemplateFile, nullptr));
	return TemplateFile;
}

void UPCGNormalToDensityKernel::CreateAdditionalInputDataInterfaces(FPCGGPUCompilationContext& InOutContext, UObject* InObjectOuter, TArray<TObjectPtr<UComputeDataInterface>>& OutDataInterfaces) const
{
	Super::CreateAdditionalInputDataInterfaces(InOutContext, InObjectOuter, OutDataInterfaces);

	TObjectPtr<UPCGNormalToDensityDataInterface> NodeDI = InOutContext.NewObject_AnyThread<UPCGNormalToDensityDataInterface>(InObjectOuter);
	NodeDI->ProducerKernel = this;

	OutDataInterfaces.Add(NodeDI);
}
#endif

void UPCGNormalToDensityKernel::GetInputPins(TArray<FPCGPinProperties>& OutPins) const
{
	OutPins.Emplace(PCGPinConstants::DefaultInputLabel, EPCGDataType::Point);
}

void UPCGNormalToDensityKernel::GetOutputPins(TArray<FPCGPinPropertiesGPU>& OutPins) const
{
	OutPins.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Point);
}

#undef LOCTEXT_NAMESPACE
