// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataLinkExecutor.h"
#include "DataLinkEnums.h"
#include "DataLinkExecutorArguments.h"
#include "DataLinkGraph.h"
#include "DataLinkLog.h"
#include "DataLinkNode.h"
#include "DataLinkPinReference.h"
#include "DataLinkSink.h"
#include "Templates/SubclassOf.h"
#include "UObject/Class.h"

TSharedPtr<FDataLinkExecutor> FDataLinkExecutor::Create(FDataLinkExecutorArguments&& InArgs)
{
	TSharedRef<FDataLinkExecutor> Executor = MakeShared<FDataLinkExecutor>(FPrivateToken());

#if WITH_DATALINK_CONTEXT
	Executor->ContextName = MoveTemp(InArgs.ContextName);
#endif
	Executor->ContextObject = MoveTemp(InArgs.ContextObject);
	Executor->Instance = MoveTemp(InArgs.Instance);
	Executor->Sink = MoveTemp(InArgs.Sink);
	Executor->OnFinishDelegate = MoveTemp(InArgs.OnFinishDelegate);

	return Executor;
}

FDataLinkExecutor::FDataLinkExecutor(FPrivateToken)
	: ExecutorId(FGuid::NewGuid())
{
}

const FGuid& FDataLinkExecutor::GetExecutorId() const
{
	return ExecutorId;
}

FStringView FDataLinkExecutor::GetContextName() const
{
#if WITH_DATALINK_CONTEXT
	return ContextName;
#else
	return FStringView();
#endif
}

UObject* FDataLinkExecutor::GetContextObject() const
{
	return ContextObject;
}

FString FDataLinkExecutor::GetReferencerName() const
{
	return TEXT("FDataLinkExecutor");
}

void FDataLinkExecutor::AddReferencedObjects(FReferenceCollector& InCollector)
{
	InCollector.AddReferencedObject(ContextObject);
	InCollector.AddPropertyReferencesWithStructARO(FDataLinkInstance::StaticStruct(), &Instance);

	if (Sink.IsValid())
	{
		Sink->AddStructReferencedObjects(InCollector);
	}

	for (TPair<TObjectPtr<const UDataLinkNode>, FDataLinkNodeInstance>& Pair : NodeInstanceMap)
	{
		Pair.Value.AddReferencedObjects(InCollector);
	}
}

void FDataLinkExecutor::Run()
{
	bRunning = true;

	if (!ValidateRun())
	{
		Finish(EDataLinkExecutionResult::Failed, FConstStructView());
		return;
	}

	// Ensure there's a valid sink instance
	if (!Sink.IsValid())
	{
		Sink = MakeShared<FDataLinkSink>();
	}

	// Hold onto an extra shared reference in case the execution finishes immediately
	TSharedRef<FDataLinkExecutor> This = SharedThis(this);

	if (!ExecuteEntryNodes())
	{
		Finish(EDataLinkExecutionResult::Failed, FConstStructView());
	}
}

const FDataLinkNodeInstance& FDataLinkExecutor::GetNodeInstance(const UDataLinkNode* InNode) const
{
	return NodeInstanceMap.FindChecked(InNode);
}

FDataLinkNodeInstance& FDataLinkExecutor::GetNodeInstanceMutable(const UDataLinkNode* InNode)
{
	return NodeInstanceMap.FindChecked(InNode);
}

FDataLinkNodeInstance* FDataLinkExecutor::FindNodeInstanceMutable(const UDataLinkNode* InNode)
{
	return NodeInstanceMap.Find(InNode);
}

void FDataLinkExecutor::SucceedNode(const UDataLinkNode* InNode, FConstStructView InOutputDataView)
{
	if (!ProcessFinishedNode(InNode))
	{
		Finish(EDataLinkExecutionResult::Failed, FConstStructView());
		return;
	}

	TConstArrayView<FDataLinkPin> OutputPins = InNode->GetOutputPins();

	TArray<FDataLinkPinReference> InputPinsToExecute;
	InputPinsToExecute.Reserve(OutputPins.Num());

	for (const FDataLinkPin& OutputPin : OutputPins)
	{
		if (const FDataLinkPin* InputPin = OutputPin.GetLinkedInputPin())
		{
			// If the Linked Input Pin was gotten successfully, then it means that the Output Pin's Linked Node was valid
			check(OutputPin.LinkedNode);
			InputPinsToExecute.Emplace(OutputPin.LinkedNode, InputPin);
		}
	}

	// No next node, finish execution
	if (InputPinsToExecute.IsEmpty())
	{
		// If the next node is null, it should be the output node
		if (!ensureAlways(Instance.DataLinkGraph && Instance.DataLinkGraph->GetOutputNode() == InNode))
		{
			UE_LOG(LogDataLink, Error, TEXT("[%s] Node ('%s') has no node to go to and it's not an output node!"), GetContextName().GetData(), *InNode->GetName());
			Finish(EDataLinkExecutionResult::Failed, FConstStructView());
			return;
		}

		Finish(EDataLinkExecutionResult::Succeeded, InOutputDataView);
		return;
	}

	TArray<FConstStructView> InputDataViews;
	InputDataViews.Add(InOutputDataView);

	if (!ExecuteInputPins(InputPinsToExecute, MoveTemp(InputDataViews)))
	{
		Finish(EDataLinkExecutionResult::Failed, FConstStructView());
	}
}

