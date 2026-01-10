// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGTextureDataInterface.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGData.h"
#include "PCGSettings.h"
#include "Compute/PCGDataBinding.h"
#include "Data/PCGTextureData.h"
#include "Helpers/PCGBlueprintHelpers.h"
#include "Helpers/PCGHelpers.h"

#include "GlobalRenderResources.h"
#include "RHIResources.h"
#include "RHIStaticStates.h"
#include "RenderGraphBuilder.h"
#include "ShaderCompilerCore.h"
#include "ShaderCore.h"
#include "ShaderParameterMetadataBuilder.h"
#include "SystemTextures.h"
#include "TextureResource.h"
#include "ComputeFramework/ComputeKernelPermutationSet.h"
#include "ComputeFramework/ComputeKernelPermutationVector.h"
#include "ComputeFramework/ShaderParamTypeDefinition.h"
#include "Engine/Texture.h"
#include "GameFramework/Actor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGTextureDataInterface)

#define LOCTEXT_NAMESPACE "PCGTextureDataInterface"

namespace PCGTextureDataInterfaceConstants
{
	const TCHAR* EnableMultipleTextureObjectsPermutationName = TEXT("ENABLE_MULTIPLE_TEXTURE_OBJECTS");
}

namespace PCGTextureDataInterfaceHelpers
{
	void GetSupportedShaderFunctions(TArray<FShaderFunctionDefinition>& OutFunctions)
	{
		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("GetNumData"))
			.AddReturnType(EShaderFundamentalType::Uint);

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("GetNumElements"))
			.AddReturnType(EShaderFundamentalType::Uint, 2)
			.AddParam(EShaderFundamentalType::Uint); // InDataIndex

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("GetThreadData"))
			.AddReturnType(EShaderFundamentalType::Bool)
			.AddParam(EShaderFundamentalType::Uint) // InThreadIndex
			.AddParam(EShaderFundamentalType::Uint, 0, 0, EShaderParamModifier::Out) // OutDataIndex
			.AddParam(EShaderFundamentalType::Uint, 2, 0, EShaderParamModifier::Out); // OutElementIndex
	}
}

FPCGTextureBindingInfo::FPCGTextureBindingInfo(const UPCGBaseTextureData* InTextureData)
{
	check(InTextureData);

	ResourceType = InTextureData->GetTextureResourceType();
	Texture = InTextureData->GetTextureRHI();
	ExportedTexture = InTextureData->GetRefCountedTexture();
	Transform = InTextureData->GetTransform();
	TextureBounds = InTextureData->GetBounds();
	bPointSample = InTextureData->Filter == EPCGTextureFilter::Point;

	if (Texture.IsValid())
	{
		const FRHITextureDesc& Desc = Texture->GetDesc();
		Size = Desc.Extent;
		Dimension = Desc.Dimension;
	}
}

FPCGTextureBindingInfo::FPCGTextureBindingInfo(const FPCGDataDesc& InDataDesc, const FTransform& InTransform)
{
	// @todo_pcg: Need to support other information here too, like format, slice index, resource type, etc.
	Size = FIntPoint(FMath::Max(InDataDesc.ElementCount2D.X, 1), FMath::Max(InDataDesc.ElementCount2D.Y, 1));
	Transform = InTransform;
	TextureBounds = FBox(FVector(-1.0f, -1.0f, 0.0f), FVector(1.0f, 1.0f, 0.0f)).TransformBy(Transform);
}

bool FPCGTextureBindingInfo::operator==(const FPCGTextureBindingInfo& Other) const
{
	return ResourceType == Other.ResourceType
		&& Texture == Other.Texture
		&& ExportedTexture == Other.ExportedTexture
		&& Size == Other.Size
		&& Dimension == Other.Dimension
		&& bPointSample == Other.bPointSample
		&& Transform.Equals(Transform)
		&& TextureBounds == Other.TextureBounds;
}

