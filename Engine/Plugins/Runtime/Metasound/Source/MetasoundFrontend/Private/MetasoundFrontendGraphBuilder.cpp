// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundFrontendGraphBuilder.h"

#include "MetasoundFrontendGraph.h"
#include "Algo/AllOf.h"
#include "Algo/AnyOf.h"
#include "Algo/TopologicalSort.h"
#include "Algo/Transform.h"
#include "CoreMinimal.h"
#include "CoreGlobals.h"
#include "MetasoundDocumentInterface.h"
#include "MetasoundFrontend.h"
#include "MetasoundFrontendDataTypeRegistry.h"
#include "MetasoundFrontendNodeTemplateRegistry.h"
#include "MetasoundFrontendProxyDataCache.h"
#include "MetasoundFrontendRegistries.h"
#include "MetasoundGraph.h"
#include "MetasoundGraphNode.h"
#include "MetasoundLiteralNode.h"
#include "MetasoundLog.h"
#include "MetasoundNodeInterface.h"

namespace Metasound::Frontend
{
	namespace GraphBuilderPrivate
	{
		// Map of Input VertexID to variable data required to construct and connect default variable
		using FNodeIDVertexID = TTuple<FGuid, FGuid>;
		using FDependencyByIDMap = TMap<FGuid, const FMetasoundFrontendClass*>;
		using FSharedNodeByIDMap = TMap<FGuid, TSharedPtr<const IGraph>>;

		// Context used throughout entire graph build process
		// (for both a root and nested subgraphs)
		struct FBuildContext
		{
			const FString& DebugAssetName;
			const Frontend::IDataTypeRegistry& DataTypeRegistry;
			const Frontend::FProxyDataCache* ProxyDataCache;
		};

		// Context related to the document being built. 
		struct FBuildDocumentContext
		{
			FDependencyByIDMap FrontendClasses;
			FSharedNodeByIDMap Graphs;
		};

		// Transient context used for building a specific graph
		struct FBuildGraphContext
		{
			TUniquePtr<FFrontendGraph> Graph;
			const FMetasoundFrontendGraphClass& GraphClass;
			const FMetasoundFrontendGraph& PagedGraph;
			const FBuildContext& BuildContext;
			const FBuildDocumentContext& BuildDocumentContext;
		};

		template <typename ResolveType>
		FGuid ResolveTargetPageID(const ResolveType& InToResolve)
		{
			// Registry is not available in tests, so for now resolution is considered successful at this level
			// if registry is not initialized and providing a resolved page ID. TODO: Add a test implementation
			// that returns the default page (or whatever page behavior is desired for testing).
			if (IDocumentBuilderRegistry* BuilderRegistry = IDocumentBuilderRegistry::Get())
			{
				return IDocumentBuilderRegistry::GetChecked().ResolveTargetPageID(InToResolve);
			}

			return Frontend::DefaultPageID;
		}

		bool InterfacesHaveEqualSize(const FMetasoundFrontendClassInterface& InClassInterface, const FMetasoundFrontendNodeInterface& InNodeInterface)
		{
			return (InClassInterface.Inputs.Num() == InNodeInterface.Inputs.Num()) && (InClassInterface.Outputs.Num() == InNodeInterface.Outputs.Num());
		}

		const FMetasoundFrontendLiteral* FindLiteralForInputVertex(const FVertexName& InVertexName, const FMetasoundFrontendNode& InNode, const FMetasoundFrontendClassInput& InNodeClassInput)
		{
			using namespace Frontend;

			// Default value priority is:
			// 1. A value set directly on the node
			// 2. A default value of the owning graph
			// 3. A default value on the input node class.

			const FMetasoundFrontendLiteral* Literal = nullptr;

			// Check for default value directly on node.
			if (InNode.InputLiterals.Num())
			{
				const FMetasoundFrontendVertex* InputVertex = InNode.Interface.Inputs.FindByPredicate([&InVertexName](const FMetasoundFrontendVertex& Vertex) { return Vertex.Name == InVertexName; });
				if (ensure(InputVertex))
				{
					const FMetasoundFrontendVertexLiteral* VertexLiteral = InNode.InputLiterals.FindByPredicate([&InputVertex](const FMetasoundFrontendVertexLiteral& InVertexLiteral) { return InVertexLiteral.VertexID == InputVertex->VertexID; });
					if (VertexLiteral)
					{
						Literal = &VertexLiteral->Value;
					}
				}
			}

			// Check for default value on node class
			if (nullptr == Literal)
			{
				const FGuid PageID = GraphBuilderPrivate::ResolveTargetPageID(InNodeClassInput);
				const FMetasoundFrontendLiteral* DefaultLiteral = InNodeClassInput.FindConstDefault(PageID);
				if (ensure(DefaultLiteral))
				{
					if (DefaultLiteral->IsValid())
					{
						Literal = DefaultLiteral;
					}
				}
			}

			return Literal;
		}

		const FMetasoundFrontendLiteral* FindInputLiteralForInputNode(const FMetasoundFrontendNode& InInputNode, const FMetasoundFrontendClassInterface& InInputNodeClassInterface, const FMetasoundFrontendClassInput& InOwningGraphClassInput)
		{
			using namespace Frontend;

			// Default value priority is:
			// 1. A value set directly on the node
			// 2. A default value of the owning graph
			// 3. A default value on the input node class.

			const FMetasoundFrontendLiteral* Literal = nullptr;

			// Check for default value directly on node.
			if (ensure(InInputNode.Interface.Inputs.Num() == 1))
			{
				const FMetasoundFrontendVertex& InputVertex = InInputNode.Interface.Inputs[0];

				// Find input literal matching VertexID
				const FMetasoundFrontendVertexLiteral* VertexLiteral = InInputNode.InputLiterals.FindByPredicate(
					[&](const FMetasoundFrontendVertexLiteral& InVertexLiteral)
					{
						return InVertexLiteral.VertexID == InputVertex.VertexID;
					}
				);

				if (nullptr != VertexLiteral)
				{
					Literal = &VertexLiteral->Value;
				}
			}

			// Check for default value on owning graph.
			if (nullptr == Literal)
			{
				// Find Class Default that is not invalid
				const FGuid PageID = GraphBuilderPrivate::ResolveTargetPageID(InOwningGraphClassInput);
				const FMetasoundFrontendLiteral& DefaultLiteral = InOwningGraphClassInput.FindConstDefaultChecked(PageID);
				if (DefaultLiteral.IsValid())
				{
					Literal = &DefaultLiteral;
				}
			}

			// Check for default value on input node class
			if (nullptr == Literal && ensure(InInputNodeClassInterface.Inputs.Num() == 1))
			{
				const FMetasoundFrontendClassInput& InputNodeClassInput = InInputNodeClassInterface.Inputs.Last();
				const FGuid PageID = GraphBuilderPrivate::ResolveTargetPageID(InputNodeClassInput);
				const FMetasoundFrontendLiteral& DefaultLiteral = InputNodeClassInput.FindConstDefaultChecked(PageID);
				if (DefaultLiteral.IsValid())
				{
					Literal = &DefaultLiteral;
				}
			}

			return Literal;
		}


