// Copyright Epic Games, Inc. All Rights Reserved.

#include "Compute/DataInterfaces/PCGDataCollectionDataInterface.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGModule.h"
#include "PCGSettings.h"
#include "PCGSubsystem.h"
#include "Compute/PCGComputeCommon.h"
#include "Compute/PCGDataBinding.h"
#include "Compute/Data/PCGProxyForGPUData.h"
#include "Compute/Elements/PCGComputeGraphElement.h"
#include "Helpers/PCGHelpers.h"

#include "RenderGraphBuilder.h"
#include "RenderGraphResources.h"
#include "ShaderCompilerCore.h"
#include "ShaderParameterMetadataBuilder.h"
#include "SystemTextures.h"
#include "ComputeFramework/ComputeKernelPermutationSet.h"
#include "ComputeFramework/ShaderParamTypeDefinition.h"

#define LOCTEXT_NAMESPACE "PCGDataCollectionDataInterface"

void UPCGDataCollectionDataInterface::GetSupportedInputs(TArray<FShaderFunctionDefinition>& OutFunctions) const
{
	// Internal direct access to buffer, used to optimize access.
	{
		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("LoadBufferInternal"))
			.AddReturnType(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("StoreBufferInternal"))
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint);
	}

	// Header Readers
	{
		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("GetNumData"))
			.AddReturnType(EShaderFundamentalType::Uint);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("GetDataNumAttributesInternal"))
			.AddReturnType(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint); // InDataIndex

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("GetNumElements"))
			.AddReturnType(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint); // InDataIndex

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("AddToElementCounterInternal"))
			.AddReturnType(EShaderFundamentalType::Int) // Value before increment. Returns -1 if counters disabled.
			.AddParam(EShaderFundamentalType::Uint) // InDataIndex
			.AddParam(EShaderFundamentalType::Uint); // InElementCount

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("GetThreadData"))
			.AddParam(EShaderFundamentalType::Uint) // InThreadIndex
			.AddParam(EShaderFundamentalType::Uint, 0, 0, EShaderParamModifier::Out) // OutDataIndex
			.AddParam(EShaderFundamentalType::Uint, 0, 0, EShaderParamModifier::Out) // OutElementIndex
			.AddReturnType(EShaderFundamentalType::Bool);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("GetNumElements"))
			.AddReturnType(EShaderFundamentalType::Uint);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("GetElementAddressInternal"))
			.AddReturnType(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint) // InDataIndex
			.AddParam(EShaderFundamentalType::Uint) // InElementIndex
			.AddParam(EShaderFundamentalType::Uint); // InAttributeId

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("GetAttributeStrideInternal"))
			.AddReturnType(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint) // InDataIndex
			.AddParam(EShaderFundamentalType::Uint); // InAttributeId
	}

	// Header Writers
	{
		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("SetAsExecutedInternal"));
	}

	// Attribute Getters
	{
		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("GetBool"))
			.AddReturnType(EShaderFundamentalType::Bool)
			.AddParam(EShaderFundamentalType::Uint) // DataIndex
			.AddParam(EShaderFundamentalType::Uint) // ElementIndex
			.AddParam(EShaderFundamentalType::Uint); // AttributeId

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("GetInt"))
			.AddReturnType(EShaderFundamentalType::Int)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("GetFloat"))
			.AddReturnType(EShaderFundamentalType::Float)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("GetFloat2"))
			.AddReturnType(EShaderFundamentalType::Float, 2)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("GetFloat3"))
			.AddReturnType(EShaderFundamentalType::Float, 3)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("GetFloat4"))
			.AddReturnType(EShaderFundamentalType::Float, 4)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("GetRotator"))
			.AddReturnType(EShaderFundamentalType::Float, 3)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("GetQuat"))
			.AddReturnType(EShaderFundamentalType::Float, 4)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("GetTransform"))
			.AddReturnType(EShaderFundamentalType::Float, 4, 4)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("GetStringKey"))
			.AddReturnType(EShaderFundamentalType::Int) // String key represented by int
			.AddParam(EShaderFundamentalType::Uint) // DataIndex
			.AddParam(EShaderFundamentalType::Uint) // ElementIndex
			.AddParam(EShaderFundamentalType::Uint); // AttributeId

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("GetName"))
			.AddReturnType(EShaderFundamentalType::Uint, 2) // Name represented by 2 uints
			.AddParam(EShaderFundamentalType::Uint) // DataIndex
			.AddParam(EShaderFundamentalType::Uint) // ElementIndex
			.AddParam(EShaderFundamentalType::Uint); // AttributeId
	}

	// Attribute Setters
	{
		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("SetBool"))
			.AddParam(EShaderFundamentalType::Uint) // DataIndex
			.AddParam(EShaderFundamentalType::Uint) // ElementIndex
			.AddParam(EShaderFundamentalType::Uint) // AttributeId
			.AddParam(EShaderFundamentalType::Bool); // Value

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("SetInt"))
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Int);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("SetFloat"))
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Float);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("SetFloat2"))
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Float, 2);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("SetFloat3"))
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Float, 3);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("SetFloat4"))
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Float, 4);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("SetRotator"))
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Float, 3);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("SetQuat"))
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Float, 4);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("SetTransform"))
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Float, 4, 4);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("SetStringKey"))
			.AddParam(EShaderFundamentalType::Uint) // DataIndex
			.AddParam(EShaderFundamentalType::Uint) // ElementIndex
			.AddParam(EShaderFundamentalType::Uint) // AttributeId
			.AddParam(EShaderFundamentalType::Int); // String key represented by int
		
		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("SetName"))
			.AddParam(EShaderFundamentalType::Uint) // DataIndex
			.AddParam(EShaderFundamentalType::Uint) // ElementIndex
			.AddParam(EShaderFundamentalType::Uint) // AttributeId
			.AddParam(EShaderFundamentalType::Uint, 2); // Name represented by 2 uints
	}

	// Atomics
	{
		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("AtomicAddInt"))
			.AddReturnType(EShaderFundamentalType::Int) // Value before it was incremented
			.AddParam(EShaderFundamentalType::Uint) // DataIndex
			.AddParam(EShaderFundamentalType::Uint) // ElementIndex
			.AddParam(EShaderFundamentalType::Int) // AttributeId
			.AddParam(EShaderFundamentalType::Int); // ValueToAdd
	}

	// Point Attribute Getters
	{
		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("GetPosition"))
			.AddReturnType(EShaderFundamentalType::Float, 3)
			.AddParam(EShaderFundamentalType::Uint) // DataIndex
			.AddParam(EShaderFundamentalType::Uint); // ElementIndex

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("GetRotation"))
			.AddReturnType(EShaderFundamentalType::Float, 4)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("GetScale"))
			.AddReturnType(EShaderFundamentalType::Float, 3)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("GetBoundsMin"))
			.AddReturnType(EShaderFundamentalType::Float, 3)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("GetBoundsMax"))
			.AddReturnType(EShaderFundamentalType::Float, 3)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("GetColor"))
			.AddReturnType(EShaderFundamentalType::Float, 4)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("GetDensity"))
			.AddReturnType(EShaderFundamentalType::Float)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("GetSeed"))
			.AddReturnType(EShaderFundamentalType::Int)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("GetSteepness"))
			.AddReturnType(EShaderFundamentalType::Float)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("IsPointRemoved"))
			.AddReturnType(EShaderFundamentalType::Bool)
			.AddParam(EShaderFundamentalType::Uint) // DataIndex
			.AddParam(EShaderFundamentalType::Uint); // ElementIndex

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("GetPointTransform"))
			.AddReturnType(EShaderFundamentalType::Float, 4, 4)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint);
	}

	// Point Attribute Setters
	{
		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("SetPosition"))
			.AddParam(EShaderFundamentalType::Uint) // DataIndex
			.AddParam(EShaderFundamentalType::Uint) // ElementIndex
			.AddParam(EShaderFundamentalType::Float, 3); // Value

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("SetRotation"))
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Float, 4);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("SetScale"))
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Float, 3);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("SetBoundsMin"))
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Float, 3);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("SetBoundsMax"))
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Float, 3);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("SetColor"))
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Float, 4);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("SetDensity"))
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Float);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("SetSeed"))
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Int);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("SetSteepness"))
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Float);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("SetPointTransform"))
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Float, 4, 4);
	}

	// Misc
	{
		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("RemovePoint"))
			.AddParam(EShaderFundamentalType::Uint) // DataIndex
			.AddParam(EShaderFundamentalType::Uint); // ElementIndex

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("InitializePoint"))
			.AddParam(EShaderFundamentalType::Uint)
			.AddParam(EShaderFundamentalType::Uint);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("GetElementCountMultiplier"))
			.AddReturnType(EShaderFundamentalType::Uint);
	}
}

