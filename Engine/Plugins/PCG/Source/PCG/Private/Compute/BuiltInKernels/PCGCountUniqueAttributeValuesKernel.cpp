// Copyright Epic Games, Inc. All Rights Reserved.

#include "Compute/BuiltInKernels/PCGCountUniqueAttributeValuesKernel.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGModule.h"
#include "Compute/Elements/PCGComputeGraphElement.h"
#include "Compute/PCGComputeGraph.h"
#include "Compute/PCGDataBinding.h"
#include "Compute/PCGPinPropertiesGPU.h"
#include "Compute/DataInterfaces/BuiltInKernels/PCGCountUniqueAttributeValuesDataInterface.h"
#include "Elements/PCGStaticMeshSpawner.h"
#include "Graph/PCGGPUGraphCompilationContext.h"
#include "MeshSelectors/PCGMeshSelectorByAttribute.h"

#include "ShaderCompilerCore.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGCountUniqueAttributeValuesKernel)

#define LOCTEXT_NAMESPACE "PCGCountUniqueAttributeValuesKernel"

bool UPCGCountUniqueAttributeValuesKernel::IsKernelDataValid(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGCountUniqueAttributeValuesKernel::IsKernelDataValid);

	if (!Super::IsKernelDataValid(InContext))
	{
		return false;
	}

	if (!InContext || !InContext->IsComputeContext())
	{
		return true;
	}

	const FPCGComputeGraphContext* Context = static_cast<FPCGComputeGraphContext*>(InContext);
	const UPCGDataBinding* DataBinding = Context->DataBinding.Get();
	if (DataBinding)
	{
		const FPCGDataCollectionDesc* InputDataDesc = DataBinding->GetCachedKernelPinDataDesc(this, PCGPinConstants::DefaultInputLabel, /*bIsInputPin=*/true);

		if (!ensure(InputDataDesc))
		{
			return false;
		}

		FPCGKernelAttributeDesc AttributeDesc;
		bool bConflictingTypesFound = false;

		if (!InputDataDesc->GetAttributeDesc(AttributeName, AttributeDesc, bConflictingTypesFound))
		{
			if (!InputDataDesc->DataDescs.IsEmpty())
			{
				PCG_KERNEL_VALIDATION_ERR(InContext, GetSettings(), FText::Format(
					LOCTEXT("AttributeMissing", "Count attribute '{0}' not found, this attribute must be present on all input data, and be of type String Key."),
					FText::FromName(AttributeName)));
			}

			return false;
		}

		if (bConflictingTypesFound)
		{
			PCG_KERNEL_VALIDATION_ERR(InContext, GetSettings(), FText::Format(
				LOCTEXT("AttributeHasMultipleTypes", "Count attribute '{0}' found with multiple types in input data, all attributes must be of type String Key."),
				FText::FromName(AttributeName)));

			return false;
		}

		if (AttributeDesc.AttributeKey.Type != EPCGKernelAttributeType::StringKey)
		{
			// Attribute value counting only currently supported for attributes of type StringKey.
			PCG_KERNEL_VALIDATION_ERR(Context, GetSettings(), FText::Format(
				LOCTEXT("AttributeTypeInvalid", "Cannot count values for attribute '{0}', only attributes of type String Key are currently supported."),
				FText::FromName(AttributeName)));

			return false;
		}
	}

	return true;
}

bool UPCGCountUniqueAttributeValuesKernel::ComputeOutputBindingDataDesc(const UPCGComputeGraph* InGraph, FName InOutputPinLabel, UPCGDataBinding* InBinding, FPCGDataCollectionDesc& OutDataDesc) const
{
	check(InGraph);
	check(InBinding);

	// Code assumes single output pin.
	if (!ensure(InOutputPinLabel == PCGPinConstants::DefaultOutputLabel))
	{
		return false;
	}

	FPCGDataCollectionDesc InputDesc;
	const FPCGKernelPin InputKernelPin(KernelIndex, PCGPinConstants::DefaultInputLabel, /*bIsInput=*/true);
	ensure(InBinding->ComputeKernelPinDataDesc(InputKernelPin, &InputDesc));

	const int32 AttributeId = InBinding->GetAttributeId(AttributeName, EPCGKernelAttributeType::StringKey);

	if (AttributeId != INDEX_NONE)
	{
		TArray<int32> UniqueStringKeyValues;
		InputDesc.GetUniqueStringKeyValues(AttributeId, UniqueStringKeyValues);

		FPCGDataDesc& OutputDataDesc = OutDataDesc.DataDescs.Emplace_GetRef();
		OutputDataDesc.Type = EPCGDataType::Param;
		OutputDataDesc.ElementCount = UniqueStringKeyValues.Num();

		OutputDataDesc.AddAttribute(FPCGKernelAttributeKey(PCGCountUniqueAttributeValuesConstants::ValueAttributeName, EPCGKernelAttributeType::Int), InBinding);
		OutputDataDesc.AddAttribute(FPCGKernelAttributeKey(PCGCountUniqueAttributeValuesConstants::ValueCountAttributeName, EPCGKernelAttributeType::Int), InBinding);
	}

	return true;
}

