// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGStaticMeshSpawnerKernel.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGModule.h"
#include "PCGParamData.h"
#include "Components/PCGProceduralISMComponent.h"
#include "Compute/PCGComputeGraph.h"
#include "Compute/PCGDataBinding.h"
#include "Compute/PCGPinPropertiesGPU.h"
#include "Compute/BuiltInKernels/PCGCountUniqueAttributeValuesKernel.h"
#include "Compute/Data/PCGProxyForGPUData.h"
#include "Compute/DataInterfaces/PCGInstanceDataInterface.h"
#include "Compute/DataInterfaces/Elements/PCGStaticMeshSpawnerDataInterface.h"
#include "Elements/PCGStaticMeshSpawner.h"
#include "Graph/PCGGPUGraphCompilationContext.h"
#include "InstanceDataPackers/PCGInstanceDataPackerBase.h"
#include "MeshSelectors/PCGMeshSelectorByAttribute.h"
#include "MeshSelectors/PCGMeshSelectorWeighted.h"

#include "ShaderCompilerCore.h"
#include "Engine/StaticMesh.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGStaticMeshSpawnerKernel)

#define LOCTEXT_NAMESPACE "PCGStaticMeshSpawnerKernel"

namespace PCGStaticMeshSpawnerKernel
{
	static const FText NoMeshEntriesFormat = LOCTEXT("NoMeshEntries", "No mesh entries provided in weighted mesh selector.");
}

bool UPCGStaticMeshSpawnerKernel::ComputeOutputBindingDataDesc(const UPCGComputeGraph* InGraph, FName InOutputPinLabel, UPCGDataBinding* InBinding, FPCGDataCollectionDesc& OutDataDesc) const
{
	check(InGraph);
	check(InBinding);

	// Code assumes single output pin.
	if (!ensure(InOutputPinLabel == PCGPinConstants::DefaultOutputLabel))
	{
		return false;
	}

	// Forward data from In to Out.
	FPCGDataCollectionDesc PinDesc;
	const FPCGKernelPin InputKernelPin(KernelIndex, PCGPinConstants::DefaultInputLabel, /*bIsInput=*/true);
	ensure(InBinding->ComputeKernelPinDataDesc(InputKernelPin, &PinDesc));

	// Add output attribute (selected mesh).
	{
		const UPCGStaticMeshSpawnerSettings* SMSettings = CastChecked<UPCGStaticMeshSpawnerSettings>(GetSettings());

		// Create unique value keys for the output string.
		if (UPCGMeshSelectorWeighted* SelectorWeighted = Cast<UPCGMeshSelectorWeighted>(SMSettings->MeshSelectorParameters))
		{
			TArray<int32> UniqueStringKeys;

			// Weighted selection - add explicit strings from settings.
			for (const FPCGMeshSelectorWeightedEntry& Entry : SelectorWeighted->MeshEntries)
			{
				const FString Value = Entry.Descriptor.StaticMesh.ToString();
				const int32 StringIndex = InBinding->GetStringTable().IndexOfByKey(Value);
				if (ensureAlways(StringIndex != INDEX_NONE))
				{
					UniqueStringKeys.AddUnique(StringIndex);
				}
			}

			PinDesc.AddAttributeToAllData(FPCGKernelAttributeKey(SMSettings->OutAttributeName, EPCGKernelAttributeType::StringKey), InBinding, &UniqueStringKeys);
		}
		else if (UPCGMeshSelectorByAttribute* SelectorByAttribute = Cast<UPCGMeshSelectorByAttribute>(SMSettings->MeshSelectorParameters))
		{
			// By-attribute selection - pass on strings from input attribute.
			if (SelectorByAttribute->AttributeName != NAME_None)
			{
				TArray<int32> StringKeys;

				for (FPCGDataDesc& DataDesc : PinDesc.DataDescs)
				{
					StringKeys.Empty();

					for (const FPCGKernelAttributeDesc& AttrDesc : DataDesc.AttributeDescs)
					{
						if (AttrDesc.AttributeKey.Identifier.Name == SelectorByAttribute->AttributeName)
						{
							StringKeys = AttrDesc.GetUniqueStringKeys();
							break;
						}
					}

					DataDesc.AddAttribute(FPCGKernelAttributeKey(SMSettings->OutAttributeName, EPCGKernelAttributeType::StringKey), InBinding, &StringKeys);
				}
			}
		}
		else if (SMSettings->MeshSelectorParameters)
		{
			UE_LOG(LogPCG, Error, TEXT("Mesh selector not supported by GPU Static Mesh Spawner: %s"), *SMSettings->MeshSelectorParameters->GetName());
		}
	}

	OutDataDesc = MoveTemp(PinDesc);
	return true;
}

int UPCGStaticMeshSpawnerKernel::ComputeThreadCount(const UPCGDataBinding* InBinding) const
{
	const FPCGDataCollectionDesc* InputPinDesc = InBinding->GetCachedKernelPinDataDesc(this, PCGPinConstants::DefaultInputLabel, /*bIsInput=*/true);
	return ensure(InputPinDesc) ? InputPinDesc->ComputeDataElementCount(EPCGDataType::Point) : 0;
}

#if WITH_EDITOR
FString UPCGStaticMeshSpawnerKernel::GetCookedSource(FPCGGPUCompilationContext& InOutContext) const
{
	FString TemplateFile;
	ensure(LoadShaderSourceFile(TEXT("/Plugin/PCG/Private/Elements/PCGStaticMeshSpawner.usf"), EShaderPlatform::SP_PCD3D_SM5, &TemplateFile, nullptr));
	return TemplateFile;
}