BEGIN_SHADER_PARAMETER_STRUCT(FPCGDataCollectionDataInterfaceParameters, )
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWByteAddressBuffer, DataCollectionBuffer)
	SHADER_PARAMETER(uint32, ElementCountMultiplier)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<int32>, BufferToGraphStringKey)
	SHADER_PARAMETER(int32, NumRemappedStringKeys)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint32>, ElementCountersBuffer)
	SHADER_PARAMETER(uint32, ElementCountersPresent)
END_SHADER_PARAMETER_STRUCT()

void UPCGDataCollectionDataInterface::GetShaderParameters(TCHAR const* UID, FShaderParametersMetadataBuilder& InOutBuilder, FShaderParametersMetadataAllocations& InOutAllocations) const
{
	InOutBuilder.AddNestedStruct<FPCGDataCollectionDataInterfaceParameters>(UID);
}

TCHAR const* UPCGDataCollectionDataInterface::TemplateFilePath = TEXT("/Plugin/PCG/Private/PCGDataCollectionDataInterface.ush");

TCHAR const* UPCGDataCollectionDataInterface::GetShaderVirtualPath() const
{
	return TemplateFilePath;
}

void UPCGDataCollectionDataInterface::GetShaderHash(FString& InOutKey) const
{
	GetShaderFileHash(TemplateFilePath, EShaderPlatform::SP_PCD3D_SM5).AppendString(InOutKey);
}

