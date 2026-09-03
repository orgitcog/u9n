// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/GenerateMutableSource/GenerateMutableSourceSurface.h"

#include "Engine/SkinnedAssetCommon.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureLODSettings.h"
#include "GPUSkinPublicDefs.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITextureFormat.h"
#include "Interfaces/ITextureFormatModule.h"
#include "Interfaces/ITextureFormatManagerModule.h"
#include "Modules/ModuleManager.h"
#include "TextureCompressorModule.h"
#include "Materials/MaterialInstance.h"
#include "GPUSkinVertexFactory.h"
#include "Rendering/SkeletalMeshLODModel.h"

#include "MuCO/CustomizableObjectInstance.h"
#include "MuCO/UnrealConversionUtils.h"
#include "MuCOE/CustomizableObjectCompiler.h"
#include "MuCOE/CustomizableObjectLayout.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSourceColor.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSourceFloat.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSourceGroupProjector.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSourceImage.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSourceMesh.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSourceTable.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSourceLayout.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSourceMacro.h"
#include "MuCOE/GraphTraversal.h"
#include "MuCOE/MutableUtils.h"
#include "MuCOE/Nodes/CustomizableObjectNodeCopyMaterial.h"
#include "MuCOE/Nodes/CustomizableObjectNodeFloatConstant.h"
#include "MuCOE/Nodes/CustomizableObjectNodeFloatParameter.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMacroInstance.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMaterialVariation.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMaterialSwitch.h"
#include "MuCOE/Nodes/CustomizableObjectNodeModifierEditMeshSection.h"
#include "MuCOE/Nodes/CustomizableObjectNodeModifierExtendMeshSection.h"
#include "MuCOE/Nodes/CustomizableObjectNodeModifierMorphMeshSection.h"
#include "MuCOE/Nodes/CustomizableObjectNodeModifierRemoveMesh.h"
#include "MuCOE/Nodes/CustomizableObjectNodeModifierRemoveMeshBlocks.h"
#include "MuCOE/Nodes/CustomizableObjectNodeSkeletalMesh.h"
#include "MuCOE/Nodes/CustomizableObjectNodeStaticString.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTable.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTunnel.h"
#include "MuT/NodeImageFormat.h"
#include "MuT/NodeImageMipmap.h"
#include "MuT/NodeImageNormalComposite.h"
#include "MuT/NodeImageResize.h"
#include "MuT/NodeImageSwizzle.h"
#include "MuT/NodeMeshConstant.h"
#include "MuT/NodeMeshFormat.h"
#include "MuT/NodeMeshFragment.h"
#include "MuT/NodeScalarConstant.h"
#include "MuT/NodeSurfaceSwitch.h"
#include "MuT/NodeSurfaceVariation.h"
#include "MuT/UnrealPixelFormatOverride.h"


#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


