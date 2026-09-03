// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/GenerateMutableSource/GenerateMutableSource.h"

#include "GenerateMutableSourceComponent.h"
#include "Animation/AnimInstance.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Engine/TextureLODSettings.h"
#include "Interfaces/ITargetPlatform.h"
#include "MuCO/CustomizableObjectPrivate.h"
#include "MuCO/CustomizableObjectExtension.h"
#include "MuCO/CustomizableObjectUIData.h"
#include "MuCO/ICustomizableObjectModule.h"
#include "MuCOE/CustomizableObjectCompiler.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSourceGroupProjector.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSourceMacro.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSourceModifier.h"
#include "MuCOE/GraphTraversal.h"
#include "MuCOE/ICustomizableObjectExtensionNode.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMacroInstance.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMaterial.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMesh.h"
#include "MuCOE/Nodes/CustomizableObjectNodeModifierClipWithMesh.h"
#include "MuCOE/Nodes/CustomizableObjectNodeObjectGroup.h"
#include "MuCOE/Nodes/CustomizableObjectNodeStaticString.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTable.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTexture.h"
#include "MuCOE/Nodes/CustomizableObjectNodePassThroughTexture.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTextureBinarise.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTextureColourMap.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTextureFromChannels.h"
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
#include "MuCOE/Nodes/CustomizableObjectNodeVariation.h"
#include "MuCOE/UnrealEditorPortabilityHelpers.h"
#include "MuT/NodeComponentEdit.h"
#include "MuT/NodeMeshConstant.h"
#include "MuT/NodeObjectGroup.h"
#include "MuT/NodeObjectNew.h"
#include "MuR/Mesh.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PlatformInfo.h"
#include "Math/NumericLimits.h"
#include "Hash/CityHash.h"
#include "MuCO/CustomizableObjectResourceDataTypes.h"
#include "MuCOE/Nodes/CustomizableObjectNodeGroupProjectorParameter.h"

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


uint32 GetTypeHash(const FGraphCycleKey& Key)
{
	return HashCombine(HashCombine(GetTypeHash(&Key.Pin), GetTypeHash(Key.Id)), GetTypeHash(Key.MacroContext));
}


FGraphCycle::FGraphCycle(const FGraphCycleKey&& Key, FMutableGraphGenerationContext& Context) :
	Key(Key),
	Context(Context)
{
}


FGraphCycle::~FGraphCycle()
{
	Context.VisitedPins.Remove(Key);
}


bool FGraphCycle::FoundCycle() const
{
	const UCustomizableObjectNode& Node = *Cast<UCustomizableObjectNode>(Key.Pin.GetOwningNode());

	if (const UCustomizableObject** Result = Context.VisitedPins.Find(Key))
	{
		Context.CompilationContext.Log(LOCTEXT("CycleFoundNode", "Cycle detected."), &Node, EMessageSeverity::Error, true);
		Context.CustomizableObjectWithCycle = *Result;
		return true;
	}
	else
	{
		Context.VisitedPins.Add(Key, Node.GetGraph()->GetTypedOuter<UCustomizableObject>());
		return false;	
	}
}


/** Warn if the node has more outputs than it is meant to have. */
void CheckNumOutputs(const UEdGraphPin& Pin, const FMutableGraphGenerationContext& GenerationContext)
{
	if (const UCustomizableObjectNode* Typed = Cast<UCustomizableObjectNode>(Pin.GetOwningNode()))
	{
		if (Typed->IsSingleOutputNode())
		{
			int numOutLinks = 0;
			for (UEdGraphPin* NodePin : Typed->GetAllNonOrphanPins())
			{
				if (NodePin->Direction == EGPD_Output)
				{
					numOutLinks += NodePin->LinkedTo.Num();
				}
			}

			if (numOutLinks > 1)
			{
				GenerationContext.CompilationContext.Log(LOCTEXT("MultipleOutgoing", "The node has several outgoing connections, but it should be limited to 1."), CastChecked<UCustomizableObjectNode>(Pin.GetOwningNode()));
			}
		}
	}
}


uint32 GetTypeHash(const FGeneratedGroupProjectorsKey& Key)
{
	uint32 Hash = GetTypeHash(Key.Node);

	Hash = HashCombine(Hash, GetTypeHash(Key.CurrentComponent));
	
	return Hash; 
}


void FMutableCompilationContext::Log(const FText& Message, const TArray<const UObject*>& Context, const EMessageSeverity::Type MessageSeverity, const bool bAddBaseObjectInfo, const ELoggerSpamBin SpamBin) const
{
	if (TSharedPtr<FCustomizableObjectCompiler> Compiler = WeakCompiler.Pin())
	{
		Compiler->CompilerLog(Message, Context, MessageSeverity, bAddBaseObjectInfo, SpamBin);
	}
}


void FMutableCompilationContext::Log(const FText& Message, const UObject* Context, const EMessageSeverity::Type MessageSeverity, const bool bAddBaseObjectInfo, const ELoggerSpamBin SpamBin) const
{
	if (TSharedPtr<FCustomizableObjectCompiler> Compiler = WeakCompiler.Pin())
	{
		Compiler->CompilerLog(Message, Context, MessageSeverity, bAddBaseObjectInfo, SpamBin);
	}
}


mu::FBoneName FMutableCompilationContext::GetBoneUnique(const FName& InBoneName)
{
	const FString BoneNameString = InBoneName.ToString().ToLower();
	mu::FBoneName Bone(CityHash32(reinterpret_cast<const char*>(*BoneNameString), BoneNameString.Len() * sizeof(FString::ElementType)));

	bool bNewRemappedBoneName = false;

	FString& BoneName = UniqueBoneNames.FindOrAdd(Bone, BoneNameString);
	while (BoneName != InBoneName)
	{
		if (mu::FBoneName* RemappedBoneName = RemappedBoneNames.Find(BoneNameString))
		{
			Bone.Id = RemappedBoneName->Id;
			break;
		}

		// Id collision detected
		bNewRemappedBoneName = true;

		// Increase Id in an attempt to make it unique again.
		++Bone.Id;
		BoneName = UniqueBoneNames.FindOrAdd(Bone, BoneNameString);
	}

	if (bNewRemappedBoneName)
	{
		RemappedBoneNames.Add(BoneNameString, Bone);
	}

	return Bone;
}


uint32 FMutableCompilationContext::GetSkinWeightProfileIdUnique(const FName InProfileName)
{
	const FString ProfileNameString = InProfileName.ToString();

	uint32 UniqueProfileId = CityHash32(reinterpret_cast<const char*>(*ProfileNameString), ProfileNameString.Len() * sizeof(FString::ElementType));

	bool bRemappedProfile = false;

	FName& ProfileName = UniqueSkinWeightProfileIds.FindOrAdd(UniqueProfileId, InProfileName);
	while (ProfileName != InProfileName)
	{
		if (uint32* RemappedProfileId = RemappedSkinWeightProfileIds.Find(InProfileName))
		{
			UniqueProfileId = *RemappedProfileId;
			break;
		}

		// Id collision detected
		bRemappedProfile = true;

		// Increase Id in an attempt to make it unique again.
		++UniqueProfileId;
		ProfileName = UniqueSkinWeightProfileIds.FindOrAdd(UniqueProfileId, InProfileName);
	}

	if (bRemappedProfile)
	{
		RemappedSkinWeightProfileIds.Add(InProfileName, UniqueProfileId);
	}

	return UniqueProfileId;
}


FMutableCompilationContext::FMutableCompilationContext(const UCustomizableObject* InObject, const TSharedPtr<FCustomizableObjectCompiler>& InCompiler, const FCompilationOptions& InOptions)
	: Object(InObject)
	, Options(InOptions)
	, WeakCompiler(InCompiler)
{
}


FMutableComponentInfo* FMutableCompilationContext::GetComponentInfo(FName ComponentName)
{
	return ComponentInfos.FindByPredicate([ComponentName](const FMutableComponentInfo& Component)
		{
			return Component.ComponentName == ComponentName;
		});
}


FString FMutableCompilationContext::GetObjectName() const
{
	return GetNameSafe(Object.Get());
}


bool FMutableCompilationContext::FindBone(const FName& InBoneName, mu::FBoneName& OutBoneName) const
{
	const FString BoneNameString = InBoneName.ToString().ToLower();
	OutBoneName.Id = CityHash32(reinterpret_cast<const char*>(*BoneNameString), BoneNameString.Len() * sizeof(FString::ElementType));
	if (UniqueBoneNames.Contains(OutBoneName))
	{
		return true;
	}

	if (const mu::FBoneName* BoneName = RemappedBoneNames.Find(BoneNameString))
	{
		OutBoneName = *BoneName;
		return true;
	}

	return false;
}


FMutableGraphGenerationContext::FMutableGraphGenerationContext(FMutableCompilationContext& InCompilationContext)
	: CompilationContext(InCompilationContext)
	, ExtensionDataCompilerInterface(*this)
{
	// Default flags for mesh generation nodes.
	MeshGenerationFlags.Push(EMutableMeshConversionFlags::None);

	// Default flags for mesh layout generation.
	LayoutGenerationFlags.Push(FLayoutGenerationFlags());
}


FString FMutableGraphGenerationContext::GetObjectName()
{
	return CompilationContext.GetObjectName();
}


void FMutableGraphGenerationContext::Log(const FText& Message, const TArray<const UObject*>& Context, const EMessageSeverity::Type MessageSeverity, const bool bAddBaseObjectInfo, const ELoggerSpamBin SpamBin) const
{
	CompilationContext.Log(Message, Context, MessageSeverity, bAddBaseObjectInfo, SpamBin);
}


void FMutableGraphGenerationContext::Log(const FText& Message, const UObject* Context, const EMessageSeverity::Type MessageSeverity, const bool bAddBaseObjectInfo, const ELoggerSpamBin SpamBin) const
{
	CompilationContext.Log(Message, Context, MessageSeverity, bAddBaseObjectInfo, SpamBin);
}


TSharedPtr<mu::FMesh> FMutableGraphGenerationContext::FindGeneratedMesh( const FGeneratedMeshData::FKey& Key )
{
	for (const FGeneratedMeshData& d : GeneratedMeshes)
	{
		if (d.Key == Key )
		{
			return d.Generated;
		}
	}

	return nullptr;
}


int32 FMutableGraphGenerationContext::AddStreamedResource(UAssetUserData& AssetUserData)
{
	const uint32 AssetIdentifier = AssetUserData.GetUniqueID();

	// Return resource index if found.
	if (int32* ResourceIndex = StreamedResourceIndices.Find(AssetIdentifier))
	{
		return *ResourceIndex;
	}

	FCustomizableObjectAssetUserData ResourceData;
	ResourceData.AssetUserDataEditor = &AssetUserData;

	const int32 ResourceIndex = StreamedResourceData.Num();

	FCustomizableObjectResourceData& Resource = StreamedResourceData.AddDefaulted_GetRef();
	Resource.Type = ECOResourceDataType::AssetUserData;
	Resource.Data.InitializeAs<FCustomizableObjectAssetUserData>(MoveTemp(ResourceData));

	StreamedResourceIndices.Add({ AssetIdentifier, ResourceIndex });

	return ResourceIndex;
}


