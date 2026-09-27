// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR
#include "MaterialX/MaterialXUtils/MaterialXSurfaceShaderAbstract.h"

#include "InterchangeImportLog.h"
#include "InterchangeTexture2DNode.h"
#include "InterchangeTextureBlurNode.h"
#include "Materials/MaterialExpressionTransform.h"
#include "Materials/MaterialExpressionVectorNoise.h"
#include "MaterialX/MaterialXUtils/MaterialXManager.h"
#include "MaterialX/MaterialExpressions/MaterialExpressionTextureSampleParameterBlur.h"

#define LOCTEXT_NAMESPACE "MaterialXSurfaceShaderAbstract"

namespace mx = MaterialX;

FString FMaterialXSurfaceShaderAbstract::EmptyString{};
FString FMaterialXSurfaceShaderAbstract::DefaultOutput{TEXT("out")};


/**
* Get the normal input of a surfaceshader, used to plug it in the displacementshader
* @param SurfaceShaderNode - The surfaceshader that we return the input from
* @return the input of the normal
*/

FMaterialXSurfaceShaderAbstract::FMaterialXSurfaceShaderAbstract(UInterchangeBaseNodeContainer& BaseNodeContainer)
	: FMaterialXBase{ BaseNodeContainer }
	, ShaderGraphNode{ nullptr }
	, bTangentSpaceInput{ false }
{}

bool FMaterialXSurfaceShaderAbstract::AddAttribute(MaterialX::InputPtr Input, const FString& InputChannelName, UInterchangeShaderNode* ShaderNode, int32 OutputIndex)
{
	using namespace UE::Interchange::Materials::Standard::Nodes;
	if(Input)
	{
		if(Input->getType() == mx::Type::Boolean)
		{
			return AddBooleanAttribute(Input, InputChannelName, ShaderNode);
		}
		else if(Input->getType() == mx::Type::Float)
		{
			return AddFloatAttribute(Input, InputChannelName, ShaderNode);
		}
		else if(Input->getType() == mx::Type::Integer) //Let's add Float attribute, because Interchange doesn't create a scalar if it's an int
		{
			return AddIntegerAttribute(Input, InputChannelName, ShaderNode);
		}
		else if(Input->getType() == mx::Type::Color3 || Input->getType() == mx::Type::Color4)
		{
			return AddLinearColorAttribute(Input, InputChannelName, ShaderNode, FLinearColor{ std::numeric_limits<float>::max(),std::numeric_limits<float>::max(),std::numeric_limits<float>::max() }, OutputIndex);
		}
		else if(Input->getType() == mx::Type::Vector2)
		{
			return AddVector2Attribute(Input, InputChannelName, ShaderNode, FVector2f{ std::numeric_limits<float>::max() ,std::numeric_limits<float>::max() }, OutputIndex);
		}
		else if(Input->getType() == mx::Type::Vector3 || Input->getType() == mx::Type::Vector4)
		{
			return AddVectorAttribute(Input, InputChannelName, ShaderNode, FVector4f{ std::numeric_limits<float>::max(),std::numeric_limits<float>::max(),std::numeric_limits<float>::max() }, OutputIndex);
		}
	}

	return false;
}

bool FMaterialXSurfaceShaderAbstract::AddAttributeFromValueOrInterface(MaterialX::InputPtr Input, const FString& InputChannelName, UInterchangeShaderNode* ShaderNode, int32 OutputIndex)
{
	bool bAttribute = false;

	if(Input)
	{
		UInterchangeShaderNode* ShaderNodeToConnectTo = ShaderNode;
		FString InputToConnectTo = InputChannelName;

		if(Input->hasChannels())
		{
			using namespace UE::Interchange::Materials::Standard::Nodes;

			UInterchangeShaderNode* SwizzleNode = CreateShaderNode(Input, Input->getParent()->asA<mx::Node>()->getName().c_str() + FString{ TEXT("_Channels_") } + Input->getName().c_str(), Swizzle::Name.ToString());
			SwizzleNode->AddStringAttribute(Swizzle::Attributes::Channels.ToString(), Input->getChannels().c_str());

			UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(ShaderNode, InputChannelName, SwizzleNode->GetUniqueID());

			ShaderNodeToConnectTo = SwizzleNode;
			InputToConnectTo = TEXT("Input");
		}

		if(Input->hasValue())
		{
			bAttribute = AddAttribute(Input, InputToConnectTo, ShaderNodeToConnectTo, OutputIndex);
		}
		else if(Input->hasInterfaceName())
		{
			if(mx::InputPtr InputInterface = Input->getInterfaceInput(); InputInterface->hasValue())
			{
				bAttribute = AddAttribute(InputInterface, InputToConnectTo, ShaderNodeToConnectTo, OutputIndex);
			}
		}
	}

	return bAttribute;
}

bool FMaterialXSurfaceShaderAbstract::AddBooleanAttribute(MaterialX::InputPtr Input, const FString& InputChannelName, UInterchangeShaderNode* ShaderNode)
{
	using namespace UE::Interchange::Materials::Standard::Nodes;
	if(Input && Input->hasValue())
	{
		bool Value = mx::fromValueString<bool>(Input->getValueString());

		// The parent is either a node, or it's an interfacename and we just take the name of the input
		mx::NodePtr Node = Input->getParent()->asA<mx::Node>();
		FString NodeName = Node ? Node->getName().c_str() + FString{ TEXT("_") } : FString{};
		NodeName += Input->getName().c_str();

		UInterchangeShaderNode* StaticBoolParameterNode = CreateShaderNode(Input, NodeName, StaticBoolParameter::Name.ToString());
		StaticBoolParameterNode->AddBooleanAttribute(UInterchangeShaderPortsAPI::MakeInputParameterKey(StaticBoolParameter::Attributes::DefaultValue.ToString()), Value);
		return UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(ShaderNode, InputChannelName, StaticBoolParameterNode->GetUniqueID());
	}

	return false;
}

bool FMaterialXSurfaceShaderAbstract::AddFloatAttribute(MaterialX::InputPtr Input, const FString& InputChannelName, UInterchangeShaderNode* ShaderNode, float DefaultValue)
{
	using namespace UE::Interchange::Materials::Standard::Nodes;
	if(Input && Input->hasValue())
	{
		float Value = mx::fromValueString<float>(Input->getValueString());

		if(!FMath::IsNearlyEqual(Value, DefaultValue))
		{
			// The parent is either a node, or it's an interfacename and we just take the name of the input
			mx::NodePtr Node = Input->getParent()->asA<mx::Node>();
			FString NodeName = Node ? Node->getName().c_str() + FString{ TEXT("_") } : FString{};
			NodeName += Input->getName().c_str();

			UInterchangeShaderNode* ScalarParameterNode = CreateShaderNode(Input, NodeName, ScalarParameter::Name.ToString());
			ScalarParameterNode->AddFloatAttribute(UInterchangeShaderPortsAPI::MakeInputParameterKey(ScalarParameter::Attributes::DefaultValue.ToString()), Value);
			return UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(ShaderNode, InputChannelName, ScalarParameterNode->GetUniqueID());
		}
	}

	return false;
}

bool FMaterialXSurfaceShaderAbstract::AddIntegerAttribute(MaterialX::InputPtr Input, const FString& InputChannelName, UInterchangeShaderNode* ShaderNode, int DefaultValue)
{
	//We handle Integers as Scalars
	using namespace UE::Interchange::Materials::Standard::Nodes;
	if (Input && Input->hasValue())
	{
		int32 Value = mx::fromValueString<int32>(Input->getValueString());

		if (Value != DefaultValue)
		{
			// The parent is either a node, or it's an interfacename and we just take the name of the input
			mx::NodePtr Node = Input->getParent()->asA<mx::Node>();
			FString NodeName = Node ? Node->getName().c_str() + FString{ TEXT("_") } : FString{};
			NodeName += Input->getName().c_str();

			UInterchangeShaderNode* ScalarParameterNode = CreateShaderNode(Input, NodeName, ScalarParameter::Name.ToString());
			ScalarParameterNode->AddFloatAttribute(UInterchangeShaderPortsAPI::MakeInputParameterKey(ScalarParameter::Attributes::DefaultValue.ToString()), Value);
			return UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(ShaderNode, InputChannelName, ScalarParameterNode->GetUniqueID());
		}
	}

	return false;
}

bool FMaterialXSurfaceShaderAbstract::AddLinearColorAttribute(MaterialX::InputPtr Input, const FString& InputChannelName, UInterchangeShaderNode* ShaderNode, const FLinearColor& DefaultValue, int32 OutputIndex)
{
	using namespace UE::Interchange::Materials::Standard::Nodes;
	if(Input && Input->hasValue())
	{
		const FLinearColor Value = GetLinearColor(Input);

		if(!Value.Equals(DefaultValue))
		{
			// The parent is either a node, or it's an interfacename and we just take the name of the input
			mx::NodePtr Node = Input->getParent()->asA<mx::Node>();
			FString NodeName = Node ? Node->getName().c_str() + FString{ TEXT("_") } : FString{};
			NodeName += Input->getName().c_str();

			UInterchangeShaderNode* VectorParameterNode = CreateShaderNode(Input, NodeName, VectorParameter::Name.ToString());
			VectorParameterNode->AddLinearColorAttribute(UInterchangeShaderPortsAPI::MakeInputParameterKey(VectorParameter::Attributes::DefaultValue.ToString()), Value);
			return UInterchangeShaderPortsAPI::ConnectOuputToInputByIndex(ShaderNode, InputChannelName, VectorParameterNode->GetUniqueID(), OutputIndex);
		}
	}

	return false;
}

bool FMaterialXSurfaceShaderAbstract::AddVectorAttribute(MaterialX::InputPtr Input, const FString& InputChannelName, UInterchangeShaderNode* ShaderNode, const FVector4f& DefaultValue, int32 OutputIndex)
{
	using namespace UE::Interchange::Materials::Standard::Nodes;
	if(Input && Input->hasValue())
	{
		const FLinearColor Value = GetVector(Input);

		if(!Value.Equals(DefaultValue))
		{
			// The parent is either a node, or it's an interfacename and we just take the name of the input
			mx::NodePtr Node = Input->getParent()->asA<mx::Node>();
			FString NodeName = Node ? Node->getName().c_str() + FString{ TEXT("_") } : FString{};
			NodeName += Input->getName().c_str();

			UInterchangeShaderNode* VectorParameterNode = CreateShaderNode(Input, NodeName, VectorParameter::Name.ToString());
			VectorParameterNode->AddLinearColorAttribute(UInterchangeShaderPortsAPI::MakeInputParameterKey(VectorParameter::Attributes::DefaultValue.ToString()), Value);
			return UInterchangeShaderPortsAPI::ConnectOuputToInputByIndex(ShaderNode, InputChannelName, VectorParameterNode->GetUniqueID(), OutputIndex);
		}
	}

	return false;
}

bool FMaterialXSurfaceShaderAbstract::AddVector2Attribute(MaterialX::InputPtr Input, const FString& InputChannelName, UInterchangeShaderNode* ShaderNode, const FVector2f& DefaultValue, int32 OutputIndex)
{
	using namespace UE::Interchange::Materials::Standard::Nodes;
	if (Input && Input->hasValue())
	{
		const FLinearColor Value = GetVector(Input);
		
		if (!FVector2f{ Value.R,Value.B }.Equals(DefaultValue))
		{
			// The parent is either a node, or it's an interfacename and we just take the name of the input
			mx::NodePtr Node = Input->getParent()->asA<mx::Node>();
			FString NodeName = Node ? Node->getName().c_str() + FString{ TEXT("_") } : FString{};
			NodeName += Input->getName().c_str();

			UInterchangeShaderNode* VectorParameterNode = CreateShaderNode(Input, NodeName, VectorParameter::Name.ToString());
			VectorParameterNode->AddLinearColorAttribute(UInterchangeShaderPortsAPI::MakeInputParameterKey(VectorParameter::Attributes::DefaultValue.ToString()), Value);
			UInterchangeShaderNode* VectorMaskNode = CreateMaskShaderNode(0b1100, Input, NodeName + TEXT("_Vector2"));
			UInterchangeShaderPortsAPI::ConnectOuputToInputByIndex(VectorMaskNode, Mask::Inputs::Input.ToString(), VectorParameterNode->GetUniqueID(), OutputIndex);
			return UInterchangeShaderPortsAPI::ConnectOuputToInputByIndex(ShaderNode, InputChannelName, VectorMaskNode->GetUniqueID(), OutputIndex);
		}
	}

	return false;
}

bool FMaterialXSurfaceShaderAbstract::ConnectNodeGraphOutputToInput(MaterialX::InputPtr InputToNodeGraph, UInterchangeShaderNode* ShaderNode, const FString& ParentInputName)
{
	using namespace UE::Interchange::Materials::Standard::Nodes;

	bool bHasNodeGraph = false;

	if(InputToNodeGraph->hasNodeGraphString())
	{
		bHasNodeGraph = true;

		mx::OutputPtr Output = InputToNodeGraph->getConnectedOutput();

		if(!Output)
		{
			UE_LOG(LogInterchangeImport, Warning, TEXT("Couldn't find a connected output to (%s)."), *GetInputName(InputToNodeGraph));
			return false;
		}

		// We may end up with an empty output string in case of a flattened nodegraph 
		FString OutputString = Output->hasOutputString() && !Output->getOutputString().empty() ? ANSI_TO_TCHAR(Output->getOutputString().c_str()) : TEXT("out");
		for(mx::Edge Edge : Output->traverseGraph())
		{
			ConnectNodeCategoryOutputToInput(Edge, ShaderNode, ParentInputName, OutputString);
		}
	}

	return bHasNodeGraph;
}

