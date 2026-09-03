// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGCopyPointsAnalysisKernel.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGModule.h"
#include "Compute/PCGComputeGraph.h"
#include "Compute/PCGDataBinding.h"
#include "Compute/PCGPinPropertiesGPU.h"
#include "Compute/DataInterfaces/Elements/PCGCopyPointsAnalysisDataInterface.h"
#include "Compute/Elements/PCGComputeGraphElement.h"
#include "Elements/PCGCopyPoints.h"
#include "Elements/PCGCopyPointsKernelShared.h"
#include "Graph/PCGGPUGraphCompilationContext.h"

#include "ShaderCompilerCore.h"
#include "Engine/StaticMesh.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGCopyPointsAnalysisKernel)

#define LOCTEXT_NAMESPACE "PCGCopyPointsAnalysisKernel"

bool UPCGCopyPointsAnalysisKernel::IsKernelDataValid(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGCopyPointsAnalysisKernel::IsKernelDataValid);

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

bool UPCGCopyPointsAnalysisKernel::ComputeOutputBindingDataDesc(const UPCGComputeGraph* InGraph, FName InOutputPinLabel, UPCGDataBinding* InBinding, FPCGDataCollectionDesc& OutDataDesc) const
{
	check(InGraph);
	check(InBinding);

	// Code assumes single output pin.
	if (!ensure(InOutputPinLabel == PCGPinConstants::DefaultOutputLabel))
	{
		return false;
	}

	FPCGDataCollectionDesc SourcesDesc, TargetsDesc;

	const FPCGKernelPin SourceKernelPin(KernelIndex, PCGCopyPointsConstants::SourcePointsLabel, /*bIsInput=*/true);
	const FPCGKernelPin TargetKernelPin(KernelIndex, PCGCopyPointsConstants::TargetPointsLabel, /*bIsInput=*/true);

	ensure(InBinding->ComputeKernelPinDataDesc(SourceKernelPin, &SourcesDesc));
	ensure(InBinding->ComputeKernelPinDataDesc(TargetKernelPin, &TargetsDesc));

	const uint32 NumSourceDatas = SourcesDesc.DataDescs.Num();
	const uint32 NumTargetDatas = TargetsDesc.DataDescs.Num();

	// Output: a single attribute set with a single attribute true/false value per NumSource*NumTarget possible outputs.
	FPCGDataCollectionDesc OutDesc;

	FPCGDataDesc& DataDesc = OutDesc.DataDescs.Emplace_GetRef();
	DataDesc.Type = EPCGDataType::Param;

	const UPCGCopyPointsSettings* CopyPointSettings = CastChecked<UPCGCopyPointsSettings>(GetSettings());

	if (CopyPointSettings->bCopyEachSourceOnEveryTarget)
	{
		DataDesc.ElementCount = NumSourceDatas * NumTargetDatas;
	}
	else
	{
		// Validation should have ensured already that these are valid: (S, T) = (N, N), (N, 1) or (1, N).
		DataDesc.ElementCount = FMath::Max(NumSourceDatas, NumTargetDatas);
	}

	FPCGKernelAttributeKey SelectedFlagAttributeKey = FPCGKernelAttributeKey(PCGCopyPointsConstants::SelectedFlagAttributeName, EPCGKernelAttributeType::Bool);
	DataDesc.AddAttribute(SelectedFlagAttributeKey, InBinding);

	OutDataDesc = MoveTemp(OutDesc);

	return true;
}

int UPCGCopyPointsAnalysisKernel::ComputeThreadCount(const UPCGDataBinding* InBinding) const
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
FString UPCGCopyPointsAnalysisKernel::GetCookedSource(FPCGGPUCompilationContext& InOutContext) const
{
	FString TemplateFile;
	ensure(LoadShaderSourceFile(TEXT("/Plugin/PCG/Private/Elements/PCGCopyPointsAnalysis.usf"), EShaderPlatform::SP_PCD3D_SM5, &TemplateFile, nullptr));
	return TemplateFile;
}

void UPCGCopyPointsAnalysisKernel::CreateAdditionalInputDataInterfaces(FPCGGPUCompilationContext& InOutContext, UObject* InObjectOuter, TArray<TObjectPtr<UComputeDataInterface>>& OutDataInterfaces) const
{
	Super::CreateAdditionalInputDataInterfaces(InOutContext, InObjectOuter, OutDataInterfaces);

	TObjectPtr<UPCGCopyPointsAnalysisDataInterface> NodeDI = InOutContext.NewObject_AnyThread<UPCGCopyPointsAnalysisDataInterface>(InObjectOuter);
	NodeDI->ProducerKernel = this;

	OutDataInterfaces.Add(NodeDI);
}
#endif

void UPCGCopyPointsAnalysisKernel::GetKernelAttributeKeys(TArray<FPCGKernelAttributeKey>& OutKeys) const
{
	// Register the attribute this node creates.
	OutKeys.AddUnique(FPCGKernelAttributeKey(PCGCopyPointsConstants::SelectedFlagAttributeName, EPCGKernelAttributeType::Bool));
}

void UPCGCopyPointsAnalysisKernel::GetInputPins(TArray<FPCGPinProperties>& OutPins) const
{
	OutPins.Emplace(PCGCopyPointsConstants::SourcePointsLabel, EPCGDataType::Point);
	OutPins.Emplace(PCGCopyPointsConstants::TargetPointsLabel, EPCGDataType::Point);
}

void UPCGCopyPointsAnalysisKernel::GetOutputPins(TArray<FPCGPinPropertiesGPU>& OutPins) const
{
	OutPins.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Point);
}

#undef LOCTEXT_NAMESPACE