void UPCGTextureDataInterface::GetSupportedInputs(TArray<FShaderFunctionDefinition>& OutFunctions) const
{
	// TODO: Support texture3d, etc. UNiagaraDataInterfaceRenderTarget2D and related are useful references.

	PCGTextureDataInterfaceHelpers::GetSupportedShaderFunctions(OutFunctions);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("GetTexCoords"))
		.AddReturnType(EShaderFundamentalType::Float, 2)
		.AddParam(EShaderFundamentalType::Float, 2) // WorldPos
		.AddParam(EShaderFundamentalType::Float, 2) // Min
		.AddParam(EShaderFundamentalType::Float, 2); // Max

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("Sample"))
		.AddReturnType(EShaderFundamentalType::Float, 4)
		.AddParam(EShaderFundamentalType::Uint) // InDataIndex
		.AddParam(EShaderFundamentalType::Float, 2); // InTextureUV

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("SampleWorldPos"))
		.AddReturnType(EShaderFundamentalType::Float, 4)
		.AddParam(EShaderFundamentalType::Uint) // InDataIndex
		.AddParam(EShaderFundamentalType::Float, 2); // WorldPos

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("Load"))
		.AddReturnType(EShaderFundamentalType::Float, 4)
		.AddParam(EShaderFundamentalType::Uint) // InDataIndex
		.AddParam(EShaderFundamentalType::Uint, 2); // InElementIndex

	{
		// Pseudo-deprecated. Assumes data index is 0.
		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("Sample"))
			.AddReturnType(EShaderFundamentalType::Float, 4)
			.AddParam(EShaderFundamentalType::Float, 2); // InTextureUV

		OutFunctions.AddDefaulted_GetRef()
			.SetName(TEXT("SampleWorldPos"))
			.AddReturnType(EShaderFundamentalType::Float, 4)
			.AddParam(EShaderFundamentalType::Float, 2); // WorldPos
	}
}

void UPCGTextureDataInterface::GetSupportedOutputs(TArray<FShaderFunctionDefinition>& OutFunctions) const
{
	PCGTextureDataInterfaceHelpers::GetSupportedShaderFunctions(OutFunctions);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("Store"))
		.AddParam(EShaderFundamentalType::Uint) // InDataIndex
		.AddParam(EShaderFundamentalType::Uint, 2) // InElementIndex
		.AddParam(EShaderFundamentalType::Float, 4); // InValue

	// @todo_pcg: We could also add a LoadUAV function. Note that we can't use the existing input Load function because it loads from the SRV, not the UAV.
}

BEGIN_SHADER_PARAMETER_STRUCT(FPCGTextureDataInterfaceParameters,)
	SHADER_PARAMETER_RDG_TEXTURE_SRV_ARRAY(Texture2D, Texture, [PCGTextureDataInterfaceConstants::MAX_NUM_SRV_BINDINGS])
	SHADER_PARAMETER_RDG_TEXTURE_SRV_ARRAY(Texture2DArray, TextureArray, [PCGTextureDataInterfaceConstants::MAX_NUM_SRV_BINDINGS])
	SHADER_PARAMETER_RDG_TEXTURE_UAV_ARRAY(RWTexture2D<float4>, TextureUAV, [PCGTextureDataInterfaceConstants::MAX_NUM_UAV_BINDINGS])
	SHADER_PARAMETER_SAMPLER(SamplerState, Sampler_Linear)
	SHADER_PARAMETER_SAMPLER(SamplerState, Sampler_Point)
	SHADER_PARAMETER_ARRAY(FVector4f, TextureBounds, [PCGTextureDataInterfaceConstants::MAX_NUM_SRV_BINDINGS])
	SHADER_PARAMETER(uint32, NumTextureInfos)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FUintVector4>, TextureInfos)
END_SHADER_PARAMETER_STRUCT()

