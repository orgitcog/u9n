// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGCountUniqueAttributeValuesDataInterface.h"

#include "Compute/PCGComputeCommon.h"
#include "Compute/PCGDataBinding.h"
#include "Compute/BuiltInKernels/PCGCountUniqueAttributeValuesKernel.h"
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

void UPCGCountUniqueAttributeValuesDataInterface::GetSupportedInputs(TArray<FShaderFunctionDefinition>& OutFunctions) const
{
	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("CountUniqueValues_GetAttributeToCountId"))
		.AddReturnType(EShaderFundamentalType::Int);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("CountUniqueValues_GetOutputCountAttributeId"))
		.AddReturnType(EShaderFundamentalType::Int);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("CountUniqueValues_GetOutputValueAttributeId"))
		.AddReturnType(EShaderFundamentalType::Int);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("CountUniqueValues_GetValueIndex"))
		.AddReturnType(EShaderFundamentalType::Uint)
		.AddParam(EShaderFundamentalType::Uint); // InAttributeValue
}

BEGIN_SHADER_PARAMETER_STRUCT(FPCGCountUniqueAttributeValuesDataInterfaceParameters,)
	SHADER_PARAMETER(int32, AttributeToCountId)
	SHADER_PARAMETER(int32, OutputValueAttributeId)
	SHADER_PARAMETER(int32, OutputCountAttributeId)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint32>, AttributeValueToValueIndex)
END_SHADER_PARAMETER_STRUCT()

void UPCGCountUniqueAttributeValuesDataInterface::GetShaderParameters(TCHAR const* UID, FShaderParametersMetadataBuilder& InOutBuilder, FShaderParametersMetadataAllocations& InOutAllocations) const
{
	InOutBuilder.AddNestedStruct<FPCGCountUniqueAttributeValuesDataInterfaceParameters>(UID);
}

void UPCGCountUniqueAttributeValuesDataInterface::GetHLSL(FString& OutHLSL, FString const& InDataInterfaceName) const
{
	TMap<FString, FStringFormatArg> TemplateArgs =
	{
		{ TEXT("DataInterfaceName"), InDataInterfaceName },
	};

	OutHLSL += FString::Format(TEXT(
		"StructuredBuffer<uint> {DataInterfaceName}_AttributeValueToValueIndex;\n"
		"int {DataInterfaceName}_AttributeToCountId;\n"
		"int {DataInterfaceName}_OutputValueAttributeId;\n"
		"int {DataInterfaceName}_OutputCountAttributeId;\n"
		"\n"
		"int CountUniqueValues_GetAttributeToCountId_{DataInterfaceName}()\n"
		"{\n"
		"	return {DataInterfaceName}_AttributeToCountId;\n"
		"}\n"
		"\n"
		"int CountUniqueValues_GetOutputValueAttributeId_{DataInterfaceName}()\n"
		"{\n"
		"	return {DataInterfaceName}_OutputValueAttributeId;\n"
		"}\n"
		"\n"
		"int CountUniqueValues_GetOutputCountAttributeId_{DataInterfaceName}()\n"
		"{\n"
		"	return {DataInterfaceName}_OutputCountAttributeId;\n"
		"}\n"
		"\n"
		"uint CountUniqueValues_GetValueIndex_{DataInterfaceName}(int InAttributeValue)\n"
		"{\n"
		"	return {DataInterfaceName}_AttributeValueToValueIndex[InAttributeValue];\n"
		"}\n"
	), TemplateArgs);
}

UComputeDataProvider* UPCGCountUniqueAttributeValuesDataInterface::CreateDataProvider() const
{
	return NewObject<UPCGCountUniqueAttributeValuesDataProvider>();
}

void UPCGCountUniqueAttributeValuesDataProvider::Initialize(const UComputeDataInterface* InDataInterface, UObject* InBinding, uint64 InInputMask, uint64 InOutputMask)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGCountUniqueAttributeValuesDataProvider::Initialize);

	Super::Initialize(InDataInterface, InBinding, InInputMask, InOutputMask);

	const UPCGCountUniqueAttributeValuesDataInterface* DataInterface = CastChecked<UPCGCountUniqueAttributeValuesDataInterface>(InDataInterface);
	Kernel = DataInterface->ProducerKernel;

	AttributeToCountName = DataInterface->AttributeToCountName;
}

