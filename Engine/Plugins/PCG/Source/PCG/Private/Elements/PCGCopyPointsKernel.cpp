// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGCopyPointsKernel.h"

#include "PCGContext.h"
#include "PCGModule.h"
#include "PCGParamData.h"
#include "Compute/PCGComputeGraph.h"
#include "Compute/PCGDataBinding.h"
#include "Compute/DataInterfaces/Elements/PCGCopyPointsDataInterface.h"
#include "Compute/Data/PCGProxyForGPUData.h"
#include "Compute/Elements/PCGComputeGraphElement.h"
#include "Elements/PCGCopyPoints.h"
#include "Elements/PCGCopyPointsKernelShared.h"
#include "Graph/PCGGPUGraphCompilationContext.h"

#include "ShaderCompilerCore.h"
#include "Containers/StaticArray.h"
#include "Internationalization/Regex.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGCopyPointsKernel)

#define LOCTEXT_NAMESPACE "PCGCopyPointsKernel"

bool UPCGCopyPointsKernel::IsKernelDataValid(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGCopyPointsKernel::IsKernelDataValid);

	if (!Super::IsKernelDataValid(InContext))
	{
		return false;
	}

	if (!InContext || !InContext->IsComputeContext())
	{
		return true;
	}

	return PCGCopyPointsKernel::IsKernelDataValid(this, CastChecked<UPCGCopyPointsSettings>(GetSettings()), static_cast<FPCGComputeGraphContext*>(InContext));
}

bool UPCGCopyPointsKernel::ComputeOutputBindingDataDesc(const UPCGComputeGraph* InGraph, FName InOutputPinLabel, UPCGDataBinding* InBinding, FPCGDataCollectionDesc& OutDataDesc) const
{
	check(InGraph);
	check(InBinding);

	const UPCGCopyPointsSettings* CPSettings = CastChecked<UPCGCopyPointsSettings>(GetSettings());

	// Code assumes single output pin.
	if (!ensure(InOutputPinLabel == PCGPinConstants::DefaultOutputLabel))
	{
		return false;
	}

	// A graph split was injected before this kernel. We expect to find selected flags
	int NumSelected = INDEX_NONE;
	TArray<bool> Selected;
	
	if (CPSettings->bMatchBasedOnAttribute)
	{
		const int32 AnalysisDataIndex = InBinding->GetFirstInputDataIndex(this, PCGCopyPointsConstants::SelectedFlagsPinLabel);

		if (AnalysisDataIndex != INDEX_NONE)
		{
			const UPCGParamData* AnalysisResultsData = Cast<UPCGParamData>(InBinding->InputDataCollection.TaggedData[AnalysisDataIndex].Data);
			const UPCGMetadata* AnalysisMetadata = AnalysisResultsData ? AnalysisResultsData->ConstMetadata() : nullptr;
			const FPCGMetadataAttributeBase* AnalysisAttributeBase = AnalysisMetadata ? AnalysisMetadata->GetConstAttribute(PCGCopyPointsConstants::SelectedFlagAttributeName) : nullptr;

			if (AnalysisAttributeBase && AnalysisAttributeBase->GetTypeId() == PCG::Private::MetadataTypes<bool>::Id)
			{
				const FPCGMetadataAttribute<bool>* AnalysisAttribute = static_cast<const FPCGMetadataAttribute<bool>*>(AnalysisAttributeBase);
				const int32 NumElements = AnalysisMetadata->GetItemCountForChild();

				Selected.SetNumUninitialized(NumElements);

				NumSelected = 0;

				for (int64 MetadataKey = 0; MetadataKey < NumElements; ++MetadataKey)
				{
					const bool bSelected = AnalysisAttribute->GetValue(MetadataKey);
					Selected[MetadataKey] = bSelected;
					NumSelected += bSelected ? 1 : 0;
				}
			}
			else
			{
				UE_LOG(LogPCG, Warning, TEXT("No analysis data received by copy points kernel, no points will be copied."));
				if (InBinding->InputDataCollection.TaggedData[AnalysisDataIndex].Data->IsA<UPCGProxyForGPUData>())
				{
					UE_LOG(LogPCG, Error, TEXT("Data was not read back."));
				}
				OutDataDesc = {};
				return true;
			}
		}
	}

	FPCGDataCollectionDesc SourcePinDesc;
	FPCGDataCollectionDesc TargetPinDesc;

	const FPCGKernelPin SourceKernelPin(KernelIndex, PCGCopyPointsConstants::SourcePointsLabel, /*bIsInput=*/true);
	const FPCGKernelPin TargetKernelPin(KernelIndex, PCGCopyPointsConstants::TargetPointsLabel, /*bIsInput=*/true);

	ensure(InBinding->ComputeKernelPinDataDesc(SourceKernelPin, &SourcePinDesc));
	ensure(InBinding->ComputeKernelPinDataDesc(TargetKernelPin, &TargetPinDesc));

	FPCGDataCollectionDesc PinDesc;

	const int32 NumSources = SourcePinDesc.DataDescs.Num();
	const int32 NumTargets = TargetPinDesc.DataDescs.Num();
	const int32 NumIterations = CPSettings->bCopyEachSourceOnEveryTarget ? NumSources * NumTargets : FMath::Max(NumSources, NumTargets);

	if (NumSources > 0 && NumTargets > 0 && (CPSettings->bCopyEachSourceOnEveryTarget || NumSources == NumTargets || NumSources == 1 || NumTargets == 1))
	{
		for (int32 I = 0; I < NumIterations; ++I)
		{
			if (!Selected.IsEmpty() && !Selected[I])
			{
				continue;
			}

			const int32 SourceIndex = CPSettings->bCopyEachSourceOnEveryTarget ? (I / NumTargets) : FMath::Min(I, NumSources - 1);
			const int32 TargetIndex = CPSettings->bCopyEachSourceOnEveryTarget ? (I % NumTargets) : FMath::Min(I, NumTargets - 1);

			const FPCGDataDesc& SourceDesc = SourcePinDesc.DataDescs[SourceIndex];
			const FPCGDataDesc& TargetDesc = TargetPinDesc.DataDescs[TargetIndex];

			FPCGDataDesc& ResultDataDesc = PinDesc.DataDescs.Emplace_GetRef(EPCGDataType::Point, SourceDesc.ElementCount * TargetDesc.ElementCount);

			if (CPSettings->AttributeInheritance == EPCGCopyPointsMetadataInheritanceMode::SourceFirst)
			{
				ResultDataDesc.AttributeDescs = SourceDesc.AttributeDescs;

				for (const FPCGKernelAttributeDesc& AttrDesc : TargetDesc.AttributeDescs)
				{
					ResultDataDesc.AttributeDescs.AddUnique(AttrDesc);
				}
			}
			else if (CPSettings->AttributeInheritance == EPCGCopyPointsMetadataInheritanceMode::TargetFirst)
			{
				ResultDataDesc.AttributeDescs = TargetDesc.AttributeDescs;

				for (const FPCGKernelAttributeDesc& AttrDesc : SourceDesc.AttributeDescs)
				{
					ResultDataDesc.AttributeDescs.AddUnique(AttrDesc);
				}
			}
			else if (CPSettings->AttributeInheritance == EPCGCopyPointsMetadataInheritanceMode::SourceOnly)
			{
				ResultDataDesc.AttributeDescs = SourceDesc.AttributeDescs;
			}
			else if (CPSettings->AttributeInheritance == EPCGCopyPointsMetadataInheritanceMode::TargetOnly)
			{
				ResultDataDesc.AttributeDescs = TargetDesc.AttributeDescs;
			}

			if (CPSettings->TagInheritance == EPCGCopyPointsTagInheritanceMode::Source || CPSettings->TagInheritance == EPCGCopyPointsTagInheritanceMode::Both)
			{
				for (const int32 SourceTagStringKey : SourceDesc.TagStringKeys)
				{
					ResultDataDesc.TagStringKeys.AddUnique(SourceTagStringKey);
				}
			}
			
			if (CPSettings->TagInheritance == EPCGCopyPointsTagInheritanceMode::Target || CPSettings->TagInheritance == EPCGCopyPointsTagInheritanceMode::Both)
			{
				for (const int32 TargetTagStringKey : TargetDesc.TagStringKeys)
				{
					ResultDataDesc.TagStringKeys.AddUnique(TargetTagStringKey);
				}
			}
		}
	}

	OutDataDesc = MoveTemp(PinDesc);
	return true;
}

