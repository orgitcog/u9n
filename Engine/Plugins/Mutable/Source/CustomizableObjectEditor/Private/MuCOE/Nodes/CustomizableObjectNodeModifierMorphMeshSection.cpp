// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeModifierMorphMeshSection.h"

#include "Engine/SkeletalMesh.h"
#include "MuCO/CustomizableObjectCustomVersion.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "MuCOE/GraphTraversal.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMaterial.h"
#include "MuCOE/Nodes/CustomizableObjectNodeSkeletalMesh.h"
#include "MuCOE/CustomizableObjectMacroLibrary/CustomizableObjectMacroLibrary.h"

class UCustomizableObjectNodeRemapPins;
struct FPropertyChangedEvent;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


void UCustomizableObjectNodeModifierMorphMeshSection::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	FString PinName = TEXT("Modifier");
	UEdGraphPin* ModifierPin = CustomCreatePin(EGPD_Output, Schema->PC_Modifier, FName(PinName));
	ModifierPin->bDefaultValueIsIgnored = true;

	PinName = TEXT("Factor");
	UEdGraphPin* FactorPin = CustomCreatePin(EGPD_Input, Schema->PC_Float, FName(*PinName));
	FactorPin->bDefaultValueIsIgnored = true;

	PinName = TEXT("Morph Target Name");
	MorphTargetNamePinRef = CustomCreatePin(EGPD_Input, Schema->PC_String, FName(*PinName));

	//Create Node Modifier Common Pins
	Super::AllocateDefaultPins(RemapPins);
}


FText UCustomizableObjectNodeModifierMorphMeshSection::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Morph_MeshSection", "Morph Mesh Section");
}


FString UCustomizableObjectNodeModifierMorphMeshSection::GetRefreshMessage() const
{
	return "Morph Target not found in the SkeletalMesh. Please Refresh Node and select a valid morph option.";
}


FText UCustomizableObjectNodeModifierMorphMeshSection::GetTooltipText() const
{
	return LOCTEXT("Morph_Material_Tooltip", "Fully activate one morph of a parent's material.");
}


bool UCustomizableObjectNodeModifierMorphMeshSection::IsSingleOutputNode() const
{
	return true;
}


UEdGraphPin* UCustomizableObjectNodeModifierMorphMeshSection::MorphTargetNamePin() const
{
	return MorphTargetNamePinRef.Get();
}


bool UCustomizableObjectNodeModifierMorphMeshSection::CreateStaticStringPinWidget() const
{
	return false;
}


void UCustomizableObjectNodeModifierMorphMeshSection::BackwardsCompatibleFixup(int32 CustomizableObjectCustomVersion)
{
	Super::BackwardsCompatibleFixup(CustomizableObjectCustomVersion);

	if (CustomizableObjectCustomVersion == FCustomizableObjectCustomVersion::EnableMutableMacrosNewVersion)
	{
		if (!MorphTargetNamePinRef.Get())
		{
			MorphTargetNamePinRef = CustomCreatePin(EGPD_Input, UEdGraphSchema_CustomizableObject::PC_String, FName("Morph Target Name"));
		}
	}
}


#undef LOCTEXT_NAMESPACE
