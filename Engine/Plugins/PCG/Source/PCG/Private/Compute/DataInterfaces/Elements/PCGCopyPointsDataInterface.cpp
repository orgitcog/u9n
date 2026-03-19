// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGCopyPointsDataInterface.h"

#include "PCGContext.h"
#include "PCGParamData.h"
#include "Compute/PCGComputeGraph.h"
#include "Compute/PCGDataBinding.h"
#include "Compute/Data/PCGProxyForGPUData.h"
#include "Elements/PCGCopyPoints.h"
#include "Elements/PCGCopyPointsKernel.h"

#include "GlobalRenderResources.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphResources.h"
#include "RHIResources.h"
#include "ShaderCompilerCore.h"
#include "ShaderCore.h"
#include "ShaderParameterMetadataBuilder.h"
#include "SystemTextures.h"
#include "ComputeFramework/ComputeKernelPermutationVector.h"
#include "ComputeFramework/ShaderParamTypeDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGCopyPointsDataInterface)

void UPCGCopyPointsDataInterface::GetSupportedInputs(TArray<FShaderFunctionDefinition>& OutFunctions) const
{
	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("CopyPoints_GetRotationInheritance"))
		.AddReturnType(FShaderValueType::Get(EShaderFundamentalType::Uint));

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("CopyPoints_GetApplyTargetRotationToPositions"))
		.AddReturnType(FShaderValueType::Get(EShaderFundamentalType::Uint));

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("CopyPoints_GetScaleInheritance"))
		.AddReturnType(FShaderValueType::Get(EShaderFundamentalType::Uint));

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("CopyPoints_GetApplyTargetScaleToPositions"))
		.AddReturnType(FShaderValueType::Get(EShaderFundamentalType::Uint));

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("CopyPoints_GetColorInheritance"))
		.AddReturnType(FShaderValueType::Get(EShaderFundamentalType::Uint));

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("CopyPoints_GetSeedInheritance"))
		.AddReturnType(FShaderValueType::Get(EShaderFundamentalType::Uint));

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("CopyPoints_GetAttributeInheritance"))
		.AddReturnType(FShaderValueType::Get(EShaderFundamentalType::Uint));

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("CopyPoints_GetCopyEachSourceOnEveryTarget"))
		.AddReturnType(FShaderValueType::Get(EShaderFundamentalType::Bool));

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("CopyPoints_GetSourceAndTargetDataIndices"))
		.AddParam(EShaderFundamentalType::Uint) // InOutputDataIndex
		.AddParam(EShaderFundamentalType::Uint) // InSourceDataCount
		.AddParam(EShaderFundamentalType::Uint) // InTargetDataCount
		.AddParam(EShaderFundamentalType::Uint, 0, 0, EShaderParamModifier::Out) // OutSourceIndex
		.AddParam(EShaderFundamentalType::Uint, 0, 0, EShaderParamModifier::Out); // OutTargetIndex
}

BEGIN_SHADER_PARAMETER_STRUCT(FPCGCopyPointsDataInterfaceParameters,)
	SHADER_PARAMETER(uint32, RotationInheritance)
	SHADER_PARAMETER(uint32, ApplyTargetRotationToPositions)
	SHADER_PARAMETER(uint32, ScaleInheritance)
	SHADER_PARAMETER(uint32, ApplyTargetScaleToPositions)
	SHADER_PARAMETER(uint32, ColorInheritance)
	SHADER_PARAMETER(uint32, SeedInheritance)
	SHADER_PARAMETER(uint32, AttributeInheritance)
	SHADER_PARAMETER(uint32, CopyEachSourceOnEveryTarget)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FUintVector2>, SourceAndTargetDataIndices)
END_SHADER_PARAMETER_STRUCT()

void UPCGCopyPointsDataInterface::GetShaderParameters(TCHAR const* UID, FShaderParametersMetadataBuilder& InOutBuilder, FShaderParametersMetadataAllocations& InOutAllocations) const
{
	InOutBuilder.AddNestedStruct<FPCGCopyPointsDataInterfaceParameters>(UID);
}

void UPCGCopyPointsDataInterface::GetPermutations(FComputeKernelPermutationVector& OutPermutationVector) const
{
	OutPermutationVector.AddPermutation(TEXT("USE_INPUT_DATA_INDICES_BUFFER"), 2);
}

