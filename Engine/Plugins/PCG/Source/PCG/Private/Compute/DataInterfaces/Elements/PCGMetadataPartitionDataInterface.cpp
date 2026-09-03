// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGMetadataPartitionDataInterface.h"

#include "Compute/PCGComputeCommon.h"
#include "Compute/PCGDataBinding.h"
#include "Elements/Metadata/PCGMetadataPartition.h"
#include "Elements/Metadata/PCGMetadataPartitionKernel.h"

#include "GlobalRenderResources.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphResources.h"
#include "RHIResources.h"
#include "ShaderCompilerCore.h"
#include "ShaderCore.h"
#include "ShaderParameterMetadataBuilder.h"
#include "ComputeFramework/ShaderParamTypeDefinition.h"

void UPCGMetadataPartitionDataInterface::GetSupportedInputs(TArray<FShaderFunctionDefinition>& OutFunctions) const
{
	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("MetadataPartition_GetPartitionAttributeId"))
		.AddReturnType(EShaderFundamentalType::Int);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("MetadataPartition_GetNumPartitions"))
		.AddReturnType(EShaderFundamentalType::Uint);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("MetadataPartition_GetPartitionIndex"))
		.AddReturnType(EShaderFundamentalType::Uint)
		.AddParam(EShaderFundamentalType::Uint); // InAttributeValue

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("MetadataPartition_GetElementIndexToWriteTo"))
		.AddReturnType(EShaderFundamentalType::Uint)
		.AddParam(EShaderFundamentalType::Int) // InInputDataIndex
		.AddParam(EShaderFundamentalType::Uint) // InPartitionIndex
		.AddParam(EShaderFundamentalType::Uint); // InNumPartitions
}

BEGIN_SHADER_PARAMETER_STRUCT(FPCGMetadataPartitionDataInterfaceParameters,)
	SHADER_PARAMETER(int32, PartitionAttributeId)
	SHADER_PARAMETER(uint32, NumPartitions)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint32>, AttributeValueToPartitionIndex)
END_SHADER_PARAMETER_STRUCT()

void UPCGMetadataPartitionDataInterface::GetShaderParameters(TCHAR const* UID, FShaderParametersMetadataBuilder& InOutBuilder, FShaderParametersMetadataAllocations& InOutAllocations) const
{
	InOutBuilder.AddNestedStruct<FPCGMetadataPartitionDataInterfaceParameters>(UID);
}

void UPCGMetadataPartitionDataInterface::GetHLSL(FString& OutHLSL, FString const& InDataInterfaceName) const
{
	TMap<FString, FStringFormatArg> TemplateArgs =
	{
		{ TEXT("DataInterfaceName"), InDataInterfaceName },
	};

	OutHLSL += FString::Format(TEXT(
		"StructuredBuffer<uint> {DataInterfaceName}_AttributeValueToPartitionIndex;\n"
		"int {DataInterfaceName}_PartitionAttributeId;\n"
		"uint {DataInterfaceName}_NumPartitions;\n"
		"\n"
		"int MetadataPartition_GetPartitionAttributeId_{DataInterfaceName}()\n"
		"{\n"
		"	return {DataInterfaceName}_PartitionAttributeId;\n"
		"}\n"
		"\n"
		"uint MetadataPartition_GetNumPartitions_{DataInterfaceName}()\n"
		"{\n"
		"	return {DataInterfaceName}_NumPartitions;\n"
		"}\n"
		"\n"
		"uint MetadataPartition_GetPartitionIndex_{DataInterfaceName}(int InAttributeValue)\n"
		"{\n"
		"	return {DataInterfaceName}_AttributeValueToPartitionIndex[InAttributeValue];\n"
		"}\n"
		"\n"
	), TemplateArgs);
}

UComputeDataProvider* UPCGMetadataPartitionDataInterface::CreateDataProvider() const
{
	return NewObject<UPCGMetaDataPartitionDataProvider>();
}

void UPCGMetaDataPartitionDataProvider::Initialize(const UComputeDataInterface* InDataInterface, UObject* InBinding, uint64 InInputMask, uint64 InOutputMask)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGMetaDataPartitionDataProvider::Initialize);

	Super::Initialize(InDataInterface, InBinding, InInputMask, InOutputMask);

	const UPCGMetadataPartitionDataInterface* DataInterface = CastChecked<UPCGMetadataPartitionDataInterface>(InDataInterface);
	Kernel = DataInterface->ProducerKernel;
}

