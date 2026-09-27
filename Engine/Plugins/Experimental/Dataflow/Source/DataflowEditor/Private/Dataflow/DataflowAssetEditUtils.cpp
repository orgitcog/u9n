// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowAssetEditUtils.h"

#include "HAL/PlatformApplicationMisc.h"
#include "Dataflow/AssetDefinition_DataflowAsset.h"
#include "Dataflow/DataflowObject.h"
#include "Dataflow/DataflowSchema.h"
#include "Dataflow/DataflowNode.h"
#include "Dataflow/DataflowEdNode.h"
#include "Dataflow/DataflowSNode.h"
#include "Dataflow/DataflowSubGraph.h"
#include "Dataflow/DataflowSubGraphNodes.h"
#include "Dataflow/DataflowVariableNodes.h"
#include "Editor.h"
#include "PropertyBagDetails.h"
#include "ScopedTransaction.h"
#include "Settings/EditorStyleSettings.h"

#define LOCTEXT_NAMESPACE "DataflowAssetEditUtils"

namespace UE::Dataflow
{
	namespace EditAssetUtils::Private
	{
		static const TCHAR* DataflowVariableClipboardPrefix = TEXT("DataflowVariable_");
		
		enum class EChangeResult
		{
			None,
			Changed,
			Cancel,
		};

		/** 
		* Change a dataflow asset with a transaction
		* The InFunction return paramater determines if the asset will be modified or not 
		* if modification happens, a PostEditChangeProperty notification is sent
		*/
		static void ChangeDataflowAssetWithTransaction(UDataflow* DataflowAsset, const FText &TransactionName, TFunctionRef<EChangeResult(UDataflow&)> InFunction, FName ChangedPropertyName)
		{
			if (DataflowAsset)
			{
				FScopedTransaction Transaction(TransactionName);

				const EChangeResult Result = InFunction(*DataflowAsset);
				switch (Result)
				{
				case EChangeResult::Cancel:
					Transaction.Cancel();
					break;

				case EChangeResult::Changed:
				{
					DataflowAsset->Modify();
					if (!ChangedPropertyName.IsNone())
					{
						FPropertyChangedEvent PropertyChangedEvent(nullptr);
						if (UClass* DataflowClass = DataflowAsset->GetClass())
						{
							FProperty* MemberProperty = DataflowClass->FindPropertyByName(ChangedPropertyName);
							PropertyChangedEvent.SetActiveMemberProperty(MemberProperty);
						}
						DataflowAsset->PostEditChangeProperty(PropertyChangedEvent);
					}
					break;
				}
				case EChangeResult::None:
				default:
					break;
				}
			}
		}

		/** Generate a Dataflow child object unique name from a BaseName ( Node or Subgraph for example ) */
		static FName GenerateUniqueObjectName(UDataflow* Dataflow, const FName InBaseName)
		{
			FString Left, Right;
			int32 NameIndex = 1;

			// Check if NodeBaseName already ends with "_dd"
			FName BaseName(InBaseName);
			if (BaseName.ToString().Split(TEXT("_"), &Left, &Right, ESearchCase::IgnoreCase, ESearchDir::FromEnd))
			{
				if (Right.IsNumeric())
				{
					NameIndex = FCString::Atoi(*Right);
					BaseName = FName(Left);
				}
			}

			// name must be unique for all nodes in the Dataflow::FGraph 
			// Unreal require names to be unique within the parent but because we have one FGraph across all EdGraph ( include SubGraphs) objects 
			// we need to make sure the name is unique across them, so that we don't get a assert when creating the EdNode
			FName UniqueName = BaseName;
			bool bNameWasChanged = false;
			do {
				// reset for this loop 
				bNameWasChanged = false;
				if (!::IsUniqueObjectName(UniqueName, Dataflow))
				{
					UniqueName = ::MakeUniqueObjectName(Dataflow, UDataflowEdNode::StaticClass(), UniqueName);
					bNameWasChanged = true;
				}

				for (TObjectPtr<UDataflowSubGraph> SubGraph: Dataflow->GetSubGraphs())
				{
					if (!::IsUniqueObjectName(UniqueName, SubGraph))
					{
						UniqueName = ::MakeUniqueObjectName(SubGraph, UDataflowEdNode::StaticClass(), UniqueName);
						bNameWasChanged = true;
					}
				}
			} while (bNameWasChanged);

			return UniqueName;
		}

		static TSharedPtr<FDataflowNode> AddDataflowNode(UDataflow* Dataflow, FName NodeName, FName NodeTypeName)
		{
			if (UE::Dataflow::FNodeFactory* Factory = UE::Dataflow::FNodeFactory::GetInstance())
			{
				FNewNodeParameters Parameters
				{
					.Guid = FGuid::NewGuid(),
					.Type = NodeTypeName,
					.Name = GenerateUniqueObjectName(Dataflow, NodeName),
					.OwningObject = Dataflow,
				};

				return Factory->NewNodeFromRegisteredType(*Dataflow->GetDataflow(), Parameters);
			}
			return nullptr;
		}

		static UDataflowEdNode* CreateDataflowEdNode(UEdGraph* EdGraph, TSharedPtr<FDataflowNode> DataflowNode, const FVector2D& Location, UEdGraphPin* FromPin)
		{
			if (UDataflow* Dataflow = UDataflow::GetDataflowAssetFromEdGraph(EdGraph))
			{
				if (UDataflowEdNode* EdNode = NewObject<UDataflowEdNode>(EdGraph, UDataflowEdNode::StaticClass(), DataflowNode->GetName()))
				{
					EdNode->SetFlags(RF_Transactional);

					Dataflow->Modify();
					EdGraph->Modify();

					// make sure we set the guid before adding to graph so that the listener to the graph notification have all the info needed
					EdNode->SetDataflowGraph(Dataflow->GetDataflow());
					EdNode->SetDataflowNodeGuid(DataflowNode->GetGuid());

					EdGraph->AddNode(EdNode, /*bUserAction*/true, /*bSelectNewNode*/false);
					
					EdNode->CreateNewGuid();
					EdNode->PostPlacedNewNode();
					EdNode->AllocateDefaultPins();

					if (FromPin)
					{
						FromPin->Modify();
						EdNode->AutowireNewNode(FromPin);
					}

					EdNode->NodePosX = Location.X;
					EdNode->NodePosY = Location.Y;

					return EdNode;
				}
			}
			return nullptr;
		}