bool FMaterialXSurfaceShaderAbstract::ConnectMatchingNodeOutputToInput(const FConnectNode& Connect)
{
	FMaterialXManager& Manager = FMaterialXManager::GetInstance();

	auto GetIndexOutput = [&Connect]()
	{
		mx::NodeDefPtr NodeDef = Connect.UpstreamNode->getNodeDef(mx::EMPTY_STRING, true);
		int Index = NodeDef->getChildIndex(TCHAR_TO_UTF8(*Connect.OutputName));
		return Index < 0 ? Index : Index % NodeDef->getInputCount();
	};

	bool bIsConnected = false;

	auto ConnectOutputToInputInternal = [&](UInterchangeShaderNode* OperatorNode)
	{
		for(mx::InputPtr Input : Connect.UpstreamNode->getInputs())
		{
			if (const FString* InputNameFound = Manager.FindMaterialExpressionInput(GetInputName(Input)))
			{
				AddAttributeFromValueOrInterface(Input, *InputNameFound, OperatorNode);
			}
			else 
			{
				AddAttributeFromValueOrInterface(Input, Input->getName().c_str(), OperatorNode);
			}
		}

		int IndexOutput = GetIndexOutput();

		bIsConnected = IndexOutput < 0 ?
			UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(Connect.ParentShaderNode, Connect.InputChannelName, OperatorNode->GetUniqueID()) :
			UInterchangeShaderPortsAPI::ConnectOuputToInputByIndex(Connect.ParentShaderNode, Connect.InputChannelName, OperatorNode->GetUniqueID(), IndexOutput);
	};

	auto ConnectOutputToInput = [&](const FString* ShaderType, auto* (FMaterialXSurfaceShaderAbstract::* CreateFunctionCallOrShaderNode)(mx::ElementPtr, const FString&, const FString&, const FString&))
	{
		UInterchangeShaderNode* OperatorNode = nullptr;

		//We don't take the node output here because it would cause the creation of a new node (output is meaningful with ComponentMaskNode/separate where we have to create a new expression
		OperatorNode = (this->*CreateFunctionCallOrShaderNode)(Connect.UpstreamNode, Connect.UpstreamNode->getName().c_str(), *ShaderType, DefaultOutput);

		ConnectOutputToInputInternal(OperatorNode);
	};

	auto ConnectFunctionShaderNodeOutputToInput = [&](uint8 EnumType, uint8 EnumValue, auto* (FMaterialXSurfaceShaderAbstract::* CreateFunctionCallOrShaderNode)(mx::ElementPtr, const FString&, uint8, uint8, const FString&))
	{
		UInterchangeShaderNode* OperatorNode = nullptr;

		//We don't take the node output here because it would cause the creation of a new node (output is meaningful with ComponentMaskNode/separate where we have to create a new expression
		OperatorNode = (this->*CreateFunctionCallOrShaderNode)(Connect.UpstreamNode, Connect.UpstreamNode->getName().c_str(), EnumType, EnumValue, DefaultOutput);

		ConnectOutputToInputInternal(OperatorNode);
	};

	const FString* MaterialFunctionPath = nullptr;

	// First search a matching Material Expression
	// search for a Material Expression based on the node group (essentially used for Substrate Mix)
	if(const FString* ShaderType = Manager.FindMatchingMaterialExpression(Connect.UpstreamNode->getCategory().c_str(), Connect.UpstreamNode->getNodeDef(mx::EMPTY_STRING, true)->getNodeGroup().c_str(), Connect.UpstreamNode->getType().c_str()))
	{
		ConnectOutputToInput(ShaderType, &FMaterialXSurfaceShaderAbstract::CreateShaderNode);
	}
	else if((ShaderType = Manager.FindMatchingMaterialExpression(Connect.UpstreamNode->getCategory().c_str())))
	{
		ConnectOutputToInput(ShaderType, &FMaterialXSurfaceShaderAbstract::CreateShaderNode);
	}
	else if(FOnConnectNodeOutputToInput* Delegate = MatchingConnectNodeDelegates.Find(Connect.UpstreamNode->getCategory().c_str()))
	{
		bIsConnected = Delegate->ExecuteIfBound(Connect);
	}
	else if(uint8 EnumType, EnumValue; Manager.FindMatchingMaterialFunction(Connect.UpstreamNode->getCategory().c_str(), MaterialFunctionPath, EnumType, EnumValue))
	{
		// In case of a surfaceshader node, especialy openpbr and standard_surface we need to check if we have to take the transmission surface shader
		if (EnumType == UE::Interchange::MaterialX::IndexSurfaceShaders)
		{
			bool bIsTransmittance = false;
			//check if the surfaceshader has a transmission input, only for openpbr and standardsurface
			if (EnumValue == uint8(EInterchangeMaterialXShaders::OpenPBRSurface) && Connect.UpstreamNode->getInput(mx::OpenPBRSurface::Input::TransmissionWeight))
			{
				EnumValue = uint8(EInterchangeMaterialXShaders::OpenPBRSurfaceTransmission);
				bIsTransmittance = true;
			}
			else if (EnumValue == uint8(EInterchangeMaterialXShaders::StandardSurface) && Connect.UpstreamNode->getInput(mx::StandardSurface::Input::Transmission))
			{
				EnumValue = uint8(EInterchangeMaterialXShaders::StandardSurfaceTransmission);
				bIsTransmittance = true;
			}

			if (bIsTransmittance)
			{
				ShaderGraphNode->SetCustomBlendMode(EBlendMode::BLEND_TranslucentColoredTransmittance);
			}
		}

		MaterialFunctionPath ?
			ConnectOutputToInput(MaterialFunctionPath, &FMaterialXSurfaceShaderAbstract::CreateFunctionCallShaderNode) :
			ConnectFunctionShaderNodeOutputToInput(EnumType, EnumValue, &FMaterialXSurfaceShaderAbstract::CreateFunctionCallShaderNode);
	}

	return bIsConnected;
}

void FMaterialXSurfaceShaderAbstract::ConnectNodeCategoryOutputToInput(const MaterialX::Edge& Edge, UInterchangeShaderNode* ShaderNode, const FString& ParentInputName, const FString& OutputName)
{
	FMaterialXManager& Manager = FMaterialXManager::GetInstance();
	if(mx::NodePtr UpstreamNode = Edge.getUpstreamElement()->asA<mx::Node>())
	{
		// We need to connect the different descending nodes to all the outputs of a node
		// At least one output connected to the root shader node
		TArray<UInterchangeShaderNode*> ParentShaderNodeOutputs{ShaderNode};
		FString InputChannelName = ParentInputName;
				
		Manager.AddInputsFromNodeDef(UpstreamNode);
		Manager.RemoveInputs(UpstreamNode);

		// Replace the input's name by the one used in UE
		SetMatchingInputsNames(UpstreamNode);

		FString OutputChannelName = OutputName;

		// Swizzle node for the attribute 'channels'
		UInterchangeShaderNode* ChannelsNode = nullptr;

		if(mx::ElementPtr DownstreamElement = Edge.getDownstreamElement())
		{
			if(mx::NodePtr DownstreamNode = DownstreamElement->asA<mx::Node>())
			{
				mx::InputPtr ConnectedInput = Edge.getConnectingElement()->asA<mx::Input>();
				if(ConnectedInput)
				{
					InputChannelName = GetInputName(ConnectedInput);
					if(ConnectedInput->hasOutputString())
					{
						OutputChannelName = ConnectedInput->getOutputString().c_str();
					}

					if(ConnectedInput->hasChannels())
					{
						using namespace UE::Interchange::Materials::Standard::Nodes;
						ChannelsNode = CreateShaderNode(ConnectedInput, (UpstreamNode->getName() + ConnectedInput->getName()).c_str() + FString{ TEXT("_Channels") }, Swizzle::Name.ToString());
						ChannelsNode->AddStringAttribute(Swizzle::Attributes::Channels.ToString(), ConnectedInput->getChannels().c_str());
					}
				}

				std::vector<mx::OutputPtr> Outputs{ DownstreamNode->getActiveOutputs()};
				if(Outputs.empty())
				{
					Outputs = DownstreamNode->getNodeDef(mx::EMPTY_STRING, true)->getActiveOutputs();
				}

				ParentShaderNodeOutputs.Empty();
				for(std::size_t i = 0; i < Outputs.size(); ++i)
				{
					if(UInterchangeShaderNode** FoundNode = ShaderNodes.Find({ GetAttributeParentName(DownstreamNode, ConnectedInput), Outputs[i]->getName().c_str()}))
					{
						//Connect the swizzle node between the upstream and downstream node
						if(ChannelsNode)
						{
							UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(*FoundNode, InputChannelName, ChannelsNode->GetUniqueID());
							InputChannelName = TEXT("Input");
							FoundNode = &ChannelsNode;
						}

						ParentShaderNodeOutputs.Emplace(*FoundNode);
					}
				}
			}
		}

		for(UInterchangeShaderNode* ParentShaderNode: ParentShaderNodeOutputs)
		{
			if(!ConnectMatchingNodeOutputToInput({ UpstreamNode, ParentShaderNode, InputChannelName, OutputChannelName }))
			{
				UE_LOG(LogInterchangeImport, Warning, TEXT("<%s>: \"%s\" is not supported. [%s]"), UTF8_TO_TCHAR(UpstreamNode->getCategory().c_str()), UTF8_TO_TCHAR(UpstreamNode->getName().c_str()), *SurfaceMaterialName);
			}
		}
	}
}

bool FMaterialXSurfaceShaderAbstract::ConnectNodeNameOutputToInput(MaterialX::InputPtr InputToConnectedNode, UInterchangeShaderNode* ShaderNode, const FString& ParentInputName)
{
	mx::NodePtr ConnectedNode = InputToConnectedNode->getConnectedNode();

	if(!ConnectedNode)
	{
		return false;
	}

	UInterchangeShaderNode* ParentShaderNode = ShaderNode;
	FString InputChannelName = ParentInputName;

	mx::Edge Edge(nullptr, InputToConnectedNode, ConnectedNode);

	TArray<mx::Edge> Stack{ Edge };

	while(!Stack.IsEmpty())
	{
		Edge = Stack.Pop();

		if(Edge.getUpstreamElement())
		{
			ConnectNodeCategoryOutputToInput(Edge, ShaderNode, ParentInputName);
			ConnectedNode = Edge.getUpstreamElement()->asA<mx::Node>();
			for(mx::InputPtr Input : ConnectedNode->getInputs())
			{
				Stack.Emplace(ConnectedNode, Input, Input->getConnectedNode());
			}
		}
	}

	return true;
}

void FMaterialXSurfaceShaderAbstract::ConnectConstantInputToOutput(const FConnectNode& Connect)
{
	AddAttributeFromValueOrInterface(Connect.UpstreamNode->getInput("value"), Connect.InputChannelName, Connect.ParentShaderNode);
}

void FMaterialXSurfaceShaderAbstract::ConnectExtractInputToOutput(const FConnectNode& Connect)
{
	using namespace UE::Interchange::Materials::Standard::Nodes;

	uint8 Index = 0;
	if(mx::InputPtr InputIndex = Connect.UpstreamNode->getInput("index"))
	{
		Index = mx::fromValueString<int>(InputIndex->getValueString());
	}

	if(mx::InputPtr Input = Connect.UpstreamNode->getInput("in"); Input && Input->hasValue())
	{
		// Output 0 means RGB, 1st channel starts at 1
		AddAttributeFromValueOrInterface(Input, Connect.InputChannelName, Connect.ParentShaderNode, Index + 1);
	}
	else
	{
		UInterchangeShaderNode* MaskShaderNode = CreateMaskShaderNode(1 << (3 - Index), Connect.UpstreamNode, Connect.UpstreamNode->getName().c_str());
		UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(Connect.ParentShaderNode, Connect.InputChannelName, MaskShaderNode->GetUniqueID());
	}
}

void FMaterialXSurfaceShaderAbstract::ConnectDotInputToOutput(const FConnectNode& Connect)
{
	if(mx::InputPtr Input = Connect.UpstreamNode->getInput("in"))
	{
		SetAttributeNewName(Input, TCHAR_TO_UTF8(*Connect.InputChannelName)); //let's take the parent node's input name
		ShaderNodes.Add({ Connect.UpstreamNode->getName().c_str(), Connect.OutputName}, Connect.ParentShaderNode);
	}
}

void FMaterialXSurfaceShaderAbstract::ConnectTransformPositionInputToOutput(const FConnectNode& Connect)
{
	UInterchangeShaderNode* TransformNode = CreateShaderNode(Connect.UpstreamNode, Connect.UpstreamNode->getName().c_str(), UE::Expressions::Names::TransformPosition);
	AddAttributeFromValueOrInterface(Connect.UpstreamNode->getInput("in"), UE::Expressions::Inputs::Input, TransformNode);
	UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(Connect.ParentShaderNode, Connect.InputChannelName, TransformNode->GetUniqueID());
}

void FMaterialXSurfaceShaderAbstract::ConnectTransformVectorInputToOutput(const FConnectNode& Connect)
{
	UInterchangeShaderNode* TransformNode = CreateShaderNode(Connect.UpstreamNode, Connect.UpstreamNode->getName().c_str(), UE::Expressions::Names::Transform);
	AddAttributeFromValueOrInterface(Connect.UpstreamNode->getInput("in"), UE::Expressions::Inputs::Input, TransformNode);
	UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(Connect.ParentShaderNode, Connect.InputChannelName, TransformNode->GetUniqueID());
}