int UPCGCountUniqueAttributeValuesKernel::ComputeThreadCount(const UPCGDataBinding* InBinding) const
{
	int ThreadCount = 0;

	// Execute a thread for every element in the input data.
	const FPCGDataCollectionDesc* OutputPinDesc = InBinding->GetCachedKernelPinDataDesc(this, PCGPinConstants::DefaultInputLabel, /*bIsInput=*/true);

	if (ensure(OutputPinDesc))
	{
		for (const FPCGDataDesc& DataDesc : OutputPinDesc->DataDescs)
		{
			ThreadCount += DataDesc.ElementCount;
		}
	}

	return ThreadCount;
}

bool UPCGCountUniqueAttributeValuesKernel::DoesOutputPinRequireZeroInitialization(FName InOutputPinLabel) const
{
	// We are atomically incrementing the values on the output so we need to ensure the values are 0-initialized.
	return InOutputPinLabel == PCGPinConstants::DefaultOutputLabel;
}

#if WITH_EDITOR
FString UPCGCountUniqueAttributeValuesKernel::GetCookedSource(FPCGGPUCompilationContext& InOutContext) const
{
	FString TemplateFile;
	ensure(LoadShaderSourceFile(TEXT("/Plugin/PCG/Private/BuiltInKernels/PCGCountUniqueAttributeValues.usf"), EShaderPlatform::SP_PCD3D_SM5, &TemplateFile, nullptr));
	return TemplateFile;
}

void UPCGCountUniqueAttributeValuesKernel::CreateAdditionalInputDataInterfaces(FPCGGPUCompilationContext& InOutContext, UObject* InObjectOuter, TArray<TObjectPtr<UComputeDataInterface>>& OutDataInterfaces) const
{
	Super::CreateAdditionalInputDataInterfaces(InOutContext, InObjectOuter, OutDataInterfaces);

	TObjectPtr<UPCGCountUniqueAttributeValuesDataInterface> KernelDI = InOutContext.NewObject_AnyThread<UPCGCountUniqueAttributeValuesDataInterface>(InObjectOuter);
	KernelDI->ProducerKernel = this;
	KernelDI->AttributeToCountName = AttributeName;

	OutDataInterfaces.Add(KernelDI);
}
#endif

void UPCGCountUniqueAttributeValuesKernel::GetKernelAttributeKeys(TArray<FPCGKernelAttributeKey>& OutKeys) const
{
	// Register the attribute this node creates.
	OutKeys.AddUnique(FPCGKernelAttributeKey(PCGCountUniqueAttributeValuesConstants::ValueAttributeName, EPCGKernelAttributeType::Int));
	OutKeys.AddUnique(FPCGKernelAttributeKey(PCGCountUniqueAttributeValuesConstants::ValueCountAttributeName, EPCGKernelAttributeType::Int));
}

void UPCGCountUniqueAttributeValuesKernel::GetInputPins(TArray<FPCGPinProperties>& OutPins) const
{
	OutPins.Emplace(PCGPinConstants::DefaultInputLabel, EPCGDataType::Point);
}

void UPCGCountUniqueAttributeValuesKernel::GetOutputPins(TArray<FPCGPinPropertiesGPU>& OutPins) const
{
	// Attribute set with a value count attribute, element count equal to number of unique values of the counted attribute.
	OutPins.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Param);
}

#undef LOCTEXT_NAMESPACE
