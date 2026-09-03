// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGStaticMeshSpawnerDataInterface.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGParamData.h"
#include "Compute/PCGComputeCommon.h"
#include "Compute/PCGComputeGraph.h"
#include "Compute/PCGDataBinding.h"
#include "Compute/BuiltInKernels/PCGCountUniqueAttributeValuesKernel.h"
#include "Compute/Data/PCGProxyForGPUData.h"
#include "Elements/PCGStaticMeshSpawner.h"
#include "Elements/PCGStaticMeshSpawnerKernel.h"
#include "InstanceDataPackers/PCGInstanceDataPackerBase.h"
#include "MeshSelectors/PCGMeshSelectorByAttribute.h"
#include "MeshSelectors/PCGMeshSelectorWeighted.h"

#include "GlobalRenderResources.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphResources.h"
#include "RHIResources.h"
#include "ShaderCompilerCore.h"
#include "ShaderCore.h"
#include "ShaderParameterMetadataBuilder.h"
#include "SystemTextures.h"
#include "Engine/StaticMesh.h"
#include "ComputeFramework/ShaderParamTypeDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGStaticMeshSpawnerDataInterface)

#define LOCTEXT_NAMESPACE "PCGStaticMeshSpawnerDataInterface"

namespace PCGStaticMeshSpawnerDataInterface
{
	static const FText CouldNotLoadStaticMeshFormat = LOCTEXT("CouldNotLoadStaticMesh", "Could not load static mesh from path '{0}'.");
	static const FText TooManyPrimitivesFormat = LOCTEXT("TooManyPrimitives", "Attempted to emit too many primitive components, terminated after creating '{0}'.");
	static const FText NoMeshEntriesFormat = LOCTEXT("NoMeshEntries", "No mesh entries provided in weighted mesh selector.");
}

void UPCGStaticMeshSpawnerDataInterface::GetSupportedInputs(TArray<FShaderFunctionDefinition>& OutFunctions) const
{
	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("SMSpawner_GetSelectorAttributeId"))
		.AddReturnType(EShaderFundamentalType::Uint); // Attribute id to get mesh path string key from, or invalid if we should use CDF instead.

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("SMSpawner_GetNumAttributes"))
		.AddReturnType(EShaderFundamentalType::Uint); // Num attributes

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("SMSpawner_GetNumPrimitives"))
		.AddReturnType(EShaderFundamentalType::Uint); // Num primitives

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("SMSpawner_ShouldApplyBounds"))
		.AddReturnType(EShaderFundamentalType::Bool);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("SMSpawner_GetPrimitiveMeshBoundsMin"))
		.AddReturnType(EShaderFundamentalType::Float, 3) // Local bounds min
		.AddParam(EShaderFundamentalType::Uint); // InPrimitiveIndex

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("SMSpawner_GetPrimitiveMeshBoundsMax"))
		.AddReturnType(EShaderFundamentalType::Float, 3) // Local bounds max
		.AddParam(EShaderFundamentalType::Uint); // InPrimitiveIndex

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("SMSpawner_GetAttributeIdOffsetStride"))
		.AddReturnType(EShaderFundamentalType::Uint, 4)
		.AddParam(EShaderFundamentalType::Uint); // InAttributeIndex

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("SMSpawner_GetPrimitiveStringKey"))
		.AddReturnType(EShaderFundamentalType::Int) // String key
		.AddParam(EShaderFundamentalType::Uint); // InPrimitiveIndex

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("SMSpawner_GetPrimitiveIndexFromStringKey"))
		.AddReturnType(EShaderFundamentalType::Uint) // Primitive index
		.AddParam(EShaderFundamentalType::Int); // InMeshPathStringKey

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("SMSpawner_GetPrimitiveSelectionCDF"))
		.AddReturnType(EShaderFundamentalType::Float) // CDF value
		.AddParam(EShaderFundamentalType::Uint); // InPrimitiveIndex

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("SMSpawner_GetSelectedMeshAttributeId"))
		.AddReturnType(EShaderFundamentalType::Uint); // Attribute id to output mesh path string key to
}

BEGIN_SHADER_PARAMETER_STRUCT(FPCGStaticMeshSpawnerDataInterfaceParameters,)
	SHADER_PARAMETER_ARRAY(FUintVector4, AttributeIdOffsetStrides, [UPCGStaticMeshSpawnerDataInterface::MAX_ATTRIBUTES])
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<int32>, PrimitiveStringKeys)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FVector4f>, PrimitiveMeshBoundsMin)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FVector4f>, PrimitiveMeshBoundsMax)
	SHADER_PARAMETER_SCALAR_ARRAY(float, SelectionCDF, [PCGComputeConstants::MAX_PRIMITIVE_COMPONENTS_PER_SPAWNER])
	SHADER_PARAMETER(uint32, NumAttributes)
	SHADER_PARAMETER(uint32, NumPrimitives)
	SHADER_PARAMETER(int32, SelectorAttributeId)
	SHADER_PARAMETER(int32, SelectedMeshAttributeId)
	SHADER_PARAMETER(uint32, ApplyBounds)
END_SHADER_PARAMETER_STRUCT()