mu::Ptr<mu::NodeSurface> GenerateMutableSourceSurface(const UEdGraphPin * Pin, FMutableGraphGenerationContext & GenerationContext)
{
	MUTABLE_CPUPROFILER_SCOPE(GenerateMutableSourceSurface);

	check(Pin)
	RETURN_ON_CYCLE(*Pin, GenerationContext)

	CheckNumOutputs(*Pin, GenerationContext);

	UCustomizableObjectNode* Node = CastChecked<UCustomizableObjectNode>(Pin->GetOwningNode());

	// Bool that determines if a node can be added to the cache of nodes.
	// Most nodes need to be added to the cache but there are some that don't. For exampel, MacroInstanceNodes
	bool bCacheNode = true;

	const FGeneratedKey Key(reinterpret_cast<void*>(&GenerateMutableSourceSurface), *Pin, *Node, GenerationContext, true);
	if (const FGeneratedData* Generated = GenerationContext.Generated.Find(Key))
	{
		return static_cast<mu::NodeSurface*>(Generated->Node.get());
	}
	
	mu::Ptr<mu::NodeSurface> Result;

	const int32 LOD = Node->IsAffectedByLOD() ? GenerationContext.CurrentLOD : 0;
	
	if (UCustomizableObjectNode* CustomObjNode = Cast<UCustomizableObjectNode>(Node))
	{
		if (CustomObjNode->IsNodeOutDatedAndNeedsRefresh())
		{
			CustomObjNode->SetRefreshNodeWarning();
		}
	}

	if (UCustomizableObjectNodeMaterialBase* TypedNodeMat = Cast<UCustomizableObjectNodeMaterialBase>(Node))
	{
		bool bGeneratingImplicitComponent = GenerationContext.ComponentMeshOverride.get() != nullptr;

		const UEdGraphPin* ConnectedMaterialPin = FollowInputPin(*TypedNodeMat->GetMeshPin());
		// Warn when texture connections are improperly used by connecting them directly to material inputs when no layout is used
		// TODO: delete the if clause and the warning when static meshes are operational again
		if (ConnectedMaterialPin)
		{
			if (const UEdGraphPin* StaticMeshPin = FindMeshBaseSource(*ConnectedMaterialPin, true, &GenerationContext.MacroNodesStack))
			{
				const UCustomizableObjectNode* StaticMeshNode = CastChecked<UCustomizableObjectNode>(StaticMeshPin->GetOwningNode());
				GenerationContext.Log(LOCTEXT("UnsupportedStaticMeshes", "Static meshes are currently not supported as material meshes"), StaticMeshNode);
			}
		}

		UMaterialInterface* Material = TypedNodeMat->GetMaterial();
		if (!Material)
		{
			const FText Message = LOCTEXT("FailedToGenerateMeshSection", "Could not generate a mesh section because it didn't have a material selected. Please assign one and recompile.");
			GenerationContext.Log(Message, Node);
			Result = nullptr;

			return Result;
		}

		mu::Ptr<mu::NodeSurfaceNew> SurfNode = new mu::NodeSurfaceNew();
		Result = SurfNode;

		// Add to the list of surfaces that could be reused between LODs for this NodeMaterial.
		// TODO(Max)
		TArray<FMutableGraphGenerationContext::FSharedSurface>& SharedSurfaces = GenerationContext.SharedSurfaceIds.FindOrAdd(TypedNodeMat, {});
		FMutableGraphGenerationContext::FSharedSurface& SharedSurface = SharedSurfaces.Add_GetRef(FMutableGraphGenerationContext::FSharedSurface(GenerationContext.CurrentLOD, SurfNode, GenerationContext.MacroNodesStack));
		SharedSurface.bMakeUnique = !TypedNodeMat->IsReuseMaterialBetweenLODs();

		GenerationContext.CurrentReferencedMaterialIndex = GenerationContext.ReferencedMaterials.AddUnique(Material);

		// Find reference mesh used to generate the surface metadata for this fragment.
		FMutableSourceMeshData MeshData;
		if (ConnectedMaterialPin)
		{
			//NOTE: This is the same is done in GenerateMutableSourceSurface. 
			if (const UEdGraphPin* SkeletalMeshPin = FindMeshBaseSource(*ConnectedMaterialPin, false, &GenerationContext.MacroNodesStack))
			{
				int32 MetadataLODIndex, MetadataSectionIndex, MetadataLayoutIndex;
				MetadataLODIndex = MetadataSectionIndex = MetadataLayoutIndex = INDEX_NONE;

				if (const UCustomizableObjectNodeSkeletalMesh* SkeletalMeshNode = Cast<UCustomizableObjectNodeSkeletalMesh>(SkeletalMeshPin->GetOwningNode()))
				{
					MeshData.Metadata.Mesh = SkeletalMeshNode->GetMesh().ToSoftObjectPath();
					SkeletalMeshNode->GetPinSection(*SkeletalMeshPin, MetadataLODIndex, MetadataSectionIndex, MetadataLayoutIndex);
				}
				else if (const UCustomizableObjectNodeTable* TableNode = Cast<UCustomizableObjectNodeTable>(SkeletalMeshPin->GetOwningNode()))
				{
					MeshData.Metadata.Mesh = TableNode->GetColumnDefaultAssetByType<USkeletalMesh>(SkeletalMeshPin);
					TableNode->GetPinLODAndSection(SkeletalMeshPin, MetadataLODIndex, MetadataSectionIndex);
				}

				MeshData.Metadata.LODIndex = MetadataLODIndex;
				MeshData.Metadata.SectionIndex = MetadataSectionIndex;
			}
		}

		mu::Ptr<mu::NodeMesh> MeshNode;
		
		if (bGeneratingImplicitComponent)
		{
			MeshNode = GenerationContext.ComponentMeshOverride;
			SurfNode->Mesh = MeshNode;

			if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeMat->GetMeshPin()))
			{
				GenerationContext.Log(LOCTEXT("MeshIgnored", "The mesh nodes connected to a material node will be ignored because it is part of an explicit mesh component."), Node);
			}
		}
		else
		{
			if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeMat->GetMeshPin()))
			{

				// Flags to know which UV channels need layout
				FLayoutGenerationFlags LayoutGenerationFlags;
				
				LayoutGenerationFlags.TexturePinModes.Init(EPinMode::Default, TEXSTREAM_MAX_NUM_UVCHANNELS);

				const int32 NumImages = TypedNodeMat->GetNumParameters(EMaterialParameterType::Texture);
				for (int32 ImageIndex = 0; ImageIndex < NumImages; ++ImageIndex)
				{
					if (TypedNodeMat->IsImageMutableMode(ImageIndex))
					{
						const int32 UVChannel = TypedNodeMat->GetImageUVLayout(ImageIndex);
						if (LayoutGenerationFlags.TexturePinModes.IsValidIndex(UVChannel))
						{
							LayoutGenerationFlags.TexturePinModes[UVChannel] = EPinMode::Mutable;
						}
					}
				}

				GenerationContext.LayoutGenerationFlags.Push(LayoutGenerationFlags);

				MeshNode = GenerateMutableSourceMesh(ConnectedPin, GenerationContext, MeshData, false, false);

				GenerationContext.LayoutGenerationFlags.Pop();

				if (!MeshNode)
				{
					GenerationContext.Log(LOCTEXT("MeshFailed", "Mesh generation failed."), Node);
				}
				else
				{
					SurfNode->Mesh = MeshNode;
				}
			}
		}

		bool bTableMaterialPinLinked = TypedNodeMat->GetMaterialAssetPin() && FollowInputPin(*TypedNodeMat->GetMaterialAssetPin()) != nullptr;

		// Checking if we should not use the material of the table node even if it is linked to the material node
		if (TypedNodeMat->GetMaterialAssetPin())
		{
			if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeMat->GetMaterialAssetPin()))
			{
				//Check if the pin goes through a macro or tunnel node
				if (const UEdGraphPin* MaterialAssetPin = GraphTraversal::FindIOPinSourceThroughMacroContext(*ConnectedPin, &GenerationContext.MacroNodesStack))
				{
					if (const UCustomizableObjectNodeTable* TypedNodeTable = Cast< UCustomizableObjectNodeTable >(MaterialAssetPin->GetOwningNode()))
					{
						if (UMaterialInstance* TableMaterial = TypedNodeTable->GetColumnDefaultAssetByType<UMaterialInstance>(MaterialAssetPin))
						{
							// Checking if the reference material of the Table Node has the same parent as the material of the Material Node 
							if (!Material || TableMaterial->GetMaterial() != Material->GetMaterial())
							{
								bTableMaterialPinLinked = false;

								GenerationContext.Log(LOCTEXT("DifferentParentMaterial", "The Default Material Instance of the Data Table must have the same Parent Material."), TypedNodeMat->GetMaterialNode());
							}
						}
						else
						{
							FString TableColumnName = TypedNodeTable->GetPinColumnName(MaterialAssetPin);

							FText Msg = FText::Format(LOCTEXT("DefaultValueNotFound", "Couldn't find a default value in the data table's struct for the column {0}. The default value is null or not a Material Instance."), FText::FromString(TableColumnName));
							GenerationContext.Log(Msg, Node);

							bTableMaterialPinLinked = false;
						}
					}
				}
			}
		}

		int32 NumImages = TypedNodeMat->GetNumParameters(EMaterialParameterType::Texture);
		SurfNode->Images.SetNum(NumImages);

		for (int32 ImageIndex = 0; ImageIndex < NumImages; ++ImageIndex)
		{
			const UEdGraphPin* ImagePin = TypedNodeMat->GetParameterPin(EMaterialParameterType::Texture, ImageIndex);

			const bool bIsImagePinLinked = ImagePin && FollowInputPin(*ImagePin);

			if (bIsImagePinLinked && !TypedNodeMat->IsImageMutableMode(ImageIndex))
			{
				if (const UEdGraphPin* ConnectedPin = FollowInputPin(*ImagePin))
				{
					// Find or add Image properties
					const FGeneratedImagePropertiesKey PropsKey(TypedNodeMat, (uint32)ImageIndex);
					const bool bNewImageProps = !GenerationContext.ImageProperties.Contains(PropsKey);

					FGeneratedImageProperties& Props = GenerationContext.ImageProperties.FindOrAdd(PropsKey);
					if (bNewImageProps)
					{
						// We don't need a reference texture or props here, but we do need the parameter name.
						Props.TextureParameterName = TypedNodeMat->GetParameterName(EMaterialParameterType::Texture, ImageIndex).ToString();
						Props.ImagePropertiesIndex = GenerationContext.ImageProperties.Num() - 1;
						Props.bIsPassThrough = true;
					}

					// This is a connected pass-through texture that simply has to be passed to the core
					mu::Ptr<mu::NodeImage> PassThroughImagePtr = GenerateMutableSourceImage(ConnectedPin, GenerationContext, 0);
					SurfNode->Images[ImageIndex].Image = PassThroughImagePtr;

					check(Props.ImagePropertiesIndex != INDEX_NONE);
					const FString SurfNodeImageName = FString::Printf(TEXT("%d"), Props.ImagePropertiesIndex);
					SurfNode->Images[ImageIndex].Name = SurfNodeImageName;
					SurfNode->Images[ImageIndex].LayoutIndex = -1;
					SurfNode->Images[ImageIndex].MaterialName = Material->GetName();
					SurfNode->Images[ImageIndex].MaterialParameterName = Props.TextureParameterName;

				}
			}
			else
			{
				mu::NodeImagePtr GroupProjectionImg;
				UTexture2D* GroupProjectionReferenceTexture = nullptr;
				const FString ImageName = TypedNodeMat->GetParameterName(EMaterialParameterType::Texture, ImageIndex).ToString();
				const FNodeMaterialParameterId ImageId = TypedNodeMat->GetParameterId(EMaterialParameterType::Texture, ImageIndex);

				FString MaterialImageId = FGroupProjectorImageInfo::GenerateId(TypedNodeMat, ImageIndex);
				bool bShareProjectionTexturesBetweenLODs = false;
				FGroupProjectorImageInfo* ProjectorInfo = GenerationContext.GroupProjectorLODCache.Find(MaterialImageId);

				if (!ProjectorInfo) // No previous LOD of this material generated the image.
				{
					bool bIsGroupProjectorImage = false;

					GroupProjectionImg = GenerateMutableSourceGroupProjector(LOD, ImageIndex, MeshNode, GenerationContext,
						TypedNodeMat, nullptr, bShareProjectionTexturesBetweenLODs, bIsGroupProjectorImage,
						GroupProjectionReferenceTexture);

					if (GroupProjectionImg.get() || TypedNodeMat->IsImageMutableMode(ImageIndex))
					{
						// Get the reference texture
						UTexture2D* ReferenceTexture = nullptr;
						{
							//TODO(Max) UE-220247: Add support for multilayer materials
							GenerationContext.CurrentMaterialTableParameter = ImageName;
							GenerationContext.CurrentMaterialTableParameterId = ImageId.ParameterId.ToString();

							ReferenceTexture = GroupProjectionImg.get() ? GroupProjectionReferenceTexture : nullptr;
						
							if (!ReferenceTexture)
							{
								ReferenceTexture = TypedNodeMat->GetImageReferenceTexture(ImageIndex);
							}

							// In case of group projector, don't follow the pin to find the reference texture.
							if (!GroupProjectionImg.get() && !ReferenceTexture && ImagePin)
							{
								if (const UEdGraphPin* ConnectedPin = FollowInputPin(*ImagePin))
								{
									ReferenceTexture = FindReferenceImage(ConnectedPin, GenerationContext);
								}
							}

							if (!ReferenceTexture && bTableMaterialPinLinked)
							{
								if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeMat->GetMaterialAssetPin()))
								{
									ReferenceTexture = FindReferenceImage(ConnectedPin, GenerationContext);
								}
							}	

							if (!ReferenceTexture)
							{
								ReferenceTexture = TypedNodeMat->GetImageValue(ImageIndex);
							}
						}

						const FGeneratedImagePropertiesKey PropsKey(TypedNodeMat, ImageIndex);
						const bool bNewImageProps = !GenerationContext.ImageProperties.Contains(PropsKey);

						FGeneratedImageProperties& Props = GenerationContext.ImageProperties.FindOrAdd(PropsKey);

						if (bNewImageProps)
						{
							if (ReferenceTexture)
							{
								// Store properties for the generated images
								Props.TextureParameterName = ImageName;
								Props.ImagePropertiesIndex = GenerationContext.ImageProperties.Num() - 1;

								Props.CompressionSettings = ReferenceTexture->CompressionSettings;
								Props.Filter = ReferenceTexture->Filter;
								Props.SRGB = ReferenceTexture->SRGB;
								Props.LODBias = 0;
								Props.MipGenSettings = ReferenceTexture->MipGenSettings;
								Props.LODGroup = ReferenceTexture->LODGroup;
								Props.AddressX = ReferenceTexture->AddressX;
								Props.AddressY = ReferenceTexture->AddressY;
								Props.bFlipGreenChannel = ReferenceTexture->bFlipGreenChannel;


								// MaxTextureSize setting. Based on the ReferenceTexture and Platform settings.
								const UTextureLODSettings& TextureLODSettings = GenerationContext.CompilationContext.Options.TargetPlatform->GetTextureLODSettings();
								Props.MaxTextureSize = GetMaxTextureSize(*ReferenceTexture, TextureLODSettings);

								// ReferenceTexture source size. Textures contributing to this Image should be equal to or smaller than TextureSize. 
								// The LOD Bias applied to the root node will be applied on top of it.
								Props.TextureSize = (int32)FMath::Max3(ReferenceTexture->Source.GetSizeX(), ReferenceTexture->Source.GetSizeY(), 1LL);

								// TODO: MTBL-1081
								// TextureGroup::TEXTUREGROUP_UI does not support streaming. If we generate a texture that requires streaming and set this group, it will crash when initializing the resource. 
								// If LODGroup == TEXTUREGROUP_UI, UTexture::IsPossibleToStream() will return false and UE will assume all mips are loaded, when they're not, and crash.
								if (Props.LODGroup == TEXTUREGROUP_UI)
								{
									Props.LODGroup = TextureGroup::TEXTUREGROUP_Character;

									FString msg = FString::Printf(TEXT("The Reference texture [%s] is using TEXTUREGROUP_UI which does not support streaming. Please set a different TEXTURE group."),
										*ReferenceTexture->GetName(), *ImageName);
									GenerationContext.Log(FText::FromString(msg), Node, EMessageSeverity::Info);
								}
							}
							else
							{
								// warning!
								FString msg = FString::Printf(TEXT("The Reference texture for material image [%s] is not set and it couldn't be found automatically."), *ImageName);
								GenerationContext.Log(FText::FromString(msg), Node);
							}
						}

						// Generate the texture nodes
						mu::NodeImagePtr ImageNode = [&]()
						{
							if (TypedNodeMat->IsImageMutableMode(ImageIndex))
							{
								if (ImagePin)
								{
									if (const UEdGraphPin* ConnectedPin = FollowInputPin(*ImagePin))
									{
										return GenerateMutableSourceImage(ConnectedPin, GenerationContext, Props.TextureSize);
									}
								}

								if (bTableMaterialPinLinked)
								{
									// If the table material pin is linked to a table node, get all the textures of the current material parameter (CurrentMaterialTableParameter) from the Material Instances of the specified data table column.
									// Then Generate a mutable table column with all these textures.
									if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeMat->GetMaterialAssetPin()))
									{
										return GenerateMutableSourceImage(ConnectedPin, GenerationContext, Props.TextureSize);
									}
								}

								// Else
								{
									UTexture2D* Texture2D = TypedNodeMat->GetImageValue(ImageIndex);

									if (Texture2D)
									{
										mu::Ptr<mu::NodeImageConstant> ConstImageNode = new mu::NodeImageConstant();
										TSharedPtr<mu::FImage> ImageConstant = GenerateImageConstant(Texture2D, GenerationContext, false);
										ConstImageNode->SetValue(ImageConstant);

										const uint32 MipsToSkip = ComputeLODBiasForTexture(GenerationContext, *Texture2D, nullptr, Props.TextureSize);
										mu::Ptr<mu::NodeImage> Result =  ResizeTextureByNumMips(ConstImageNode, MipsToSkip);
	
										const UTextureLODSettings& TextureLODSettings = GenerationContext.CompilationContext.Options.TargetPlatform->GetTextureLODSettings();
										const FTextureLODGroup& LODGroupInfo = TextureLODSettings.GetTextureLODGroup(Texture2D->LODGroup);

										ConstImageNode->SourceDataDescriptor.OptionalMaxLODSize = LODGroupInfo.OptionalMaxLODSize;
										ConstImageNode->SourceDataDescriptor.OptionalLODBias = LODGroupInfo.OptionalLODBias;
										ConstImageNode->SourceDataDescriptor.NumNonOptionalLODs = GenerationContext.CompilationContext.Options.MinDiskMips;

										const FString TextureName = GetNameSafe(Texture2D).ToLower();
										ConstImageNode->SourceDataDescriptor.SourceId = CityHash32(reinterpret_cast<const char*>(*TextureName), TextureName.Len() * sizeof(FString::ElementType));

										return Result;
									}
									else
									{
										return mu::NodeImagePtr();
									}
								}
							}
							else
							{
								return mu::NodeImagePtr();
							}
						}();

						if (GroupProjectionImg.get())
						{
							ImageNode = GroupProjectionImg;
						}

						if (ReferenceTexture)
						{
							// Apply base LODBias. It will be propagated to most images.
							const uint32 BaseLODBias = ComputeLODBiasForTexture(GenerationContext, *ReferenceTexture);
							mu::NodeImagePtr LastImage = ResizeTextureByNumMips(ImageNode, BaseLODBias);

							if (ReferenceTexture->MipGenSettings != TextureMipGenSettings::TMGS_NoMipmaps)
							{
								mu::EMipmapFilterType MipGenerationFilterType = Invoke([&]()
								{
									if (ReferenceTexture)
									{
										switch (ReferenceTexture->MipGenSettings)
										{
											case TextureMipGenSettings::TMGS_SimpleAverage: return mu::EMipmapFilterType::SimpleAverage;
											case TextureMipGenSettings::TMGS_Unfiltered:    return mu::EMipmapFilterType::Unfiltered;
											default: return mu::EMipmapFilterType::SimpleAverage;
										}
									}

									return mu::EMipmapFilterType::SimpleAverage;
								});


								mu::Ptr<mu::NodeImageMipmap> MipmapImage = new mu::NodeImageMipmap();
								MipmapImage->Source = LastImage;
								MipmapImage->Settings.FilterType = MipGenerationFilterType;
								MipmapImage->Settings.AddressMode = mu::EAddressMode::None;

								MipmapImage->SetMessageContext(Node);
								LastImage = MipmapImage;
							}

							// Apply composite image. This needs to be computed after mipmaps generation. 	
							if (ReferenceTexture && ReferenceTexture->GetCompositeTexture() && ReferenceTexture->CompositeTextureMode != CTM_Disabled)
							{
								mu::Ptr<mu::NodeImageNormalComposite> CompositedImage = new mu::NodeImageNormalComposite();
								CompositedImage->Base = LastImage;
								CompositedImage->Power = ReferenceTexture->CompositePower;

								mu::ECompositeImageMode CompositeImageMode = [CompositeTextureMode = ReferenceTexture->CompositeTextureMode]() -> mu::ECompositeImageMode
								{
									switch (CompositeTextureMode)
									{
										case CTM_NormalRoughnessToRed: return mu::ECompositeImageMode::CIM_NormalRoughnessToRed;
										case CTM_NormalRoughnessToGreen: return mu::ECompositeImageMode::CIM_NormalRoughnessToGreen;
										case CTM_NormalRoughnessToBlue: return mu::ECompositeImageMode::CIM_NormalRoughnessToBlue;
										case CTM_NormalRoughnessToAlpha: return mu::ECompositeImageMode::CIM_NormalRoughnessToAlpha;

										default: return mu::ECompositeImageMode::CIM_Disabled;
									}
								}();

								CompositedImage->Mode = CompositeImageMode;

								mu::Ptr<mu::NodeImageConstant> CompositeNormalImage = new mu::NodeImageConstant();

								UTexture2D* ReferenceCompositeNormalTexture = Cast<UTexture2D>(ReferenceTexture->GetCompositeTexture());
								if (ReferenceCompositeNormalTexture)
								{
									// GenerationContext.ArrayTextureUnrealToMutableTask.Add(FTextureUnrealToMutableTask(CompositeNormalImage, ReferenceCompositeNormalTexture, Node, true));
									// TODO: The normal composite part is not propagated, so it will be unsupported. Create a task that performs the required transforms at mutable image level, and add the right operations here
									// instead of propagating the flag and doing them on unreal-convert.
									TSharedPtr<mu::FImage> ImageConstant = GenerateImageConstant(ReferenceCompositeNormalTexture, GenerationContext, false);
									CompositeNormalImage->SetValue(ImageConstant);

									mu::Ptr<mu::NodeImageMipmap> NormalCompositeMipmapImage = new mu::NodeImageMipmap();
									const uint32 MipsToSkip = ComputeLODBiasForTexture(GenerationContext, *ReferenceCompositeNormalTexture, ReferenceTexture);
									NormalCompositeMipmapImage->Source = ResizeTextureByNumMips(CompositeNormalImage, MipsToSkip);
									NormalCompositeMipmapImage->Settings.FilterType = mu::EMipmapFilterType::SimpleAverage;
									NormalCompositeMipmapImage->Settings.AddressMode = mu::EAddressMode::None;

									CompositedImage->Normal = NormalCompositeMipmapImage;

									CompositeNormalImage->SourceDataDescriptor.OptionalMaxLODSize = 0;
									if (GenerationContext.CompilationContext.Options.TargetPlatform)
									{
										const UTextureLODSettings& TextureLODSettings = GenerationContext.CompilationContext.Options.TargetPlatform->GetTextureLODSettings();
										const FTextureLODGroup& LODGroupInfo = TextureLODSettings.GetTextureLODGroup(ReferenceCompositeNormalTexture->LODGroup);

										CompositeNormalImage->SourceDataDescriptor.OptionalMaxLODSize = LODGroupInfo.OptionalMaxLODSize;
										CompositeNormalImage->SourceDataDescriptor.OptionalLODBias = LODGroupInfo.OptionalLODBias;
										CompositeNormalImage->SourceDataDescriptor.NumNonOptionalLODs = GenerationContext.CompilationContext.Options.MinDiskMips;
									}

									const FString TextureName = GetNameSafe(ReferenceCompositeNormalTexture).ToLower();
									CompositeNormalImage->SourceDataDescriptor.SourceId = CityHash32(reinterpret_cast<const char*>(*TextureName), TextureName.Len() * sizeof(FString::ElementType));
								}

								LastImage = CompositedImage;
							}

							mu::Ptr<mu::NodeImage> FormatSource = LastImage;
							mu::Ptr<mu::NodeImageFormat> FormatImage = new mu::NodeImageFormat();
							FormatImage->Source = LastImage.get();
							FormatImage->Format = mu::EImageFormat::RGBA_UByte;
							FormatImage->SetMessageContext(Node);
							LastImage = FormatImage;

							TArray<TArray<FTextureBuildSettings>> BuildSettingsPerFormatPerLayer;
							if (GenerationContext.CompilationContext.Options.TargetPlatform)
							{
								ReferenceTexture->GetTargetPlatformBuildSettings(GenerationContext.CompilationContext.Options.TargetPlatform, BuildSettingsPerFormatPerLayer);

								bool bIsServerOnly = GenerationContext.CompilationContext.Options.TargetPlatform->IsServerOnly();
								// Suppress the message for server only platforms. Images are discarded is ServerOnly. 
								if (!bIsServerOnly && BuildSettingsPerFormatPerLayer.IsEmpty())
								{
									const FString ReplacedImageFormatMsg = FString::Printf(TEXT("In object [%s] for platform [%s] the unsupported image format of texture [%s] is used, RGBA_UByte will be used instead."),
										*GenerationContext.GetObjectName(),
										*GenerationContext.CompilationContext.Options.TargetPlatform->PlatformName(),
										*ReferenceTexture->GetName());
									const FText ReplacedImageFormatText = FText::FromString(ReplacedImageFormatMsg);
									GenerationContext.Log(ReplacedImageFormatText, Node, EMessageSeverity::Info);
									UE_LOG(LogMutable, Log, TEXT("%s"), *ReplacedImageFormatMsg);
								}
								else if (BuildSettingsPerFormatPerLayer.Num() > 1)
								{
									const FString ReplacedImageFormatMsg = FString::Printf(TEXT("In object [%s] for platform [%s] the image format of texture [%s] has multiple target formats. Only one will be used.."),
										*GenerationContext.GetObjectName(),
										*GenerationContext.CompilationContext.Options.TargetPlatform->PlatformName(),
										*ReferenceTexture->GetName());
									const FText ReplacedImageFormatText = FText::FromString(ReplacedImageFormatMsg);
									GenerationContext.Log(ReplacedImageFormatText, Node, EMessageSeverity::Info);
									UE_LOG(LogMutable, Log, TEXT("%s"), *ReplacedImageFormatMsg);
								}
							}

							if (!BuildSettingsPerFormatPerLayer.IsEmpty())
							{
								const TArray<FTextureBuildSettings>& BuildSettingsPerLayer = BuildSettingsPerFormatPerLayer[0];

								if (GenerationContext.CompilationContext.Options.TextureCompression!=ECustomizableObjectTextureCompression::None)
								{
									static ITextureFormatManagerModule* TextureFormatManager = nullptr;
									if (!TextureFormatManager)
									{
										TextureFormatManager = &FModuleManager::LoadModuleChecked<ITextureFormatManagerModule>("TextureFormat");
										check(TextureFormatManager);
									}
									const ITextureFormat* TextureFormat = TextureFormatManager->FindTextureFormat(BuildSettingsPerLayer[0].TextureFormatName);
									check(TextureFormat);
									EPixelFormat UnrealTargetPlatformFormat = TextureFormat->GetEncodedPixelFormat(BuildSettingsPerLayer[0], false);
									EPixelFormat UnrealTargetPlatformFormatAlpha = TextureFormat->GetEncodedPixelFormat(BuildSettingsPerLayer[0], true);

									// \TODO: The QualityFix filter is used while the internal mutable runtime compression doesn't provide enough quality for some large block formats.
									mu::EImageFormat MutableFormat = QualityAndPerformanceFix(UnrealToMutablePixelFormat(UnrealTargetPlatformFormat,false));
									mu::EImageFormat MutableFormatIfAlpha = QualityAndPerformanceFix(UnrealToMutablePixelFormat(UnrealTargetPlatformFormatAlpha,true));

									// Temp hack to enable RG->LA 
									if (GenerationContext.CompilationContext.Options.TargetPlatform)
									{
										bool bUseLA = GenerationContext.CompilationContext.Options.TargetPlatform->SupportsFeature(ETargetPlatformFeatures::NormalmapLAEncodingMode);
										if (bUseLA)
										{
											// We'll have to trust the reference texture because the actual internal settings are opaque.
											// See GetQualityFormat in TextureFormatASTC.cpp to understand how it works, but it depends on some inaccessible texture name constants like this:
											//bool bIsNormalMapFormat = TextureFormatName == GTextureFormatNameASTC_NormalAG ||
											//		TextureFormatName == GTextureFormatNameASTC_NormalRG ||
											//		TextureFormatName == GTextureFormatNameASTC_NormalLA ||
											//		TextureFormatName == GTextureFormatNameASTC_NormalRG_Precise;
											bool bIsNormalMapFormat = ReferenceTexture->IsNormalMap();

											if (bIsNormalMapFormat)
											{
												// Insert a channel swizzle
												mu::Ptr<mu::NodeImageSwizzle> Swizzle = new mu::NodeImageSwizzle;
												Swizzle->SetFormat( mu::EImageFormat::RGBA_UByte );
												Swizzle->Sources[0] = FormatSource;
												Swizzle->Sources[1] = FormatSource;
												Swizzle->Sources[2] = FormatSource;
												Swizzle->Sources[3] = FormatSource;
												Swizzle->SourceChannels[0] = 0;
												Swizzle->SourceChannels[1] = 0;
												Swizzle->SourceChannels[2] = 0;
												Swizzle->SourceChannels[3] = 1;

												FormatImage->Source = Swizzle;
											}
										}
									}

									// Unsupported format: look for something generic
									if (MutableFormat == mu::EImageFormat::None)
									{
										const FString ReplacedImageFormatMsg = FString::Printf(TEXT("In object [%s] the unsupported image format %d is used, RGBA_UByte will be used instead."), *GenerationContext.GetObjectName(), UnrealTargetPlatformFormat );
										const FText ReplacedImageFormatText = FText::FromString(ReplacedImageFormatMsg);
										GenerationContext.Log(ReplacedImageFormatText, Node, EMessageSeverity::Info);
										UE_LOG(LogMutable, Log, TEXT("%s"), *ReplacedImageFormatMsg);
										MutableFormat = mu::EImageFormat::RGBA_UByte;
									}
									if (MutableFormatIfAlpha == mu::EImageFormat::None)
									{
										const FString ReplacedImageFormatMsg = FString::Printf(TEXT("In object [%s] the unsupported image format %d is used, RGBA_UByte will be used instead."), *GenerationContext.GetObjectName(), UnrealTargetPlatformFormatAlpha);
										const FText ReplacedImageFormatText = FText::FromString(ReplacedImageFormatMsg);
										GenerationContext.Log(ReplacedImageFormatText, Node, EMessageSeverity::Info);
										UE_LOG(LogMutable, Log, TEXT("%s"), *ReplacedImageFormatMsg);
										MutableFormatIfAlpha = mu::EImageFormat::RGBA_UByte;
									}

									FormatImage->Format = MutableFormat;
									FormatImage->FormatIfAlpha = MutableFormatIfAlpha;
								}
							}

							ImageNode = LastImage;
						}

						SurfNode->Images[ImageIndex].Image = ImageNode;

						check(Props.ImagePropertiesIndex != INDEX_NONE);
						const FString SurfNodeImageName = FString::Printf(TEXT("%d"), Props.ImagePropertiesIndex);

						// Encoding material layer in mutable name
						const int32 LayerIndex = TypedNodeMat->GetParameterLayerIndex(EMaterialParameterType::Texture, ImageIndex);
						const FString LayerEncoding = LayerIndex != INDEX_NONE ? "-MutableLayerParam:" + FString::FromInt(LayerIndex) : "";
						
						SurfNode->Images[ImageIndex].Name = SurfNodeImageName + LayerEncoding;

						// If we are generating an implicit component (with a passthrough mesh) we don't apply any layout.
						int32 UVLayout = -1;
						if (!bGeneratingImplicitComponent)
						{
							UVLayout = TypedNodeMat->GetImageUVLayout(ImageIndex);;
						}
						SurfNode->Images[ImageIndex].LayoutIndex = UVLayout;
						SurfNode->Images[ImageIndex].MaterialName = Material->GetName();
						SurfNode->Images[ImageIndex].MaterialParameterName = ImageName;

						if (bShareProjectionTexturesBetweenLODs && bIsGroupProjectorImage)
						{
							// Add to the GroupProjectorLODCache to potentially reuse this projection texture in higher LODs
							ensure(LOD == GenerationContext.FirstLODAvailable[GenerationContext.CurrentMeshComponent]);
							GenerationContext.GroupProjectorLODCache.Add(MaterialImageId,
								FGroupProjectorImageInfo(ImageNode, ImageName, ImageName, TypedNodeMat, SurfNode, UVLayout));
						}
					}
				}
				else
				{
					ensure(LOD > GenerationContext.FirstLODAvailable[GenerationContext.CurrentMeshComponent]);
					check(ProjectorInfo->SurfNode->Images[ImageIndex].Image == ProjectorInfo->ImageNode);
					SurfNode->Images[ImageIndex].Image = ProjectorInfo->ImageNode;
					SurfNode->Images[ImageIndex].Name = ProjectorInfo->TextureName;
					SurfNode->Images[ImageIndex].LayoutIndex = ProjectorInfo->UVLayout;
				}
			}
		}

		const int32 NumVectors = TypedNodeMat->GetNumParameters(EMaterialParameterType::Vector);
		SurfNode->Vectors.SetNum(NumVectors);
		for (int32 VectorIndex = 0; VectorIndex < NumVectors; ++VectorIndex)
		{
			const UEdGraphPin* VectorPin = TypedNodeMat->GetParameterPin(EMaterialParameterType::Vector, VectorIndex);
			bool bVectorPinConnected = VectorPin && FollowInputPin(*VectorPin);

			FString VectorName = TypedNodeMat->GetParameterName(EMaterialParameterType::Vector, VectorIndex).ToString();
			FNodeMaterialParameterId VectorId = TypedNodeMat->GetParameterId(EMaterialParameterType::Vector, VectorIndex);

			GenerationContext.CurrentMaterialTableParameter = VectorName;
			GenerationContext.CurrentMaterialTableParameterId = VectorId.ParameterId.ToString();

			if (bVectorPinConnected)
			{				
				if (const UEdGraphPin* ConnectedPin = FollowInputPin(*VectorPin))
				{
					mu::Ptr<mu::NodeColour> ColorNode = GenerateMutableSourceColor(ConnectedPin, GenerationContext);

					// Encoding material layer in mutable name
					if (const int32 LayerIndex = TypedNodeMat->GetParameterLayerIndex(EMaterialParameterType::Vector, VectorIndex); LayerIndex != INDEX_NONE)
					{
						VectorName += "-MutableLayerParam:" + FString::FromInt(LayerIndex);
					}

					SurfNode->Vectors[VectorIndex].Vector = ColorNode;
					SurfNode->Vectors[VectorIndex].Name = VectorName;
				}
			}
		}

		const int32 NumScalar = TypedNodeMat->GetNumParameters(EMaterialParameterType::Scalar);
		SurfNode->Scalars.SetNum(NumScalar);
		for (int32 ScalarIndex = 0; ScalarIndex < NumScalar; ++ScalarIndex)
		{
			const UEdGraphPin* ScalarPin = TypedNodeMat->GetParameterPin(EMaterialParameterType::Scalar, ScalarIndex);
			bool bScalarPinConnected = ScalarPin && FollowInputPin(*ScalarPin);

			FString ScalarName = TypedNodeMat->GetParameterName(EMaterialParameterType::Scalar, ScalarIndex).ToString();
			FNodeMaterialParameterId ScalarId = TypedNodeMat->GetParameterId(EMaterialParameterType::Scalar, ScalarIndex);

			GenerationContext.CurrentMaterialTableParameter = ScalarName;
			GenerationContext.CurrentMaterialTableParameterId = ScalarId.ParameterId.ToString();

			if (bScalarPinConnected)
			{
				if (const UEdGraphPin* ConnectedPin = FollowInputPin(*ScalarPin))
				{
					mu::Ptr<mu::NodeScalar> ScalarNode = GenerateMutableSourceFloat(ConnectedPin, GenerationContext);

					// Encoding material layer in mutable name
					if (const int32 LayerIndex = TypedNodeMat->GetParameterLayerIndex(EMaterialParameterType::Scalar, ScalarIndex); LayerIndex != INDEX_NONE)
					{
						ScalarName += "-MutableLayerParam:" + FString::FromInt(LayerIndex);
					}

					SurfNode->Scalars[ScalarIndex].Scalar = ScalarNode;
					SurfNode->Scalars[ScalarIndex].Name = ScalarName;
				}
			}
		}

		// New method to pass the surface id as a scalar parameter
		{
			int32 MaterialIndex = NumScalar;
			SurfNode->Scalars.SetNum(NumScalar + 1);

			const UEdGraphPin* MaterialPin = TypedNodeMat->GetMaterialAssetPin();
			check(MaterialPin);

			//Encoding name for material material id parameter
			FString MaterialName = "__MutableMaterialId";

			if (bTableMaterialPinLinked)
			{
				if (const UEdGraphPin* ConnectedPin = FollowInputPin(*MaterialPin))
				{					
					// The 'None' option will use the CurrentReferencedMaterialIndex.
					check(GenerationContext.CurrentReferencedMaterialIndex != INDEX_NONE);
					
					// Adding pin ID to support multiple material columns
					MaterialName += MaterialPin->PinId.ToString();

					GenerationContext.CurrentMaterialTableParameterId = MaterialName;
					mu::Ptr<mu::NodeScalar> ScalarNode = GenerateMutableSourceFloat(ConnectedPin, GenerationContext);

					SurfNode->Scalars[MaterialIndex].Scalar = ScalarNode;
					SurfNode->Scalars[MaterialIndex].Name = MaterialName;
				}
			}
			else
			{
				mu::Ptr<mu::NodeScalarConstant> ScalarNode = new mu::NodeScalarConstant();
				ScalarNode->Value = GenerationContext.CurrentReferencedMaterialIndex;

				SurfNode->Scalars[MaterialIndex].Scalar = ScalarNode;
				SurfNode->Scalars[MaterialIndex].Name = MaterialName;
			}
		}

		for (const FString& Tag : TypedNodeMat->GetEnableTags(&GenerationContext.MacroNodesStack))
		{
			SurfNode->Tags.AddUnique(Tag);
		}

		SurfNode->Tags.AddUnique( TypedNodeMat->GetInternalTag() );
	}

	else if (const UCustomizableObjectNodeMaterialVariation* TypedNodeVar = Cast<UCustomizableObjectNodeMaterialVariation>(Node))
	{
		mu::Ptr<mu::NodeSurfaceVariation> SurfNode = new mu::NodeSurfaceVariation();
		Result = SurfNode;

		mu::NodeSurfaceVariation::VariationType muType = mu::NodeSurfaceVariation::VariationType::Tag;
		switch (TypedNodeVar->Type)
		{
		case ECustomizableObjectNodeMaterialVariationType::Tag: muType = mu::NodeSurfaceVariation::VariationType::Tag; break;
		case ECustomizableObjectNodeMaterialVariationType::State: muType = mu::NodeSurfaceVariation::VariationType::State; break;
		default:
			check(false);
			break;
		}
		SurfNode->Type = muType;

		for (const UEdGraphPin* ConnectedPin : FollowInputPinArray(*TypedNodeVar->DefaultPin()))
		{
			// Is it a modifier?
			mu::NodeSurfacePtr ChildNode = GenerateMutableSourceSurface(ConnectedPin, GenerationContext);
			if (ChildNode)
			{
				SurfNode->DefaultSurfaces.Add(ChildNode);
			}
			else
			{
				GenerationContext.Log(LOCTEXT("SurfaceFailed", "Surface generation failed."), Node);
			}
		}

		const int32 NumVariations = TypedNodeVar->GetNumVariations();
		SurfNode->Variations.SetNum(NumVariations);
		for (int VariationIndex = 0; VariationIndex < NumVariations; ++VariationIndex)
		{
			mu::NodeSurfacePtr VariationSurfaceNode;

			if (UEdGraphPin* VariationPin = TypedNodeVar->VariationPin(VariationIndex))
			{
				SurfNode->Variations[VariationIndex].Tag = TypedNodeVar->GetVariationTag(VariationIndex, &GenerationContext.MacroNodesStack);
				for (const UEdGraphPin* ConnectedPin : FollowInputPinArray(*VariationPin))
				{
					// Is it a modifier?
					mu::NodeSurfacePtr ChildNode = GenerateMutableSourceSurface(ConnectedPin, GenerationContext);
					if (ChildNode)
					{
						SurfNode->Variations[VariationIndex].Surfaces.Add( ChildNode );
					}
					else
					{
						GenerationContext.Log(LOCTEXT("SurfaceModifierFailed", "Surface generation failed."), Node);
					}
				}
			}
		}
	}

	else if (const UCustomizableObjectNodeMaterialSwitch* TypedNodeSwitch = Cast<UCustomizableObjectNodeMaterialSwitch>(Node))
	{
		// Using a lambda so control flow is easier to manage.
		Result = [&]()
		{
			const UEdGraphPin* SwitchParameter = TypedNodeSwitch->SwitchParameter();

			// Check Switch Parameter arity preconditions.
			if (const UEdGraphPin* EnumPin = FollowInputPin(*SwitchParameter))
			{
				mu::Ptr<mu::NodeScalar> SwitchParam = GenerateMutableSourceFloat(EnumPin, GenerationContext);

				// Switch Param not generated
				if (!SwitchParam)
				{
					// Warn about a failure.
					if (EnumPin)
					{
						const FText Message = LOCTEXT("FailedToGenerateSwitchParam", "Could not generate switch enum parameter. Please refesh the switch node and connect an enum.");
						GenerationContext.Log(Message, Node);
					}

					return Result;
				}

				if (SwitchParam->GetType() != mu::NodeScalarEnumParameter::GetStaticType())
				{
					const FText Message = LOCTEXT("WrongSwitchParamType", "Switch parameter of incorrect type.");
					GenerationContext.Log(Message, Node);

					return Result;
				}

				const int32 NumSwitchOptions = TypedNodeSwitch->GetNumElements();

				mu::NodeScalarEnumParameter* EnumParameter = static_cast<mu::NodeScalarEnumParameter*>(SwitchParam.get());
				if (NumSwitchOptions != EnumParameter->Options.Num())
				{
					const FText Message = LOCTEXT("MismatchedSwitch", "Switch enum and switch node have different number of options. Please refresh the switch node to make sure the outcomes are labeled properly.");
					GenerationContext.Log(Message, Node);
				}

				mu::Ptr<mu::NodeSurfaceSwitch> SwitchNode = new mu::NodeSurfaceSwitch;
				SwitchNode->Parameter = SwitchParam;
				SwitchNode->Options.SetNum(NumSwitchOptions);

				for (int32 SelectorIndex = 0; SelectorIndex < NumSwitchOptions; ++SelectorIndex)
				{
					if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeSwitch->GetElementPin(SelectorIndex)))
					{
						mu::NodeSurfacePtr ChildNode = GenerateMutableSourceSurface(ConnectedPin, GenerationContext);
						if (ChildNode)
						{
							SwitchNode->Options[SelectorIndex] = ChildNode;
						}
						else
						{
							// Probably ok
							//GenerationContext.Log(LOCTEXT("SurfaceModifierFailed", "Surface generation failed."), Node);
						}
					}
				}

				Result = SwitchNode;
				return Result;
			}
			else
			{
				GenerationContext.Log(LOCTEXT("NoEnumParamInSwitch", "Switch nodes must have an enum switch parameter. Please connect an enum and refesh the switch node."), Node);
				return Result;
			}
		}(); // invoke lambda.
	}

	else if (const UCustomizableObjectNodeMacroInstance* TypedNodeMacro = Cast<UCustomizableObjectNodeMacroInstance>(Node))
	{
		bCacheNode = false;
		Result = GenerateMutableSourceMacro<mu::NodeSurface>(*Pin, GenerationContext, GenerateMutableSourceSurface);
	}

	else if (const UCustomizableObjectNodeTunnel* TypedNodeTunnel = Cast<UCustomizableObjectNodeTunnel>(Node))
	{
		bCacheNode = false;
		Result = GenerateMutableSourceMacro<mu::NodeSurface>(*Pin, GenerationContext, GenerateMutableSourceSurface);
	}

	else
	{
		GenerationContext.Log(LOCTEXT("UnimplementedNode", "Node type not implemented yet."), Node);
	}


	if (Result)
	{
		Result->SetMessageContext(Node);
	}

	if (bCacheNode)
	{
		GenerationContext.Generated.Add(Key, FGeneratedData(Node, Result));
		GenerationContext.GeneratedNodes.Add(Node);
	}

	return Result;
}


#undef LOCTEXT_NAMESPACE
