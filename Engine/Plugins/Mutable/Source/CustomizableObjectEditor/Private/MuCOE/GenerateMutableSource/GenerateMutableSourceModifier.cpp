// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/GenerateMutableSource/GenerateMutableSourceModifier.h"

#include "Engine/StaticMesh.h"
#include "MuCOE/CustomizableObjectCompiler.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSource.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSourceMesh.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSourceImage.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSourceFloat.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSourceSurface.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSourceLayout.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSourceGroupProjector.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSourceTransform.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSourceMacro.h"
#include "MuCOE/GraphTraversal.h"
#include "MuCOE/MutableUtils.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMacroInstance.h"
#include "MuCOE/Nodes/CustomizableObjectNodeModifierClipDeform.h"
#include "MuCOE/Nodes/CustomizableObjectNodeModifierClipMorph.h"
#include "MuCOE/Nodes/CustomizableObjectNodeModifierClipWithMesh.h"
#include "MuCOE/Nodes/CustomizableObjectNodeModifierClipWithUVMask.h"
#include "MuCOE/Nodes/CustomizableObjectNodeModifierExtendMeshSection.h"
#include "MuCOE/Nodes/CustomizableObjectNodeModifierEditMeshSection.h"
#include "MuCOE/Nodes/CustomizableObjectNodeModifierMorphMeshSection.h"
#include "MuCOE/Nodes/CustomizableObjectNodeModifierRemoveMesh.h"
#include "MuCOE/Nodes/CustomizableObjectNodeModifierRemoveMeshBlocks.h"
#include "MuCOE/Nodes/CustomizableObjectNodeStaticMesh.h"
#include "MuCOE/Nodes/CustomizableObjectNodeSkeletalMesh.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTable.h"
#include "MuCOE/Nodes/CustomizableObjectNodeFloatParameter.h"
#include "MuCOE/Nodes/CustomizableObjectNodeFloatConstant.h"
#include "MuCOE/Nodes/CustomizableObjectNodeModifierTransformInMesh.h"
#include "MuCOE/Nodes/CustomizableObjectNodeStaticString.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTunnel.h"
#include "MuR/Mesh.h"
#include "MuT/NodeMeshTransform.h"
#include "MuT/NodeModifierMeshClipDeform.h"
#include "MuT/NodeModifierMeshClipMorphPlane.h"
#include "MuT/NodeModifierMeshClipWithUVMask.h"
#include "MuT/NodeModifierMeshTransformInMesh.h"
#include "MuT/NodeModifierSurfaceEdit.h"
#include "MuT/NodeMeshConstant.h"
#include "MuT/NodeMeshFragment.h"
#include "MuT/NodeMeshFormat.h"
#include "Rendering/SkeletalMeshLODModel.h"

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"

