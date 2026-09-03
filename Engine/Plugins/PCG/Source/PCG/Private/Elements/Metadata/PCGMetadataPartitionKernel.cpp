// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/Metadata/PCGMetadataPartitionKernel.h"

#include "PCGContext.h"
#include "PCGModule.h"
#include "Compute/PCGComputeGraph.h"
#include "Compute/PCGDataBinding.h"
#include "Compute/PCGPinPropertiesGPU.h"
#include "Compute/DataInterfaces/Elements/PCGMetadataPartitionDataInterface.h"
#include "Compute/Elements/PCGComputeGraphElement.h"
#include "Elements/Metadata/PCGMetadataPartition.h"
#include "Graph/PCGGPUGraphCompilationContext.h"

#include "ShaderCompilerCore.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGMetadataPartitionKernel)

#define LOCTEXT_NAMESPACE "PCGMetadataPartitionKernel"

bool UPCGMetadataPartitionKernel::IsKernelDataValid(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGMetadataPartitionKernel::IsKernelDataValid);

	if (!Super::IsKernelDataValid(InContext))
	{
		return false;
	}

	FPCGComputeGraphContext* Context = InContext->IsComputeContext() ? static_cast<FPCGComputeGraphContext*>(InContext) : nullptr;

	if (Context && Context->DataBinding)
	{
		const UPCGMetadataPartitionSettings* MPSettings = CastChecked<UPCGMetadataPartitionSettings>(GetSettings());
		const FName AttributeName = MPSettings->PartitionAttributeSelectors[0].GetAttributeName();

		const FPCGDataCollectionDesc* InputDesc = Context->DataBinding->GetCachedKernelPinDataDesc(this, PCGPinConstants::DefaultInputLabel, /*bIsInput=*/true);

		if (!ensure(InputDesc))
		{
			return false;
		}

		FPCGKernelAttributeDesc AttributeDesc;
		bool bConflictingTypesInData = false;
		const bool bAttributePresent = InputDesc->GetAttributeDesc(AttributeName, AttributeDesc, bConflictingTypesInData);

		if (!bAttributePresent)
		{
			if (!InputDesc->DataDescs.IsEmpty())
			{
				PCG_KERNEL_VALIDATION_ERR(InContext, GetSettings(), FText::Format(
					LOCTEXT("PartitionAttributeMissing", "Partition attribute was not present in GPU data."),
					FText::FromName(AttributeName)));
			}

			return false;
		}

		if (bConflictingTypesInData)
		{
			PCG_KERNEL_VALIDATION_ERR(InContext, GetSettings(), FText::Format(
				LOCTEXT("PartitionAttributeTypeConflict", "Attribute '{0}' encountered with multiple different types in input data."),
				FText::FromName(AttributeName)));
			return false;
		}

		if (AttributeDesc.AttributeKey.Type != EPCGKernelAttributeType::StringKey)
		{
			PCG_KERNEL_VALIDATION_ERR(Context, GetSettings(), FText::Format(
				LOCTEXT("PartitionAttributeTypeInvalid", "Attribute '{0}' not usable for partitioning, only attributes of type String Key are currently supported."),
				FText::FromName(AttributeName)));
			return false;
		}
	}

	return true;
}

bool UPCGMetadataPartitionKernel::ComputeOutputBindingDataDesc(const UPCGComputeGraph* InGraph, FName InOutputPinLabel, UPCGDataBinding* InBinding, FPCGDataCollectionDesc& OutDataDesc) const
{
	check(InGraph);
	check(InBinding);

	const UPCGMetadataPartitionSettings* MPSettings = CastChecked<UPCGMetadataPartitionSettings>(GetSettings());

	// Code assumes single output pin.
	if (!ensure(InOutputPinLabel == PCGPinConstants::DefaultOutputLabel))
	{
		return false;
	}

	FPCGDataCollectionDesc InputPinDesc;
	const FPCGKernelPin InputKernelPin(KernelIndex, PCGPinConstants::DefaultInputLabel, /*bIsInput=*/true);
	ensure(InBinding->ComputeKernelPinDataDesc(InputKernelPin, &InputPinDesc));

	const int32 PartitionAttributeId = InBinding->GetAttributeId(MPSettings->PartitionAttributeSelectors[0].GetAttributeName(), EPCGKernelAttributeType::StringKey);

	FPCGDataCollectionDesc PinDesc;

	// Emit a point data for each potential partition.
	if (PartitionAttributeId != INDEX_NONE)
	{
		TArray<int32> UniqueStringKeyValues;
		InputPinDesc.GetUniqueStringKeyValues(PartitionAttributeId, UniqueStringKeyValues);

		for (const FPCGDataDesc& InputData : InputPinDesc.DataDescs)
		{
			// Before execution we don't know what string key value each attribute has, so we have to assume worst case - could be up to N points in M partitions.
			for (int32 StringKeyValue : UniqueStringKeyValues)
			{
				FPCGDataDesc& Desc = PinDesc.DataDescs.Add_GetRef(InputData);

				// Set the unique string keys to reflect the partition operation.
				for (FPCGKernelAttributeDesc& AttrDesc : Desc.AttributeDescs)
				{
					if (AttrDesc.AttributeId == PartitionAttributeId)
					{
						AttrDesc.SetStringKeys(MakeArrayView(&StringKeyValue, 1));
						break;
					}
				}
			}
		}
	}

	// Add output attribute (partition index).
	if (MPSettings->bAssignIndexPartition)
	{
		PinDesc.AddAttributeToAllData(FPCGKernelAttributeKey(MPSettings->PartitionIndexAttributeName, EPCGKernelAttributeType::Int), InBinding);
	}

	OutDataDesc = MoveTemp(PinDesc);
	return true;
}