void UPCGDataCollectionDataInterface::GetHLSL(FString& OutHLSL, FString const& InDataInterfaceName) const
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

void UPCGDataCollectionDataInterface::GetDefines(FComputeKernelDefinitionSet& OutDefinitionSet) const
{
	Super::GetDefines(OutDefinitionSet);

	// Point property IDs
	OutDefinitionSet.Defines.Add(FComputeKernelDefinition(TEXT("PCG_POINT_POSITION_ID"),   FString::FromInt(PCGComputeConstants::POINT_POSITION_ATTRIBUTE_ID)));
	OutDefinitionSet.Defines.Add(FComputeKernelDefinition(TEXT("PCG_POINT_ROTATION_ID"),   FString::FromInt(PCGComputeConstants::POINT_ROTATION_ATTRIBUTE_ID)));
	OutDefinitionSet.Defines.Add(FComputeKernelDefinition(TEXT("PCG_POINT_SCALE_ID"),      FString::FromInt(PCGComputeConstants::POINT_SCALE_ATTRIBUTE_ID)));
	OutDefinitionSet.Defines.Add(FComputeKernelDefinition(TEXT("PCG_POINT_BOUNDS_MIN_ID"), FString::FromInt(PCGComputeConstants::POINT_BOUNDS_MIN_ATTRIBUTE_ID)));
	OutDefinitionSet.Defines.Add(FComputeKernelDefinition(TEXT("PCG_POINT_BOUNDS_MAX_ID"), FString::FromInt(PCGComputeConstants::POINT_BOUNDS_MAX_ATTRIBUTE_ID)));
	OutDefinitionSet.Defines.Add(FComputeKernelDefinition(TEXT("PCG_POINT_COLOR_ID"),      FString::FromInt(PCGComputeConstants::POINT_COLOR_ATTRIBUTE_ID)));
	OutDefinitionSet.Defines.Add(FComputeKernelDefinition(TEXT("PCG_POINT_DENSITY_ID"),    FString::FromInt(PCGComputeConstants::POINT_DENSITY_ATTRIBUTE_ID)));
	OutDefinitionSet.Defines.Add(FComputeKernelDefinition(TEXT("PCG_POINT_SEED_ID"),       FString::FromInt(PCGComputeConstants::POINT_SEED_ATTRIBUTE_ID)));
	OutDefinitionSet.Defines.Add(FComputeKernelDefinition(TEXT("PCG_POINT_STEEPNESS_ID"),  FString::FromInt(PCGComputeConstants::POINT_STEEPNESS_ATTRIBUTE_ID)));

	// Header sizes
	OutDefinitionSet.Defines.Add(FComputeKernelDefinition(TEXT("PCG_DATA_COLLECTION_HEADER_SIZE_BYTES"), FString::FromInt(PCGComputeConstants::DATA_COLLECTION_HEADER_SIZE_BYTES)));
	OutDefinitionSet.Defines.Add(FComputeKernelDefinition(TEXT("PCG_DATA_HEADER_SIZE_BYTES"),            FString::FromInt(PCGComputeConstants::DATA_HEADER_SIZE_BYTES)));
	OutDefinitionSet.Defines.Add(FComputeKernelDefinition(TEXT("PCG_ATTRIBUTE_HEADER_SIZE_BYTES"),       FString::FromInt(PCGComputeConstants::ATTRIBUTE_HEADER_SIZE_BYTES)));

	// Misc
	OutDefinitionSet.Defines.Add(FComputeKernelDefinition(TEXT("PCG_KERNEL_EXECUTED_FLAG"), FString::FromInt(PCGComputeConstants::KernelExecutedFlag)));
	OutDefinitionSet.Defines.Add(FComputeKernelDefinition(TEXT("PCG_INVALID_DENSITY"),      FString::Printf(TEXT("%f"), PCGComputeConstants::INVALID_DENSITY)));
}