void FMaterialXSurfaceShaderAbstract::ConnectRotate2DInputToOutput(const FConnectNode& Connect)
{
	using namespace UE::Interchange::Materials::Standard::Nodes;

	UInterchangeShaderNode* Rotate2DNode = CreateShaderNode(Connect.UpstreamNode, Connect.UpstreamNode->getName().c_str(), Rotator::Name.ToString());


	if(mx::InputPtr Input = Connect.UpstreamNode->getInput("in"))
	{
		AddAttributeFromValueOrInterface(Input, GetInputName(Input), Rotate2DNode);
	}

	// Amount is in degrees whereas Time (which in our case is the angle) input is in radians
	if(mx::InputPtr Input = Connect.UpstreamNode->getInput("amount"))
	{
		FString InputName = GetInputName(Input);
		UInterchangeShaderNode* DegreesToRadiansNode = CreateShaderNode(Connect.UpstreamNode, (Connect.UpstreamNode->getName() + "multiply").c_str(), Multiply::Name.ToString());
		constexpr float DegreesToRadians = UE_PI / 180.f;
		DegreesToRadiansNode->AddFloatAttribute(UInterchangeShaderPortsAPI::MakeInputValueKey(TEXT("B")), DegreesToRadians);

		//If it's a value we should always attach it on the A input of Divide node
		AddAttributeFromValueOrInterface(Input, UE::Expressions::Inputs::A, DegreesToRadiansNode);
		UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(Rotate2DNode, InputName, DegreesToRadiansNode->GetUniqueID());
		SetAttributeNewName(Input, TCHAR_TO_ANSI(UE::Expressions::Inputs::A));
	}

	Rotate2DNode->AddFloatAttribute(Rotator::Attributes::CenterX.ToString(), 0.f);
	Rotate2DNode->AddFloatAttribute(Rotator::Attributes::CenterY.ToString(), 0.f);
	Rotate2DNode->AddFloatAttribute(Rotator::Attributes::Speed.ToString(), 1.f);

	UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(Connect.ParentShaderNode, Connect.InputChannelName, Rotate2DNode->GetUniqueID());
}

void FMaterialXSurfaceShaderAbstract::ConnectRotate3DInputToOutput(const FConnectNode& Connect)
{
	using namespace UE::Interchange::Materials::Standard::Nodes;

	UInterchangeShaderNode* Rotate3DNode = CreateShaderNode(Connect.UpstreamNode, Connect.UpstreamNode->getName().c_str(), RotateAboutAxis::Name.ToString());
	Rotate3DNode->AddLinearColorAttribute(UInterchangeShaderPortsAPI::MakeInputValueKey(RotateAboutAxis::Inputs::PivotPoint.ToString()), FLinearColor(0., 0., 0));

	mx::InputPtr Input = Connect.UpstreamNode->getInput("in");
	AddAttributeFromValueOrInterface(Input, UE::Expressions::Inputs::Position, Rotate3DNode);
	AddAttributeFromValueOrInterface(Connect.UpstreamNode->getInput("axis"), UE::Expressions::Inputs::NormalizedRotationAxis, Rotate3DNode);
	AddAttributeFromValueOrInterface(Connect.UpstreamNode->getInput("amount"), UE::Expressions::Inputs::RotationAngle, Rotate3DNode);

	// Convert Degrees into radians by setting the period to 360
	Rotate3DNode->AddFloatAttribute(RotateAboutAxis::Attributes::Period.ToString(), 360.f);

	//RotateAboutAxis returns an offset of the rotated vector, let's add to the vector to rotate it directly
	FString AddNodeName{ (Connect.UpstreamNode->getName() + "_Add").c_str() };
	UInterchangeShaderNode* AddNode = CreateShaderNode(Connect.UpstreamNode, AddNodeName, Add::Name.ToString());
	UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(AddNode, Add::Inputs::A.ToString(), Rotate3DNode->GetUniqueID());

	//Add another input to connect the Position to the Add node
	if (!Connect.UpstreamNode->getInput("position_add"))
	{
		mx::InputPtr InputPositionAdd = Connect.UpstreamNode->addInput("position_add");
		InputPositionAdd->copyContentFrom(Input);
		InputPositionAdd->setAttribute(mx::Attributes::ParentName, TCHAR_TO_ANSI(*AddNodeName));
		SetAttributeNewName(InputPositionAdd, TCHAR_TO_ANSI(UE::Expressions::Inputs::B));
	}

	UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(Connect.ParentShaderNode, Connect.InputChannelName, AddNode->GetUniqueID());
}

void FMaterialXSurfaceShaderAbstract::ConnectImageInputToOutput(const FConnectNode& Connect)
{
	using namespace UE::Interchange::Materials::Standard::Nodes;

	if(UInterchangeTextureNode* TextureNode = CreateTextureNode<UInterchangeTexture2DNode>(Connect.UpstreamNode))
	{
		//By default set the output of a texture to RGB
		FString OutputChannel{ TEXT("RGB") };

		if(Connect.UpstreamNode->getType() == mx::Type::Vector4 || Connect.UpstreamNode->getType() == mx::Type::Color4)
		{
			OutputChannel = TEXT("RGBA");
		}
		else if(Connect.UpstreamNode->getType() == mx::Type::Float)
		{
			OutputChannel = TEXT("R");
		}

		UInterchangeShaderNode* TextureShaderNode = CreateShaderNode(Connect.UpstreamNode, Connect.UpstreamNode->getName().c_str(), TextureSample::Name.ToString());
		TextureShaderNode->AddStringAttribute(UInterchangeShaderPortsAPI::MakeInputValueKey(TextureSample::Inputs::Texture.ToString()), TextureNode->GetUniqueID());

		if (Connect.UpstreamNode->getTypedAttribute<bool>(mx::Attributes::GeomPropImage))
		{
			// Add a custom attribute that this image node is in fact a geompropvalue converted to an image, in order to update the TextureSample material expression with the baked texture
			TextureShaderNode->AddBooleanAttribute(mx::Attributes::GeomPropImage, true);
		}
		UInterchangeShaderPortsAPI::ConnectOuputToInputByName(Connect.ParentShaderNode, Connect.InputChannelName, TextureShaderNode->GetUniqueID(), OutputChannel);
	}
	else
	{
		AddAttributeFromValueOrInterface(Connect.UpstreamNode->getInput(mx::NodeGroup::Texture2D::Inputs::Default), Connect.InputChannelName, Connect.ParentShaderNode);
	}
}

void FMaterialXSurfaceShaderAbstract::ConnectConvertInputToOutput(const FConnectNode& Connect)
{
	using namespace UE::Interchange::Materials::Standard::Nodes;

	//In case of an upwards conversion, let's do an append, downwards a mask, otherwise leave as is
	mx::InputPtr Input = Connect.UpstreamNode->getInput("in");
	const std::string& NodeType = Connect.UpstreamNode->getType();
	const std::string& InputType = Input->getType();

	//Ensure that the types are supported
	const bool bIsNodeTypeSupported =
		NodeType == mx::Type::Color4 ||
		NodeType == mx::Type::Color3 ||
		NodeType == mx::Type::Vector4 ||
		NodeType == mx::Type::Vector3 ||
		NodeType == mx::Type::Vector2 ||
		NodeType == mx::Type::Float ||
		NodeType == mx::Type::Integer ||
		NodeType == mx::Type::Boolean;

	const bool bIsInputTypeSupported =
		InputType == mx::Type::Color4 ||
		InputType == mx::Type::Color3 ||
		InputType == mx::Type::Vector4 ||
		InputType == mx::Type::Vector3 ||
		InputType == mx::Type::Vector2 ||
		InputType == mx::Type::Float ||
		InputType == mx::Type::Integer ||
		InputType == mx::Type::Boolean;

	if(!bIsNodeTypeSupported || !bIsInputTypeSupported)
	{
		UE_LOG(LogInterchangeImport, Warning, TEXT("<convert> node has unsupported types."));
		return;
	}

	char NodeN = NodeType.back();
	char InputN = InputType.back();

	constexpr auto Remap = [](auto Value, auto Low1, auto High1, auto Low2, auto High2)
	{
		return Low2 + (Value - Low1) * (High2 - Low2) / (High1 - Low1);
	};

	//Remap NodeN and InputN in a range lower than '2' in case of integer/boolean/float
	if(NodeN > '4')
	{
		NodeN = Remap(NodeN, 'a', 'z', 0, '1');
	}
	if(InputN > '4')
	{
		InputN = Remap(InputN, 'a', 'z', 0, '1');
	}

	if(InputN > NodeN) // Mask
	{
		UInterchangeShaderNode* MaskShaderNode = nullptr;
		if(NodeN == '3')
		{
			MaskShaderNode = CreateMaskShaderNode(0b1110, Connect.UpstreamNode, Connect.UpstreamNode->getName().c_str());
		}
		else if(NodeN == '2')
		{
			MaskShaderNode = CreateMaskShaderNode(0b1100, Connect.UpstreamNode, Connect.UpstreamNode->getName().c_str());
		}
		else
		{
			MaskShaderNode = CreateMaskShaderNode(0b1000, Connect.UpstreamNode, Connect.UpstreamNode->getName().c_str());
		}

		AddAttributeFromValueOrInterface(Input, UE::Expressions::Inputs::Input, MaskShaderNode);

		UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(Connect.ParentShaderNode, Connect.InputChannelName, MaskShaderNode->GetUniqueID());
	}
	else // Append
	{
		//same as dot, just connect the next output to this parent input
		SetAttributeNewName(Input, TCHAR_TO_UTF8(*Connect.InputChannelName)); //let's take the parent node's input name
		ShaderNodes.Add({ Connect.UpstreamNode->getName().c_str(), Connect.OutputName }, Connect.ParentShaderNode);

		//No need to create a node, since the input and the node have the same channels, we just check if there's a value
		if(NodeN == InputN || (NodeN < '2' && InputN < '2'))
		{
			AddAttributeFromValueOrInterface(Input, Connect.InputChannelName, Connect.ParentShaderNode);
			return;
		}
		std::string Category;
		TArray<FInputToCopy> InputsToCopy;
		TArray<FInputToCreate> InputsToCreate;

		// float to N
		if(InputN < '2')
		{
			if(NodeN == '2')
			{
				Category = mx::Category::Combine2;
				InputsToCopy.Add({ "in1", Input });
				InputsToCopy.Add({ "in2", Input });
			}
			else if(NodeN == '3')
			{
				Category = mx::Category::Combine3;
				InputsToCopy.Add({ "in1", Input });
				InputsToCopy.Add({ "in2", Input });
				InputsToCopy.Add({ "in3", Input });
			}
			else if(NodeN == '4')
			{
				Category = mx::Category::Combine4;
				InputsToCopy.Add({ "in1", Input });
				InputsToCopy.Add({ "in2", Input });
				InputsToCopy.Add({ "in3", Input });
				InputsToCopy.Add({ "in4", Input });
			}
		}
		else if((InputN == '2' && NodeN == '3') || (InputN == '3' && NodeN == '4'))
		{
			Category = mx::Category::Combine2;
			InputsToCopy.Add({ "in1", Input });
			InputsToCreate.Add({ "in2", FAttributeValueArray{{"type","float"}, {"value","1"}} });
		}

		//copy "in" input into "in1" and create "amount" input as "in2" with the value 360 because UE's angle is a value between [0,1]
		mx::NodePtr CombineNode = CreateNode(Connect.UpstreamNode->getParent(),
											 Connect.UpstreamNode->getName().c_str(),
											 Category.c_str(),
											 InputsToCopy, // rename input to match <divide> input "in1"
											 InputsToCreate);

		//We set the type and an output to find it during connect node resolution phase
		//Because the case Combine2 vector2->vector3 has no nodedef in the standard library
		CombineNode->setType(Connect.UpstreamNode->getType());
		if(!CombineNode->getOutput("out"))
		{
			CombineNode->addOutput("out", Connect.UpstreamNode->getType());
		}

		// Input now points to the new node
		Input->setNodeName(CombineNode->getName());
	}
}

void FMaterialXSurfaceShaderAbstract::ConnectIfGreaterInputToOutput(const FConnectNode& Connect)
{
	UInterchangeShaderNode* NodeIf = CreateShaderNode(Connect.UpstreamNode, Connect.UpstreamNode->getName().c_str(), UE::Expressions::Names::If);
	UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(Connect.ParentShaderNode, Connect.InputChannelName, NodeIf->GetUniqueID());

	if(mx::InputPtr Input = Connect.UpstreamNode->getInput("value1"))
	{
		AddAttributeFromValueOrInterface(Input, GetInputName(Input), NodeIf);
	}

	if(mx::InputPtr Input = Connect.UpstreamNode->getInput("value2"))
	{
		AddAttributeFromValueOrInterface(Input, GetInputName(Input), NodeIf);
	}

	if(mx::InputPtr Input = Connect.UpstreamNode->getInput("in1"))
	{
		AddAttributeFromValueOrInterface(Input, GetInputName(Input), NodeIf);
	}

	//In that case we also need to add an attribute to AEqualsB
	mx::InputPtr Input = Connect.UpstreamNode->getInput("in2");
	if(Input)
	{
		AddAttributeFromValueOrInterface(Input, GetInputName(Input), NodeIf);
		AddAttributeFromValueOrInterface(Input, UE::Expressions::Inputs::AEqualsB, NodeIf);

		//Let's add a new input that is a copy of in2 to connect it to the equal input
		if (!Connect.UpstreamNode->getInput("in3"))
		{
			mx::InputPtr Input3 = Connect.UpstreamNode->addInput("in3");
			Input3->copyContentFrom(Input);
			SetAttributeNewName(Input3, TCHAR_TO_ANSI(UE::Expressions::Inputs::AEqualsB));
		}
	}
}

