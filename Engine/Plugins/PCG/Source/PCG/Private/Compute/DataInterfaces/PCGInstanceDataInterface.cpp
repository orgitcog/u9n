// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGInstanceDataInterface.h"

#include "PCGModule.h"
#include "PCGSettings.h"
#include "PCGSceneWriterCS.h"
#include "Components/PCGProceduralISMComponent.h"
#include "Compute/PCGComputeKernel.h"
#include "Compute/PCGDataBinding.h"

#include "GlobalRenderResources.h"
#include "GlobalShader.h"
#include "GPUSceneWriter.h"
#include "InstanceDataSceneProxy.h"
#include "InstancedSkinnedMeshSceneProxyDesc.h"
#include "InstancedStaticMeshSceneProxyDesc.h"
#include "PrimitiveSceneDesc.h"
#include "PrimitiveSceneInfo.h"
#include "PrimitiveSceneProxy.h"
#include "RenderCaptureInterface.h"
#include "RendererUtils.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RHIResources.h"
#include "RHIStaticStates.h"
#include "SceneInterface.h"
#include "ShaderCompilerCore.h"
#include "ShaderCore.h"
#include "ShaderParameterMetadataBuilder.h"
#include "ShaderParameterStruct.h"
#include "Algo/AnyOf.h"
#include "Components/InstancedSkinnedMeshComponent.h"
#include "ComputeFramework/ComputeKernelPermutationSet.h"
#include "ComputeFramework/ShaderParamTypeDefinition.h"
#include "Containers/Ticker.h"
#include "Engine/Texture.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGInstanceDataInterface)

#define PCG_INSTANCE_DATA_LOGGING 0

namespace PCGInstanceDataInterface
{
#if !UE_BUILD_SHIPPING
	static int32 TriggerGPUCaptureDispatchIndex = 0;
	static FAutoConsoleVariableRef CVarTriggerGPUCaptureDispatchIndex(
		TEXT("pcg.GPU.TriggerRenderCaptures.InstanceSceneWriter"),
		TriggerGPUCaptureDispatchIndex,
		TEXT("Index of the next dispatch to capture. I.e. if set to 2, will ignore one dispatch and then trigger a capture on the next one."),
		ECVF_RenderThreadSafe
	);
#endif // !UE_BUILD_SHIPPING
}

void UPCGInstanceDataInterface::GetSupportedOutputs(TArray<FShaderFunctionDefinition>& OutFunctions) const
{
	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("InstanceData_GetNumPrimitives"))
		.AddReturnType(EShaderFundamentalType::Uint);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("InstanceData_GetNumInstancesAllPrimitives"))
		.AddReturnType(EShaderFundamentalType::Uint);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("InstanceData_GetIndexToWriteTo"))
		.AddReturnType(EShaderFundamentalType::Int)
		.AddParam(EShaderFundamentalType::Uint); // InPrimitiveIndex

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("InstanceData_WriteInstanceLocalToWorld"))
		.AddParam(EShaderFundamentalType::Uint) // InInstanceIndex
		.AddParam(EShaderFundamentalType::Float, 4, 4);  // InTransform

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("InstanceData_WriteCustomFloat"))
		.AddParam(EShaderFundamentalType::Uint) // InInstanceIndex
		.AddParam(EShaderFundamentalType::Uint) // InCustomFloatIndex
		.AddParam(EShaderFundamentalType::Float); // InValue

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("InstanceData_WriteCustomFloatRaw"))
		.AddParam(EShaderFundamentalType::Uint) // InInstanceIndex
		.AddParam(EShaderFundamentalType::Uint) // InCustomFloatIndex
		.AddParam(EShaderFundamentalType::Uint); // InValueAsUint
}