		UEdGraphNode_Comment* CreateCommentEdNode(UEdGraph* EdGraph, const FVector2D& Location, const FString& Comment, const FVector2D Size, const FLinearColor& Color, int32 FontSize)
		{
			if (UDataflow* Dataflow = UDataflow::GetDataflowAssetFromEdGraph(EdGraph))
			{
				if (UEdGraphNode_Comment* CommentTemplate = NewObject<UEdGraphNode_Comment>(EdGraph))
				{
					CommentTemplate->SetFlags(RF_Transactional);

					Dataflow->Modify();
					EdGraph->Modify();

					CommentTemplate->bCommentBubbleVisible_InDetailsPanel = false;
					CommentTemplate->bCommentBubbleVisible = false;
					CommentTemplate->bCommentBubblePinned = false;

					// set outer to be the graph so it doesn't go away
					CommentTemplate->Rename(NULL, EdGraph, REN_NonTransactional);
					EdGraph->AddNode(CommentTemplate, true, /*bSelectNewNode*/false);

					CommentTemplate->CreateNewGuid();
					CommentTemplate->PostPlacedNewNode();
					CommentTemplate->AllocateDefaultPins();

					CommentTemplate->NodePosX = Location.X;
					CommentTemplate->NodePosY = Location.Y;
					CommentTemplate->NodeWidth = Size.X;
					CommentTemplate->NodeHeight = Size.Y;
					CommentTemplate->SnapToGrid(GetDefault<UEditorStyleSettings>()->GridSnapSize);
					CommentTemplate->CommentColor = Color;
					CommentTemplate->FontSize = FontSize;

					CommentTemplate->NodeComment = Comment;


					EdGraph->NotifyGraphChanged();

					return CommentTemplate;
				}
			}
			return nullptr;
		}

		/** returns true if the variable was modified */
		static bool ModifyVariable(UDataflow& DataflowAsset, FName Variable, TFunctionRef<void(FPropertyBagPropertyDesc& PropertyDesc)> InFunction)
		{
			bool bModified = false;

			TArray<FPropertyBagPropertyDesc> NewPropertyDescs;
			NewPropertyDescs.Append(DataflowAsset.Variables.GetPropertyBagStruct()->GetPropertyDescs());
			for (FPropertyBagPropertyDesc& PropertyDesc : NewPropertyDescs)
			{
				if (PropertyDesc.Name == Variable)
				{
					InFunction(PropertyDesc);
					bModified = true;
				}
			}

			if (bModified)
			{
				if (const UPropertyBag* NewBagStruct = UPropertyBag::GetOrCreateFromDescs(NewPropertyDescs))
				{
					DataflowAsset.Variables.MigrateToNewBagStruct(NewBagStruct);
					FDataflowAssetDelegates::OnVariablesChanged.Broadcast(&DataflowAsset, Variable);
					return true;
				}
			}
			return false;
		}

		/** Generate a Dataflow asset variable unique name from a BaseName */
		static FName GenerateUniqueVariableName(const UDataflow& DataflowAsset, FName BaseName)
		{
			int32 Counter = 1;
			FName UniqueName{ BaseName };
			const FString BasenameStr{ BaseName.ToString() };
			while (true)
			{
				if (DataflowAsset.Variables.FindPropertyDescByName(UniqueName) == nullptr)
				{
					break; // found an available name exit 
				}
				UniqueName = FName(FString::Format(TEXT("{0}_{1}"), { BasenameStr, FString::FromInt(Counter++)}));
			}
			return UniqueName;
		}

		static UEdGraphPin* FindPin(const UDataflowEdNode* Node, const EEdGraphPinDirection Direction, const FName Name)
		{
			for (UEdGraphPin* Pin : Node->GetAllPins())
			{
				if (Pin->PinName == Name && Pin->Direction == Direction)
				{
					return Pin;
				}
			}

			return nullptr;
		}

		void RenameSubGraphCallNodes(UDataflow& DataflowAsset, UEdGraph& EdGraph, const FGuid& SubGraphGuid, FName NewSubGraphName)
		{
			for (UEdGraphNode* EdNode : EdGraph.Nodes)
			{
				if (UDataflowEdNode* DataflowEdNode = Cast<UDataflowEdNode>(EdNode))
				{
					if (TSharedPtr<FDataflowNode> DataflowNode = DataflowEdNode->GetDataflowNode())
					{
						if (FDataflowCallSubGraphNode* CallNode = DataflowNode->AsType<FDataflowCallSubGraphNode>())
						{
							if (CallNode->GetSubGraphGuid() == SubGraphGuid)
							{
								const FName UniqueName = GenerateUniqueObjectName(&DataflowAsset, NewSubGraphName);
								DataflowEdNode->Rename(*UniqueName.ToString(), &EdGraph);
								CallNode->SetName(UniqueName);
							}
						}
					}
				}
			}
		}

		void RenameSubGraphCallNodes(UDataflow& DataflowAsset, const FGuid& SubGraphGuid, FName NewSubGraphName)
		{
			RenameSubGraphCallNodes(DataflowAsset, DataflowAsset, SubGraphGuid, NewSubGraphName);
			for (UDataflowSubGraph* SubGraph : DataflowAsset.GetSubGraphs())
			{
				if (SubGraph)
				{
					RenameSubGraphCallNodes(DataflowAsset, *SubGraph, SubGraphGuid, NewSubGraphName);
				}
			}
		}

		void RenameVariableCallNodes(UDataflow& DataflowAsset, UEdGraph& EdGraph, FName VariableName, FName NewVariableName)
		{
			for (UEdGraphNode* EdNode : EdGraph.Nodes)
			{
				if (UDataflowEdNode* DataflowEdNode = Cast<UDataflowEdNode>(EdNode))
				{
					if (TSharedPtr<FDataflowNode> DataflowNode = DataflowEdNode->GetDataflowNode())
					{
						if (FGetDataflowVariableNode* VariableNode = DataflowNode->AsType<FGetDataflowVariableNode>())
						{
							if (VariableNode->GetVariableName() == VariableName)
							{
								const FName UniqueName = GenerateUniqueObjectName(&DataflowAsset, NewVariableName);
								DataflowEdNode->Rename(*UniqueName.ToString(), &EdGraph);
								VariableNode->SetName(UniqueName);
								VariableNode->SetVariable(&DataflowAsset, NewVariableName);
							}
						}
					}
				}
			}
		}

