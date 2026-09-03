// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeModifierTransformInMesh.h"

#include "MuCOE/EdGraphSchema_CustomizableObject.h"

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


const TCHAR* UCustomizableObjectNodeModifierTransformInMesh::OutputPinName = TEXT("Modifier");
const TCHAR* UCustomizableObjectNodeModifierTransformInMesh::BoundingMeshPinName = TEXT("Bounding Mesh");
const TCHAR* UCustomizableObjectNodeModifierTransformInMesh::TransformPinName = TEXT("Transform");


FText UCustomizableObjectNodeModifierTransformInMesh::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Transform_Mesh_In_Mesh", "Transform Mesh In Mesh");
}

FText UCustomizableObjectNodeModifierTransformInMesh::GetTooltipText() const
{
	return LOCTEXT("Transform_Mesh_In_Mesh_Tooltip", "Applies a transform to the vertices of a mesh that is contained within the given bounding mesh");
}

void UCustomizableObjectNodeModifierTransformInMesh::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	UEdGraphPin* ClipMeshPin = CustomCreatePin(EGPD_Input, Schema->PC_Mesh, FName(BoundingMeshPinName));
	ClipMeshPin->bDefaultValueIsIgnored = true;

	UEdGraphPin* TransformPin = CustomCreatePin(EGPD_Input, Schema->PC_Transform, FName(TransformPinName));
	TransformPin->bDefaultValueIsIgnored = true;
	
	(void)CustomCreatePin(EGPD_Output, Schema->PC_Modifier, FName(OutputPinName));

	//Create Node Modifier Common Pins
	Super::AllocateDefaultPins(RemapPins);
}


void UCustomizableObjectNodeModifierTransformInMesh::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property == nullptr)
	{
		return;
	}
	
	if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UCustomizableObjectNodeModifierTransformInMesh, BoundingMeshTransform))
	{
		if (TransformChangedDelegate.IsBound())
		{
			TransformChangedDelegate.Broadcast(BoundingMeshTransform);
		}
	}
}


UEdGraphPin* UCustomizableObjectNodeModifierTransformInMesh::GetOutputPin() const
{
	return FindPin(OutputPinName);
}

UEdGraphPin* UCustomizableObjectNodeModifierTransformInMesh::GetBoundingMeshPin() const
{
	return FindPin(BoundingMeshPinName);
}

UEdGraphPin* UCustomizableObjectNodeModifierTransformInMesh::GetTransformPin() const
{
	return FindPin(TransformPinName);
}

#undef LOCTEXT_NAMESPACE