void UPCGCopyPointsDataInterface::GetHLSL(FString& OutHLSL, FString const& InDataInterfaceName) const
{
	TMap<FString, FStringFormatArg> TemplateArgs =
	{
		{ TEXT("DataInterfaceName"), InDataInterfaceName },
	};

	OutHLSL += FString::Format(TEXT(
		"uint {DataInterfaceName}_RotationInheritance;\n"
		"uint {DataInterfaceName}_ApplyTargetRotationToPositions;\n"
		"uint {DataInterfaceName}_ScaleInheritance;\n"
		"uint {DataInterfaceName}_ApplyTargetScaleToPositions;\n"
		"uint {DataInterfaceName}_ColorInheritance;\n"
		"uint {DataInterfaceName}_SeedInheritance;\n"
		"uint {DataInterfaceName}_AttributeInheritance;\n"
		"uint {DataInterfaceName}_CopyEachSourceOnEveryTarget;\n"
		"StructuredBuffer<uint2> {DataInterfaceName}_SourceAndTargetDataIndices;\n"
		"\n"
		"uint CopyPoints_GetRotationInheritance_{DataInterfaceName}() { return {DataInterfaceName}_RotationInheritance; }\n"
		"uint CopyPoints_GetApplyTargetRotationToPositions_{DataInterfaceName}() { return {DataInterfaceName}_ApplyTargetRotationToPositions; }\n"
		"uint CopyPoints_GetScaleInheritance_{DataInterfaceName}() { return {DataInterfaceName}_ScaleInheritance; }\n"
		"uint CopyPoints_GetApplyTargetScaleToPositions_{DataInterfaceName}() { return {DataInterfaceName}_ApplyTargetScaleToPositions; }\n"
		"uint CopyPoints_GetColorInheritance_{DataInterfaceName}() { return {DataInterfaceName}_ColorInheritance; }\n"
		"uint CopyPoints_GetSeedInheritance_{DataInterfaceName}() { return {DataInterfaceName}_SeedInheritance; }\n"
		"uint CopyPoints_GetAttributeInheritance_{DataInterfaceName}() { return {DataInterfaceName}_AttributeInheritance; }\n"
		"uint CopyPoints_GetCopyEachSourceOnEveryTarget_{DataInterfaceName}() { return {DataInterfaceName}_CopyEachSourceOnEveryTarget; }\n"
		"\n"
		"void CopyPoints_GetSourceAndTargetDataIndices_{DataInterfaceName}(uint InOutputDataIndex, int InSourceDataCount, int InTargetDataCount, out uint OutSourceIndex, out uint OutTargetIndex)\n"
		"{\n"
		"#if USE_INPUT_DATA_INDICES_BUFFER\n"
		"	const uint2 Indices = {DataInterfaceName}_SourceAndTargetDataIndices[InOutputDataIndex];\n"
		"	OutSourceIndex = Indices[0];\n"
		"	OutTargetIndex = Indices[1];\n"
		"#else\n"
		"	if (CopyPoints_GetCopyEachSourceOnEveryTarget_{DataInterfaceName}())\n"
		"	{\n"
		"		OutSourceIndex = InOutputDataIndex / InTargetDataCount;\n"
		"		OutTargetIndex = InOutputDataIndex % InTargetDataCount;\n"
		"	}\n"
		"	else\n"
		"	{\n"
		"		OutSourceIndex = clamp(InOutputDataIndex, 0u, (uint)(InSourceDataCount - 1));\n"
		"		OutTargetIndex = clamp(InOutputDataIndex, 0u, (uint)(InTargetDataCount - 1));\n"
		"	}\n"
		"#endif\n"
		"}\n"
		), TemplateArgs);
}

UComputeDataProvider* UPCGCopyPointsDataInterface::CreateDataProvider() const
{
	return NewObject<UPCGCopyPointsDataProvider>();
}

bool UPCGCopyPointsDataProvider::PerformPreExecuteReadbacks_GameThread(UPCGDataBinding* InBinding)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGCopyPointsDataProvider::PerformPreExecuteReadbacks_GameThread);
	check(InBinding);

	if (!Super::PerformPreExecuteReadbacks_GameThread(InBinding))
	{
		return false;
	}

	const UPCGCopyPointsSettings* Settings = CastChecked<UPCGCopyPointsSettings>(ProducerKernel->GetSettings());
	if (!Settings->bMatchBasedOnAttribute)
	{
		// If we're not matching based on attribute then we don't need to do any readbacks.
		return true;
	}

	const int32 AnalysisDataIndex = ProducerKernel ? InBinding->GetFirstInputDataIndex(ProducerKernel, PCGCopyPointsConstants::SelectedFlagsPinLabel) : INDEX_NONE;
	if (AnalysisDataIndex == INDEX_NONE)
	{
		// Analysis data was not produced, nothing to read back.
		return true;
	}

	// Readback analysis data - poll until readback complete.
	return InBinding->ReadbackInputDataToCPU(AnalysisDataIndex);
}