void UPCGTextureDataInterface::GetShaderParameters(TCHAR const* UID, FShaderParametersMetadataBuilder& InOutBuilder, FShaderParametersMetadataAllocations& InOutAllocations) const
{
	InOutBuilder.AddNestedStruct<FPCGTextureDataInterfaceParameters>(UID);
}

TCHAR const* UPCGTextureDataInterface::TemplateFilePath = TEXT("/Plugin/PCG/Private/PCGTextureDataInterface.ush");

TCHAR const* UPCGTextureDataInterface::GetShaderVirtualPath() const
{
	return TemplateFilePath;
}

void UPCGTextureDataInterface::GetShaderHash(FString& InOutKey) const
{
	GetShaderFileHash(TemplateFilePath, EShaderPlatform::SP_PCD3D_SM5).AppendString(InOutKey);
}

void UPCGTextureDataInterface::GetHLSL(FString& OutHLSL, FString const& InDataInterfaceName) const
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

void UPCGTextureDataInterface::GetDefines(FComputeKernelDefinitionSet& OutDefinitionSet) const
{
	Super::GetDefines(OutDefinitionSet);

	OutDefinitionSet.Defines.Add(FComputeKernelDefinition(TEXT("PCG_MAX_NUM_SRV_BINDINGS"), FString::FromInt(PCGTextureDataInterfaceConstants::MAX_NUM_SRV_BINDINGS)));
}

void UPCGTextureDataInterface::GetPermutations(FComputeKernelPermutationVector& OutPermutationVector) const
{
	OutPermutationVector.AddPermutation(PCGTextureDataInterfaceConstants::EnableMultipleTextureObjectsPermutationName, /*NumValues=*/2);
}

UComputeDataProvider* UPCGTextureDataInterface::CreateDataProvider() const
{
	return NewObject<UPCGTextureDataProvider>();
}

void UPCGTextureDataProvider::Initialize(const UComputeDataInterface* InDataInterface, UObject* InBinding, uint64 InInputMask, uint64 InOutputMask)
{
	Super::Initialize(InDataInterface, InBinding, InInputMask, InOutputMask);

	const UPCGTextureDataInterface* DataInterface = CastChecked<UPCGTextureDataInterface>(InDataInterface);
	bInitializeFromDataCollection = DataInterface->GetInitializeFromDataCollection();
}

FComputeDataProviderRenderProxy* UPCGTextureDataProvider::GetRenderProxy()
{
	return new FPCGTextureDataProviderProxy(this);
}

void UPCGTextureDataProvider::Reset()
{
	Super::Reset();

	BindingInfos.Empty();
	TextureInfos.Empty();
	bInitializeFromDataCollection = false;
}

bool UPCGTextureDataProvider::PrepareForExecute_GameThread(UPCGDataBinding* InBinding)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGTextureDataProvider::PrepareForExecute_GameThread);

	check(InBinding);

	if (!Super::PrepareForExecute_GameThread(InBinding))
	{
		return false;
	}

	// @todo_pcg: Check if the texture size is too large
	//if (PCGComputeHelpers::IsBufferSizeTooLarge(PinDesc->ComputePackedSizeBytes()))
	//{
	//	PinDesc = nullptr;
	//}

	if (bInitializeFromDataCollection)
	{
		BuildInfosFromDataCollection(InBinding);
	}
	else
	{
		BuildInfosFromDataDescription(InBinding);
	}

	return true;
}

