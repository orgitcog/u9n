// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGGenerateGrassMaps.h"

#include "PCGComponent.h"
#include "PCGGrassMapUnpackerCS.h"
#include "PCGModule.h"
#include "PCGSubsystem.h"
#include "Compute/PCGComputeCommon.h"
#include "Data/PCGLandscapeData.h"
#include "Data/PCGTextureData.h"
#include "Helpers/PCGHelpers.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"

#include "DrawDebugHelpers.h"
#include "EngineDefines.h" // For UE_ENABLE_DEBUG_DRAWING
#include "Landscape.h"
#include "LandscapeComponent.h"
#include "LandscapeGrassType.h"
#include "LandscapeGrassWeightExporter.h"
#include "LandscapeProxy.h"
#include "RenderCaptureInterface.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGGenerateGrassMaps)

#define LOCTEXT_NAMESPACE "PCGGenerateGrassMapsElement"

namespace PCGGenerateGrassMaps
{
	const FName InputPinLabel = TEXT("Landscape");
	const FName GrassTypeOverridesPinLabel = TEXT("Grass Types");

	const FText LandscapeComponentLostError = LOCTEXT("LandscapeComponentLost", "Reference to one or more landscape components lost, grass maps will not be generated.");

	static int32 GTriggerGPUCaptureDispatches = 0;
	static FAutoConsoleVariableRef CVarTriggerGPUCapture(
		TEXT("pcg.GPU.TriggerRenderCaptures.GrassMapGeneration"),
		GTriggerGPUCaptureDispatches,
		TEXT("Trigger GPU captures for this many of the subsequent grass generations."));

#if WITH_EDITOR
	TAutoConsoleVariable<bool> CVarDebugDrawGeneratedComponents(
		TEXT("pcg.Grass.DebugDrawGeneratedComponents"),
		false,
		TEXT("Draws debug boxes around landscapes for which grass maps are generated, colored by the current task ID."));
#endif

	static bool IsTextureFullyStreamedIn(UTexture* InTexture)
	{
		const bool bCheckForLODTransition = true;
		return InTexture &&
#if WITH_EDITOR
			!InTexture->IsDefaultTexture() &&
#endif // WITH_EDITOR
			!InTexture->HasPendingInitOrStreaming(bCheckForLODTransition) && InTexture->IsFullyStreamedIn();
	}
}

TArray<FPCGPinProperties> UPCGGenerateGrassMapsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;

	// Single landscape only for now. In the future we should iterate over all landscapes and generate grass maps for each.
	FPCGPinProperties& PinProp = PinProperties.Emplace_GetRef(
		PCGGenerateGrassMaps::InputPinLabel,
		EPCGDataType::Landscape,
		/*bAllowMultipleConnections=*/true,
		/*bInAllowMultipleData=*/false);
	PinProp.SetRequiredPin();

	if (bOverrideFromInput)
	{
		FPCGPinProperties& GrassTypeOverrides = PinProperties.Emplace_GetRef(PCGGenerateGrassMaps::GrassTypeOverridesPinLabel, EPCGDataType::Param);
		GrassTypeOverrides.SetRequiredPin();
	}

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGGenerateGrassMapsSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;

	FPCGPinProperties& PinProp = PinProperties.Emplace_GetRef(
		PCGPinConstants::DefaultOutputLabel,
		EPCGDataType::Texture,
		/*bAllowMultipleConnections=*/true,
		/*bInAllowMultipleData=*/false);
	PinProp.SetRequiredPin();

	return PinProperties;
}

FPCGElementPtr UPCGGenerateGrassMapsSettings::CreateElement() const
{
	return MakeShared<FPCGGenerateGrassMapsElement>();
}

#if WITH_EDITOR
EPCGChangeType UPCGGenerateGrassMapsSettings::GetChangeTypeForProperty(const FName& InPropertyName) const
{
	EPCGChangeType ChangeType = Super::GetChangeTypeForProperty(InPropertyName);

	if (InPropertyName == GET_MEMBER_NAME_CHECKED(UPCGGenerateGrassMapsSettings, bOverrideFromInput))
	{
		ChangeType |= EPCGChangeType::Structural;
	}

	return ChangeType;
}
#endif // WITH_EDITOR

FPCGGenerateGrassMapsContext::~FPCGGenerateGrassMapsContext()
{
	delete LandscapeGrassWeightExporter;
}

