// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/GenerateMutableSource/GenerateMutableSourceImage.h"

#include "Engine/TextureLODSettings.h"

#include "MuCOE/CustomizableObjectCompiler.h"
#include "MuCOE/CustomizableObjectGraph.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSourceColor.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSourceFloat.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSourceMesh.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSourceProjector.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSourceTable.h"
#include "MuCOE/GraphTraversal.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMacroInstance.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMesh.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTable.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMaterial.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTexture.h"
#include "MuCOE/Nodes/CustomizableObjectNodePassThroughTexture.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTextureBinarise.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTextureColourMap.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTextureFromChannels.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTextureFromColor.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTextureFromFloats.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTextureInterpolate.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTextureInvert.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTextureLayer.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTextureParameter.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTextureProject.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTextureSwitch.h"
#include "MuCOE/Nodes/CustomizableObjectNodePassThroughTextureSwitch.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTextureToChannels.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTextureTransform.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTextureSaturate.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTextureVariation.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTunnel.h"
#include "MuCOE/UnrealEditorPortabilityHelpers.h"
#include "MuCO/UnrealToMutableTextureConversionUtils.h"
#include "MuT/NodeImageBinarise.h"
#include "MuT/NodeImageColourMap.h"
#include "MuT/NodeImageFormat.h"
#include "MuT/NodeImageInterpolate.h"
#include "MuT/NodeImageInvert.h"
#include "MuT/NodeImageLayer.h"
#include "MuT/NodeImageLayerColour.h"
#include "MuT/NodeImageParameter.h"
#include "MuT/NodeImagePlainColour.h"
#include "MuT/NodeColourFromScalars.h"
#include "MuT/NodeImageProject.h"
#include "MuT/NodeImageResize.h"
#include "MuT/NodeImageSaturate.h"
#include "MuT/NodeImageSwitch.h"
#include "MuT/NodeImageSwizzle.h"
#include "MuT/NodeImageTable.h"
#include "MuT/NodeImageTransform.h"
#include "MuT/NodeImageVariation.h"
#include "MuT/NodeImageConstant.h"
#include "MuT/NodeScalarConstant.h"

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


mu::Ptr<mu::NodeImage> ResizeTextureByNumMips(const mu::Ptr<mu::NodeImage>& ImageConstant, int32 MipsToSkip)
{
	if (MipsToSkip > 0)
	{
		mu::Ptr<mu::NodeImageResize> ImageResize = new mu::NodeImageResize();
		ImageResize->Base = ImageConstant;
		ImageResize->bRelative = true;
		const float Factor = FMath::Pow(0.5f, MipsToSkip);
		ImageResize->SizeX = Factor;
		ImageResize->SizeY = Factor;
		return ImageResize;
	}

	return ImageConstant;
}