void UPCGStaticMeshSpawnerDataInterface::GetShaderParameters(TCHAR const* UID, FShaderParametersMetadataBuilder& InOutBuilder, FShaderParametersMetadataAllocations& InOutAllocations) const
{
	InOutBuilder.AddNestedStruct<FPCGStaticMeshSpawnerDataInterfaceParameters>(UID);
}

void UPCGStaticMeshSpawnerDataInterface::GetHLSL(FString& OutHLSL, FString const& InDataInterfaceName) const
{
	TMap<FString, FStringFormatArg> TemplateArgs =
	{
		{ TEXT("DataInterfaceName"), InDataInterfaceName },
		{ TEXT("MaxAttributes"), MAX_ATTRIBUTES },
		{ TEXT("MaxPrimitives"), PCGComputeConstants::MAX_PRIMITIVE_COMPONENTS_PER_SPAWNER },
	};

	OutHLSL += FString::Format(TEXT(
		"int {DataInterfaceName}_SelectorAttributeId;\n"
		"uint {DataInterfaceName}_NumAttributes;\n"
		"uint {DataInterfaceName}_NumPrimitives;\n"
		"uint {DataInterfaceName}_ApplyBounds;\n"
		"int {DataInterfaceName}_SelectedMeshAttributeId;\n"
		"uint4 {DataInterfaceName}_AttributeIdOffsetStrides[{MaxAttributes}];\n"
		"StructuredBuffer<float4> {DataInterfaceName}_PrimitiveMeshBoundsMin;\n"
		"StructuredBuffer<float4> {DataInterfaceName}_PrimitiveMeshBoundsMax;\n"
		"StructuredBuffer<int> {DataInterfaceName}_PrimitiveStringKeys;\n"
		"DECLARE_SCALAR_ARRAY(float, {DataInterfaceName}_SelectionCDF, {MaxPrimitives});\n"
		"\n"
		"int SMSpawner_GetSelectorAttributeId_{DataInterfaceName}()\n"
		"{\n"
		"	return {DataInterfaceName}_SelectorAttributeId;\n"
		"}\n"
		"\n"
		"uint SMSpawner_GetNumAttributes_{DataInterfaceName}()\n"
		"{\n"
		"	return {DataInterfaceName}_NumAttributes;\n"
		"}\n"
		"\n"
		"uint4 SMSpawner_GetAttributeIdOffsetStride_{DataInterfaceName}(uint InAttributeIndex)\n"
		"{\n"
		"	return {DataInterfaceName}_AttributeIdOffsetStrides[InAttributeIndex];\n"
		"}\n"
		"\n"
		"uint SMSpawner_GetNumPrimitives_{DataInterfaceName}()\n"
		"{\n"
		"	return {DataInterfaceName}_NumPrimitives;\n"
		"}\n"
		"\n"
		"bool SMSpawner_ShouldApplyBounds_{DataInterfaceName}()\n"
		"{\n"
		"	return {DataInterfaceName}_ApplyBounds > 0;\n"
		"}\n"
		"\n"
		"float3 SMSpawner_GetPrimitiveMeshBoundsMin_{DataInterfaceName}(uint InPrimitiveIndex)\n"
		"{\n"
		"	return {DataInterfaceName}_PrimitiveMeshBoundsMin[InPrimitiveIndex].xyz;\n"
		"}\n"
		"\n"
		"float3 SMSpawner_GetPrimitiveMeshBoundsMax_{DataInterfaceName}(uint InPrimitiveIndex)\n"
		"{\n"
		"	return {DataInterfaceName}_PrimitiveMeshBoundsMax[InPrimitiveIndex].xyz;\n"
		"}\n"
		"\n"
		"int SMSpawner_GetPrimitiveStringKey_{DataInterfaceName}(uint InPrimitiveIndex)\n"
		"{\n"
		"	return {DataInterfaceName}_PrimitiveStringKeys[InPrimitiveIndex];\n"
		"}\n"
		"\n"
		"uint SMSpawner_GetPrimitiveIndexFromStringKey_{DataInterfaceName}(int InMeshPathStringKey)\n"
		"{\n"
		"	for (uint Index = 0; Index < {DataInterfaceName}_NumPrimitives; ++Index)\n"
		"	{\n"
		"		if ({DataInterfaceName}_PrimitiveStringKeys[Index] == InMeshPathStringKey)\n"
		"		{\n"
		"			return Index;\n"
		"		}\n"
		"	}\n"
		"	\n"
		"	return (uint)-1;\n"
		"}\n"
		"\n"
		"float SMSpawner_GetPrimitiveSelectionCDF_{DataInterfaceName}(uint InPrimitiveIndex)\n"
		"{\n"
		"	return GET_SCALAR_ARRAY_ELEMENT({DataInterfaceName}_SelectionCDF, InPrimitiveIndex);\n"
		"}\n"
		"\n"
		"int SMSpawner_GetSelectedMeshAttributeId_{DataInterfaceName}()\n"
		"{\n"
		"	return {DataInterfaceName}_SelectedMeshAttributeId;\n"
		"}\n\n"
	), TemplateArgs);
}

UComputeDataProvider* UPCGStaticMeshSpawnerDataInterface::CreateDataProvider() const
{
	return NewObject<UPCGStaticMeshSpawnerDataProvider>();
}