void UPCGTextureDataProvider::BuildInfosFromDataCollection(UPCGDataBinding* InBinding)
{
	check(InBinding);

	// Take any input pin label alias to obtain the data from the input data collection.
	check(!DownstreamInputPinLabelAliases.IsEmpty());
	const TArray<FPCGTaggedData> InputTaggedData = InBinding->InputDataCollection.GetInputsByPin(DownstreamInputPinLabelAliases[0]);

	for (int I = 0; I < InputTaggedData.Num(); ++I)
	{
		const FPCGTaggedData& TaggedData = InputTaggedData[I];
		const UPCGBaseTextureData* TextureData = Cast<UPCGBaseTextureData>(TaggedData.Data);

		if (!ensure(TextureData))
		{
			UE_LOG(LogPCG, Error, TEXT("Unsupported data type encountered by texture data interface: '%s'"), TaggedData.Data ? *TaggedData.Data->GetName() : TEXT("NULL"));
			continue;
		}

		FPCGTextureBindingInfo BindingInfo(TextureData);
		int32 BindingIndex = BindingInfos.Find(BindingInfo);

		if (BindingIndex == INDEX_NONE)
		{
			if (BindingInfos.Num() < PCGTextureDataInterfaceConstants::MAX_NUM_SRV_BINDINGS)
			{
				BindingIndex = BindingInfos.Add(MoveTemp(BindingInfo));
			}
			else
			{
				UE_LOG(LogPCG, Warning, TEXT("Texture data interface on pin '%s' received too many textures to bind. Only the first %d textures will be bound."),
					*DownstreamInputPinLabelAliases[0].ToString(),
					PCGTextureDataInterfaceConstants::MAX_NUM_SRV_BINDINGS);
				continue;
			}
		}

		FPCGTextureInfo& TextureInfo = TextureInfos.Emplace_GetRef();
		TextureInfo.BindingIndex = BindingIndex;
		TextureInfo.SliceIndex = TextureData->GetTextureSlice();
	}
}

void UPCGTextureDataProvider::BuildInfosFromDataDescription(UPCGDataBinding* InBinding)
{
	check(InBinding);
	check(PinDesc);

	TSharedPtr<FPCGContextHandle> ContextHandle = InBinding->ContextHandle.Pin();
	FPCGContext* Context = ContextHandle ? ContextHandle->GetContext() : nullptr;

	if (!ensure(Context))
	{
		return;
	}

	// @todo_pcg: Instead of using this helper and relying on an actor (which may not exist for some cases), we should use the context execution source/state.
	AActor* OriginalActor = UPCGBlueprintHelpers::GetOriginalComponent(*Context)->GetOwner();
	if (!ensure(OriginalActor))
	{
		return;
	}

	const FBox OriginalActorLocalBounds = PCGHelpers::GetActorLocalBounds(OriginalActor);
	FTransform Transform = OriginalActor->GetTransform();
	Transform.SetScale3D(Transform.GetScale3D() * 0.5 * (OriginalActorLocalBounds.Max - OriginalActorLocalBounds.Min));

	for (int DataIndex = 0; DataIndex < PinDesc->DataDescs.Num(); ++DataIndex)
	{
		const FPCGDataDesc& DataDesc = PinDesc->DataDescs[DataIndex];

		FPCGTextureBindingInfo BindingInfo(DataDesc, Transform);
		int32 BindingIndex = BindingInfos.Find(BindingInfo);

		if (BindingIndex == INDEX_NONE)
		{
			if (BindingInfos.Num() < PCGTextureDataInterfaceConstants::MAX_NUM_UAV_BINDINGS)
			{
				BindingIndex = BindingInfos.Add(MoveTemp(BindingInfo));
			}
			else
			{
				UE_LOG(LogPCG, Warning, TEXT("Texture data interface on pin '%s' received too many texture UAVs to bind. Only the first %d texture UAVs will be bound."),
					*OutputPinLabel.ToString(),
					PCGTextureDataInterfaceConstants::MAX_NUM_UAV_BINDINGS);
				continue;
			}
		}

		FPCGTextureInfo& TextureInfo = TextureInfos.Emplace_GetRef();
		TextureInfo.BindingIndex = BindingIndex;
		TextureInfo.SliceIndex = 0;
	}
}