mu::Ptr<mu::NodeImage> GenerateMutableSourceImage(const UEdGraphPin* Pin, FMutableGraphGenerationContext& GenerationContext, int32 ReferenceTextureSize)
{
	check(Pin)
	RETURN_ON_CYCLE(*Pin, GenerationContext)

	CheckNumOutputs(*Pin, GenerationContext);
	
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	UCustomizableObjectNode* Node = CastChecked<UCustomizableObjectNode>(Pin->GetOwningNode());

	const FGeneratedKey Key(reinterpret_cast<void*>(&GenerateMutableSourceImage), *Pin, *Node, GenerationContext, true);
	if (const FGeneratedData* Generated = GenerationContext.Generated.Find(Key))
	{
		return static_cast<mu::NodeImage*>(Generated->Node.get());
	}

	if (Node->IsNodeOutDatedAndNeedsRefresh())
	{
		Node->SetRefreshNodeWarning();
	}

	// Bool that determines if a node can be added to the cache of nodes.
	// Most nodes need to be added to the cache but there are some that don't. For exampel, MacroInstanceNodes
	bool bCacheNode = true;

	mu::Ptr<mu::NodeImage> Result;

	const FCompilationOptions& CompilationOptions = GenerationContext.CompilationContext.Options;

	if (const UCustomizableObjectNodeTexture* TypedNodeTex = Cast<UCustomizableObjectNodeTexture>(Node))
	{
		UTexture2D* BaseTexture = TypedNodeTex->Texture;
		if (BaseTexture)
		{
			// Check the specific image cache
			FGeneratedImageKey ImageKey = FGeneratedImageKey(Pin);
			mu::Ptr<mu::NodeImage> ImageNode;
			
			const uint32 MipsToSkip = ComputeLODBiasForTexture(GenerationContext, *BaseTexture, nullptr, ReferenceTextureSize);

			if (mu::Ptr<mu::NodeImage>* Cached = GenerationContext.GeneratedImages.Find(ImageKey))
			{
				ImageNode = *Cached;
			}
			else
			{
				TSharedPtr<mu::FImage> ImageConstant = GenerateImageConstant(BaseTexture, GenerationContext, false);

				mu::Ptr<mu::NodeImageConstant> ConstantImageNode = new mu::NodeImageConstant;
				ConstantImageNode->SetValue( ImageConstant );
				
				const UTextureLODSettings& TextureLODSettings = GenerationContext.CompilationContext.Options.TargetPlatform->GetTextureLODSettings();
				const FTextureLODGroup& LODGroupInfo = TextureLODSettings.GetTextureLODGroup(BaseTexture->LODGroup);

				ConstantImageNode->SourceDataDescriptor.OptionalMaxLODSize = LODGroupInfo.OptionalMaxLODSize;
				ConstantImageNode->SourceDataDescriptor.OptionalLODBias = LODGroupInfo.OptionalLODBias;
				ConstantImageNode->SourceDataDescriptor.NumNonOptionalLODs = CompilationOptions.MinDiskMips;

				const FString TextureName = GetNameSafe(BaseTexture).ToLower();
				ConstantImageNode->SourceDataDescriptor.SourceId = CityHash32(reinterpret_cast<const char*>(*TextureName), TextureName.Len() * sizeof(FString::ElementType));

				ImageNode = ConstantImageNode;

				GenerationContext.GeneratedImages.Add(ImageKey, ImageNode);
			}

			Result = ResizeTextureByNumMips(ImageNode, MipsToSkip);
		}
		else
		{
			GenerationContext.Log(LOCTEXT("MissingImage", "Missing image in texture node."), Node, EMessageSeverity::Warning);
		}
	}

	else if (const UCustomizableObjectNodeTextureParameter* TypedNodeParam = Cast<UCustomizableObjectNodeTextureParameter>(Node))
	{
		mu::Ptr<mu::NodeImageParameter> TextureNode = new mu::NodeImageParameter();

		TextureNode->Name = TypedNodeParam->GetParameterName(&GenerationContext.MacroNodesStack);
		TextureNode->UID = GenerationContext.GetNodeIdUnique(Node).ToString();

		if (TypedNodeParam->DefaultValue)
		{
			TextureNode->DefaultValue = FName(TypedNodeParam->DefaultValue->GetPathName());
		}

		GenerationContext.ParameterUIDataMap.Add(TypedNodeParam->GetParameterName(&GenerationContext.MacroNodesStack), FMutableParameterData(
			TypedNodeParam->ParamUIMetadata,
			EMutableParameterType::Texture));

		// Force the same format that the default texture if any.
		mu::Ptr<mu::NodeImageFormat> FormatNode = new mu::NodeImageFormat();
		// \TODO: Take it from default?
		//if (TypedNodeParam->DefaultValue)
		//{
		//	FormatNode->SetFormat(mu::EImageFormat::RGBA_UByte);
		//}
		//else
		{
			// Force an "easy format" on the texture.
			FormatNode->Format = mu::EImageFormat::RGBA_UByte;
		}
		FormatNode->Source = TextureNode;

		mu::Ptr<mu::NodeImageResize> ResizeNode = new mu::NodeImageResize();
		ResizeNode->Base = FormatNode;
		ResizeNode->bRelative = false;

		FUintVector2 TextureSize(TypedNodeParam->TextureSizeX, TypedNodeParam->TextureSizeY);

		const UTexture2D* ReferenceTexture = TypedNodeParam->ReferenceValue;
		if (ReferenceTexture)
		{
			const uint32 LODBias = ComputeLODBiasForTexture(GenerationContext, *TypedNodeParam->ReferenceValue, ReferenceTexture, ReferenceTextureSize);
			TextureSize.X = FMath::Max(ReferenceTexture->Source.GetSizeX() >> LODBias, 1);
			TextureSize.Y = FMath::Max(ReferenceTexture->Source.GetSizeY() >> LODBias, 1);
		}
		else
		{
			const int32 MaxNodeTextureSize = FMath::Max(TypedNodeParam->TextureSizeX, TypedNodeParam->TextureSizeY);
			if (MaxNodeTextureSize <= 0)
			{
				TextureSize.X = TextureSize.Y = 1;
				GenerationContext.Log(LOCTEXT("TextureParameterSize0", "Texture size not specified. Add a reference texture or set a valid value to the Texture Size variables."), Node);
			}
			else if (ReferenceTextureSize > 0 && ReferenceTextureSize < MaxNodeTextureSize)
			{
				const uint32 MipsToSkip = FMath::CeilLogTwo(MaxNodeTextureSize) - FMath::CeilLogTwo(ReferenceTextureSize);
				TextureSize.X = FMath::Max(TextureSize.X >> MipsToSkip, (uint32)1);
				TextureSize.Y = FMath::Max(TextureSize.Y >> MipsToSkip, (uint32)1);
			}
		}

		ResizeNode->SizeX = TextureSize.X;
		ResizeNode->SizeY = TextureSize.Y;

		Result = ResizeNode;
	}

	else if (const UCustomizableObjectNodeMesh* TypedNodeMesh = Cast<UCustomizableObjectNodeMesh>(Node))
	{
		mu::Ptr<mu::NodeImageConstant> ImageNode = new mu::NodeImageConstant();
		Result = ImageNode;

		UTexture2D* Texture = TypedNodeMesh->FindTextureForPin(Pin);

		if (Texture)
		{
			TSharedPtr<mu::FImage> ImageConstant = GenerateImageConstant(Texture, GenerationContext, false);
			ImageNode->SetValue(ImageConstant);

			const uint32 MipsToSkip = ComputeLODBiasForTexture(GenerationContext, *Texture, nullptr, ReferenceTextureSize);
			Result = ResizeTextureByNumMips(ImageNode, MipsToSkip);

			const UTextureLODSettings& TextureLODSettings = GenerationContext.CompilationContext.Options.TargetPlatform->GetTextureLODSettings();
			const FTextureLODGroup& LODGroupInfo = TextureLODSettings.GetTextureLODGroup(Texture->LODGroup);

			ImageNode->SourceDataDescriptor.OptionalMaxLODSize = LODGroupInfo.OptionalMaxLODSize;
			ImageNode->SourceDataDescriptor.OptionalLODBias = LODGroupInfo.OptionalLODBias;
			ImageNode->SourceDataDescriptor.NumNonOptionalLODs = CompilationOptions.MinDiskMips;

			const FString TextureName = GetNameSafe(Texture).ToLower();
			ImageNode->SourceDataDescriptor.SourceId = CityHash32(reinterpret_cast<const char*>(*TextureName), TextureName.Len() * sizeof(FString::ElementType));
		}
		else
		{
			Result = mu::Ptr<mu::NodeImage>();
		}
	}

	else if (const UCustomizableObjectNodeTextureInterpolate* TypedNodeInterp = Cast<UCustomizableObjectNodeTextureInterpolate>(Node))
	{
		mu::Ptr<mu::NodeImageInterpolate> ImageNode = new mu::NodeImageInterpolate();
		Result = ImageNode;

		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeInterp->FactorPin()))
		{
			mu::Ptr<mu::NodeScalar> FactorNode = GenerateMutableSourceFloat(ConnectedPin, GenerationContext);
			ImageNode->Factor = FactorNode;
		}

		int currentTarget = 0;
		for (int LayerIndex = 0; LayerIndex < TypedNodeInterp->GetNumTargets(); ++LayerIndex)
		{
			if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeInterp->Targets(LayerIndex)))
			{
				mu::Ptr<mu::NodeImage> TargetNode = GenerateMutableSourceImage(ConnectedPin, GenerationContext, ReferenceTextureSize);

				if (TargetNode)
				{
					ImageNode->Targets.SetNum(currentTarget + 1);
					ImageNode->Targets[currentTarget] = TargetNode;
					++currentTarget;
				}
			}
		}
	}

	else if (const UCustomizableObjectNodeTextureLayer* TypedNodeLayer = Cast<UCustomizableObjectNodeTextureLayer>(Node))
	{
		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeLayer->BasePin()))
		{
			Result = GenerateMutableSourceImage(ConnectedPin, GenerationContext, ReferenceTextureSize);
		}

		for (int LayerIndex = 0; LayerIndex < TypedNodeLayer->GetNumLayers(); ++LayerIndex)
		{
			if (const UEdGraphPin* OtherPin = FollowInputPin(*TypedNodeLayer->LayerPin(LayerIndex)))
			{
				mu::Ptr<mu::NodeImage> MaskNode = nullptr;
				if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeLayer->MaskPin(LayerIndex)))
				{
					MaskNode = GenerateMutableSourceImage(ConnectedPin, GenerationContext, ReferenceTextureSize);
				}

				mu::EBlendType Type = mu::EBlendType::BT_BLEND;
				switch (TypedNodeLayer->Layers[LayerIndex].Effect)
				{
				case COTLE_MODULATE: Type = mu::EBlendType::BT_BLEND; break;
				case COTLE_MULTIPLY: Type = mu::EBlendType::BT_MULTIPLY; break;
				case COTLE_SOFTLIGHT: Type = mu::EBlendType::BT_SOFTLIGHT; break;
				case COTLE_HARDLIGHT: Type = mu::EBlendType::BT_HARDLIGHT; break;
				case COTLE_DODGE: Type = mu::EBlendType::BT_DODGE; break;
				case COTLE_BURN: Type = mu::EBlendType::BT_BURN; break;
				case COTLE_SCREEN: Type = mu::EBlendType::BT_SCREEN; break;
				case COTLE_OVERLAY: Type = mu::EBlendType::BT_OVERLAY; break;
				case COTLE_ALPHA_OVERLAY: Type = mu::EBlendType::BT_LIGHTEN; break;
				case COTLE_NORMAL_COMBINE: Type = mu::EBlendType::BT_NORMAL_COMBINE; break;
				default:
					GenerationContext.Log(LOCTEXT("UnsupportedImageEffect", "Texture layer effect not supported. Setting to 'Blend'."), Node);
					break;
				}

				if (Type == mu::EBlendType::BT_BLEND && !MaskNode)
				{
					GenerationContext.Log(LOCTEXT("ModulateWithoutMask", "Texture layer effect uses Modulate without a mask. It will replace everything below it!"), Node);
				}
				
				if (OtherPin->PinType.PinCategory == Schema->PC_Image)
				{
					mu::Ptr<mu::NodeImage> BlendNode = GenerateMutableSourceImage(OtherPin, GenerationContext, ReferenceTextureSize);

					mu::Ptr<mu::NodeImageLayer> LayerNode = new mu::NodeImageLayer;
					LayerNode->Type = Type;
					LayerNode->Base = Result;
					LayerNode->Blended = BlendNode;
					LayerNode->Mask = MaskNode;
					Result = LayerNode;
				}

				else if (OtherPin->PinType.PinCategory == Schema->PC_Color)
				{
					mu::Ptr<mu::NodeColour> ColorNode = GenerateMutableSourceColor(OtherPin, GenerationContext);

					mu::Ptr<mu::NodeImageLayerColour> LayerNode = new mu::NodeImageLayerColour;
					LayerNode->Type = Type;
					LayerNode->Base = Result;
					LayerNode->Colour = ColorNode;
					LayerNode->Mask = MaskNode;
					Result = LayerNode;
				}

				// We need it here because we create multiple nodes.
				Result->SetMessageContext(Node);
			}
		}
	}

	else if (const UCustomizableObjectNodeTextureSwitch* TypedNodeTextureSwitch = Cast<UCustomizableObjectNodeTextureSwitch>(Node))
	{
		Result = [&]()
		{
			const UEdGraphPin* SwitchParameter = TypedNodeTextureSwitch->SwitchParameter();

			// Check Switch Parameter arity preconditions.
			if (const UEdGraphPin* ConnectedPin = FollowInputPin(*SwitchParameter))
			{
				mu::Ptr<mu::NodeScalar> SwitchParam = GenerateMutableSourceFloat(ConnectedPin, GenerationContext);
				// Switch Param not generated
				if (!SwitchParam)
				{
					const FText Message = LOCTEXT("FailedToGenerateSwitchParam", "Could not generate switch enum parameter. Please refesh the switch node and connect an enum.");
					GenerationContext.Log(Message, Node);
					
					return Result;
				}

				if (SwitchParam->GetType() != mu::NodeScalarEnumParameter::GetStaticType())
				{
					const FText Message = LOCTEXT("WrongSwitchParamType", "Switch parameter of incorrect type.");
					GenerationContext.Log(Message, Node);

					return Result;
				}

				const int32 NumSwitchOptions = TypedNodeTextureSwitch->GetNumElements();

				mu::NodeScalarEnumParameter* EnumParameter = static_cast<mu::NodeScalarEnumParameter*>(SwitchParam.get());
				if (NumSwitchOptions != EnumParameter->Options.Num())
				{
					const FText Message = LOCTEXT("MismatchedSwitch", "Switch enum and switch node have different number of options. Please refresh the switch node to make sure the outcomes are labeled properly.");
					GenerationContext.Log(Message, Node);
				}

				mu::Ptr<mu::NodeImageSwitch> SwitchNode = new mu::NodeImageSwitch;
				SwitchNode->Parameter = SwitchParam;
				SwitchNode->Options.SetNum(NumSwitchOptions);

				for (int SelectorIndex = 0; SelectorIndex < NumSwitchOptions; ++SelectorIndex)
				{
					if (const UEdGraphPin* TexturePin = FollowInputPin(*TypedNodeTextureSwitch->GetElementPin(SelectorIndex)))
					{
						SwitchNode->Options[SelectorIndex] = GenerateMutableSourceImage(TexturePin, GenerationContext, ReferenceTextureSize);
					}
					else
					{
						const FText Message = LOCTEXT("MissingTexture", "Unable to generate texture switch node. Required connection not found.");
						GenerationContext.Log(Message, Node);
						return Result;
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

	else if (const UCustomizableObjectNodeTextureVariation* TypedNodeImageVar = Cast<const UCustomizableObjectNodeTextureVariation>(Node))
	{
		// UCustomizableObjectNodePassThroughTextureVariation nodes are also handled here
		mu::Ptr<mu::NodeImageVariation> TextureNode = new mu::NodeImageVariation();
		Result = TextureNode;

		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeImageVar->DefaultPin()))
		{
			mu::Ptr<mu::NodeImage> ChildNode = GenerateMutableSourceImage(ConnectedPin, GenerationContext, ReferenceTextureSize);
			if (ChildNode)
			{
				TextureNode->DefaultImage = ChildNode;
			}
			else
			{
				GenerationContext.Log(LOCTEXT("TextureFailed", "Texture generation failed."), Node);
			}
		}

		const int32 NumVariations = TypedNodeImageVar->GetNumVariations();
		TextureNode->Variations.SetNum(NumVariations);
		
		for (int32 VariationIndex = 0; VariationIndex < NumVariations; ++VariationIndex)
		{
			const UEdGraphPin* VariationPin = TypedNodeImageVar->VariationPin(VariationIndex);
			if (!VariationPin)
			{
				continue;
			}

			TextureNode->Variations[VariationIndex].Tag = TypedNodeImageVar->GetVariationTag(VariationIndex, &GenerationContext.MacroNodesStack);

			if (const UEdGraphPin* ConnectedPin = FollowInputPin(*VariationPin))
			{
				mu::Ptr<mu::NodeImage> ChildNode = GenerateMutableSourceImage(ConnectedPin, GenerationContext, ReferenceTextureSize);
				TextureNode->Variations[VariationIndex].Image = ChildNode;
			}
		}
	}

	else if (const UCustomizableObjectNodeTextureFromColor* TypedNodeFromColor = Cast<UCustomizableObjectNodeTextureFromColor>(Node))
	{
		mu::Ptr<mu::NodeColour> color;

		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeFromColor->ColorPin()))
		{
			color = GenerateMutableSourceColor(ConnectedPin, GenerationContext);
		}

		mu::Ptr<mu::NodeImagePlainColour> ImageFromColour = new mu::NodeImagePlainColour;
		Result = ImageFromColour;

		if (color)
		{
			ImageFromColour->Colour = color;
		}

		if (ReferenceTextureSize > 0)
		{
			ImageFromColour->SizeX = ReferenceTextureSize;
			ImageFromColour->SizeY = ReferenceTextureSize;
		}
	}

	else if (const UCustomizableObjectNodeTextureFromFloats* TypedNodeFromFloats = Cast<UCustomizableObjectNodeTextureFromFloats>(Node))
	{		
		mu::Ptr<mu::NodeColourFromScalars> Color = new mu::NodeColourFromScalars();

		if (const UEdGraphPin* RPin = FollowInputPin(*TypedNodeFromFloats->RPin()))
		{
			Color->X = GenerateMutableSourceFloat(RPin, GenerationContext);
		}

		if (const UEdGraphPin* GPin = FollowInputPin(*TypedNodeFromFloats->GPin()))
		{
			Color->Y = GenerateMutableSourceFloat(GPin, GenerationContext);
		}

		if (const UEdGraphPin* BPin = FollowInputPin(*TypedNodeFromFloats->BPin()))
		{
			Color->Z = GenerateMutableSourceFloat(BPin, GenerationContext);
		}

		if (const UEdGraphPin* APin = FollowInputPin(*TypedNodeFromFloats->APin()))
		{
			Color->W = GenerateMutableSourceFloat(APin, GenerationContext);
		}

		mu::Ptr<mu::NodeImagePlainColour> ImageFromColour = new mu::NodeImagePlainColour;
		Result = ImageFromColour;

		if (Color)
		{
			ImageFromColour->Colour = Color;
		}

		if (ReferenceTextureSize > 0)
		{
			ImageFromColour->SizeX = ReferenceTextureSize;
			ImageFromColour->SizeY = ReferenceTextureSize;
		}
	}

	else if (const UCustomizableObjectNodeTextureFromChannels* TypedNodeFrom = Cast<UCustomizableObjectNodeTextureFromChannels>(Node))
	{
		mu::Ptr<mu::NodeImage> RNode, GNode, BNode, ANode;
		bool RGB = false;

		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeFrom->RPin()))
		{
			RNode = GenerateMutableSourceImage(ConnectedPin, GenerationContext, ReferenceTextureSize);
			RGB = true;
		}
		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeFrom->GPin()))
		{
			GNode = GenerateMutableSourceImage(ConnectedPin, GenerationContext, ReferenceTextureSize);
			RGB = true;
		}
		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeFrom->BPin()))
		{
			BNode = GenerateMutableSourceImage(ConnectedPin, GenerationContext, ReferenceTextureSize);
			RGB = true;
		}
		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeFrom->APin()))
		{
			ANode = GenerateMutableSourceImage(ConnectedPin, GenerationContext, ReferenceTextureSize);
		}

		mu::Ptr<mu::NodeImageSwizzle> SwizzleNode = new mu::NodeImageSwizzle;
		Result = SwizzleNode;

		if (RGB && ANode)
		{
			SwizzleNode->SetFormat(mu::EImageFormat::RGBA_UByte);
			SwizzleNode->Sources[0] = RNode;
			SwizzleNode->SourceChannels[0] = 0;
			SwizzleNode->Sources[1] = GNode;
			SwizzleNode->SourceChannels[1] = 0;
			SwizzleNode->Sources[2] = BNode;
			SwizzleNode->SourceChannels[2] = 0;
			SwizzleNode->Sources[3] = ANode;
			SwizzleNode->SourceChannels[3] = 0;
		}
		else if (RGB)
		{
			SwizzleNode->SetFormat(mu::EImageFormat::RGB_UByte);
			SwizzleNode->Sources[0] = RNode;
			SwizzleNode->SourceChannels[0] = 0;
			SwizzleNode->Sources[1] = GNode;
			SwizzleNode->SourceChannels[1] = 0;
			SwizzleNode->Sources[2] = BNode;
			SwizzleNode->SourceChannels[2] = 0;
		}
		else if (RNode)
		{
			SwizzleNode->SetFormat(mu::EImageFormat::L_UByte);
			SwizzleNode->Sources[0] = RNode;
			SwizzleNode->SourceChannels[0] = 0;
		}
		else if (ANode)
		{
			SwizzleNode->SetFormat(mu::EImageFormat::L_UByte);
			SwizzleNode->Sources[0] = ANode;
			SwizzleNode->SourceChannels[0] = 0;
		}
	}

	else if (const UCustomizableObjectNodeTextureToChannels* TypedNodeTo = Cast<UCustomizableObjectNodeTextureToChannels>(Node))
	{
		mu::Ptr<mu::NodeImage> BaseNode;
		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeTo->InputPin()))
		{
			BaseNode = GenerateMutableSourceImage(ConnectedPin, GenerationContext, ReferenceTextureSize);
		}

		mu::Ptr<mu::NodeImageSwizzle> SwizzleNode = new mu::NodeImageSwizzle;
		Result = SwizzleNode;
		SwizzleNode->SetFormat(mu::EImageFormat::L_UByte);
		SwizzleNode->Sources[0] = BaseNode;

		if (Pin == TypedNodeTo->RPin())
		{
			SwizzleNode->SourceChannels[0] = 0;
		}
		else if (Pin == TypedNodeTo->GPin())
		{
			SwizzleNode->SourceChannels[0] = 1;
		}
		else if (Pin == TypedNodeTo->BPin())
		{
			SwizzleNode->SourceChannels[0] = 2;
		}
		else if (Pin == TypedNodeTo->APin())
		{
			SwizzleNode->SourceChannels[0] = 3;
		}
		else
		{
			check(false);
		}
	}

	else if (const UCustomizableObjectNodeTextureProject* TypedNodeProject = Cast<UCustomizableObjectNodeTextureProject>(Node))
	{
		mu::Ptr<mu::NodeImageProject> ImageNode = new mu::NodeImageProject();
		Result = ImageNode;

		if (!FollowInputPin(*TypedNodeProject->MeshPin()))
		{
			GenerationContext.Log(LOCTEXT("MissingMeshInProjector", "Texture projector does not have a Mesh. It will be ignored. "), Node, EMessageSeverity::Warning);
			Result = nullptr;
		}

		ImageNode->Layout = TypedNodeProject->Layout;

		// Calculate the max TextureSize allowed using the ReferenceTextureSize and the Reference texture from the node
		int32 MaxReferenceTextureSizeInGame = ReferenceTextureSize;
		if (TypedNodeProject->ReferenceTexture)
		{
			const UTextureLODSettings& TextureLODSettings = CompilationOptions.TargetPlatform->GetTextureLODSettings();
			MaxReferenceTextureSizeInGame = GetTextureSizeInGame(*TypedNodeProject->ReferenceTexture, TextureLODSettings);
		}

		FUintVector2 TextureSize(TypedNodeProject->TextureSizeX, TypedNodeProject->TextureSizeY);

		// Max TextureSize allowed
		const int32 MaxProjectedTextureSizeInGame = ReferenceTextureSize > 0 && ReferenceTextureSize < MaxReferenceTextureSizeInGame ? ReferenceTextureSize : MaxReferenceTextureSizeInGame;

		const int32 ProjectorNodeTextureSize = FMath::Max(TextureSize.X, TextureSize.Y);
		if (ProjectorNodeTextureSize > 0 && MaxProjectedTextureSizeInGame > 0 && ProjectorNodeTextureSize > MaxProjectedTextureSizeInGame)
		{
			const int32 NumMips = FMath::CeilLogTwo(ProjectorNodeTextureSize) + 1;
			const int32 MaxNumMips = FMath::CeilLogTwo(MaxProjectedTextureSizeInGame) + 1;

			TextureSize.X = TextureSize.X >> (NumMips - MaxNumMips);
			TextureSize.Y = TextureSize.Y >> (NumMips - MaxNumMips);
		}

		ImageNode->ImageSize = TextureSize;

		ImageNode->bEnableTextureSeamCorrection = TypedNodeProject->bEnableTextureSeamCorrection;
		ImageNode->bIsRGBFadingEnabled = TypedNodeProject->bEnableAngleFadeOutForRGB;
		ImageNode->bIsAlphaFadingEnabled = TypedNodeProject->bEnableAngleFadeOutForAlpha;
		ImageNode->SamplingMethod = Invoke(
			[](ETextureProjectSamplingMethod Method) -> mu::ESamplingMethod 
			{
				switch(Method)
				{
				case ETextureProjectSamplingMethod::Point:    return mu::ESamplingMethod::Point;
				case ETextureProjectSamplingMethod::BiLinear: return mu::ESamplingMethod::BiLinear;
				default: { check(false); return mu::ESamplingMethod::Point; }
				}
			}, 
			TypedNodeProject->SamplingMethod);

		ImageNode->MinFilterMethod = Invoke(
			[](ETextureProjectMinFilterMethod Method) -> mu::EMinFilterMethod 
			{
				switch(Method)
				{
				case ETextureProjectMinFilterMethod::None:               return mu::EMinFilterMethod::None;
				case ETextureProjectMinFilterMethod::TotalAreaHeuristic: return mu::EMinFilterMethod::TotalAreaHeuristic;
				default: { check(false); return mu::EMinFilterMethod::None; }
				}
			}, 
			TypedNodeProject->MinFilterMethod);

		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeProject->AngleFadeStartPin()))
		{
			mu::Ptr<mu::NodeScalar> ScalarNode = GenerateMutableSourceFloat(ConnectedPin, GenerationContext);
			ImageNode->AngleFadeStart = ScalarNode;
		}
		else
		{
			mu::Ptr<mu::NodeScalarConstant> ScalarNode = new mu::NodeScalarConstant;
			float value = FCString::Atof(*TypedNodeProject->AngleFadeStartPin()->DefaultValue);
			ScalarNode->Value = value;
			ImageNode->AngleFadeStart = ScalarNode;
		}

		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeProject->AngleFadeEndPin()))
		{
			mu::Ptr<mu::NodeScalar> ScalarNode = GenerateMutableSourceFloat(ConnectedPin, GenerationContext);
			ImageNode->AngleFadeEnd = ScalarNode;
		}
		else
		{
			mu::Ptr<mu::NodeScalarConstant> ScalarNode = new mu::NodeScalarConstant;
			float value = FCString::Atof(*TypedNodeProject->AngleFadeEndPin()->DefaultValue);
			ScalarNode->Value = value;
			ImageNode->AngleFadeEnd = ScalarNode;
		}

		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeProject->MeshPin()))
		{
			FLayoutGenerationFlags LayoutGenerationFlags;
			LayoutGenerationFlags.TexturePinModes.Init(EPinMode::Mutable, TEXSTREAM_MAX_NUM_UVCHANNELS);
			GenerationContext.LayoutGenerationFlags.Push(LayoutGenerationFlags);
			mu::Ptr<mu::NodeMesh> MeshNode = GenerateMutableSourceMesh(ConnectedPin, GenerationContext, FMutableSourceMeshData(), false, false);
			GenerationContext.LayoutGenerationFlags.Pop();
			ImageNode->Mesh = MeshNode;
		}

		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeProject->MeshMaskPin()))
		{
			mu::Ptr<mu::NodeImage> MeshMaskNode = GenerateMutableSourceImage(ConnectedPin, GenerationContext, ReferenceTextureSize);
			ImageNode->Mask = MeshMaskNode;
		}

		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeProject->ProjectorPin()))
		{
			mu::Ptr<mu::NodeProjector> ProjectorNode = GenerateMutableSourceProjector(ConnectedPin, GenerationContext);
			ImageNode->Projector = ProjectorNode;
		}

		int TexIndex = -1;// TypedNodeProject->OutputPins.Find((UEdGraphPin*)Pin);

		for (int32 i = 0; i < TypedNodeProject->GetNumOutputs(); ++i)
		{
			if (TypedNodeProject->OutputPins(i) == Pin)
			{
				TexIndex = i;
			}
		}

		check(TexIndex >= 0 && TexIndex < TypedNodeProject->GetNumTextures());

		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeProject->TexturePins(TexIndex)))
		{
			mu::Ptr<mu::NodeImage> SourceNode = GenerateMutableSourceImage(ConnectedPin, GenerationContext, FMath::Max(TextureSize.X, TextureSize.Y));
			ImageNode->Image = SourceNode;
		}
	}

	else if (const UCustomizableObjectNodeTextureBinarise* TypedNodeTexBin = Cast<UCustomizableObjectNodeTextureBinarise>(Node))
	{
		mu::Ptr<mu::NodeImageBinarise> BinariseNode = new mu::NodeImageBinarise();
		Result = BinariseNode;

		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeTexBin->GetBaseImagePin()))
		{
			mu::Ptr<mu::NodeImage> BaseImageNode = GenerateMutableSourceImage(ConnectedPin, GenerationContext, ReferenceTextureSize);
			BinariseNode->Base = BaseImageNode;
		}

		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeTexBin->GetThresholdPin()))
		{
			mu::Ptr<mu::NodeScalar> ThresholdNode = GenerateMutableSourceFloat(ConnectedPin, GenerationContext);
			BinariseNode->Threshold = ThresholdNode;
		}
	}

	else if (const UCustomizableObjectNodeTextureInvert* TypedNodeTexInv = Cast<UCustomizableObjectNodeTextureInvert>(Node))
	{
		mu::Ptr<mu::NodeImageInvert> InvertNode = new mu::NodeImageInvert();
		Result = InvertNode;

		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeTexInv->GetBaseImagePin()))
		{
			mu::Ptr<mu::NodeImage> BaseImageNode = GenerateMutableSourceImage(ConnectedPin, GenerationContext, ReferenceTextureSize);
			InvertNode->Base = BaseImageNode;
		}
	}

	else if (const UCustomizableObjectNodeTextureColourMap* TypedNodeColourMap = Cast<UCustomizableObjectNodeTextureColourMap>(Node))
	{
		mu::Ptr<mu::NodeImageColourMap> ColourMapNode = new mu::NodeImageColourMap();

		Result = ColourMapNode;

		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeColourMap->GetMapPin()))
		{
			mu::Ptr<mu::NodeImage> GradientImageNode = GenerateMutableSourceImage(ConnectedPin, GenerationContext, ReferenceTextureSize);
			ColourMapNode->Map = GradientImageNode;
		}

		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeColourMap->GetMaskPin()))
		{
			mu::Ptr<mu::NodeImage> GradientImageNode = GenerateMutableSourceImage(ConnectedPin, GenerationContext, ReferenceTextureSize);
			ColourMapNode->Mask = GradientImageNode;
		}

		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeColourMap->GetBasePin()))
		{
			mu::Ptr<mu::NodeImage> SourceImageNode = GenerateMutableSourceImage(ConnectedPin, GenerationContext, ReferenceTextureSize);
			ColourMapNode->Base = SourceImageNode;
		}
	}

	else if (const UCustomizableObjectNodeTextureTransform* TypedNodeTransform = Cast<UCustomizableObjectNodeTextureTransform>(Node))
	{
		mu::Ptr<mu::NodeImageTransform> TransformNode = new mu::NodeImageTransform();
		Result = TransformNode;
		
		if ( UEdGraphPin* BaseImagePin = FollowInputPin(*TypedNodeTransform->GetBaseImagePin()) )
		{
			mu::Ptr<mu::NodeImage> ImageNode = GenerateMutableSourceImage( BaseImagePin, GenerationContext, ReferenceTextureSize);
			TransformNode->Base = ImageNode;
		}

		if ( UEdGraphPin* OffsetXPin = FollowInputPin(*TypedNodeTransform->GetOffsetXPin()) )
		{
			mu::Ptr<mu::NodeScalar> OffsetXNode = GenerateMutableSourceFloat(OffsetXPin, GenerationContext);
			TransformNode->OffsetX = OffsetXNode;
		}

		if ( UEdGraphPin* OffsetYPin = FollowInputPin(*TypedNodeTransform->GetOffsetYPin()) )
		{
			mu::Ptr<mu::NodeScalar> OffsetYNode = GenerateMutableSourceFloat(OffsetYPin, GenerationContext);
			TransformNode->OffsetY = OffsetYNode;
		}

		if ( UEdGraphPin* ScaleXPin = FollowInputPin(*TypedNodeTransform->GetScaleXPin()) )
		{
			mu::Ptr<mu::NodeScalar> ScaleXNode = GenerateMutableSourceFloat(ScaleXPin, GenerationContext);
			TransformNode->ScaleX = ScaleXNode;
		}

		if ( UEdGraphPin* ScaleYPin = FollowInputPin(*TypedNodeTransform->GetScaleYPin()) )
		{
			mu::Ptr<mu::NodeScalar> ScaleYNode = GenerateMutableSourceFloat(ScaleYPin, GenerationContext);
			TransformNode->ScaleY = ScaleYNode; 
		}

		if ( UEdGraphPin* RotationPin = FollowInputPin(*TypedNodeTransform->GetRotationPin()) )
		{
			mu::Ptr<mu::NodeScalar> RotationNode = GenerateMutableSourceFloat(RotationPin, GenerationContext);
			TransformNode->Rotation = RotationNode; 
		}

		TransformNode->AddressMode = Invoke([&]() 
		{
			switch (TypedNodeTransform->AddressMode)
			{
			case ETextureTransformAddressMode::Wrap:		 return mu::EAddressMode::Wrap;
			case ETextureTransformAddressMode::ClampToEdge:  return mu::EAddressMode::ClampToEdge;
			case ETextureTransformAddressMode::ClampToBlack: return mu::EAddressMode::ClampToBlack;
			default: { check(false); return mu::EAddressMode::None; }
			}
		});

		FUintVector2 TextureSize(TypedNodeTransform->TextureSizeX, TypedNodeTransform->TextureSizeY);

		// Calculate the max TextureSize allowed using the ReferenceTextureSize and the Reference texture from the node
		int32 MaxReferenceTextureSizeInGame = ReferenceTextureSize;
		if (TypedNodeTransform->ReferenceTexture)
		{
			const UTextureLODSettings& TextureLODSettings = CompilationOptions.TargetPlatform->GetTextureLODSettings();
			MaxReferenceTextureSizeInGame = GetTextureSizeInGame(*TypedNodeTransform->ReferenceTexture, TextureLODSettings);
		}

		// Max TextureSize allowed
		const int32 MaxTransformTextureSizeInGame = ReferenceTextureSize > 0 && ReferenceTextureSize < MaxReferenceTextureSizeInGame ? ReferenceTextureSize : MaxReferenceTextureSizeInGame;

		const int32 TransformNodeTextureSize = FMath::Max(TextureSize.X, TextureSize.Y);
		if (TransformNodeTextureSize > 0 && TransformNodeTextureSize > MaxTransformTextureSizeInGame)
		{
			const int32 NumMips = FMath::CeilLogTwo(TransformNodeTextureSize) + 1;
			const int32 MaxNumMips = FMath::CeilLogTwo(MaxTransformTextureSizeInGame) + 1;

			TextureSize.X = TextureSize.X >> (NumMips - MaxNumMips);
			TextureSize.Y = TextureSize.Y >> (NumMips - MaxNumMips);
		}

		TransformNode->bKeepAspectRatio = TypedNodeTransform->bKeepAspectRatio;
		TransformNode->SizeX = TextureSize.X;
		TransformNode->SizeY = TextureSize.Y;
	}

	else if (const UCustomizableObjectNodeTextureSaturate* TypedNodeSaturate = Cast<UCustomizableObjectNodeTextureSaturate>(Node))
	{
		mu::Ptr<mu::NodeImageSaturate> SaturateNode = new mu::NodeImageSaturate();
		Result = SaturateNode;
	
		if ( UEdGraphPin* BaseImagePin = FollowInputPin(*TypedNodeSaturate->GetBaseImagePin()) )
		{
			mu::Ptr<mu::NodeImage> ImageNode = GenerateMutableSourceImage(BaseImagePin, GenerationContext, ReferenceTextureSize);
			SaturateNode->Source = ImageNode;
		}

		if ( UEdGraphPin* FactorPin = FollowInputPin(*TypedNodeSaturate->GetFactorPin()))
		{
			mu::Ptr<mu::NodeScalar> FactorNode = GenerateMutableSourceFloat(FactorPin, GenerationContext);
			SaturateNode->Factor = FactorNode;
		}
	}

	else if (const UCustomizableObjectNodePassThroughTexture* TypedNodePassThroughTex = Cast<UCustomizableObjectNodePassThroughTexture>(Node))
	{
		UTexture* BaseTexture = TypedNodePassThroughTex->PassThroughTexture;
		if (BaseTexture)
		{
			mu::Ptr<mu::NodeImageConstant> ImageNode = new mu::NodeImageConstant();
			Result = ImageNode;

			ImageNode->SetValue(GenerateImageConstant(BaseTexture, GenerationContext, true));
		}
		else
		{
			GenerationContext.Log(LOCTEXT("MissingImagePassThrough", "Missing image in pass-through texture node."), Node);
		}
	}

	else if (const UCustomizableObjectNodePassThroughTextureSwitch* TypedNodePassThroughTextureSwitch = Cast<UCustomizableObjectNodePassThroughTextureSwitch>(Node))
	{
		Result = [&]()
		{
			const UEdGraphPin* SwitchParameter = TypedNodePassThroughTextureSwitch->SwitchParameter();

			// Check Switch Parameter arity preconditions.
			if (const UEdGraphPin* ConnectedPin = FollowInputPin(*SwitchParameter))
			{
				mu::Ptr<mu::NodeScalar> SwitchParam = GenerateMutableSourceFloat(ConnectedPin, GenerationContext);
				// Switch Param not generated
				if (!SwitchParam)
				{
					const FText Message = LOCTEXT("FailedToGenerateSwitchParam", "Could not generate switch enum parameter. Please refesh the switch node and connect an enum.");
					GenerationContext.Log(Message, Node);

					return Result;
				}

				if (SwitchParam->GetType() != mu::NodeScalarEnumParameter::GetStaticType())
				{
					const FText Message = LOCTEXT("WrongSwitchParamType", "Switch parameter of incorrect type.");
					GenerationContext.Log(Message, Node);

					return Result;
				}

				const int32 NumSwitchOptions = TypedNodePassThroughTextureSwitch->GetNumElements();

				mu::NodeScalarEnumParameter* EnumParameter = static_cast<mu::NodeScalarEnumParameter*>(SwitchParam.get());
				if (NumSwitchOptions != EnumParameter->Options.Num())
				{
					const FText Message = LOCTEXT("MismatchedSwitch", "Switch enum and switch node have different number of options. Please refresh the switch node to make sure the outcomes are labeled properly.");
					GenerationContext.Log(Message, Node);
				}

				// TODO: Implement Mutable core pass-through switch nodes
				mu::Ptr<mu::NodeImageSwitch> SwitchNode = new mu::NodeImageSwitch;
				SwitchNode->Parameter = SwitchParam;
				SwitchNode->Options.SetNum(NumSwitchOptions);

				for (int32 SelectorIndex = 0; SelectorIndex < NumSwitchOptions; ++SelectorIndex)
				{
					if (const UEdGraphPin* TexturePin = FollowInputPin(*TypedNodePassThroughTextureSwitch->GetElementPin(SelectorIndex)))
					{
						mu::Ptr<mu::NodeImage> PassThroughImage = GenerateMutableSourceImage(TexturePin, GenerationContext, ReferenceTextureSize);
						SwitchNode->Options[SelectorIndex] = PassThroughImage;
					}
					else
					{
						const FText Message = LOCTEXT("MissingPassThroughTexture", "Unable to generate pass-through texture switch node. Required connection not found.");
						GenerationContext.Log(Message, Node);
						return Result;
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

	// If the node is a plain colour node, generate an image out of it
	else if (Pin->PinType.PinCategory == Schema->PC_Color)
	{
		mu::Ptr<mu::NodeColour> ColorNode = GenerateMutableSourceColor(Pin, GenerationContext);

		mu::Ptr<mu::NodeImagePlainColour> ImageNode = new mu::NodeImagePlainColour;
		ImageNode->SizeX = 16;
		ImageNode->SizeY = 16;
		ImageNode->Colour = ColorNode;
		Result = ImageNode;
	}

	else if (const UCustomizableObjectNodeTable* TypedNodeTable = Cast<UCustomizableObjectNodeTable>(Node))
	{
		if (Pin->PinType.PinCategory == Schema->PC_Color)
		{
			mu::Ptr<mu::NodeColour> ColorNode = GenerateMutableSourceColor(Pin, GenerationContext);
			mu::Ptr<mu::NodeImagePlainColour> ImageNode = new mu::NodeImagePlainColour;

			ImageNode->SizeX = 16;
			ImageNode->SizeY = 16;
			ImageNode->Colour = ColorNode;

			Result = ImageNode;
		}
		else
		{
			TObjectPtr<UDataTable> Table = MutablePrivate::LoadObject(TypedNodeTable->Table);
			TObjectPtr<UScriptStruct> Structure = MutablePrivate::LoadObject(TypedNodeTable->Structure);
			const FString TableName = Table ? GetNameSafe(Table).ToLower() : GetNameSafe(Structure).ToLower();
			const uint32 TableId = CityHash32(reinterpret_cast<const char*>(*TableName), TableName.Len() * sizeof(FString::ElementType));

			// This node will add a checker texture in case of error
			mu::Ptr<mu::NodeImageConstant> EmptyNode = new mu::NodeImageConstant();
			Result = EmptyNode;

			bool bSuccess = true;

			if (Pin->PinType.PinCategory == Schema->PC_MaterialAsset)
			{
				// Material pins have to skip the cache of nodes or they will return always the same column node
				bCacheNode = false;
			}

			UDataTable* DataTable = GetDataTable(TypedNodeTable, GenerationContext);

			if (DataTable)
			{
				FString ColumnName = TypedNodeTable->GetPinColumnName(Pin);
				FProperty* Property = TypedNodeTable->FindPinProperty(*Pin);

				if (!Property)
				{
					FString Msg = FString::Printf(TEXT("Couldn't find the column [%s] in the data table's struct."), *ColumnName);
					GenerationContext.Log(FText::FromString(Msg), Node);

					bSuccess = false;
				}

				UTexture* DefaultTexture = TypedNodeTable->GetColumnDefaultAssetByType<UTexture>(Pin);

				if (bSuccess && Pin->PinType.PinCategory != Schema->PC_MaterialAsset && !DefaultTexture)
				{
					FString Msg = FString::Printf(TEXT("Couldn't find a default value in the data table's struct for the column [%s]. The default value is null or not a supported Texture"), *ColumnName);
					GenerationContext.Log(FText::FromString(Msg), Node);
					
					bSuccess = false;
				}

				if (bSuccess)
				{
					// Generating a new data table if not exists
					mu::Ptr<mu::FTable> GeneratedTable = nullptr;
					GeneratedTable = GenerateMutableSourceTable(DataTable, TypedNodeTable, GenerationContext);

					if (GeneratedTable)
					{
						mu::Ptr<mu::NodeImageTable> ImageTableNode = new mu::NodeImageTable();

						if (Pin->PinType.PinCategory == Schema->PC_MaterialAsset)
						{
							// Material parameters use the Data Table Column Name + Parameter id as mutable column Name to aboid duplicated names (i.e. two MI columns with the same parents but different values).
							ColumnName = Property->GetDisplayNameText().ToString() + GenerationContext.CurrentMaterialTableParameterId;
						}
						else
						{
							// Checking if this pin texture has been used in another table node but with a different texture mode
							const int32 ImageDataIndex = GenerationContext.GeneratedTableImages.Find({ ColumnName, Pin->PinType.PinCategory, GeneratedTable });

							if (ImageDataIndex != INDEX_NONE )
							{
								if (GenerationContext.GeneratedTableImages[ImageDataIndex].PinType != Pin->PinType.PinCategory)
								{
									TArray<const UObject*> Nodes;
									Nodes.Add(TypedNodeTable);
									Nodes.Add(GenerationContext.GeneratedTableImages[ImageDataIndex].TableNode);

									FString Msg = FString::Printf(TEXT("Texture pin [%s] with different texture modes found in more than one table node. This will add multiple times the texture reseource in the final cook."), *ColumnName);
									GenerationContext.Log(FText::FromString(Msg), Nodes);
								}
							}
							else
							{
								GenerationContext.GeneratedTableImages.Add({ColumnName, Pin->PinType.PinCategory, GeneratedTable, TypedNodeTable});
							}

							// Encoding the texture mode of the type to allow different texture modes from the same pin
							if (Pin->PinType.PinCategory == Schema->PC_PassThroughImage)
							{
								ColumnName += "--PassThrough";
							}
						}

						// Generating a new Texture column if not exists
						if (GeneratedTable->FindColumn(ColumnName) == INDEX_NONE)
						{
							int32 Dummy = -1; // TODO MTBL-1512
							bool Dummy2 = false;
							bSuccess = GenerateTableColumn(TypedNodeTable, Pin, GeneratedTable, ColumnName, Property, 
								FMutableSourceMeshData(), Dummy, Dummy, GenerationContext.CurrentLOD, Dummy, Dummy2, GenerationContext);

							if (!bSuccess)
							{
								FString Msg = FString::Printf(TEXT("Failed to generate the mutable table column [%s]"), *ColumnName);
								GenerationContext.Log(FText::FromString(Msg), Node);
							}
						}

						if (bSuccess)
						{
							Result = ImageTableNode;

							ImageTableNode->Table = GeneratedTable;
							ImageTableNode->ColumnName = ColumnName;
							ImageTableNode->ParameterName = TypedNodeTable->ParameterName;
							ImageTableNode->bNoneOption = TypedNodeTable->bAddNoneOption;
							ImageTableNode->DefaultRowName = TypedNodeTable->DefaultRowName.ToString();

							bool bIsPassthrough = (Pin->PinType.PinCategory == Schema->PC_PassThroughImage);

							UTexture2D* DefaultTexture2D = Cast<UTexture2D>(DefaultTexture);
							if (!bIsPassthrough && DefaultTexture2D)
							{
								mu::FImageDesc ImageDesc = GenerateImageDescriptor(DefaultTexture2D);

								uint32 LODBias = ReferenceTextureSize > 0 ? ComputeLODBiasForTexture(GenerationContext, *DefaultTexture2D, nullptr, ReferenceTextureSize) : 0;
								ImageDesc.m_size[0] = ImageDesc.m_size[0] >> LODBias;
								ImageDesc.m_size[1] = ImageDesc.m_size[1] >> LODBias;
								
								const uint16 MaxTextureSize = FMath::Max3(ImageDesc.m_size[0], ImageDesc.m_size[1], (uint16)1);
								ImageTableNode->MaxTextureSize = MaxTextureSize;
								ImageTableNode->ReferenceImageDesc = ImageDesc;
				
								const UTextureLODSettings& TextureLODSettings = GenerationContext.CompilationContext.Options.TargetPlatform->GetTextureLODSettings();
								const FTextureLODGroup& LODGroupInfo = TextureLODSettings.GetTextureLODGroup(DefaultTexture2D->LODGroup);

								ImageTableNode->SourceDataDescriptor.OptionalMaxLODSize = LODGroupInfo.OptionalMaxLODSize;
								ImageTableNode->SourceDataDescriptor.OptionalLODBias = LODGroupInfo.OptionalLODBias;
								ImageTableNode->SourceDataDescriptor.NumNonOptionalLODs = CompilationOptions.MinDiskMips;

								ImageTableNode->SourceDataDescriptor.SourceId = TableId; // It will be combined with the RowId when generating constants
							}
						}
					}
					else
					{
						FString Msg = FString::Printf(TEXT("Couldn't generate a mutable table."));
						GenerationContext.Log(FText::FromString(Msg), Node);
					}
				}
			}
			else
			{
				GenerationContext.Log(LOCTEXT("ImageTableError", "Couldn't find the data table of the node."), Node);
			}
		}
	}

	else if (const UCustomizableObjectNodeMacroInstance* TypedNodeMacro = Cast<UCustomizableObjectNodeMacroInstance>(Node))
	{
		// Can't use the GenerateMutableSourceMacro function here because GenerateMutableSourceImage needs the extra parameter ReferenceTextureSize 
		bCacheNode = false;

		if (const UEdGraphPin* OutputPin = TypedNodeMacro->GetMacroIOPin(ECOMacroIOType::COMVT_Output, Pin->PinName))
		{
			if (const UEdGraphPin* FollowPin = FollowInputPin(*OutputPin))
			{
				GenerationContext.MacroNodesStack.Push(TypedNodeMacro);
				Result = GenerateMutableSourceImage(FollowPin, GenerationContext, ReferenceTextureSize);
				GenerationContext.MacroNodesStack.Pop();
			}
			else
			{
				FText Msg = FText::Format(LOCTEXT("MacroInstanceError_PinNotLinked_Image", "Macro Output node Pin {0} not linked."), FText::FromName(Pin->PinName));
				GenerationContext.Log(Msg, Node);
			}
		}
		else
		{
			FText Msg = FText::Format(LOCTEXT("MacroInstanceError_PinNameNotFound_Image", "Macro Output node does not contain a pin with name {0}."), FText::FromName(Pin->PinName));
			GenerationContext.Log(Msg, Node);
		}
	}

	else if (const UCustomizableObjectNodeTunnel* TypedNodeTunnel = Cast<UCustomizableObjectNodeTunnel>(Node))
	{
		// Can't use the GenerateMutableSourceMacro function here because GenerateMutableSourceImage needs the extra parameter ReferenceTextureSize 
		check(TypedNodeTunnel->bIsInputNode);
		check(GenerationContext.MacroNodesStack.Num());

		bCacheNode = false;

		const UCustomizableObjectNodeMacroInstance* MacroInstanceNode = GenerationContext.MacroNodesStack.Pop();
		check(MacroInstanceNode);

		if (const UEdGraphPin* InputPin = MacroInstanceNode->FindPin(Pin->PinName, EEdGraphPinDirection::EGPD_Input))
		{
			if (const UEdGraphPin* FollowPin = FollowInputPin(*InputPin))
			{
				Result = GenerateMutableSourceImage(FollowPin, GenerationContext, ReferenceTextureSize);
			}
		}
		else
		{
			FText Msg = FText::Format(LOCTEXT("MacroTunnelError_PinNameNotFound_Image", "Macro Instance Node does not contain a pin with name {0}."), FText::FromName(Pin->PinName));
			GenerationContext.Log(Msg, Node);
		}

		// Push the Macro again even if the result is null
		GenerationContext.MacroNodesStack.Push(MacroInstanceNode);
	}

	else
	{
		GenerationContext.Log(LOCTEXT("UnimplementedNode", "Node type not implemented yet."), Node);
	}

	if (bCacheNode)
	{
		GenerationContext.Generated.Add(Key, FGeneratedData(Node, Result));
		GenerationContext.GeneratedNodes.Add(Node);
	}

	if (Result)
	{
		Result->SetMessageContext(Node);
	}

	return Result;
}


#undef LOCTEXT_NAMESPACE

