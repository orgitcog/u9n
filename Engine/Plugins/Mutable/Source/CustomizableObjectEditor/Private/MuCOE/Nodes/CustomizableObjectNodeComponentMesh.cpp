// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeComponentMesh.h"

#include "MuCO/CustomizableObjectCustomVersion.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "MuCOE/GraphTraversal.h"


#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


UCustomizableObjectNodeComponentMesh::UCustomizableObjectNodeComponentMesh()
{
	const FString CVarName = TEXT("r.SkeletalMesh.MinLodQualityLevel");
	const FString ScalabilitySectionName = TEXT("ViewDistanceQuality");
	LODSettings.MinQualityLevelLOD.SetQualityLevelCVarForCooking(*CVarName, *ScalabilitySectionName);
}


void UCustomizableObjectNodeComponentMesh::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if (!PropertyThatChanged)
	{
		return;
	}

	if (PropertyThatChanged->GetFName() == GET_MEMBER_NAME_CHECKED(UCustomizableObjectNodeComponentMesh, NumLODs))
	{
		LODReductionSettings.SetNum(NumLODs);
		
		ReconstructNode();
	}
}


void UCustomizableObjectNodeComponentMesh::BackwardsCompatibleFixup(int32 CustomizableObjectCustomVersion)
{
	Super::BackwardsCompatibleFixup(CustomizableObjectCustomVersion);
	
	if (CustomizableObjectCustomVersion == FCustomizableObjectCustomVersion::ComponentsArray)
	{
		UCustomizableObject* Object = GraphTraversal::GetObject(*this);

		if (FMutableMeshComponentData* Result = Object->GetPrivate()->MutableMeshComponents_DEPRECATED.FindByPredicate([&](const FMutableMeshComponentData& ComponentData)
		{
			return ComponentData.Name == ComponentName;
		}))
		{
			ReferenceSkeletalMesh = Result->ReferenceSkeletalMesh;
		}
	}
}


void UCustomizableObjectNodeComponentMesh::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	// Mesh Component Pins
	const FString PinFriendlyName = TEXT("Overlay Material");
	const FString PinName = PinFriendlyName + FString(TEXT("_Input_Pin"));
	UEdGraphPin* OverlayMaterialPin = CustomCreatePin(EGPD_Input, UEdGraphSchema_CustomizableObject::PC_MaterialAsset, FName(*PinName));
	OverlayMaterialPin->PinFriendlyName = FText::FromString(PinFriendlyName);
	OverlayMaterialPin->bDefaultValueIsIgnored = true;
	OverlayMaterialPin->PinToolTip = "Pin for an Overlay Material from a Table Node";
	
	// Base Mesh Interface Pins
	LODPins.Empty(NumLODs);
	for (int32 LODIndex = 0; LODIndex < NumLODs; ++LODIndex)
	{
		FString LODName = FString::Printf(TEXT("LOD %d"), LODIndex);

		UEdGraphPin* Pin = CustomCreatePin(EGPD_Input, UEdGraphSchema_CustomizableObject::PC_Material, FName(*LODName), true);
		LODPins.Add(Pin);
	}

	Super::AllocateDefaultPins(RemapPins);
}


FText UCustomizableObjectNodeComponentMesh::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	FText NodeTitle;
	FFormatNamedArguments Args;
	Args.Add(TEXT("ComponentName"), FText::FromName(ComponentName));

	if (TitleType == ENodeTitleType::ListView || (GetComponentNamePin() && GetComponentNamePin()->LinkedTo.Num()))
	{
		NodeTitle = LOCTEXT("Component_Mesh", "Mesh Component");
	}
	else if (TitleType == ENodeTitleType::EditableTitle)
	{
		NodeTitle = LOCTEXT("ComponentMesh_Edit", "{ComponentName}");
	}
	else
	{
		NodeTitle = LOCTEXT("ComponentMesh_Title", "{ComponentName}\nMesh Component");
	}

	return FText::Format(NodeTitle, Args);
}


TSoftObjectPtr<UMaterialInterface> UCustomizableObjectNodeComponentMesh::GetOverlayMaterial() const
{
	return OverlayMaterial;
}


UEdGraphPin* UCustomizableObjectNodeComponentMesh::GetOverlayMaterialAssetPin() const
{
	const FString PinFriendlyName = TEXT("Overlay Material");
	const FString PinName = PinFriendlyName + FString(TEXT("_Input_Pin"));

	if (UEdGraphPin* Pin = FindPin(PinName))
	{
		return Pin;
	}
	else
	{
		return FindPin(PinFriendlyName);
	}
}


bool UCustomizableObjectNodeComponentMesh::IsSingleOutputNode() const
{
	// todo UE-225446 : By limiting the number of connections this node can have we avoid a check failure. However, this method should be
	// removed in the future and the inherent issue with 1:n output connections should be fixed in its place
	return true;
}


int32 UCustomizableObjectNodeComponentMesh::GetNumLODs()
{
	return NumLODs;
}


ECustomizableObjectAutomaticLODStrategy UCustomizableObjectNodeComponentMesh::GetAutoLODStrategy()
{
	return AutoLODStrategy;
}


const TArray<FEdGraphPinReference>& UCustomizableObjectNodeComponentMesh::GetLODPins() const
{
	return LODPins;
}


UEdGraphPin* UCustomizableObjectNodeComponentMesh::GetOutputPin() const
{
	return OutputPin.Get();
}


void UCustomizableObjectNodeComponentMesh::SetOutputPin(const UEdGraphPin* Pin)
{
	OutputPin = Pin;
}


 const UCustomizableObjectNode* UCustomizableObjectNodeComponentMesh::GetOwningNode() const
{
	 return this;
}

#undef LOCTEXT_NAMESPACE