BEGIN_SHADER_PARAMETER_STRUCT(FPCGInstanceDataInterfaceParameters,)
	SHADER_PARAMETER_ARRAY(FUintVector4, PrimitiveParameters, [PCGComputeConstants::MAX_PRIMITIVE_COMPONENTS_PER_SPAWNER]) // (NumInstancesAllocated, InstanceOffset, Unused, Unused)
	SHADER_PARAMETER(uint32, NumPrimitives)
	SHADER_PARAMETER(uint32, NumInstancesAllPrimitives)
	SHADER_PARAMETER(uint32, NumCustomFloatsPerInstance)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FUintVector4>, InstanceData)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint32>, InstanceCustomFloatData)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint32>, WriteCounters)
END_SHADER_PARAMETER_STRUCT()

void UPCGInstanceDataInterface::GetShaderParameters(TCHAR const* UID, FShaderParametersMetadataBuilder& InOutBuilder, FShaderParametersMetadataAllocations& InOutAllocations) const
{
	InOutBuilder.AddNestedStruct<FPCGInstanceDataInterfaceParameters>(UID);
}

TCHAR const* UPCGInstanceDataInterface::TemplateFilePath = TEXT("/Plugin/PCG/Private/PCGInstanceDataInterface.ush");

TCHAR const* UPCGInstanceDataInterface::GetShaderVirtualPath() const
{
	return TemplateFilePath;
}

void UPCGInstanceDataInterface::GetShaderHash(FString& InOutKey) const
{
	GetShaderFileHash(TemplateFilePath, EShaderPlatform::SP_PCD3D_SM5).AppendString(InOutKey);
}

void UPCGInstanceDataInterface::GetHLSL(FString& OutHLSL, FString const& InDataInterfaceName) const
{
	TMap<FString, FStringFormatArg> TemplateArgs =
	{
		{ TEXT("DataInterfaceName"), InDataInterfaceName },
	};

	FString TemplateFile;
	if (ensure(LoadShaderSourceFile(TemplateFilePath, EShaderPlatform::SP_PCD3D_SM5, &TemplateFile, nullptr)))
	{
		OutHLSL += FString::Format(*TemplateFile, TemplateArgs);
	}
}

void UPCGInstanceDataInterface::GetDefines(FComputeKernelDefinitionSet& OutDefinitionSet) const
{
	Super::GetDefines(OutDefinitionSet);

	OutDefinitionSet.Defines.Add(FComputeKernelDefinition(TEXT("PCG_MAX_PRIMITIVES"), FString::FromInt(PCGComputeConstants::MAX_PRIMITIVE_COMPONENTS_PER_SPAWNER)));
}

UComputeDataProvider* UPCGInstanceDataInterface::CreateDataProvider() const
{
	return NewObject<UPCGInstanceDataProvider>();
}

bool UPCGInstanceDataProvider::PrepareForExecute_GameThread(UPCGDataBinding* InBinding)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGInstanceDataProvider::PrepareForExecute_GameThread);
	UPCGDataBinding* Binding = CastChecked<UPCGDataBinding>(InBinding);

	if (!Binding->CompletedMeshSpawners.Contains(ProducerKernel))
	{
		// The static mesh data interface(s) set this up, so wait until it is ready.
		return false;
	}

	FPCGSpawnerPrimitives* FoundPrimitives = Binding->MeshSpawnersToPrimitives.Find(ProducerKernel);

	if (!FoundPrimitives)
	{
		return true;
	}

	if (!ensure(FoundPrimitives->Primitives.Num() <= PCGComputeConstants::MAX_PRIMITIVE_COMPONENTS_PER_SPAWNER))
	{
		// Last resort - should be clamped earlier during setup and not come to this.
		FoundPrimitives->Primitives.SetNum(PCGComputeConstants::MAX_PRIMITIVE_COMPONENTS_PER_SPAWNER);
	}

	NumInstancesAllPrimitives = 0;
	Primitives.Reserve(FoundPrimitives->Primitives.Num());

	for (const FPCGSpawnerPrimitive& Primitive : FoundPrimitives->Primitives)
	{
		Primitives.Add(FPCGPrimitiveComponentAndNumInstances(Primitive.Component, Primitive.NumInstances));
		NumInstancesAllPrimitives += Primitive.NumInstances;
	}

	NumCustomFloatsPerInstance = FoundPrimitives->NumCustomFloats;

	return true;
}

