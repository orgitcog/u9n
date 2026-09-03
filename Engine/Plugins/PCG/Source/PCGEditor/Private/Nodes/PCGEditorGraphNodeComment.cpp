// Copyright Epic Games, Inc. All Rights Reserved.

#include "Nodes/PCGEditorGraphNodeComment.h"

#include "PCGEditorGraph.h"
#include "Misc/TransactionObjectEvent.h"

void UPCGEditorGraphNodeComment::PostTransacted(const FTransactionObjectEvent& TransactionEvent)
{
	Super::PostTransacted(TransactionEvent);

	// If anything happens, we need to dirty the pcg graph, as it will need to serialize the changes
	if (TransactionEvent.HasPropertyChanges())
	{
		UPCGEditorGraph* PCGEditorGraph = CastChecked<UPCGEditorGraph>(GetGraph());
		if (UPCGGraph* PCGGraph = PCGEditorGraph->GetPCGGraph(); ensure(PCGGraph))
		{
			PCGGraph->Modify();	
		}
	}
}

void UPCGEditorGraphNodeComment::InitializeFromNodeData(const FPCGGraphCommentNodeData& NodeData)
{
	NodePosX = NodeData.NodePosX;
	NodePosY = NodeData.NodePosY;
	NodeWidth = NodeData.NodeWidth;
	NodeHeight = NodeData.NodeHeight;
	NodeComment = NodeData.NodeComment;
	CommentColor = NodeData.CommentColor;
	FontSize = NodeData.FontSize;
	bCommentBubbleVisible_InDetailsPanel = NodeData.bCommentBubbleVisible_InDetailsPanel;
	bColorCommentBubble = NodeData.bColorCommentBubble;
	MoveMode = static_cast<ECommentBoxMode::Type>(NodeData.MoveMode);
	NodeDetails = NodeData.NodeDetails;
	CommentDepth = NodeData.CommentDepth;
	NodeGuid = NodeData.GUID;
	bCommentBubblePinned = NodeData.bCommentBubblePinned;
	bCommentBubbleVisible = NodeData.bCommentBubbleVisible;
}
