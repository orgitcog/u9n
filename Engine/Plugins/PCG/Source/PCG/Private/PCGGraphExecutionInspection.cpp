// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGGraphExecutionInspection.h"

#if WITH_EDITOR

#include "PCGInputOutputSettings.h"
#include "PCGPin.h"
#include "PCGSettings.h"
#include "PCGSubgraph.h"

bool FPCGGraphExecutionInspection::IsInspecting() const
{
	return InspectionCounter > 0;
}

void FPCGGraphExecutionInspection::EnableInspection()
{
	if (!ensure(InspectionCounter >= 0))
	{
		InspectionCounter = 0;
	}

	InspectionCounter++;
}

void FPCGGraphExecutionInspection::DisableInspection()
{
	if (ensure(InspectionCounter > 0))
	{
		InspectionCounter--;
	}

	if (InspectionCounter == 0)
	{
		ClearInspectionData(/*bClearPerNodeExecutionData=*/false);
	}
};

void FPCGGraphExecutionInspection::NotifyNodeExecuted(const UPCGNode* InNode, const FPCGStack* InStack, const PCGUtils::FCallTime* InTimer, bool bNodeUsedCache)
{
	if (!ensure(InStack && InNode))
	{
		return;
	}

	// Reset timer information if taken from cache to provide good info in the profiling window
	PCGUtils::FCallTime Timer;
	if (InTimer && !bNodeUsedCache)
	{
		Timer = *InTimer;
	}

	FWriteScopeLock Lock(NodeToStacksInWhichNodeExecutedLock);
	NodeToStacksInWhichNodeExecuted.FindOrAdd(InNode).Add(FPCGGraphExecutionInspection::FNodeExecutedNotificationData(*InStack, MoveTemp(Timer)));
}

TMap<TObjectKey<const UPCGNode>, TSet<FPCGGraphExecutionInspection::FNodeExecutedNotificationData>> FPCGGraphExecutionInspection::GetExecutedNodeStacks() const
{
	FReadScopeLock Lock(NodeToStacksInWhichNodeExecutedLock);
	return NodeToStacksInWhichNodeExecuted;
}

uint64 FPCGGraphExecutionInspection::GetNodeInactivePinMask(const UPCGNode* InNode, const FPCGStack& Stack) const
{
	FReadScopeLock Lock(NodeToStackToInactivePinMaskLock);

	if (const TMap<const FPCGStack, uint64>* StackToMask = NodeToStackToInactivePinMask.Find(InNode))
	{
		if (const uint64* Mask = StackToMask->Find(Stack))
		{
			return *Mask;
		}
	}

	return 0;
}

void FPCGGraphExecutionInspection::NotifyNodeDynamicInactivePins(const UPCGNode* InNode, const FPCGStack* InStack, uint64 InactivePinBitmask) const
{
	if (!ensure(InStack && InNode))
	{
		return;
	}

	FWriteScopeLock Lock(NodeToStackToInactivePinMaskLock);
	TMap<const FPCGStack, uint64>& StackToInactivePinMask = NodeToStackToInactivePinMask.FindOrAdd(InNode);
	StackToInactivePinMask.FindOrAdd(*InStack) = InactivePinBitmask;
}

bool FPCGGraphExecutionInspection::WasNodeExecuted(const UPCGNode* InNode, const FPCGStack& Stack) const
{
	FReadScopeLock Lock(NodeToStacksInWhichNodeExecutedLock);
	const TSet<FPCGGraphExecutionInspection::FNodeExecutedNotificationData>* FoundNotifications = NodeToStacksInWhichNodeExecuted.Find(InNode);

	// Since the operator== & hash functions don't rely on the timer, we can just build a stub from the stack.
	FPCGGraphExecutionInspection::FNodeExecutedNotificationData NotificationStub(Stack, PCGUtils::FCallTime());
	return FoundNotifications && FoundNotifications->Contains(NotificationStub);
}