bool UPCGInstanceDataProvider::PostExecute(UPCGDataBinding* InBinding)
{
	if (!Super::PostExecute(InBinding))
	{
		return false;
	}

	return bWroteInstances;
}

FComputeDataProviderRenderProxy* UPCGInstanceDataProvider::GetRenderProxy()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGInstanceDataProvider::GetRenderProxy);

	return new FPCGInstanceDataProviderProxy(Primitives, NumInstancesAllPrimitives, NumCustomFloatsPerInstance, MakeWeakObjectPtr(this));
}

void UPCGInstanceDataProvider::Reset()
{
	Super::Reset();

	Primitives.Empty(Primitives.Num());
	NumInstancesAllPrimitives = 0;
	NumCustomFloatsPerInstance = 0;
	bWroteInstances = false;
}

FPCGInstanceDataProviderProxy::FPCGInstanceDataProviderProxy(TArray<FPCGPrimitiveComponentAndNumInstances> InPrimitives, uint32 InNumInstancesAllPrimitives, uint32 InNumCustomFloatsPerInstance, TWeakObjectPtr<UPCGInstanceDataProvider> InDataProvider)
	: Primitives(MoveTemp(InPrimitives))
	, NumInstancesAllPrimitives(InNumInstancesAllPrimitives)
	, NumCustomFloatsPerInstance(InNumCustomFloatsPerInstance)
	, DataProvider(InDataProvider)
{
	bIsValid = true;

	if (ensure(InDataProvider.IsValid()))
	{
		DataProviderGeneration = InDataProvider->GenerationCounter;

		if (NumInstancesAllPrimitives == 0)
		{
			InDataProvider->bWroteInstances = true;
		}
	}
	else
	{
		UE_LOG(LogPCG, Warning, TEXT("FPCGInstanceDataProviderProxy: Data provider missing, proxy is invalid, compute graph will not execute."));
		bIsValid = false;
	}

	if (Algo::AnyOf(Primitives, [](const FPCGPrimitiveComponentAndNumInstances& InPrimitive) { return !InPrimitive.Get<0>().IsValid(); }))
	{
		UE_LOG(LogPCG, Warning, TEXT("FPCGInstanceDataProviderProxy: One or more components were invalid, proxy is invalid, compute graph will not execute."));
		bIsValid = false;
	}
}

bool FPCGInstanceDataProviderProxy::IsValid(FValidationData const& InValidationData) const
{
	return bIsValid && InValidationData.ParameterStructSize == sizeof(FParameters);
}

void FPCGInstanceDataProviderProxy::AllocateResources(FRDGBuilder& GraphBuilder, FAllocationData const& InAllocationData)
{
	const uint32 StrideUint4s = 3;
	InstanceData = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(FUintVector4), FMath::Max(1u, NumInstancesAllPrimitives) * StrideUint4s), TEXT("PCGInstanceDataBuffer"));
	InstanceDataSRV = GraphBuilder.CreateSRV(InstanceData);
	InstanceDataUAV = GraphBuilder.CreateUAV(InstanceData);

	const uint32 CustomFloatsRequired = FMath::Max(1u, NumInstancesAllPrimitives * NumCustomFloatsPerInstance);
	InstanceCustomFloatData = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), CustomFloatsRequired), TEXT("PCGInstanceCustomFloatDataBuffer"));
	InstanceCustomFloatDataSRV = GraphBuilder.CreateSRV(InstanceCustomFloatData);
	InstanceCustomFloatDataUAV = GraphBuilder.CreateUAV(InstanceCustomFloatData);

	const int32 NumCountersRequired = FMath::Max(1, Primitives.Num());
	WriteCounters = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), NumCountersRequired), TEXT("PCGWriteCounters"));
	WriteCountersSRV = GraphBuilder.CreateSRV(WriteCounters);
	WriteCountersUAV = GraphBuilder.CreateUAV(WriteCounters);
	TArray<uint32> Zeros;
	Zeros.SetNumZeroed(NumCountersRequired);
	GraphBuilder.QueueBufferUpload(WriteCounters, Zeros.GetData(), Zeros.Num() * Zeros.GetTypeSize());
}