int UPCGCopyPointsKernel::ComputeThreadCount(const UPCGDataBinding* InBinding) const
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
FString UPCGCopyPointsKernel::GetCookedSource(FPCGGPUCompilationContext& InOutContext) const
{
	FString TemplateFile;
	ensure(LoadShaderSourceFile(TEXT("/Plugin/PCG/Private/Elements/PCGCopyPoints.usf"), EShaderPlatform::SP_PCD3D_SM5, &TemplateFile, nullptr));
	return TemplateFile;
}

void UPCGCopyPointsKernel::CreateAdditionalInputDataInterfaces(FPCGGPUCompilationContext& InOutContext, UObject* InObjectOuter, TArray<TObjectPtr<UComputeDataInterface>>& OutDataInterfaces) const
{
	Super::CreateAdditionalInputDataInterfaces(InOutContext, InObjectOuter, OutDataInterfaces);

	TObjectPtr<UPCGCopyPointsDataInterface> NodeDI = InOutContext.NewObject_AnyThread<UPCGCopyPointsDataInterface>(InObjectOuter);
	NodeDI->ProducerKernel = this;

	OutDataInterfaces.Add(NodeDI);
}
#endif

void UPCGCopyPointsKernel::GetInputPins(TArray<FPCGPinProperties>& OutPins) const
{
	const UPCGCopyPointsSettings* CPSettings = CastChecked<UPCGCopyPointsSettings>(GetSettings());

	OutPins.Emplace(PCGCopyPointsConstants::SourcePointsLabel, EPCGDataType::Point);
	OutPins.Emplace(PCGCopyPointsConstants::TargetPointsLabel, EPCGDataType::Point);

	if (CPSettings->bMatchBasedOnAttribute)
	{
		// One bool flag per output data that signals if the output data should be computed or not.
		OutPins.Emplace(PCGCopyPointsConstants::SelectedFlagsPinLabel, EPCGDataType::Param);
	}
}

void UPCGCopyPointsKernel::GetOutputPins(TArray<FPCGPinPropertiesGPU>& OutPins) const
{
	OutPins.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Point);
}

#undef LOCTEXT_NAMESPACE