bool UPCGCopyPointsDataProvider::PrepareForExecute_GameThread(UPCGDataBinding* InBinding)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGCopyPointsDataProvider::PrepareForExecute_GameThread);
	check(InBinding);

	if (!Super::PrepareForExecute_GameThread(InBinding))
	{
		return false;
	}

	const UPCGCopyPointsSettings* Settings = CastChecked<UPCGCopyPointsSettings>(ProducerKernel->GetSettings());

	if (Settings->bMatchBasedOnAttribute)
	{
		const int32 AnalysisDataIndex = InBinding->GetFirstInputDataIndex(ProducerKernel, PCGCopyPointsConstants::SelectedFlagsPinLabel);
		if (AnalysisDataIndex == INDEX_NONE)
		{
			return true;
		}

		const FPCGDataCollectionDesc* SourcePinDesc = InBinding->GetCachedKernelPinDataDesc(ProducerKernel, PCGCopyPointsConstants::SourcePointsLabel, /*bIsInput=*/true);
		const FPCGDataCollectionDesc* TargetPinDesc = InBinding->GetCachedKernelPinDataDesc(ProducerKernel, PCGCopyPointsConstants::TargetPointsLabel, /*bIsInput=*/true);

		if (!ensure(SourcePinDesc) || !ensure(TargetPinDesc))
		{
			return true;
		}

		const int32 NumOutputs = Settings->bCopyEachSourceOnEveryTarget ? SourcePinDesc->DataDescs.Num() * TargetPinDesc->DataDescs.Num() : FMath::Max(SourcePinDesc->DataDescs.Num(), TargetPinDesc->DataDescs.Num());
		SourceAndTargetDataIndices.Reserve(NumOutputs);

		const UPCGParamData* AnalysisResultsData = Cast<UPCGParamData>(InBinding->InputDataCollection.TaggedData[AnalysisDataIndex].Data);
		const UPCGMetadata* AnalysisMetadata = AnalysisResultsData ? AnalysisResultsData->ConstMetadata() : nullptr;
		const FPCGMetadataAttributeBase* AnalysisAttributeBase = AnalysisMetadata ? AnalysisMetadata->GetConstAttribute(PCGCopyPointsConstants::SelectedFlagAttributeName) : nullptr;
		const int32 NumElements = AnalysisAttributeBase ? AnalysisMetadata->GetItemCountForChild() : INDEX_NONE;
		const FPCGMetadataAttribute<bool>* SelectedFlagsAttribute = ((NumElements == NumOutputs) && AnalysisAttributeBase && (AnalysisAttributeBase->GetTypeId() == PCG::Private::MetadataTypes<bool>::Id)) ? static_cast<const FPCGMetadataAttribute<bool>*>(AnalysisAttributeBase) : nullptr;

		if (SelectedFlagsAttribute)
		{
			if (Settings->bCopyEachSourceOnEveryTarget)
			{
				for (int32 S = 0; S < SourcePinDesc->DataDescs.Num(); ++S)
				{
					for (int32 T = 0; T < TargetPinDesc->DataDescs.Num(); ++T)
					{
						if (SelectedFlagsAttribute->GetValue(S * TargetPinDesc->DataDescs.Num() + T))
						{
							SourceAndTargetDataIndices.Emplace(S, T);
						}
					}
				}
			}
			else
			{
				for (int32 OutputIndex = 0; OutputIndex < NumOutputs; ++OutputIndex)
				{
					const int32 S = FMath::Clamp(OutputIndex, 0, SourcePinDesc->DataDescs.Num() - 1);
					const int32 T = FMath::Clamp(OutputIndex, 0, TargetPinDesc->DataDescs.Num() - 1);

					if (SelectedFlagsAttribute->GetValue(OutputIndex))
					{
						SourceAndTargetDataIndices.Emplace(S, T);
					}
				}
			}
		}
	}

	return true;
}

FComputeDataProviderRenderProxy* UPCGCopyPointsDataProvider::GetRenderProxy()
{
	const UPCGCopyPointsSettings* Settings = CastChecked<UPCGCopyPointsSettings>(ProducerKernel->GetSettings());

	FPCGCopyPointsDataProviderProxy::FCopyPointsData_RenderThread ProxyData;
	ProxyData.RotationInheritance = static_cast<uint32>(Settings->RotationInheritance);
	ProxyData.ApplyTargetRotationToPositions = static_cast<uint32>((Settings->RotationInheritance != EPCGCopyPointsInheritanceMode::Source || Settings->bApplyTargetRotationToPositions) ? 1 : 0);
	ProxyData.ScaleInheritance = static_cast<uint32>(Settings->ScaleInheritance);
	ProxyData.ApplyTargetScaleToPositions = static_cast<uint32>((Settings->ScaleInheritance != EPCGCopyPointsInheritanceMode::Source || Settings->bApplyTargetScaleToPositions) ? 1 : 0);
	ProxyData.ColorInheritance = static_cast<uint32>(Settings->ColorInheritance);
	ProxyData.SeedInheritance = static_cast<uint32>(Settings->SeedInheritance);
	ProxyData.AttributeInheritance = static_cast<uint32>(Settings->AttributeInheritance);
	ProxyData.bCopyEachSourceOnEveryTarget = static_cast<uint32>(Settings->bCopyEachSourceOnEveryTarget);
	ProxyData.SourceAndTargetDataIndices = SourceAndTargetDataIndices;

	return new FPCGCopyPointsDataProviderProxy(MoveTemp(ProxyData));
}