void FMaterialXSurfaceShaderAbstract::ConnectIfGreaterEqInputToOutput(const FConnectNode& Connect)
{
	UInterchangeShaderNode* NodeIf = CreateShaderNode(Connect.UpstreamNode, Connect.UpstreamNode->getName().c_str(), UE::Expressions::Names::If);
	UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(Connect.ParentShaderNode, Connect.InputChannelName, NodeIf->GetUniqueID());

	if(mx::InputPtr Input = Connect.UpstreamNode->getInput("value1"))
	{
		AddAttributeFromValueOrInterface(Input, GetInputName(Input), NodeIf);
	}

	if(mx::InputPtr Input = Connect.UpstreamNode->getInput("value2"))
	{
		AddAttributeFromValueOrInterface(Input, GetInputName(Input), NodeIf);
	}

	if(mx::InputPtr Input = Connect.UpstreamNode->getInput("in2"))
	{
		AddAttributeFromValueOrInterface(Input, GetInputName(Input), NodeIf);
	}

	//In that case we also need to add an attribute to AEqualsB
	mx::InputPtr Input = Connect.UpstreamNode->getInput("in1");
	if(Input)
	{
		AddAttributeFromValueOrInterface(Input, GetInputName(Input), NodeIf);
		AddAttributeFromValueOrInterface(Input, UE::Expressions::Inputs::AEqualsB, NodeIf);

		//Let's add a new input that is a copy of in2 to connect it to the equal input
		if (!Connect.UpstreamNode->getInput("in3"))
		{
			mx::InputPtr Input3 = Connect.UpstreamNode->addInput("in3");
			Input3->copyContentFrom(Input);
			SetAttributeNewName(Input3, TCHAR_TO_ANSI(UE::Expressions::Inputs::AEqualsB));
		}
	}
}

void FMaterialXSurfaceShaderAbstract::ConnectIfEqualInputToOutput(const FConnectNode& Connect)
{
	UInterchangeShaderNode* NodeIf = CreateShaderNode(Connect.UpstreamNode, Connect.UpstreamNode->getName().c_str(), UE::Expressions::Names::If);
	UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(Connect.ParentShaderNode, Connect.InputChannelName, NodeIf->GetUniqueID());

	if(mx::InputPtr Input = Connect.UpstreamNode->getInput("value1"))
	{
		AddAttributeFromValueOrInterface(Input, GetInputName(Input), NodeIf);
	}

	if(mx::InputPtr Input = Connect.UpstreamNode->getInput("value2"))
	{
		AddAttributeFromValueOrInterface(Input, GetInputName(Input), NodeIf);
	}

	if(mx::InputPtr Input = Connect.UpstreamNode->getInput("in1"))
	{
		AddAttributeFromValueOrInterface(Input, GetInputName(Input), NodeIf);
	}

	//In that case we also need to add an attribute to AGreaterThanB
	mx::InputPtr Input = Connect.UpstreamNode->getInput("in2");
	if(Input)
	{
		AddAttributeFromValueOrInterface(Input, GetInputName(Input), NodeIf);
		AddAttributeFromValueOrInterface(Input, UE::Expressions::Inputs::AGreaterThanB, NodeIf);

		//Let's add a new input that is a copy of in2 to connect it to the equal input
		if(!Connect.UpstreamNode->getInput("in3"))
		{
			mx::InputPtr Input3 = Connect.UpstreamNode->addInput("in3");
			Input3->copyContentFrom(Input);
			SetAttributeNewName(Input3, TCHAR_TO_ANSI(UE::Expressions::Inputs::AGreaterThanB));
		}
	}
}

void FMaterialXSurfaceShaderAbstract::ConnectOutsideInputToOutput(const FConnectNode& Connect)
{
	using namespace UE::Interchange::Materials::Standard::Nodes;
	//in * (1 - mask)
	UInterchangeShaderNode* NodeMultiply = CreateShaderNode(Connect.UpstreamNode, Connect.UpstreamNode->getName().c_str(), Multiply::Name.ToString());
	AddAttributeFromValueOrInterface(Connect.UpstreamNode->getInput("in"), Multiply::Inputs::A.ToString(), NodeMultiply);
	UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(Connect.ParentShaderNode, Connect.InputChannelName, NodeMultiply->GetUniqueID());

	UInterchangeShaderNode* NodeOneMinus = CreateShaderNode(Connect.UpstreamNode, Connect.UpstreamNode->getName().c_str() + FString(TEXT("_OneMinus")), OneMinus::Name.ToString());
	AddAttributeFromValueOrInterface(Connect.UpstreamNode->getInput("mask"), OneMinus::Inputs::Input.ToString(), NodeOneMinus);
	UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(NodeMultiply, Multiply::Inputs::B.ToString(), NodeOneMinus->GetUniqueID());
}

UInterchangeShaderNode* FMaterialXSurfaceShaderAbstract::ConnectGeometryInputToOutput(const FConnectNode& Connect, const FString& ShaderType, const FString& TransformShaderType, const FString& TransformInput, const FString& TransformSourceType, int32 TransformSource, const FString& TransformType, int32 TransformSDestination, bool bIsVector)
{
	// MaterialX defines the space as: object, model, world
	// model: The local coordinate space of the geometry, before any local deformations or global transforms have been applied.
	// object: The local coordinate space of the geometry, after local deformations have been applied, but before any global transforms.
	// world : The global coordinate space of the geometry, after local deformationsand global transforms have been applied.

	// In case of model/object we need to add a TransformVector from world to local space
	UInterchangeShaderNode* GeometryNode = CreateShaderNode(Connect.UpstreamNode, Connect.UpstreamNode->getName().c_str(), ShaderType);

	UInterchangeShaderNode* NodeToConnectTo = Connect.ParentShaderNode;
	FString InputToConnectTo = Connect.InputChannelName;

	mx::InputPtr InputSpace = Connect.UpstreamNode->getInput("space");

	//the default space defined by the nodedef is "object"
	bool bIsObjectSpace = (InputSpace && InputSpace->getValueString() != "world") || !InputSpace;

	// We transform to Tangent Space only for Vector nodes
	if(bTangentSpaceInput && bIsVector)
	{
		using namespace UE::Interchange::Materials::Standard::Nodes;
		UInterchangeShaderNode* TransformTSNode = CreateShaderNode(Connect.UpstreamNode, Connect.UpstreamNode->getName().c_str() + FString(TEXT("_TransformTS")), TransformShaderType);
		EMaterialVectorCoordTransformSource SpaceSource = bIsObjectSpace ? EMaterialVectorCoordTransformSource::TRANSFORMSOURCE_Local : EMaterialVectorCoordTransformSource::TRANSFORMSOURCE_World;
		TransformTSNode->AddInt32Attribute(TransformSourceType, SpaceSource);
		TransformTSNode->AddInt32Attribute(TransformType, EMaterialVectorCoordTransform::TRANSFORM_Tangent);
		UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(NodeToConnectTo, InputToConnectTo, TransformTSNode->GetUniqueID());
		NodeToConnectTo = TransformTSNode;
		InputToConnectTo = TransformInput; //Same a TransformVector
	}

	if(bIsObjectSpace)
	{
		UInterchangeShaderNode* TransformNode = CreateShaderNode(Connect.UpstreamNode, Connect.UpstreamNode->getName().c_str() + FString(TEXT("_Transform")), TransformShaderType);
		TransformNode->AddInt32Attribute(TransformSourceType, TransformSource);
		TransformNode->AddInt32Attribute(TransformType, TransformSDestination);
		UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(NodeToConnectTo, InputToConnectTo, TransformNode->GetUniqueID());
		NodeToConnectTo = TransformNode;
		InputToConnectTo = TransformInput;
	}

	UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(NodeToConnectTo, InputToConnectTo, GeometryNode->GetUniqueID());

	return GeometryNode;
}

UInterchangeShaderNode* FMaterialXSurfaceShaderAbstract::ConnectNoise2DInputToOutput(const FConnectNode& Connect, const FString& ShaderType, EVectorNoiseFunction NoiseFunction, uint8 Mask)
{
	using namespace UE::Interchange::Materials::Standard::Nodes;

	// Let's use a vector noise for this one, the only one that is close to MaterialX implementation
	UInterchangeShaderNode* NoiseNode = CreateShaderNode(Connect.UpstreamNode, (Connect.UpstreamNode->getName() + '_').c_str() + ShaderType, VectorNoise::Name.ToString());
	NoiseNode->AddInt32Attribute(VectorNoise::Attributes::Function.ToString(), NoiseFunction);
	//Quality only has a meaning with Voronoi (Worley), to be as close to MaterialX spec we want a 3x3x3 neighborhood
	NoiseNode->AddInt32Attribute(VectorNoise::Attributes::Quality.ToString(), 3);

	// These noise algorithms are meant to be used with 3D inputs, in order to use it with 2D input, let's put an arbitrary seed for 'z'
	// not sure 0 is a good candidate, we'll see when we'll have real use cases.
	UInterchangeShaderNode* AppendNode = CreateShaderNode(Connect.UpstreamNode, Connect.UpstreamNode->getName().c_str(), UE::Expressions::Names::AppendVector);
	AppendNode->AddFloatAttribute(UInterchangeShaderPortsAPI::MakeInputValueKey(UE::Expressions::Inputs::B), 0.f);

	// The connecting node has to be plugged in the Append expression not the noise, since we are constructing a vector2 to a vector3
	if (mx::InputPtr Input = Connect.UpstreamNode->getInput("texcoord"))
	{
		AddAttributeFromValueOrInterface(Input, UE::Expressions::Inputs::A, AppendNode);
		SetAttributeNewName(Input, TCHAR_TO_ANSI(UE::Expressions::Inputs::A));
	}

	UInterchangeShaderNode* MaskNode = CreateMaskShaderNode(Mask, Connect.UpstreamNode, (Connect.UpstreamNode->getName() + "_ComponentMask").c_str());

	UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(NoiseNode, VectorNoise::Inputs::Position.ToString(), AppendNode->GetUniqueID());
	UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(MaskNode, Mask::Inputs::Input.ToString(), NoiseNode->GetUniqueID());

	return MaskNode;
}

void FMaterialXSurfaceShaderAbstract::ConnectPositionInputToOutput(const FConnectNode& Connect)
{
	using namespace UE::Interchange::Materials::Standard::Nodes;
	UInterchangeShaderNode* PositionNode = CreateShaderNode(Connect.UpstreamNode, (Connect.UpstreamNode->getName() + "_Position").c_str(), UE::Expressions::Names::LocalPosition);
	
	if (mx::InputPtr InputSpace = Connect.UpstreamNode->getInput("space"); InputSpace && InputSpace->getValueString() == "world")
	{
		PositionNode->SetCustomShaderType(UE::Expressions::Names::WorldPosition);
	}

	UInterchangeShaderNode* UnitNode = CreateShaderNode(Connect.UpstreamNode, Connect.UpstreamNode->getName().c_str(), Multiply::Name.ToString());

	UnitNode->AddFloatAttribute(UInterchangeShaderPortsAPI::MakeInputValueKey(TEXT("B")), 0.01f);
	UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(UnitNode, TEXT("A"), PositionNode->GetUniqueID());

	UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(Connect.ParentShaderNode, Connect.InputChannelName, UnitNode->GetUniqueID());
}

void FMaterialXSurfaceShaderAbstract::ConnectNormalInputToOutput(const FConnectNode& Connect)
{
	using namespace UE::Interchange::Materials::Standard::Nodes;
	ConnectGeometryInputToOutput(Connect, TEXT("VertexNormalWS"),
								 TransformVector::Name.ToString(),
								 TransformVector::Inputs::Input.ToString(),
								 TransformVector::Attributes::TransformSourceType.ToString(), EMaterialVectorCoordTransformSource::TRANSFORMSOURCE_World,
								 TransformVector::Attributes::TransformType.ToString(), EMaterialVectorCoordTransform::TRANSFORM_Local);
}

void FMaterialXSurfaceShaderAbstract::ConnectTangentInputToOutput(const FConnectNode& Connect)
{
	using namespace UE::Interchange::Materials::Standard::Nodes;
	ConnectGeometryInputToOutput(Connect, TEXT("VertexTangentWS"),
								 TransformVector::Name.ToString(),
								 TransformVector::Inputs::Input.ToString(),
								 TransformVector::Attributes::TransformSourceType.ToString(), EMaterialVectorCoordTransformSource::TRANSFORMSOURCE_World,
								 TransformVector::Attributes::TransformType.ToString(), EMaterialVectorCoordTransform::TRANSFORM_Local);
}

void FMaterialXSurfaceShaderAbstract::ConnectBitangentInputToOutput(const FConnectNode& Connect)
{
	using namespace UE::Interchange::Materials::Standard::Nodes;

	UInterchangeShaderNode* BitangentNode = ConnectGeometryInputToOutput(Connect, TEXT("CrossProduct"),
																		 TransformVector::Name.ToString(),
																		 TransformVector::Inputs::Input.ToString(),
																		 TransformVector::Attributes::TransformSourceType.ToString(), EMaterialVectorCoordTransformSource::TRANSFORMSOURCE_World,
																		 TransformVector::Attributes::TransformType.ToString(), EMaterialVectorCoordTransform::TRANSFORM_Local);

	UInterchangeShaderNode* NormalNode = CreateShaderNode(Connect.UpstreamNode, Connect.UpstreamNode->getName().c_str() + FString(TEXT("_Normal")), UE::Expressions::Names::VertexNormalWS);
	UInterchangeShaderNode* TangentNode = CreateShaderNode(Connect.UpstreamNode, Connect.UpstreamNode->getName().c_str() + FString(TEXT("_Tangent")), UE::Expressions::Names::VertexTangentWS);

	UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(BitangentNode, TEXT("A"), NormalNode->GetUniqueID());
	UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(BitangentNode, TEXT("B"), TangentNode->GetUniqueID());
}