		FEnvironmentVertexInterface CreateEnvironmentVertexInterface(const FMetasoundFrontendNodeInterface& InNodeInterface)
		{
			if (InNodeInterface.Environment.Num())
			{
				auto Convert = [](const FMetasoundFrontendVertex& InFrontendVertex) -> FEnvironmentVertex 
				{
					return FEnvironmentVertex{InFrontendVertex.Name, FText::GetEmpty()};
				};

				TArray<FEnvironmentVertex> Vertices;
				Vertices.Reserve(InNodeInterface.Environment.Num());
				Algo::Transform(InNodeInterface.Environment, Vertices, Convert);

				return FEnvironmentVertexInterface{MoveTemp(Vertices)};
			}
			else
			{
				return FEnvironmentVertexInterface{};
			}
		}

		FOutputVertexInterface CreateOutputVertexInterface(const FBuildContext& InContext, const FMetasoundFrontendNode& InNode, const FMetasoundFrontendClass& InClass, const FMetasoundFrontendClassInterface& InClassInterface, const FMetasoundFrontendClassInput* InOwningGraphClassInput, const FMetasoundFrontendClassOutput* InOwningGraphClassOutput)
		{
			check(InClassInterface.Outputs.Num() == InNode.Interface.Outputs.Num());

			// TODO: migrate sub interface builders from MetasoundVertex.cpp when
			// sub interfaces exist on the frontend document
			if (InNode.Interface.Outputs.Num())
			{
				TArray<FOutputDataVertex> Vertices;
				Vertices.Reserve(InNode.Interface.Outputs.Num());

				switch (InClass.Metadata.GetType())
				{
					// The vertices on an input/output node do not have matching
					// names with the vertices on input/output node classes. The
					// node class vertex names are blank, and the input/output node
					// vertex names match the name of the input or output. Here 
					// we have to grab data from both the class and node interfaces
					case EMetasoundFrontendClassType::Input:
						{
							if (ensureMsgf(InOwningGraphClassInput, TEXT("Input nodes require the owning graph class input in order to retrieve the access type")))
							{
								if (InNode.Interface.Outputs.Num() == 1)
								{
									const FMetasoundFrontendVertex& NodeOutputVertex = InNode.Interface.Outputs[0];
									const FMetasoundFrontendClassOutput& ClassOutputVertex = InClassInterface.Outputs[0];
									// Note: stripping metadata here. If we make this function public, we may want to keep metadata if WITH_EDITORONLY_DATA
									Vertices.Add(FOutputDataVertex{
												NodeOutputVertex.Name, 
												NodeOutputVertex.TypeName, 
												FDataVertexMetadata{}, 
												// NOTE: The access type reported on the ClassInputVertex may be incorrect. When the access type is changed in the editor,
												// the access type of the input node's FMetasoundFrontendClass is not updated. 
												FrontendVertexAccessTypeToCoreVertexAccessType(InOwningGraphClassInput->AccessType)
											});
								}
								else
								{
									// This is an edge case in the off scenario that someone
									// is trying to build a graph that used the `TInputNode<FStereoAudio>` 
									// specialization.  Auto-update should have removed these
									// long ago, but this log will help identify any issues.
									// This log is added in UE 5.6 and if no one is hitting
									// this issue, can be removed in UE 5.7.
									UE_LOG(LogMetaSound, Error, TEXT("The MetaSound %s has an malformed input node %s. Please replace the input node and resave"), *InContext.DebugAssetName, *InNode.Name.ToString());
								}
							}
						}
						break;

					case EMetasoundFrontendClassType::Output:
						{
							if (ensureMsgf(InOwningGraphClassOutput, TEXT("Output nodes require the owning graph class output in order to retrieve the access type")))
							{
								check(InNode.Interface.Outputs.Num() == 1);
								const FMetasoundFrontendVertex& NodeOutputVertex = InNode.Interface.Outputs[0];
								const FMetasoundFrontendClassOutput& ClassOutputVertex = InClassInterface.Outputs[0];
								// Note: stripping metadata here. If we make this function public, we may want to keep metadata if WITH_EDITORONLY_DATA
								Vertices.Add(FOutputDataVertex{
											NodeOutputVertex.Name, 
											NodeOutputVertex.TypeName, 
											FDataVertexMetadata{}, 
											// NOTE: The access type reported on the ClassInputVertex may be incorrect. When the access type is changed in the editor,
											// the access type of the input node's FMetasoundFrontendClass is not updated. 
											FrontendVertexAccessTypeToCoreVertexAccessType(InOwningGraphClassOutput->AccessType)
										});
							}
						}
						break;

					default:
						{
							// As-it-stands, node interfaces and class interfaces
							// match for all other nodes. This will change when
							// node config is done as a feature. 
							for (const FMetasoundFrontendClassOutput& ClassOutput : InClassInterface.Outputs)
							{
								// Note: stripping metadata here. If we make this function public, we may want to keep metadata if WITH_EDITORONLY_DATA
								Vertices.Add(FOutputDataVertex{ClassOutput.Name, ClassOutput.TypeName, FDataVertexMetadata{}, FrontendVertexAccessTypeToCoreVertexAccessType(ClassOutput.AccessType)});
							}
						}
						break;
				}

				return FOutputVertexInterface{MoveTemp(Vertices), TArray<VertexPrivate::FSubInterfaceLayout>()};
			}
			else
			{
				return FOutputVertexInterface{};
			}
		}