UComputeDataProvider* UPCGDataCollectionDataInterface::CreateDataProvider() const
{
	return NewObject<UPCGDataCollectionDataProvider>();
}

void UPCGDataCollectionDataProvider::Initialize(const UComputeDataInterface* InDataInterface, UObject* InBinding, uint64 InInputMask, uint64 InOutputMask)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGDataCollectionDataProvider::Initialize);

	Super::Initialize(InDataInterface, InBinding, InInputMask, InOutputMask);

	const UPCGDataCollectionDataInterface* DataInterface = CastChecked<UPCGDataCollectionDataInterface>(InDataInterface);

	bRequiresZeroInitialization = DataInterface->bRequiresZeroInitialization;

	ElementCountMultiplier = DataInterface->ElementCountMultiplier;

	bExposeElementCounters = DataInterface->bExposeElementCounters;
}

bool UPCGDataCollectionDataProvider::PrepareForExecute_GameThread(UPCGDataBinding* InBinding)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGDataCollectionDataProvider::PrepareForExecute_GameThread);
	check(InBinding);

	if (!Super::PrepareForExecute_GameThread(InBinding))
	{
		return false;
	}

	check(PinDesc);

	if (PCGComputeHelpers::IsBufferSizeTooLarge(PinDesc->ComputePackedSizeBytes()))
	{
		PinDesc = nullptr;
	}

	return true;
}

