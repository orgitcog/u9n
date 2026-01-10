// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGCopyPointsAnalysisDataInterface.h"

#include "PCGContext.h"
#include "Compute/PCGComputeKernel.h"
#include "Compute/PCGDataBinding.h"
#include "Elements/PCGCopyPoints.h"

#include "GlobalRenderResources.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphResources.h"
#include "RHIResources.h"
#include "ShaderCompilerCore.h"
#include "ShaderCore.h"
#include "ShaderParameterMetadataBuilder.h"
#include "ComputeFramework/ShaderParamTypeDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGCopyPointsAnalysisDataInterface)

void UPCGCopyPointsAnalysisDataInterface::GetSupportedInputs(TArray<FShaderFunctionDefinition>& OutFunctions) const
{
	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("CopyPoints_GetMatchAttributeId"))
		.AddReturnType(FShaderValueType::Get(EShaderFundamentalType::Int));

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("CopyPoints_GetSelectedFlagAttributeId"))
		.AddReturnType(FShaderValueType::Get(EShaderFundamentalType::Int));

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("CopyPoints_GetCopyEachSourceOnEveryTarget"))
		.AddReturnType(FShaderValueType::Get(EShaderFundamentalType::Uint));
}

BEGIN_SHADER_PARAMETER_STRUCT(FPCGCopyPointsAnalysisDataInterfaceParameters,)
	SHADER_PARAMETER(int32, MatchAttributeId)
	SHADER_PARAMETER(int32, SelectedFlagAttributeId)
	SHADER_PARAMETER(uint32, CopyEachSourceOnEveryTarget)
END_SHADER_PARAMETER_STRUCT()

void UPCGCopyPointsAnalysisDataInterface::GetShaderParameters(TCHAR const* UID, FShaderParametersMetadataBuilder& InOutBuilder, FShaderParametersMetadataAllocations& InOutAllocations) const
{
	InOutBuilder.AddNestedStruct<FPCGCopyPointsAnalysisDataInterfaceParameters>(UID);
}

void UPCGCopyPointsAnalysisDataInterface::GetHLSL(FString& OutHLSL, FString const& InDataInterfaceName) const
{
	TMap<FString, FStringFormatArg> TemplateArgs =
	{
		{ TEXT("DataInterfaceName"), InDataInterfaceName },
	};

	OutHLSL += FString::Format(TEXT(
		"int {DataInterfaceName}_MatchAttributeId;\n"
		"int {DataInterfaceName}_SelectedFlagAttributeId;\n"
		"uint {DataInterfaceName}_CopyEachSourceOnEveryTarget;\n"
		"\n"
		"int CopyPoints_GetMatchAttributeId_{DataInterfaceName}() { return {DataInterfaceName}_MatchAttributeId; }\n"
		"int CopyPoints_GetSelectedFlagAttributeId_{DataInterfaceName}() { return {DataInterfaceName}_SelectedFlagAttributeId; }\n"
		"uint CopyPoints_GetCopyEachSourceOnEveryTarget_{DataInterfaceName}() { return {DataInterfaceName}_CopyEachSourceOnEveryTarget; }\n"
		), TemplateArgs);
}

UComputeDataProvider* UPCGCopyPointsAnalysisDataInterface::CreateDataProvider() const
{
	return NewObject<UPCGCopyPointsAnalysisDataProvider>();
}

void UPCGCopyPointsAnalysisDataProvider::Initialize(const UComputeDataInterface* InDataInterface, UObject* InBinding, uint64 InInputMask, uint64 InOutputMask)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGCopyPointsAnalysisDataProvider::Initialize);

	Super::Initialize(InDataInterface, InBinding, InInputMask, InOutputMask);

	const UPCGCopyPointsAnalysisDataInterface* DataInterface = CastChecked<UPCGCopyPointsAnalysisDataInterface>(InDataInterface);
	UPCGDataBinding* Binding = CastChecked<UPCGDataBinding>(InBinding);

	Settings = CastChecked<UPCGCopyPointsSettings>(DataInterface->ProducerKernel->GetSettings());

	bCopyEachSourceOnEveryTarget = Settings->bCopyEachSourceOnEveryTarget;
}

bool UPCGCopyPointsAnalysisDataProvider::PrepareForExecute_GameThread(UPCGDataBinding* InBinding)
{
	if (!Super::PrepareForExecute_GameThread(InBinding))
	{
		return false;
	}

	// Resolve attribute IDs from incoming data.
	const FName MatchAttributeName = Settings->MatchAttribute;
	MatchAttributeId = InBinding->GetAttributeId(MatchAttributeName, EPCGKernelAttributeType::Int);
	ensure(MatchAttributeId != INDEX_NONE);

	SelectedFlagAttributeId = InBinding->GetAttributeId(PCGCopyPointsConstants::SelectedFlagAttributeName, EPCGKernelAttributeType::Bool);
	ensure(SelectedFlagAttributeId != INDEX_NONE);

	return true;
}

FComputeDataProviderRenderProxy* UPCGCopyPointsAnalysisDataProvider::GetRenderProxy()
{
	FPCGCopyPointsAnalysisDataProviderProxy::FCopyPointsAnalysisData_RenderThread ProxyData;
	ProxyData.MatchAttributeId = MatchAttributeId;
	ProxyData.SelectedFlagAttributeId = SelectedFlagAttributeId;
	ProxyData.bCopyEachSourceOnEveryTarget = bCopyEachSourceOnEveryTarget;

	return new FPCGCopyPointsAnalysisDataProviderProxy(MoveTemp(ProxyData));
}

bool FPCGCopyPointsAnalysisDataProviderProxy::IsValid(FValidationData const& InValidationData) const
{
	return InValidationData.ParameterStructSize == sizeof(FParameters)
		&& Data.MatchAttributeId != INDEX_NONE
		&& Data.SelectedFlagAttributeId != INDEX_NONE;
}

void FPCGCopyPointsAnalysisDataProviderProxy::GatherDispatchData(FDispatchData const& InDispatchData)
{
	const TStridedView<FParameters> ParameterArray = MakeStridedParameterView<FParameters>(InDispatchData);
	for (int32 InvocationIndex = 0; InvocationIndex < ParameterArray.Num(); ++InvocationIndex)
	{
		FParameters& Parameters = ParameterArray[InvocationIndex];
		Parameters.MatchAttributeId = Data.MatchAttributeId;
		Parameters.SelectedFlagAttributeId = Data.SelectedFlagAttributeId;
		Parameters.CopyEachSourceOnEveryTarget = Data.bCopyEachSourceOnEveryTarget ? 1 : 0;
	}
}