bool UPCGStaticMeshSpawnerDataProvider::PerformPreExecuteReadbacks_GameThread(UPCGDataBinding* InBinding)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGStaticMeshSpawnerDataProvider::PerformPreExecuteReadbacks_GameThread);
	check(InBinding);

	if (!Super::PerformPreExecuteReadbacks_GameThread(InBinding))
	{
		return false;
	}

	const UPCGStaticMeshSpawnerSettings* Settings = CastChecked<UPCGStaticMeshSpawnerSettings>(ProducerKernel->GetSettings());

	// Obtain index of analysis data that we need to read back.
	if (AnalysisDataIndex == INDEX_NONE)
	{
		if (!Cast<UPCGMeshSelectorByAttribute>(Settings->MeshSelectorParameters))
		{
			// Non by-attribute selection does not need to do any readbacks.
			return true;
		}

		AnalysisDataIndex = ProducerKernel ? InBinding->GetFirstInputDataIndex(ProducerKernel, PCGStaticMeshSpawnerConstants::InstanceCountsPinLabel) : INDEX_NONE;

		if (AnalysisDataIndex == INDEX_NONE)
		{
			// No analysis data to read back.
			return true;
		}
	}

	// Readback analysis data - poll until readback complete (return true).
	if (!InBinding->ReadbackInputDataToCPU(AnalysisDataIndex))
	{
		return false;
	}

	const UPCGParamData* AnalysisResultsData = Cast<UPCGParamData>(InBinding->InputDataCollection.TaggedData[AnalysisDataIndex].Data);
	const UPCGMetadata* AnalysisMetadata = AnalysisResultsData ? AnalysisResultsData->ConstMetadata() : nullptr;

	const FPCGMetadataAttributeBase* AnalysisValueAttributeBase = AnalysisMetadata ? AnalysisMetadata->GetConstAttribute(PCGCountUniqueAttributeValuesConstants::ValueAttributeName) : nullptr;
	const FPCGMetadataAttributeBase* AnalysisCountAttributeBase = AnalysisMetadata ? AnalysisMetadata->GetConstAttribute(PCGCountUniqueAttributeValuesConstants::ValueCountAttributeName) : nullptr;

	if (AnalysisValueAttributeBase && AnalysisValueAttributeBase->GetTypeId() == PCG::Private::MetadataTypes<int32>::Id &&
		AnalysisCountAttributeBase && AnalysisCountAttributeBase->GetTypeId() == PCG::Private::MetadataTypes<int32>::Id)
	{
		const FPCGMetadataAttribute<int32>* ValueAttribute = static_cast<const FPCGMetadataAttribute<int32>*>(AnalysisValueAttributeBase);
		const FPCGMetadataAttribute<int32>* CountAttribute = static_cast<const FPCGMetadataAttribute<int32>*>(AnalysisCountAttributeBase);

		const int32 NumElements = AnalysisMetadata->GetItemCountForChild();

		StringKeyToInstanceCount.Reserve(NumElements);

		// TODO: Range based get would scale better.
		for (int64 MetadataKey = 0; MetadataKey < NumElements; ++MetadataKey)
		{
			StringKeyToInstanceCount.Add(ValueAttribute->GetValue(MetadataKey), CountAttribute->GetValue(MetadataKey));
		}
	}
	else
	{
		UE_LOG(LogPCG, Warning, TEXT("No analysis data received by static mesh spawner kernel, worst case instance allocations will be made."));

		if (InBinding->InputDataCollection.TaggedData[AnalysisDataIndex].Data->IsA<UPCGProxyForGPUData>())
		{
			UE_LOG(LogPCG, Error, TEXT("Data was not read back."));
		}

		return true;
	}

	return true;
}