void FMaterialXSurfaceShaderAbstract::ConnectTimeInputToOutput(const FConnectNode& Connect)
{
	using namespace UE::Interchange::Materials::Standard::Nodes;
	UInterchangeShaderNode* TimeNode = CreateShaderNode(Connect.UpstreamNode, Connect.UpstreamNode->getName().c_str(), UE::Expressions::Names::Time);
	TimeNode->AddBooleanAttribute(Time::Attributes::OverridePeriod.ToString(), true);

	float FPS;
	mx::InputPtr Input = Connect.UpstreamNode->getInput("fps");

	//Take the default value from the node definition
	if(!Input)
	{
		Input = Connect.UpstreamNode->getNodeDef(mx::EMPTY_STRING, true)->getInput("fps");
	}

	FPS = mx::fromValueString<float>(Input->getValueString());

	//UE is a period
	TimeNode->AddFloatAttribute(Time::Attributes::Period.ToString(), 1.f / FPS);

	UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(Connect.ParentShaderNode, Connect.InputChannelName, TimeNode->GetUniqueID());
}

void FMaterialXSurfaceShaderAbstract::ConnectNoise2DInputToOutput(const FConnectNode& Connect)
{
	using namespace UE::Interchange::Materials::Standard::Nodes;
	
	const std::string& Type = Connect.UpstreamNode->getType();
	uint8 Mask = 
		Type == "float" ? 0b1000 :
		Type == "vector2" ? 0b1100 :
		0b1110;

	UInterchangeShaderNode* PerlinNoiseNode = ConnectNoise2DInputToOutput(Connect, VectorNoise::Name.ToString(), EVectorNoiseFunction::VNF_VectorALU, Mask);

	UInterchangeShaderNode* NodeToConnect = PerlinNoiseNode;

	// Amplitude * Noise + Pivot
	auto ConnectNodeToInput = [&](mx::InputPtr Input, UInterchangeShaderNode* NodeToConnectTo, const FString& ShaderType) -> UInterchangeShaderNode*
		{
			if (!Input)
			{
				return nullptr;
			}

			const FString ShaderNodeName = Connect.UpstreamNode->getName().c_str() + FString{ TEXT("_") } + ShaderType;
			UInterchangeShaderNode* ShaderNode = CreateShaderNode(Connect.UpstreamNode, ShaderNodeName, ShaderType);

			UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(ShaderNode, UE::Expressions::Inputs::A, NodeToConnectTo->GetUniqueID());

			// Connect the amplitude/pivot node to the shader node not the noise
			// it will be handle during the upstream-downstream connection phase
			Input->setAttribute(mx::Attributes::ParentName, TCHAR_TO_UTF8(*ShaderNodeName));
			AddAttributeFromValueOrInterface(Input, UE::Expressions::Inputs::B, ShaderNode);

			return ShaderNode;
		};

	if (UInterchangeShaderNode* MultiplyNode = ConnectNodeToInput(Connect.UpstreamNode->getInput("amplitude"), PerlinNoiseNode, Multiply::Name.ToString()))
	{
		NodeToConnect = MultiplyNode;
	}

	if (UInterchangeShaderNode* AddNode = ConnectNodeToInput(Connect.UpstreamNode->getInput("pivot"), NodeToConnect, Add::Name.ToString()))
	{
		NodeToConnect = AddNode;
	}

	UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(Connect.ParentShaderNode, Connect.InputChannelName, NodeToConnect->GetUniqueID());
}

void FMaterialXSurfaceShaderAbstract::ConnectCellNoise2DInputToOutput(const FConnectNode& Connect)
{
	using namespace UE::Interchange::Materials::Standard::Nodes;

	// UE's Cellnoise algorithm returns a float3 but MaterialX returns only a float
	UInterchangeShaderNode* CellNoiseNode = ConnectNoise2DInputToOutput(Connect, VectorNoise::Name.ToString(), EVectorNoiseFunction::VNF_CellnoiseALU, 0b1000);
	UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(Connect.ParentShaderNode, Connect.InputChannelName, CellNoiseNode->GetUniqueID());
}

void FMaterialXSurfaceShaderAbstract::ConnectWorleyNoise2DInputToOutput(const FConnectNode& Connect)
{
	using namespace UE::Interchange::Materials::Standard::Nodes;

	// UE's Voronoi algorithm returns either XYZ (the position of a Cell in Voronoi) or W which is the distance (what we want in that case)
	// Sadly we have no way to return the distance to the closest seed in another dimensions, since the component is too deep in the algorithm (see VoronoiNoise3D_ALU in Random.ush)
	UInterchangeShaderNode* WorleyNoiseNode = ConnectNoise2DInputToOutput(Connect, VectorNoise::Name.ToString(), EVectorNoiseFunction::VNF_VoronoiALU, 0b0001);
	UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(Connect.ParentShaderNode, Connect.InputChannelName, WorleyNoiseNode->GetUniqueID());
}

void FMaterialXSurfaceShaderAbstract::ConnectNoise3DInputToOutput(const FConnectNode& Connect)
{
	using namespace UE::Interchange::Materials::Standard::Nodes;

	// MaterialX defines the Noise3d as Perlin Noise which is multiplied by the Amplitude then Added to Pivot
	UInterchangeShaderNode* NoiseNode = CreateShaderNode(Connect.UpstreamNode, Connect.UpstreamNode->getName().c_str(), VectorNoise::Name.ToString());
	NoiseNode->AddInt32Attribute(VectorNoise::Attributes::Function.ToString(), EVectorNoiseFunction::VNF_VectorALU);

	UInterchangeShaderNode* NodeToConnect = NoiseNode;

	// Multiply Node
	auto ConnectNodeToInput = [&](mx::InputPtr Input, UInterchangeShaderNode* NodeToConnectTo, const FString& ShaderType) -> UInterchangeShaderNode*
	{
		if(!Input)
		{
			return nullptr;
		}

		const FString ShaderNodeName = Connect.UpstreamNode->getName().c_str() + FString{ TEXT("_") } + ShaderType;
		UInterchangeShaderNode* ShaderNode = CreateShaderNode(Connect.UpstreamNode, ShaderNodeName, ShaderType);

		UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(ShaderNode, TEXT("A"), NodeToConnectTo->GetUniqueID());

		// Connect the amplitude node to the shader node not the noise
		// it will be handle during the upstream-downstream connection phase
		Input->setAttribute(mx::Attributes::ParentName, TCHAR_TO_UTF8(*ShaderNodeName));
		AddAttributeFromValueOrInterface(Input, UE::Expressions::Inputs::B, ShaderNode);

		return ShaderNode;
	};

	if(UInterchangeShaderNode* MultiplyNode = ConnectNodeToInput(Connect.UpstreamNode->getInput("amplitude"), NoiseNode, Multiply::Name.ToString()))
	{
		NodeToConnect = MultiplyNode;
	}

	if(UInterchangeShaderNode* AddNode = ConnectNodeToInput(Connect.UpstreamNode->getInput("pivot"), NodeToConnect, Add::Name.ToString()))
	{
		NodeToConnect = AddNode;
	}

	const std::string& Type = Connect.UpstreamNode->getType();
	uint8 Mask =
		Type == "float" ? 0b1000 :
		Type == "vector2" ? 0b1100 :
		0b1110;

	UInterchangeShaderNode* MaskNode = CreateMaskShaderNode(Mask, Connect.UpstreamNode, (Connect.UpstreamNode->getName() + "_ComponentMask").c_str());

	UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(MaskNode, Mask::Inputs::Input.ToString(), NodeToConnect->GetUniqueID());
	UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(Connect.ParentShaderNode, Connect.InputChannelName, MaskNode->GetUniqueID());
}

void FMaterialXSurfaceShaderAbstract::ConnectCellNoise3DInputToOutput(const FConnectNode& Connect)
{
	using namespace UE::Interchange::Materials::Standard::Nodes;

	// Let's use a vector noise for this one, the only one that is close to MaterialX implementation
	UInterchangeShaderNode* NoiseNode = CreateShaderNode(Connect.UpstreamNode, Connect.UpstreamNode->getName().c_str(), VectorNoise::Name.ToString());
	NoiseNode->AddInt32Attribute(VectorNoise::Attributes::Function.ToString(), EVectorNoiseFunction::VNF_CellnoiseALU);

	//cellnoise3d only supports float output
	UInterchangeShaderNode* MaskNode = CreateMaskShaderNode(0b1000, Connect.UpstreamNode, (Connect.UpstreamNode->getName() + "_ComponentMask").c_str());

	UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(MaskNode, Mask::Inputs::Input.ToString(), NoiseNode->GetUniqueID());
	UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(Connect.ParentShaderNode, Connect.InputChannelName, MaskNode->GetUniqueID());
}

void FMaterialXSurfaceShaderAbstract::ConnectWorleyNoise3DInputToOutput(const FConnectNode& Connect)
{
	using namespace UE::Interchange::Materials::Standard::Nodes;

	//Also called Voronoi, the implementation is a bit different in UE, especially we don't have access to the jitter
	UInterchangeShaderNode* NoiseNode = CreateShaderNode(Connect.UpstreamNode, Connect.UpstreamNode->getName().c_str(), VectorNoise::Name.ToString());
	NoiseNode->AddInt32Attribute(Noise::Attributes::Function.ToString(), EVectorNoiseFunction::VNF_VoronoiALU);
	// 3x3x3 neighborhood
	NoiseNode->AddInt32Attribute(Noise::Attributes::Quality.ToString(), 3);

	// Voronoi only supports float distance to the seed
	UInterchangeShaderNode* MaskNode = CreateMaskShaderNode(0b0001, Connect.UpstreamNode, (Connect.UpstreamNode->getName() + "_ComponentMask").c_str());

	UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(MaskNode, Mask::Inputs::Input.ToString(), NoiseNode->GetUniqueID());
	UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(Connect.ParentShaderNode, Connect.InputChannelName, MaskNode->GetUniqueID());
}

void FMaterialXSurfaceShaderAbstract::ConnectHeightToNormalInputToOutput(const FConnectNode& Connect)
{
	if(mx::InputPtr Input = Connect.UpstreamNode->getInput("in"))
	{
		// Image node will become this node
		if(mx::NodePtr ConnectedNode = Input->getConnectedNode();
		   ConnectedNode && ConnectedNode->getCategory() == mx::Category::Image)
		{
			//we need to copy the content of the image node to this node
			Connect.UpstreamNode->copyContentFrom(ConnectedNode);

			// the copy overwrite every attribute of the node, so we need to get them back, essentially the type and the renaming
			// the output is always a vec3
			Connect.UpstreamNode->setType(mx::Type::Vector3);

			SetMatchingInputsNames(Connect.UpstreamNode);

			mx::NodeGraphPtr Graph = Connect.UpstreamNode->getParent()->asA<mx::NodeGraph>();
			Graph->removeNode(ConnectedNode->getName());

			using namespace UE::Interchange::Materials::Standard::Nodes;

			if(UInterchangeTextureNode* TextureNode = CreateTextureNode<UInterchangeTexture2DNode>(Connect.UpstreamNode))
			{
				UInterchangeShaderNode* HeightMapNode = CreateShaderNode(Connect.UpstreamNode, Connect.UpstreamNode->getName().c_str(), NormalFromHeightMap::Name.ToString());
				UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(Connect.ParentShaderNode, Connect.InputChannelName, HeightMapNode->GetUniqueID());

				const FString TextureNodeName = Connect.UpstreamNode->getName().c_str() + FString{ "_texture" };
				UInterchangeShaderNode* TextureShaderNode = CreateShaderNode(Connect.UpstreamNode, TextureNodeName, TextureObject::Name.ToString());
				TextureShaderNode->AddStringAttribute(UInterchangeShaderPortsAPI::MakeInputValueKey(TextureObject::Inputs::Texture.ToString()), TextureNode->GetUniqueID());
				UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(HeightMapNode, NormalFromHeightMap::Inputs::HeightMap.ToString(), TextureShaderNode->GetUniqueID());

				AddAttributeFromValueOrInterface(Connect.UpstreamNode->getInput("scale"), NormalFromHeightMap::Inputs::Intensity.ToString(), HeightMapNode);
			}
			else
			{
				AddAttributeFromValueOrInterface(Connect.UpstreamNode->getInput(mx::NodeGroup::Texture2D::Inputs::Default), Connect.InputChannelName, Connect.ParentShaderNode);
			}
		}
		else
		{
			using namespace UE::Interchange::Materials::Standard::Nodes;
			SetAttributeNewName(Connect.UpstreamNode->getInput("in"), "Height");
			// This MF outputs a normal vector in World Space, we need to transform the output in Tangent Space
			UInterchangeFunctionCallShaderNode* HeightToNormalSmoothShaderNode =
				CreateFunctionCallShaderNode(Connect.UpstreamNode,
											 Connect.UpstreamNode->getName().c_str(),
											 UE::MaterialFunctions::Path::HeightToNormalSmooth);

			UInterchangeShaderNode* TransformNode = CreateShaderNode(Connect.UpstreamNode, (Connect.UpstreamNode->getName() + "_TS").c_str(), TransformVector::Name.ToString());
			TransformNode->AddInt32Attribute(TransformVector::Attributes::TransformSourceType.ToString(), EMaterialVectorCoordTransformSource::TRANSFORMSOURCE_World);
			TransformNode->AddInt32Attribute(TransformVector::Attributes::TransformType.ToString(), EMaterialVectorCoordTransform::TRANSFORM_Tangent);

			// By default positions in MaterialX are in object space so we need to create a local position expression here.
			// TODO: Handling it as a geompropdef so all nodes refer to the same LocalPosition expression
			UInterchangeShaderNode* PositionNode = CreateShaderNode(Connect.UpstreamNode, (Connect.UpstreamNode->getName() + "_Position").c_str(), UE::Expressions::Names::LocalPosition);
			UInterchangeShaderNode* UnitNode = CreateShaderNode(Connect.UpstreamNode, (Connect.UpstreamNode->getName() + "_Unit").c_str(), Multiply::Name.ToString());

			UnitNode->AddFloatAttribute(UInterchangeShaderPortsAPI::MakeInputValueKey(Multiply::Inputs::B.ToString()), 0.01f);
			UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(UnitNode, Multiply::Inputs::A.ToString(), PositionNode->GetUniqueID());
			UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(HeightToNormalSmoothShaderNode, UE::Expressions::Inputs::AbsoluteWorldPosition, UnitNode->GetUniqueID());

			UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(TransformNode, TransformVector::Inputs::Input.ToString(), HeightToNormalSmoothShaderNode->GetUniqueID());
			UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(Connect.ParentShaderNode, Connect.InputChannelName, TransformNode->GetUniqueID());
		}
	}
}