bool UPCGCountUniqueAttributeValuesDataProvider::PrepareForExecute_GameThread(UPCGDataBinding* InBinding)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGCountUniqueAttributeValuesDataInterface::PrepareForExecute_GameThread);
	check(InBinding);

	if (!Super::PrepareForExecute_GameThread(InBinding))
	{
		return false;
	}

	OutputValueAttributeId = InBinding->GetAttributeId(PCGCountUniqueAttributeValuesConstants::ValueAttributeName, EPCGKernelAttributeType::Int);
	check(OutputValueAttributeId != -1);
	OutputCountAttributeId = InBinding->GetAttributeId(PCGCountUniqueAttributeValuesConstants::ValueCountAttributeName, EPCGKernelAttributeType::Int);
	check(OutputCountAttributeId != -1);

	const FPCGDataCollectionDesc* InputDataDesc = InBinding->GetCachedKernelPinDataDesc(Kernel, PCGPinConstants::DefaultInputLabel, /*bIsInput*/true);

	if (!ensure(InputDataDesc))
	{
		return true;
	}

	AttributeToCountId = InBinding->GetAttributeId(AttributeToCountName, EPCGKernelAttributeType::StringKey);

	if (AttributeToCountId != INDEX_NONE)
	{
		InputDataDesc->GetUniqueStringKeyValues(AttributeToCountId, UniqueStringKeyValues);
	}

	MaxAttributeValue = INDEX_NONE;

	for (int32 Key : UniqueStringKeyValues)
	{
		MaxAttributeValue = FMath::Max(MaxAttributeValue, Key);
	}

	return true;
}

FComputeDataProviderRenderProxy* UPCGCountUniqueAttributeValuesDataProvider::GetRenderProxy()
{
	FPCGCountUniqueAttributeValuesProviderProxy::FCountUniqueValuesData_RenderThread Data;
	Data.AttributeToCountId = AttributeToCountId;
	Data.MaxAttributeValue = MaxAttributeValue;
	Data.OutputValueAttributeId = OutputValueAttributeId;
	Data.OutputCountAttributeId = OutputCountAttributeId;
	Data.UniqueStringKeyValues = UniqueStringKeyValues;

	return new FPCGCountUniqueAttributeValuesProviderProxy(MoveTemp(Data));
}

void UPCGCountUniqueAttributeValuesDataProvider::Reset()
{
	Kernel = nullptr;
	AttributeToCountName = NAME_None;
	AttributeToCountId = INDEX_NONE;
	OutputValueAttributeId = INDEX_NONE;
	OutputCountAttributeId = INDEX_NONE;
	MaxAttributeValue = INDEX_NONE;
	UniqueStringKeyValues.Empty();

	Super::Reset();
}

bool FPCGCountUniqueAttributeValuesProviderProxy::IsValid(FValidationData const& InValidationData) const
{
	return (InValidationData.ParameterStructSize == sizeof(FParameters));
}

void FPCGCountUniqueAttributeValuesProviderProxy::AllocateResources(FRDGBuilder& GraphBuilder, FAllocationData const& InAllocationData)
{
	const int32 NumIndicesToAllocate = FMath::Max(1, Data.MaxAttributeValue + 1);

	{
		FRDGBufferDesc Desc = FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), NumIndicesToAllocate);
		Desc.Usage |= BUF_SourceCopy;

		AttributeValueToValueIndex = GraphBuilder.CreateBuffer(Desc, TEXT("PCGAttributeValueToValueIndex"));
		AttributeValueToValueIndexSRV = GraphBuilder.CreateSRV(AttributeValueToValueIndex);
		
		TArray<uint32> ValueIndices;
		ValueIndices.SetNumZeroed(NumIndicesToAllocate);

		for (int Value = 0; Value <= Data.MaxAttributeValue; ++Value)
		{
			const int32 ValueIndex = Data.UniqueStringKeyValues.IndexOfByKey(Value);
			ValueIndices[Value] = static_cast<uint32>(ValueIndex);
		}

		GraphBuilder.QueueBufferUpload(AttributeValueToValueIndex, MakeArrayView(ValueIndices));
	}
}

void FPCGCountUniqueAttributeValuesProviderProxy::GatherDispatchData(FDispatchData const& InDispatchData)
{
	const TStridedView<FParameters> ParameterArray = MakeStridedParameterView<FParameters>(InDispatchData);
	for (int32 InvocationIndex = 0; InvocationIndex < ParameterArray.Num(); ++InvocationIndex)
	{
		FParameters& Parameters = ParameterArray[InvocationIndex];

		Parameters.AttributeToCountId = Data.AttributeToCountId;
		Parameters.OutputValueAttributeId = Data.OutputValueAttributeId;
		Parameters.OutputCountAttributeId = Data.OutputCountAttributeId;
		Parameters.AttributeValueToValueIndex = AttributeValueToValueIndexSRV;
	}
}
