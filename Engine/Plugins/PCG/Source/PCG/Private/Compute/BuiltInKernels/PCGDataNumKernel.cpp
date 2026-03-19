// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGDataNumKernel.h"

#include "PCGContext.h"
#include "Compute/DataInterfaces/BuiltInKernels/PCGDataNumDataInterface.h"
#include "Compute/Elements/PCGComputeGraphElement.h"
#include "Graph/PCGGPUGraphCompilationContext.h"

#include "ShaderCompilerCore.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGDataNumKernel)

#define LOCTEXT_NAMESPACE "PCGDataNumKernel"

bool UPCGDataNumKernel::ComputeOutputBindingDataDesc(const UPCGComputeGraph* InGraph, const FName InOutputPinLabel, UPCGDataBinding* InBinding, FPCGDataCollectionDesc& OutDataDesc) const
{
	check(InGraph);
	check(InBinding);

	// Only single output pin
	if (!ensure(InOutputPinLabel == PCGPinConstants::DefaultOutputLabel))
	{
		return false;
	}

	FPCGDataDesc& OutputDataDesc = OutDataDesc.DataDescs.Emplace_GetRef(EPCGDataType::Param, 1);
	OutputDataDesc.AddAttribute(OutCountAttributeKey, InBinding);

	return true;
}

int UPCGDataNumKernel::ComputeThreadCount(const UPCGDataBinding* InBinding) const
{
	// One element returned. Only need 1 thread.
	return 1;
}

bool UPCGDataNumKernel::IsKernelDataValid(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGDataNumKernel::IsKernelDataValid);

	if (!Super::IsKernelDataValid(InContext))
	{
		return false;
	}

	if (!InContext || !InContext->IsComputeContext())
	{
		return true;
	}

	const FPCGComputeGraphContext* Context = static_cast<FPCGComputeGraphContext*>(InContext);
	if (const UPCGDataBinding* DataBinding = Context->DataBinding.Get())
	{
		if (const FPCGDataCollectionDesc* OutputDataDesc = DataBinding->GetCachedKernelPinDataDesc(this, PCGPinConstants::DefaultOutputLabel, /*bIsInputPin=*/false))
		{
			FPCGKernelAttributeDesc AttributeDesc;
			bool bConflictingTypesFound = false;
			if (OutputDataDesc->GetAttributeDesc(OutCountAttributeKey.Identifier, AttributeDesc, bConflictingTypesFound))
			{
				if (OutCountAttributeKey.Type == EPCGKernelAttributeType::Invalid)
				{
					PCG_KERNEL_VALIDATION_ERR(InContext, GetSettings(), FText::Format(
						LOCTEXT("InvalidAttributeKeyType", "Output attribute for the count could not be created with an 'invalid' type."),
						FText::FromString(OutCountAttributeKey.Identifier.ToString())));

					return false;
				}

				return true;
			}
			else
			{
				PCG_KERNEL_VALIDATION_ERR(InContext, GetSettings(), FText::Format(
					LOCTEXT("AttributeMissing", "Output attribute '{0}' not found."),
					FText::FromString(OutCountAttributeKey.Identifier.ToString())));

				return false;
			}
		}
	}

	return false;
}

#if WITH_EDITOR
FString UPCGDataNumKernel::GetCookedSource(FPCGGPUCompilationContext& InOutContext) const
{
	FString DataNumFile;
	ensure(LoadShaderSourceFile(TEXT("/Plugin/PCG/Private/BuiltInKernels/PCGDataNum.usf"), EShaderPlatform::SP_PCD3D_SM5, &DataNumFile, nullptr));
	return DataNumFile;
}

void UPCGDataNumKernel::CreateAdditionalInputDataInterfaces(FPCGGPUCompilationContext& InOutContext, UObject* InObjectOuter, TArray<TObjectPtr<UComputeDataInterface>>& OutDataInterfaces) const
{
	Super::CreateAdditionalInputDataInterfaces(InOutContext, InObjectOuter, OutDataInterfaces);

	TObjectPtr<UPCGDataNumDataInterface> NodeDI = InOutContext.NewObject_AnyThread<UPCGDataNumDataInterface>(InObjectOuter);
	NodeDI->ProducerKernel = this;

	OutDataInterfaces.Add(NodeDI);
}
#endif

void UPCGDataNumKernel::GetKernelAttributeKeys(TArray<FPCGKernelAttributeKey>& OutKeys) const
{
	OutKeys.AddUnique(OutCountAttributeKey);
}

void UPCGDataNumKernel::GetInputPins(TArray<FPCGPinProperties>& OutPins) const
{
	OutPins.Emplace(PCGPinConstants::DefaultInputLabel, EPCGDataType::Point);
}

void UPCGDataNumKernel::GetOutputPins(TArray<FPCGPinPropertiesGPU>& OutPins) const
{
	OutPins.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Param);
}

void UPCGDataNumKernel::SetOutputAttribute(const FName InAttributeName)
{
	OutCountAttributeKey = FPCGKernelAttributeKey(InAttributeName, EPCGKernelAttributeType::Int);
}

#undef LOCTEXT_NAMESPACE