		void RenameVariableCallNodes(UDataflow& DataflowAsset, FName VariableName, FName NewVariableName)
		{
			RenameVariableCallNodes(DataflowAsset, DataflowAsset, VariableName, NewVariableName);
			for (UDataflowSubGraph* SubGraph : DataflowAsset.GetSubGraphs())
			{
				if (SubGraph)
				{
					RenameVariableCallNodes(DataflowAsset, *SubGraph, VariableName, NewVariableName);
				}
			}
		}

		void DeleteNodesNoTransaction(UEdGraph* EdGraph, const TArray<UEdGraphNode*>& NodesToDelete)
		{
			if (EdGraph && NodesToDelete.Num())
			{
				for (UEdGraphNode* EdNode : NodesToDelete)
				{
					EdNode->Modify();
					if (UDataflowEdNode* DataflowEdNode = Cast<UDataflowEdNode>(EdNode))
					{
						if (const TSharedPtr<UE::Dataflow::FGraph> DataflowGraph = DataflowEdNode->GetDataflowGraph())
						{
							EdGraph->RemoveNode(EdNode);
							if (TSharedPtr<FDataflowNode> DataflowNode = DataflowEdNode->GetDataflowNode())
							{
								DataflowGraph->RemoveNode(DataflowNode);
							}
						}
					}
					else if (UEdGraphNode_Comment* CommentNode = Cast<UEdGraphNode_Comment>(EdNode))
					{
						EdGraph->RemoveNode(CommentNode);
					}

					// Auto-rename node so that its current name is made available until it is garbage collected
					EdNode->Rename();
				}
			}
		}

	}