FPCGTextureDataProviderProxy::FPCGTextureDataProviderProxy(TWeakObjectPtr<UPCGTextureDataProvider> InDataProvider)
{
	check(IsInGameThread());
	check(InDataProvider.IsValid());

	BindingInfos = InDataProvider->GetBindingInfos();
	TextureInfos = InDataProvider->GetTextureInfos();
	ExportMode = InDataProvider->GetExportMode();
	OutputPinLabel = InDataProvider->OutputPinLabel;
	OutputPinLabelAlias = InDataProvider->OutputPinLabelAlias;
	OriginatingGenerationCount = InDataProvider->GenerationCounter;
	DataProviderWeakPtr_GT = InDataProvider;

	if (InDataProvider->GetPinDescription())
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(CopyDataDescription);

		// @todo_pcg: Avoid costly copy of data description.
		PinDesc = *InDataProvider->GetPinDescription();
	}
}

bool FPCGTextureDataProviderProxy::IsValid(FValidationData const& InValidationData) const
{
	for (const FPCGTextureBindingInfo& BindingInfo : BindingInfos)
	{
		if (BindingInfo.ResourceType == EPCGTextureResourceType::Invalid)
		{
			UE_LOG(LogPCG, Warning, TEXT("FPCGTextureDataProviderProxy invalid due to invalid resource type."));
			return false;
		}
	}

	for (const FPCGTextureInfo& TextureInfo : TextureInfos)
	{
		if (!BindingInfos.IsValidIndex(TextureInfo.BindingIndex) || TextureInfo.SliceIndex < 0)
		{
			UE_LOG(LogPCG, Warning, TEXT("FPCGTextureDataProviderProxy invalid due to invalid binding indices."));
			return false;
		}
	}

	return InValidationData.ParameterStructSize == sizeof(FParameters);
}

struct FPCGTextureDataInterfacePermutationIds
{
	uint32 EnableMultipleTextureObjects = 0;
 
	FPCGTextureDataInterfacePermutationIds(const FComputeKernelPermutationVector& PermutationVector)
	{
		static FString Name(PCGTextureDataInterfaceConstants::EnableMultipleTextureObjectsPermutationName);
		static uint32 Hash = GetTypeHash(Name);
		EnableMultipleTextureObjects = PermutationVector.GetPermutationBits(Name, Hash, /*Value=*/1);
	}
};

void FPCGTextureDataProviderProxy::GatherPermutations(FPermutationData& InOutPermutationData) const
{
	FPCGTextureDataInterfacePermutationIds PermutationIds(InOutPermutationData.PermutationVector);
	for (int32 InvocationIndex = 0; InvocationIndex < InOutPermutationData.NumInvocations; ++InvocationIndex)
	{
		InOutPermutationData.PermutationIds[InvocationIndex] |= (BindingInfos.Num() > 1 ? PermutationIds.EnableMultipleTextureObjects : 0);
	}
}

void FPCGTextureDataProviderProxy::GatherDispatchData(FDispatchData const& InDispatchData)
{
	const TStridedView<FParameters> ParameterArray = MakeStridedParameterView<FParameters>(InDispatchData);
	for (int32 InvocationIndex = 0; InvocationIndex < ParameterArray.Num(); ++InvocationIndex)
	{
		FParameters& Parameters = ParameterArray[InvocationIndex];

		Parameters.Sampler_Linear = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		Parameters.Sampler_Point = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		Parameters.NumTextureInfos = TextureInfos.Num();
		Parameters.TextureInfos = TextureInfosBufferSRV;

		for (int BindingIndex = 0; BindingIndex < PCGTextureDataInterfaceConstants::MAX_NUM_SRV_BINDINGS; ++BindingIndex)
		{
			Parameters.Texture[BindingIndex] = TextureSRV[BindingIndex];
			Parameters.TextureArray[BindingIndex] = TextureArraySRV[BindingIndex];

			if (BindingIndex < BindingInfos.Num())
			{
				Parameters.TextureBounds[BindingIndex] = FVector4f(
					BindingInfos[BindingIndex].TextureBounds.Min.X,
					BindingInfos[BindingIndex].TextureBounds.Min.Y,
					BindingInfos[BindingIndex].TextureBounds.Max.X,
					BindingInfos[BindingIndex].TextureBounds.Max.Y);
			}
			else
			{
				Parameters.TextureBounds[BindingIndex] = FVector4f::Zero();
			}
		}

		for (int BindingIndex = 0; BindingIndex < PCGTextureDataInterfaceConstants::MAX_NUM_UAV_BINDINGS; ++BindingIndex)
		{
			Parameters.TextureUAV[BindingIndex] = TextureUAV[BindingIndex];
		}
	}
}