		FInputVertexInterface CreateInputVertexInterface(const FBuildContext& InContext, const FMetasoundFrontendNode& InNode, const FMetasoundFrontendClass& InClass, const FMetasoundFrontendClassInterface& InClassInterface, const FMetasoundFrontendClassInput* InOwningGraphClassInput, const FMetasoundFrontendClassOutput* InOwningGraphClassOutput)
		{
			check(InClassInterface.Inputs.Num() == InNode.Interface.Inputs.Num());

			// TODO: migrate sub interface builders from MetasoundVertex.cpp when
			// sub interfaces exist on the frontend document
			if (InNode.Interface.Inputs.Num())
			{
				auto MakeVertex = [&InContext](const FVertexName& Name, const FName& DataTypeName, EMetasoundFrontendVertexAccessType InFrontendAccessType, const FMetasoundFrontendLiteral* InFrontendLiteral) -> FInputDataVertex
				{
					EVertexAccessType AccessType = FrontendVertexAccessTypeToCoreVertexAccessType(InFrontendAccessType);
					// Note: stripping metadata here. If we make this function public, we may want to keep metadata if WITH_EDITORONLY_DATA
					if (InFrontendLiteral)
					{
						FLiteral CoreLiteral = InFrontendLiteral->ToLiteral(DataTypeName, &InContext.DataTypeRegistry, InContext.ProxyDataCache);
						return FInputDataVertex{Name, DataTypeName, FDataVertexMetadata{}, AccessType, MoveTemp(CoreLiteral)};
					}
					else
					{
						return FInputDataVertex{Name, DataTypeName, FDataVertexMetadata{}, AccessType};
					}
				};

				TArray<FInputDataVertex> Vertices;
				Vertices.Reserve(InNode.Interface.Inputs.Num());

				switch (InClass.Metadata.GetType())
				{
					// The vertices on an input/output node do not have matching
					// names with the vertices on input/output node classes. The
					// node class vertex names are blank, and the input/output node
					// vertex names match the name of the input or output. Here 
					// we have to grab data from both the class and node interfaces
					case EMetasoundFrontendClassType::Input:
						{
							if (ensureMsgf(InOwningGraphClassInput, TEXT("Input nodes require the owning graph class input in order to retrieve the default literal and access type")))
							{
								check(InNode.Interface.Inputs.Num() == 1);
								const FMetasoundFrontendVertex& NodeInputVertex = InNode.Interface.Inputs[0];
								const FMetasoundFrontendClassInput& ClassInputVertex = InClassInterface.Inputs[0];
								const FMetasoundFrontendLiteral* FrontendLiteral = FindInputLiteralForInputNode(InNode, InClassInterface, *InOwningGraphClassInput);
								
								// NOTE: The access type reported on the ClassInputVertex may be incorrect. When the access type is changed in the editor,
								// the access type of the input node's FMetasoundFrontendClass is not updated. 
								Vertices.Add(MakeVertex(NodeInputVertex.Name, NodeInputVertex.TypeName, InOwningGraphClassInput->AccessType, FrontendLiteral));
							}
						}
						break;
					case EMetasoundFrontendClassType::Output:
						{
							if (ensureMsgf(InOwningGraphClassOutput, TEXT("Output nodes require the owning graph class output in order to retrieve the access type")))
							{
								if (InNode.Interface.Inputs.Num() == 1)
								{
									const FMetasoundFrontendVertex& NodeInputVertex = InNode.Interface.Inputs[0];
									const FMetasoundFrontendClassInput& ClassInputVertex = InClassInterface.Inputs[0];
									const FMetasoundFrontendLiteral* FrontendLiteral = FindLiteralForInputVertex(NodeInputVertex.Name, InNode, ClassInputVertex);
									// NOTE: The access type reported on the ClassInputVertex may be incorrect. When the access type is changed in the editor,
									// the access type of the output node's FMetasoundFrontendClass is not updated. 
									Vertices.Add(MakeVertex(NodeInputVertex.Name, NodeInputVertex.TypeName, InOwningGraphClassOutput->AccessType, FrontendLiteral));
								}
								else
								{
									// This is an edge case in the off scenario that someone
									// is trying to build a graph that used the `TOutputNode<FStereoAudio>` 
									// specialization.  Auto-update should have removed these
									// long ago, but this log will help identify any issues.
									// This log is added in UE 5.6 and if no one is hitting
									// this issue, can be removed in UE 5.7.
									UE_LOG(LogMetaSound, Error, TEXT("The MetaSound %s has an malformed output node %s. Please replace the output node and resave"), *InContext.DebugAssetName, *InNode.Name.ToString());
								}
							}
						}
						break;

					default:
						{
							// As-it-stands, node interfaces and class interfaces
							// match for all other nodes. This will change when
							// node config is done as a feature. 
							for (const FMetasoundFrontendClassInput& ClassInput : InClassInterface.Inputs)
							{
								const FMetasoundFrontendLiteral* FrontendLiteral = FindLiteralForInputVertex(ClassInput.Name, InNode, ClassInput);
								Vertices.Add(MakeVertex(ClassInput.Name, ClassInput.TypeName, ClassInput.AccessType, FrontendLiteral));
							}
						}
						break;
				}

				return FInputVertexInterface{MoveTemp(Vertices), TArray<VertexPrivate::FSubInterfaceLayout>()};
			}
			else
			{
				return FInputVertexInterface{};
			}
		}

		FVertexInterface CreateVertexInterface(const FBuildContext& InContext, const FMetasoundFrontendNode& InNode, const FMetasoundFrontendClass& InClass, const FMetasoundFrontendClassInterface& InClassInterface, const FMetasoundFrontendClassInput* InOwningGraphClassInput, const FMetasoundFrontendClassOutput* InOwningGraphClassOutput)
		{
			return FVertexInterface(
				CreateInputVertexInterface(InContext, InNode, InClass, InClassInterface, InOwningGraphClassInput, InOwningGraphClassOutput), 
				CreateOutputVertexInterface(InContext, InNode, InClass, InClassInterface, InOwningGraphClassInput, InOwningGraphClassOutput), 
				CreateEnvironmentVertexInterface(InNode.Interface));
		}

		const FName& GetNodeName(const FMetasoundFrontendNode& InNode, const FMetasoundFrontendClassInput* InOwningGraphClassInput=nullptr, const FMetasoundFrontendClassOutput* InOwningGraphClassOutput=nullptr)
		{
			// Make sure the node's name is correct when it is an input or output node. 
			// The node name is used when adding the node to the FGraph as an input.
			if (InOwningGraphClassInput)
			{
				return InOwningGraphClassInput->Name;
			}
			else if (InOwningGraphClassOutput)
			{
				return InOwningGraphClassOutput->Name;
			}
			else
			{
				return InNode.Name;
			}
		}

		FNodeData CreateNodeData(const FBuildContext& InContext, const FMetasoundFrontendNode& InNode, const FMetasoundFrontendClass& InClass, const FMetasoundFrontendClassInterface& InClassInterface, const FMetasoundFrontendClassInput* InOwningGraphClassInput=nullptr, const FMetasoundFrontendClassOutput* InOwningGraphClassOutput=nullptr)
		{
			TSharedPtr<const IOperatorData> Config;
			if (InNode.Configuration.IsValid())
			{
				Config = InNode.Configuration.Get().GetOperatorData();
			}

			const FName& NodeName = GetNodeName(InNode, InOwningGraphClassInput, InOwningGraphClassOutput);

			return FNodeData(NodeName, InNode.GetID(), CreateVertexInterface(InContext, InNode, InClass, InClassInterface, InOwningGraphClassInput, InOwningGraphClassOutput), MoveTemp(Config));
		}