FComputeDataProviderRenderProxy* UPCGDataCollectionDataProvider::GetRenderProxy()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGDataCollectionDataProvider::GetRenderProxy);

	FPCGDataCollectionDataProviderProxy* Proxy = new FPCGDataCollectionDataProviderProxy(this, PinDesc, ExportMode, bRequiresZeroInitialization, ElementCountMultiplier, bExposeElementCounters, OutputPinLabel, OutputPinLabelAlias);

	if (bExposeElementCounters)
	{
		const uint64 GenerationCount = GenerationCounter;

		Proxy->AsyncReadbackCallback_RenderThread = [ThisWeak=TWeakObjectPtr<UPCGDataCollectionDataProvider>(this), /*InitStack=InitStack, ProviderName,*/ GenerationCount](const void* InData, int InNumBytes)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(UPCGDataCollectionDataProvider::ProcessReadbackData_RenderThread);
			TStrongObjectPtr<UPCGDataCollectionDataProvider> This = ThisWeak.Pin();
			if (!This)
			{
				return;
			}

			if (This->GenerationCounter != GenerationCount)
			{
				return;
			}

			// We should never find ourselves stomping existing data.
			check(This->ReadbackCounters.IsEmpty());

			if (ensure(InData && InNumBytes > 0))
			{
				// Copy the data to temp storage for the game thread to pick up.
				This->ReadbackCounters.SetNumUninitialized(InNumBytes / This->ReadbackCounters.GetTypeSize());
				FMemory::Memcpy(This->ReadbackCounters.GetData(), InData, InNumBytes);
			}

			This->bReadbackComplete = true;

			ExecuteOnGameThread(UE_SOURCE_LOCATION, [ThisWeak=ThisWeak, OutputPinLabelAlias=This->OutputPinLabelAlias, GenerationCount]()
			{
				TStrongObjectPtr<UPCGDataCollectionDataProvider> This = ThisWeak.Pin();
				if (This && This->GenerationCounter == GenerationCount)
				{
					if (This->Binding.IsValid())
					{
						This->Binding->ReceiveElementCountsFromGPU_GameThread(OutputPinLabelAlias, This->ReadbackCounters);
					}

					This->OnCountersReadBack.Broadcast();
				}
			});
		};
	}

	return Proxy;
}

void UPCGDataCollectionDataProvider::Reset()
{
	Super::Reset();

	OnCountersReadBack = {};
	ElementCountMultiplier = 0;
	bExposeElementCounters = false;
	bReadbackComplete = false;
	ReadbackCounters.Empty();
	bRequiresZeroInitialization = false;
}

FPCGDataCollectionDataProviderProxy::FPCGDataCollectionDataProviderProxy(
	TWeakObjectPtr<UPCGDataCollectionDataProvider> InDataProvider,
	const FPCGDataCollectionDesc* InPinDesc,
	EPCGExportMode InExportMode,
	bool bInZeroInitialize,
	uint32 InElementCountMultiplier,
	bool bInExposeElementCounters,
	FName InOutputPinLabel,
	FName InOutputPinLabelAlias)
	: ExportMode(InExportMode)
	, bZeroInitialize(bInZeroInitialize)
	, ElementCountMultiplier(InElementCountMultiplier)
	, bExposeElementCounters(bInExposeElementCounters)
	, DataProviderWeakPtr(InDataProvider)
	, OutputPinLabel(InOutputPinLabel)
	, OutputPinLabelAlias(InOutputPinLabelAlias)
{
	OriginatingGenerationCount = InDataProvider->GenerationCounter;

	if (InPinDesc)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(CopyDataDescription);

		// @todo_pcg: Avoid costly copy of data description.
		PinDesc = *InPinDesc;
	}

	SizeBytes = PinDesc.ComputePackedSizeBytes();
}