int UPCGMetadataPartitionKernel::ComputeThreadCount(const UPCGDataBinding* InBinding) const
{
	const FPCGDataCollectionDesc* InputPinDesc = InBinding->GetCachedKernelPinDataDesc(this, PCGPinConstants::DefaultInputLabel, /*bIsInput=*/true);
	return ensure(InputPinDesc) ? InputPinDesc->ComputeDataElementCount(EPCGDataType::Point) : 0;
}

#if WITH_EDITOR
FString UPCGMetadataPartitionKernel::GetCookedSource(FPCGGPUCompilationContext& InOutContext) const
{
	FString TemplateFile;
	ensure(LoadShaderSourceFile(TEXT("/Plugin/PCG/Private/Elements/PCGMetadataPartition.usf"), EShaderPlatform::SP_PCD3D_SM5, &TemplateFile, nullptr));
	return TemplateFile;
}

void UPCGMetadataPartitionKernel::CreateAdditionalInputDataInterfaces(FPCGGPUCompilationContext& InOutContext, UObject* InObjectOuter, TArray<TObjectPtr<UComputeDataInterface>>& OutDataInterfaces) const
{
	Super::CreateAdditionalInputDataInterfaces(InOutContext, InObjectOuter, OutDataInterfaces);

	TObjectPtr<UPCGMetadataPartitionDataInterface> NodeDI = InOutContext.NewObject_AnyThread<UPCGMetadataPartitionDataInterface>(InObjectOuter);
	NodeDI->ProducerKernel = this;
	OutDataInterfaces.Add(NodeDI);
}
#endif

void UPCGMetadataPartitionKernel::GetKernelAttributeKeys(TArray<FPCGKernelAttributeKey>& OutKeys) const
{
	const UPCGMetadataPartitionSettings* MPSettings = CastChecked<UPCGMetadataPartitionSettings>(GetSettings());

	// Register the attributes this node reads or writes for which we know the attribute type. Currently
	// only StringKey attributes can be partitioned when executing on GPU, so declare the attribute here
	// rather than doing work to resolve at runtime.
	for (const FPCGAttributePropertyInputSelector& Selector : MPSettings->PartitionAttributeSelectors)
	{
		if (Selector.IsBasicAttribute())
		{
			OutKeys.Add(FPCGKernelAttributeKey(Selector.GetAttributeName(), EPCGKernelAttributeType::StringKey));
		}
	}

	// Register output attribute created by this node.
	if (MPSettings->bAssignIndexPartition)
	{
		OutKeys.AddUnique(FPCGKernelAttributeKey(MPSettings->PartitionIndexAttributeName, EPCGKernelAttributeType::Int));
	}
}

void UPCGMetadataPartitionKernel::GetInputPins(TArray<FPCGPinProperties>& OutPins) const
{
	OutPins.Emplace(PCGPinConstants::DefaultInputLabel, EPCGDataType::Point);
}

void UPCGMetadataPartitionKernel::GetOutputPins(TArray<FPCGPinPropertiesGPU>& OutPins) const
{
	OutPins.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Point);
}

#if WITH_EDITOR
bool UPCGMetadataPartitionKernel::PerformStaticValidation()
{
	if (!Super::PerformStaticValidation())
	{
		return false;
	}
	
	const UPCGMetadataPartitionSettings* MPSettings = CastChecked<UPCGMetadataPartitionSettings>(GetSettings());

	if (MPSettings->PartitionAttributeSelectors.Num() != 1)
	{
#if PCG_KERNEL_LOGGING_ENABLED
		StaticLogEntries.Emplace(LOCTEXT("MustProvideOnePartitionAttribute", "GPU implementation currently only supports a single partition attribute."), EPCGKernelLogVerbosity::Error);
#endif
		return false;
	}

	if (!MPSettings->PartitionAttributeSelectors[0].IsBasicAttribute())
	{
#if PCG_KERNEL_LOGGING_ENABLED
		StaticLogEntries.Emplace(LOCTEXT("OnlyBasicAttributesSupported", "GPU implementation currently only supports basic attributes."), EPCGKernelLogVerbosity::Error);
#endif
		return false;
	}

	return true;
}
#endif

#undef LOCTEXT_NAMESPACE