const FGuid FMutableGraphGenerationContext::GetNodeIdUnique(const UCustomizableObjectNode* Node)
{
	FGuid NodeID = GetNodeIdUnchecked(Node);
	TArray<const UObject*>* ArrayResult = NodeIdsMap.Find(NodeID);

	if (ArrayResult == nullptr)
	{
		TArray<const UObject*> ArrayTemp;
		ArrayTemp.Add(Node);
		NodeIdsMap.Add(NodeID, ArrayTemp);
		return NodeID;
	}

	ArrayResult->Add(Node);
	return FGuid::NewGuid();
}


const FGuid FMutableGraphGenerationContext::GetNodeIdUnchecked(const UCustomizableObjectNode* Node)
{
	FGuid NodeID = Node->NodeGuid;

	if (Node->IsInMacro())
	{
		check(MacroNodesStack.Num()); // To ensure that we only enter here when compiling

		for (int32 MacroIndex = 0; MacroIndex < MacroNodesStack.Num(); ++MacroIndex)
		{
			NodeID = FGuid::Combine(NodeID, MacroNodesStack[MacroIndex]->NodeGuid);
		}
	}

	return NodeID;
}


void FMutableGraphGenerationContext::GenerateSharedSurfacesUniqueIds()
{
	int32 UniqueId = 0;

	TArray<TArray<FSharedSurface>> NodeToSharedSurfaces;
	SharedSurfaceIds.GenerateValueArray(NodeToSharedSurfaces);

	TArray<bool> VisitedSurfaces;
	for (TArray<FSharedSurface>& SharedSurfaces : NodeToSharedSurfaces)
	{
		const int32 NumSurfaces = SharedSurfaces.Num();
		VisitedSurfaces.Init(false, NumSurfaces);
		
		// Iterate all surfaces for a given NodeMaterial and set the same SharedSurfaceId to those that are equal.
		for (int32 SurfaceIndex = 0; SurfaceIndex < NumSurfaces; ++SurfaceIndex)
		{
			if (VisitedSurfaces[SurfaceIndex])
			{
				continue;
			}

			FSharedSurface& CurrentSharedSurface = SharedSurfaces[SurfaceIndex];
			CurrentSharedSurface.NodeSurfaceNew->SharedSurfaceId = UniqueId;
			VisitedSurfaces[SurfaceIndex] = true;

			for (int32 AuxSurfaceIndex = SurfaceIndex; AuxSurfaceIndex < NumSurfaces && !CurrentSharedSurface.bMakeUnique; ++AuxSurfaceIndex)
			{
				if (VisitedSurfaces[AuxSurfaceIndex])
				{
					continue;
				}

				if (SharedSurfaces[AuxSurfaceIndex].NodeModifierIDs != CurrentSharedSurface.NodeModifierIDs)
				{
					continue;
				}

				if (SharedSurfaces[AuxSurfaceIndex].MacroContext != CurrentSharedSurface.MacroContext)
				{
					continue;
				}

				SharedSurfaces[AuxSurfaceIndex].NodeSurfaceNew->SharedSurfaceId = UniqueId;
				VisitedSurfaces[AuxSurfaceIndex] = true;
			}

			++UniqueId;
		}
	}
}


FMutableComponentInfo* FMutableGraphGenerationContext::GetCurrentComponentInfo()
{
	return CompilationContext.GetComponentInfo(CurrentMeshComponent);
}


UObject* FMutableGraphGenerationContext::LoadObject(const FSoftObjectPtr& SoftObject)
{
	return  MutablePrivate::LoadObject(SoftObject);
}


FGeneratedKey::FGeneratedKey(void* InFunctionAddress, const UEdGraphPin& InPin, const UCustomizableObjectNode& Node, FMutableGraphGenerationContext& GenerationContext, const bool UseMesh, const bool InbOnlyConnectedLOD)
{
	FunctionAddress = InFunctionAddress;
	Pin = &InPin;
	LOD = Node.IsAffectedByLOD() ? GenerationContext.CurrentLOD : 0;
	MacroContext = GenerationContext.MacroNodesStack;

	if (UseMesh)
	{
		Flags = GenerationContext.MeshGenerationFlags.Last();
		LayoutFlags = GenerationContext.LayoutGenerationFlags.Last();
		MeshMorphStack = GenerationContext.MeshMorphStack;
		bOnlyConnectedLOD = InbOnlyConnectedLOD;
	}
}


uint32 GetTypeHash(const FGeneratedKey& Key)
{
	uint32 Hash = GetTypeHash(Key.FunctionAddress);
	Hash = HashCombine(Hash, GetTypeHash(Key.Pin));
	Hash = HashCombine(Hash, GetTypeHash(Key.LOD));
	Hash = HashCombine(Hash, GetTypeHash(Key.Flags));
	//Hash = HashCombine(Hash, GetTypeHash(Key.LayoutFlags)); // Does not support array
	//Hash = HashCombine(Hash, GetTypeHash(Key.MeshMorphStack)); // Does not support array
	Hash = HashCombine(Hash, GetTypeHash(Key.bOnlyConnectedLOD));
	Hash = HashCombine(Hash, GetTypeHash(Key.CurrentMeshComponent));

	for (int32 MacroIndex = 0; MacroIndex < Key.MacroContext.Num(); ++MacroIndex)
	{
		Hash = HashCombine(Hash, GetTypeHash(Key.MacroContext[MacroIndex]));
	}
	
	return Hash;
}


FGraphCycleKey::FGraphCycleKey(const UEdGraphPin& Pin, const FString& Id, const UCustomizableObjectNodeMacroInstance* MacroContext) :
	Pin(Pin),
	Id(Id),
	MacroContext(MacroContext)
{
}


bool FGraphCycleKey::operator==(const FGraphCycleKey& Other) const
{
	return &Pin == &Other.Pin && Id == Other.Id && MacroContext == Other.MacroContext;
}