		const FMetasoundFrontendVariable* FindVariableForVariableNode(const FMetasoundFrontendNode& InVariableNode, const FMetasoundFrontendGraph& InGraph)
		{
			const FGuid& DesiredID = InVariableNode.GetID();
			return InGraph.Variables.FindByPredicate([&](const FMetasoundFrontendVariable& InVar) { return InVar.VariableNodeID == DesiredID; });
		}

		TUniquePtr<INode> CreateVariableNode(const FBuildContext& InContext, const FMetasoundFrontendNode& InNode, const FMetasoundFrontendClass& InClass, const FMetasoundFrontendClassInterface& InClassInterface, const FMetasoundFrontendGraph& InGraph)
		{
			check(InClass.Metadata.GetType() == EMetasoundFrontendClassType::Variable);
			check(InNode.ClassID == InClass.ID);
			check(InterfacesHaveEqualSize(InClassInterface, InNode.Interface));

			// Find the variable object associated with the node.
			const FMetasoundFrontendVariable* FrontendVariable = FindVariableForVariableNode(InNode, InGraph);

			if (nullptr != FrontendVariable)
			{
				const IDataTypeRegistry& DataTypeRegistry = InContext.DataTypeRegistry;
				const bool IsLiteralParsableByDataType = DataTypeRegistry.IsLiteralTypeSupported(FrontendVariable->TypeName, FrontendVariable->Literal.GetType());

				if (IsLiteralParsableByDataType)
				{
					FLiteral Literal = FrontendVariable->Literal.ToLiteral(FrontendVariable->TypeName, &InContext.DataTypeRegistry, InContext.ProxyDataCache);
					return DataTypeRegistry.CreateVariableNode(FrontendVariable->TypeName, MoveTemp(Literal), CreateNodeData(InContext, InNode, InClass, InClassInterface));
				}
				else
				{
					UE_LOG(LogMetaSound, Error, TEXT("Cannot create variable node [NodeID:%s]. [Variable:%s] cannot be constructed with the provided literal type."), *InNode.GetID().ToString(), *FrontendVariable->Name.ToString());
				}
			}
			else
			{
				UE_LOG(LogMetaSound, Error, TEXT("Cannot create variable node [NodeID:%s]. No variable found for variable node."), *InNode.GetID().ToString());
			}

			return TUniquePtr<INode>(nullptr);
		}


		TUniquePtr<INode> CreateInputNode(const FBuildContext& InContext, const FMetasoundFrontendNode& InNode, const FMetasoundFrontendClass& InClass, const FMetasoundFrontendClassInterface& InClassInterface, const FMetasoundFrontendClassInput& InOwningGraphClassInput)
		{
			check(InClass.Metadata.GetType() == EMetasoundFrontendClassType::Input);
			check(InNode.ClassID == InClass.ID);
			check(InterfacesHaveEqualSize(InClassInterface, InNode.Interface));

			// Check that the frontend node and class is correct.
			if (InNode.Interface.Inputs.Num() != 1)
			{
				UE_LOG(LogMetaSound, Error, TEXT("MetaSound %s contains invalid number of inputs (%d) on input node %s"), *InContext.DebugAssetName, InNode.Interface.Inputs.Num(), *InNode.Name.ToString());
				return TUniquePtr<INode>(nullptr);
			}

			// Create the input node
			const FMetasoundFrontendVertex& InputVertex = InNode.Interface.Inputs[0];

			if (InOwningGraphClassInput.AccessType == EMetasoundFrontendVertexAccessType::Reference)
			{
				return InContext.DataTypeRegistry.CreateInputNode(InputVertex.TypeName, CreateNodeData(InContext, InNode, InClass, InClassInterface, &InOwningGraphClassInput));
			}
			else if (InOwningGraphClassInput.AccessType == EMetasoundFrontendVertexAccessType::Value)
			{
				return InContext.DataTypeRegistry.CreateConstructorInputNode(InputVertex.TypeName, CreateNodeData(InContext, InNode, InClass, InClassInterface, &InOwningGraphClassInput));
			}
			else
			{
				UE_LOG(LogMetaSound, Error, TEXT("MetaSound %s contains invalid input access type on input %s"), *InContext.DebugAssetName, *InNode.Name.ToString());
			}

			return TUniquePtr<INode>(nullptr);
		}

		TUniquePtr<INode> CreateOutputNode(const FBuildContext& InContext, const FMetasoundFrontendNode& InNode, const FMetasoundFrontendClass& InClass, const FMetasoundFrontendClassInterface& InClassInterface, const FMetasoundFrontendClassOutput& InOwningGraphClassOutput)
		{
			check(InClass.Metadata.GetType() == EMetasoundFrontendClassType::Output);
			check(InNode.ClassID == InClass.ID);
			check(InterfacesHaveEqualSize(InClassInterface, InNode.Interface));

			if (InNode.Interface.Outputs.Num() != 1)
			{
				UE_LOG(LogMetaSound, Error, TEXT("MetaSound %s contains invalid number of outputs (%d) on output node %s"), *InContext.DebugAssetName, InNode.Interface.Outputs.Num(), *InNode.Name.ToString());
				return TUniquePtr<INode>(nullptr);
			}

			const FMetasoundFrontendVertex& OutputVertex = InNode.Interface.Outputs[0];

			if (InClassInterface.Outputs[0].AccessType == EMetasoundFrontendVertexAccessType::Reference)
			{
				return InContext.DataTypeRegistry.CreateOutputNode(OutputVertex.TypeName, CreateNodeData(InContext, InNode, InClass, InClassInterface, nullptr, &InOwningGraphClassOutput));
			}
			else if (InClassInterface.Outputs[0].AccessType == EMetasoundFrontendVertexAccessType::Value)
			{
				return InContext.DataTypeRegistry.CreateConstructorOutputNode(OutputVertex.TypeName, CreateNodeData(InContext, InNode, InClass, InClassInterface, nullptr, &InOwningGraphClassOutput));
			}
			else
			{
				UE_LOG(LogMetaSound, Error, TEXT("MetaSound %s contains invalid output access type on output %s"), *InContext.DebugAssetName, *InNode.Name.ToString());
			}

			return TUniquePtr<INode>(nullptr);
		}

