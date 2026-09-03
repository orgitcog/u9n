// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundBasicNode.h"

#include "MetasoundNodeInterface.h"
#include "Templates/SharedPointer.h"

namespace Metasound
{
	FBasicNode::FBasicNode(FNodeData InNodeData, TSharedRef<const FNodeClassMetadata> InClassMetadata)
	: NodeData(MoveTemp(InNodeData))
	, ClassMetadata(MoveTemp(InClassMetadata))
	{
	}

	/** Return the name of this specific instance of the node class. */
	const FName& FBasicNode::GetInstanceName() const
	{
		return NodeData.Name;
	}

	/** Return the ID of this specific instance of the node class. */
	const FGuid& FBasicNode::GetInstanceID() const
	{
		return NodeData.ID;
	}

	/** Return the type name of this node. */
	const FNodeClassMetadata& FBasicNode::GetMetadata() const
	{
		return *ClassMetadata;
	}

	const FVertexInterface& FBasicNode::GetVertexInterface() const
	{
		return NodeData.Interface;
	}
	
	TSharedPtr<const IOperatorData> FBasicNode::GetOperatorData() const 
	{
		return NodeData.OperatorData;
	}

	void FBasicNode::SetDefaultInput(const FVertexName& InVertexName, const FLiteral& InLiteral)
	{
		if (FInputDataVertex* Vertex = NodeData.Interface.GetInputInterface().Find(InVertexName))
		{
			Vertex->SetDefaultLiteral(InLiteral);
		}
		else
		{
			UE_LOG(LogMetaSound, Error, TEXT("Could not set default input. Could not find input vertex %s on node %s of node class %s"), *InVertexName.ToString(), *NodeData.Name.ToString(), *ClassMetadata->ClassName.ToString());
		}
	}
}
