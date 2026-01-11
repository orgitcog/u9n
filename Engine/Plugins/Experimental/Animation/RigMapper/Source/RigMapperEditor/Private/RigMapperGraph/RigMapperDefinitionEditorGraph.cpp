// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigMapperDefinitionEditorGraph.h"

#include "RigMapperDefinition.h"
#include "RigMapperDefinitionEditorGraphNode.h"
#include "RigMapperDefinitionEditorGraphSchema.h"

URigMapperDefinitionEditorGraph::URigMapperDefinitionEditorGraph(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	Schema = URigMapperDefinitionEditorGraphSchema::StaticClass();
}

void URigMapperDefinitionEditorGraph::Initialize(URigMapperDefinition* InDefinition)
{
	WeakDefinition = InDefinition;
}

void URigMapperDefinitionEditorGraph::RebuildGraph()
{
	RemoveAllNodes();
	ConstructNodes();
	NotifyGraphChanged();
	RequestRefreshLayout(true);
}

void URigMapperDefinitionEditorGraph::ConstructNodes()
{
	if (!WeakDefinition.IsValid())
	{
		return;
	}
	URigMapperDefinition* Definition = WeakDefinition.Get();
	
	for (const TPair<FString, FString>& Output : Definition->Outputs)
	{
		CreateGraphNodesRec(Definition, Output.Key, true);
	}

	// Generate nodes not related to any output
	TArray<FString> FeatureNames;
	Definition->Features.GetFeatureNames(FeatureNames);
	for (const FString& Feature : FeatureNames)
	{
		if (!FeatureNodes.Contains(Feature))
		{
			CreateGraphNodesRec(Definition, Feature, false);
		}
	}
	for (const FString& Input : Definition->Inputs)
	{
		if (!InputNodes.Contains(Input))
		{
			CreateGraphNode(Input, ERigMapperNodeType::Input);
		}
	}
	for (const FString& NullOutput : Definition->NullOutputs)
	{
		if (!NullOutputNodes.Contains(NullOutput))
		{
			CreateGraphNode(NullOutput, ERigMapperNodeType::NullOutput);
		}
	}
}

TArray<URigMapperDefinitionEditorGraphNode*> URigMapperDefinitionEditorGraph::GetNodesByName(const TArray<FString>& Inputs, const TArray<FString>& Features, const TArray<FString>& Outputs, const TArray<FString>& NullOutputs) const
{
	TArray<URigMapperDefinitionEditorGraphNode*> OutNodes;
	OutNodes.Reserve(Inputs.Num() + Features.Num() + Outputs.Num() + NullOutputs.Num());

	auto AddNodesByNameFromMap = [&OutNodes](const TArray<FString>& InArray, TMap<FString, URigMapperDefinitionEditorGraphNode*> InMap)
	{
		for (const FString& NodeName : InArray)
		{
			if (URigMapperDefinitionEditorGraphNode** Node = InMap.Find(NodeName))
			{
				OutNodes.Add(*Node);
			}
		}
	};

	AddNodesByNameFromMap(Inputs, InputNodes);
	AddNodesByNameFromMap(Features, FeatureNodes);
	AddNodesByNameFromMap(Outputs, OutputNodes);
	AddNodesByNameFromMap(NullOutputs, NullOutputNodes);

	return OutNodes;
}

URigMapperDefinitionEditorGraphNode* URigMapperDefinitionEditorGraph::CreateGraphNodesRec(URigMapperDefinition* Definition, const FString& NodeName, bool bIsOutputNode)
{
	// No need to do anything is node was already created. Output nodes should not be referenced by other nodes and should only get created once.
	if (!bIsOutputNode)
	{
		URigMapperDefinitionEditorGraphNode** ExistingNode = InputNodes.Find(NodeName);
		if (!ExistingNode)
		{
			ExistingNode = FeatureNodes.Find(NodeName);
		}
		if (ExistingNode)
		{
			return *ExistingNode;		
		}
	}

	// Node was not created (referenced by a previously created node)
	if (bIsOutputNode)
	{
		return CreateOutputNode(Definition, NodeName);
	}
	if (Definition->Inputs.Contains(NodeName))
	{
		return CreateGraphNode(NodeName, ERigMapperNodeType::Input); 
	}
	return CreateFeatureNode(Definition, NodeName);
}

URigMapperDefinitionEditorGraphNode* URigMapperDefinitionEditorGraph::CreateOutputNode(URigMapperDefinition* Definition, const FString& NodeName)
{
	if (const FString* LinkedInputName = Definition->Outputs.Find(NodeName))
	{
		URigMapperDefinitionEditorGraphNode* Node = CreateGraphNode(NodeName, ERigMapperNodeType::Output);
		
		if (URigMapperDefinitionEditorGraphNode* LinkedNode = CreateGraphNodesRec(Definition, *LinkedInputName, false))
		{
			LinkGraphNodes(LinkedNode, Node);
		}

		return Node;
	}
	return nullptr;
}