bool UPCGStaticMeshSpawnerDataProvider::PrepareForExecute_GameThread(UPCGDataBinding* InBinding)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGStaticMeshSpawnerDataProvider::PrepareForExecute_GameThread);
	check(InBinding);
	const UPCGStaticMeshSpawnerSettings* Settings = CastChecked<UPCGStaticMeshSpawnerSettings>(ProducerKernel->GetSettings());

	if (!Super::PrepareForExecute_GameThread(InBinding))
	{
		return false;
	}

	if (!bRegisteredPrimitives)
	{
		InBinding->MeshSpawnersToPrimitives.Add(ProducerKernel, {});
		bRegisteredPrimitives = true;
	}

	TSharedPtr<FPCGContextHandle> ContextHandle = InBinding->ContextHandle.Pin();
	FPCGContext* Context = ContextHandle ? ContextHandle->GetContext() : nullptr;
	if (!ensure(Context))
	{
		InBinding->CompletedMeshSpawners.Add(ProducerKernel);
		return true;
	}

	UPCGComponent* SourceComponent = Cast<UPCGComponent>(Context->ExecutionSource.Get());
	if (!ensure(SourceComponent))
	{
		InBinding->CompletedMeshSpawners.Add(ProducerKernel);
		return true;
	}

	const UPCGMeshSelectorByAttribute* SelectorByAttribute = Cast<UPCGMeshSelectorByAttribute>(Settings->MeshSelectorParameters);
	if (SelectorAttributeId == INDEX_NONE && SelectorByAttribute)
	{
		const FName SelectorName = SelectorByAttribute->AttributeName;
		const FPCGDataCollectionDesc* InputDataDesc = InBinding->GetCachedKernelPinDataDesc(ProducerKernel, PCGPinConstants::DefaultInputLabel, /*bIsInput=*/true);

		if (!ensure(InputDataDesc))
		{
			InBinding->CompletedMeshSpawners.Add(ProducerKernel);
			return true;
		}

		bool bAnyPointsPresent = false;

		for (const FPCGDataDesc& Desc : InputDataDesc->DataDescs)
		{
			if (Desc.ElementCount <= 0)
			{
				continue;
			}

			bAnyPointsPresent = true;

			for (const FPCGKernelAttributeDesc& AttributeDesc : Desc.AttributeDescs)
			{
				if (AttributeDesc.AttributeKey.Identifier.Name == SelectorName && AttributeDesc.AttributeKey.Type == EPCGKernelAttributeType::StringKey)
				{
					SelectorAttributeId = AttributeDesc.AttributeId;
					break;
				}
			}

			if (SelectorAttributeId != INDEX_NONE)
			{
				break;
			}
		}

		if (SelectorAttributeId == INDEX_NONE)
		{
			// Mute this error if the point data is empty.
			if (!InputDataDesc->DataDescs.IsEmpty() && bAnyPointsPresent)
			{
				PCG_KERNEL_VALIDATION_ERR(Context, Settings, FText::Format(
					LOCTEXT("MeshSelectorAttributeNotFound", "Mesh selector attribute '{0}' not found."),
					FText::FromName(SelectorName)));
			}

			InBinding->CompletedMeshSpawners.Add(ProducerKernel);
			return true;
		}

		if (Context->AsyncState.ShouldStop())
		{
			return false;
		}
	}

	if (!bPrimitiveDescriptorsCreated)
	{
		CreatePrimitiveDescriptors(Context, InBinding);

		for (const FPCGProceduralISMComponentDescriptor& PrimitiveDescriptor : PrimitiveDescriptors)
		{
			UE_LOG(LogPCG, Verbose, TEXT("Request '%s' to load."), *PrimitiveDescriptor.StaticMesh.ToString());

			PrimitiveDescriptor.StaticMesh.LoadAsync({});
		}

		bPrimitiveDescriptorsCreated = true;

		if (Context->AsyncState.ShouldStop())
		{
			return false;
		}
	}

	if (!bStaticMeshesLoaded)
	{
		for (const FPCGProceduralISMComponentDescriptor& PrimitiveDescriptor : PrimitiveDescriptors)
		{
			if (PrimitiveDescriptor.StaticMesh.IsPending())
			{
				UE_LOG(LogPCG, Verbose, TEXT("Waiting for '%s' to load."), *PrimitiveDescriptor.StaticMesh.ToString());

				return false;
			}
		}

		bStaticMeshesLoaded = true;
	}

	if (!bPrimitivesSetUp)
	{
		if (!SetupPrimitives(Context, InBinding))
		{
			return false;
		}

		bPrimitivesSetUp = true;

		if (NumPrimitivesSetup > 0)
		{
			SourceComponent->NotifyProceduralInstancesInUse();
		}
		else
		{
			// No component set up means we have no more work to do.
			InBinding->CompletedMeshSpawners.Add(ProducerKernel);
			return true;
		}

		if (Context->AsyncState.ShouldStop())
		{
			return false;
		}
	}

	// We know the name and type of the selected mesh attribute statically and declared the attribute in GetKernelAttributeKeys,
	// so the attribute ID should be present in the attribute table.
	SelectedMeshAttributeId = InBinding->GetAttributeId(Settings->OutAttributeName, EPCGKernelAttributeType::StringKey);
	ensure(SelectedMeshAttributeId != INDEX_NONE);

	InBinding->CompletedMeshSpawners.Add(ProducerKernel);
	return true;
}

FComputeDataProviderRenderProxy* UPCGStaticMeshSpawnerDataProvider::GetRenderProxy()
{
	return new FPCGStaticMeshSpawnerDataProviderProxy(AttributeIdOffsetStrides, SelectorAttributeId, PrimitiveStringKeys, PrimitiveSelectionCDF, SelectedMeshAttributeId, PrimitiveMeshBounds);
}

void UPCGStaticMeshSpawnerDataProvider::Reset()
{
	Super::Reset();

	AttributeIdOffsetStrides.Empty();
	PrimitiveStringKeys.Empty();
	PrimitiveMeshBounds.Empty();
	PrimitiveSelectionCDF.Empty();
	SelectorAttributeId = INDEX_NONE;
	NumInputPoints = 0;
	SelectedMeshAttributeId = INDEX_NONE;
	StringKeyToInstanceCount.Empty();
	AnalysisDataIndex = INDEX_NONE;
	bPrimitiveDescriptorsCreated = false;
	PrimitiveDescriptors.Empty();
	CustomFloatCount = 0;
	bPrimitivesSetUp = false;
	NumPrimitivesProcessed = 0;
	NumPrimitivesSetup = 0;
	bRegisteredPrimitives = false;
	bStaticMeshesLoaded = false;
}