void FPCGGenerateGrassMapsContext::AddExtraStructReferencedObjects(FReferenceCollector& Collector)
{
	for (TObjectPtr<UPCGTextureData>& Data : TextureDatas)
	{
		if (Data)
		{
			Collector.AddReferencedObject(Data);
		}
	}

	for (TObjectPtr<UTexture>& Texture : TexturesToStream)
	{
		if (Texture)
		{
			Collector.AddReferencedObject(Texture);
		}
	}
}

void FPCGGenerateGrassMapsElement::GetDependenciesCrc(const FPCGGetDependenciesCrcParams& InParams, FPCGCrc& OutCrc) const
{
	FPCGCrc Crc;
	IPCGElement::GetDependenciesCrc(InParams, Crc);

	// todo_pcg: Technically this could be fancier and hash the set of landscape components that overlap with the generation volume.
	if (const UPCGData* Data = InParams.ExecutionSource ? InParams.ExecutionSource->GetExecutionState().GetSelfData() : nullptr)
	{
		Crc.Combine(Data->GetOrComputeCrc(/*bFullDataCrc=*/false));
	}

	OutCrc = Crc;
}

FPCGContext* FPCGGenerateGrassMapsElement::CreateContext()
{
	return new FPCGGenerateGrassMapsContext();
}

bool FPCGGenerateGrassMapsElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGGenerateGrassMapsElement::ExecuteInternal);

	FPCGGenerateGrassMapsContext* Context = static_cast<FPCGGenerateGrassMapsContext*>(InContext);
	check(Context);

	const UPCGGenerateGrassMapsSettings* Settings = Context->GetInputSettings<UPCGGenerateGrassMapsSettings>();
	check(Settings);

	IPCGGraphExecutionSource* ExecutionSource = Context->ExecutionSource.Get();
	if (!ExecutionSource)
	{
		return true;
	}

	// 1. Select landscape components that overlap the given bounds.
	if (!Context->bLandscapeComponentsFiltered)
	{
		const UPCGLandscapeData* LandscapeData = nullptr;
		for (const FPCGTaggedData& Data : Context->InputData.TaggedData)
		{
			if (const UPCGLandscapeData* InputLandscapeData = Cast<UPCGLandscapeData>(Data.Data))
			{
				if (!LandscapeData)
				{
					LandscapeData = InputLandscapeData;
				}
				else
				{
					PCGLog::LogWarningOnGraph(LOCTEXT("MultipleLandscapesNotSpported", "Multiple landscape data inputs not currently supported, only first will be used."), Context);
				}
			}
		}

		if (!LandscapeData)
		{
			// No input landscape, done.
			return true;
		}

		if (!UE::Landscape::IsRuntimeGrassMapGenerationSupported() && (!ExecutionSource->GetExecutionState().GetWorld() || ExecutionSource->GetExecutionState().GetWorld()->IsGameWorld()))
		{
			UE_LOG(LogPCG, Warning, TEXT("Grass map generation is disabled outside of editor worlds. Try enabling the CVar 'grass.GrassMap.AlwaysBuildRuntimeGenerationResources'."));
			return true;
		}

		TArray<FString> SelectedGrassTypes;

		if (!Settings->bOverrideFromInput)
		{
			SelectedGrassTypes = Settings->SelectedGrassTypes;
		}
		else
		{
			const TArray<FPCGTaggedData> OverrideTaggedDatas = InContext->InputData.GetInputsByPin(PCGGenerateGrassMaps::GrassTypeOverridesPinLabel);
			TArray<FString> GrassTypeOverrides;

			for (const FPCGTaggedData& OverrideTaggedData : OverrideTaggedDatas)
			{
				if (const UPCGData* OverrideData = OverrideTaggedData.Data)
				{
					const FPCGAttributePropertyInputSelector Selector = Settings->GrassTypesAttribute.CopyAndFixLast(OverrideData);

					if (PCGAttributeAccessorHelpers::ExtractAllValues(OverrideData, Selector, GrassTypeOverrides, InContext))
					{
						SelectedGrassTypes = std::move(GrassTypeOverrides);
					}
					else
					{
						PCGLog::LogWarningOnGraph(LOCTEXT("FailExtractGrassTypeOverrides", "Failed to extract grass type overrides."), InContext);
					}
				}
			}
		}

		const FBox GenerationBounds = ExecutionSource->GetExecutionState().GetBounds();

		for (TSoftObjectPtr<ALandscapeProxy> LandscapeProxyPtr : LandscapeData->Landscapes)
		{
			ALandscapeProxy* LandscapeProxy = LandscapeProxyPtr.Get();

			if (!LandscapeProxy || !LandscapeProxy->LandscapeMaterial)
			{
				continue;
			}

			if (!Context->LandscapeProxy.Get())
			{
				Context->LandscapeProxy = LandscapeProxy;
			}
			// TODO: In order to support multiple-material we need to do a dispatch of one landscape grass exporter per LS proxy.
			else if (Context->LandscapeProxy->LandscapeMaterial != LandscapeProxy->LandscapeMaterial)
			{
				PCGLog::LogErrorOnGraph(LOCTEXT("LandscapeMaterialMismatch", "Encountered multiple landscape materials on the same landscape. Only one landscape material per landscape is supported at this time."), Context);
				return true;
			}

			for (ULandscapeComponent* LandscapeComponent : LandscapeProxy->LandscapeComponents)
			{
				if (!ensure(LandscapeComponent))
				{
					continue;
				}

				// Only generate grass map if there is meaningful overlap with our domain of interest.
				const FBox LandscapeComponentBounds = LandscapeComponent->Bounds.GetBox();
				if (LandscapeComponentBounds.Overlap(GenerationBounds).GetVolume() > UE_KINDA_SMALL_NUMBER)
				{
					if (Context->LandscapeComponents.Num() >= FPCGGrassMapUnpackerCS::MaxNumLandscapeComponents)
					{
						PCGLog::LogWarningOnGraph(LOCTEXT("MaxLandscapeComponentsExceeded", "Too many landscape components overlap the generation domain. Consider partitioning the component onto a smaller grid size."), Context);
						break;
					}

					if (LandscapeComponent->OverrideMaterial && Context->LandscapeProxy->LandscapeMaterial->GetBaseMaterial() != LandscapeComponent->OverrideMaterial->GetBaseMaterial())
					{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
						PCGLog::LogErrorOnGraph(
							FText::Format(LOCTEXT("LandscapeOverrideMaterialMismatch",
								"Encountered landscape component {0} with override material {1} that did not match the landscape proxy material {2}. Only one landscape material per landscape is supported at this time."),
								FText::FromString(LandscapeComponent->GetName()),
								FText::FromString(LandscapeComponent->OverrideMaterial->GetName()),
								FText::FromString(Context->LandscapeProxy->LandscapeMaterial->GetName())),
							Context);
#endif

						return true;
					}

					LandscapeComponent->UpdateGrassTypes();

					Context->LandscapeComponents.Add(LandscapeComponent);

					if (Context->LandscapeComponents.Num() == 1)
					{
						const TArray<TObjectPtr<ULandscapeGrassType>>& GrassTypes = LandscapeComponent->GetGrassTypes();
						Context->NumGrassTypes = GrassTypes.Num();

						FString GrassTypeName;
						GrassTypeName.Reserve(256);

						for (int GrassTypeIndex = 0; GrassTypeIndex < GrassTypes.Num(); ++GrassTypeIndex)
						{
							if (TObjectPtr<ULandscapeGrassType> GrassType = GrassTypes[GrassTypeIndex])
							{
								GrassType->GetName(GrassTypeName);

								const bool bIsSelectedLayer = SelectedGrassTypes.Contains(GrassTypeName);

								if (bIsSelectedLayer != Settings->bExcludeSelectedGrassTypes)
								{
									Context->SelectedGrassTypes.Emplace(GrassType, GrassTypeIndex);
								}
							}
						}

						Context->GrassMapBounds = LandscapeComponentBounds;

						Context->LandscapeComponentExtent = LandscapeComponent->Bounds.BoxExtent.X * 2.0;

						if (!ensure(FMath::IsNearlyEqual(LandscapeComponent->Bounds.BoxExtent.X, LandscapeComponent->Bounds.BoxExtent.Y)))
						{
							return true;
						}

						if (!ensure(Context->LandscapeComponentExtent > 0.0))
						{
							return true;
						}
					}
					else
					{
						Context->GrassMapBounds += LandscapeComponentBounds;

						// We expect all landscape components for a single landscape to have the same grass types. Do weak validation here.
						const uint32 ExpectedNumGrassTypes = Context->NumGrassTypes;
						const uint32 ActualNumGrassTypes = LandscapeComponent->GetGrassTypes().Num();
						ensure(ExpectedNumGrassTypes == ActualNumGrassTypes);

						// Currently assuming all landscape have the same extents.
						ensure(FMath::IsNearlyEqual(Context->LandscapeComponentExtent, LandscapeComponent->Bounds.BoxExtent.X * 2.0));
					}

#if WITH_EDITOR
					if (PCGGenerateGrassMaps::CVarDebugDrawGeneratedComponents.GetValueOnGameThread())
					{
						DrawDebugBox(ExecutionSource->GetExecutionState().GetWorld(),
							LandscapeComponentBounds.GetCenter(),
							LandscapeComponentBounds.GetExtent(),
							FColor::MakeRandomSeededColor(Context->TaskId),
							/*bPersistentLines=*/false,
							/*LifeTime=*/3.0f);
					}
#endif
				}
			}
		}

		if (!Context->LandscapeProxy.Get() || Context->LandscapeComponents.IsEmpty() || Context->SelectedGrassTypes.IsEmpty())
		{
			return true;
		}

		Context->bLandscapeComponentsFiltered = true;
	}

	// 2. Wait for landscape components to be ready for grass map rendering.
	if (!Context->bTextureStreamingRequested)
	{
		if (const UWorld* World = ExecutionSource->GetExecutionState().GetWorld())
		{
			for (TWeakObjectPtr<ULandscapeComponent> LandscapeComponentWeak : Context->LandscapeComponents)
			{
				if (ULandscapeComponent* LandscapeComponent = LandscapeComponentWeak.Get())
				{
					// Make list of textures to stream before generating.
					if (UTexture* HeightMap = LandscapeComponent->GetHeightmap())
					{
						Context->TexturesToStream.Add(HeightMap);
					}

					const ERHIFeatureLevel::Type FeatureLevel = World->GetFeatureLevel();
					for (UTexture2D* WeightmapTexture : LandscapeComponent->GetRenderedWeightmapTexturesForFeatureLevel(FeatureLevel))
					{
						if (WeightmapTexture)
						{
							Context->TexturesToStream.Add(WeightmapTexture);
						}
					}
				}
			}

			for (UTexture* Texture : Context->TexturesToStream)
			{
				Texture->bForceMiplevelsToBeResident = true;
			}
		}

		Context->bTextureStreamingRequested = true;
	}

	if (!Context->bReadyToRender)
	{
		bool bAllReady = true;

		for (TWeakObjectPtr<ULandscapeComponent> LandscapeComponentWeak : Context->LandscapeComponents)
		{
			ULandscapeComponent* LandscapeComponent = LandscapeComponentWeak.Get();
			if (!LandscapeComponent)
			{
				PCGLog::LogErrorOnGraph(PCGGenerateGrassMaps::LandscapeComponentLostError, InContext);
				return true;
			}

			bAllReady &= UE::Landscape::CanRenderGrassMap(LandscapeComponent);
		}

		if (bAllReady)
		{
			for (UTexture* Texture : Context->TexturesToStream)
			{
				const bool bStreamedIn = PCGGenerateGrassMaps::IsTextureFullyStreamedIn(Texture);
				bAllReady &= bStreamedIn;

				if (!bStreamedIn)
				{
					UE_LOG(LogPCG, Verbose, TEXT("Waiting for landscape texture '%s' to stream in."), *Texture->GetName());
					break;
				}
			}
		}
		
		if (!bAllReady)
		{
#if UE_ENABLE_DEBUG_DRAWING
			if (PCGSystemSwitches::CVarPCGDebugDrawGeneratedCells.GetValueOnGameThread())
			{
				FColor DebugColor = FColor::Yellow;
				PCGHelpers::DebugDrawGenerationVolume(InContext, &DebugColor);
			}
#endif

			// Sleep until next frame, no use spinning on this.
			Context->bIsPaused = true;
			FPCGModule::GetPCGModuleChecked().ExecuteNextTick([ContextHandle = InContext->GetOrCreateHandle()]()
			{
				if (TSharedPtr<FPCGContextHandle> SharedHandle = ContextHandle.Pin())
				{
					if (FPCGContext* ContextPtr = SharedHandle->GetContext())
					{
						ContextPtr->bIsPaused = false;
					}
				}
			});

			return false;
		}

		Context->bReadyToRender = true;
	}

	// 3. Schedule grass map generation.
	if (!Context->bGenerationScheduled)
	{
		ALandscapeProxy* LandscapeProxy = Context->LandscapeProxy.IsValid() ? Context->LandscapeProxy.Get() : nullptr;
		if (!LandscapeProxy)
		{
			PCGLog::LogErrorOnGraph(LOCTEXT("LandscapeProxyLost", "Reference to landscape proxy actor lost, grass maps will not be generated."), InContext);
			return true;
		}

		TArray<ULandscapeComponent*> LandscapeComponents;
		TArray<FIntVector2> LandscapeTileCoords;
		LandscapeComponents.Reserve(Context->LandscapeComponents.Num());
		LandscapeTileCoords.Reserve(Context->LandscapeComponents.Num());

		for (TWeakObjectPtr<ULandscapeComponent> LandscapeComponentPtr : Context->LandscapeComponents)
		{
			ULandscapeComponent* LandscapeComponent = LandscapeComponentPtr.IsValid() ? LandscapeComponentPtr.Get() : nullptr;

			if (!LandscapeComponent)
			{
				PCGLog::LogErrorOnGraph(PCGGenerateGrassMaps::LandscapeComponentLostError, InContext);
				return true;
			}

			LandscapeComponents.Add(LandscapeComponent);

			// Landscape components are not ordered, so store a 2d index of each component within the grass map.
			// TODO could likely sort the LandscapeComponents array so the order is known and no indices need looking up.
			LandscapeTileCoords.Add(FIntVector2(
				(LandscapeComponent->Bounds.Origin.X - Context->GrassMapBounds.Min.X) / Context->LandscapeComponentExtent,
				(LandscapeComponent->Bounds.Origin.Y - Context->GrassMapBounds.Min.Y) / Context->LandscapeComponentExtent));
		}

		Context->LandscapeGrassWeightExporter = new FLandscapeGrassWeightExporter(
			LandscapeProxy,
			LandscapeComponents,
			/*bInNeedsGrassmap=*/true,
			/*bInNeedsHeightmap=*/false,
			/*HeightMips=*/{},
			/*bExport=*/false,
			/*bShouldReadback=*/false);

		UE::RenderCommandPipe::FSyncScope SyncScope;

		RenderCaptureInterface::FScopedCapture RenderCapture(PCGGenerateGrassMaps::GTriggerGPUCaptureDispatches > 0, TEXT("PCGLandscapeGrassmapCapture"));
		PCGGenerateGrassMaps::GTriggerGPUCaptureDispatches = FMath::Max(PCGGenerateGrassMaps::GTriggerGPUCaptureDispatches - 1, 0);
		
		TWeakPtr<FPCGContextHandle> ContextHandle = Context->GetOrCreateHandle();

		ENQUEUE_RENDER_COMMAND(GenerateGrassMaps)([ContextHandle, LandscapeComponentExtent=Context->LandscapeComponentExtent, LandscapeTileCoords=MoveTemp(LandscapeTileCoords), ComponentSizeQuads=LandscapeProxy->ComponentSizeQuads](FRHICommandListImmediate& RHICmdList)
		{
			FPCGContext::FSharedContext<FPCGGenerateGrassMapsContext> SharedContext(ContextHandle);
			FPCGGenerateGrassMapsContext* Context = SharedContext.Get();
			if (!Context)
			{
				return;
			}

			FRDGBuilder GraphBuilder(RHICmdList);

			FRDGTextureDesc GrassMapTextureDesc = FRDGTextureDesc::Create2D(
				Context->LandscapeGrassWeightExporter->GetTargetSize(),
				PF_B8G8R8A8,
				FClearValueBinding(),
				ETextureCreateFlags::RenderTargetable | ETextureCreateFlags::ShaderResource);

			FRDGTextureRef GrassMapTexture = GraphBuilder.CreateTexture(GrassMapTextureDesc, TEXT("PCGLandscapeGrassMapRenderTarget"));
			FRDGTextureSRVRef GrassMapTextureSRV = GraphBuilder.CreateSRV(GrassMapTexture);

			// Generate grass maps. All will be generated to a single texture.
			Context->LandscapeGrassWeightExporter->RenderLandscapeComponentToTexture_RenderThread(GraphBuilder, GrassMapTexture);

			const FVector GrassMapExtent = Context->GrassMapBounds.GetExtent() * 2;
			const uint32 NumTilesX = FMath::RoundToInt(GrassMapExtent.X / LandscapeComponentExtent);
			const uint32 NumTilesY = FMath::RoundToInt(GrassMapExtent.Y / LandscapeComponentExtent);

			// Each corner of a quad in the landscape corresponds to one texel in the grass map.
			const uint32 LandscapeComponentResolution = ComponentSizeQuads + 1;
			const FIntPoint GrassMapResolution(LandscapeComponentResolution * NumTilesX, LandscapeComponentResolution * NumTilesY);
			const uint32 NumGrassTypes = Context->NumGrassTypes;

			// Output texture is array of textures, one per grass map.
			FRDGTextureDesc GrassMapDesc = FRDGTextureDesc::Create2DArray(
				GrassMapResolution,
				PF_G8,
				FClearValueBinding(),
				ETextureCreateFlags::ShaderResource | ETextureCreateFlags::UAV,
				NumGrassTypes);

			FRDGTextureRef GrassMap = GraphBuilder.CreateTexture(GrassMapDesc, TEXT("PCGLandscapeGrassMapUnpacked"));
			FRDGTextureUAVRef GrassMapUAV = GraphBuilder.CreateUAV(GrassMap);

			// Export the output texture so it can be used downstream.
			TRefCountPtr<IPooledRenderTarget> GrassMapExported = GraphBuilder.ConvertToExternalTexture(GrassMap);
			GraphBuilder.SetTextureAccessFinal(GrassMap, ERHIAccess::SRVCompute);

			// Unpack the generated results to simple world aligned textures.
			FPCGGrassMapUnpackerCS::FParameters* Parameters = GraphBuilder.AllocParameters<FPCGGrassMapUnpackerCS::FParameters>();
			Parameters->InPackedGrassMaps = GrassMapTextureSRV;
			Parameters->OutUnpackedGrassMaps = GrassMapUAV;
			Parameters->InNumTilesX = NumTilesX;
			Parameters->InOutputResolution = FUintVector2(GrassMapResolution.X, GrassMapResolution.Y);
			Parameters->InLandscapeComponentResolution = LandscapeComponentResolution;

			// The first 2 channels are reserved for height data. See illustration of packing in PCGGrassMapUnpackerCS.usf.
			Parameters->InNumGrassMapPasses = FMath::DivideAndRoundUp<int>(NumGrassTypes + 2, 4);

			// Initialize to invalid component indices.
			for (int Index = 0; Index < FPCGGrassMapUnpackerCS::MaxNumLandscapeComponents; ++Index)
			{
				Parameters->InLinearTileIndexToComponentIndex[Index].X = -1;
			}

			// Now write component mapping.
			for (int Index = 0; Index < LandscapeTileCoords.Num(); ++Index)
			{
				Parameters->InLinearTileIndexToComponentIndex[LandscapeTileCoords[Index].Y * NumTilesX + LandscapeTileCoords[Index].X].X = Index;
			}

			TShaderMapRef<FPCGGrassMapUnpackerCS> Shader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
			const int GroupCountX = FMath::DivideAndRoundUp<int>(GrassMapResolution.X, FPCGGrassMapUnpackerCS::ThreadGroupDim);
			const int GroupCountY = FMath::DivideAndRoundUp<int>(GrassMapResolution.Y, FPCGGrassMapUnpackerCS::ThreadGroupDim);
			const int GroupCountZ = NumGrassTypes;

			FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("PCGUnpackGrassMap"), ERDGPassFlags::Compute, Shader, Parameters, FIntVector(GroupCountX, GroupCountY, GroupCountZ));

			GraphBuilder.Execute();

			// Pass exported buffer back to game thread and wake up this element.
			ExecuteOnGameThread(UE_SOURCE_LOCATION, [ContextHandle, GrassMapExported]()
			{
				FPCGContext::FSharedContext<FPCGGenerateGrassMapsContext> SharedContext(ContextHandle);
				if (FPCGGenerateGrassMapsContext* Context = SharedContext.Get())
				{
					Context->GrassMapHandle = GrassMapExported;
					Context->bIsPaused = false;
				}
			});
		});

		Context->bGenerationScheduled = true;

		// Render command will wake this task up after completing.
		Context->bIsPaused = true;

		return false;
	}

	// 4. Initialize texture data objects.

	if (Context->TextureDatas.IsEmpty())
	{
		// Create the texture data objects if they haven't been created already. There should be one per selected grass type.
		for (int DataIndex = 0; DataIndex < Context->SelectedGrassTypes.Num(); ++DataIndex)
		{
			UPCGTextureData* TextureData = FPCGContext::NewObject_AnyThread<UPCGTextureData>(Context);
			Context->TextureDatas.Add(TextureData);
		}
	}

	const FTransform GrassMapTransform(FQuat::Identity, Context->GrassMapBounds.GetCenter(), Context->GrassMapBounds.GetExtent());
	bool bAllTexturesInitialized = true;

	if (!Context->GrassMapHandle.IsValid())
	{
		return true;
	}

	for (int DataIndex = 0; DataIndex < Context->SelectedGrassTypes.Num(); ++DataIndex)
	{
		check(Context->TextureDatas.IsValidIndex(DataIndex));
		UPCGTextureData* TextureData = Context->TextureDatas[DataIndex];
		check(TextureData);

		// Poll initialize (fine to be called even when initialization was already complete).
		bAllTexturesInitialized &= TextureData->Initialize(Context->GrassMapHandle, /*TextureIndex=*/Context->SelectedGrassTypes[DataIndex].Get<1>(), GrassMapTransform, Settings->bSkipReadbackToCPU);
	}

	if (!bAllTexturesInitialized)
	{
		// Initialization not complete. Could be waiting on async texture processing or for GPU readback. Sleep until next frame.
		// TODO: Ideally we do lazy readback on texture data in the future so we don't have to read it back to CPU unless it's needed.
		Context->bIsPaused = true;
		FPCGModule::GetPCGModuleChecked().ExecuteNextTick([ContextHandle = Context->GetOrCreateHandle()]()
		{
			FPCGContext::FSharedContext<FPCGGenerateGrassMapsContext> SharedContext(ContextHandle);
			if (FPCGGenerateGrassMapsContext* ContextPtr = SharedContext.Get())
			{
				ContextPtr->bIsPaused = false;
			}
		});

		return false;
	}