URigMapperDefinitionEditorGraphNode* URigMapperDefinitionEditorGraph::CreateFeatureNode(URigMapperDefinition* Definition, const FString& NodeName)
{
	ERigMapperFeatureType FeatureType;

	if (const FRigMapperFeature* Feature = Definition->Features.Find(NodeName, FeatureType))
	{
		const ERigMapperNodeType NodeType = static_cast<ERigMapperNodeType>(static_cast<uint8>(FeatureType));
		URigMapperDefinitionEditorGraphNode* Node = CreateGraphNode(NodeName, NodeType);
			
		TArray<FString> FeatureInputNames;
		Feature->GetInputs(FeatureInputNames);
	
		for (const FString& FeatureInput : FeatureInputNames)
		{
			if (URigMapperDefinitionEditorGraphNode* LinkedNode = CreateGraphNodesRec(Definition, FeatureInput, false))
			{
				LinkGraphNodes(LinkedNode, Node);
			}
		}
		
		return Node;
	}
	return nullptr;
}

void URigMapperDefinitionEditorGraph::LinkGraphNodes(URigMapperDefinitionEditorGraphNode* InNode, URigMapperDefinitionEditorGraphNode* OutNode)
{
	if (InNode != OutNode)
	{
		UEdGraphPin* InPin = OutNode->CreateInputPin();
		InPin->bHidden = false;

		UEdGraphPin* OutPin = InNode->GetOutputPin();
		if (!OutPin)
		{
			OutPin = InNode->CreateOutputPin();
			OutPin->bHidden = false;
		}
		if (OutPin)
		{
			OutPin->MakeLinkTo(InPin);
		}
	}
}
		

URigMapperDefinitionEditorGraphNode* URigMapperDefinitionEditorGraph::CreateGraphNode(const FString& NodeName, ERigMapperNodeType NodeType)
{
	if (!WeakDefinition.IsValid())
	{
		return nullptr;
	}
	
	constexpr bool bSelectNewNode = false;
	
	FGraphNodeCreator<URigMapperDefinitionEditorGraphNode> GraphNodeCreator(*this);
	URigMapperDefinitionEditorGraphNode* Node = GraphNodeCreator.CreateNode(bSelectNewNode);
	GraphNodeCreator.Finalize();

	Node->SetupNode(WeakDefinition.Get(), NodeName, NodeType);

	if (NodeType == ERigMapperNodeType::Input)
	{
		InputNodes.Add(NodeName, Node);
	}
	else if (NodeType == ERigMapperNodeType::Output)
	{
		OutputNodes.Add(NodeName, Node);
	}
	else if (NodeType == ERigMapperNodeType::NullOutput)
	{
		NullOutputNodes.Add(NodeName, Node);
	}
	else // todo: Add feature type
	{
		FeatureNodes.Add(NodeName, Node);
	}
	
	return Node;
}

void URigMapperDefinitionEditorGraph::LayoutNodeRec(URigMapperDefinitionEditorGraphNode* InNode, double InputsWidth, double PosY, TArray<URigMapperDefinitionEditorGraphNode*>& LayedOutNodes) const
{
	const int32 NodeMarginX = 20;
	const int32 NodeMarginY = 5;
	
	URigMapperDefinitionEditorGraphNode* LinkedNode = nullptr;
	double SubGraphHeight = 0;
	
	for (UEdGraphPin* InPin : InNode->GetInputPins())
	{
		for (const UEdGraphPin* OutPin : InPin->LinkedTo)
		{
			LinkedNode = Cast<URigMapperDefinitionEditorGraphNode>(OutPin->GetOwningNode());
			if (!LayedOutNodes.Contains(LinkedNode))
			{
				LayedOutNodes.Add(LinkedNode);

				const double DesiredPosY =  PosY + SubGraphHeight;
				LayoutNodeRec(LinkedNode, InputsWidth, DesiredPosY, LayedOutNodes);
				SubGraphHeight += LinkedNode->GetDimensions().Y + LinkedNode->GetMargin().Y + (LinkedNode->NodePosY - DesiredPosY);
			}

			const double TargetPosX = LinkedNode->NodePosX + LinkedNode->GetDimensions().X + LinkedNode->GetMargin().X; 
			if (TargetPosX > InNode->NodePosX)
			{
				InNode->NodePosX = TargetPosX;
			}
		}
	}
	
	InNode->NodePosY = PosY;

	const FVector2D& Dimensions = InNode->GetDimensions();
	FVector2D Margin = { NodeMarginX, NodeMarginY };
	
	if (InNode->GetNodeType() == ERigMapperNodeType::Input)
	{
		InNode->NodePosX = 0;
		Margin.X += InputsWidth - Dimensions.X;
	}
	else if (InNode->GetNodeType() == ERigMapperNodeType::Output)
	{
		Margin.X = 0;
	}
	else if (InNode->GetNodeType() == ERigMapperNodeType::NullOutput)
	{
		Margin.X = 0;
	}
	else
	{
		InNode->NodePosX = FMath::Max(InNode->NodePosX, InputsWidth);
	}
	if (SubGraphHeight > Dimensions.Y + Margin.Y)
	{
		const double Offset = SubGraphHeight / 2 - (Dimensions.Y + Margin.Y) / 2;

		Margin.Y += Offset;
		InNode->NodePosY += Offset;
	}

	InNode->SetMargin(Margin);
}