void FPCGGraphExecutionInspection::StoreInspectionData(const FPCGStack* InStack, const UPCGNode* InNode, const PCGUtils::FCallTime* InTimer, const FPCGDataCollection& InInputData, const FPCGDataCollection& InOutputData, bool bUsedCache)
{
	if (!InNode || !ensure(InStack))
	{
		return;
	}

	// Notify component that this task executed. Useful for editor visualization.
	NotifyNodeExecuted(InNode, InStack, InTimer, bUsedCache);

	if (!InOutputData.TaggedData.IsEmpty())
	{
		FWriteScopeLock Lock(NodeToStacksThatProducedDataLock);

		NodeToStacksThatProducedData.FindOrAdd(InNode).Add(*InStack);
	}
	else
	{
		FWriteScopeLock Lock(NodeToStacksThatProducedDataLock);

		if (TSet<FPCGStack>* Stacks = NodeToStacksThatProducedData.Find(InNode))
		{
			Stacks->Remove(*InStack);
		}
	}

	if (IsInspecting())
	{
		InInputData.MarkUsage(EPCGDataUsage::ComponentInspectionData);
		InOutputData.MarkUsage(EPCGDataUsage::ComponentInspectionData);

		auto StorePinInspectionDataFromNode = [](const FPCGStack* InStack, const TArray<TObjectPtr<UPCGPin>>& InPins, const FPCGDataCollection& InData, TMap<FPCGStack, FPCGDataCollection>& InOutInspectionCache)
		{
			for (const UPCGPin* Pin : InPins)
			{
				FPCGStack Stack = *InStack;

				// Append the Node and Pin to the current Stack to uniquely identify each DataCollection
				TArray<FPCGStackFrame>& StackFrames = Stack.GetStackFramesMutable();
				StackFrames.Emplace(Pin);

				FPCGDataCollection PinDataCollection;
				InData.GetInputsAndCrcsByPin(Pin->Properties.Label, PinDataCollection.TaggedData, PinDataCollection.DataCrcs);

				// Implementation note: since static subgraphs actually are visited twice and the second time the input doesn't match the input pins, we don't clear the data.
				if (!PinDataCollection.TaggedData.IsEmpty())
				{
					if (FPCGDataCollection* CollectionInCache = InOutInspectionCache.Find(Stack))
					{
						CollectionInCache->TaggedData.Append(PinDataCollection.TaggedData);
					}
					else
					{
						InOutInspectionCache.Add(Stack, PinDataCollection);
					}
				}
			}
		};

		auto StorePinInspectionData = [InStack, InNode, &StorePinInspectionDataFromNode](const TArray<TObjectPtr<UPCGPin>>& InPins, const FPCGDataCollection& InData, TMap<FPCGStack, FPCGDataCollection>& InOutInspectionCache)
		{
			FPCGStack Stack = *InStack;

			// Append the Node (here) and Pin (in call) to the current Stack to uniquely identify each DataCollection
			TArray<FPCGStackFrame>& StackFrames = Stack.GetStackFramesMutable();
			StackFrames.Reserve(StackFrames.Num() + 2);
			StackFrames.Emplace(InNode);

			StorePinInspectionDataFromNode(&Stack, InPins, InData, InOutInspectionCache);
		};

		FWriteScopeLock Lock(InspectionCacheLock);

		// Special case: if we have a static (embedded) subgraph, then the actual data inputs (not params) of the subgraph will be on the input node.
		// Considering we don't allow inspection on input pins of the input node, then we can move that data up the chain.
		if (InNode->GetSettings()->IsA<UPCGGraphInputOutputSettings>() && InStack->GetStackFrames().Num() > 2)
		{
			// We're expecting the last frame to be the graph
			// Then, if the graph was statically dispatched, it will be the subgraph node.
			// In the case of a dynamic subgraph or loop, it will be the loop index instead.
			FPCGStack StackToSubgraphNode = *InStack;
			TArray<FPCGStackFrame>& StackFrames = StackToSubgraphNode.GetStackFramesMutable();
			StackFrames.Pop();

			if (const UPCGSubgraphNode* Node = StackFrames.Last().GetObject_AnyThread<UPCGSubgraphNode>())
			{
				StorePinInspectionDataFromNode(&StackToSubgraphNode, Node->GetInputPins(), InInputData, InspectionCache);
			}
		}

		StorePinInspectionData(InNode->GetInputPins(), InInputData, InspectionCache);
		StorePinInspectionData(InNode->GetOutputPins(), InOutputData, InspectionCache);
	}
}

