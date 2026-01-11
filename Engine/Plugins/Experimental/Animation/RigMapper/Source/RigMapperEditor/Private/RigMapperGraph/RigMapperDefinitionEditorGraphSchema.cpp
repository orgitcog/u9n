// Copyright Epic Games, Inc. All Rights Reserved.


#include "RigMapperDefinitionEditorGraphSchema.h"

#include "RigMapperDefinition.h"
#include "RigMapperDefinitionEditorGraphNode.h"
#include "SRigMapperDefinitionGraphEditorNode.h"

#include "EdGraph/EdGraph.h"


#define LOCTEXT_NAMESPACE "RigMapperDefinitionEditorGraphSchema"


void URigMapperDefinitionEditorGraphSchema::GetContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const
{
	// Menu->AddDynamicSection("PhysicsAssetGraphSchema", FNewToolMenuDelegateLegacy::CreateLambda([](FMenuBuilder& MenuBuilder, UToolMenu* InMenu)
	// {
	// 	UGraphNodeContextMenuContext* ContextObject = InMenu->FindContext<UGraphNodeContextMenuContext>();
	// 	if (!ContextObject)
	// 	{
	// 		return;
	// 	}
	//
	// 	const UPhysicsAssetGraph* PhysicsAssetGraph = CastChecked<const UPhysicsAssetGraph>(ContextObject->Graph);
	// 	TSharedPtr<FPhysicsAssetEditorSharedData> SharedData = PhysicsAssetGraph->GetPhysicsAssetEditor()->GetSharedData();
	//
	// 	if (const UPhysicsAssetGraphNode_Constraint* ConstraintNode = Cast<const UPhysicsAssetGraphNode_Constraint>(ContextObject->Node))
	// 	{
	// 		PhysicsAssetGraph->GetPhysicsAssetEditor()->BuildMenuWidgetConstraint(MenuBuilder);
	// 	}
	// 	else if (const UPhysicsAssetGraphNode_Bone* BoneNode = Cast<const UPhysicsAssetGraphNode_Bone>(ContextObject->Node))
	// 	{
	// 		PhysicsAssetGraph->GetPhysicsAssetEditor()->BuildMenuWidgetBody(MenuBuilder);
	// 	}
	//
	// 	PhysicsAssetGraph->GetPhysicsAssetEditor()->BuildMenuWidgetSelection(MenuBuilder);
	// }));
}

FLinearColor URigMapperDefinitionEditorGraphSchema::GetPinTypeColor(const FEdGraphPinType& PinType) const
{
	return FLinearColor::White;
}

void URigMapperDefinitionEditorGraphSchema::BreakPinLinks(UEdGraphPin& TargetPin, bool bSendsNodeNotification) const
{
	// Don't allow breaking any links
}

void URigMapperDefinitionEditorGraphSchema::BreakSinglePinLink(UEdGraphPin* SourcePin, UEdGraphPin* TargetPin) const
{
	// Don't allow breaking any links
}

FPinConnectionResponse URigMapperDefinitionEditorGraphSchema::MovePinLinks(UEdGraphPin& MoveFromPin, UEdGraphPin& MoveToPin, bool bIsIntermediateMove, bool bNotifyLinkedNodes) const
{
	// Don't allow moving any links
	return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, TEXT(""));
}

FPinConnectionResponse URigMapperDefinitionEditorGraphSchema::CopyPinLinks(UEdGraphPin& CopyFromPin, UEdGraphPin& CopyToPin, bool bIsIntermediateCopy) const
{
	// Don't allow copying any links
	return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, TEXT(""));
}

FConnectionDrawingPolicy* URigMapperDefinitionEditorGraphSchema::CreateConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID, float InZoomFactor, const FSlateRect& InClippingRect, class FSlateWindowElementList& InDrawElements, class UEdGraph* InGraphObj) const
{
	return new ConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, InZoomFactor, InClippingRect, InDrawElements);
}

const FPinConnectionResponse URigMapperDefinitionEditorGraphSchema::CanCreateConnection(const UEdGraphPin* A, const UEdGraphPin* B) const
{
	return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("RigMapperDefinitionEditor_CannotCreateConnection", "It is currently not possible to create new connections from the graph"));
}

FPinConnectionResponse URigMapperDefinitionEditorGraphSchema::CanCreateNewNodes(UEdGraphPin* InSourcePin) const
{
	// if(URigMapperDefinitionEditorGraphNode* Node = Cast<URigMapperDefinitionEditorGraphNode>(InSourcePin->GetOwningNode()))
	// {
	// 	if(InSourcePin->Direction == EGPD_Output)
	// 	{
	// 		return FPinConnectionResponse(CONNECT_RESPONSE_MAKE, LOCTEXT("MakeANewConstraint", "Create a new constraint"));
	// 	}
	// }

	return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("RigMapperDefinitionEditor_CannotCreateNode", "It is currently not possible to create new nodes from the graph"));
}

bool URigMapperDefinitionEditorGraphSchema::SupportsDropPinOnNode(UEdGraphNode* InTargetNode, const FEdGraphPinType& InSourcePinType, EEdGraphPinDirection InSourcePinDirection, FText& OutErrorMessage) const
{
	OutErrorMessage = LOCTEXT("RigMapperDefinitionEditor_CannotCreateConnection_DropPinOnNode", "It is currently not possible to create new connections or nodes from the graph");
	return false;
}

#undef LOCTEXT_NAMESPACE