void FDataLinkExecutor::FailNode(const UDataLinkNode* InNode)
{
	ProcessFinishedNode(InNode);

	UE_LOG(LogDataLink, Log, TEXT("[%s] Node ('%s') has failed to execute. Data Link execution finished."), GetContextName().GetData(), *InNode->GetName());
	Finish(EDataLinkExecutionResult::Failed, FConstStructView());
}

bool FDataLinkExecutor::ValidateRun()
{
	if (!Instance.DataLinkGraph)
	{
		UE_LOG(LogDataLink, Error, TEXT("[%s] Invalid Data Link specified!"), GetContextName().GetData());
		return false;
	}

	const int32 InputPinCount = Instance.DataLinkGraph->GetInputPinCount();

	if (Instance.InputData.Num() != InputPinCount)
	{
		UE_LOG(LogDataLink, Error, TEXT("[%s] DataLink Graph '%s' requires %d inputs but %d were provided")
			, GetContextName().GetData()
			, *Instance.DataLinkGraph->GetName()
			, InputPinCount
			, Instance.InputData.Num());
		return false;
	}

	int32 PinIndex = 0;

	// Ensure the input pins are compatible with the input data
	const bool bResult = Instance.DataLinkGraph->ForEachInputPin(
		[&PinIndex, this](FDataLinkPinReference InPinReference)->bool
		{
			const int32 CurrentPinIndex = PinIndex++;

			if (InPinReference.Pin->Struct && InPinReference.Pin->Struct != Instance.InputData[CurrentPinIndex].GetScriptStruct())
			{
				UE_LOG(LogDataLink, Error, TEXT("[%s] Input pin (%s': input struct '%s', owner '%s') is not compatible with input data view '%s'")
					, GetContextName().GetData()
					, *InPinReference.Pin->Name.ToString()
					, *InPinReference.Pin->Struct->GetName()
					, *InPinReference.OwningNode->GetName()
					, *GetNameSafe(Instance.InputData[CurrentPinIndex].GetScriptStruct()));
				return false;
			}

			return true;
		});

	return bResult;
}

bool FDataLinkExecutor::ValidateInputPins(TConstArrayView<FDataLinkPinReference> InInputPins, TConstArrayView<FConstStructView> InInputDataViews) const
{
	if (!ensureAlways(InInputPins.Num() == InInputDataViews.Num()))
	{
		UE_LOG(LogDataLink, Error, TEXT("[%s] Input Pin count %d does not match Input Data View %d")
			, GetContextName().GetData()
			, InInputPins.Num()
			, InInputDataViews.Num());
		return false;
	}

	for (int32 InputIndex = 0; InputIndex < InInputPins.Num(); ++InputIndex)
	{
		const FDataLinkPin* InputPin = InInputPins[InputIndex].Pin;
		check(InputPin);

		const FConstStructView& InputData = InInputDataViews[InputIndex];
		if (InputPin->Struct && InputPin->Struct != InputData.GetScriptStruct())
		{
			UE_LOG(LogDataLink, Error, TEXT("[%s] Input Data struct '%s' does not match expected input pin '%s' of type '%s'")
				, GetContextName().GetData()
				, *GetNameSafe(InputData.GetScriptStruct())
				, *InputPin->Name.ToString()
				, *InputPin->Struct->GetName());
			return false;
		}
	}

	return true;
}

bool FDataLinkExecutor::ExecuteEntryNodes()
{
	const TArray<FDataLinkPinReference> InputPinsToExecute = Instance.DataLinkGraph->GetInputPins();

	uint16 NodesExecuted = 0;
	if (!ExecuteInputPins(InputPinsToExecute, TArray<FConstStructView>(Instance.InputData), &NodesExecuted))
	{
		return false;
	}

	// Execute Entry nodes with no input pins (i.e. without dependency) as these were not included in the initial input pins to execute
	for (const UDataLinkNode* InputNode : Instance.DataLinkGraph->GetInputNodes())
	{
		if (InputNode->GetInputPins().IsEmpty())
		{
			// Ensure there's a Node Instance to this Input Node
			FindOrAddNodeInstance(InputNode);

			if (!ExecuteNode(*InputNode))
			{
				return false;
			}

			++NodesExecuted;
		}
	}

	return NodesExecuted > 0;
}