UTexture2D* FindReferenceImage(const UEdGraphPin* Pin, FMutableGraphGenerationContext& GenerationContext)
{
	check(Pin)
	RETURN_ON_CYCLE(*Pin, GenerationContext)

	CheckNumOutputs(*Pin, GenerationContext);
	
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();
	const UCustomizableObjectNode* Node = CastChecked<UCustomizableObjectNode>(Pin->GetOwningNode());

	UTexture2D* Result = nullptr;

	if (const UCustomizableObjectNodeTexture* TypedNodeTex = Cast<UCustomizableObjectNodeTexture>(Node))
	{
		Result = TypedNodeTex->Texture;
	}

	else if (const UCustomizableObjectNodePassThroughTexture* TypedNodePassThroughTex = Cast<UCustomizableObjectNodePassThroughTexture>(Node))
	{
		Result = Cast<UTexture2D>(TypedNodePassThroughTex->PassThroughTexture);
	}

	else if (const UCustomizableObjectNodeTextureParameter* ParamNodeTex = Cast<UCustomizableObjectNodeTextureParameter>(Node))
	{
		Result = ParamNodeTex->ReferenceValue;
	}

	else if (const UCustomizableObjectNodeMesh* TypedNodeMesh = Cast<UCustomizableObjectNodeMesh>(Node))
	{
		Result = TypedNodeMesh->FindTextureForPin(Pin);
	}

	else if (const UCustomizableObjectNodeTextureInterpolate* TypedNodeInterp = Cast<UCustomizableObjectNodeTextureInterpolate>(Node))
	{
		for (int LayerIndex = 0; !Result && LayerIndex < TypedNodeInterp->GetNumTargets(); ++LayerIndex)
		{
			if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeInterp->Targets(LayerIndex)))
			{
				Result = FindReferenceImage(ConnectedPin, GenerationContext);
			}
		}
	}

	else if (const UCustomizableObjectNodeTextureLayer* TypedNodeLayer = Cast<UCustomizableObjectNodeTextureLayer>(Node))
	{
		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeLayer->BasePin()))
		{
			Result = FindReferenceImage(ConnectedPin, GenerationContext);
		}

		for (int LayerIndex = 0; !Result && LayerIndex < TypedNodeLayer->GetNumLayers(); ++LayerIndex)
		{
			if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeLayer->LayerPin(LayerIndex)))
			{
				if (ConnectedPin->PinType.PinCategory == Schema->PC_Image)
				{
					Result = FindReferenceImage(ConnectedPin, GenerationContext);
				}
			}
		}
	}

	else if (const UCustomizableObjectNodeTextureSwitch* TypedNodeSwitch = Cast<UCustomizableObjectNodeTextureSwitch>(Node))
	{
		for (int SelectorIndex = 0; !Result && SelectorIndex < TypedNodeSwitch->GetNumElements(); ++SelectorIndex)
		{
			if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeSwitch->GetElementPin(SelectorIndex)))
			{
				Result = FindReferenceImage(ConnectedPin, GenerationContext);
			}
		}
	}

	else if (const UCustomizableObjectNodePassThroughTextureSwitch* TypedNodePassThroughSwitch = Cast<UCustomizableObjectNodePassThroughTextureSwitch>(Node))
	{
		for (int32 SelectorIndex = 0; !Result && SelectorIndex < TypedNodePassThroughSwitch->GetNumElements(); ++SelectorIndex)
		{
			if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodePassThroughSwitch->GetElementPin(SelectorIndex)))
			{
				Result = FindReferenceImage(ConnectedPin, GenerationContext);
			}
		}
	}

	else if (const UCustomizableObjectNodeTextureVariation* TypedNodeVariation = Cast<UCustomizableObjectNodeTextureVariation>(Node))
	{
		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeVariation->DefaultPin()))
		{
			Result = FindReferenceImage(ConnectedPin, GenerationContext);
		}

		for (int SelectorIndex = 0; !Result && SelectorIndex < TypedNodeVariation->GetNumVariations(); ++SelectorIndex)
		{
			if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeVariation->VariationPin(SelectorIndex)))
			{
				Result = FindReferenceImage(ConnectedPin, GenerationContext);
			}
		}
	}

	else if (const UCustomizableObjectNodeTextureFromChannels* TypedNodeFrom = Cast<UCustomizableObjectNodeTextureFromChannels>(Node))
	{
		mu::NodeImagePtr RNode, GNode, BNode, ANode;

		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeFrom->RPin()))
		{
			Result = FindReferenceImage(ConnectedPin, GenerationContext);
		}
		if (!Result)
		{
			if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeFrom->GPin()))
			{
				Result = FindReferenceImage(ConnectedPin, GenerationContext);
			}
		}
		if (!Result)
		{
			if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeFrom->BPin()))
			{
				Result = FindReferenceImage(ConnectedPin, GenerationContext);
			}
			
		}
		if (!Result)
		{
			if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeFrom->APin()))
			{
				Result = FindReferenceImage(ConnectedPin, GenerationContext);
			}
		}
	}

	else if (const UCustomizableObjectNodeTextureToChannels* TypedNodeTo = Cast<UCustomizableObjectNodeTextureToChannels>(Node))
	{
		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeTo->InputPin()))
		{
			Result = FindReferenceImage(ConnectedPin, GenerationContext);
		}
	}

	else if (const UCustomizableObjectNodeTextureProject* TypedNodeProj = Cast<UCustomizableObjectNodeTextureProject>(Node))
	{
		if (TypedNodeProj->ReferenceTexture)
		{
			Result = TypedNodeProj->ReferenceTexture;
		}
		else
		{
			int TexIndex = -1;// TypedNodeProj->OutputPins.Find((UEdGraphPin*)Pin);
			for (int32 i = 0; i < TypedNodeProj->GetNumOutputs(); ++i)
			{
				if (TypedNodeProj->OutputPins(i) == Pin)
				{
					TexIndex = i;
				}
			}

			check(TexIndex >= 0 && TexIndex < TypedNodeProj->GetNumTextures());

			if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeProj->TexturePins(TexIndex)))
			{
				Result = FindReferenceImage(ConnectedPin, GenerationContext);
			}
		}
	}

	else if (const UCustomizableObjectNodeTextureBinarise* TypedNodeBin = Cast<UCustomizableObjectNodeTextureBinarise>(Node))
	{
		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeBin->GetBaseImagePin()))
		{
			Result = FindReferenceImage(ConnectedPin, GenerationContext);
		}
	}

	else if (const UCustomizableObjectNodeTextureInvert* TypedNodeInv = Cast<UCustomizableObjectNodeTextureInvert>(Node))
	{
		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeInv->GetBaseImagePin()))
		{
			Result = FindReferenceImage(ConnectedPin, GenerationContext);
		}
	}

	else if (const UCustomizableObjectNodeTextureColourMap* TypedNodeColourMap = Cast<UCustomizableObjectNodeTextureColourMap>(Node))
	{
		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeColourMap->GetBasePin()))
		{
			Result = FindReferenceImage(ConnectedPin, GenerationContext);
		}
	}

	else if (const UCustomizableObjectNodeTextureTransform* TypedNodeTransform = Cast<UCustomizableObjectNodeTextureTransform>(Node))
	{
		if ( UEdGraphPin* BaseImagePin = FollowInputPin(*TypedNodeTransform->GetBaseImagePin()) )
		{
			Result = FindReferenceImage(BaseImagePin, GenerationContext);
		}
	}

	else if (const UCustomizableObjectNodeTextureSaturate* TypedNodeSaturate = Cast<UCustomizableObjectNodeTextureSaturate>(Node))
	{
		if ( UEdGraphPin* BaseImagePin = FollowInputPin(*TypedNodeSaturate->GetBaseImagePin()) )
		{
			Result = FindReferenceImage(BaseImagePin, GenerationContext);
		}
	}

	else if (const UCustomizableObjectNodeTable* TypedNodeTable = Cast<UCustomizableObjectNodeTable>(Node))
	{
		if (Pin->PinType.PinCategory == Schema->PC_MaterialAsset)
		{
			Result = TypedNodeTable->FindReferenceTextureParameter(Pin, GenerationContext.CurrentMaterialTableParameter);
		}
		else
		{
			Result = TypedNodeTable->GetColumnDefaultAssetByType<UTexture2D>(Pin);
		}
	}

	else if (const UCustomizableObjectNodeMacroInstance* TypedNodeMacro = Cast<UCustomizableObjectNodeMacroInstance>(Node))
	{
		if (const UEdGraphPin* OutputPin = TypedNodeMacro->GetMacroIOPin(ECOMacroIOType::COMVT_Output, Pin->PinName))
		{
			if (const UEdGraphPin* ConnectedPin = FollowInputPin(*OutputPin))
			{
				GenerationContext.MacroNodesStack.Push(TypedNodeMacro);
				Result = FindReferenceImage(ConnectedPin, GenerationContext);
				GenerationContext.MacroNodesStack.Pop();
			}
		}
	}

	else if (const UCustomizableObjectNodeTunnel* TypedNodeTunnel = Cast<UCustomizableObjectNodeTunnel>(Node))
	{
		check(TypedNodeTunnel->bIsInputNode);
		check(GenerationContext.MacroNodesStack.Num());

		const UCustomizableObjectNodeMacroInstance* MacroInstanceNode = GenerationContext.MacroNodesStack.Pop();
		check(MacroInstanceNode);

		if (const UEdGraphPin* InputPin = MacroInstanceNode->FindPin(Pin->PinName, EEdGraphPinDirection::EGPD_Input))
		{
			if (const UEdGraphPin* FollowPin = FollowInputPin(*InputPin))
			{
				Result = FindReferenceImage(FollowPin, GenerationContext);
			}
		}

		// Push the Macro again even if the result is null
		GenerationContext.MacroNodesStack.Push(MacroInstanceNode);
	}
	
	return Result;
}


mu::Ptr<mu::NodeMeshApplyPose> CreateNodeMeshApplyPose(FMutableGraphGenerationContext& GenerationContext, mu::NodeMeshPtr InputMeshNode, const TArray<FName>& ArrayBoneName, const TArray<FTransform>& ArrayTransform)
{
	check(ArrayBoneName.Num() == ArrayTransform.Num());

	TSharedPtr<mu::FMesh> MutableMesh = MakeShared<mu::FMesh>();
	mu::Ptr<mu::NodeMeshConstant> PoseNodeMesh = new mu::NodeMeshConstant;
	PoseNodeMesh->Value = MutableMesh;

	TSharedPtr<mu::FSkeleton> MutableSkeleton = MakeShared<mu::FSkeleton>();
	MutableMesh->SetSkeleton(MutableSkeleton);
	MutableMesh->SetBonePoseCount(ArrayBoneName.Num());
	MutableSkeleton->SetBoneCount(ArrayBoneName.Num());

	for (int32 i = 0; i < ArrayBoneName.Num(); ++i)
	{
		const FName BoneName = ArrayBoneName[i];
		const mu::FBoneName& BoneId = GenerationContext.CompilationContext.GetBoneUnique(BoneName);

		MutableSkeleton->SetDebugName(i, BoneName);
		MutableSkeleton->SetBoneName(i, { BoneId });
		MutableMesh->SetBonePose(i, BoneId, (FTransform3f)ArrayTransform[i], mu::EBoneUsageFlags::Skinning);
	}

	mu::Ptr<mu::NodeMeshApplyPose> NodeMeshApplyPose = new mu::NodeMeshApplyPose;
	NodeMeshApplyPose->Base = InputMeshNode;
	NodeMeshApplyPose->Pose = PoseNodeMesh;

	return NodeMeshApplyPose;
}


uint32 AddUniqueSurfaceMetadata(const FMutableSourceSurfaceMetadata& SourceMetadata, FMutableCompilationContext& CompilationContext)
{
	auto GetSkeletalMaterialFunc = [](const USkeletalMesh* SkeletalMesh, uint8 LODIndex, uint8 SectionIndex) -> const FSkeletalMaterial*
		{
			if (!SkeletalMesh)
			{
				return nullptr;
			}

			// We assume that LODIndex and MaterialIndex are valid for the imported model
			int32 MaterialIndex = INDEX_NONE;

			// Check if we have lod info map to get the correct material index
			if (const FSkeletalMeshLODInfo* LodInfo = SkeletalMesh->GetLODInfo(LODIndex))
			{
				if (LodInfo->LODMaterialMap.IsValidIndex(SectionIndex))
				{
					MaterialIndex = LodInfo->LODMaterialMap[SectionIndex];
				}
			}

			if (MaterialIndex == INDEX_NONE)
			{
				const FSkeletalMeshModel* ImportedModel = SkeletalMesh->GetImportedModel();
				if (ImportedModel && ImportedModel->LODModels.IsValidIndex(LODIndex) && ImportedModel->LODModels[LODIndex].Sections.IsValidIndex(SectionIndex))
				{
					MaterialIndex = ImportedModel->LODModels[LODIndex].Sections[SectionIndex].MaterialIndex;
				}
			}

			if (SkeletalMesh->GetMaterials().IsValidIndex(MaterialIndex))
			{
				return &SkeletalMesh->GetMaterials()[MaterialIndex];
			}
			
			return nullptr;
		};

	auto GetMeshSectionFunc = [](const USkeletalMesh* SkeletalMesh, uint8 LODIndex, uint8 SectionIndex) -> const FSkelMeshSection*
	{
		if (!SkeletalMesh)
		{
			return nullptr;
		}

		const FSkeletalMeshModel* ImportedModel = SkeletalMesh->GetImportedModel();
		if (ImportedModel && ImportedModel->LODModels.IsValidIndex(LODIndex) && ImportedModel->LODModels[LODIndex].Sections.IsValidIndex(SectionIndex))
		{
			return &ImportedModel->LODModels[LODIndex].Sections[SectionIndex];
		}

		return nullptr;
	};

	auto HashSurfaceMetadataFunc = [](const FMutableSurfaceMetadata& Data) -> uint32
	{
		// Create a string representation of the metadata.
		FString SlotNameString = Data.MaterialSlotName.ToString();
		SlotNameString.ToLowerInline();

		TArrayView<const uint8> NameDataView(
				reinterpret_cast<const uint8*>(*SlotNameString), SlotNameString.Len()*sizeof(FString::ElementType));

		TArray<uint8, TInlineAllocator<256>> DataToHash;

		DataToHash.Add(static_cast<uint8>(Data.bCastShadow));
		DataToHash.Append(NameDataView);

		return CityHash32(reinterpret_cast<const char*>(DataToHash.GetData()), DataToHash.Num());
	};

	auto CompareSurfaceMetadataFunc = [](const FMutableSurfaceMetadata& A, const FMutableSurfaceMetadata& B)
	{
		return A.bCastShadow == B.bCastShadow && A.MaterialSlotName == B.MaterialSlotName;
	};

	if (uint32* Found = CompilationContext.CachedSurfaceMetadataIds.Find(SourceMetadata))
	{
		return *Found;
	}
	
	USkeletalMesh* MetadataBaseMesh = Cast<USkeletalMesh>(MutablePrivate::LoadObject(SourceMetadata.Mesh));
	
	FMutableSurfaceMetadata SurfaceMetadata;

	if (const FSkeletalMaterial* Material = GetSkeletalMaterialFunc(MetadataBaseMesh, SourceMetadata.LODIndex, SourceMetadata.SectionIndex))
	{
		SurfaceMetadata.MaterialSlotName = Material->MaterialSlotName;
	}

	if (const FSkelMeshSection* MeshSection = GetMeshSectionFunc(MetadataBaseMesh, SourceMetadata.LODIndex, SourceMetadata.SectionIndex))
	{
		SurfaceMetadata.bCastShadow = MeshSection->bCastShadow;
	}

	uint32 SurfaceMetadataUniqueHash = Private::GenerateUniquePersistentHash(
			SurfaceMetadata, CompilationContext.SurfaceMetadata, HashSurfaceMetadataFunc, CompareSurfaceMetadataFunc);

	if (SurfaceMetadataUniqueHash != 0)
	{
		CompilationContext.SurfaceMetadata.FindOrAdd(SurfaceMetadataUniqueHash, SurfaceMetadata);
		CompilationContext.CachedSurfaceMetadataIds.FindOrAdd(SourceMetadata, SurfaceMetadataUniqueHash);
	}
	else
	{
		UE_LOG(LogMutable, Error, TEXT("Maximum number of surfaces reached."));
	}

	return SurfaceMetadataUniqueHash;
}