		TUniquePtr<INode> CreateExternalNode(const FBuildContext& InContext, const FMetasoundFrontendNode& InNode, const FMetasoundFrontendClass& InClass, const FMetasoundFrontendClassInterface& InClassInterface)
		{
			using namespace Frontend;

			check(InNode.ClassID == InClass.ID);
			check(InterfacesHaveEqualSize(InClassInterface, InNode.Interface));

			const FNodeRegistryKey Key = FNodeRegistryKey(InClass.Metadata);
			return INodeClassRegistry::Get()->CreateNode(Key, CreateNodeData(InContext, InNode, InClass, InClassInterface));
		}

		TUniquePtr<INode> CreateSubgraphNode(const FBuildContext& InContext, const TSharedRef<const IGraph>& InSubgraph, const FMetasoundFrontendNode& InNode, const FMetasoundFrontendClass& InClass, const FMetasoundFrontendClassInterface& InClassInterface)
		{
			using namespace Frontend;

			check(InNode.ClassID == InClass.ID);
			check(InterfacesHaveEqualSize(InClassInterface, InNode.Interface));

			return MakeUnique<FGraphNode>(CreateNodeData(InContext, InNode, InClass, InClassInterface), InSubgraph);
		}

		const FMetasoundFrontendClassInput* FindClassInputForInputNode(const FMetasoundFrontendGraphClass& InOwningGraph, const FMetasoundFrontendNode& InInputNode)
		{
			// Input nodes should have exactly one input.
			if (ensure(InInputNode.Interface.Inputs.Num() == 1))
			{
				const FName& TypeName = InInputNode.Interface.Inputs[0].TypeName;

				auto IsMatchingInput = [&](const FMetasoundFrontendClassInput& GraphInput)
				{
					return (InInputNode.GetID() == GraphInput.NodeID);
				};

				return InOwningGraph.GetDefaultInterface().Inputs.FindByPredicate(IsMatchingInput);
			}
			return nullptr;
		}

		const FMetasoundFrontendClassOutput* FindClassOutputForOutputNode(const FMetasoundFrontendGraphClass& InOwningGraph, const FMetasoundFrontendNode& InOutputNode)
		{
			// Output nodes should have exactly one output
			if (ensure(InOutputNode.Interface.Outputs.Num() == 1))
			{
				const FName& TypeName = InOutputNode.Interface.Outputs[0].TypeName;

				auto IsMatchingOutput = [&](const FMetasoundFrontendClassOutput& GraphOutput)
				{
					return (InOutputNode.GetID() == GraphOutput.NodeID);
				};

				return InOwningGraph.GetDefaultInterface().Outputs.FindByPredicate(IsMatchingOutput);
			}
			return nullptr;
		}


		void LogFailedToCreateNode(const FString& InAssetName, const FMetasoundFrontendNode& InNode, const FMetasoundFrontendClass& InNodeClass)
		{
			UE_LOG(LogMetaSound, Error, TEXT("Metasound '%s': Failed to create node [NodeID:%s, NodeName:%s] from registry [Class: %s %s]"), *InAssetName, *InNode.GetID().ToString(), *InNode.Name.ToString(), *InNodeClass.Metadata.GetClassName().ToString(), *InNodeClass.Metadata.GetVersion().ToString());
		}

		bool AddNodesToGraph(FBuildGraphContext& InGraphContext)
		{
			for (const FMetasoundFrontendNode& FrontendNode : InGraphContext.PagedGraph.Nodes)
			{
				const FMetasoundFrontendClass* FrontendNodeClass = InGraphContext.BuildDocumentContext.FrontendClasses.FindRef(FrontendNode.ClassID);

				if (ensure(nullptr != FrontendNodeClass))
				{
					const FGraphBuilder::FCreateNodeParams CreateNodeParams
					{
						InGraphContext.BuildContext.DebugAssetName,
						FrontendNode,
						*FrontendNodeClass,
						InGraphContext.PagedGraph,
						InGraphContext.GraphClass,
						InGraphContext.BuildContext.ProxyDataCache, 
						&InGraphContext.BuildContext.DataTypeRegistry,
						&InGraphContext.BuildDocumentContext.Graphs
					};
					TUniquePtr<INode> Node = FGraphBuilder::CreateNode(CreateNodeParams);

					if (!Node.IsValid())
					{
						LogFailedToCreateNode(InGraphContext.BuildContext.DebugAssetName, FrontendNode, *FrontendNodeClass);
						continue;
					}

					switch (FrontendNodeClass->Metadata.GetType())
					{
						case EMetasoundFrontendClassType::Input:
							{
								const FName& InputName = Node->GetInstanceName();
								InGraphContext.Graph->AddInputNode(FrontendNode.GetID(), InputName, MoveTemp(Node));
							}
						break;

						case EMetasoundFrontendClassType::Output:
							{
								const FName& OutputName = Node->GetInstanceName();
								InGraphContext.Graph->AddOutputNode(FrontendNode.GetID(), OutputName, MoveTemp(Node));
							}
						break;

						case EMetasoundFrontendClassType::Literal:
						case EMetasoundFrontendClassType::Graph:
						case EMetasoundFrontendClassType::Variable:
						case EMetasoundFrontendClassType::Template:
						case EMetasoundFrontendClassType::VariableAccessor:
						case EMetasoundFrontendClassType::VariableDeferredAccessor:
						case EMetasoundFrontendClassType::VariableMutator:
						case EMetasoundFrontendClassType::External:
						default:
							InGraphContext.Graph->AddNode(FrontendNode.GetID(), MoveTemp(Node));
						break;
					}
				}
			}

			return true;
		}