void FPCGTextureDataProviderProxy::AllocateResources(FRDGBuilder& GraphBuilder, FAllocationData const& InAllocationData)
{
	CreateDefaultTextures(GraphBuilder);
	CreateTextures(GraphBuilder);
	PackTextureInfos(GraphBuilder);
}

void FPCGTextureDataProviderProxy::CreateDefaultTextures(FRDGBuilder& GraphBuilder)
{
	FRDGTextureRef DummyTextureForSRV = GSystemTextures.GetDefaultTexture2D(GraphBuilder, EPixelFormat::PF_G8, 0.0f);
	FRDGTextureRef DummyTextureArrayForSRV = GSystemTextures.GetDefaultTexture(GraphBuilder, ETextureDimension::Texture2DArray, EPixelFormat::PF_G8, 0.0f);

	FRDGTextureRef DummyTextureForUAV = GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create2D(FIntPoint(1, 1), PF_G8, FClearValueBinding::Black, ETextureCreateFlags::ShaderResource | ETextureCreateFlags::UAV),
			TEXT("PCGTextureDI_DummyTextureForUAV"));

	FRDGTextureRef DummyTextureArrayForUAV = GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create2DArray(FIntPoint(1, 1), PF_G8, FClearValueBinding::Black, ETextureCreateFlags::ShaderResource | ETextureCreateFlags::UAV, /*ArraySize=*/1),
			TEXT("PCGTextureDI_DummyTextureArrayForUAV"));

	for (int BindingIndex = 0; BindingIndex < PCGTextureDataInterfaceConstants::MAX_NUM_SRV_BINDINGS; ++BindingIndex)
	{
		TextureSRV[BindingIndex] = GraphBuilder.CreateSRV(DummyTextureForSRV);
		TextureArraySRV[BindingIndex] = GraphBuilder.CreateSRV(DummyTextureArrayForSRV);
	}

	for (int BindingIndex = 0; BindingIndex < PCGTextureDataInterfaceConstants::MAX_NUM_UAV_BINDINGS; ++BindingIndex)
	{
		TextureUAV[BindingIndex] = GraphBuilder.CreateUAV(DummyTextureForUAV);
	}
}