#if WITH_EDITOR
	if (!Context->TextureDatas.IsEmpty() && !Settings->bSkipReadbackToCPU)
	{
		if (!Settings->bSkipReadbackToCPU && Context->Node && Context->GetStack() && Context->ExecutionSource.IsValid())
		{
			Context->ExecutionSource->GetExecutionState().GetInspection().NotifyGPUToCPUReadback(Context->Node, Context->GetStack());
		}
	}
#endif // WITH_EDITOR

	// 5. Emit texture data objects.
	for (int DataIndex = 0; DataIndex < Context->SelectedGrassTypes.Num(); ++DataIndex)
	{
		check(Context->TextureDatas.IsValidIndex(DataIndex));
		UPCGTextureData* TextureData = Context->TextureDatas[DataIndex];
		check(TextureData);

		if (!TextureData->IsSuccessfullyInitialized())
		{
			PCGE_LOG(Warning, LogOnly, LOCTEXT("TextureInitFailed", "Data could not be retrieved for this texture, initialization failed."));
			continue;
		}

		FPCGTaggedData& OutTaggedData = Context->OutputData.TaggedData.Emplace_GetRef();
		OutTaggedData.Data = TextureData;
		
		FString GrassTypeName = TEXT("");

		if (ULandscapeGrassType* GrassType = Context->SelectedGrassTypes[DataIndex].Get<0>().Get())
		{
			GrassType->GetName(GrassTypeName);
		}

		if (!GrassTypeName.IsEmpty())
		{
			OutTaggedData.Tags.Add(PCGComputeHelpers::GetPrefixedDataLabel(GrassTypeName));
		}
		else
		{
			PCGE_LOG(Warning, LogOnly, LOCTEXT("MissingGrassName",
				"Grass type name was missing, data could not be labeled. Make sure all GrassTypes in your landscape material have an asset associated."));
		}
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