void UPCGStaticMeshSpawnerDataProvider::CreatePrimitiveDescriptors(FPCGContext* InContext, UPCGDataBinding* InBinding)
{
	PrimitiveDescriptors.Empty();

	const UPCGStaticMeshSpawnerSettings* Settings = CastChecked<UPCGStaticMeshSpawnerSettings>(ProducerKernel->GetSettings());

	AActor* TargetActor = Settings->TargetActor.Get() ? Settings->TargetActor.Get() : InContext->GetTargetActor(nullptr);
	if (!ensure(Settings->MeshSelectorParameters) || !ensure(TargetActor))
	{
		return;
	}

	const FPCGDataCollectionDesc* InputDataDesc = InBinding->GetCachedKernelPinDataDesc(ProducerKernel, PCGPinConstants::DefaultInputLabel, /*bIsInput=*/true);

	if (!ensure(InputDataDesc))
	{
		return;
	}

	const int32 TotalInputPointCount = InputDataDesc->ComputeDataElementCount(EPCGDataType::Point);
	if (TotalInputPointCount == 0)
	{
		return;
	}

	UPCGComponent* SourceComponent = Cast<UPCGComponent>(InContext->ExecutionSource.Get());
	if (!SourceComponent)
	{
		return;
	}

	if (Settings->InstanceDataPackerParameters && Settings->InstanceDataPackerParameters->GetAttributeNames(/*OutNames=*/nullptr))
	{
		TArray<FName> AttributeNames;
		Settings->InstanceDataPackerParameters->GetAttributeNames(&AttributeNames);

		for (FName AttributeName : AttributeNames)
		{
			if (AttributeName == NAME_None)
			{
				PCG_KERNEL_VALIDATION_ERR(InContext, Settings,
					FText::Format(LOCTEXT("InstanceDataAttributeInvalid", "Invalid instance data attribute specified '{0}'."), FText::FromName(AttributeName)));

				continue;
			}

			if (!InputDataDesc->ContainsAttributeOnAnyData(AttributeName))
			{
				PCG_KERNEL_VALIDATION_ERR(InContext, Settings,
					FText::Format(LOCTEXT("InstanceDataAttributeNotFound", "Instance data attribute '{0}' not found."), FText::FromName(AttributeName)));

				continue;
			}
		}

		PCGDataForGPUHelpers::ComputeCustomFloatPacking(InContext, Settings, AttributeNames, InBinding, InputDataDesc, CustomFloatCount, AttributeIdOffsetStrides);
	}

	if (const UPCGMeshSelectorByAttribute* SelectorByAttribute = Cast<UPCGMeshSelectorByAttribute>(Settings->MeshSelectorParameters))
	{
		const FName SelectorName = SelectorByAttribute->AttributeName;

		// Compute how many instances we expect for each mesh, if we do not already have the answer from analysis.
		if (StringKeyToInstanceCount.IsEmpty())
		{
			for (const FPCGDataDesc& Desc : InputDataDesc->DataDescs)
			{
				if (Desc.ElementCount <= 0)
				{
					continue;
				}

				for (const FPCGKernelAttributeDesc& AttributeDesc : Desc.AttributeDescs)
				{
					if (AttributeDesc.AttributeKey.Identifier.Name == SelectorName)
					{
						if (AttributeDesc.AttributeKey.Type == EPCGKernelAttributeType::StringKey)
						{
							for (int32 AttributeStringKeyValue : AttributeDesc.GetUniqueStringKeys())
							{
								if (AttributeStringKeyValue > 0)
								{
									StringKeyToInstanceCount.FindOrAdd(AttributeStringKeyValue, 0) += Desc.GetElementCountForAttribute(AttributeDesc);
								}
							}

							// We could early out but we currently continue looping to generate the below warning for mismatched types.
						}
						else
						{
							// Currently only a single type per attribute name is supported (the name/type in the attribute table). It's possible to wire a graph
							// for which an attribute is present with multiple types. Warn if this is encountered.
							PCG_KERNEL_VALIDATION_WARN(InContext, Settings, FText::Format(
								LOCTEXT("MeshSelectorAttributeNotUsable", "Attribute '{0}' not usable for mesh selection, only attributes of type String Key are supported."),
								FText::FromName(SelectorName)));
						}
					}
				}
			}
		}

		PrimitiveDescriptors.Reserve(StringKeyToInstanceCount.Num());

		for (const TPair<int32, uint32>& StringKeyAndInstanceCount : StringKeyToInstanceCount)
		{
			if (StringKeyAndInstanceCount.Value == 0)
			{
				continue;
			}

			if (!ensure(InBinding->GetStringTable().IsValidIndex(StringKeyAndInstanceCount.Key)))
			{
				continue;
			}

			const FString& MeshPathString = InBinding->GetStringTable()[StringKeyAndInstanceCount.Key];
			if (MeshPathString.IsEmpty())
			{
				continue;
			}

			if (PrimitiveDescriptors.Num() >= PCGComputeConstants::MAX_PRIMITIVE_COMPONENTS_PER_SPAWNER)
			{
				PCG_KERNEL_VALIDATION_WARN(InContext, Settings,
					FText::Format(PCGStaticMeshSpawnerDataInterface::TooManyPrimitivesFormat, FText::FromString(FString::FromInt(PCGComputeConstants::MAX_PRIMITIVE_COMPONENTS_PER_SPAWNER))));

				break;
			}

			FPCGProceduralISMComponentDescriptor Descriptor;
			Descriptor = SelectorByAttribute->TemplateDescriptor;
			Descriptor.NumInstances = StringKeyAndInstanceCount.Value;
			Descriptor.WorldBounds = SourceComponent->GetGridBounds();
			Descriptor.NumCustomFloats = CustomFloatCount;
			Descriptor.StaticMesh = FSoftObjectPath(MeshPathString);

			// Sanity check instance count.
			if (!ensure(Descriptor.NumInstances <= TotalInputPointCount))
			{
				Descriptor.NumInstances = TotalInputPointCount;
			}

			PrimitiveStringKeys.Add(StringKeyAndInstanceCount.Key);
			PrimitiveDescriptors.Add(MoveTemp(Descriptor));
		}

		PrimitiveSelectionCDF.SetNumZeroed(PrimitiveDescriptors.Num());
	}
	else if (const UPCGMeshSelectorWeighted* SelectorWeighted = Cast<UPCGMeshSelectorWeighted>(Settings->MeshSelectorParameters))
	{
		if (SelectorWeighted->MeshEntries.IsEmpty())
		{
			PCG_KERNEL_VALIDATION_ERR(InContext, Settings, PCGStaticMeshSpawnerDataInterface::NoMeshEntriesFormat);
			return;
		}

		float CumulativeWeight = 0.0f;

		float TotalWeight = 0.0f;
		for (const FPCGMeshSelectorWeightedEntry& Entry : SelectorWeighted->MeshEntries)
		{
			TotalWeight += Entry.Weight;
		}

		if (!ensure(TotalWeight > UE_SMALL_NUMBER))
		{
			return;
		}

		PrimitiveSelectionCDF.Reserve(SelectorWeighted->MeshEntries.Num());

		PrimitiveDescriptors.Reserve(SelectorWeighted->MeshEntries.Num());

		for (const FPCGMeshSelectorWeightedEntry& Entry : SelectorWeighted->MeshEntries)
		{
			if (Entry.Descriptor.StaticMesh.IsNull())
			{
				continue;
			}

			if (PrimitiveDescriptors.Num() >= PCGComputeConstants::MAX_PRIMITIVE_COMPONENTS_PER_SPAWNER)
			{
				PCG_KERNEL_VALIDATION_WARN(InContext, Settings,
					FText::Format(PCGStaticMeshSpawnerDataInterface::TooManyPrimitivesFormat, FText::FromString(FString::FromInt(PCGComputeConstants::MAX_PRIMITIVE_COMPONENTS_PER_SPAWNER))));

				break;
			}

			if (!Entry.Descriptor.StaticMesh.IsNull())
			{
				const float Weight = float(Entry.Weight) / TotalWeight;
				CumulativeWeight += Weight;
				PrimitiveSelectionCDF.Add(CumulativeWeight);
				PrimitiveStringKeys.Add(InBinding->GetStringTable().IndexOfByKey(Entry.Descriptor.StaticMesh.ToString()));

				FPCGProceduralISMComponentDescriptor Descriptor;
				Descriptor = Entry.Descriptor;
				Descriptor.WorldBounds = SourceComponent->GetGridBounds();
				Descriptor.NumCustomFloats = CustomFloatCount;
				Descriptor.StaticMesh = Entry.Descriptor.StaticMesh;

				int32 InstanceCount = FMath::CeilToInt(TotalInputPointCount * Weight);

				if (SelectorWeighted->MeshEntries.Num() > 1)
				{
					// Since we'll be selecting meshes based on random draws using the point random seeds which we don't have on CPU,
					// we may pick more or less than the expected number of instances for each mesh. Use binomial variance to calculate
					// the overallocation that gives 99.7% confidence (3 sigma).
					const double Variance = double(TotalInputPointCount) * Weight * (1.0f - Weight);
					const double Sigma = FMath::Sqrt(Variance);
					const uint32 AdditionalAllocation = FMath::CeilToInt(3.0f * Sigma);

					// Useful for debugging instance counts.
					//UE_LOG(LogPCG, Warning, TEXT("Over allocation, N = %d, P = %.2f, NP = %d, 3sig = %f, Extra = %d (%.2f%%), Calculated = %d, Final = %d"), TotalInputPointCount, Weight, InstanceCount, float(3.0f * Sigma), AdditionalAllocation, float(AdditionalAllocation) / TotalInputPointCount, InstanceCount + AdditionalAllocation, FMath::Min(InstanceCount + AdditionalAllocation, TotalInputPointCount));

					InstanceCount += AdditionalAllocation;
				}

				Descriptor.NumInstances = FMath::Min(InstanceCount, TotalInputPointCount);

				PrimitiveDescriptors.Add(MoveTemp(Descriptor));
			}
			else
			{
				PCG_KERNEL_VALIDATION_ERR(InContext, Settings,
					FText::Format(PCGStaticMeshSpawnerDataInterface::CouldNotLoadStaticMeshFormat, FText::FromString(Entry.Descriptor.StaticMesh.ToString())));
			}
		}
	}

	// Validate instance count is not too large.
	uint64 TotalInstanceCount = 0;
	for (const FPCGProceduralISMComponentDescriptor& Desc : PrimitiveDescriptors)
	{
		TotalInstanceCount += Desc.NumInstances;
	}

	if (TotalInstanceCount >= MAX_INSTANCE_ID)
	{
		PCG_KERNEL_VALIDATION_ERR(InContext, Settings, FText::Format(
			LOCTEXT("TooManyInstances", "Tried to spawn too many instances ({0}), procedural ISM component creation skipped and instances will not be rendered."),
			FText::FromString(FString::FromInt(TotalInstanceCount))));

		return;
	}

	FPCGSpawnerPrimitives& Primitives = InBinding->MeshSpawnersToPrimitives.FindOrAdd(ProducerKernel);
	Primitives.NumCustomFloats = CustomFloatCount;
	Primitives.AttributeIdOffsetStrides = AttributeIdOffsetStrides;
	Primitives.SelectorAttributeId = SelectorAttributeId;
	Primitives.SelectionCDF = PrimitiveSelectionCDF;
	Primitives.PrimitiveStringKeys = PrimitiveStringKeys;
	Primitives.PrimitiveMeshBounds = PrimitiveMeshBounds;

	// Useful for debugging instance counts.
	//UE_LOG(LogPCG, Log, TEXT("Input points %d, total instance count %d (+ %.2f%%)"), TotalInputPointCount, Primitives.NumInstancesAllPrimitives, (float(Primitives.NumInstancesAllPrimitives - TotalInputPointCount) / TotalInputPointCount) * 100.0f);
}