	bool FEditAssetUtils::IsUniqueDataflowSubObjectName(UDataflow* DataflowAsset, FName SubObjectName)
	{
		return ::IsUniqueObjectName(SubObjectName, DataflowAsset);
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////////////
	// 
	// NODE API
	// 
	//////////////////////////////////////////////////////////////////////////////////////////////////////////
	UDataflowEdNode* FEditAssetUtils::AddNewNode(UEdGraph* EdGraph, const FVector2D& Location, const FName NodeName, const FName NodeTypeName, UEdGraphPin* FromPin)
	{
		using namespace EditAssetUtils::Private;

		const FText TransactionName{ LOCTEXT("AddNewDataflowNode", "Add New Dataflow Node") };

		UDataflowEdNode* EdNodeToReturn = nullptr;

		if (UDataflow* Dataflow = UDataflow::GetDataflowAssetFromEdGraph(EdGraph))
		{
			auto AddNewNodeInternal =
				[&Dataflow, &EdGraph, &Location, &NodeName, &NodeTypeName, &FromPin, &EdNodeToReturn]
				(UDataflow& DataflowAsset) -> EChangeResult
				{
					if (TSharedPtr<FDataflowNode> DataflowNode = AddDataflowNode(Dataflow, NodeName, NodeTypeName))
					{
						if (UDataflowEdNode* EdNode = CreateDataflowEdNode(EdGraph, DataflowNode, Location, FromPin))
						{
							EdNodeToReturn = EdNode;
							return EChangeResult::Changed;
						}
					}
					return EChangeResult::None;
				};

			ChangeDataflowAssetWithTransaction(Dataflow, TransactionName, AddNewNodeInternal, NAME_None);
		}
		return EdNodeToReturn;
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////////////
	UEdGraphNode* FEditAssetUtils::AddNewComment(UEdGraph* EdGraph, const FVector2D& Location, const FVector2D& Size)
	{
		using namespace EditAssetUtils::Private;

		const FText TransactionName{ LOCTEXT("AddNewDataflowComment", "Add New Dataflow Comment") };

		UEdGraphNode* EdNodeToReturn = nullptr;

		if (UDataflow* Dataflow = UDataflow::GetDataflowAssetFromEdGraph(EdGraph))
		{
			auto AddNewCommentInternal =
				[&Dataflow, &EdGraph, &Location, &Size, &EdNodeToReturn]
				(UDataflow& DataflowAsset) -> EChangeResult
				{
					const FString DefaultText = "Comment";
					const FLinearColor DefaultColor = FLinearColor::White;
					const int32 DefaultFontSize = 18;
					if (UEdGraphNode* EdNode = CreateCommentEdNode(EdGraph, Location, DefaultText, Size, DefaultColor, DefaultFontSize))
					{
						EdNodeToReturn = EdNode;
						return EChangeResult::Changed;
					}
					return EChangeResult::None;
				};

			ChangeDataflowAssetWithTransaction(Dataflow, TransactionName, AddNewCommentInternal, NAME_None);
		}
		return EdNodeToReturn;
	}
	
	//////////////////////////////////////////////////////////////////////////////////////////////////////////
	void FEditAssetUtils::DeleteNodes(UEdGraph* EdGraph, const TArray<UEdGraphNode*>& NodesToDelete)
	{
		using namespace EditAssetUtils::Private;

		const FText TransactionName{ LOCTEXT("DeleteDataflowNodes", "Delete Dataflow Nodes") };

		if (UDataflow* Dataflow = UDataflow::GetDataflowAssetFromEdGraph(EdGraph))
		{
			auto DeleteNodesInternal =
				[&EdGraph, &NodesToDelete]
				(UDataflow& DataflowAsset) -> EChangeResult
				{
					DeleteNodesNoTransaction(EdGraph, NodesToDelete);
					return EChangeResult::Changed;
				};

			ChangeDataflowAssetWithTransaction(Dataflow, TransactionName, DeleteNodesInternal, NAME_None);
		}
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////////////
	void FEditAssetUtils::DuplicateNodes(UEdGraph* EdGraph, const TArray<UEdGraphNode*>& EdNodesToDuplicate, const FVector2D& Location, TArray<UEdGraphNode*>& OutDuplicatedNodes)
	{
		TMap<FGuid, FGuid> NodeGuidMap;
		DuplicateNodes(EdGraph, EdNodesToDuplicate, EdGraph, Location, OutDuplicatedNodes, NodeGuidMap);
	}

	void FEditAssetUtils::DuplicateNodes(UEdGraph* SourceEdGraph, const TArray<UEdGraphNode*>& EdNodesToDuplicate, UEdGraph* TargetEdGraph, const FVector2D& Location, TArray<UEdGraphNode*>& OutDuplicatedNodes, TMap<FGuid, FGuid>& OutNodeGuidMap)
	{
		using namespace EditAssetUtils::Private;

		UDataflow* SourceDataflowAsset = UDataflow::GetDataflowAssetFromEdGraph(SourceEdGraph);
		UDataflow* TargetDataflowAsset = UDataflow::GetDataflowAssetFromEdGraph(TargetEdGraph);
		if (!SourceDataflowAsset || !TargetDataflowAsset)
		{
			// no graph to copy to
			return;
		}

		if (EdNodesToDuplicate.Num() == 0)
		{
			return;
		}

		const FText TransactionName = FText::Format(LOCTEXT("DuplicateDataflowNode", "Duplicate {0} Dataflow Nodes"), FText::AsNumber(EdNodesToDuplicate.Num()));

		// location of the first node as a reference for all the others 
		const FVector2D RefLocation
		{
			(double)EdNodesToDuplicate[0]->NodePosX,
			(double)EdNodesToDuplicate[0]->NodePosY
		};

		auto DuplicateNodesInternal =
			[&OutNodeGuidMap, &SourceEdGraph, TargetEdGraph, &Location, &RefLocation, &EdNodesToDuplicate, &OutDuplicatedNodes]
			(UDataflow& TargetDataflowAsset) -> EChangeResult
			{
				TMap<FGuid, UDataflowEdNode*> EdNodeMap;
				TMap<FGuid, FGuid> NodeGuidMap;

				// copy the nodes and comments first 
				for (UEdGraphNode* EdNodeToDuplicate : EdNodesToDuplicate)
				{
					const FVector2D OriginalNodeLocation
					{
						(double)EdNodeToDuplicate->NodePosX,
						(double)EdNodeToDuplicate->NodePosY
					};
					const FVector2D NodeLocation(Location + (OriginalNodeLocation - RefLocation));

					if (UDataflowEdNode* DataflowEdNodeToDuplicate = Cast<UDataflowEdNode>(EdNodeToDuplicate))
					{
						if (const TSharedPtr<FDataflowNode> NodeToDuplicate = DataflowEdNodeToDuplicate->GetDataflowNode())
						{
							const FName NodeName = NodeToDuplicate->GetName();
							const FName NodeTypeName = NodeToDuplicate->GetType();

							if (TSharedPtr<FDataflowNode> DataflowNode = AddDataflowNode(&TargetDataflowAsset, NodeName, NodeTypeName))
							{
								SDataflowEdNode::CopyDataflowNodeSettings(NodeToDuplicate, DataflowNode);
	
								if (UDataflowEdNode* EdNode = CreateDataflowEdNode(TargetEdGraph, DataflowNode, NodeLocation, /*FromPin*/nullptr))
								{
									EdNodeMap.Add(DataflowEdNodeToDuplicate->DataflowNodeGuid, EdNode);
									NodeGuidMap.Add(DataflowEdNodeToDuplicate->DataflowNodeGuid, EdNode->DataflowNodeGuid);
									OutDuplicatedNodes.Add(EdNode);
								}
							}
						}
					}
					else if (const UEdGraphNode_Comment* CommentEdNodeToDuplicate = Cast<const UEdGraphNode_Comment>(EdNodeToDuplicate))
					{
						const FVector2D CommentSize
						{
							(double)CommentEdNodeToDuplicate->NodeWidth,
							(double)CommentEdNodeToDuplicate->NodeHeight
						};
						if (UEdGraphNode_Comment* CommentEdNode = CreateCommentEdNode(TargetEdGraph, NodeLocation, CommentEdNodeToDuplicate->NodeComment, CommentSize, CommentEdNodeToDuplicate->CommentColor, CommentEdNodeToDuplicate->FontSize))
						{
							OutDuplicatedNodes.Add(CommentEdNode);
						}
					}
				}

				// Recreate connections between duplicated nodes
				for (const UEdGraphNode* EdNodeToDuplicate : EdNodesToDuplicate)
				{
					if (const UDataflowEdNode* DataflowEdNodeToDuplicate = Cast<const UDataflowEdNode>(EdNodeToDuplicate))
					{
						if (const TSharedPtr<const FDataflowNode> DataflowNode = DataflowEdNodeToDuplicate->GetDataflowNode())
						{
							const FGuid DataflowNodeAGuid = DataflowNode->GetGuid();
							for (const FDataflowOutput* Output : DataflowNode->GetOutputs())
							{
								for (const FDataflowInput* Connection : Output->Connections)
								{
									const FName OutputputName = Connection->GetConnection()->GetName();

									// Check if the node on the end of the connection was duplicated
									const FGuid DataflowNodeBGuid = Connection->GetOwningNode()->GetGuid();

									if (NodeGuidMap.Contains(DataflowNodeBGuid))
									{
										const FName InputputName = Connection->GetName();
										if (TSharedPtr<FDataflowNode> DuplicatedDataflowNodeA = TargetDataflowAsset.GetDataflow()->FindBaseNode(NodeGuidMap[DataflowNodeAGuid]))
										{
											FDataflowOutput* OutputConnection = DuplicatedDataflowNodeA->FindOutput(OutputputName);
											if (TSharedPtr<FDataflowNode> DuplicatedDataflowNodeB = TargetDataflowAsset.GetDataflow()->FindBaseNode(NodeGuidMap[DataflowNodeBGuid]))
											{
												FDataflowInput* InputConnection = DuplicatedDataflowNodeB->FindInput(InputputName);

												TargetDataflowAsset.GetDataflow()->Connect(OutputConnection, InputConnection);

												// Connect the UDataflowEdNode FPins as well
												if (UEdGraphPin* OutputPin = FindPin(EdNodeMap[DataflowNodeAGuid], EEdGraphPinDirection::EGPD_Output, OutputputName))
												{
													if (UEdGraphPin* InputPin = FindPin(EdNodeMap[DataflowNodeBGuid], EEdGraphPinDirection::EGPD_Input, InputputName))
													{
														OutputPin->MakeLinkTo(InputPin);
													}
												}
											}
										}
									}
								}
							}
						}
					}
				}
				OutNodeGuidMap = MoveTemp(NodeGuidMap);
				return EChangeResult::Changed;
			};

		ChangeDataflowAssetWithTransaction(TargetDataflowAsset, TransactionName, DuplicateNodesInternal, NAME_None);
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////////////
	void FEditAssetUtils::CopyNodesToClipboard(const TArray<const UEdGraphNode*>& NodesToCopy, int32& OutNumCopiedNodes)
	{
		FDataflowCopyPasteContent CopyPasteContent;

		TSet<FGuid> NodeGuids;
		TArray<const FDataflowInput*> NodeInputsToSave;

		// no need for transaction when copying to external system like the clipboard
		for (const UEdGraphNode* EdNode : NodesToCopy)
		{
			if (const UDataflowEdNode* DataflowEdNode = Cast<const UDataflowEdNode>(EdNode))
			{
				if (const TSharedPtr<const FDataflowNode> DataflowNode = DataflowEdNode->GetDataflowNode())
				{
					NodeGuids.Add(DataflowNode->GetGuid());
					NodeInputsToSave.Append(DataflowNode->GetInputs());

					FString ContentString;
					const TUniquePtr<FDataflowNode> DefaultElement;
					DataflowNode->TypedScriptStruct()->ExportText(ContentString, DataflowNode.Get(), DataflowNode.Get(), nullptr, PPF_None, nullptr);

					FDataflowNodeData NodeData
					{
						.Type = DataflowNode->GetType().ToString(),
						.Name = DataflowNode->GetName().ToString(),
						.Properties = MoveTemp(ContentString),
						.Position = { (double)EdNode->NodePosX, (double)EdNode->NodePosY },
					};
					CopyPasteContent.NodeData.Emplace(NodeData);
				}
			}
			else if (const UEdGraphNode_Comment* CommentEdNode = Cast<const UEdGraphNode_Comment>(EdNode))
			{
				FDataflowCommentNodeData CommentNodeData
				{
					.Name = CommentEdNode->NodeComment,
					.Size = { (double)CommentEdNode->NodeWidth, (double)CommentEdNode->NodeHeight },
					.Color = CommentEdNode->CommentColor,
					.Position = { (double)CommentEdNode->NodePosX, (double)CommentEdNode->NodePosY },
					.FontSize = CommentEdNode->FontSize,
				};
				CopyPasteContent.CommentNodeData.Emplace(CommentNodeData);
			}
		}

		// now gather connection data 
		// Build connection data
		for (const FDataflowInput* Input : NodeInputsToSave)
		{
			if (!Input)
			{
				continue;
			}

			const FDataflowOutput* Output = Input->GetConnection();
			if (!Output)
			{
				continue;
			}
			
			if (NodeGuids.Contains(Output->GetOwningNodeGuid()))
			{
				FDataflowConnectionData DataflowConnectionData;
				DataflowConnectionData.Set(*Output, *Input);
				CopyPasteContent.ConnectionData.Emplace(DataflowConnectionData);
			}
		}

		// copy to clipboard 
		if (CopyPasteContent.NodeData.Num() ||
			CopyPasteContent.CommentNodeData.Num() ||
			CopyPasteContent.ConnectionData.Num())
		{
			FString ClipboardContent;
			const FDataflowCopyPasteContent DefaultContent;
			FDataflowCopyPasteContent::StaticStruct()->ExportText(ClipboardContent, &CopyPasteContent, &DefaultContent, nullptr, PPF_None, nullptr);

			FPlatformApplicationMisc::ClipboardCopy(*ClipboardContent);
		}

		OutNumCopiedNodes = CopyPasteContent.NodeData.Num() + CopyPasteContent.CommentNodeData.Num();
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////////////
	void FEditAssetUtils::PasteNodesFromClipboard(UEdGraph* EdGraph, const FVector2D& Location, TArray<UEdGraphNode*>& OutPastedNodes)
	{
		using namespace EditAssetUtils::Private;

		UDataflow* DataflowAsset = UDataflow::GetDataflowAssetFromEdGraph(EdGraph);
		if (!DataflowAsset)
		{
			// no graph to copy to
			return;
		}

		FString ClipboardPayload;
		FPlatformApplicationMisc::ClipboardPaste(ClipboardPayload);

		if (ClipboardPayload.IsEmpty())
		{
			// nothing to paste, nothing to do
			return;
		}

		const FDataflowCopyPasteContent DefaultContent;
		FDataflowCopyPasteContent CopyPasteContent;
		FDataflowCopyPasteContent::StaticStruct()->ImportText(*ClipboardPayload, &CopyPasteContent, nullptr, EPropertyPortFlags::PPF_None, nullptr, FDataflowCopyPasteContent::StaticStruct()->GetName(), true);

		const int32 TotalNodesToPaste = CopyPasteContent.NodeData.Num() + CopyPasteContent.CommentNodeData.Num();
		const FText TransactionName = FText::Format(LOCTEXT("PasteDataflowNodes", "Paste {0} Dataflow Nodes"), FText::AsNumber(TotalNodesToPaste));

		auto PasteNodesInternal =
		[&EdGraph, &CopyPasteContent, &Location, &OutPastedNodes](UDataflow& DataflowAsset)->EChangeResult
		{
			TMap<FString, UDataflowEdNode*> OriginalNodeNameToEdNode;

			// compute a ref for all nodes to refer to 
			FVector2D RefLocation { 0, 0 };
			if (CopyPasteContent.NodeData.Num())
			{
				RefLocation.X = CopyPasteContent.NodeData[0].Position.X;
				RefLocation.Y = CopyPasteContent.NodeData[0].Position.Y;
			}
			else if (CopyPasteContent.CommentNodeData.Num())
			{
				RefLocation.X = CopyPasteContent.CommentNodeData[0].Position.X;
				RefLocation.Y = CopyPasteContent.CommentNodeData[0].Position.Y;
			}

			// paste nodes
			for (const FDataflowNodeData& NodeData : CopyPasteContent.NodeData)
			{
				const FName NodeType(*NodeData.Type);
				const FName NodeName(*NodeData.Name);
				const FVector2D NodeLocation(Location + (NodeData.Position - RefLocation));

				if (TSharedPtr<FDataflowNode> DataflowNode = AddDataflowNode(&DataflowAsset, NodeName, NodeType))
				{
					// load properties to DataflowNode
					if (!NodeData.Properties.IsEmpty())
					{
						DataflowNode->TypedScriptStruct()->ImportText(*NodeData.Properties, DataflowNode.Get(), nullptr, EPropertyPortFlags::PPF_None, nullptr, DataflowNode->TypedScriptStruct()->GetName(), true);
					}

					// if we are pasting a dataflow variable, let's create the variable if needed
					if (FGetDataflowVariableNode* VariableNode = DataflowNode->AsType<FGetDataflowVariableNode>())
					{
						VariableNode->TryAddVariableToDataflowAsset(DataflowAsset);
					}

					// Do any post-import fixup.
					FArchive Ar;
					Ar.SetIsLoading(true);
					DataflowNode->PostSerialize(Ar);
		
					if (UDataflowEdNode* EdNode = CreateDataflowEdNode(EdGraph, DataflowNode, NodeLocation, nullptr))
					{
						OriginalNodeNameToEdNode.Add(NodeData.Name, EdNode);
						OutPastedNodes.Add(EdNode);
					}
				}
			}

			// Paste Comment nodes
			for (const FDataflowCommentNodeData& CommentNodeData : CopyPasteContent.CommentNodeData)
			{
				const FVector2D CommentNodeLocation(Location + (CommentNodeData.Position - RefLocation));

				if (UEdGraphNode_Comment* CommentEdNode = CreateCommentEdNode(EdGraph, CommentNodeLocation, CommentNodeData.Name, CommentNodeData.Size, CommentNodeData.Color, CommentNodeData.FontSize))
				{
					OutPastedNodes.Add(CommentEdNode);
				}
			}

			// Recreate connections
			for (const FDataflowConnectionData& Connection : CopyPasteContent.ConnectionData)
			{
				FString NodeIn, PropertyIn, TypeIn;
				FDataflowConnectionData::GetNodePropertyAndType(Connection.In, NodeIn, PropertyIn, TypeIn);

				FString NodeOut, PropertyOut, TypeOut;
				FDataflowConnectionData::GetNodePropertyAndType(Connection.Out, NodeOut, PropertyOut, TypeOut);

				ensure(TypeIn == TypeOut);

				const UDataflowEdNode* EdNodeIn = OriginalNodeNameToEdNode[NodeIn];
				const UDataflowEdNode* EdNodeOut = OriginalNodeNameToEdNode[NodeOut];

				const FGuid GuidIn = EdNodeIn? EdNodeIn->DataflowNodeGuid: FGuid();
				const FGuid GuidOut = EdNodeOut ? EdNodeOut->DataflowNodeGuid : FGuid();

				const FName InputputName = *PropertyIn;
				const FName OutputputName = *PropertyOut;

				if (TSharedPtr<UE::Dataflow::FGraph> DataflowGraph = DataflowAsset.GetDataflow())
				{
					if (TSharedPtr<FDataflowNode> DataflowNodeFrom = DataflowGraph->FindBaseNode(GuidOut))
					{
						if (TSharedPtr<FDataflowNode> DataflowNodeTo = DataflowGraph->FindBaseNode(GuidIn))
						{
							FDataflowInput* InputConnection = DataflowNodeTo->FindInput(InputputName);
							FDataflowOutput* OutputConnection = DataflowNodeFrom->FindOutput(OutputputName);

							// make sure we set the right type before attempting any connection
							DataflowNodeFrom->TrySetConnectionType(OutputConnection, FName(TypeOut));
							DataflowNodeTo->TrySetConnectionType(InputConnection, FName(TypeIn));

							// first connect the edgraph as this may affect the dataflow inputs ( for AnyType )
							UEdGraphPin* OutputPin = FindPin(EdNodeOut, EEdGraphPinDirection::EGPD_Output, OutputputName);
							UEdGraphPin* InputPin = FindPin(EdNodeIn, EEdGraphPinDirection::EGPD_Input, InputputName);
							if (OutputPin && InputPin)
							{
								EdGraph->GetSchema()->TryCreateConnection(OutputPin, InputPin);
							}

							// now connect the dataflow
							DataflowGraph->Connect(OutputConnection, InputConnection);
						}
					}
				}
			}
			return EChangeResult::Changed;
		};

		// make sure we notify that variables may have changed as a property since we may have pasted variable nodes
		ChangeDataflowAssetWithTransaction(DataflowAsset, TransactionName, PasteNodesInternal, GET_MEMBER_NAME_CHECKED(UDataflow, Variables));
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////////////
	// 
	// VARIABLES API
	// 
	//////////////////////////////////////////////////////////////////////////////////////////////////////////
	FName FEditAssetUtils::AddNewVariable(UDataflow* DataflowAsset, FName BaseName)
	{
		using namespace EditAssetUtils::Private;

		const FText TransactionName{ LOCTEXT("AddNewDataflowVariable", "Add New Dataflow Variable") };

		FName UniqueVariableName;

		auto AddNewVariableInternal =
			[&BaseName, &UniqueVariableName](UDataflow& DataflowAsset) -> EChangeResult
			{
				UniqueVariableName = GenerateUniqueVariableName(DataflowAsset, BaseName);
				DataflowAsset.Variables.AddProperty(UniqueVariableName, EPropertyBagPropertyType::Int32);
				FDataflowAssetDelegates::OnVariablesChanged.Broadcast(&DataflowAsset, UniqueVariableName);
				return EChangeResult::Changed;
			};

		ChangeDataflowAssetWithTransaction(DataflowAsset, TransactionName, AddNewVariableInternal, GET_MEMBER_NAME_CHECKED(UDataflow, Variables));

		return UniqueVariableName;
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////////////
	void FEditAssetUtils::DeleteVariable(UDataflow* DataflowAsset, FName VariableName)
	{
		using namespace EditAssetUtils::Private;

		const FText TransactionName{ LOCTEXT("DeleteDataflowVariable", "Delete Dataflow Variable") };

		auto DeleteVariableInternal =
			[&VariableName](UDataflow& DataflowAsset) -> EChangeResult
			{
				DataflowAsset.Variables.RemovePropertyByName(VariableName);
				FDataflowAssetDelegates::OnVariablesChanged.Broadcast(&DataflowAsset, VariableName);
				return EChangeResult::Changed;
			};

		ChangeDataflowAssetWithTransaction(DataflowAsset, TransactionName, DeleteVariableInternal, GET_MEMBER_NAME_CHECKED(UDataflow, Variables));
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////////////
	void FEditAssetUtils::RenameVariable(UDataflow* DataflowAsset, FName OldVariableName, FName NewVariableName)
	{
		using namespace EditAssetUtils::Private;

		const FText TransactionName{ LOCTEXT("RenameDataflowVariable", "Rename Dataflow Variable") };

		auto SetVariableNameInternal =
			[OldVariableName, NewVariableName](UDataflow& DataflowAsset) -> EChangeResult
			{
				auto ChangePropertyNameLambda =
					[NewVariableName](FPropertyBagPropertyDesc& PropertyDesc)
					{
						PropertyDesc.Name = NewVariableName;
					};

				if (ModifyVariable(DataflowAsset, OldVariableName, ChangePropertyNameLambda))
				{
					RenameVariableCallNodes(DataflowAsset, OldVariableName, NewVariableName);
					return EChangeResult::Changed;
				}
				return EChangeResult::None;
			};

		ChangeDataflowAssetWithTransaction(DataflowAsset, TransactionName, SetVariableNameInternal, GET_MEMBER_NAME_CHECKED(UDataflow, Variables));
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////////////
	FName FEditAssetUtils::DuplicateVariable(UDataflow* DataflowAsset, FName VariableName)
	{
		using namespace EditAssetUtils::Private;

		const FText TransactionName = FText::Format(LOCTEXT("DuplicateDataflowVariable", "Duplicate Dataflow Variable: {0}"), FText::FromName(VariableName));
		
		FName NewVariableName;

		auto DuplicateVariableInternal =
			[VariableName, &NewVariableName](UDataflow& DataflowAsset) -> EChangeResult
			{
				if (const FPropertyBagPropertyDesc* PropertyDescPtr = DataflowAsset.Variables.FindPropertyDescByName(VariableName))
				{
					NewVariableName = GenerateUniqueVariableName(DataflowAsset, VariableName);

					// make sure the name is unique and the GUID is invalidated to avoid copying it as is 
					FPropertyBagPropertyDesc NewDesc(*PropertyDescPtr);
					NewDesc.Name = NewVariableName;
					NewDesc.ID.Invalidate();
					DataflowAsset.Variables.AddProperties(MakeConstArrayView(&NewDesc, 1));
					FDataflowAssetDelegates::OnVariablesChanged.Broadcast(&DataflowAsset, NewVariableName);
					return EChangeResult::Changed;
				}
				return EChangeResult::None;
			};

		ChangeDataflowAssetWithTransaction(DataflowAsset, TransactionName, DuplicateVariableInternal, GET_MEMBER_NAME_CHECKED(UDataflow, Variables));

		return NewVariableName;
	}


	//////////////////////////////////////////////////////////////////////////////////////////////////////////
	void FEditAssetUtils::SetVariableType(UDataflow* DataflowAsset, FName VariableName, const FEdGraphPinType& PinType)
	{
		using namespace EditAssetUtils::Private;

		const FText TransactionName{ LOCTEXT("ChangeDataflowVariableType", "Change Dataflow Variable Type") };

		auto SetVariableTypeInternal =
			[VariableName, &PinType](UDataflow& DataflowAsset) -> EChangeResult
			{
				auto ChangePropertyTypeLambda =
					[VariableName, &PinType](FPropertyBagPropertyDesc& PropertyDesc)
					{
						UE::StructUtils::SetPropertyDescFromPin(PropertyDesc, PinType);
					};

				const bool bModified = ModifyVariable(DataflowAsset, VariableName, ChangePropertyTypeLambda);
				return (bModified) ? EChangeResult::Changed : EChangeResult::None;
			};

		ChangeDataflowAssetWithTransaction(DataflowAsset, TransactionName, SetVariableTypeInternal, GET_MEMBER_NAME_CHECKED(UDataflow, Variables));
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////////////
	void FEditAssetUtils::SetVariableValue(UDataflow* DataflowAsset, FName VariableName, const FInstancedPropertyBag& SourceBag)
	{
		using namespace EditAssetUtils::Private;

		const FText TransactionName{ LOCTEXT("ChangeDataflowVariableValue", "Change Dataflow Variable Value") };

		auto SetVariableValueInternal =
			[VariableName, &SourceBag](UDataflow& DataflowAsset) -> EChangeResult
			{
				if (const FPropertyBagPropertyDesc* SourceDesc = SourceBag.FindPropertyDescByName(VariableName))
				{
					if (SourceDesc->CachedProperty)
					{
						EPropertyBagResult Result = DataflowAsset.Variables.SetValue(VariableName, SourceDesc->CachedProperty, SourceBag.GetValue().GetMemory());
						if (Result == EPropertyBagResult::Success)
						{
							FDataflowAssetDelegates::OnVariablesChanged.Broadcast(&DataflowAsset, VariableName);
							return EChangeResult::Changed;
						}
					}
				}
				return EChangeResult::None;
			};

		// do we need a transction in that case ? 
		ChangeDataflowAssetWithTransaction(DataflowAsset, TransactionName, SetVariableValueInternal, GET_MEMBER_NAME_CHECKED(UDataflow, Variables));
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////////////
	void FEditAssetUtils::CopyVariableToClipboard(UDataflow* DataflowAsset, FName VariableName)
	{
		using namespace EditAssetUtils::Private;

		if (DataflowAsset)
		{
			// no transaction needed in that case as we write to an external system 
			if (const FPropertyBagPropertyDesc* PropertyDescPtr = DataflowAsset->Variables.FindPropertyDescByName(VariableName))
			{
				FString ClipboardPayload;

				FPropertyBagPropertyDesc::StaticStruct()->ExportText(ClipboardPayload, PropertyDescPtr, PropertyDescPtr, nullptr, 0, nullptr, false);

				if (!ClipboardPayload.IsEmpty())
				{
					ClipboardPayload = DataflowVariableClipboardPrefix + ClipboardPayload;
					FPlatformApplicationMisc::ClipboardCopy(ClipboardPayload.GetCharArray().GetData());
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////////////
	FName FEditAssetUtils::PasteVariableFromClipboard(UDataflow* DataflowAsset)
	{
		using namespace EditAssetUtils::Private;

		FString ClipboardPayload;
		FPlatformApplicationMisc::ClipboardPaste(ClipboardPayload);

		if (!ensure(ClipboardPayload.StartsWith(DataflowVariableClipboardPrefix, ESearchCase::CaseSensitive)))
		{
			return NAME_None;
		}

		FStringOutputDevice Errors;
		const TCHAR* ImportPayload = ClipboardPayload.GetCharArray().GetData() + FCString::Strlen(DataflowVariableClipboardPrefix);

		FPropertyBagPropertyDesc PropertyDesc;
		FPropertyBagPropertyDesc::StaticStruct()->ImportText(ImportPayload, &PropertyDesc, nullptr, PPF_None, &Errors, FPropertyBagPropertyDesc::StaticStruct()->GetName());

		if (Errors.IsEmpty())
		{
			if (DataflowAsset)
			{
				// make sure the name is unique and the GUID is invalidated to avoid copying it as is 
				PropertyDesc.Name = GenerateUniqueVariableName(*DataflowAsset, PropertyDesc.Name);
				PropertyDesc.ID.Invalidate();

				const FText TransactionName = FText::Format(LOCTEXT("PasteDataflowVariable", "Paste Dataflow Variable: {0}"), FText::FromName(PropertyDesc.Name));

				auto PasteVariableInternal =
					[&PropertyDesc](UDataflow& DataflowAsset) -> EChangeResult
					{
						DataflowAsset.Variables.AddProperties(MakeConstArrayView(&PropertyDesc, 1));
						FDataflowAssetDelegates::OnVariablesChanged.Broadcast(&DataflowAsset, PropertyDesc.Name);
						return EChangeResult::Changed;
					};

				ChangeDataflowAssetWithTransaction(DataflowAsset, TransactionName, PasteVariableInternal, GET_MEMBER_NAME_CHECKED(UDataflow, Variables));
				return PropertyDesc.Name;
			}
		}
		return NAME_None;
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////////////
	// 
	// SUBGRAPHS API
	// 
	//////////////////////////////////////////////////////////////////////////////////////////////////////////
	FName FEditAssetUtils::AddNewSubGraph(UDataflow* DataflowAsset, FName BaseName)
	{
		using namespace EditAssetUtils::Private;

		const FText TransactionName{ LOCTEXT("AddNewDataflowSubGraph", "Add New Dataflow SubGraph") };

		FName UniqueSubGraphName;

		auto AddNewVariableInternal =
			[&BaseName, &UniqueSubGraphName](UDataflow& DataflowAsset) -> EChangeResult
			{
				UniqueSubGraphName = GenerateUniqueObjectName(&DataflowAsset, BaseName);
				UDataflowSubGraph* NewSubGraph = NewObject<UDataflowSubGraph>(&DataflowAsset, UniqueSubGraphName);
				ensure(NewSubGraph->GetFName().IsEqual(UniqueSubGraphName));
				NewSubGraph->Schema = UDataflowSchema::StaticClass();
				NewSubGraph->SetFlags(RF_Transactional);

				DataflowAsset.AddSubGraph(NewSubGraph);

				FDataflowAssetDelegates::OnSubGraphsChanged.Broadcast(&DataflowAsset, NewSubGraph->GetSubGraphGuid(), UE::Dataflow::ESubGraphChangedReason::Created);
				return EChangeResult::Changed;
			};

		ChangeDataflowAssetWithTransaction(DataflowAsset, TransactionName, AddNewVariableInternal, NAME_None);

		return UniqueSubGraphName;
	}

	void FEditAssetUtils::RenameSubGraph(UDataflow* DataflowAsset, FName OldSubGraphName, FName NewSubGraphName)
	{
		using namespace EditAssetUtils::Private;

		if (DataflowAsset && IsUniqueDataflowSubObjectName(DataflowAsset, NewSubGraphName))
		{
			if (UDataflowSubGraph* SubGraphToRename = DataflowAsset->FindSubGraphByName(OldSubGraphName))
			{
				const FText TransactionName{ LOCTEXT("RenameDataflowSubGraph", "Rename a Dataflow SubGraph") };

				auto RenameSubGraphInternal =
					[&SubGraphToRename, &NewSubGraphName](UDataflow& DataflowAsset) -> EChangeResult
					{
						if (SubGraphToRename->Rename(*NewSubGraphName.ToString()))
						{
							// rename the call nodes using it 
							RenameSubGraphCallNodes(DataflowAsset, SubGraphToRename->GetSubGraphGuid(), NewSubGraphName);

							FDataflowAssetDelegates::OnSubGraphsChanged.Broadcast(&DataflowAsset, SubGraphToRename->GetSubGraphGuid(), UE::Dataflow::ESubGraphChangedReason::Renamed);
							return EChangeResult::Changed;
						}
						return EChangeResult::Cancel;
					};

				ChangeDataflowAssetWithTransaction(DataflowAsset, TransactionName, RenameSubGraphInternal, NAME_None);
			}
		}
	}

	void FEditAssetUtils::DeleteSubGraph(UDataflow* DataflowAsset, FGuid SubGraphGuid)
	{
		using namespace EditAssetUtils::Private;

		if (DataflowAsset)
		{
			if (UDataflowSubGraph* SubGraphToDelete = DataflowAsset->FindSubGraphByGuid(SubGraphGuid))
			{
				const FText TransactionName{ LOCTEXT("DeleteDataflowSubGraph", "Delete a Dataflow SubGraph") };

				auto DeleteSubGraphInternal =
					[&SubGraphToDelete, SubGraphGuid](UDataflow& DataflowAsset) -> EChangeResult
					{
						FDataflowAssetDelegates::OnSubGraphsChanged.Broadcast(&DataflowAsset, SubGraphGuid, UE::Dataflow::ESubGraphChangedReason::Deleting);

						// make a copy so that we don't mofdy the array while iterating through it 
						const TArray<UEdGraphNode*> NodesToDelete = SubGraphToDelete->Nodes;
						DeleteNodesNoTransaction(SubGraphToDelete, NodesToDelete);

						// delete the Subgraph 
						DataflowAsset.RemoveSubGraph(SubGraphToDelete);

						FDataflowAssetDelegates::OnSubGraphsChanged.Broadcast(&DataflowAsset, SubGraphGuid, UE::Dataflow::ESubGraphChangedReason::Deleted);
						return EChangeResult::Changed;
					};

				ChangeDataflowAssetWithTransaction(DataflowAsset, TransactionName, DeleteSubGraphInternal, NAME_None);
			}
		}
	}

}

#undef LOCTEXT_NAMESPACE