void UPCGStaticMeshSpawnerKernel::CreateAdditionalInputDataInterfaces(FPCGGPUCompilationContext& InOutContext, UObject* InObjectOuter, TArray<TObjectPtr<UComputeDataInterface>>& OutDataInterfaces) const
{
	Super::CreateAdditionalInputDataInterfaces(InOutContext, InObjectOuter, OutDataInterfaces);

	TObjectPtr<UPCGStaticMeshSpawnerDataInterface> NodeDI = InOutContext.NewObject_AnyThread<UPCGStaticMeshSpawnerDataInterface>(InObjectOuter);
	NodeDI->ProducerKernel = this;
	OutDataInterfaces.Add(NodeDI);
}

void UPCGStaticMeshSpawnerKernel::CreateAdditionalOutputDataInterfaces(FPCGGPUCompilationContext& InOutContext, UObject* InObjectOuter, TArray<TObjectPtr<UComputeDataInterface>>& OutDataInterfaces) const
{
	Super::CreateAdditionalOutputDataInterfaces(InOutContext, InObjectOuter, OutDataInterfaces);

	UPCGInstanceDataInterface* InstanceDI = InOutContext.NewObject_AnyThread<UPCGInstanceDataInterface>(InObjectOuter);
	InstanceDI->ProducerKernel = this;
	InstanceDI->InputPinProvidingData = PCGPinConstants::DefaultInputLabel;
	OutDataInterfaces.Add(InstanceDI);
}
#endif

void UPCGStaticMeshSpawnerKernel::AddStaticCreatedStrings(TArray<FString>& InOutStringTable) const
{
	const UPCGStaticMeshSpawnerSettings* SMSettings = CastChecked<UPCGStaticMeshSpawnerSettings>(GetSettings());
	if (UPCGMeshSelectorWeighted* SelectorWeighted = Cast<UPCGMeshSelectorWeighted>(SMSettings->MeshSelectorParameters))
	{
		for (const FPCGMeshSelectorWeightedEntry& Entry : SelectorWeighted->MeshEntries)
		{
			InOutStringTable.AddUnique(Entry.Descriptor.StaticMesh.ToString());
		}
	}
}

void UPCGStaticMeshSpawnerKernel::GetKernelAttributeKeys(TArray<FPCGKernelAttributeKey>& OutKeys) const
{
	// Register the attribute this node creates.
	const UPCGStaticMeshSpawnerSettings* SMSettings = CastChecked<UPCGStaticMeshSpawnerSettings>(GetSettings());
	OutKeys.AddUnique(FPCGKernelAttributeKey(SMSettings->OutAttributeName, EPCGKernelAttributeType::StringKey));
}

void UPCGStaticMeshSpawnerKernel::GetInputPins(TArray<FPCGPinProperties>& OutPins) const
{
	OutPins.Emplace(PCGPinConstants::DefaultInputLabel, EPCGDataType::Point);
	OutPins.Emplace(PCGStaticMeshSpawnerConstants::InstanceCountsPinLabel, EPCGDataType::Param);
}

void UPCGStaticMeshSpawnerKernel::GetOutputPins(TArray<FPCGPinPropertiesGPU>& OutPins) const
{
	OutPins.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Point);
}

#if WITH_EDITOR
bool UPCGStaticMeshSpawnerKernel::PerformStaticValidation()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGStaticMeshSpawnerKernel::PerformStaticValidation);

	if (!Super::PerformStaticValidation())
	{
		return false;
	}

	const UPCGStaticMeshSpawnerSettings* SMSettings = CastChecked<UPCGStaticMeshSpawnerSettings>(GetSettings());

	if (UPCGMeshSelectorWeighted* SelectorWeighted = Cast<UPCGMeshSelectorWeighted>(SMSettings->MeshSelectorParameters))
	{
		if (SelectorWeighted->MeshEntries.IsEmpty())
		{
#if PCG_KERNEL_LOGGING_ENABLED
			StaticLogEntries.Emplace(PCGStaticMeshSpawnerKernel::NoMeshEntriesFormat, EPCGKernelLogVerbosity::Error);
#endif
			return false;
		}

		for (const FPCGMeshSelectorWeightedEntry& Entry : SelectorWeighted->MeshEntries)
		{
			if (Entry.Descriptor.StaticMesh.IsNull())
			{
#if PCG_KERNEL_LOGGING_ENABLED
				StaticLogEntries.Emplace(LOCTEXT("UnassignedMesh", "Unassigned mesh."), EPCGKernelLogVerbosity::Error);
#endif
				return false;
			}
		}
	}
	else if (!SMSettings->MeshSelectorParameters || !SMSettings->MeshSelectorParameters->IsA<UPCGMeshSelectorByAttribute>())
	{
#if PCG_KERNEL_LOGGING_ENABLED
		StaticLogEntries.Emplace(LOCTEXT("InvalidMeshSelector", "Currently GPU Static Mesh Spawner nodes must use PCGMeshSelectorWeighted or UPCGMeshSelectorByAttribute as the mesh selector type."), EPCGKernelLogVerbosity::Error);
#endif
		return false;
	}

	// Currently instance packers must be able to specify a full list of attribute names upfront, to build the attribute table at compile time.
	// TODO: We should be able to augment a static attribute table with new attributes at execution time, which will allow other types like regex.
	if (SMSettings->InstanceDataPackerParameters && !SMSettings->InstanceDataPackerParameters->GetAttributeNames(/*OutNames=*/nullptr))
	{
#if PCG_KERNEL_LOGGING_ENABLED
		StaticLogEntries.Emplace(LOCTEXT("InvalidInstancePacker", "Selected instance packer does not support GPU execution."), EPCGKernelLogVerbosity::Error);
#endif
		return false;
	}
	
	return true;
}
#endif

#undef LOCTEXT_NAMESPACE
