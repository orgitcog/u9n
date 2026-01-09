// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextGraphPanelPinFactory.h"

#include "NodeFactory.h"
#include "SGraphPinModuleEvent.h"
#include "EdGraph/RigVMEdGraphNode.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

TSharedPtr<SGraphPin> FAnimNextGraphPanelPinFactory::CreatePin(UEdGraphPin* InPin) const
{
	if (URigVMEdGraphNode* RigNode = Cast<URigVMEdGraphNode>(InPin->GetOwningNode()))
	{
		URigVMPin* ModelPin = RigNode->GetModelPinFromPinPath(InPin->GetName());
		if (ModelPin)
		{
			static const FString META_AnimNextModuleEvent("AnimNextModuleEvent");
			static const FName META_CustomWidget("CustomWidget");
			if(InPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Name && ModelPin->GetMetaData(META_CustomWidget) == META_AnimNextModuleEvent)
			{
				return SNew(UE::AnimNext::Editor::SGraphPinModuleEvent, InPin);
			}
		}
	}

	return nullptr;
}