// Convert a CustomizableObject Source Graph into a mutable source graph  
mu::Ptr<mu::NodeObject> GenerateMutableSource(const UEdGraphPin * Pin, FMutableGraphGenerationContext & GenerationContext)
{
	MUTABLE_CPUPROFILER_SCOPE(GenerateMutableSource);

	check(Pin)
	RETURN_ON_CYCLE(*Pin, GenerationContext)

	CheckNumOutputs(*Pin, GenerationContext);
	
	UCustomizableObjectNode* Node = CastChecked<UCustomizableObjectNode>(Pin->GetOwningNode());
	
	const FGeneratedKey Key(reinterpret_cast<void*>(&GenerateMutableSource), *Pin, *Node, GenerationContext, true);
	if (const FGeneratedData* Generated = GenerationContext.Generated.Find(Key))
	{
		return static_cast<mu::NodeObject*>(Generated->Node.get());
	}

	// Bool that determines if a node can be added to the cache of nodes.
	// Most nodes need to be added to the cache but there are some that don't. For exampel, MacroInstanceNodes
	bool bCacheNode = true;

	mu::Ptr<mu::NodeObject> Result;
	
	if (const UCustomizableObjectNodeObject* TypedNodeObj = Cast<UCustomizableObjectNodeObject>(Node))
	{
		mu::Ptr<mu::NodeObjectNew> ObjectNode = new mu::NodeObjectNew();
		Result = ObjectNode;

		ObjectNode->SetMessageContext(Node);
		ObjectNode->SetName(TypedNodeObj->GetObjectName(&GenerationContext.MacroNodesStack));
		FGuid FinalGuid = GenerationContext.GetNodeIdUnique(TypedNodeObj);
		if (FinalGuid != TypedNodeObj->NodeGuid && !TypedNodeObj->IsInMacro())
		{
			GenerationContext.Log(FText::FromString(TEXT("Warning: Node has a duplicated GUID. A new ID has been generated, but cooked data will not be deterministic.")), Node, EMessageSeverity::Warning);
		}
		ObjectNode->SetUid(FinalGuid.ToString());
		
		// States
		int NumStates = TypedNodeObj->States.Num();
		ObjectNode->SetStateCount(NumStates);

		// In a partial compilation we will filter the states of the root object
		bool bFilterStates = true;

		if (GenerationContext.bPartialCompilation)
		{
			if (!TypedNodeObj->ParentObject)
			{
				bFilterStates = false;
			}
		}

		for (int StateIndex = 0; StateIndex < NumStates && bFilterStates; ++StateIndex)
		{
			const FCustomizableObjectState& State = TypedNodeObj->States[StateIndex];
			ObjectNode->SetStateName(StateIndex, State.Name);
			for (int ParamIndex = 0; ParamIndex < State.RuntimeParameters.Num(); ++ParamIndex)
			{
				ObjectNode->AddStateParam(StateIndex, State.RuntimeParameters[ParamIndex]);
			}

			int32 MinQualityLevelLOD = INT32_MAX;
			int32 MaxQualityLevelLOD = 0;

			for (const FMutableComponentInfo& ComponentInfo : GenerationContext.CompilationContext.ComponentInfos)
			{
				if (!Cast<UCustomizableObjectNodeComponentMesh>(ComponentInfo.Node))
				{
					continue;
				}
				
				const FMutableLODSettings& LODSettings = ComponentInfo.Node->LODSettings;
				const ITargetPlatform* TargetPlatform = GenerationContext.CompilationContext.Options.TargetPlatform;
				const FSupportedQualityLevelArray SupportedQualityLevels = LODSettings.MinQualityLevelLOD.GetSupportedQualityLevels(*TargetPlatform->GetPlatformInfo().IniPlatformName.ToString());
				
				for (const int32& QualitySetting : SupportedQualityLevels)
				{
					if (LODSettings.MinQualityLevelLOD.IsQualityLevelValid(QualitySetting))
					{
						const int32 QualityLod = LODSettings.MinQualityLevelLOD.GetValueForQualityLevel(QualitySetting);
						MinQualityLevelLOD = FMath::Min(MinQualityLevelLOD, QualityLod);
						MaxQualityLevelLOD = FMath::Max(MaxQualityLevelLOD, QualityLod);
					}
				}
			}

			// Generate the difference between the maximum and minimum LODs found.
			const int32 NumExtraLODsToBuildAfterFirstLOD = MinQualityLevelLOD != INT32_MAX ? MaxQualityLevelLOD - MinQualityLevelLOD : 0;

			ObjectNode->SetStateProperties(StateIndex, State.TextureCompressionStrategy, State.bBuildOnlyFirstLOD, NumExtraLODsToBuildAfterFirstLOD);

			// UI Data
			FMutableStateData StateUIData;
			StateUIData.StateUIMetadata = State.UIMetadata;
			StateUIData.bDisableTextureStreaming = State.bDisableTextureStreaming;
			StateUIData.bLiveUpdateMode = State.bLiveUpdateMode;
			StateUIData.bReuseInstanceTextures = State.bReuseInstanceTextures;
			StateUIData.ForcedParameterValues = State.ForcedParameterValues;

			GenerationContext.StateUIDataMap.Add(State.Name, StateUIData);
		}
		
		// Process components.
		//-------------------------------------------------------------------
		const UEdGraphPin* ComponentsPin = TypedNodeObj->ComponentsPin();
		if (ComponentsPin)
		{
			TArray<UEdGraphPin*> ConnectedComponentPins = FollowInputPinArray(*ComponentsPin);
			for (const UEdGraphPin* ComponentNodePin : ConnectedComponentPins)
			{
				mu::Ptr<mu::NodeComponent> ComponentNode = GenerateMutableSourceComponent(ComponentNodePin, GenerationContext);
				ObjectNode->Components.Add(ComponentNode);
			}
		}

		// Process modifiers.
		//-------------------------------------------------------------------
		if (const UEdGraphPin* ModifierPin = TypedNodeObj->ModifiersPin())
		{
			TArray<UEdGraphPin*> ConnectedModifierPins = FollowInputPinArray(*ModifierPin);
			for (UEdGraphPin* const ConnectedModifier : ConnectedModifierPins)
			{
				bool bMacroModifier = false;
				UEdGraphPin* ChildNodePin = ConnectedModifier;

				if (const UCustomizableObjectNodeMacroInstance* TypedNodeMacro = Cast<UCustomizableObjectNodeMacroInstance>(ChildNodePin->GetOwningNode()))
				{
					if (const UEdGraphPin* OutputPin = TypedNodeMacro->GetMacroIOPin(ECOMacroIOType::COMVT_Output, ChildNodePin->PinName))
					{
						// Get the modifier pin inside the macro
						if (UEdGraphPin* FollowPin = FollowInputPin(*OutputPin))
						{
							bMacroModifier = true;
							GenerationContext.MacroNodesStack.Push(TypedNodeMacro);
							ChildNodePin = FollowPin;
						}
					}
				}
				
				else if (!Cast<UCustomizableObjectNodeModifierBase>(ChildNodePin->GetOwningNode()))
				{
					continue;
				}
				
				for (const FMutableComponentInfo& Pair : GenerationContext.CompilationContext.ComponentInfos)
				{
					GenerationContext.CurrentMeshComponent = Pair.ComponentName;
					GenerationContext.CurrentAutoLODStrategy = Pair.Node->AutoLODStrategy;
					
					mu::Ptr<mu::NodeModifier> ModifierNode = GenerateMutableSourceModifier(ChildNodePin, GenerationContext);
					ObjectNode->Modifiers.Add(ModifierNode);

					GenerationContext.CurrentMeshComponent = {};
					GenerationContext.CurrentAutoLODStrategy = {};
				}

				if (bMacroModifier)
				{
					GenerationContext.MacroNodesStack.Pop();
				}
			}
		}
		
		// Generate inputs to Object node pins added by extensions
		//-------------------------------------------------------------------
		for (const FRegisteredObjectNodeInputPin& ExtensionInputPin : ICustomizableObjectModule::Get().GetAdditionalObjectNodePins())
		{
			const UEdGraphPin* GraphPin = TypedNodeObj->FindPin(ExtensionInputPin.GlobalPinName, EGPD_Input);
			if (!GraphPin)
			{
				continue;
			}

			TArray<UEdGraphPin*> ConnectedPins = FollowInputPinArray(*GraphPin);

			// If the pin isn't supposed to take more than one connection, ignore all but the first
			// incoming connection.
			if (!ExtensionInputPin.InputPin.bIsArray && ConnectedPins.Num() > 1)
			{
				FString Msg = FString::Printf(TEXT("Extension input %s has multiple incoming connections but is only expecting one connection."),
					*ExtensionInputPin.InputPin.DisplayName.ToString());

				GenerationContext.Log(FText::FromString(Msg), Node, EMessageSeverity::Warning);
			}

			for (const UEdGraphPin* ConnectedPin : ConnectedPins)
			{
				const UEdGraphNode* ConnectedNode = ConnectedPin->GetOwningNode();
				
				if (const ICustomizableObjectExtensionNode* ExtensionNode = Cast<ICustomizableObjectExtensionNode>(ConnectedNode))
				{
					if (mu::Ptr<mu::NodeExtensionData> GeneratedNode = ExtensionNode->GenerateMutableNode(GenerationContext.ExtensionDataCompilerInterface))
					{
						ObjectNode->AddExtensionDataNode(GeneratedNode, TCHAR_TO_ANSI(*ExtensionInputPin.GlobalPinName.ToString()));
					}
				}
			}
		}

		// Children
		//-------------------------------------------------------------------
		TArray<UEdGraphPin*> ConnectedChildrenPins = FollowInputPinArray(*TypedNodeObj->ChildrenPin());
		ObjectNode->Children.Reserve(ConnectedChildrenPins.Num());
		for (int32 ChildIndex = 0; ChildIndex < ConnectedChildrenPins.Num(); ++ChildIndex)
		{
			mu::Ptr<mu::NodeObject> ChildNode = GenerateMutableSource(ConnectedChildrenPins[ChildIndex], GenerationContext);
			ObjectNode->Children.Add(ChildNode);
		}
	}

	else if (const UCustomizableObjectNodeObjectGroup* TypedNodeGroup = Cast<UCustomizableObjectNodeObjectGroup>(Node))
	{
		mu::Ptr<mu::NodeObjectGroup> GroupNode = new mu::NodeObjectGroup();
		Result = GroupNode;

		// All sockets from all mesh parts plugged into this group node will have the following priority when there's a socket name clash
		GenerationContext.SocketPriorityStack.Push(TypedNodeGroup->SocketPriority);

		// Getting node Id. We may need a different ID if the group node is inside a Macro and this Macro is used more than once.
		// Group nodes of Macros Can Not have external children so we can change their ids here safetely.
		FGuid GroupNodeId = GenerationContext.GetNodeIdUnchecked(TypedNodeGroup);

		GroupNode->SetMessageContext(Node);
		GroupNode->SetName(TypedNodeGroup->GetGroupName(&GenerationContext.MacroNodesStack));
		GroupNode->SetUid(GroupNodeId.ToString());
		
		TArray<UCustomizableObjectNodeGroupProjectorParameter*> GroupProjectors;
		if (UEdGraphPin* ProjectorsPin = TypedNodeGroup->GroupProjectorsPin())
		{
			for (const UEdGraphPin* ConnectedPin : FollowInputPinArray(*ProjectorsPin))
			{
				// Checking if it's linked to a Macro or tunnel node
				if (const UEdGraphPin* GroupProjectorPin = GraphTraversal::FindIOPinSourceThroughMacroContext(*ConnectedPin, &GenerationContext.MacroNodesStack))
				{
					if (UCustomizableObjectNodeGroupProjectorParameter* GroupProjectorNode = Cast<UCustomizableObjectNodeGroupProjectorParameter>(GroupProjectorPin->GetOwningNode()))
					{
						GroupProjectors.Add(GroupProjectorNode);
					}
				}
			}
		}

		GenerationContext.CurrentGroupProjectors.Push(GroupProjectors);
		ON_SCOPE_EXIT
		{
			GenerationContext.CurrentGroupProjectors.Pop();
		};

		mu::NodeObjectGroup::EChildSelection Type = mu::NodeObjectGroup::CS_ALWAYS_ALL;
		switch (TypedNodeGroup->GroupType)
		{
		case ECustomizableObjectGroupType::COGT_ALL: Type = mu::NodeObjectGroup::CS_ALWAYS_ALL; break;
		case ECustomizableObjectGroupType::COGT_TOGGLE: Type = mu::NodeObjectGroup::CS_TOGGLE_EACH; break;
		case ECustomizableObjectGroupType::COGT_ONE: Type = mu::NodeObjectGroup::CS_ALWAYS_ONE; break;
		case ECustomizableObjectGroupType::COGT_ONE_OR_NONE: Type = mu::NodeObjectGroup::CS_ONE_OR_NONE; break;
		default:
			GenerationContext.Log(LOCTEXT("UnsupportedGroupType", "Object Group Type not supported. Setting to 'ALL'."), Node);
			break;
		}
		GroupNode->Type = Type;

		// External children
		TArray<UCustomizableObjectNodeObject*> ExternalChildNodes;

		GenerationContext.GroupIdToExternalNodeMap.MultiFind(GroupNodeId, ExternalChildNodes);
		FMutableGraphGenerationContext::FParamInfo ParamInfo(TypedNodeGroup->GetGroupName(&GenerationContext.MacroNodesStack),
															 TypedNodeGroup->GroupType == ECustomizableObjectGroupType::COGT_TOGGLE);
		GenerationContext.GuidToParamNameMap.Add(GroupNodeId,ParamInfo);

		// Children
		TArray<const UEdGraphPin*> ConnectedChildrenPins;

		for (const UEdGraphPin* LinkedPin : FollowInputPinArray(*TypedNodeGroup->ObjectsPin()))
		{
			// Checking if it's linked to a macro node and the macro node is linked to something
			if (const UEdGraphPin* MacroCheckPin = GraphTraversal::FindIOPinSourceThroughMacroContext(*LinkedPin, &GenerationContext.MacroNodesStack))
			{
				// We use the original pins to generate the children objects. GenerateMutableSourceX functions already go through macro nodes.
				ConnectedChildrenPins.Add(LinkedPin);
			}
		}

		const int32 NumChildren = ConnectedChildrenPins.Num();
		const int32 TotalNumChildren = NumChildren + ExternalChildNodes.Num();

		GroupNode->Children.SetNum(TotalNumChildren);
		GroupNode->DefaultValue = (Type == mu::NodeObjectGroup::CS_ONE_OR_NONE ? -1 : 0);
		int32 ChildIndex = 0;

		// UI data
		FMutableParameterData ParameterUIData(
			TypedNodeGroup->ParamUIMetadata,
			EMutableParameterType::Int);

		ParameterUIData.IntegerParameterGroupType = TypedNodeGroup->GroupType;

		// In the case of a partial compilation, make sure at least one child is connected so that the param is no optimized
		bool bAtLeastOneConnected = false;

		for (; ChildIndex < NumChildren; ++ChildIndex)
		{
			bool bLastChildNode = (ChildIndex == NumChildren - 1) && (ExternalChildNodes.Num() == 0);
			bool bConnectAtLeastTheLastChild = bLastChildNode && !bAtLeastOneConnected;

			// Check if this node is linked to a MacroNode
			const UEdGraphPin* MacroContextPin = GraphTraversal::FindIOPinSourceThroughMacroContext(*ConnectedChildrenPins[ChildIndex], &GenerationContext.MacroNodesStack);
			check(MacroContextPin);

			UCustomizableObjectNodeObject* CustomizableObjectNodeObject = Cast<UCustomizableObjectNodeObject>(MacroContextPin->GetOwningNode());

			const FString* SelectedOptionName = GenerationContext.CompilationContext.Options.ParamNamesToSelectedOptions.Find(TypedNodeGroup->GetGroupName(&GenerationContext.MacroNodesStack)); // If the param is in the map restrict to only the selected option
			mu::Ptr<mu::NodeObject> ChildNode;

			if (bConnectAtLeastTheLastChild || !SelectedOptionName || (CustomizableObjectNodeObject && *SelectedOptionName == CustomizableObjectNodeObject->GetObjectName(&GenerationContext.MacroNodesStack)) )
			{
				bAtLeastOneConnected = true;

				ChildNode = GenerateMutableSource(ConnectedChildrenPins[ChildIndex], GenerationContext);
				GroupNode->Children[ChildIndex] = ChildNode;

				if (CustomizableObjectNodeObject)
				{
					FString LeftSplit = CustomizableObjectNodeObject->GetPathName();
					LeftSplit.Split(".", &LeftSplit, nullptr);
					GenerationContext.CustomizableObjectPathMap.Add(CustomizableObjectNodeObject->Identifier.ToString(), LeftSplit);
					GenerationContext.GroupNodeMap.Add(CustomizableObjectNodeObject->Identifier.ToString(), FCustomizableObjectIdPair(TypedNodeGroup->GetGroupName(&GenerationContext.MacroNodesStack), ChildNode->GetName()));
					ParameterUIData.ArrayIntegerParameterOption.Add(
						CustomizableObjectNodeObject->GetObjectName(&GenerationContext.MacroNodesStack),
				FIntegerParameterUIData(CustomizableObjectNodeObject->ParamUIMetadata));

					if (TypedNodeGroup->GroupType == ECustomizableObjectGroupType::COGT_TOGGLE)
					{
						// UI Data is only relevant when the group node is set to Toggle
						GenerationContext.ParameterUIDataMap.Add(CustomizableObjectNodeObject->GetObjectName(&GenerationContext.MacroNodesStack), FMutableParameterData(
							CustomizableObjectNodeObject->ParamUIMetadata,
							EMutableParameterType::Int));
					}
				}
			}
			else
			{
				ChildNode = new mu::NodeObjectNew;
				ChildNode->SetName(CustomizableObjectNodeObject->GetObjectName(&GenerationContext.MacroNodesStack));
				GroupNode->Children[ChildIndex] = ChildNode;
			}

			if ((TypedNodeGroup->GroupType == ECustomizableObjectGroupType::COGT_ONE ||
				TypedNodeGroup->GroupType == ECustomizableObjectGroupType::COGT_ONE_OR_NONE)
				&& TypedNodeGroup->DefaultValue == ChildNode->GetName())
			{
				GroupNode->DefaultValue = ChildIndex;
			}
		}

		const bool bCollapseUnderParent = TypedNodeGroup->ParamUIMetadata.ExtraInformation.Find(FString("CollapseUnderParent")) != nullptr;
		constexpr bool bHideWhenNotSelected = true; //TypedNodeGroup->ParamUIMetadata.ExtraInformation.Find(FString("HideWhenNotSelected"));
		
		if (bCollapseUnderParent || bHideWhenNotSelected)
		{
			if (const UEdGraphPin* ConnectedPin = FollowOutputPin(*Pin))
			{
				// Checking if it's linked to a Macro or tunnel node
				const UEdGraphPin* NodeLinkedPin = GraphTraversal::FindIOPinSourceThroughMacroContext(*ConnectedPin, &GenerationContext.MacroNodesStack);

				if (NodeLinkedPin)
				{
					const FGuid* ParentId = nullptr;
					FString ValueName;

					if (UCustomizableObjectNodeObject* NodeObject = Cast<UCustomizableObjectNodeObject>(NodeLinkedPin->GetOwningNode()))
					{
						ParentId = GenerationContext.GroupIdToExternalNodeMap.FindKey(NodeObject);
						ValueName = NodeObject->GetObjectName(&GenerationContext.MacroNodesStack);

						// Group objects in the same graph aren't in the GroupIdToExternalNodeMap, so follow the pins instead
						if (!ParentId && NodeObject->OutputPin())
						{
							if (const UEdGraphPin* ConnectedPinToObject = FollowOutputPin(*NodeObject->OutputPin()))
							{
								if (UCustomizableObjectNodeObjectGroup* ParentGroupNode = Cast<UCustomizableObjectNodeObjectGroup>(ConnectedPinToObject->GetOwningNode()))
								{
									FGuid ParentGroupNodeId = GenerationContext.GetNodeIdUnchecked(ParentGroupNode);
									ParentId = &ParentGroupNodeId;
								}
							}
						}
					}
					else if (UCustomizableObjectNodeObjectGroup* NodeObjectGroup = Cast<UCustomizableObjectNodeObjectGroup>(NodeLinkedPin->GetOwningNode()))
					{
						FGuid ParentGroupNodeId = GenerationContext.GetNodeIdUnchecked(NodeObjectGroup);
						ParentId = &ParentGroupNodeId;

						ValueName = TypedNodeGroup->GetGroupName(&GenerationContext.MacroNodesStack);
					}

					if (ParentId)
					{
						FMutableGraphGenerationContext::FParamInfo* ParentParamInfo = GenerationContext.GuidToParamNameMap.Find(*ParentId);

						if (ParentParamInfo)
						{
							FString ParentParamName = ParentParamInfo->ParamName;

							ParameterUIData.ParamUIMetadata.ExtraInformation.Add(FString("__ParentParamName"),
								ParentParamInfo->bIsToggle ? ValueName : *ParentParamName);

							if (bHideWhenNotSelected)
							{
								ParameterUIData.ParamUIMetadata.ExtraInformation.Add(FString("__DisplayWhenParentValueEquals"),
									ParentParamInfo->bIsToggle ? FString("1") : ValueName);
							}

							if (bCollapseUnderParent)
							{
								ParameterUIData.ParamUIMetadata.ExtraInformation.Add(FString("CollapseUnderParent"));

								FMutableParameterData ParentParameterUIData;
								ParentParameterUIData.ParamUIMetadata.ExtraInformation.Add(FString("__HasCollapsibleChildren"));
								GenerationContext.ParameterUIDataMap.Add(*ParentParamName, ParentParameterUIData);
							}
						}
					}
				}
			}
		}

		// Build external objects that reference this object as parent
		const int32 NumExternalChildren = FMath::Max(0, TotalNumChildren - NumChildren);
		for (int32 ExternalChildIndex = 0; ExternalChildIndex < NumExternalChildren; ++ExternalChildIndex)
		{
			const UCustomizableObjectNodeObject* ExternalChildNode = ExternalChildNodes[ExternalChildIndex];
			bool bLastExternalChildNode = ExternalChildIndex == ExternalChildNodes.Num() - 1;
			bool bConnectAtLeastTheLastChild = bLastExternalChildNode && !bAtLeastOneConnected;

			UCustomizableObjectNodeObject* CustomizableObjectNodeObject = Cast<UCustomizableObjectNodeObject>(ExternalChildNode->OutputPin()->GetOwningNode());

			const FString* SelectedOptionName = GenerationContext.CompilationContext.Options.ParamNamesToSelectedOptions.Find(TypedNodeGroup->GetGroupName(&GenerationContext.MacroNodesStack)); // If the param is in the map restrict to only the selected option
			mu::Ptr<mu::NodeObject> ChildNode;

			if (bConnectAtLeastTheLastChild || !SelectedOptionName || (CustomizableObjectNodeObject  && *SelectedOptionName == CustomizableObjectNodeObject->GetObjectName(&GenerationContext.MacroNodesStack)) )
			{
				bAtLeastOneConnected = true;

				ChildNode = GenerateMutableSource(ExternalChildNode->OutputPin(), GenerationContext);
				GroupNode->Children[ChildIndex] = ChildNode;

				if (CustomizableObjectNodeObject)
				{
					FString LeftSplit = ExternalChildNode->GetPathName();
					LeftSplit.Split(".", &LeftSplit, nullptr);
					GenerationContext.CustomizableObjectPathMap.Add(CustomizableObjectNodeObject->Identifier.ToString(), LeftSplit);
					GenerationContext.GroupNodeMap.Add(CustomizableObjectNodeObject->Identifier.ToString(), FCustomizableObjectIdPair(TypedNodeGroup->GetGroupName(&GenerationContext.MacroNodesStack), ChildNode->GetName()));
					ParameterUIData.ArrayIntegerParameterOption.Add(
						CustomizableObjectNodeObject->GetObjectName(&GenerationContext.MacroNodesStack),
						FIntegerParameterUIData(CustomizableObjectNodeObject->ParamUIMetadata));

					if (CustomizableObjectNodeObject->GetObjectName(&GenerationContext.MacroNodesStack).IsEmpty())
					{
						GenerationContext.NoNameNodeObjectArray.AddUnique(CustomizableObjectNodeObject);
					}

					if (TypedNodeGroup->GroupType == ECustomizableObjectGroupType::COGT_TOGGLE)
					{
						// UI Data is only relevant when the group node is set to Toggle
						GenerationContext.ParameterUIDataMap.Add(CustomizableObjectNodeObject->GetObjectName(&GenerationContext.MacroNodesStack), FMutableParameterData(
							CustomizableObjectNodeObject->ParamUIMetadata,
							EMutableParameterType::Int));
					}
				}
			}
			else
			{
				ChildNode = new mu::NodeObjectNew;
				ChildNode->SetName(CustomizableObjectNodeObject->GetObjectName(&GenerationContext.MacroNodesStack));
				GroupNode->Children[ChildIndex] = ChildNode;
			}

			if ((TypedNodeGroup->GroupType == ECustomizableObjectGroupType::COGT_ONE ||
				TypedNodeGroup->GroupType == ECustomizableObjectGroupType::COGT_ONE_OR_NONE)
				&& TypedNodeGroup->DefaultValue == ChildNode->GetName())
			{
				GroupNode->DefaultValue = ChildIndex;
			}

			ChildIndex++;
		}

		const FMutableParameterData* ChildFilledUIData = GenerationContext.ParameterUIDataMap.Find(TypedNodeGroup->GetGroupName(&GenerationContext.MacroNodesStack));
		if (ChildFilledUIData && ChildFilledUIData->ParamUIMetadata.ExtraInformation.Find(FString("__HasCollapsibleChildren")))
		{
			// Some child param filled the HasCollapsibleChildren UI info, refill it so it's not lost
			ParameterUIData.ParamUIMetadata.ExtraInformation.Add(FString("__HasCollapsibleChildren"));
		}

		if (TypedNodeGroup->GroupType == ECustomizableObjectGroupType::COGT_TOGGLE)
		{
			for (const TTuple<FString, FIntegerParameterUIData>& BooleanParam : ParameterUIData.ArrayIntegerParameterOption)
			{
				FMutableParameterData ParameterUIDataBoolean(
					BooleanParam.Value.ParamUIMetadata,
					EMutableParameterType::Bool);

				ParameterUIDataBoolean.ParamUIMetadata.ExtraInformation = ParameterUIData.ParamUIMetadata.ExtraInformation;

				GenerationContext.ParameterUIDataMap.Add(BooleanParam.Key, ParameterUIDataBoolean);
			}
		}
		else
		{
			GenerationContext.ParameterUIDataMap.Add(TypedNodeGroup->GetGroupName(&GenerationContext.MacroNodesStack), ParameterUIData);
		}

		// Go back to the parent group node's socket priority if it exists
		ensure(GenerationContext.SocketPriorityStack.Num() > 0);
		GenerationContext.SocketPriorityStack.Pop();
	}

	else if (const UCustomizableObjectNodeMacroInstance* TypedNodeMacro = Cast<UCustomizableObjectNodeMacroInstance>(Node))
	{
		bCacheNode = false;

		Result = GenerateMutableSourceMacro<mu::NodeObject>(*Pin, GenerationContext, GenerateMutableSource);
	}

	else if (const UCustomizableObjectNodeTunnel* TypedNodeTunnel = Cast<UCustomizableObjectNodeTunnel>(Node))
	{
		bCacheNode = false;

		Result = GenerateMutableSourceMacro<mu::NodeObject>(*Pin, GenerationContext, GenerateMutableSource);
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

	return Result;
}


int32 AddTagToMutableMeshUnique(mu::FMesh& MutableMesh, const FString& Tag)
{
	const int32 TagCount = MutableMesh.GetTagCount();

	for (int32 TagIndex = TagCount - 1; TagIndex >= 0; --TagIndex)
	{
		if (FString(MutableMesh.GetTag(TagIndex)) == Tag)
		{
			return TagIndex;
		}
	}

	MutableMesh.SetTagCount(TagCount + 1);
	MutableMesh.SetTag(TagCount, Tag);

	return TagCount;
}

FString GenerateAnimationInstanceTag(const int32 AnimBpIndex, const FName& SlotIndex)
{
	return FString("__AnimBP:") + FString::Printf(TEXT("%s_Slot_"), *SlotIndex.ToString()) + FString::FromInt(AnimBpIndex);
}


FString GenerateGameplayTag(const FString& GameplayTag)
{
	return FString("__AnimBPTag:") + GameplayTag;
}


void PopulateReferenceSkeletalMeshesData(FMutableGraphGenerationContext& GenerationContext)
{
	const FString PlatformName = GenerationContext.CompilationContext.Options.TargetPlatform->IniPlatformName();
	
	const uint32 ComponentCount = GenerationContext.CompilationContext.ComponentInfos.Num();

	GenerationContext.ReferenceSkeletalMeshesData.AddDefaulted(ComponentCount);
	for (uint32 ComponentIndex = 0; ComponentIndex < ComponentCount; ++ComponentIndex)
	{
		const FMutableComponentInfo& ComponentInfo = GenerationContext.CompilationContext.ComponentInfos[ComponentIndex];
		const FName& ComponentName = ComponentInfo.ComponentName;
		
		USkeletalMesh* RefSkeletalMesh = ComponentInfo.RefSkeletalMesh.Get();
		check(RefSkeletalMesh);

		FMutableRefSkeletalMeshData& Data = GenerationContext.ReferenceSkeletalMeshesData[ComponentIndex];

		// Set the RefSkeletalMesh
		if (!GenerationContext.CompilationContext.Options.TargetPlatform->IsClientOnly()
			|| GenerationContext.CompilationContext.Object->bEnableUseRefSkeletalMeshAsPlaceholder)
		{
			Data.SkeletalMesh = RefSkeletalMesh;
		}

		Data.SoftSkeletalMesh = RefSkeletalMesh;

		// Set the optional USkeletalMeshLODSettings that will be applied to the generated transient meshes or the baked meshes
		Data.SkeletalMeshLODSettings = RefSkeletalMesh->GetLODSettings();

		// Gather LODData, this may include per LOD settings such as render data config or LODDataInfoArray
		Data.LODData.AddDefaulted(GenerationContext.NumLODs[ComponentName]);
		
		const uint32 RefSkeletalMeshLODCount = RefSkeletalMesh->GetLODNum();

		const uint32 LODCount = GenerationContext.NumLODs[ComponentName];
		for (uint32 LODIndex = 0; LODIndex < LODCount; ++LODIndex)
		{
			FMutableRefLODData& LODData = Data.LODData[LODIndex];
			if(LODIndex < RefSkeletalMeshLODCount)
			{
				// Copy LOD info data from the reference skeletal mesh
				const FSkeletalMeshLODInfo& LODInfo = *RefSkeletalMesh->GetLODInfo(LODIndex);
				LODData.LODInfo.ScreenSize = LODInfo.ScreenSize.GetValueForPlatform(*PlatformName);
				LODData.LODInfo.LODHysteresis = LODInfo.LODHysteresis;
				LODData.LODInfo.bSupportUniformlyDistributedSampling = LODInfo.bSupportUniformlyDistributedSampling;
				LODData.LODInfo.bAllowCPUAccess = LODInfo.bAllowCPUAccess;

				// Copy Render data settings from the reference skeletal mesh
				const FSkeletalMeshLODRenderData& ReferenceLODModel = RefSkeletalMesh->GetResourceForRendering()->LODRenderData[LODIndex];
				LODData.RenderData.bIsLODOptional = ReferenceLODModel.bIsLODOptional;
				LODData.RenderData.bStreamedDataInlined = ReferenceLODModel.bStreamedDataInlined;
			}
			else
			{
				LODData.LODInfo.ScreenSize = 0.3f / (LODIndex + 1);
				LODData.LODInfo.LODHysteresis = 0.02f;				
			}
		}

		// Gather SkeletalMesh Sockets;
		const TArray<USkeletalMeshSocket*>& RefSkeletonSockets = RefSkeletalMesh->GetMeshOnlySocketList();
		const uint32 SocketCount = RefSkeletonSockets.Num();

		Data.Sockets.AddDefaulted(SocketCount);
		for(uint32 SocketIndex = 0 ; SocketIndex < SocketCount; ++SocketIndex)
		{
			const USkeletalMeshSocket* RefSocket = RefSkeletonSockets[SocketIndex];
			check(RefSocket);

			FMutableRefSocket& Socket = Data.Sockets[SocketIndex];
			Socket.SocketName = RefSocket->SocketName;
			Socket.BoneName = RefSocket->BoneName;
			Socket.RelativeLocation = RefSocket->RelativeLocation;
			Socket.RelativeRotation = RefSocket->RelativeRotation;
			Socket.RelativeScale = RefSocket->RelativeScale;
			Socket.bForceAlwaysAnimated = RefSocket->bForceAlwaysAnimated;
		}

		// TODO: Generate Bounds?
		// Gather Bounds
		Data.Bounds = RefSkeletalMesh->GetBounds();
		
		// Additional Settings
		Data.Settings.bEnablePerPolyCollision = RefSkeletalMesh->GetEnablePerPolyCollision();

		const TArray<FSkeletalMaterial>& Materials = RefSkeletalMesh->GetMaterials();
		for (const FSkeletalMaterial& Material : Materials)
		{
			if (Material.UVChannelData.bInitialized)
			{
				for (int32 UVIndex = 0; UVIndex < TEXSTREAM_MAX_NUM_UVCHANNELS; ++UVIndex)
				{
					Data.Settings.DefaultUVChannelDensity = FMath::Max(Data.Settings.DefaultUVChannelDensity, Material.UVChannelData.LocalUVDensities[UVIndex]);
				}
			}
		}

		// Skeleton
		if(USkeleton* Skeleton = RefSkeletalMesh->GetSkeleton())
		{
			Data.Skeleton = Skeleton;
		}
		
		// Physics Asset
		if (UPhysicsAsset* PhysicsAsset = RefSkeletalMesh->GetPhysicsAsset())
		{
			Data.PhysicsAsset = PhysicsAsset;
		}

		// Post ProcessAnimInstance
		if(const TSubclassOf<UAnimInstance> PostProcessAnimInstance = RefSkeletalMesh->GetPostProcessAnimBlueprint())
		{
			Data.PostProcessAnimInst = PostProcessAnimInstance;
		}
		
		// Shadow Physics Asset
		if (UPhysicsAsset* PhysicsAsset = RefSkeletalMesh->GetShadowPhysicsAsset())
		{
			Data.ShadowPhysicsAsset = PhysicsAsset;
		}

		// Asset User Data
		if (const TArray<UAssetUserData*>* AssetUserDataArray = RefSkeletalMesh->GetAssetUserDataArray())
		{
			for (UAssetUserData* AssetUserData : *AssetUserDataArray)
			{
				if (AssetUserData)
				{
					Data.AssetUserDataIndices.AddUnique(GenerationContext.AddStreamedResource(*AssetUserData));
				}
			}
		}
	}
}


uint32 GetBaseTextureSize(const FMutableGraphGenerationContext& GenerationContext, const UCustomizableObjectNodeMaterialBase* Material, uint32 ImageIndex)
{
	const FGeneratedImageProperties* ImageProperties = GenerationContext.ImageProperties.Find({ Material, ImageIndex });
	return ImageProperties ? ImageProperties->TextureSize : 0;
}


// Find the LODBias to apply to stay within the MaxTextureSize limit of the TargetPlatform
int32 GetPlatformLODBias(int32 TextureSize, int32 NumMips, int32 MaxPlatformSize)
{
	if (MaxPlatformSize > 0 && MaxPlatformSize < TextureSize)
	{
		const int32 MaxMipsAllowed = FMath::CeilLogTwo(MaxPlatformSize) + 1;
		return NumMips - MaxMipsAllowed;
	}

	return 0;
}


uint32 ComputeLODBiasForTexture(const FMutableGraphGenerationContext& GenerationContext, const UTexture2D& Texture, const UTexture2D* ReferenceTexture, int32 BaseTextureSize)
{
	constexpr int32 MaxAllowedLODBias = 6;

	// Force a large LODBias for debug
	if (GenerationContext.CompilationContext.Options.bForceLargeLODBias)
	{
		return FMath::Min(GenerationContext.CompilationContext.Options.DebugBias, MaxAllowedLODBias);
	}

	// Max size and number of mips from Texture. 
	const int32 SourceSize = (int32)FMath::Max3(Texture.Source.GetSizeX(),Texture.Source.GetSizeY(),(int64)1);
	const int32 NumMipsSource = FMath::CeilLogTwo(SourceSize) + 1;

	// When the BaseTextureSize is known, skip mips until the texture is equal or smaller.
	if (BaseTextureSize > 0)
	{
		if (BaseTextureSize < SourceSize)
		{
			const int32 MaxNumMipsInGame = FMath::CeilLogTwo(BaseTextureSize) + 1;
			return FMath::Max(NumMipsSource - MaxNumMipsInGame, 0);
		}

		return 0;
	}

	const UTextureLODSettings& LODSettings = GenerationContext.CompilationContext.Options.TargetPlatform->GetTextureLODSettings();

	// Get the MaxTextureSize for the TargetPlatform.
	const int32 MaxTextureSize = GetMaxTextureSize(ReferenceTexture ? *ReferenceTexture : Texture, LODSettings);

	if (ReferenceTexture)
	{
		// Max size and number of mips from ReferenceTexture. 
		const int32 MaxRefSourceSize = (uint32)FMath::Max3(ReferenceTexture->Source.GetSizeX(), ReferenceTexture->Source.GetSizeY(), (int64)1);
		const int32 NumMipsRefSource = FMath::CeilLogTwo(MaxRefSourceSize) + 1;

		// Find the LODBias to apply to stay within the MaxTextureSize limit of the TargetPlatform
		const int32 PlatformLODBias = GetPlatformLODBias(MaxRefSourceSize, NumMipsRefSource, MaxTextureSize);

		// TextureSize in-game without any additional LOD bias.
		const int64 ReferenceTextureSize = MaxRefSourceSize >> PlatformLODBias;

		// Additional LODBias of the Texture
		const int32 ReferenceTextureLODBias = LODSettings.CalculateLODBias(ReferenceTextureSize, ReferenceTextureSize, 0,	ReferenceTexture->LODGroup,
			ReferenceTexture->LODBias, 0, ReferenceTexture->MipGenSettings, ReferenceTexture->IsCurrentlyVirtualTextured());

		return FMath::Max(NumMipsSource - NumMipsRefSource + PlatformLODBias + ReferenceTextureLODBias, 0);
	}

	// Find the LODBias to apply to stay within the MaxTextureSize limit of the TargetPlatform
	const int32 PlatformLODBias = GetPlatformLODBias(SourceSize, NumMipsSource, MaxTextureSize);

	// TextureSize in-game without any additional LOD bias.
	const int64 TextureSize = SourceSize >> PlatformLODBias;

	// Additional LODBias of the Texture
	const int32 TextureLODBias = LODSettings.CalculateLODBias(TextureSize, TextureSize, 0, Texture.LODGroup, Texture.LODBias, 0, Texture.MipGenSettings, Texture.IsCurrentlyVirtualTextured());

	return FMath::Max(PlatformLODBias + TextureLODBias, 0);
}


int32 GetMaxTextureSize(const UTexture2D& ReferenceTexture, const UTextureLODSettings& LODSettings)
{
	// Setting the maximum texture size
	FTextureLODGroup TextureGroupSettings = LODSettings.GetTextureLODGroup(ReferenceTexture.LODGroup);

	if (TextureGroupSettings.MaxLODSize > 0)
	{
		return ReferenceTexture.MaxTextureSize == 0 ? TextureGroupSettings.MaxLODSize : FMath::Min(TextureGroupSettings.MaxLODSize, ReferenceTexture.MaxTextureSize);
	}

	return ReferenceTexture.MaxTextureSize;
}


int32 GetTextureSizeInGame(const UTexture2D& Texture, const UTextureLODSettings& LODSettings)
{
	const int32 SourceSize = (uint32)FMath::Max3(Texture.Source.GetSizeX(), Texture.Source.GetSizeY(), (int64)1);
	const int32 NumMipsSource = FMath::CeilLogTwo(SourceSize) + 1;

	// Max size allowed on the TargetPlatform
	const int32 MaxTextureSize = GetMaxTextureSize(Texture, LODSettings);

	// Find the LODBias to apply to stay within the MaxTextureSize limit of the TargetPlatform
	const int32 PlatformLODBias = GetPlatformLODBias(SourceSize, NumMipsSource, MaxTextureSize);
	
	// MaxTextureSize in-game without any additional LOD bias.
	const int32 MaxTextureSizeAllowed = SourceSize >> PlatformLODBias;

	// Calculate the LODBias specific for this texture 
	const int32 TextureLODBias = LODSettings.CalculateLODBias(MaxTextureSizeAllowed, MaxTextureSizeAllowed, 0, Texture.LODGroup, Texture.LODBias, 0, Texture.MipGenSettings, Texture.IsCurrentlyVirtualTextured());

	return MaxTextureSizeAllowed >> TextureLODBias;
}


mu::FImageDesc GenerateImageDescriptor(UTexture* Texture)
{
	check(Texture);
	mu::FImageDesc ImageDesc;

	ImageDesc.m_size[0] = Texture->Source.GetSizeX();
	ImageDesc.m_size[1] = Texture->Source.GetSizeY();
	ImageDesc.m_lods = Texture->Source.GetNumMips();

	mu::EImageFormat MutableFormat = mu::EImageFormat::RGBA_UByte;
	ETextureSourceFormat SourceFormat = Texture->Source.GetFormat();
	switch (SourceFormat)
	{
	case ETextureSourceFormat::TSF_G8:
	case ETextureSourceFormat::TSF_G16:
	case ETextureSourceFormat::TSF_R16F:
	case ETextureSourceFormat::TSF_R32F:
		MutableFormat = mu::EImageFormat::L_UByte;
		break;

	default:
		break;
	}

	ImageDesc.m_format = MutableFormat;

	return ImageDesc;
}


TSharedPtr<mu::FImage> GenerateImageConstant(UTexture* Texture, FMutableGraphGenerationContext& GenerationContext, bool bIsReference)
{
	MUTABLE_CPUPROFILER_SCOPE(GenerateImageConstant);

	if (!Texture)
	{
		return nullptr;
	}

	bool bForceLoad = false;
	bool bIsCompileTime = false;
	if (!bIsReference)
	{
		bForceLoad = true;
		if (GenerationContext.CompilationContext.Options.OptimizationLevel == 0)
		{
			bIsCompileTime = false;
		}
		else
		{
			bIsCompileTime = true;
		}
	}

	// Create a descriptor for the image.
	// \TODO: If passthrough (bIsReference) we should apply lod bias, and max texture size to this desc.
	// For now it is not a problem because passthrough textures shouldn't mix with any other operations.
	mu::FImageDesc ImageDesc = GenerateImageDescriptor(Texture);

	FMutableGraphGenerationContext::FGeneratedReferencedTexture InvalidEntry;
	InvalidEntry.ID = TNumericLimits<uint32>::Max();

	FMutableGraphGenerationContext::FGeneratedReferencedTexture* Entry;
	int32 Num = 0;
	
	if (bIsReference)
	{
		Entry = &GenerationContext.PassthroughTextureMap.FindOrAdd(Texture, InvalidEntry);
		Num = GenerationContext.PassthroughTextureMap.Num();
	}
	else if (bIsCompileTime)
	{
        Entry = &GenerationContext.CompileTimeTextureMap.FindOrAdd(Texture, InvalidEntry);
		Num = GenerationContext.CompileTimeTextureMap.Num();
	}
	else
	{
		Entry = &GenerationContext.RuntimeReferencedTextureMap.FindOrAdd(Texture, InvalidEntry);
		Num = GenerationContext.RuntimeReferencedTextureMap.Num();
	}

	if (Entry->ID == TNumericLimits<uint32>::Max())
	{
		Entry->ID = Num - 1;
	}
	
	// Compile-time references that are left should be resolved immediately (should only happen in editor).
	TSharedPtr<mu::FImage> Result = mu::FImage::CreateAsReference(Entry->ID, ImageDesc, bForceLoad);
	return Result;
}


TSharedPtr<mu::FMesh> GenerateMeshConstant(
	const FMutableSourceMeshData& Source,
	FMutableGraphGenerationContext& GenerationContext)
{
	MUTABLE_CPUPROFILER_SCOPE(GenerateMeshConstant)

	if (Source.Mesh.IsNull())
	{
		return nullptr;
	}

	bool bForceLoad = false;
	bool bIsCompileTime = false;
	if (!Source.bIsPassthrough)
	{
		bForceLoad = true;
		// TODO: For now, even in none, we turn them into compile-time references, since runtime meshes are not
		// supported yet.
		//if (GenerationContext.CompilationContext.Options.OptimizationLevel == 0)
		//{
		//	bIsCompileTime = false;
		//}
		//else
		{
			bIsCompileTime = true;
		}
	}


	FMutableGraphGenerationContext::FGeneratedReferencedMesh InvalidEntry;
	InvalidEntry.ID = TNumericLimits<uint32>::Max();

	FMutableGraphGenerationContext::FGeneratedReferencedMesh* Entry = nullptr;
	int32 Num = 0;

	if (Source.bIsPassthrough)
	{
		Entry = &GenerationContext.PassthroughMeshMap.FindOrAdd(Source.Mesh, InvalidEntry);
		Num = GenerationContext.PassthroughMeshMap.Num();
	}
	else if (bIsCompileTime)
	{
		Entry = &GenerationContext.CompileTimeMeshMap.FindOrAdd(Source, InvalidEntry);
		Num = GenerationContext.CompileTimeMeshMap.Num();
	}
	else
	{
		Entry = &GenerationContext.RuntimeReferencedMeshMap.FindOrAdd(Source, InvalidEntry);
		Num = GenerationContext.RuntimeReferencedMeshMap.Num();
	}

	if (Entry->ID == TNumericLimits<uint32>::Max())
	{
		Entry->ID = Num - 1;
	}

	TSharedPtr<mu::FMesh> Result = mu::FMesh::CreateAsReference(Entry->ID, bForceLoad);
	return Result;
}


void AddSocketTagsToMesh(const USkeletalMesh* SourceMesh, TSharedPtr<mu::FMesh> MutableMesh, FMutableGraphGenerationContext& GenerationContext)
{
	for (int32 SocketIndex = 0; SocketIndex < SourceMesh->NumSockets(); ++SocketIndex)
	{
		USkeletalMeshSocket* Socket = SourceMesh->GetSocketByIndex(SocketIndex);

		FMutableRefSocket MutableSocket;
		MutableSocket.SocketName = Socket->SocketName;
		MutableSocket.BoneName = Socket->BoneName;
		MutableSocket.RelativeLocation = Socket->RelativeLocation;
		MutableSocket.RelativeRotation = Socket->RelativeRotation;
		MutableSocket.RelativeScale = Socket->RelativeScale;
		MutableSocket.bForceAlwaysAnimated = Socket->bForceAlwaysAnimated;

		MutableSocket.Priority = GenerationContext.SocketPriorityStack.IsEmpty() ? 0 : GenerationContext.SocketPriorityStack.Top();
			
		int32 SocketArrayIndex = GenerationContext.SocketArray.AddUnique(MutableSocket);
		FString SocketTag = FString::Printf(TEXT("__Socket:%d"), SocketArrayIndex);

		AddTagToMutableMeshUnique(*MutableMesh, SocketTag);
	}
}


FMutableComponentInfo::FMutableComponentInfo(FName InComponentName, USkeletalMesh* InRefSkeletalMesh)
{
	ComponentName = InComponentName;

	if (!InRefSkeletalMesh || !InRefSkeletalMesh->GetSkeleton())
	{
		return;
	}

	RefSkeletalMesh = TStrongObjectPtr<USkeletalMesh>(InRefSkeletalMesh);
	RefSkeleton = RefSkeletalMesh->GetSkeleton();

	const int32 NumBones = RefSkeleton->GetReferenceSkeleton().GetRawBoneNum();
	BoneNamesToPathHash.Reserve(NumBones);

	const TArray<FMeshBoneInfo>& Bones = RefSkeleton->GetReferenceSkeleton().GetRawRefBoneInfo();

	for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
	{
		const FMeshBoneInfo& Bone = Bones[BoneIndex];

		// Retrieve parent bone name and respective hash, root-bone is assumed to have a parent hash of 0
		const FName ParentName = Bone.ParentIndex != INDEX_NONE ? Bones[Bone.ParentIndex].Name : NAME_None;
		const uint32 ParentHash = Bone.ParentIndex != INDEX_NONE ? GetTypeHash(ParentName) : 0;

		// Look-up the path-hash from root to the parent bone
		const uint32* ParentPath = BoneNamesToPathHash.Find(ParentName);
		const uint32 ParentPathHash = ParentPath ? *ParentPath : 0;

		// Append parent hash to path to give full path hash to current bone
		const uint32 BonePathHash = HashCombine(ParentPathHash, ParentHash);

		// Add path hash to current bone
		BoneNamesToPathHash.Add(Bone.Name, BonePathHash);
	}
}


void FMutableComponentInfo::AccumulateBonesToRemovePerLOD(const TArray<FLODReductionSettings>& LODReductionSettings, int32 NumLODs)
{
	BonesToRemovePerLOD.SetNum(NumLODs);

	TMap<FName, bool> BonesToRemove;

	const int32 ComponentSettingsLODCount = LODReductionSettings.Num();
	for (int32 LODIndex = 0; LODIndex < NumLODs; ++LODIndex)
	{
		if (LODIndex < ComponentSettingsLODCount)
		{
			const FLODReductionSettings& LODReductionSetting = LODReductionSettings[LODIndex];

			for (const FBoneToRemove& Bone : LODReductionSetting.BonesToRemove)
			{
				if (bool* bOnlyRemoveChildren = BonesToRemove.Find(Bone.BoneName))
				{
					// Removed by a previous LOD
					*bOnlyRemoveChildren = (*bOnlyRemoveChildren) && Bone.bOnlyRemoveChildren;
				}
				else
				{
					BonesToRemove.Add(Bone.BoneName, Bone.bOnlyRemoveChildren);
				}
			}
		}
		
		BonesToRemovePerLOD[LODIndex] = BonesToRemove;
	}
}

#undef LOCTEXT_NAMESPACE

FMutableGraphGenerationContext::FSharedSurface::FSharedSurface(uint8 InLOD, const mu::Ptr<mu::NodeSurfaceNew>& InNodeSurfaceNew, TArray<const UCustomizableObjectNodeMacroInstance*> CurrentMacroContext)
{
	LOD = InLOD;
	NodeSurfaceNew = InNodeSurfaceNew;
	MacroContext = CurrentMacroContext;
}


bool FMutableGraphGenerationContext::FSharedSurface::operator==(const FSharedSurface& o) const
{
	return NodeModifierIDs == o.NodeModifierIDs;
}


FGeneratedMutableDataTableKey::FGeneratedMutableDataTableKey(FString TableName, FName VersionColumn, const TArray<FTableNodeCompilationFilter>& CompilationFilterOptions)
	: TableName(TableName), VersionColumn(VersionColumn), CompilationFilterOptions(CompilationFilterOptions)
{}


uint32 GetTypeHash(const FGeneratedMutableDataTableKey& Key)
{
	uint32 Hash = GetTypeHash(Key.TableName);
	Hash = HashCombine(Hash, GetTypeHash(Key.VersionColumn));

	for (int32 FilterIndex = 0; FilterIndex < Key.CompilationFilterOptions.Num(); ++FilterIndex)
	{
		Hash = HashCombine(Hash, GetTypeHash(Key.CompilationFilterOptions[FilterIndex].FilterColumn));

		for (int32 NameIndex = 0; NameIndex < Key.CompilationFilterOptions[FilterIndex].Filters.Num(); ++NameIndex)
		{
			Hash = HashCombine(Hash, GetTypeHash(Key.CompilationFilterOptions[FilterIndex].Filters[NameIndex]));
		}

		Hash = HashCombine(Hash, GetTypeHash(Key.CompilationFilterOptions[FilterIndex].OperationType));
	}

	return Hash;
}