bool UPCGStaticMeshSpawnerDataProvider::SetupPrimitives(FPCGContext* InContext, UPCGDataBinding* InBinding)
{
	const UPCGStaticMeshSpawnerSettings* Settings = CastChecked<UPCGStaticMeshSpawnerSettings>(ProducerKernel->GetSettings());

	AActor* TargetActor = Settings->TargetActor.Get() ? Settings->TargetActor.Get() : InContext->GetTargetActor(nullptr);
	if (!ensure(TargetActor))
	{
		return true;
	}

	FPCGSpawnerPrimitives* Primitives = InBinding->MeshSpawnersToPrimitives.Find(ProducerKernel);
	if (!Primitives)
	{
		return true;
	}

	while (NumPrimitivesProcessed < PrimitiveDescriptors.Num())
	{
		const FPCGProceduralISMComponentDescriptor& Desc = PrimitiveDescriptors[NumPrimitivesProcessed];

		FPCGProceduralISMCBuilderParameters Params;
		Params.Descriptor = Desc;
		Params.bAllowDescriptorChanges = false;

		UPCGComponent* SourceComponent = Cast<UPCGComponent>(InContext->ExecutionSource.Get());
		check(SourceComponent);

		UPCGManagedProceduralISMComponent* MISMC = PCGManagedProceduralISMComponent::GetOrCreateManagedProceduralISMC(TargetActor, SourceComponent, Settings->UID, Params);

		if (MISMC)
		{
			MISMC->SetCrc(InContext->DependenciesCrc);

			// Don't bother registering the resource change as we're transient anyway.
			//InContext->TouchedResources.Emplace(MISMC);

			FPCGSpawnerPrimitive Primitive;
			Primitive.Component = MISMC->GetComponent();
			Primitive.NumInstances = Desc.NumInstances;

			Primitives->Primitives.Add(MoveTemp(Primitive));

			if (Settings->bApplyMeshBoundsToPoints)
			{
				PrimitiveMeshBounds.Add(Desc.StaticMesh->GetBoundingBox());
			}

			++NumPrimitivesSetup;
		}

		++NumPrimitivesProcessed;

		if (NumPrimitivesProcessed < PrimitiveDescriptors.Num() && InContext->AsyncState.ShouldStop())
		{
			return false;
		}
	}

	return true;
}