bool FPCGDataCollectionDataProviderProxy::IsValid(FValidationData const& InValidationData) const
{
	if (InValidationData.ParameterStructSize != sizeof(FParameters))
	{
		return false;
	}

	if (SizeBytes <= 0)
	{
		UE_LOG(LogPCG, Error, TEXT("Proxy invalid due to invalid buffer size."));
		return false;
	}

	return true;
}

void FPCGDataCollectionDataProviderProxy::GatherDispatchData(FDispatchData const& InDispatchData)
{
	check(BufferUAV);
	check(CountersUAV);

	const TStridedView<FParameters> ParameterArray = MakeStridedParameterView<FParameters>(InDispatchData);
	for (int32 InvocationIndex = 0; InvocationIndex < ParameterArray.Num(); ++InvocationIndex)
	{
		FParameters& Parameters = ParameterArray[InvocationIndex];
		Parameters.DataCollectionBuffer = BufferUAV;
		Parameters.ElementCountMultiplier = ElementCountMultiplier;
		Parameters.BufferToGraphStringKey = BufferToGraphStringKeySRV;
		Parameters.NumRemappedStringKeys = NumRemappedStringKeys;
		Parameters.ElementCountersBuffer = CountersUAV;
		Parameters.ElementCountersPresent = bExposeElementCounters ? 1 : 0;
	}
}