		bool AddEdgesToGraph(FBuildGraphContext& InGraphContext)
		{
			// Pair of frontend node and core node. The frontend node can be one of
			// several types.
			struct FCoreNodeAndFrontendVertex
			{
				const INode* Node = nullptr;
				const FMetasoundFrontendVertex* Vertex = nullptr;
			};

			TMap<FNodeIDVertexID, FCoreNodeAndFrontendVertex> NodeSourcesByID;
			TMap<FNodeIDVertexID, FCoreNodeAndFrontendVertex> NodeDestinationsByID;

			// Add nodes to NodeID/VertexID map
			for (const FMetasoundFrontendNode& Node : InGraphContext.PagedGraph.Nodes)
			{
				const INode* CoreNode = InGraphContext.Graph->FindNode(Node.GetID());
				if (nullptr == CoreNode)
				{
					UE_LOG(LogMetaSound, Warning, TEXT("Metasound '%s': Could not find referenced node [Name:%s, NodeID:%s]"), *InGraphContext.BuildContext.DebugAssetName, *Node.Name.ToString(), *Node.GetID().ToString());
					return false;
				}

				for (const FMetasoundFrontendVertex& Vertex : Node.Interface.Inputs)
				{
					NodeDestinationsByID.Add(FNodeIDVertexID(Node.GetID(), Vertex.VertexID), FCoreNodeAndFrontendVertex({CoreNode, &Vertex}));
				}

				for (const FMetasoundFrontendVertex& Vertex : Node.Interface.Outputs)
				{
					NodeSourcesByID.Add(FNodeIDVertexID(Node.GetID(), Vertex.VertexID), FCoreNodeAndFrontendVertex({CoreNode, &Vertex}));
				}
			};

			for (const FMetasoundFrontendEdge& Edge : InGraphContext.PagedGraph.Edges)
			{
				const FNodeIDVertexID DestinationKey(Edge.ToNodeID, Edge.ToVertexID);
				const FCoreNodeAndFrontendVertex* DestinationNodeAndVertex = NodeDestinationsByID.Find(DestinationKey);

				if (nullptr == DestinationNodeAndVertex)
				{
					UE_LOG(LogMetaSound, Error, TEXT("MetaSound '%s': Failed to add edge. Could not find destination [NodeID:%s, VertexID:%s]"), *InGraphContext.BuildContext.DebugAssetName, *Edge.ToNodeID.ToString(), *Edge.ToVertexID.ToString());
					return false;
				}

				if (nullptr == DestinationNodeAndVertex->Node)
				{
					UE_LOG(LogMetaSound, Warning, TEXT("MetaSound '%s': 'Failed to add edge. Null destination node [NodeID:%s]"), *InGraphContext.BuildContext.DebugAssetName, *Edge.ToNodeID.ToString());
					return false;
				}

				const FNodeIDVertexID SourceKey(Edge.FromNodeID, Edge.FromVertexID);
				const FCoreNodeAndFrontendVertex* SourceNodeAndVertex = NodeSourcesByID.Find(SourceKey);

				if (nullptr == SourceNodeAndVertex)
				{
					UE_LOG(LogMetaSound, Error, TEXT("MetaSound '%s': Failed to add edge. Could not find source [NodeID:%s, VertexID:%s]"), *InGraphContext.BuildContext.DebugAssetName, *Edge.FromNodeID.ToString(), *Edge.FromVertexID.ToString());
					return false;
				}

				if (nullptr == SourceNodeAndVertex->Node)
				{
					UE_LOG(LogMetaSound, Warning, TEXT("MetaSound '%s': Skipping edge. Null source node [NodeID:%s]"), *InGraphContext.BuildContext.DebugAssetName, *Edge.FromNodeID.ToString());
					return false;
				}

				const INode* FromNode = SourceNodeAndVertex->Node;
				const FVertexName FromVertexKey = SourceNodeAndVertex->Vertex->Name;

				const INode* ToNode = DestinationNodeAndVertex->Node;
				const FVertexName ToVertexKey = DestinationNodeAndVertex->Vertex->Name;

				bool bSuccess = InGraphContext.Graph->AddDataEdge(*FromNode, FromVertexKey,  *ToNode, ToVertexKey);

				// If succeeded, remove input as viable vertex to construct default variable, as it has been superceded by a connection.
				if (!bSuccess)
				{
					UE_LOG(LogMetaSound, Error, TEXT("MetaSound '%s': Failed to connect edge from [NodeID:%s, VertexID:%s] to [NodeID:%s, VertexID:%s]"), *InGraphContext.BuildContext.DebugAssetName, *Edge.FromNodeID.ToString(), *Edge.FromVertexID.ToString(), *Edge.ToNodeID.ToString(), *Edge.ToVertexID.ToString());
					return false;
				}
			}

			return true;
		}

		bool SortSubgraphDependencies(TArray<const FMetasoundFrontendGraphClass*>& Subgraphs)
		{
			// Helper for caching and querying subgraph dependencies
			struct FSubgraphDependencyLookup
			{
				FSubgraphDependencyLookup(TArrayView<const FMetasoundFrontendGraphClass*> InGraphs)
				{
					// Map ClassID to graph pointer. 
					TMap<FGuid, const FMetasoundFrontendGraphClass*> ClassIDAndGraph;
					for (const FMetasoundFrontendGraphClass* Graph: InGraphs)
					{
						ClassIDAndGraph.Add(Graph->ID, Graph);
					}

					// Cache subgraph dependencies.
					for (const FMetasoundFrontendGraphClass* GraphClass : InGraphs)
					{
						for (const FMetasoundFrontendNode& Node : GraphClass->GetConstDefaultGraph().Nodes)
						{
							if (ClassIDAndGraph.Contains(Node.ClassID))
							{
								DependencyMap.Add(GraphClass, ClassIDAndGraph[Node.ClassID]);
							}
						}
					}
				}

				TArray<const FMetasoundFrontendGraphClass*> operator()(const FMetasoundFrontendGraphClass* InParent) const
				{
					TArray<const FMetasoundFrontendGraphClass*> Dependencies;
					DependencyMap.MultiFind(InParent, Dependencies);
					return Dependencies;
				}

			private:

				TMultiMap<const FMetasoundFrontendGraphClass*, const FMetasoundFrontendGraphClass*> DependencyMap;
			};

			bool bSuccess = Algo::TopologicalSort(Subgraphs, FSubgraphDependencyLookup(Subgraphs));
			if (!bSuccess)
			{
				UE_LOG(LogMetaSound, Error, TEXT("Failed to topologically sort subgraphs. Possible recursive subgraph dependency"));
			}

			return bSuccess;
		}

		TUniquePtr<FFrontendGraph> CreateGraphInternal(FBuildContext& InContext, const FBuildDocumentContext& InDocumentContext, const FMetasoundFrontendGraphClass& InGraphClass, const FGuid& InGraphId)
		{
			const FString GraphName = InContext.DebugAssetName;

			const FGuid PageID = GraphBuilderPrivate::ResolveTargetPageID(InGraphClass);
			const FMetasoundFrontendGraph& PageGraph = InGraphClass.FindConstGraphChecked(PageID);
			FBuildGraphContext BuildGraphContext
			{
				MakeUnique<FFrontendGraph>(GraphName, InGraphId),
				InGraphClass,
				PageGraph,
				InContext,
				InDocumentContext
			};

			bool bSuccess = AddNodesToGraph(BuildGraphContext);

			if (bSuccess)
			{
				bSuccess = AddEdgesToGraph(BuildGraphContext);
			}

			if (bSuccess)
			{
				return MoveTemp(BuildGraphContext.Graph);
			}
			else
			{
				return TUniquePtr<FFrontendGraph>(nullptr);
			}
		}
	} // namespace GraphBuilderPrivate
	 