void URigMapperDefinitionEditorGraph::LayoutNodes() const
{
	double InputsMaxWidth = 0;

	const int32 InputMarginX = 50;
	for (const TPair<FString, URigMapperDefinitionEditorGraphNode*>& InputNode : InputNodes)
	{
		const FVector2D& NodeDimensions = InputNode.Value->GetDimensions();

		if (NodeDimensions.X > InputsMaxWidth)
		{
			InputsMaxWidth = NodeDimensions.X;
		}
	}
	InputsMaxWidth += InputMarginX;

	double MaxPosX = 0;
	TArray<URigMapperDefinitionEditorGraphNode*> LayedOutNodes;

	double PosY = 0;
	const int32 SubGraphMarginY = 25;
	
	for (const TPair<FString, URigMapperDefinitionEditorGraphNode*>& Output : OutputNodes)
	{
		LayoutNodeRec(Output.Value, InputsMaxWidth, PosY, LayedOutNodes);
		PosY = Output.Value->NodePosY + Output.Value->GetDimensions().Y + Output.Value->GetMargin().Y + SubGraphMarginY;
		if (Output.Value->NodePosX > MaxPosX)
		{
			MaxPosX = Output.Value->NodePosX;
		}
	}
	
	const int32 OutputMarginX = 50;
	for (const TPair<FString, URigMapperDefinitionEditorGraphNode*>& Output : OutputNodes)
	{
		Output.Value->NodePosX = MaxPosX + OutputMarginX;
		// for (UEdGraphPin* InPin : Output.Value->GetInputPins())
		// {
		// 	for (const UEdGraphPin* OutPin : InPin->LinkedTo)
		// 	{
		// 		URigMapperDefinitionEditorGraphNode* LinkedNode = Cast<URigMapperDefinitionEditorGraphNode>(OutPin->GetOwningNode());
		//
		// 		if (!LinkedNode->GetInputPins().IsEmpty())
		// 		{
		// 			LinkedNode->NodePosX = Output.Value->NodePosX - (LinkedNode->GetDimensions().X + LinkedNode->GetMargin().X + OutputMarginX);
		// 		}
		// 	}
		// }
	}

	// Layout nodes not related to any output
	for (const TPair<FString, URigMapperDefinitionEditorGraphNode*>& Node : FeatureNodes)
	{
		if (!LayedOutNodes.Contains(Node.Value))
		{
			LayoutNodeRec(Node.Value, InputsMaxWidth, PosY, LayedOutNodes);
			PosY = Node.Value->NodePosY + Node.Value->GetDimensions().Y + Node.Value->GetMargin().Y + SubGraphMarginY;
		}
	}
	for (const TPair<FString, URigMapperDefinitionEditorGraphNode*>& Node : InputNodes)
	{
		if (!LayedOutNodes.Contains(Node.Value))
		{
			LayoutNodeRec(Node.Value, InputsMaxWidth, PosY, LayedOutNodes);
			PosY = Node.Value->NodePosY + Node.Value->GetDimensions().Y + Node.Value->GetMargin().Y + SubGraphMarginY;
		}
	}

	for (const TPair<FString, URigMapperDefinitionEditorGraphNode*>& Node : NullOutputNodes)
	{
		if (!LayedOutNodes.Contains(Node.Value))
		{
			Node.Value->NodePosX = MaxPosX + OutputMarginX;
			LayoutNodeRec(Node.Value, InputsMaxWidth, PosY, LayedOutNodes);
			PosY = Node.Value->NodePosY + Node.Value->GetDimensions().Y + Node.Value->GetMargin().Y + SubGraphMarginY;
		}
	}
}

void URigMapperDefinitionEditorGraph::RemoveAllNodes()
{
	TArray<UEdGraphNode*> NodesToRemove = Nodes;
	
	for (int32 NodeIndex = 0; NodeIndex < NodesToRemove.Num(); ++NodeIndex)
	{
		RemoveNode(NodesToRemove[NodeIndex]);
	}

	Nodes.Reset();
	InputNodes.Reset();
	FeatureNodes.Reset();
	OutputNodes.Reset();
	NullOutputNodes.Reset();
}