FPCGStaticMeshSpawnerDataProviderProxy::FPCGStaticMeshSpawnerDataProviderProxy(
	const TArray<FUintVector4>& InAttributeIdOffsetStrides,
	int32 InSelectorAttributeId,
	const TArray<int32>& InPrimitiveStringKeys,
	TArray<float> InSelectionCDF,
	int32 InSelectedMeshAttributeId,
	const TArray<FBox>& InPrimitiveMeshBounds)
	: AttributeIdOffsetStrides(InAttributeIdOffsetStrides)
	, SelectionCDF(InSelectionCDF)
	, SelectorAttributeId(InSelectorAttributeId)
	, PrimitiveStringKeys(InPrimitiveStringKeys)
	, SelectedMeshAttributeId(InSelectedMeshAttributeId)
{
	PrimitiveMeshBoundsMin.Reserve(InPrimitiveMeshBounds.Num());
	PrimitiveMeshBoundsMax.Reserve(InPrimitiveMeshBounds.Num());

	for (int Index = 0; Index < InPrimitiveMeshBounds.Num(); ++Index)
	{
		PrimitiveMeshBoundsMin.Add(FVector4f(InPrimitiveMeshBounds[Index].Min.X, InPrimitiveMeshBounds[Index].Min.Y, InPrimitiveMeshBounds[Index].Min.Z, /*Unused*/0.0f));
		PrimitiveMeshBoundsMax.Add(FVector4f(InPrimitiveMeshBounds[Index].Max.X, InPrimitiveMeshBounds[Index].Max.Y, InPrimitiveMeshBounds[Index].Max.Z, /*Unused*/0.0f));
	}
}