void FMaterialXSurfaceShaderAbstract::ConnectBlurInputToOutput(const FConnectNode& Connect)
{
	if(mx::InputPtr Input = Connect.UpstreamNode->getInput("in"))
	{
		// Image node will become this node
		if(mx::NodePtr ConnectedNode = Input->getConnectedNode();
		   ConnectedNode && ConnectedNode->getCategory() == mx::Category::Image)
		{
			std::string NodeType = Connect.UpstreamNode->getType();

			//we need to copy the content of the image node to this node
			Connect.UpstreamNode->copyContentFrom(ConnectedNode);

			//the copy overwrites every attribute of the node, so we need to get them back, essentially the type and the renaming
			Connect.UpstreamNode->setType(NodeType);

			SetMatchingInputsNames(Connect.UpstreamNode);

			mx::NodeGraphPtr Graph = Connect.UpstreamNode->getParent()->asA<mx::NodeGraph>();
			Graph->removeNode(ConnectedNode->getName());

			using namespace UE::Interchange::Materials::Standard::Nodes;

			if(UInterchangeTextureNode* TextureNode = CreateTextureNode<UInterchangeTextureBlurNode>(Connect.UpstreamNode))
			{
				FString OutputChannel{ TEXT("RGB") };

				if(NodeType == mx::Type::Vector4 || NodeType == mx::Type::Color4)
				{
					OutputChannel = TEXT("RGBA");
				}
				else if(NodeType == mx::Type::Float)
				{
					OutputChannel = TEXT("R");
				}

				UInterchangeShaderNode* TextureShaderNode = CreateShaderNode(Connect.UpstreamNode, Connect.UpstreamNode->getName().c_str(), TextureSampleBlur::Name.ToString());
				TextureShaderNode->AddStringAttribute(UInterchangeShaderPortsAPI::MakeInputValueKey(TextureSampleBlur::Inputs::Texture.ToString()), TextureNode->GetUniqueID());
				UInterchangeShaderPortsAPI::ConnectOuputToInputByName(Connect.ParentShaderNode, Connect.InputChannelName, TextureShaderNode->GetUniqueID(), OutputChannel);

				if(mx::InputPtr InputKernel = Connect.UpstreamNode->getInput("filtertype"))
				{
					// By default TextureSampleBox uses gaussian filter
					if(InputKernel->getValueString() == "box")
					{
						TextureShaderNode->AddInt32Attribute(TextureSampleBlur::Attributes::Filter.ToString(), int32(EMaterialXTextureSampleBlurFilter::Box));
					}
				}

				if(mx::InputPtr InputKernel = Connect.UpstreamNode->getInput("size"))
				{
					if(InputKernel->hasValueString())
					{
						float KernelSize = mx::fromValueString<float>(InputKernel->getValueString());
						constexpr float Kernel1x1 = 0.f / 3.f;
						constexpr float Kernel3x3 = 1.f / 3.f;
						constexpr float Kernel5x5 = 2.f / 3.f;
						constexpr float Kernel7x7 = 3.f / 3.f;
						TextureShaderNode->AddInt32Attribute(TextureSampleBlur::Attributes::KernelSize.ToString(),
															 FMath::IsNearlyEqual(KernelSize, Kernel1x1) ? int32(EMAterialXTextureSampleBlurKernel::Kernel1) :
															 KernelSize <= Kernel3x3 ? int32(EMAterialXTextureSampleBlurKernel::Kernel3) :
															 KernelSize <= Kernel5x5 ? int32(EMAterialXTextureSampleBlurKernel::Kernel5) :
															 KernelSize <= Kernel7x7 ? int32(EMAterialXTextureSampleBlurKernel::Kernel7) :
															 int32(EMAterialXTextureSampleBlurKernel::Kernel1));
					}
					else
					{
						UE_LOG(LogInterchangeImport, Warning, TEXT("<%s>: input 'size' must have a value."), UTF8_TO_TCHAR(Connect.UpstreamNode->getName().c_str()));
					}
				}
			}
			else
			{
				AddAttributeFromValueOrInterface(Connect.UpstreamNode->getInput(mx::NodeGroup::Texture2D::Inputs::Default), Connect.InputChannelName, Connect.ParentShaderNode);
			}
		}
		else
		{
			// For a blur it doesn't make sense if there's no image input
			SetAttributeNewName(Input, TCHAR_TO_UTF8(*Connect.InputChannelName)); //let's take the parent node's input name
			ShaderNodes.Add({ Connect.UpstreamNode->getName().c_str(), Connect.OutputName }, Connect.ParentShaderNode);
		}
	}
}

void FMaterialXSurfaceShaderAbstract::ConnectTexCoordInputToOutput(const FConnectNode& Connect)
{
	using namespace UE::Interchange::Materials::Standard::Nodes;
	UInterchangeShaderNode* TexCoord = CreateShaderNode(Connect.UpstreamNode, Connect.UpstreamNode->getName().c_str(), TextureCoordinate::Name.ToString());
	if(mx::InputPtr Input = Connect.UpstreamNode->getInput("index"))
	{
		TexCoord->AddInt32Attribute(UInterchangeShaderPortsAPI::MakeInputValueKey(TextureCoordinate::Inputs::Index.ToString()), mx::fromValueString<int>(Input->getValueString()));
	}

	UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(Connect.ParentShaderNode, Connect.InputChannelName, TexCoord->GetUniqueID());
}

void FMaterialXSurfaceShaderAbstract::ConnectSeparateInputToOutput(const FConnectNode& Connect)
{
	bool bHasValueOrInterface = false;
	if(mx::InputPtr Input = Connect.UpstreamNode->getInput("in"))
	{
		bHasValueOrInterface = Input->hasValue() || Input->hasInterfaceName();
	}

	uint8 Mask = 0b1110;
	int32 OutpuxIndex = 0;

	if(Connect.OutputName == TEXT("outx") || Connect.OutputName == TEXT("outr"))
	{
		Mask = 0b1000;
		OutpuxIndex = 1;
	}
	else if(Connect.OutputName == TEXT("outy") || Connect.OutputName == TEXT("outg"))
	{
		Mask = 0b0100;
		OutpuxIndex = 2;
	}
	else if(Connect.OutputName == TEXT("outz") || Connect.OutputName == TEXT("outb"))
	{
		Mask = 0b0010;
		OutpuxIndex = 3;
	}
	else if(Connect.OutputName == TEXT("outw") || Connect.OutputName == TEXT("outa"))
	{
		Mask = 0b0001;
		OutpuxIndex = 4;
	}
	else
	{
		UE_LOG(LogInterchangeImport, Warning, TEXT("output <%s> not defined in <%s>."), *Connect.OutputName, UTF8_TO_TCHAR(Connect.UpstreamNode->getCategory().c_str()));
	}

	if(!bHasValueOrInterface)
	{
		UInterchangeShaderNode* OutNode = CreateMaskShaderNode(Mask, Connect.UpstreamNode, Connect.UpstreamNode->getName().c_str(), Connect.OutputName);
		UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(Connect.ParentShaderNode, Connect.InputChannelName, OutNode->GetUniqueID());
	}
	else
	{
		AddAttributeFromValueOrInterface(Connect.UpstreamNode->getInput("in"), Connect.InputChannelName, Connect.ParentShaderNode, OutpuxIndex);
	}
}

void FMaterialXSurfaceShaderAbstract::ConnectSwitchInputToOutput(const FConnectNode& Connect)
{
	using namespace UE::Interchange::Materials::Standard::Nodes;
	UInterchangeShaderNode* SwitchNode = CreateShaderNode(Connect.UpstreamNode, Connect.UpstreamNode->getName().c_str(), Switch::Name.ToString());

	int32 Index = 0;

	for (mx::InputPtr Input : Connect.UpstreamNode->getInputs())
	{
		if (Input->getName() != "which")
		{
			SwitchNode->AddStringAttribute(Switch::Inputs::InputName.ToString() + FString::FromInt(Index), Input->getName().c_str());
			AddAttributeFromValueOrInterface(Input, Input->getName().c_str(), SwitchNode);
			++Index;
		}
		else
		{
			AddAttributeFromValueOrInterface(Input, Switch::Inputs::Value.ToString(), SwitchNode);
		}
	}

	//In that case we also need to add an attribute to Default
	if (mx::InputPtr Input = Connect.UpstreamNode->getInput("in1"))
	{
		AddAttributeFromValueOrInterface(Input, Switch::Inputs::Default.ToString(), SwitchNode);

		//Let's add a new input that is a copy of in1 to connect it to the Default input of the MaterialExpressionSwitch
		if(!Connect.UpstreamNode->getInput("default"))
		{
			mx::InputPtr InputDefault = Connect.UpstreamNode->addInput("default");
			InputDefault->copyContentFrom(Input);
			SetAttributeNewName(InputDefault, TCHAR_TO_UTF8(*Switch::Inputs::Default.ToString()));
		}
	}

	SwitchNode->AddInt32Attribute(Switch::Attributes::InputCount.ToString(), Index);
	UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(Connect.ParentShaderNode, Connect.InputChannelName, SwitchNode->GetUniqueID());
}

void FMaterialXSurfaceShaderAbstract::ConnectSwizzleInputToOutput(const FConnectNode& Connect)
{
	using namespace UE::Interchange::Materials::Standard::Nodes;

	UInterchangeShaderNode* SwizzleNode = CreateShaderNode(Connect.UpstreamNode, Connect.UpstreamNode->getName().c_str(), Swizzle::Name.ToString());
	if(mx::InputPtr Input = Connect.UpstreamNode->getInput("in"))
	{
		AddAttributeFromValueOrInterface(Input, GetInputName(Input), SwizzleNode);
	}
	if(mx::InputPtr Input = Connect.UpstreamNode->getInput("channels"))
	{
		SwizzleNode->AddStringAttribute(Swizzle::Attributes::Channels.ToString(), Input->getValueString().c_str());
	}

	UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(Connect.ParentShaderNode, Connect.InputChannelName, SwizzleNode->GetUniqueID());
}

void FMaterialXSurfaceShaderAbstract::ConnectNormalMapInputToOutput(const FConnectNode& Connect)
{
	using namespace UE::Interchange::Materials::Standard::Nodes;

	// Only create a FunctionCall if there's a scale and the value is not 1, otherwise just like dot
	mx::InputPtr Input = Connect.UpstreamNode->getInput("scale");
	bool bIsNotEqualOne = true;
	if(Input && Input->hasValueString())
	{
		bIsNotEqualOne = (mx::fromValueString<float>(Input->getValueString()) != 1.f);
	}
	else if(Input && Input->hasInterfaceName())
	{
		bIsNotEqualOne = (mx::fromValueString<float>(Input->getInterfaceInput()->getValueString()) != 1.f);
	}

	if(Input && bIsNotEqualOne)
	{
		UInterchangeShaderNode* FlattenNormalNode = CreateFunctionCallShaderNode(Connect.UpstreamNode, Connect.UpstreamNode->getName().c_str(), TEXT("/Engine/Functions/Engine_MaterialFunctions01/Texturing/FlattenNormal.FlattenNormal"));

		UInterchangeShaderNode* OneMinusNode = CreateShaderNode(Connect.UpstreamNode, (Connect.UpstreamNode->getName() + "_OneMinusFlatness").c_str(), OneMinus::Name.ToString());
		UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(FlattenNormalNode, FlattenNormal::Inputs::Flatness.ToString(), OneMinusNode->GetUniqueID());

		AddAttributeFromValueOrInterface(Input, OneMinus::Inputs::Input.ToString(), OneMinusNode);
		UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(Connect.ParentShaderNode, Connect.InputChannelName, FlattenNormalNode->GetUniqueID());
	}
	else
	{
		SetAttributeNewName(Connect.UpstreamNode->getInput("in"), TCHAR_TO_UTF8(*Connect.InputChannelName)); //let's take the parent node's input name
		ShaderNodes.Add({ Connect.UpstreamNode->getName().c_str(), Connect.OutputName }, Connect.ParentShaderNode);
	}
}

void FMaterialXSurfaceShaderAbstract::ConnectRefractInputToOutput(const FConnectNode& Connect)
{
	UInterchangeShaderNode* RefractNode = CreateFunctionCallShaderNode(Connect.UpstreamNode, Connect.UpstreamNode->getName().c_str(), TEXT("/Engine/Functions/Engine_MaterialFunctions01/Vectors/Refract.Refract"));
	UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(Connect.ParentShaderNode, Connect.InputChannelName, RefractNode->GetUniqueID());

	for (mx::InputPtr Input : Connect.UpstreamNode->getInputs())
	{
		AddAttributeFromValueOrInterface(Input, GetInputName(Input), RefractNode);
	}

	//Let's add a new input to map its value to the Refractive Index Target since its default value is 1.33 and we want it be 1
	// Because the Material Function does the operation IOR_SRC/IOR_TARGET
	mx::InputPtr InputIorTarget = Connect.UpstreamNode->getInput("ior_target");
	if(!InputIorTarget)
	{
		InputIorTarget = Connect.UpstreamNode->addInput("ior_target");
		InputIorTarget->setValue("1", "float");
		SetAttributeNewName(InputIorTarget, TCHAR_TO_ANSI(UE::Expressions::Inputs::RefractiveIndexTarget));
	}
	AddAttributeFromValueOrInterface(InputIorTarget, UE::Expressions::Inputs::RefractiveIndexTarget, RefractNode);
}