void FPCGInstanceDataProviderProxy::GatherDispatchData(FDispatchData const& InDispatchData)
{
	const TStridedView<FParameters> ParameterArray = MakeStridedParameterView<FParameters>(InDispatchData);
	for (int32 InvocationIndex = 0; InvocationIndex < ParameterArray.Num(); ++InvocationIndex)
	{
		FParameters& Parameters = ParameterArray[InvocationIndex];
		Parameters.NumPrimitives = Primitives.Num();
		Parameters.NumInstancesAllPrimitives = NumInstancesAllPrimitives;
		Parameters.NumCustomFloatsPerInstance = NumCustomFloatsPerInstance;
		Parameters.InstanceData = InstanceDataUAV;
		Parameters.InstanceCustomFloatData = InstanceCustomFloatDataUAV;
		Parameters.WriteCounters = WriteCountersUAV;

		uint32 CumulativeInstanceCount = 0;
		for (int32 Index = 0; Index < Primitives.Num(); ++Index)
		{
			const uint32 PrimitiveInstanceCount = (Primitives[Index].Get<1>() > 0) ? static_cast<uint32>(Primitives[Index].Get<1>()) : 0u;

			Parameters.PrimitiveParameters[Index] = FUintVector4(
				PrimitiveInstanceCount,
				CumulativeInstanceCount,
				0, 0); // Unused

			CumulativeInstanceCount += PrimitiveInstanceCount;
		}
	}
}