void UPCGCopyPointsDataProvider::Reset()
{
	Super::Reset();

	SourceAndTargetDataIndices.Empty();
}

bool FPCGCopyPointsDataProviderProxy::IsValid(FValidationData const& InValidationData) const
{
	return InValidationData.ParameterStructSize == sizeof(FParameters);
}

struct FCopyPointsDataInterfacePermutationIds
{
	FCopyPointsDataInterfacePermutationIds(FComputeKernelPermutationVector const& PermutationVector)
	{
		{
			static FString Name(TEXT("USE_INPUT_DATA_INDICES_BUFFER"));
			static uint32 Hash = GetTypeHash(Name);
			UseInputDataIndicesBuffer = PermutationVector.GetPermutationBits(Name, Hash, 1);
		}
	}

	uint32 UseInputDataIndicesBuffer = 0;
};

void FPCGCopyPointsDataProviderProxy::GatherPermutations(FPermutationData& InOutPermutationData) const
{
	FCopyPointsDataInterfacePermutationIds PermutationIds(InOutPermutationData.PermutationVector);

	for (int32 InvocationIndex = 0; InvocationIndex < InOutPermutationData.NumInvocations; ++InvocationIndex)
	{
		// Select permutation based on whether we have data indices or not.
		InOutPermutationData.PermutationIds[InvocationIndex] |= (Data.SourceAndTargetDataIndices.IsEmpty() ? 0 : PermutationIds.UseInputDataIndicesBuffer);
	}
}

void FPCGCopyPointsDataProviderProxy::AllocateResources(FRDGBuilder& GraphBuilder, FAllocationData const& InAllocationData)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGCopyPointsDataProviderProxy::AllocateResources);

	if (!Data.SourceAndTargetDataIndices.IsEmpty())
	{
		const FRDGBufferDesc IndexPairsDesc = FRDGBufferDesc::CreateStructuredDesc(sizeof(FUintVector2), Data.SourceAndTargetDataIndices.Num());

		SourceAndTargetDataIndicesBuffer = GraphBuilder.CreateBuffer(IndexPairsDesc, TEXT("PCGCopyPoints_SourceAndTargetDataIndices"));
		SourceAndTargetDataIndicesBufferSRV = GraphBuilder.CreateSRV(SourceAndTargetDataIndicesBuffer);

		GraphBuilder.QueueBufferUpload(SourceAndTargetDataIndicesBuffer, MakeArrayView(Data.SourceAndTargetDataIndices));
	}
	else
	{
		// Fallback
		SourceAndTargetDataIndicesBufferSRV = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(GSystemTextures.GetDefaultStructuredBuffer(GraphBuilder, sizeof(FUintVector2))));
	}
}

void FPCGCopyPointsDataProviderProxy::GatherDispatchData(FDispatchData const& InDispatchData)
{
	const TStridedView<FParameters> ParameterArray = MakeStridedParameterView<FParameters>(InDispatchData);
	for (int32 InvocationIndex = 0; InvocationIndex < ParameterArray.Num(); ++InvocationIndex)
	{
		FParameters& Parameters = ParameterArray[InvocationIndex];
		Parameters.RotationInheritance = Data.RotationInheritance;
		Parameters.ApplyTargetRotationToPositions = Data.ApplyTargetRotationToPositions;
		Parameters.ScaleInheritance = Data.ScaleInheritance;
		Parameters.ApplyTargetScaleToPositions = Data.ApplyTargetScaleToPositions;
		Parameters.ColorInheritance = Data.ColorInheritance;
		Parameters.SeedInheritance = Data.SeedInheritance;
		Parameters.AttributeInheritance = Data.AttributeInheritance;
		Parameters.CopyEachSourceOnEveryTarget = Data.bCopyEachSourceOnEveryTarget;
		Parameters.SourceAndTargetDataIndices = SourceAndTargetDataIndicesBufferSRV;
	}
}