void FMaterialXSurfaceShaderAbstract::ConnectViewDirectionInputToOutput(const FConnectNode& Connect)
{
	using namespace UE::Interchange::Materials::Standard::Nodes;

	UInterchangeShaderNode* ViewDirectionNode = CreateShaderNode(Connect.UpstreamNode, Connect.UpstreamNode->getName().c_str(), UE::Expressions::Names::CameraVectorWS);
	UInterchangeShaderNode* NegateNode = CreateShaderNode(Connect.UpstreamNode, (Connect.UpstreamNode->getName() + "_Negate").c_str(), UE::Expressions::Names::Subtract);
	NegateNode->AddFloatAttribute(UInterchangeShaderPortsAPI::MakeInputValueKey(UE::Expressions::Inputs::A), 0.f);
	UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(NegateNode, UE::Expressions::Inputs::B, ViewDirectionNode->GetUniqueID());
	
	UInterchangeShaderNode* NodeToConnectTo = NegateNode;
	if (mx::InputPtr InputSpace = Connect.UpstreamNode->getInput("space"))
	{
		const bool bIsObjectSpace = InputSpace->getValueString() != "world";
		if (bIsObjectSpace)
		{
			UInterchangeShaderNode* TransformNode = CreateShaderNode(Connect.UpstreamNode, Connect.UpstreamNode->getName().c_str() + FString(TEXT("_Transform")), TransformVector::Name.ToString());
			TransformNode->AddInt32Attribute(TransformVector::Attributes::TransformSourceType.ToString(), EMaterialVectorCoordTransformSource::TRANSFORMSOURCE_World);
			TransformNode->AddInt32Attribute(TransformVector::Attributes::TransformType.ToString(), EMaterialVectorCoordTransform::TRANSFORM_Local);
			UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(TransformNode, TransformVector::Inputs::Input.ToString(), NodeToConnectTo->GetUniqueID());
			NodeToConnectTo = TransformNode;
		}
	}
	UInterchangeShaderPortsAPI::ConnectDefaultOuputToInput(Connect.ParentShaderNode, Connect.InputChannelName, NodeToConnectTo->GetUniqueID());
}

UInterchangeShaderNode* FMaterialXSurfaceShaderAbstract::CreateMaskShaderNode(uint8 RGBA, mx::ElementPtr Element, const FString& NodeName, const FString& OutputName)
{
	bool bR = (0b1000 & RGBA) >> 3;
	bool bG = (0b0100 & RGBA) >> 2;
	bool bB = (0b0010 & RGBA) >> 1;
	bool bA = (0b0001 & RGBA) >> 0;
	using namespace UE::Interchange::Materials::Standard::Nodes;
	UInterchangeShaderNode* MaskShaderNode = CreateShaderNode(Element, NodeName, Mask::Name.ToString(), OutputName);
	MaskShaderNode->AddBooleanAttribute(Mask::Attributes::R.ToString(), bR);
	MaskShaderNode->AddBooleanAttribute(Mask::Attributes::G.ToString(), bG);
	MaskShaderNode->AddBooleanAttribute(Mask::Attributes::B.ToString(), bB);
	MaskShaderNode->AddBooleanAttribute(Mask::Attributes::A.ToString(), bA);

	return MaskShaderNode;
}

UInterchangeShaderNode* FMaterialXSurfaceShaderAbstract::CreateShaderNode(mx::ElementPtr Element, const FString& NodeName, const FString& ShaderType, const FString& OutputName)
{
	UInterchangeShaderNode* Node;

	const FString NodeUID = UInterchangeShaderNode::MakeNodeUid(GetUniqueName(Element) + TEXT("_") + NodeName + TEXT('_') + OutputName, FStringView{});

	//Test directly in the NodeContainer, because the ShaderNodes can be altered during the node graph either by the parent (dot/normalmap),
	//or by putting an intermediary node between the child and the parent (tiledimage)
	if(Node = const_cast<UInterchangeShaderNode*>(Cast<UInterchangeShaderNode>(NodeContainer.GetNode(NodeUID))); !Node)
	{
		Node = NewObject<UInterchangeShaderNode>(&NodeContainer);
		NodeContainer.SetupNode(Node, NodeUID, NodeName, EInterchangeNodeContainerType::TranslatedAsset);
		Node->SetCustomShaderType(ShaderType);

		ShaderNodes.Add({ NodeName, OutputName }, Node);
	}

	return Node;
}

UInterchangeFunctionCallShaderNode* FMaterialXSurfaceShaderAbstract::CreateFunctionCallShaderNode(MaterialX::ElementPtr Element, const FString& NodeName, const FString& FunctionPath, const FString& OutputName)
{
	UInterchangeFunctionCallShaderNode* Node;

	const FString NodeUID = UInterchangeShaderNode::MakeNodeUid(GetUniqueName(Element) + TEXT("_") + NodeName  + TEXT('_') + OutputName, FStringView{});

	if(Node = const_cast<UInterchangeFunctionCallShaderNode*>(Cast<UInterchangeFunctionCallShaderNode>(NodeContainer.GetNode(NodeUID))); !Node)
	{
		Node = NewObject<UInterchangeFunctionCallShaderNode>(&NodeContainer);
		NodeContainer.SetupNode(Node, NodeUID, NodeName, EInterchangeNodeContainerType::TranslatedAsset);
		Node->SetCustomMaterialFunction(FunctionPath);

		ShaderNodes.Add({ NodeName, OutputName}, Node);
	}

	return Node;
}

UInterchangeFunctionCallShaderNode* FMaterialXSurfaceShaderAbstract::CreateFunctionCallShaderNode(MaterialX::ElementPtr Element, const FString& NodeName, uint8 EnumType, uint8 EnumValue, const FString& OutputName)
{
	UInterchangeFunctionCallShaderNode* Node;

	const FString NodeUID = UInterchangeShaderNode::MakeNodeUid(GetUniqueName(Element) + TEXT("_") + NodeName + TEXT('_') + OutputName, FStringView{});

	if(Node = const_cast<UInterchangeFunctionCallShaderNode*>(Cast<UInterchangeFunctionCallShaderNode>(NodeContainer.GetNode(NodeUID))); !Node)
	{
		Node = NewObject<UInterchangeFunctionCallShaderNode>(&NodeContainer);
		NodeContainer.SetupNode(Node, NodeUID, NodeName, EInterchangeNodeContainerType::TranslatedAsset);
		//this is just a dummy path name so the Generic Material Pipeline consider it as a FunctionCallShader but where in fact the path is given by an enum
		Node->SetCustomMaterialFunction(TEXT("/Game/Default.Default"));
		Node->AddInt32Attribute(UE::Interchange::MaterialX::Attributes::EnumType, EnumType);
		Node->AddInt32Attribute(UE::Interchange::MaterialX::Attributes::EnumValue, EnumValue);

		ShaderNodes.Add({ NodeName, OutputName }, Node);
	}

	return Node;
}

const FString& FMaterialXSurfaceShaderAbstract::GetMatchedInputName(MaterialX::NodePtr Node, MaterialX::InputPtr Input) const
{
	FMaterialXManager& Manager = FMaterialXManager::GetInstance();

	if(Input)
	{
		const FString NodeCategory{ Node->getCategory().c_str() };
		const FString InputName{ GetInputName(Input) };

		if(const FString* Result = Manager.FindMatchingInput(NodeCategory, InputName, Node->getNodeDef(mx::EMPTY_STRING, true)->getNodeGroup().c_str(), Node->getType().c_str()))
		{
			return *Result;
		}
		else if((Result = Manager.FindMatchingInput(NodeCategory, InputName)))
		{
			return *Result;
		}
		else if((Result = Manager.FindMatchingInput(EmptyString, InputName)))
		{
			return *Result;
		}
	}

	return EmptyString;
}

FString FMaterialXSurfaceShaderAbstract::GetInputName(MaterialX::InputPtr Input) const
{
	if(Input->hasAttribute(mx::Attributes::NewName))
	{
		return Input->getAttribute(mx::Attributes::NewName).c_str();
	}
	else
	{
		return Input->getName().c_str();
	}
}

FString FMaterialXSurfaceShaderAbstract::GetFilePrefix(MaterialX::ElementPtr Element) const
{
	FString FilePrefix;

	if(Element)
	{
		if(Element->hasFilePrefix())
		{
			return FString(Element->getFilePrefix().c_str());
		}
		else
		{
			return GetFilePrefix(Element->getParent());
		}
	}

	return FilePrefix;
}

FLinearColor FMaterialXSurfaceShaderAbstract::GetVector(MaterialX::InputPtr Input) const
{
	FLinearColor LinearColor = FLinearColor::Black;

	if(Input->getType() == mx::Type::Vector2)
	{
		mx::Vector2 Color = mx::fromValueString<mx::Vector2>(Input->getValueString());
		LinearColor = FLinearColor{ Color[0], Color[1], 0 };
	}
	else if(Input->getType() == mx::Type::Vector3)
	{
		mx::Vector3 Color = mx::fromValueString<mx::Vector3>(Input->getValueString());
		LinearColor = FLinearColor{ Color[0], Color[1], Color[2] };
	}
	else if(Input->getType() == mx::Type::Vector4)
	{
		mx::Vector4 Color = mx::fromValueString<mx::Vector4>(Input->getValueString());
		LinearColor = FLinearColor{ Color[0], Color[1], Color[2], Color[3] };
	}
	else
	{
		ensureMsgf(false, TEXT("Input type can only be a vectorN."));
	}

	return LinearColor;
}

FString FMaterialXSurfaceShaderAbstract::GetAttributeParentName(MaterialX::NodePtr Node, MaterialX::InputPtr ConnectedInput) const
{
	if (ConnectedInput && ConnectedInput->hasAttribute(mx::Attributes::ParentName))
	{
		return ConnectedInput->getAttribute(mx::Attributes::ParentName).c_str();
	}

	return Node->getName().c_str();
}

namespace
{
	void GetUniqueName(mx::ElementPtr Element, TStringBuilder<256>& Buffer, const FString& SurfaceMaterialName)
	{
		//Write the hierarchy name as A_B_C
		if (Element)
		{
			if (mx::ElementPtr Parent = Element->getParent())
			{
				GetUniqueName(Parent, Buffer, SurfaceMaterialName);
			}
			else
			{
				Buffer += SurfaceMaterialName + TEXT("_");
			}

			Buffer += Element->getName().c_str();
			Buffer += TEXT("_");
			Element->setAttribute(mx::Attributes::UniqueName, TCHAR_TO_UTF8(*FString{ Buffer.ToString() }.TrimChar(TEXT('_'))));
		}
	}
}

FString FMaterialXSurfaceShaderAbstract::GetUniqueName(MaterialX::ElementPtr Element) const
{
	if (Element->hasAttribute(mx::Attributes::UniqueName))
	{
		return Element->getAttribute(mx::Attributes::UniqueName).c_str();
	}
	else
	{
		TStringBuilder<256> Buffer;
		::GetUniqueName(Element, Buffer, SurfaceMaterialName);
		return FString{ Buffer.ToString() }.TrimChar(TEXT('_')); // remove any trailing '_'
	}
}