void FPCGInstanceDataProviderProxy::PostSubmit(FRDGBuilder& InGraphBuilder) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGInstanceDataProviderProxy::PostSubmit);

	if (NumInstancesAllPrimitives == 0)
	{
		return;
	}

	const TRefCountPtr<FRDGPooledBuffer> InstanceDataExported = InGraphBuilder.ConvertToExternalBuffer(InstanceData);
	const TRefCountPtr<FRDGPooledBuffer> InstanceCustomFloatDataExported = InGraphBuilder.ConvertToExternalBuffer(InstanceCustomFloatData);
	const TRefCountPtr<FRDGPooledBuffer> WriteCountersExported = InGraphBuilder.ConvertToExternalBuffer(WriteCounters);
	
	FTSTicker::GetCoreTicker().AddTicker(TEXT("ApplyPrimitiveSceneUpdates"), 0.0f, [DataProviderWeak=DataProvider, DataProviderGeneration=DataProviderGeneration, InstanceDataExported, InstanceCustomFloatDataExported, WriteCountersExported](float)
	{
		TStrongObjectPtr<UPCGInstanceDataProvider> DataProviderInner = DataProviderWeak.Pin();
		if (!DataProviderInner || DataProviderInner->GenerationCounter != DataProviderGeneration)
		{
			UE_LOG(LogPCG, Verbose, TEXT("Data provider object lost, GPU instancing will fail."));
			return false;
		}

		bool bAllProxiesAreReady = true;

		for (const FPCGPrimitiveComponentAndNumInstances& PrimitiveAndInstanceCount : DataProviderInner->Primitives)
		{
			if (UPrimitiveComponent* PrimitiveComponent = PrimitiveAndInstanceCount.Get<0>().Get())
			{
				bAllProxiesAreReady &= PrimitiveComponent->GetSceneProxy() != nullptr;
			}
		}

		if (!bAllProxiesAreReady)
		{
			UE_LOG(LogPCG, Verbose, TEXT("FPCGInstanceDataProvider: One or more scene proxies were not ready. Will try again on the next tick."));
			return true;
		}

		// All instance data is stored in a single buffer, so this is used to give the scene writer an index to the first primitive.
		int32 CumulativeInstanceCount = 0;

		for (int PrimitiveIndex = 0; PrimitiveIndex < DataProviderInner->Primitives.Num(); ++PrimitiveIndex)
		{
			FPCGPrimitiveComponentAndNumInstances& PrimitiveAndInstanceCount = DataProviderInner->Primitives[PrimitiveIndex];

			UPrimitiveComponent* PrimitiveComponent = PrimitiveAndInstanceCount.Get<0>().Get();
			if (!PrimitiveComponent)
			{
				UE_LOG(LogPCG, Warning, TEXT("Null primitive component encountered during instancing, should not happen."));
				continue;
			}

			const int32 PrimitiveNumInstances = PrimitiveAndInstanceCount.Get<1>();
			if (PrimitiveNumInstances <= 0)
			{
				UE_LOG(LogPCG, Warning, TEXT("Primitive with 0 instances encountered during instancing, should not happen."));
				continue;
			}

			FPrimitiveSceneProxyDesc* SceneProxyDesc = nullptr;

			// TODO: Slight mess, could add a GetSceneProxyDesc function to the base descriptor class. However we plan
			// to move away from components and this code is temporary.
			FInstancedStaticMeshSceneProxyDesc PISMCSceneProxyDesc;
			FInstancedSkinnedMeshSceneProxyDesc ISKMSceneProxyDesc;

			if (UPCGProceduralISMComponent* PISM = Cast<UPCGProceduralISMComponent>(PrimitiveComponent))
			{
				PISM->GetSceneProxyDesc(PISMCSceneProxyDesc);
				SceneProxyDesc = &PISMCSceneProxyDesc;
			}
			else if (UInstancedSkinnedMeshComponent* ISKM = Cast<UInstancedSkinnedMeshComponent>(PrimitiveComponent))
			{
				ISKMSceneProxyDesc = FInstancedSkinnedMeshSceneProxyDesc(ISKM);
				SceneProxyDesc = &ISKMSceneProxyDesc;
			}

			if (!SceneProxyDesc)
			{
				UE_LOG(LogPCG, Warning, TEXT("Primitive '%s' of unsupported type, instancing aborted."), *PrimitiveComponent->GetName());
				continue;
			}

			FPrimitiveSceneDesc PrimitiveSceneDesc;

			// TODO try add FPrimitiveSceneDesc::InitFromPrimitiveComponent
			if (UPCGProceduralISMComponent* PISM = Cast<UPCGProceduralISMComponent>(PrimitiveComponent))
			{
				PISM->BuildSceneDesc(SceneProxyDesc, PrimitiveSceneDesc);
			}
			else if (UInstancedSkinnedMeshComponent* ISKM = Cast<UInstancedSkinnedMeshComponent>(PrimitiveComponent))
			{
				ISKM->BuildSceneDesc(SceneProxyDesc, PrimitiveSceneDesc);
			}

			if (FSceneInterface* Scene = PrimitiveComponent->GetWorld() ? PrimitiveComponent->GetWorld()->Scene : nullptr)
			{
#if UE_BUILD_SHIPPING
				const bool bTriggerCapture = false;
#else
				const bool bTriggerCapture = (PCGInstanceDataInterface::TriggerGPUCaptureDispatchIndex > 0);
				PCGInstanceDataInterface::TriggerGPUCaptureDispatchIndex = FMath::Max(PCGInstanceDataInterface::TriggerGPUCaptureDispatchIndex - 1, 0);
#endif // UE_BUILD_SHIPPING

				Scene->UpdatePrimitiveInstancesFromCompute(&PrimitiveSceneDesc, FGPUSceneWriteDelegate::CreateLambda([PrimitiveIndex, PrimitiveNumInstances, CumulativeInstanceCount, bTriggerCapture, InstanceDataExported, InstanceCustomFloatDataExported, WriteCountersExported](FRDGBuilder& InGraphBuilder, const FGPUSceneWriteDelegateParams& Params)
				{
					RDG_EVENT_SCOPE(InGraphBuilder, "SceneComputeUpdateInterface->EnqueueUpdate");
					check(Params.PersistentPrimitiveId != (uint32)INDEX_NONE);

#if !UE_BUILD_SHIPPING
					RenderCaptureInterface::FScopedCapture RenderCapture(bTriggerCapture, InGraphBuilder, TEXT("SceneComputeUpdateInterface->EnqueueUpdate"));
#endif // !UE_BUILD_SHIPPING

					FRDGBufferRef InstanceDataInner  = InGraphBuilder.RegisterExternalBuffer(InstanceDataExported);
					FRDGBufferRef InstanceCustomFloatDataInner = InGraphBuilder.RegisterExternalBuffer(InstanceCustomFloatDataExported);
					FRDGBufferRef WriteCountersInner = InGraphBuilder.RegisterExternalBuffer(WriteCountersExported);

					// Write instances
					FPCGSceneWriterCS::FParameters* Parameters = InGraphBuilder.AllocParameters<FPCGSceneWriterCS::FParameters>();
					Parameters->InPrimitiveIndex = PrimitiveIndex;
					Parameters->InNumInstancesAllocatedInGPUScene = PrimitiveNumInstances;
					Parameters->InInstanceOffset = CumulativeInstanceCount;
					Parameters->InInstanceData = InGraphBuilder.CreateSRV(InstanceDataInner);
					Parameters->InInstanceCustomFloatData = InGraphBuilder.CreateSRV(InstanceCustomFloatDataInner);
					Parameters->InWriteCounters = InGraphBuilder.CreateSRV(WriteCountersInner);
					Parameters->InPrimitiveId = Params.PersistentPrimitiveId;
					PRAGMA_DISABLE_DEPRECATION_WARNINGS
					Parameters->GPUSceneWriterParameters = Params.GPUWriteParams;
					PRAGMA_ENABLE_DEPRECATION_WARNINGS
					Parameters->InCustomDataCount = Params.NumCustomDataFloats;
					Parameters->InPayloadDataFlags = Params.PackedInstanceSceneDataFlags;

#if PCG_INSTANCE_DATA_LOGGING
					UE_LOG(LogPCG, Log, TEXT("\tScene writer delegate [%d]:\tPrimitive ID %u,\tsource instance offset %u,\tInstanceSceneDataOffset %u, num instances %u"),
						Parameters->InPrimitiveIndex,
						Parameters->InPrimitiveId,
						Parameters->InInstanceOffset,
						Params.InstanceSceneDataOffset,
						Parameters->InNumInstancesAllocatedInGPUScene);
#endif

					// We're using a custom compute shader here rather than using a compute graph kernel, because ultimately the scene update will incorporate the
					// new instance data. In the future we will not write directly to the scene here.
					TShaderMapRef<FPCGSceneWriterCS> Shader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
					int GroupCount = FMath::DivideAndRoundUp<int>(PrimitiveNumInstances, FPCGSceneWriterCS::NumThreadsPerGroup);
					ensureAlways(GroupCount <= GRHIMaxDispatchThreadGroupsPerDimension.X);
					GroupCount = FMath::Min(GRHIMaxDispatchThreadGroupsPerDimension.X, GroupCount);

					FComputeShaderUtils::AddPass(InGraphBuilder, RDG_EVENT_NAME("PCGWriteInstanceData"), ERDGPassFlags::Compute | ERDGPassFlags::NeverCull, Shader, Parameters, FIntVector(GroupCount, 1, 1));
				}));
			}

			CumulativeInstanceCount += PrimitiveNumInstances;
		}

		DataProviderInner->bWroteInstances = true;

		return false;
	});
}