	TUniquePtr<INode> FGraphBuilder::CreateNode(const FCreateNodeParams& InParams)
	{
		using namespace GraphBuilderPrivate;

		checkf(InParams.ProxyDataCache != nullptr || IsInGameThread(), TEXT("An FProxyDataCache must be used if creating nodes on a thread other than the game thread"));


		TUniquePtr<INode> Node;

		const FMetasoundFrontendClassInterface& ClassInterface = InParams.FrontendNodeClass.GetInterfaceForNode(InParams.FrontendNode);

		if (ClassInterface.Inputs.Num() != InParams.FrontendNode.Interface.Inputs.Num())
		{
			UE_LOG(LogMetaSound, Error, TEXT("Cannot create node. MetaSound %s contains mismatched number of inputs (%d / %d) on node %s class %s"), *InParams.DebugAssetName, InParams.FrontendNode.Interface.Inputs.Num(), ClassInterface.Inputs.Num(), *InParams.FrontendNode.Name.ToString(), *InParams.FrontendNodeClass.Metadata.GetClassName().ToString());
			return Node;
		}
		else if (ClassInterface.Outputs.Num() != InParams.FrontendNode.Interface.Outputs.Num())
		{
			UE_LOG(LogMetaSound, Error, TEXT("Cannot create node. MetaSound %s contains mismatched number of outputs (%d / %d) on node %s class %s"), *InParams.DebugAssetName, InParams.FrontendNode.Interface.Outputs.Num(), ClassInterface.Outputs.Num(), *InParams.FrontendNode.Name.ToString(), *InParams.FrontendNodeClass.Metadata.GetClassName().ToString());
			return Node;
		}

		FBuildContext BuildContext
		{
			InParams.DebugAssetName,
			InParams.DataTypeRegistry ? *InParams.DataTypeRegistry : IDataTypeRegistry::Get(),
			InParams.ProxyDataCache
		};

		switch (InParams.FrontendNodeClass.Metadata.GetType())
		{
			case EMetasoundFrontendClassType::Input:
			{
				const FMetasoundFrontendClassInput* ClassInput = FindClassInputForInputNode(InParams.OwningFrontendGraphClass, InParams.FrontendNode);

				if (nullptr != ClassInput)
				{
					Node = CreateInputNode(BuildContext, InParams.FrontendNode, InParams.FrontendNodeClass, ClassInterface, *ClassInput);
				}
				else
				{
					const FString GraphClassIDString = InParams.OwningFrontendGraphClass.ID.ToString();
					UE_LOG(LogMetaSound, Error, TEXT("MetaSound '%s': Failed to match input node [NodeID:%s, NodeName:%s] to owning graph [ClassID:%s] input."), *InParams.DebugAssetName, *InParams.FrontendNode.GetID().ToString(), *InParams.FrontendNode.Name.ToString(), *GraphClassIDString);
				}
			}
			break;

			case EMetasoundFrontendClassType::Output:
			{
				const FMetasoundFrontendClassOutput* ClassOutput = FindClassOutputForOutputNode(InParams.OwningFrontendGraphClass, InParams.FrontendNode);
				if (nullptr != ClassOutput)
				{
					Node = CreateOutputNode(BuildContext, InParams.FrontendNode, InParams.FrontendNodeClass, ClassInterface, *ClassOutput);
				}
				else
				{
					const FString GraphClassIDString = InParams.OwningFrontendGraphClass.ID.ToString();
					UE_LOG(LogMetaSound, Error, TEXT("MetaSound '%s': Failed to match output node [NodeID:%s, NodeName:%s] to owning graph [ClassID:%s] output."), *InParams.DebugAssetName, *InParams.FrontendNode.GetID().ToString(), *InParams.FrontendNode.Name.ToString(), *GraphClassIDString);
				}
			}
			break;

			case EMetasoundFrontendClassType::Graph:
			{
				if (InParams.Subgraphs == nullptr)
				{
					UE_LOG(LogMetaSound, Error, TEXT("MetaSound '%s': Failed to find subgraph for node [NodeID:%s, NodeName:%s, ClassID:%s] because of missing subgraph map"), *InParams.DebugAssetName, *InParams.FrontendNode.GetID().ToString(), *InParams.FrontendNode.Name.ToString(), *InParams.FrontendNode.ClassID.ToString());

				}
				else
				{
					TSharedPtr<const IGraph> SubgraphPtr = InParams.Subgraphs->FindRef(InParams.FrontendNode.ClassID);
					if (SubgraphPtr.IsValid())
					{
						Node = CreateSubgraphNode(BuildContext, SubgraphPtr.ToSharedRef(), InParams.FrontendNode, InParams.FrontendNodeClass, ClassInterface);
					}
					else
					{
						UE_LOG(LogMetaSound, Error, TEXT("MetaSound '%s': Found invalid subgraph for node [NodeID:%s, NodeName:%s, ClassID:%s]"), *InParams.DebugAssetName, *InParams.FrontendNode.GetID().ToString(), *InParams.FrontendNode.Name.ToString(), *InParams.FrontendNode.ClassID.ToString());

					}
				}
			}
			break;

			case EMetasoundFrontendClassType::Literal:
			{
				UE_LOG(LogMetaSound, Warning, TEXT("MetaSound '%s': Adding literal nodes is being deprecated. Please set literal values on the connected node interface directly."), *InParams.DebugAssetName);
				const FName& DataTypeName = InParams.FrontendNodeClass.Metadata.GetClassName().Name;
PRAGMA_DISABLE_DEPRECATION_WARNINGS
				FDefaultLiteralNodeConstructorParams InitData { InParams.FrontendNode.Name, InParams.FrontendNode.GetID(), BuildContext.DataTypeRegistry.CreateDefaultLiteral(DataTypeName)};
				Node = BuildContext.DataTypeRegistry.CreateLiteralNode(DataTypeName, MoveTemp(InitData));
PRAGMA_ENABLE_DEPRECATION_WARNINGS
				break;
			}

			case EMetasoundFrontendClassType::Variable:
			{
				Node = CreateVariableNode(BuildContext, InParams.FrontendNode, InParams.FrontendNodeClass, ClassInterface, InParams.OwningFrontendGraph);
			}
			break;

			// Templates, variable accessors and mutators are
			// constructed with the same parameters as external nodes.
			case EMetasoundFrontendClassType::Template:
			case EMetasoundFrontendClassType::VariableAccessor:
			case EMetasoundFrontendClassType::VariableDeferredAccessor:
			case EMetasoundFrontendClassType::VariableMutator:
			case EMetasoundFrontendClassType::External:
			default:
			{
				Node = CreateExternalNode(BuildContext, InParams.FrontendNode, InParams.FrontendNodeClass, ClassInterface);
			}
			break;
		}

		return Node;
	}