bool UPCGMetaDataPartitionDataProvider::PrepareForExecute_GameThread(UPCGDataBinding* InBinding)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGMetadataPartitionDataInterface::PrepareForExecute_GameThread);
	check(InBinding);

	if (!Super::PrepareForExecute_GameThread(InBinding))
	{
		return false;
	}

	check(Kernel);
	const UPCGMetadataPartitionSettings* Settings = Cast<UPCGMetadataPartitionSettings>(Kernel->GetSettings());
	check(Settings);

	if (ensure(Settings->PartitionAttributeSelectors.Num() == 1 && Settings->PartitionAttributeSelectors[0].IsBasicAttribute()))
	{
		const FPCGDataCollectionDesc* InputDataDesc = InBinding->GetCachedKernelPinDataDesc(Kernel, PCGPinConstants::DefaultInputLabel, /*bIsInput*/true);

		if (!ensure(InputDataDesc))
		{
			return true;
		}

		PartitionAttributeId = InBinding->GetAttributeId(Settings->PartitionAttributeSelectors[0].GetAttributeName(), EPCGKernelAttributeType::StringKey);
		if (PartitionAttributeId != INDEX_NONE)
		{
			InputDataDesc->GetUniqueStringKeyValues(PartitionAttributeId, UniqueStringKeyValues);
		}

		MaxAttributeValue = INDEX_NONE;
		for (int32 Key : UniqueStringKeyValues)
		{
			MaxAttributeValue = FMath::Max(MaxAttributeValue, Key);
		}

		NumInputData = InputDataDesc->DataDescs.Num();
		NumPartitions = UniqueStringKeyValues.Num();
	}

	return true;
}

FComputeDataProviderRenderProxy* UPCGMetaDataPartitionDataProvider::GetRenderProxy()
{
	return new FPCGMetaDataPartitionProviderProxy(PartitionAttributeId, NumInputData, NumPartitions, MaxAttributeValue, UniqueStringKeyValues);
}

void UPCGMetaDataPartitionDataProvider::Reset()
{
	Kernel = nullptr;
	PartitionAttributeId = INDEX_NONE;
	NumInputData = 0;
	NumPartitions = 0;
	MaxAttributeValue = INDEX_NONE;
	UniqueStringKeyValues.Empty();

	Super::Reset();
}

bool FPCGMetaDataPartitionProviderProxy::IsValid(FValidationData const& InValidationData) const
{
	return InValidationData.ParameterStructSize == sizeof(FParameters)
		&& !UniqueStringKeyValues.IsEmpty();
}

void FPCGMetaDataPartitionProviderProxy::AllocateResources(FRDGBuilder& GraphBuilder, FAllocationData const& InAllocationData)
{
	const int32 NumPartitionsToAllocate = FMath::Max(1u, NumInputData) * FMath::Max(1u, NumPartitions);
	const int32 NumIndicesToAllocate = FMath::Max(1, MaxAttributeValue + 1);

	TArray<uint32> Zeros;
	Zeros.SetNumZeroed(FMath::Max(NumPartitionsToAllocate, NumIndicesToAllocate));

	{
		FRDGBufferDesc Desc = FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), NumIndicesToAllocate);
		Desc.Usage |= BUF_SourceCopy;

		AttributeValueToPartitionIndex = GraphBuilder.CreateBuffer(Desc, TEXT("PCGAttributeValueToPartitionIndex"));
		AttributeValueToPartitionIndexSRV = GraphBuilder.CreateSRV(AttributeValueToPartitionIndex);

		int PartitionCounter = 0;
		// Skipping default empty string.
		for (int Index = 1; Index <= MaxAttributeValue; ++Index)
		{
			const int PartitionIndex = UniqueStringKeyValues.IndexOfByKey(Index);
			if (PartitionIndex != INDEX_NONE) // TODO - probably should make it uint and give it index_nones.
			{
				Zeros[Index] = PartitionIndex;
			}
		}

		GraphBuilder.QueueBufferUpload(AttributeValueToPartitionIndex, Zeros.GetData(), NumIndicesToAllocate * Zeros.GetTypeSize());
	}
}

void FPCGMetaDataPartitionProviderProxy::GatherDispatchData(FDispatchData const& InDispatchData)
{
	const TStridedView<FParameters> ParameterArray = MakeStridedParameterView<FParameters>(InDispatchData);
	for (int32 InvocationIndex = 0; InvocationIndex < ParameterArray.Num(); ++InvocationIndex)
	{
		FParameters& Parameters = ParameterArray[InvocationIndex];

		Parameters.PartitionAttributeId = PartitionAttributeId;
		Parameters.NumPartitions = NumPartitions;
		Parameters.AttributeValueToPartitionIndex = AttributeValueToPartitionIndexSRV;
	}
}