void FPCGDataCollectionDataProviderProxy::AllocateResources(FRDGBuilder& GraphBuilder, FAllocationData const& InAllocationData)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGDataCollectionDataProviderProxy::AllocateResources);
	check(SizeBytes > 0);

	{
		// Ensure we always allocate at least 1 element so we have something to bind.
		const uint32 NumCounters = bExposeElementCounters ? FMath::Max(PinDesc.DataDescs.Num(), 1) : 1;

		FRDGBufferDesc CountersDesc = FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), NumCounters);
		CountersDesc.Usage |= BUF_SourceCopy;

		Counters = GraphBuilder.CreateBuffer(CountersDesc, TEXT("PCGDataCollection_Counters"));
		CountersUAV = GraphBuilder.CreateUAV(Counters);

		// A single counter, or a small number of counters, will be a very common case.
		if (NumCounters <= 4)
		{
			GraphBuilder.QueueBufferUpload(Counters, &FUintVector4::ZeroValue, NumCounters * sizeof(uint32));
		}
		else
		{
			TArray<uint32> Zeroes;
			Zeroes.SetNumZeroed(NumCounters);

			GraphBuilder.QueueBufferUpload(Counters, Zeroes.GetData(), Zeroes.Num() * Zeroes.GetTypeSize());
		}
	}

	FRDGBufferDesc Desc = FRDGBufferDesc::CreateByteAddressDesc(SizeBytes);
	if (ExportMode != EPCGExportMode::NoExport)
	{
		// We don't know for sure whether buffer will be read back or not, so need to flag the possibility if the buffer will be passed downstream.
		Desc.Usage |= BUF_SourceCopy;
	}

	Buffer = GraphBuilder.CreateBuffer(Desc, TEXT("PCGDataCollection"));
	BufferUAV = GraphBuilder.CreateUAV(Buffer);

	// Initialize with an empty data collection. The kernel may not run, for example if indirect dispatch args end up being 0. This ensures
	// there is something meaningful to readback.
	TArray<uint32> PackedDataCollection;
	PackedDataCollection.Reserve(bZeroInitialize ? (SizeBytes / PackedDataCollection.GetTypeSize()) : PinDesc.ComputePackedHeaderSizeBytes());

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGDataCollectionDataProviderProxy::AllocateResources::WriteHeader);
		PinDesc.WriteHeader(PackedDataCollection);
	}

	if (bZeroInitialize)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGDataCollectionDataProviderProxy::AllocateResources::ZeroInitialize);

		// Fill remaining part of full buffer with 0s.
		PackedDataCollection.SetNumZeroed(SizeBytes / PackedDataCollection.GetTypeSize());
	}

	GraphBuilder.QueueBufferUpload(Buffer, PackedDataCollection.GetData(), PackedDataCollection.Num() * PackedDataCollection.GetTypeSize(), ERDGInitialDataFlags::None);

	BufferToGraphStringKeySRV = GetBufferToGraphStringKeySRV(GraphBuilder, NumRemappedStringKeys);

	// If buffer needs to be exported, can do it now. Pass back a reference to the main thread where it can be picked up by the compute graph element.
	if (ExportMode != EPCGExportMode::NoExport)
	{
		const TRefCountPtr<FRDGPooledBuffer> ExportedPooledBuffer = GraphBuilder.ConvertToExternalBuffer(Buffer);
		GraphBuilder.SetBufferAccessFinal(Buffer, ERHIAccess::CopySrc);

		ExecuteOnGameThread(UE_SOURCE_LOCATION, [ExportedPooledBuffer, DataProviderWeakPtr=DataProviderWeakPtr, PinDesc=PinDesc, OutputPinLabel=OutputPinLabel, SizeBytes=SizeBytes, ExportMode=ExportMode, OutputPinLabelAlias=OutputPinLabelAlias, GenerationCount=OriginatingGenerationCount]()
		{
			// Obtain objects. No ensures added because a graph cancellation could feasibly destroy some or all of these.
			UPCGDataCollectionDataProvider* DataProvider = DataProviderWeakPtr.Get();
			if (!DataProvider)
			{
				UE_LOG(LogPCG, Error, TEXT("Could not resolve UPCGDataCollectionDataProvider object to pass back buffer handle."));
				return;
			}

			if (DataProvider->GenerationCounter != GenerationCount)
			{
				return;
			}

			UPCGDataBinding* Binding = DataProvider ? DataProvider->GetDataBinding() : nullptr;
			UPCGSubsystem* Subsystem = UPCGSubsystem::GetSubsystemForCurrentWorld();

			if (Binding && Subsystem)
			{
				TSharedPtr<FPCGProxyForGPUDataCollection> DataCollectionOnGPU = MakeShared<FPCGProxyForGPUDataCollection>(ExportedPooledBuffer, SizeBytes, PinDesc, Binding->GetStringTable());

				for (int DataIndex = 0; DataIndex < PinDesc.DataDescs.Num(); ++DataIndex)
				{
					UPCGProxyForGPUData* Proxy = NewObject<UPCGProxyForGPUData>();
					Proxy->Initialize(DataCollectionOnGPU, DataIndex);

					// TODO - binding is doing a lot of work. Could store a context handle in the data provider instead?
					Binding->ReceiveDataFromGPU_GameThread(Proxy, DataProvider->GetProducerSettings(), ExportMode, OutputPinLabel, OutputPinLabelAlias);
				}

				DataProvider->OnDataExported_GameThread().Broadcast();
			}
		});
	}
}

void FPCGDataCollectionDataProviderProxy::GetReadbackData(TArray<FReadbackData>& OutReadbackData) const
{
	if (bExposeElementCounters)
	{
		FReadbackData Data;
		Data.Buffer = Counters;
		Data.NumBytes = PinDesc.DataDescs.Num() * sizeof(uint32);
		Data.ReadbackCallback_RenderThread = &AsyncReadbackCallback_RenderThread;

		OutReadbackData.Add(MoveTemp(Data));
	}
}

FRDGBufferSRVRef FPCGDataCollectionDataProviderProxy::GetBufferToGraphStringKeySRV(FRDGBuilder& GraphBuilder, int32& OutNumRemappedStringKeys) const
{
	// Default implementation. Disable remapping.
	OutNumRemappedStringKeys = 0;

	return GraphBuilder.CreateSRV(FRDGBufferSRVDesc(GSystemTextures.GetDefaultStructuredBuffer(GraphBuilder, sizeof(int32))));
}

#undef LOCTEXT_NAMESPACE