bool FDataLinkExecutor::ExecuteInputPins(TConstArrayView<FDataLinkPinReference> InInputPins, TArray<FConstStructView>&& InInputDataViews, uint16* OutNodesExecuted)
{
	if (!ValidateInputPins(InInputPins, InInputDataViews))
	{
		return false;
	}

	// Struct bundling the pin and the data that will be fed into the pin
	struct FPinData
	{
		const FDataLinkPin* Pin;
		FConstStructView DataView;
	};

	// Struct bundling the node and the pin data that will be fed into the node
	struct FNodeData
	{
		bool operator==(const UDataLinkNode* InNode) const
		{
			return InNode == Node;
		}
		const UDataLinkNode* Node = nullptr;
		TArray<FPinData> PinData;
	};

	TArray<FNodeData> NodeData;

	// Group the pins per node
	for (int32 Index = 0; Index < InInputPins.Num(); ++Index)
	{
		const FDataLinkPinReference& InputPinReference = InInputPins[Index];
		check(InputPinReference.Pin);

		FNodeData* NodeDataEntry = NodeData.FindByKey(InputPinReference.OwningNode);
		if (!NodeDataEntry)
		{
			NodeDataEntry = &NodeData.AddDefaulted_GetRef();
			NodeDataEntry->Node = InputPinReference.OwningNode;
		}

		FPinData& PinDataEntry = NodeDataEntry->PinData.AddDefaulted_GetRef();
		PinDataEntry.Pin = InputPinReference.Pin;
		PinDataEntry.DataView = InInputDataViews[Index];
	}

	// Set the Node Instance Input Data Views with the corresponding Input Data Views
	// This does not guarantee that all the input pins have their data filled.
	// It could be that there's still data not ready yet.
	for (const FNodeData& NodeDataEntry : NodeData)
	{
		FDataLinkInputDataViewer& InputDataViewer = FindOrAddNodeInstance(NodeDataEntry.Node).InputDataViewer;

		// Copy the input data view to the node instance
		for (const FPinData& PinData : NodeDataEntry.PinData)
		{
			InputDataViewer.SetEntryData(*PinData.Pin, PinData.DataView);
		}
	}

	uint16 NodesExecuted = 0;

	// Execute nodes that have all their data ready to go
	for (const FNodeData& NodeInfo : NodeData)
	{
		FDataLinkNodeInstance& NodeInstance = GetNodeInstanceMutable(NodeInfo.Node);

		// don't execute nodes that don't have their input data views fully set yet
		if (NodeInstance.InputDataViewer.HasInvalidDataEntry())
		{
			continue;
		}

		if (!ExecuteNode(*NodeInfo.Node))
		{
			return false;
		}

		++NodesExecuted;
	}

	if (OutNodesExecuted)
	{
		*OutNodesExecuted = NodesExecuted;
	}
	return true;
}

bool FDataLinkExecutor::ExecuteNode(const UDataLinkNode& InNode)
{
	FDataLinkNodeInstance& NodeInstance = GetNodeInstanceMutable(&InNode);
	NodeInstance.Status = EDataLinkNodeStatus::Executing;
	InNode.Execute(*this);
	return true;
}

FDataLinkNodeInstance& FDataLinkExecutor::FindOrAddNodeInstance(const UDataLinkNode* InNode)
{
	check(InNode);
	FDataLinkNodeInstance* NodeInstance = NodeInstanceMap.Find(InNode);
	if (!NodeInstance)
	{
		NodeInstance = &NodeInstanceMap.Add(InNode, FDataLinkNodeInstance(*InNode));
	}
	return *NodeInstance;
}

bool FDataLinkExecutor::ProcessFinishedNode(const UDataLinkNode* InNode)
{
	FDataLinkNodeInstance* const NodeInstance = FindNodeInstanceMutable(InNode);
	if (!ensureAlwaysMsgf(NodeInstance, TEXT("[%.*s] FinishNode called on a node that no longer has node instance data!"), GetContextName().Len(), GetContextName().GetData()))
	{
		return false;
	}

	const bool bWasExecuting = NodeInstance->Status == EDataLinkNodeStatus::Executing;
	if (!ensureAlwaysMsgf(bWasExecuting, TEXT("[%.*s] FinishNode called on a node that was not executing!"), GetContextName().Len(), GetContextName().GetData()))
	{
		return false;
	}

	NodeInstance->Status = EDataLinkNodeStatus::Finished;
	return true;
}

void FDataLinkExecutor::Finish(EDataLinkExecutionResult InStatus, FConstStructView InOutputData)
{
	// Finish has already been called
	if (!bRunning)
	{
		return;
	}

	// Hold onto an extra shared reference to allow implementers to reset their held shared reference in the finish delegate
	// without destroying this
	TSharedRef<FDataLinkExecutor> This = SharedThis(this);

	OnFinishDelegate.ExecuteIfBound(*this, InStatus, InOutputData);
	NodeInstanceMap.Reset();
	Sink.Reset();

	bRunning = false;
}