void FPCGTextureDataProviderProxy::CreateTextures(FRDGBuilder& GraphBuilder)
{
	FRDGTextureRef ExportableTextures[PCGTextureDataInterfaceConstants::MAX_NUM_UAV_BINDINGS] = {};

	for (int BindingIndex = 0; BindingIndex < BindingInfos.Num(); ++BindingIndex)
	{
		FRDGTextureRef Texture = nullptr;
		bool bCanCreateUAV = false;

		if (BindingInfos[BindingIndex].ResourceType == EPCGTextureResourceType::TextureObject && BindingInfos[BindingIndex].Texture)
		{
			Texture = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(BindingInfos[BindingIndex].Texture, TEXT("PCGTextureDI_RenderTarget")));
		}
		else if (BindingInfos[BindingIndex].ResourceType == EPCGTextureResourceType::ExportedTexture && BindingInfos[BindingIndex].ExportedTexture)
		{
			Texture = GraphBuilder.RegisterExternalTexture(BindingInfos[BindingIndex].ExportedTexture);
		}
		else if (BindingIndex < PCGTextureDataInterfaceConstants::MAX_NUM_UAV_BINDINGS)
		{
			// @todo_pcg: Expose options like size, format, dimensions (e.g. Tex2D vs TexArray), etc.
			Texture = GraphBuilder.CreateTexture(
				FRDGTextureDesc::Create2D(
					BindingInfos[BindingIndex].Size,
					EPixelFormat::PF_FloatRGBA,
					FClearValueBinding::Black,
					ETextureCreateFlags::ShaderResource | ETextureCreateFlags::UAV),
				TEXT("PCGTextureDI_UAV"));

			bCanCreateUAV = true;
		}

		if (!Texture)
		{
			continue;
		}

		ExportableTextures[BindingIndex] = Texture;

		if (BindingInfos[BindingIndex].Dimension == ETextureDimension::Texture2D)
		{
			TextureSRV[BindingIndex] = GraphBuilder.CreateSRV(Texture);

			if (bCanCreateUAV)
			{
				TextureUAV[BindingIndex] = GraphBuilder.CreateUAV(Texture);
			}
		}
		else if (BindingInfos[BindingIndex].Dimension == ETextureDimension::Texture2DArray)
		{
			TextureArraySRV[BindingIndex] = GraphBuilder.CreateSRV(Texture);
		}
		else
		{
			checkNoEntry();
		}
	}

	if (ExportMode != EPCGExportMode::NoExport)
	{
		TArray<TRefCountPtr<IPooledRenderTarget>> ExportedTextures;

		for (const FPCGTextureInfo& TextureInfo : TextureInfos)
		{
			if (ensure(TextureInfo.BindingIndex >= 0 && TextureInfo.BindingIndex < PCGTextureDataInterfaceConstants::MAX_NUM_UAV_BINDINGS))
			{
				if (FRDGTextureRef ExportableTexture = ExportableTextures[TextureInfo.BindingIndex])
				{
					ExportedTextures.Add(GraphBuilder.ConvertToExternalTexture(ExportableTexture));
				}
			}
		}

		ExportTextureUAVs(ExportedTextures);
	}
}

void FPCGTextureDataProviderProxy::ExportTextureUAVs(const TArray<TRefCountPtr<IPooledRenderTarget>>& ExportedTextures)
{
	// Export textures and pass a reference back to the main thread where it will be picked up by the compute graph element.
	ExecuteOnGameThread(UE_SOURCE_LOCATION, [ExportedTextures=ExportedTextures, DataProviderWeakPtr=DataProviderWeakPtr_GT, PinDesc=PinDesc, OutputPinLabel=OutputPinLabel, ExportMode=ExportMode, OutputPinLabelAlias=OutputPinLabelAlias, GenerationCount=OriginatingGenerationCount]()
	{
		// Obtain objects. No ensures added because a graph cancellation could feasibly destroy some or all of these.
		UPCGTextureDataProvider* DataProvider = DataProviderWeakPtr.Get();
		if (!DataProvider)
		{
			UE_LOG(LogPCG, Error, TEXT("Could not resolve UPCGTextureDataProvider object to pass back buffer handle."));
			return;
		}

		if (DataProvider->GenerationCounter != GenerationCount)
		{
			return;
		}

		if (DataProvider->bInitializeFromDataCollection)
		{
			DataProvider->OnDataExported_GameThread().Broadcast();
			return;
		}

		if (UPCGDataBinding* Binding = DataProvider->GetDataBinding())
		{
			const TArray<FPCGTextureInfo>& TextureInfos_GT = DataProvider->GetTextureInfos();
			const TArray<FPCGTextureBindingInfo>& BindingInfos_GT = DataProvider->GetBindingInfos();
			const TArray<FString>& StringTable = Binding->GetStringTable();

			for (int TextureDataIndex = 0; TextureDataIndex < TextureInfos_GT.Num(); ++TextureDataIndex)
			{
				const FPCGTextureInfo& TextureInfo = TextureInfos_GT[TextureDataIndex];
				check(ExportedTextures.IsValidIndex(TextureDataIndex));
				check(PinDesc.DataDescs.IsValidIndex(TextureDataIndex))
				check(BindingInfos_GT.IsValidIndex(TextureInfo.BindingIndex));

				const FPCGTextureBindingInfo& BindingInfo = BindingInfos_GT[TextureInfo.BindingIndex];
				const FTransform TextureTransform = BindingInfos_GT[TextureInfo.BindingIndex].Transform;
				const FVector::FReal XSize = 2.0 * TextureTransform.GetScale3D().X;
				const FVector::FReal YSize = 2.0 * TextureTransform.GetScale3D().Y;
				const FVector2D TexelSize = FVector2D(XSize, YSize) / BindingInfo.Size;

				UPCGTextureData* ExportedData = NewObject<UPCGTextureData>();
				ExportedData->TexelSize = TexelSize.GetMin();
				ExportedData->Initialize(ExportedTextures[TextureDataIndex], TextureInfo.SliceIndex, TextureTransform, /*bInSkipReadbackToCPU=*/true);

				TSet<FString> Tags;
				for (const int32 TagStringKey : PinDesc.DataDescs[TextureDataIndex].TagStringKeys)
				{
					if (ensure(StringTable.IsValidIndex(TagStringKey)))
					{
						Tags.Add(StringTable[TagStringKey]);
					}
				}

				// @todo_pcg: Binding is doing a lot of work. Could store a context handle in the data provider instead?
				Binding->ReceiveDataFromGPU_GameThread(ExportedData, DataProvider->GetProducerSettings(), ExportMode, OutputPinLabel, OutputPinLabelAlias, Tags);
			}
		}

		DataProvider->OnDataExported_GameThread().Broadcast();
	});
}