mu::Ptr<mu::NodeModifier> GenerateMutableSourceModifier(const UEdGraphPin * Pin, FMutableGraphGenerationContext & GenerationContext)
{
	check(Pin)
	RETURN_ON_CYCLE(*Pin, GenerationContext)

	CheckNumOutputs(*Pin, GenerationContext);
	
	UCustomizableObjectNode* Node = CastChecked<UCustomizableObjectNode>(Pin->GetOwningNode());

	FGeneratedKey Key(reinterpret_cast<void*>(&GenerateMutableSourceModifier), *Pin, *Node, GenerationContext, true);
	Key.CurrentMeshComponent = GenerationContext.CurrentMeshComponent;
	
	if (const FGeneratedData* Generated = GenerationContext.Generated.Find(Key))
	{
		return static_cast<mu::NodeModifier*>(Generated->Node.get());
	}
	
	mu::Ptr<mu::NodeModifier> Result;

	// Bool that determines if a node can be added to the cache of nodes.
	// Most nodes need to be added to the cache but there are some that don't. For exampel, MacroInstanceNodes
	bool bCacheNode = true; 

	if (const UCustomizableObjectNodeModifierClipMorph* TypedNodeClip = Cast<UCustomizableObjectNodeModifierClipMorph>(Node))
	{
		const EMutableMeshConversionFlags ModifiersMeshFlags =
			EMutableMeshConversionFlags::IgnoreSkinning |
			EMutableMeshConversionFlags::IgnorePhysics |
			EMutableMeshConversionFlags::IgnoreMorphs |
			EMutableMeshConversionFlags::DoNotCreateMeshMetadata;
		GenerationContext.MeshGenerationFlags.Push(ModifiersMeshFlags);

		// This modifier can be connected to multiple nodes at the same time and, when that happens and if the cache is being used, only the first node to be processed does work. 
		// By not caching the mutable node we avoid this from even happening
		bCacheNode = false;
		
		mu::Ptr<mu::NodeModifierMeshClipMorphPlane> ClipNode = new mu::NodeModifierMeshClipMorphPlane();
		Result = ClipNode;

		const FVector Origin = TypedNodeClip->GetOriginWithOffset();
		const FVector& Normal = TypedNodeClip->Normal;

		ClipNode->SetPlane(FVector3f(Origin), FVector3f(Normal));
		ClipNode->SetParams(TypedNodeClip->B, TypedNodeClip->Exponent);
		ClipNode->SetMorphEllipse(TypedNodeClip->Radius, TypedNodeClip->Radius2, TypedNodeClip->RotationAngle);

		ClipNode->SetVertexSelectionBone(GenerationContext.CompilationContext.GetBoneUnique(TypedNodeClip->BoneName), TypedNodeClip->MaxEffectRadius);

		ClipNode->MultipleTagsPolicy = TypedNodeClip->MultipleTagPolicy;
		ClipNode->RequiredTags = TypedNodeClip->GetNodeRequiredTags(&GenerationContext.MacroNodesStack);

		ClipNode->Parameters.FaceCullStrategy = TypedNodeClip->FaceCullStrategy;

		GenerationContext.MeshGenerationFlags.Pop();
	}

	else if (const UCustomizableObjectNodeModifierClipDeform* TypedNodeClipDeform = Cast<UCustomizableObjectNodeModifierClipDeform>(Node))
	{
		const EMutableMeshConversionFlags ModifiersMeshFlags =
			EMutableMeshConversionFlags::IgnoreSkinning |
			EMutableMeshConversionFlags::IgnorePhysics |
			EMutableMeshConversionFlags::IgnoreMorphs |
			EMutableMeshConversionFlags::DoNotCreateMeshMetadata;
		GenerationContext.MeshGenerationFlags.Push(ModifiersMeshFlags);

		mu::Ptr<mu::NodeModifierMeshClipDeform> ClipNode = new mu::NodeModifierMeshClipDeform();
		Result = ClipNode;
	
		ClipNode->FaceCullStrategy = TypedNodeClipDeform->FaceCullStrategy;

		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeClipDeform->ClipShapePin()))
		{
			mu::Ptr<mu::NodeMesh> ClipMesh = GenerateMutableSourceMesh(ConnectedPin, GenerationContext, FMutableSourceMeshData(), false, true);

			ClipNode->ClipMesh = ClipMesh;

			mu::EShapeBindingMethod BindingMethod = mu::EShapeBindingMethod::ClipDeformClosestProject;
			switch(TypedNodeClipDeform->BindingMethod)
			{
				case EShapeBindingMethod::ClosestProject:
					BindingMethod = mu::EShapeBindingMethod::ClipDeformClosestProject;
					break;
				case EShapeBindingMethod::NormalProject:
					BindingMethod = mu::EShapeBindingMethod::ClipDeformNormalProject;
					break;
				case EShapeBindingMethod::ClosestToSurface:
					BindingMethod = mu::EShapeBindingMethod::ClipDeformClosestToSurface;
					break;
				default:
					check(false);
					break;
			}

			ClipNode->BindingMethod = BindingMethod;
		}
		else
		{
			FText ErrorMsg = LOCTEXT("ClipDeform mesh", "The clip deform node requires an input clip shape.");
			GenerationContext.Log(ErrorMsg, TypedNodeClipDeform, EMessageSeverity::Error);
			Result = nullptr;
		}
	
		ClipNode->MultipleTagsPolicy = TypedNodeClipDeform->MultipleTagPolicy;
		ClipNode->RequiredTags = TypedNodeClipDeform->GetNodeRequiredTags(&GenerationContext.MacroNodesStack);

		GenerationContext.MeshGenerationFlags.Pop();
	}

	else if (const UCustomizableObjectNodeModifierClipWithMesh* TypedNodeClipMesh = Cast<UCustomizableObjectNodeModifierClipWithMesh>(Node))
	{
		const EMutableMeshConversionFlags ModifiersMeshFlags =
			EMutableMeshConversionFlags::IgnoreSkinning |
			EMutableMeshConversionFlags::IgnorePhysics |
			EMutableMeshConversionFlags::IgnoreMorphs |
			EMutableMeshConversionFlags::DoNotCreateMeshMetadata;
		GenerationContext.MeshGenerationFlags.Push(ModifiersMeshFlags);

		// MeshClipWithMesh can be connected to multiple objects, so the compiled NodeModifierMeshClipWithMesh
		// needs to be different for each object. If it were added to the Generated cache, all the objects would get the same.
		bCacheNode = false;

		mu::Ptr<mu::NodeModifierMeshClipWithMesh> ClipNode = new mu::NodeModifierMeshClipWithMesh();
		Result = ClipNode;

		ClipNode->FaceCullStrategy = TypedNodeClipMesh->FaceCullStrategy;

		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeClipMesh->GetClipMeshPin()))
		{
			mu::NodeMeshPtr ClipMesh = GenerateMutableSourceMesh(ConnectedPin, GenerationContext, FMutableSourceMeshData(), false, true);

			if (FMatrix Matrix = TypedNodeClipMesh->Transform.ToMatrixWithScale(); Matrix != FMatrix::Identity)
			{
				mu::Ptr<mu::NodeMeshTransform> TransformMesh = new mu::NodeMeshTransform();
				TransformMesh->Source = ClipMesh;

				TransformMesh->Transform = FMatrix44f(Matrix);
				ClipMesh = TransformMesh;
			}

			ClipNode->ClipMesh = ClipMesh;
		}
		else
		{
			FText ErrorMsg = LOCTEXT("Clipping mesh missing", "The clip mesh with mesh node requires an input clip mesh.");
			GenerationContext.Log(ErrorMsg, TypedNodeClipMesh, EMessageSeverity::Error);
			Result = nullptr;
		}

		ClipNode->MultipleTagsPolicy = TypedNodeClipMesh->MultipleTagPolicy;
		ClipNode->RequiredTags = TypedNodeClipMesh->GetNodeRequiredTags(&GenerationContext.MacroNodesStack);

		GenerationContext.MeshGenerationFlags.Pop();
	}

	else if (const UCustomizableObjectNodeModifierClipWithUVMask* TypedNodeClipUVMask = Cast<UCustomizableObjectNodeModifierClipWithUVMask>(Node))
	{
		const EMutableMeshConversionFlags ModifiersMeshFlags =
			EMutableMeshConversionFlags::IgnoreSkinning |
			EMutableMeshConversionFlags::IgnorePhysics |
			EMutableMeshConversionFlags::IgnoreMorphs |
			EMutableMeshConversionFlags::DoNotCreateMeshMetadata;
		GenerationContext.MeshGenerationFlags.Push(ModifiersMeshFlags);

		// This modifier can be connected to multiple objects, so the compiled node
		// needs to be different for each object. If it were added to the Generated cache, all the objects would get the same.
		bCacheNode = false;

		mu::Ptr<mu::NodeModifierMeshClipWithUVMask> ClipNode = new mu::NodeModifierMeshClipWithUVMask();
		Result = ClipNode;

		ClipNode->FaceCullStrategy = TypedNodeClipUVMask->FaceCullStrategy;

		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeClipUVMask->ClipMaskPin()))
		{
			mu::Ptr<mu::NodeImage> ClipMask = GenerateMutableSourceImage(ConnectedPin, GenerationContext, 0);

			ClipNode->ClipMask = ClipMask;
		}
		else
		{
			FText ErrorMsg = LOCTEXT("ClipUVMask mesh", "The clip mesh with UV Mask node requires an input texture mask.");
			GenerationContext.Log(ErrorMsg, TypedNodeClipUVMask, EMessageSeverity::Error);
			Result = nullptr;
		}

		ClipNode->LayoutIndex = TypedNodeClipUVMask->UVChannelForMask;

		ClipNode->MultipleTagsPolicy = TypedNodeClipUVMask->MultipleTagPolicy;
		ClipNode->RequiredTags = TypedNodeClipUVMask->GetNodeRequiredTags(&GenerationContext.MacroNodesStack);

		GenerationContext.MeshGenerationFlags.Pop();
	}

	else if (UCustomizableObjectNodeModifierExtendMeshSection* TypedNodeExt = Cast<UCustomizableObjectNodeModifierExtendMeshSection>(Node))
	{
		const EMutableMeshConversionFlags ModifiersMeshFlags = EMutableMeshConversionFlags::None;
		GenerationContext.MeshGenerationFlags.Push(ModifiersMeshFlags);

		mu::Ptr<mu::NodeModifierSurfaceEdit> SurfNode = new mu::NodeModifierSurfaceEdit();
		Result = SurfNode;

		// TODO: This was used in the non-modifier version for group projectors. It may affect the "drop projection from LOD" feature.
		const int32 LOD = Node->IsAffectedByLOD() ? GenerationContext.CurrentLOD : 0;

		SurfNode->MultipleTagsPolicy = TypedNodeExt->MultipleTagPolicy;
		SurfNode->RequiredTags = TypedNodeExt->GetNodeRequiredTags(&GenerationContext.MacroNodesStack);

		// Is this enough? Should we try to narrow down with potential mesh sections modified by this?
		int32 LODCount = GenerationContext.NumLODs[GenerationContext.CurrentMeshComponent];
		SurfNode->LODs.SetNum(LODCount);

		for (int32 LODIndex= TypedNodeExt->FirstLOD; LODIndex<LODCount; ++LODIndex)
		{
			GenerationContext.FromLOD = TypedNodeExt->FirstLOD;
			GenerationContext.CurrentLOD = LODIndex;

			mu::Ptr<mu::NodeMesh> AddMeshNode;
			if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeExt->AddMeshPin()))
			{
				// Flags to know which UV channels need layout
				FLayoutGenerationFlags LayoutGenerationFlags;
				LayoutGenerationFlags.TexturePinModes.Init(EPinMode::Mutable, TEXSTREAM_MAX_NUM_UVCHANNELS);

				GenerationContext.LayoutGenerationFlags.Push(LayoutGenerationFlags);

				FMutableSourceMeshData MeshData;

				// Find reference mesh used to generate the surface metadata for this fragment.
				if(ConnectedPin)
				{
					//NOTE: This is the same is done in GenerateMutableSourceSurface. 
					if (const UEdGraphPin* SkeletalMeshPin = FindMeshBaseSource(*ConnectedPin, false, &GenerationContext.MacroNodesStack))
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
				
				AddMeshNode = GenerateMutableSourceMesh(ConnectedPin, GenerationContext, MeshData, true, false);

				GenerationContext.LayoutGenerationFlags.Pop();
			}

			SurfNode->LODs[LODIndex].MeshAdd = AddMeshNode;

			const int32 NumImages = TypedNodeExt->GetNumParameters(EMaterialParameterType::Texture);
			SurfNode->LODs[LODIndex].Textures.SetNum(NumImages);
			for (int32 ImageIndex = 0; ImageIndex < NumImages; ++ImageIndex)
			{
				mu::NodeImagePtr ImageNode;
				FString MaterialParameterName;

				if (!ImageNode) // If
				{
					const FString MaterialImageId = FGroupProjectorImageInfo::GenerateId(TypedNodeExt, ImageIndex);
					const FGroupProjectorImageInfo* ProjectorInfo = GenerationContext.GroupProjectorLODCache.Find(MaterialImageId);

					if (ProjectorInfo)
					{
						ensure(LOD > GenerationContext.FirstLODAvailable[GenerationContext.CurrentMeshComponent]);
						check(ProjectorInfo->SurfNode->Images[ImageIndex].Image == ProjectorInfo->ImageNode);
						ImageNode = ProjectorInfo->ImageNode;
					}
				}

				if (!ImageNode) // Else if
				{
					bool bShareProjectionTexturesBetweenLODs = false;
					bool bIsGroupProjectorImage = false;
					UTexture2D* GroupProjectionReferenceTexture = nullptr;

					ImageNode = GenerateMutableSourceGroupProjector(LOD, ImageIndex, AddMeshNode, GenerationContext,
						nullptr, TypedNodeExt, bShareProjectionTexturesBetweenLODs, bIsGroupProjectorImage,
						GroupProjectionReferenceTexture);
				}

				if (!ImageNode) // Else if
				{
					const FNodeMaterialParameterId ImageId = TypedNodeExt->GetParameterId(EMaterialParameterType::Texture, ImageIndex);

					if (TypedNodeExt->UsesImage(ImageId))
					{
						// TODO
						//check(ParentMaterialNode->IsImageMutableMode(ImageIndex)); // Ensured at graph time. If it fails, something is wrong.

						if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeExt->GetUsedImagePin(ImageId)))
						{
							// ReferenceTextureSize is used to limit the size of textures contributing to the final image.
							const int32 ReferenceTextureSize = 0; // TODO GetBaseTextureSize(GenerationContext, TypedNodeExt, ImageIndex);

							ImageNode = GenerateMutableSourceImage(ConnectedPin, GenerationContext, ReferenceTextureSize);
							MaterialParameterName = TypedNodeExt->GetParameterName(EMaterialParameterType::Texture, ImageIndex).ToString();

						}
					}
				}


				SurfNode->LODs[LODIndex].Textures[ImageIndex].Extend = ImageNode;
				SurfNode->LODs[LODIndex].Textures[ImageIndex].MaterialParameterName = MaterialParameterName;
			}
		}

		SurfNode->EnableTags = TypedNodeExt->GetEnableTags(&GenerationContext.MacroNodesStack);
		SurfNode->EnableTags.AddUnique(TypedNodeExt->GetInternalTag());

		GenerationContext.MeshGenerationFlags.Pop();
		GenerationContext.FromLOD = 0;
		GenerationContext.CurrentLOD = 0;

	}

	else if (const UCustomizableObjectNodeModifierRemoveMesh* TypedNodeRem = Cast<UCustomizableObjectNodeModifierRemoveMesh>(Node))
	{
		const EMutableMeshConversionFlags ModifiersMeshFlags =
			EMutableMeshConversionFlags::IgnoreSkinning |
			EMutableMeshConversionFlags::IgnorePhysics |
			EMutableMeshConversionFlags::IgnoreMorphs |
			EMutableMeshConversionFlags::DoNotCreateMeshMetadata;
		GenerationContext.MeshGenerationFlags.Push(ModifiersMeshFlags);

		mu::Ptr<mu::NodeModifierSurfaceEdit> SurfNode = new mu::NodeModifierSurfaceEdit();
		Result = SurfNode;

		SurfNode->MultipleTagsPolicy = TypedNodeRem->MultipleTagPolicy;
		SurfNode->RequiredTags = TypedNodeRem->GetNodeRequiredTags(&GenerationContext.MacroNodesStack);

		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeRem->RemoveMeshPin()))
		{
			// Is this enough? Should we try to narrow down with potential mesh sections modified by this?
			int32 LODCount = GenerationContext.NumLODs[GenerationContext.CurrentMeshComponent];
			SurfNode->LODs.SetNum(LODCount);

			SurfNode->FaceCullStrategy = TypedNodeRem->FaceCullStrategy;

			for (int32 LODIndex = 0; LODIndex < LODCount; ++LODIndex)
			{
				GenerationContext.FromLOD = 0;
				GenerationContext.CurrentLOD = LODIndex;

				mu::Ptr<mu::NodeMesh> RemoveMeshNode = GenerateMutableSourceMesh(ConnectedPin, GenerationContext, FMutableSourceMeshData(), false, true);
				SurfNode->LODs[LODIndex].MeshRemove = RemoveMeshNode;
			}
		}

		GenerationContext.MeshGenerationFlags.Pop();
		GenerationContext.FromLOD = 0;
		GenerationContext.CurrentLOD = 0;
	}

	else if (const UCustomizableObjectNodeModifierRemoveMeshBlocks* TypedNodeRemBlocks = Cast<UCustomizableObjectNodeModifierRemoveMeshBlocks>(Node))
	{
		const EMutableMeshConversionFlags ModifiersMeshFlags =
			EMutableMeshConversionFlags::IgnoreSkinning |
			EMutableMeshConversionFlags::IgnorePhysics |
			EMutableMeshConversionFlags::IgnoreMorphs |
			EMutableMeshConversionFlags::DoNotCreateMeshMetadata;
		GenerationContext.MeshGenerationFlags.Push(ModifiersMeshFlags);

		mu::Ptr<mu::NodeModifierMeshClipWithUVMask> ClipNode = new mu::NodeModifierMeshClipWithUVMask();
		Result = ClipNode;

		ClipNode->FaceCullStrategy = TypedNodeRemBlocks->FaceCullStrategy;

		ClipNode->MultipleTagsPolicy = TypedNodeRemBlocks->MultipleTagPolicy;
		ClipNode->RequiredTags = TypedNodeRemBlocks->GetNodeRequiredTags(&GenerationContext.MacroNodesStack);

		bool bWasEmpty = false;
		mu::Ptr<mu::NodeLayout> SourceLayout = CreateMutableLayoutNode(GenerationContext, TypedNodeRemBlocks->Layout, true, bWasEmpty);
		ClipNode->ClipLayout = SourceLayout;
		ClipNode->LayoutIndex = TypedNodeRemBlocks->ParentLayoutIndex;

		GenerationContext.MeshGenerationFlags.Pop();
	}

	else if (UCustomizableObjectNodeModifierEditMeshSection* TypedNodeEdit = Cast<UCustomizableObjectNodeModifierEditMeshSection>(Node))
	{
		const EMutableMeshConversionFlags ModifiersMeshFlags =
			EMutableMeshConversionFlags::IgnoreSkinning |
			EMutableMeshConversionFlags::IgnorePhysics |
			EMutableMeshConversionFlags::IgnoreMorphs |
			EMutableMeshConversionFlags::DoNotCreateMeshMetadata;
		GenerationContext.MeshGenerationFlags.Push(ModifiersMeshFlags);

		mu::Ptr<mu::NodeModifierSurfaceEdit> SurfNode = new mu::NodeModifierSurfaceEdit();
		Result = SurfNode;

		SurfNode->MultipleTagsPolicy = TypedNodeEdit->MultipleTagPolicy;
		SurfNode->RequiredTags = TypedNodeEdit->GetNodeRequiredTags(&GenerationContext.MacroNodesStack);

		// Is this enough? Should we try to narrow down with potential mesh sections modified by this?
		int32 LODCount = GenerationContext.NumLODs[GenerationContext.CurrentMeshComponent];
		SurfNode->LODs.SetNum(LODCount);

		for (int32 LODIndex = 0; LODIndex < LODCount; ++LODIndex)
		{
			GenerationContext.FromLOD = 0;
			GenerationContext.CurrentLOD = LODIndex;


			const int32 NumImages = TypedNodeEdit->GetNumParameters(EMaterialParameterType::Texture);
			SurfNode->LODs[LODIndex].Textures.SetNum(NumImages);
			for (int32 ImageIndex = 0; ImageIndex < NumImages; ++ImageIndex)
			{
				const FNodeMaterialParameterId ImageId = TypedNodeEdit->GetParameterId(EMaterialParameterType::Texture, ImageIndex);

				if (TypedNodeEdit->UsesImage(ImageId))
				{
					// TODO
					//check(ParentMaterialNode->IsImageMutableMode(ImageIndex)); // Ensured at graph time. If it fails, something is wrong.

					const UEdGraphPin* ConnectedImagePin = FollowInputPin(*TypedNodeEdit->GetUsedImagePin(ImageId));

					mu::NodeModifierSurfaceEdit::FTexture& ImagePatch = SurfNode->LODs[LODIndex].Textures[ImageIndex];

					ImagePatch.MaterialParameterName = TypedNodeEdit->GetParameterName(EMaterialParameterType::Texture, ImageIndex).ToString();

					// \todo: expose these two options?
					ImagePatch.PatchBlendType = mu::EBlendType::BT_BLEND;
					ImagePatch.bPatchApplyToAlpha = true;

					// ReferenceTextureSize is used to limit the size of textures contributing to the final image.
					const int32 ReferenceTextureSize = 0; //TODO GetBaseTextureSize(GenerationContext, ParentMaterialNode, ImageIndex);

					ImagePatch.PatchImage = GenerateMutableSourceImage(ConnectedImagePin, GenerationContext, ReferenceTextureSize);

					const UEdGraphPin* ImageMaskPin = TypedNodeEdit->GetUsedImageMaskPin(ImageId);
					check(ImageMaskPin); // Ensured when reconstructing EditMaterial nodes. If it fails, something is wrong.

					if (const UEdGraphPin* ConnectedMaskPin = FollowInputPin(*ImageMaskPin))
					{
						ImagePatch.PatchMask = GenerateMutableSourceImage(ConnectedMaskPin, GenerationContext, ReferenceTextureSize);
					}

					// Add the blocks to patch
					FIntPoint GridSize = TypedNodeEdit->Layout->GetGridSize();
					FVector2f GridSizeF = FVector2f(GridSize);
					ImagePatch.PatchBlocks.Reserve(TypedNodeEdit->Layout->Blocks.Num());
					for (const FCustomizableObjectLayoutBlock& LayoutBlock : TypedNodeEdit->Layout->Blocks)
					{
						FBox2f Rect;
						Rect.Min = FVector2f(LayoutBlock.Min) / GridSizeF;
						Rect.Max = FVector2f(LayoutBlock.Max) / GridSizeF;
						ImagePatch.PatchBlocks.Add(Rect);
					}
				}
			}
		}

		GenerationContext.MeshGenerationFlags.Pop();
		GenerationContext.FromLOD = 0;
		GenerationContext.CurrentLOD = 0;
	}

	else if (const UCustomizableObjectNodeModifierMorphMeshSection* TypedNodeMorph = Cast<UCustomizableObjectNodeModifierMorphMeshSection>(Node))
	{
		const EMutableMeshConversionFlags ModifiersMeshFlags =
			EMutableMeshConversionFlags::IgnoreSkinning |
			EMutableMeshConversionFlags::IgnorePhysics |
			EMutableMeshConversionFlags::IgnoreMorphs |
			EMutableMeshConversionFlags::DoNotCreateMeshMetadata;
		GenerationContext.MeshGenerationFlags.Push(ModifiersMeshFlags);

		mu::Ptr<mu::NodeModifierSurfaceEdit> SurfNode = new mu::NodeModifierSurfaceEdit();
		Result = SurfNode;

		// This modifier needs to be applied right after the mesh constant is generated
		SurfNode->bApplyBeforeNormalOperations = true;

		SurfNode->MultipleTagsPolicy = TypedNodeMorph->MultipleTagPolicy;
		SurfNode->RequiredTags = TypedNodeMorph->GetNodeRequiredTags(&GenerationContext.MacroNodesStack);

		FString MorphTargetName = TypedNodeMorph->MorphTargetName;

		if (UEdGraphPin* MorphTargetNamePin = TypedNodeMorph->MorphTargetNamePin())
		{
			if (const UEdGraphPin* ConnectedStringPin = FollowInputPin(*MorphTargetNamePin))
			{
				if (const UEdGraphPin* SourceStringPin = GraphTraversal::FindIOPinSourceThroughMacroContext(*ConnectedStringPin, &GenerationContext.MacroNodesStack))
				{
					if (const UCustomizableObjectNodeStaticString* StringNode = Cast<UCustomizableObjectNodeStaticString>(SourceStringPin->GetOwningNode()))
					{
						MorphTargetName = StringNode->Value;
					}
				}
				else
				{
					GenerationContext.Log(LOCTEXT("ModifierNodeTagError", "Could not find a linked String node."), Node);
				}
			}
		}

		SurfNode->MeshMorph = MorphTargetName;

		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeMorph->FactorPin()))
		{
			// Checking if it's linked to a Macro or tunnel node
			const UEdGraphPin* FloatPin = GraphTraversal::FindIOPinSourceThroughMacroContext(*ConnectedPin, &GenerationContext.MacroNodesStack);
			bool validStaticFactor = true;

			if (FloatPin)
			{
				UEdGraphNode* floatNode = FloatPin->GetOwningNode();
				if (const UCustomizableObjectNodeFloatParameter* floatParameterNode = Cast<UCustomizableObjectNodeFloatParameter>(floatNode))
				{
					if (floatParameterNode->DefaultValue < -1.0f || floatParameterNode->DefaultValue > 1.0f)
					{
						validStaticFactor = false;
						FString msg = FString::Printf(TEXT("Mesh morph nodes only accept factors between -1.0 and 1.0 inclusive but the default value of the float parameter node is (%f). Factor will be ignored."), floatParameterNode->DefaultValue);
						GenerationContext.Log(FText::FromString(msg), Node);
					}
					if (floatParameterNode->ParamUIMetadata.MinimumValue < -1.0f)
					{
						validStaticFactor = false;
						FString msg = FString::Printf(TEXT("Mesh morph nodes only accept factors between -1.0 and 1.0 inclusive but the minimum UI value for the input float parameter node is (%f). Factor will be ignored."), floatParameterNode->ParamUIMetadata.MinimumValue);
						GenerationContext.Log(FText::FromString(msg), Node);
					}
					if (floatParameterNode->ParamUIMetadata.MaximumValue > 1.0f)
					{
						validStaticFactor = false;
						FString msg = FString::Printf(TEXT("Mesh morph nodes only accept factors between -1.0 and 1.0 inclusive but the maximum UI value for the input float parameter node is (%f). Factor will be ignored."), floatParameterNode->ParamUIMetadata.MaximumValue);
						GenerationContext.Log(FText::FromString(msg), Node);
					}
				}
				else if (const UCustomizableObjectNodeFloatConstant* floatConstantNode = Cast<UCustomizableObjectNodeFloatConstant>(floatNode))
				{
					if (floatConstantNode->Value < -1.0f || floatConstantNode->Value > 1.0f)
					{
						validStaticFactor = false;
						FString msg = FString::Printf(TEXT("Mesh morph nodes only accept factors between -1.0 and 1.0 inclusive but the value of the float constant node is (%f). Factor will be ignored."), floatConstantNode->Value);
						GenerationContext.Log(FText::FromString(msg), Node);
					}
				}
			}

			// If is a valid factor, continue the Generation
			if (validStaticFactor)
			{
				mu::Ptr<mu::NodeScalar> FactorNode = GenerateMutableSourceFloat(ConnectedPin, GenerationContext);
				SurfNode->MorphFactor = FactorNode;
			}
		}

		GenerationContext.MeshGenerationFlags.Pop();
	}

	else if (const UCustomizableObjectNodeModifierTransformInMesh* TypedNodeTransformMesh = Cast<UCustomizableObjectNodeModifierTransformInMesh>(Node))
	{
		const EMutableMeshConversionFlags ModifiersMeshFlags =
			EMutableMeshConversionFlags::IgnoreSkinning |
			EMutableMeshConversionFlags::IgnorePhysics |
			EMutableMeshConversionFlags::IgnoreMorphs |
			EMutableMeshConversionFlags::DoNotCreateMeshMetadata;
		GenerationContext.MeshGenerationFlags.Push(ModifiersMeshFlags);

		// MeshTransformInMesh can be connected to multiple objects, so the compiled NodeModifierMeshTransformInMesh
		// needs to be different for each object. If it were added to the Generated cache, all the objects would get the same.
		bCacheNode = false;

		mu::Ptr<mu::NodeModifierMeshTransformInMesh> TransformNode = new mu::NodeModifierMeshTransformInMesh();
		Result = TransformNode;

		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeTransformMesh->GetTransformPin()))
		{
			TransformNode->MatrixNode = GenerateMutableSourceTransform(ConnectedPin, GenerationContext);
		}

		// If no bounding mesh is provided, we transform the entire mesh.
		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeTransformMesh->GetBoundingMeshPin()))
		{
			mu::Ptr<mu::NodeMesh> BoundingMesh = GenerateMutableSourceMesh(ConnectedPin, GenerationContext, FMutableSourceMeshData(), false, true);

			if (FMatrix Matrix = TypedNodeTransformMesh->BoundingMeshTransform.ToMatrixWithScale(); Matrix != FMatrix::Identity)
			{
				mu::Ptr<mu::NodeMeshTransform> TransformMesh = new mu::NodeMeshTransform();
				TransformMesh->Source = BoundingMesh;

				TransformMesh->Transform = FMatrix44f(Matrix);
				BoundingMesh = TransformMesh;
			}

			TransformNode->BoundingMesh = BoundingMesh;
		}

		TransformNode->MultipleTagsPolicy = TypedNodeTransformMesh->MultipleTagPolicy;
		TransformNode->RequiredTags = TypedNodeTransformMesh->GetNodeRequiredTags(&GenerationContext.MacroNodesStack);

		GenerationContext.MeshGenerationFlags.Pop();
	}

	else if (const UCustomizableObjectNodeMacroInstance* TypedNodeMacro = Cast<UCustomizableObjectNodeMacroInstance>(Node))
	{
		bCacheNode = false;
		Result = GenerateMutableSourceMacro<mu::NodeModifier>(*Pin, GenerationContext, GenerateMutableSourceModifier);
	}

	else if (const UCustomizableObjectNodeTunnel* TypedNodeTunnel = Cast<UCustomizableObjectNodeTunnel>(Node))
	{
		bCacheNode = false;
		Result = GenerateMutableSourceMacro<mu::NodeModifier>(*Pin, GenerationContext, GenerateMutableSourceModifier);
	}

	else
	{
		GenerationContext.Log(LOCTEXT("UnimplementedNode", "Node type not implemented yet."), Node);
	}

	if (Result)
	{
		Result->SetMessageContext(Node);

		int32 ComponentId = GenerationContext.ComponentNames.IndexOfByKey(GenerationContext.CurrentMeshComponent);
		check(ComponentId>=0);
		Result->RequiredComponentId = ComponentId;
	}

	if (bCacheNode)
	{
		GenerationContext.Generated.Add(Key, FGeneratedData(Node, Result));
	}
	GenerationContext.GeneratedNodes.Add(Node);
	

	return Result;
}

#undef LOCTEXT_NAMESPACE