	/** Check that all dependencies are C++ class dependencies. */
	bool FGraphBuilder::IsFlat(const FMetasoundFrontendDocument& InDocument)
	{
		if (InDocument.Subgraphs.Num() > 0)
		{
			return false;
		}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
		return IsFlat(InDocument.RootGraph, InDocument.Dependencies);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	bool FGraphBuilder::IsFlat(const FMetasoundFrontendGraphClass& InRoot, const TArray<FMetasoundFrontendClass>& InDependencies)
	{
		// All dependencies are external dependencies in a flat graph
		auto IsClassExternal = [&](const FMetasoundFrontendClass& InDesc) 
		{
			const bool bIsExternal = (InDesc.Metadata.GetType() == EMetasoundFrontendClassType::External) ||
				(InDesc.Metadata.GetType() == EMetasoundFrontendClassType::Template) ||
				(InDesc.Metadata.GetType() == EMetasoundFrontendClassType::Input) ||
				(InDesc.Metadata.GetType() == EMetasoundFrontendClassType::Output);
			return bIsExternal;
		};
		const bool bIsEveryDependencyExternal = Algo::AllOf(InDependencies, IsClassExternal);

		if (!bIsEveryDependencyExternal)
		{
			return false;
		}

		// All the dependencies are met 
		TSet<FGuid> AvailableDependencies;
		Algo::Transform(InDependencies, AvailableDependencies, [](const FMetasoundFrontendClass& InDesc) { return InDesc.ID; });

		auto IsDependencyMet = [&](const FMetasoundFrontendNode& InNode) 
		{ 
			return AvailableDependencies.Contains(InNode.ClassID);
		};

		const bool bIsEveryDependencyMet = Algo::AllOf(InRoot.GetConstDefaultGraph().Nodes, IsDependencyMet);

		return bIsEveryDependencyMet;
	}


	TUniquePtr<FFrontendGraph> FGraphBuilder::CreateGraph(const FMetasoundFrontendGraphClass& InGraph, const TArray<FMetasoundFrontendGraphClass>& InSubgraphs, const TArray<FMetasoundFrontendClass>& InDependencies, const Frontend::FProxyDataCache& InProxyDataCache, const FString& InDebugAssetName, const FGuid InGraphId)
	{
		using namespace GraphBuilderPrivate;

		FBuildContext Context
		{
			InDebugAssetName, 					// DebugAssetName
			Frontend::IDataTypeRegistry::Get(), // DataTypeRegistry
			&InProxyDataCache, 					// ProxyDataCache
		};

		FBuildDocumentContext DocumentContext;

		// Gather all references to node classes from external dependencies and subgraphs.
		for (const FMetasoundFrontendClass& ExtClass : InDependencies)
		{
			DocumentContext.FrontendClasses.Add(ExtClass.ID, &ExtClass);
		}
		for (const FMetasoundFrontendClass& ExtClass : InSubgraphs)
		{
			DocumentContext.FrontendClasses.Add(ExtClass.ID, &ExtClass);
		}

		// Sort subgraphs so that dependent subgraphs are created in correct order.
		TArray<const FMetasoundFrontendGraphClass*> FrontendSubgraphPtrs;
		Algo::Transform(InSubgraphs, FrontendSubgraphPtrs, [](const FMetasoundFrontendGraphClass& InClass) { return &InClass; });

		bool bSuccess = SortSubgraphDependencies(FrontendSubgraphPtrs);
		if (!bSuccess)
		{
			UE_LOG(LogMetaSound, Error, TEXT("Failed to create graph due to failed subgraph ordering in asset '%s'."), *InDebugAssetName);
			return TUniquePtr<FFrontendGraph>(nullptr);
		}

		// Create each subgraph.
		for (const FMetasoundFrontendGraphClass* FrontendSubgraphPtr : FrontendSubgraphPtrs)
		{
			TSharedPtr<const IGraph> Subgraph(CreateGraphInternal(Context, DocumentContext, *FrontendSubgraphPtr, FrontendSubgraphPtr->ID).Release());
			if (!Subgraph.IsValid())
			{
				UE_LOG(LogMetaSound, Warning, TEXT("Failed to create subgraph [SubgraphName: %s] in asset '%s'"), *FrontendSubgraphPtr->Metadata.GetClassName().ToString(), *InDebugAssetName);
			}
			else
			{
				// Add subgraphs to context so they are accessible for subsequent graphs.
				DocumentContext.Graphs.Add(FrontendSubgraphPtr->ID, Subgraph);
			}
		}

		// Create parent graph.
		return CreateGraphInternal(Context, DocumentContext, InGraph, InGraphId);
	}

	TUniquePtr<FFrontendGraph> FGraphBuilder::CreateGraph(const FMetasoundFrontendDocument& InDocument, const FString& InDebugAssetName)
	{
		// Create proxies before creating graph.
		Frontend::FProxyDataCache ProxyDataCache;
		ProxyDataCache.CreateAndCacheProxies(InDocument);
		
		return CreateGraph(InDocument, ProxyDataCache, InDebugAssetName, Frontend::CreateLocallyUniqueId());
	}

	TUniquePtr<FFrontendGraph> FGraphBuilder::CreateGraph(const FMetasoundFrontendGraphClass& InGraph, const TArray<FMetasoundFrontendGraphClass>& InSubgraphs, const TArray<FMetasoundFrontendClass>& InDependencies, const FString& InDebugAssetName)
	{
		// Create proxies before building graph
		Frontend::FProxyDataCache ProxyDataCache;
		ProxyDataCache.CreateAndCacheProxies(InGraph);

		for (const FMetasoundFrontendGraphClass& SubgraphClass : InSubgraphs)
		{
			ProxyDataCache.CreateAndCacheProxies(SubgraphClass);
		}

		for (const FMetasoundFrontendClass& DependencyClass : InDependencies)
		{
			ProxyDataCache.CreateAndCacheProxies(DependencyClass);
		}

		return CreateGraph(InGraph, InSubgraphs, InDependencies, ProxyDataCache, InDebugAssetName, Frontend::CreateLocallyUniqueId());
	}

	TUniquePtr<FFrontendGraph> FGraphBuilder::CreateGraph(const FMetasoundFrontendDocument& InDocument, const Frontend::FProxyDataCache& InProxyDataCache, const FString& InDebugAssetName, const FGuid InGraphId)
	{
		return CreateGraph(InDocument.RootGraph, InDocument.Subgraphs, InDocument.Dependencies, InProxyDataCache, InDebugAssetName, InGraphId);
	}
}