void FPCGTextureDataProviderProxy::PackTextureInfos(FRDGBuilder& GraphBuilder)
{
	// Layout:
	// - TextureInfo0
	//   - uint32 - BindingIndex
	//   - uint32 - SliceIndex (aka TextureIndex if bound to a TextureArray, 0 otherwise.)
	//   - uint32 - Dimension
	//   - uint32 - Sampler
	//   - uint32 - SizeX
	//   - uint32 - SizeY
	// - TextureInfo1
	// - TextureInfo2
	// - ...

	if (!TextureInfos.IsEmpty())
	{
		const FRDGBufferDesc TextureInfosDesc = FRDGBufferDesc::CreateStructuredDesc(sizeof(FUintVector4), TextureInfos.Num() * 2);

		FRDGBufferRef TextureInfosBuffer = GraphBuilder.CreateBuffer(TextureInfosDesc, TEXT("PCGTextureDI_InfosBuffer"));
		TextureInfosBufferSRV = GraphBuilder.CreateSRV(TextureInfosBuffer);

		TArray<FUintVector4> TextureInfosPackedData;
		TextureInfosPackedData.SetNumUninitialized(TextureInfos.Num() * 2);

		for (int TextureInfoIndex = 0; TextureInfoIndex < TextureInfos.Num(); ++TextureInfoIndex)
		{
			const FPCGTextureInfo& TextureInfo = TextureInfos[TextureInfoIndex];
			const FPCGTextureBindingInfo& BindingInfo = BindingInfos[TextureInfo.BindingIndex];
			TextureInfosPackedData[TextureInfoIndex * 2 + 0] = FUintVector4(TextureInfo.BindingIndex, TextureInfo.SliceIndex, static_cast<uint32>(BindingInfo.Dimension), BindingInfo.bPointSample);
			TextureInfosPackedData[TextureInfoIndex * 2 + 1] = FUintVector4(BindingInfo.Size.X, BindingInfo.Size.Y, 0u, 0u);
		}

		GraphBuilder.QueueBufferUpload(TextureInfosBuffer, MakeArrayView(TextureInfosPackedData));
	}
	else
	{
		TextureInfosBufferSRV = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(GSystemTextures.GetDefaultStructuredBuffer(GraphBuilder, sizeof(FUintVector4))));
	}
}

#undef LOCTEXT_NAMESPACE
