// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGEditorGraphNodeFactory.h"

#include "PCGEditorGraphNodeBase.h"
#include "SPCGEditorGraphNode.h"

TSharedPtr<SGraphNode> FPCGEditorGraphNodeFactory::CreateNode(UEdGraphNode* InNode) const
{
	if (TSharedPtr<SGraphNode> VisualNode = InNode->CreateVisualWidget())
	{
		VisualNode->SlatePrepass();
		return VisualNode;
	}

	return nullptr;
}
