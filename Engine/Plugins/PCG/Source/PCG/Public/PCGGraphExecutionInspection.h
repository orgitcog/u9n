// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGCommon.h"
#include "PCGData.h"
#include "PCGNode.h"
#include "Graph/PCGStackContext.h"
#include "Utils/PCGExtraCapture.h"

#include "UObject/ObjectKey.h"

#define UE_API PCG_API

#if WITH_EDITOR
class FPCGGraphExecutionInspection
{
public:
	friend struct FPCGComponentInstanceData;
	friend class UPCGComponent;

	/* Retrieves the executed nodes information */
	struct FNodeExecutedNotificationData
	{
		FNodeExecutedNotificationData(const FPCGStack& InStack, const PCGUtils::FCallTime& InTimer) : Stack(InStack), Timer(InTimer) {}
		// Important implementation note: some logic in WasNodeExecuted relies on the fact we don't use the timer for the operator== and hash functions.
		friend PCG_API uint32 GetTypeHash(const FNodeExecutedNotificationData& NotifData) { return GetTypeHash(NotifData.Stack); }
		bool operator==(const FNodeExecutedNotificationData& OtherNotifData) const { return Stack == OtherNotifData.Stack; }

		FPCGStack Stack;
		PCGUtils::FCallTime Timer;
	};

	UE_API bool IsInspecting() const;
	UE_API void EnableInspection();
	UE_API void DisableInspection();
	UE_API void StoreInspectionData(const FPCGStack* InStack, const UPCGNode* InNode, const PCGUtils::FCallTime* InTimer, const FPCGDataCollection& InInputData, const FPCGDataCollection& InOutputData, bool bUsedCache);
	UE_API const FPCGDataCollection* GetInspectionData(const FPCGStack& InStack) const;
	UE_API void ClearInspectionData(bool bClearPerNodeExecutionData = true);

	/** Whether a task for the given node and stack was executed during the last execution. */
	UE_API bool WasNodeExecuted(const UPCGNode* InNode, const FPCGStack& Stack) const;

	/** Called at execution time each time a node has been executed. */
	UE_API void NotifyNodeExecuted(const UPCGNode* InNode, const FPCGStack* InStack, const PCGUtils::FCallTime* InTimer, bool bNodeUsedCache);
		
	UE_API TMap<TObjectKey<const UPCGNode>, TSet<FNodeExecutedNotificationData>> GetExecutedNodeStacks() const;

	/** Retrieve the inactive pin bitmask for the given node and stack in the last execution. */
	UE_API uint64 GetNodeInactivePinMask(const UPCGNode* InNode, const FPCGStack& Stack) const;

	/** Whether the given node was culled by a dynamic branch in the given stack. */
	UE_API void NotifyNodeDynamicInactivePins(const UPCGNode* InNode, const FPCGStack* InStack, uint64 InactivePinBitmask) const;

	/** Did the given node produce one or more data items in the given stack during the last execution. */
	UE_API bool HasNodeProducedData(const UPCGNode* InNode, const FPCGStack& Stack) const;

	UE_API void NotifyGPUToCPUReadback(const UPCGNode* InNode, const FPCGStack* InStack) const;
	UE_API void NotifyCPUToGPUUpload(const UPCGNode* InNode, const FPCGStack* InStack) const;

	UE_API bool DidNodeTriggerGPUToCPUReadback(const UPCGNode* InNode, const FPCGStack& Stack) const;
	UE_API bool DidNodeTriggerCPUToGPUUpload(const UPCGNode* InNode, const FPCGStack& Stack) const;

private:
	UE_API void AddReferencedObjects(FReferenceCollector& Collector);

	int32 InspectionCounter = 0;

	TMap<FPCGStack, FPCGDataCollection> InspectionCache;

	mutable FRWLock InspectionCacheLock;

	/** Map from nodes to all stacks for which the node produced at least one data item. */
	TMap<TObjectKey<const UPCGNode>, TSet<FPCGStack>> NodeToStacksThatProducedData;
	mutable FRWLock NodeToStacksThatProducedDataLock;

	/** Map from nodes to all stacks for which a task for the node was executed. */
	TMap<TObjectKey<const UPCGNode>, TSet<FNodeExecutedNotificationData>> NodeToStacksInWhichNodeExecuted;
	mutable FRWLock NodeToStacksInWhichNodeExecutedLock;

	/** Map from nodes to stacks to mask of output pins that were deactivated during execution. */
	mutable TMap<TObjectKey<const UPCGNode>, TMap<const FPCGStack, uint64>> NodeToStackToInactivePinMask;
	mutable FRWLock NodeToStackToInactivePinMaskLock;

	/** Map from nodes to all stacks for which GPU data transfers occurred. */
	mutable TMap<TObjectKey<const UPCGNode>, TSet<FPCGStack>> NodeToStacksTriggeringGPUUploads;
	mutable TMap<TObjectKey<const UPCGNode>, TSet<FPCGStack>> NodeToStacksTriggeringGPUReadbacks;
	mutable FRWLock NodeToStacksTriggeringGPUTransfersLock;
};
#endif

#undef UE_API