const FPCGDataCollection* FPCGGraphExecutionInspection::GetInspectionData(const FPCGStack& InStack) const
{
	FReadScopeLock Lock(InspectionCacheLock);
	return InspectionCache.Find(InStack);
}

void FPCGGraphExecutionInspection::ClearInspectionData(bool bClearPerNodeExecutionData)
{
	{
		FWriteScopeLock Lock(InspectionCacheLock);

		for (TPair<FPCGStack, FPCGDataCollection>& Entry : InspectionCache)
		{
			Entry.Value.ClearUsage(EPCGDataUsage::ComponentInspectionData);
		}

		InspectionCache.Reset();
	}

	if (bClearPerNodeExecutionData)
	{
		{
			FWriteScopeLock Lock(NodeToStacksThatProducedDataLock);
			NodeToStacksThatProducedData.Reset();
		}

		{
			FWriteScopeLock Lock(NodeToStacksInWhichNodeExecutedLock);
			NodeToStacksInWhichNodeExecuted.Reset();
		}

		{
			FWriteScopeLock Lock(NodeToStackToInactivePinMaskLock);
			NodeToStackToInactivePinMask.Reset();
		}

		{
			FWriteScopeLock Lock(NodeToStacksTriggeringGPUTransfersLock);
			NodeToStacksTriggeringGPUUploads.Reset();
			NodeToStacksTriggeringGPUReadbacks.Reset();
		}
	}
}

bool FPCGGraphExecutionInspection::HasNodeProducedData(const UPCGNode* InNode, const FPCGStack& Stack) const
{
	FReadScopeLock Lock(NodeToStacksThatProducedDataLock);

	const TSet<FPCGStack>* StacksThatProducedData = NodeToStacksThatProducedData.Find(InNode);

	return StacksThatProducedData && StacksThatProducedData->Contains(Stack);
}

void FPCGGraphExecutionInspection::NotifyGPUToCPUReadback(const UPCGNode* InNode, const FPCGStack* InStack) const
{
	if (!ensure(InNode) || !ensure(InStack))
	{
		return;
	}

	FWriteScopeLock Lock(NodeToStacksTriggeringGPUTransfersLock);
	NodeToStacksTriggeringGPUReadbacks.FindOrAdd(InNode).Add(*InStack);
}

void FPCGGraphExecutionInspection::NotifyCPUToGPUUpload(const UPCGNode* InNode, const FPCGStack* InStack) const
{
	if (!ensure(InNode) || !ensure(InStack))
	{
		return;
	}

	FWriteScopeLock Lock(NodeToStacksTriggeringGPUTransfersLock);
	NodeToStacksTriggeringGPUUploads.FindOrAdd(InNode).Add(*InStack);
}

bool FPCGGraphExecutionInspection::DidNodeTriggerGPUToCPUReadback(const UPCGNode* InNode, const FPCGStack& Stack) const
{
	FReadScopeLock Lock(NodeToStacksTriggeringGPUTransfersLock);
	const TSet<FPCGStack>* FoundStacks = NodeToStacksTriggeringGPUReadbacks.Find(InNode);
	return FoundStacks && FoundStacks->Contains(Stack);
}

bool FPCGGraphExecutionInspection::DidNodeTriggerCPUToGPUUpload(const UPCGNode* InNode, const FPCGStack& Stack) const
{
	FReadScopeLock Lock(NodeToStacksTriggeringGPUTransfersLock);
	const TSet<FPCGStack>* FoundStacks = NodeToStacksTriggeringGPUUploads.Find(InNode);
	return FoundStacks && FoundStacks->Contains(Stack);
}

void FPCGGraphExecutionInspection::AddReferencedObjects(FReferenceCollector& Collector)
{
	for (auto& Pair : InspectionCache)
	{
		Pair.Value.AddReferences(Collector);
	}
}

#endif // WITH_EDITOR