bool FPCGStaticMeshSpawnerDataProviderProxy::IsValid(FValidationData const& InValidationData) const
{
	return InValidationData.ParameterStructSize == sizeof(FParameters);
}

void FPCGStaticMeshSpawnerDataProviderProxy::AllocateResources(FRDGBuilder& GraphBuilder, FAllocationData const& InAllocationData)
{
	if (!PrimitiveStringKeys.IsEmpty())
	{
		const FRDGBufferDesc BufferDesc = FRDGBufferDesc::CreateStructuredDesc(PrimitiveStringKeys.GetTypeSize(), PrimitiveStringKeys.Num());

		FRDGBufferRef Buffer = GraphBuilder.CreateBuffer(BufferDesc, TEXT("PCGStaticMeshSpawner_PrimitiveStringKeys"));
		PrimitiveStringKeysBufferSRV = GraphBuilder.CreateSRV(Buffer);

		GraphBuilder.QueueBufferUpload(Buffer, MakeArrayView(PrimitiveStringKeys));
	}
	else
	{
		PrimitiveStringKeysBufferSRV = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(GSystemTextures.GetDefaultStructuredBuffer(GraphBuilder, PrimitiveStringKeys.GetTypeSize())));
	}

	if (!PrimitiveMeshBoundsMin.IsEmpty())
	{
		const FRDGBufferDesc BufferDesc = FRDGBufferDesc::CreateStructuredDesc(PrimitiveMeshBoundsMin.GetTypeSize(), PrimitiveMeshBoundsMin.Num());

		FRDGBufferRef Buffer = GraphBuilder.CreateBuffer(BufferDesc, TEXT("PCGStaticMeshSpawner_PrimitiveMeshBoundsMin"));
		PrimitiveMeshBoundsMinBufferSRV = GraphBuilder.CreateSRV(Buffer);

		GraphBuilder.QueueBufferUpload(Buffer, MakeArrayView(PrimitiveMeshBoundsMin));
	}
	else
	{
		PrimitiveMeshBoundsMinBufferSRV = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(GSystemTextures.GetDefaultStructuredBuffer(GraphBuilder, PrimitiveMeshBoundsMin.GetTypeSize())));
	}

	if (!PrimitiveMeshBoundsMax.IsEmpty())
	{
		const FRDGBufferDesc BufferDesc = FRDGBufferDesc::CreateStructuredDesc(PrimitiveMeshBoundsMax.GetTypeSize(), PrimitiveMeshBoundsMax.Num());

		FRDGBufferRef Buffer = GraphBuilder.CreateBuffer(BufferDesc, TEXT("PCGStaticMeshSpawner_PrimitiveMeshBoundsMax"));
		PrimitiveMeshBoundsMaxBufferSRV = GraphBuilder.CreateSRV(Buffer);

		GraphBuilder.QueueBufferUpload(Buffer, MakeArrayView(PrimitiveMeshBoundsMax));
	}
	else
	{
		PrimitiveMeshBoundsMaxBufferSRV = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(GSystemTextures.GetDefaultStructuredBuffer(GraphBuilder, PrimitiveMeshBoundsMax.GetTypeSize())));
	}
}

void FPCGStaticMeshSpawnerDataProviderProxy::GatherDispatchData(FDispatchData const& InDispatchData)
{
	const TStridedView<FParameters> ParameterArray = MakeStridedParameterView<FParameters>(InDispatchData);
	for (int32 InvocationIndex = 0; InvocationIndex < ParameterArray.Num(); ++InvocationIndex)
	{
		FParameters& Parameters = ParameterArray[InvocationIndex];

		Parameters.NumAttributes = AttributeIdOffsetStrides.Num();
		Parameters.NumPrimitives = SelectionCDF.Num();
		Parameters.SelectorAttributeId = SelectorAttributeId;
		Parameters.SelectedMeshAttributeId = SelectedMeshAttributeId;

		for (int32 Index = 0; Index < AttributeIdOffsetStrides.Num(); ++Index)
		{
			Parameters.AttributeIdOffsetStrides[Index] = AttributeIdOffsetStrides[Index];
		}

		Parameters.PrimitiveStringKeys = PrimitiveStringKeysBufferSRV;

		for (int32 Index = 0; Index < SelectionCDF.Num(); ++Index)
		{
			GET_SCALAR_ARRAY_ELEMENT(Parameters.SelectionCDF, Index) = SelectionCDF[Index];
		}

		Parameters.ApplyBounds = PrimitiveMeshBoundsMin.IsEmpty() ? 0 : 1;

		Parameters.PrimitiveMeshBoundsMin = PrimitiveMeshBoundsMinBufferSRV;
		Parameters.PrimitiveMeshBoundsMax = PrimitiveMeshBoundsMaxBufferSRV;
	}
}

#undef LOCTEXT_NAMESPACE