void FMaterialXSurfaceShaderAbstract::RegisterConnectNodeOutputToInputDelegates()
{
	MatchingConnectNodeDelegates.Add(mx::Category::Constant,		FOnConnectNodeOutputToInput::CreateSP(this, &FMaterialXSurfaceShaderAbstract::ConnectConstantInputToOutput));
	MatchingConnectNodeDelegates.Add(mx::Category::Extract,			FOnConnectNodeOutputToInput::CreateSP(this, &FMaterialXSurfaceShaderAbstract::ConnectExtractInputToOutput));
	MatchingConnectNodeDelegates.Add(mx::Category::Dot,				FOnConnectNodeOutputToInput::CreateSP(this, &FMaterialXSurfaceShaderAbstract::ConnectDotInputToOutput));
	MatchingConnectNodeDelegates.Add(mx::Category::NormalMap,		FOnConnectNodeOutputToInput::CreateSP(this, &FMaterialXSurfaceShaderAbstract::ConnectNormalMapInputToOutput));
	MatchingConnectNodeDelegates.Add(mx::Category::TransformPoint,	FOnConnectNodeOutputToInput::CreateSP(this, &FMaterialXSurfaceShaderAbstract::ConnectTransformPositionInputToOutput));
	MatchingConnectNodeDelegates.Add(mx::Category::TransformVector, FOnConnectNodeOutputToInput::CreateSP(this, &FMaterialXSurfaceShaderAbstract::ConnectTransformVectorInputToOutput));
	MatchingConnectNodeDelegates.Add(mx::Category::TransformNormal, FOnConnectNodeOutputToInput::CreateSP(this, &FMaterialXSurfaceShaderAbstract::ConnectTransformVectorInputToOutput));
	MatchingConnectNodeDelegates.Add(mx::Category::Rotate2D,		FOnConnectNodeOutputToInput::CreateSP(this, &FMaterialXSurfaceShaderAbstract::ConnectRotate2DInputToOutput));
	MatchingConnectNodeDelegates.Add(mx::Category::Rotate3D,		FOnConnectNodeOutputToInput::CreateSP(this, &FMaterialXSurfaceShaderAbstract::ConnectRotate3DInputToOutput));
	MatchingConnectNodeDelegates.Add(mx::Category::Image,			FOnConnectNodeOutputToInput::CreateSP(this, &FMaterialXSurfaceShaderAbstract::ConnectImageInputToOutput));
	MatchingConnectNodeDelegates.Add(mx::Category::Convert,			FOnConnectNodeOutputToInput::CreateSP(this, &FMaterialXSurfaceShaderAbstract::ConnectConvertInputToOutput));
	MatchingConnectNodeDelegates.Add(mx::Category::IfGreater,		FOnConnectNodeOutputToInput::CreateSP(this, &FMaterialXSurfaceShaderAbstract::ConnectIfGreaterInputToOutput));
	MatchingConnectNodeDelegates.Add(mx::Category::IfGreaterEq,		FOnConnectNodeOutputToInput::CreateSP(this, &FMaterialXSurfaceShaderAbstract::ConnectIfGreaterEqInputToOutput));
	MatchingConnectNodeDelegates.Add(mx::Category::IfEqual,			FOnConnectNodeOutputToInput::CreateSP(this, &FMaterialXSurfaceShaderAbstract::ConnectIfEqualInputToOutput));
	MatchingConnectNodeDelegates.Add(mx::Category::Outside,			FOnConnectNodeOutputToInput::CreateSP(this, &FMaterialXSurfaceShaderAbstract::ConnectOutsideInputToOutput));
	MatchingConnectNodeDelegates.Add(mx::Category::Position,		FOnConnectNodeOutputToInput::CreateSP(this, &FMaterialXSurfaceShaderAbstract::ConnectPositionInputToOutput));
	MatchingConnectNodeDelegates.Add(mx::Category::Normal,			FOnConnectNodeOutputToInput::CreateSP(this, &FMaterialXSurfaceShaderAbstract::ConnectNormalInputToOutput));
	MatchingConnectNodeDelegates.Add(mx::Category::Tangent,			FOnConnectNodeOutputToInput::CreateSP(this, &FMaterialXSurfaceShaderAbstract::ConnectTangentInputToOutput));
	MatchingConnectNodeDelegates.Add(mx::Category::Bitangent,		FOnConnectNodeOutputToInput::CreateSP(this, &FMaterialXSurfaceShaderAbstract::ConnectBitangentInputToOutput));
	MatchingConnectNodeDelegates.Add(mx::Category::Time,			FOnConnectNodeOutputToInput::CreateSP(this, &FMaterialXSurfaceShaderAbstract::ConnectTimeInputToOutput));
	MatchingConnectNodeDelegates.Add(mx::Category::Noise2D,			FOnConnectNodeOutputToInput::CreateSP(this, &FMaterialXSurfaceShaderAbstract::ConnectNoise2DInputToOutput));
	MatchingConnectNodeDelegates.Add(mx::Category::Noise3D,			FOnConnectNodeOutputToInput::CreateSP(this, &FMaterialXSurfaceShaderAbstract::ConnectNoise3DInputToOutput));
	MatchingConnectNodeDelegates.Add(mx::Category::CellNoise2D,     FOnConnectNodeOutputToInput::CreateSP(this, &FMaterialXSurfaceShaderAbstract::ConnectCellNoise2DInputToOutput));
	MatchingConnectNodeDelegates.Add(mx::Category::CellNoise3D,		FOnConnectNodeOutputToInput::CreateSP(this, &FMaterialXSurfaceShaderAbstract::ConnectCellNoise3DInputToOutput));
	MatchingConnectNodeDelegates.Add(mx::Category::WorleyNoise2D,	FOnConnectNodeOutputToInput::CreateSP(this, &FMaterialXSurfaceShaderAbstract::ConnectWorleyNoise2DInputToOutput));
	MatchingConnectNodeDelegates.Add(mx::Category::WorleyNoise3D,	FOnConnectNodeOutputToInput::CreateSP(this, &FMaterialXSurfaceShaderAbstract::ConnectWorleyNoise3DInputToOutput));
	MatchingConnectNodeDelegates.Add(mx::Category::Blur,			FOnConnectNodeOutputToInput::CreateSP(this, &FMaterialXSurfaceShaderAbstract::ConnectBlurInputToOutput));
	MatchingConnectNodeDelegates.Add(mx::Category::HeightToNormal,	FOnConnectNodeOutputToInput::CreateSP(this, &FMaterialXSurfaceShaderAbstract::ConnectHeightToNormalInputToOutput));
	MatchingConnectNodeDelegates.Add(mx::Category::Separate2,		FOnConnectNodeOutputToInput::CreateSP(this, &FMaterialXSurfaceShaderAbstract::ConnectSeparateInputToOutput));
	MatchingConnectNodeDelegates.Add(mx::Category::Separate3,		FOnConnectNodeOutputToInput::CreateSP(this, &FMaterialXSurfaceShaderAbstract::ConnectSeparateInputToOutput));
	MatchingConnectNodeDelegates.Add(mx::Category::Separate4,		FOnConnectNodeOutputToInput::CreateSP(this, &FMaterialXSurfaceShaderAbstract::ConnectSeparateInputToOutput));
	MatchingConnectNodeDelegates.Add(mx::Category::TexCoord,		FOnConnectNodeOutputToInput::CreateSP(this, &FMaterialXSurfaceShaderAbstract::ConnectTexCoordInputToOutput));
	MatchingConnectNodeDelegates.Add(mx::Category::Swizzle,			FOnConnectNodeOutputToInput::CreateSP(this, &FMaterialXSurfaceShaderAbstract::ConnectSwizzleInputToOutput));
	MatchingConnectNodeDelegates.Add(mx::Category::Switch,			FOnConnectNodeOutputToInput::CreateSP(this, &FMaterialXSurfaceShaderAbstract::ConnectSwitchInputToOutput));
	MatchingConnectNodeDelegates.Add(mx::Category::Refract,			FOnConnectNodeOutputToInput::CreateSP(this, &FMaterialXSurfaceShaderAbstract::ConnectRefractInputToOutput));
	MatchingConnectNodeDelegates.Add(mx::Category::ViewDirection,	FOnConnectNodeOutputToInput::CreateSP(this, &FMaterialXSurfaceShaderAbstract::ConnectViewDirectionInputToOutput));
}

void FMaterialXSurfaceShaderAbstract::SetMatchingInputsNames(MaterialX::NodePtr Node) const
{
	if(Node)
	{
		if(const std::string& IsVisited = Node->getAttribute(mx::Attributes::IsVisited); IsVisited.empty())
		{
			Node->setAttribute(mx::Attributes::IsVisited, "true");

			for(mx::InputPtr Input : Node->getInputs())
			{
				if(const FString& Name = GetMatchedInputName(Node, Input); !Name.IsEmpty())
				{
					SetAttributeNewName(Input, TCHAR_TO_UTF8(*Name));
				}
			}
		}
	}
}

void FMaterialXSurfaceShaderAbstract::SetAttributeNewName(MaterialX::InputPtr Input, const char* NewName) const
{
	Input->setAttribute(mx::Attributes::NewName, NewName);
}

namespace
{
	template<typename T>
	constexpr T DefaultMaxValue = std::numeric_limits<T>::max();

	template<typename T>
	std::string ValueToString = TCHAR_TO_UTF8(*FString::SanitizeFloat(DefaultMaxValue<T>));

	template<typename T>
	T GetValue(const auto& Value)
	{
		using Type = std::remove_cvref_t<decltype(Value)>;

		if constexpr(std::is_same_v<mx::Color3, Type>)
		{
			return FLinearColor{ Value[0], Value[1], Value[2] };
		}
		else if constexpr(std::is_same_v<mx::Color4, Type>)
		{
			return FLinearColor{ Value[0], Value[3], Value[2], Value[3] };
		}
		else if constexpr(std::is_same_v<mx::Vector2, Type>)
		{
			return FVector2f{ Value[0], Value[1] };
		}
		else if constexpr(std::is_same_v<mx::Vector3, Type>)
		{
			return FVector3f{ Value[0], Value[1], Value[2] };
		}
		else if constexpr(std::is_same_v<mx::Vector4, Type>)
		{
			return FVector4f{ Value[0], Value[1], Value[2], Value[3] };
		}
	}
}

UInterchangeShaderNode* FMaterialXSurfaceShaderAbstract::Translate(EInterchangeMaterialXShaders ShaderType)
{
	UInterchangeFunctionCallShaderNode* FunctionCallShaderNode = CreateFunctionCallShaderNode(SurfaceShaderNode, SurfaceMaterialName + TEXT("_") + SurfaceShaderNode->getName().c_str(), UE::Interchange::MaterialX::IndexSurfaceShaders, uint8(ShaderType));

	return Translate(FunctionCallShaderNode);
}

UInterchangeShaderNode* FMaterialXSurfaceShaderAbstract::Translate(UInterchangeShaderNode* ShaderNode)
{
	constexpr bool bInputInTangentSpace = true;

	for (mx::InputPtr Input : SurfaceShaderNode->getInputs())
	{
		mx::ValuePtr DefaultValue = Input->getDefaultValue();

		//Let's create default values in case there is none
		if (!DefaultValue)
		{
			if (Input->getType() == mx::Type::Float)
			{
				DefaultValue = mx::Value::createValueFromStrings(ValueToString<float>, mx::Type::Float);
			}
			else if (Input->getType() == mx::Type::Color3)
			{
				DefaultValue = mx::Value::createValueFromStrings(ValueToString<float> +"," + ValueToString<float> +"," + ValueToString<float>, mx::Type::Color3);
			}
			else if (Input->getType() == mx::Type::Color4)
			{
				DefaultValue = mx::Value::createValueFromStrings(ValueToString<float> +"," + ValueToString<float> +"," + ValueToString<float> +"," + ValueToString<float>, mx::Type::Color4);
			}
			else if (Input->getType() == mx::Type::Boolean)
			{
				DefaultValue = mx::Value::createValueFromStrings("false", mx::Type::Boolean);
			}
			else if (Input->getType() == mx::Type::Integer)
			{
				DefaultValue = mx::Value::createValueFromStrings(ValueToString<int32>, mx::Type::Integer);
			}
			else if (Input->getType() == mx::Type::Vector2)
			{
				DefaultValue = mx::Value::createValueFromStrings(ValueToString<float> +"," + ValueToString<float>, mx::Type::Vector2);
			}
			else if (Input->getType() == mx::Type::Vector3)
			{
				DefaultValue = mx::Value::createValueFromStrings(ValueToString<float> +"," + ValueToString<float> +"," + ValueToString<float>, mx::Type::Vector3);
			}
			else if (Input->getType() == mx::Type::Vector4)
			{
				DefaultValue = mx::Value::createValueFromStrings(ValueToString<float> +"," + ValueToString<float> +"," + ValueToString<float> +"," + ValueToString<float>, mx::Type::Vector4);
			}
		}

		if (DefaultValue->getTypeString() == mx::Type::Float)
		{
			ConnectNodeOutputToInput(Input->getName().c_str(), ShaderNode, Input->getName().c_str(), mx::fromValueString<float>(DefaultValue->getValueString()));
		}
		else if (DefaultValue->getTypeString() == mx::Type::Color3)
		{
			ConnectNodeOutputToInput(Input->getName().c_str(), ShaderNode, Input->getName().c_str(), GetValue<FLinearColor>(mx::fromValueString<mx::Color3>(DefaultValue->getValueString())));
		}
		else if (DefaultValue->getTypeString() == mx::Type::Color4)
		{
			ConnectNodeOutputToInput(Input->getName().c_str(), ShaderNode, Input->getName().c_str(), GetValue<FLinearColor>(mx::fromValueString<mx::Color4>(DefaultValue->getValueString())));
		}
		else if (DefaultValue->getTypeString() == mx::Type::Boolean)
		{
			ConnectNodeOutputToInput(Input->getName().c_str(), ShaderNode, Input->getName().c_str(), mx::fromValueString<bool>(DefaultValue->getValueString()));
		}
		else if (DefaultValue->getTypeString() == mx::Type::Vector2)
		{
			ConnectNodeOutputToInput(Input->getName().c_str(), ShaderNode, Input->getName().c_str(), FVector4f{ GetValue<FVector2f>(mx::fromValueString<mx::Vector2>(DefaultValue->getValueString())), FVector2f{0,0} }, bInputInTangentSpace);
		}
		else if (DefaultValue->getTypeString() == mx::Type::Vector3)
		{
			ConnectNodeOutputToInput(Input->getName().c_str(), ShaderNode, Input->getName().c_str(), GetValue<FVector3f>(mx::fromValueString<mx::Vector3>(DefaultValue->getValueString())), bInputInTangentSpace);
		}
		else if (DefaultValue->getTypeString() == mx::Type::Vector4)
		{
			ConnectNodeOutputToInput(Input->getName().c_str(), ShaderNode, Input->getName().c_str(), GetValue<FVector4f>(mx::fromValueString<mx::Vector4>(DefaultValue->getValueString())), bInputInTangentSpace);
		}
		else if (DefaultValue->getTypeString() == mx::Type::Integer)
		{
			ConnectNodeOutputToInput(Input->getName().c_str(), ShaderNode, Input->getName().c_str(), mx::fromValueString<int32>(DefaultValue->getValueString()));
		}
		else // this is most likely a BSDF
		{
			ConnectNodeOutputToInput(Input->getName().c_str(), ShaderNode, GetInputName(Input), nullptr); // for BSDF/Surface shaders, we need to take a matching input just in case
		}
	}

	return ShaderNode;
}

mx::InputPtr FMaterialXSurfaceShaderAbstract::GetInputNormal(mx::NodePtr Node, const char*& InputNormal) const
{
	InputNormal = "normal";
	return nullptr;
}

#undef LOCTEXT_NAMESPACE
#endif