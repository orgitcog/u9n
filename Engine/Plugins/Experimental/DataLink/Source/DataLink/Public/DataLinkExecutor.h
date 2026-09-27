// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DataLinkDelegates.h"
#include "DataLinkInstance.h"
#include "DataLinkNodeInstance.h"
#include "DataLinkSink.h"
#include "StructUtils/InstancedStruct.h"

class FDataLinkExecutorArguments;
class UDataLinkGraph;
class UDataLinkNode;
struct FDataLinkPinReference;

/**
 * Executes a Data Link graph with custom instance data.
 * @see FDataLinkExecutor::Run
 */
class FDataLinkExecutor : public FGCObject, public TSharedFromThis<FDataLinkExecutor>
{
	friend class FDataLinkExecutorArguments;

	struct FPrivateToken { explicit FPrivateToken() = default; };

public:
	DATALINK_API static TSharedPtr<FDataLinkExecutor> Create(FDataLinkExecutorArguments&& InArgs);

	explicit FDataLinkExecutor(FPrivateToken);

	/** Gets the unique identifier for this Executor */
	DATALINK_API const FGuid& GetExecutorId() const;

	/**
	 * Gets a brief description of the context for debugging purposes.
	 * Returns a valid context name when WITH_DATALINK_CONTEXT is true.
	 * Note: WITH_DATALINK_CONTEXT is true in builds with logging enabled.
	 */
	DATALINK_API FStringView GetContextName() const;

	/** Gets the object that is responsible for this execution */
	DATALINK_API UObject* GetContextObject() const;

	/** Called to start the graph execution */
	DATALINK_API void Run();

	/** Retrieves the node instance data of this execution for the given node */
	DATALINK_API const FDataLinkNodeInstance& GetNodeInstance(const UDataLinkNode* InNode) const;

	/** Retrieves the node instance data (mutable) of this execution for the given node */
	DATALINK_API FDataLinkNodeInstance& GetNodeInstanceMutable(const UDataLinkNode* InNode);

	/** Finds the node instance data (mutable) of this execution for the given node, or null if already removed */
	DATALINK_API FDataLinkNodeInstance* FindNodeInstanceMutable(const UDataLinkNode* InNode);

	/**
	 * Called when a node has finished executing successfully
	 * If it's the output node that has finished executing, it will finish the entire graph execution
	 * @param InNode the node that finished execution
	 * @param InOutputDataView the data view of the output (if succeeded)
	 */
	DATALINK_API void SucceedNode(const UDataLinkNode* InNode, FConstStructView InOutputDataView);

	/**
	 * Called when a node has failed to execute and produce a valid output
	 * This stops the entire graph execution
	 * @param InNode the node that failed
	 */
	DATALINK_API void FailNode(const UDataLinkNode* InNode);

private:
	/** Called when running to ensure all the parameters are properly set. Returns true if run can be done, false otherwise */
	bool ValidateRun();

	/** Called in Pin execution to make sure the pins and data views match in compatibility */
	bool ValidateInputPins(TConstArrayView<FDataLinkPinReference> InInputPins, TConstArrayView<FConstStructView> InInputDataViews) const;

	/** Called to execute the graph's input nodes */
	bool ExecuteEntryNodes();

	/**
	 * Called to execute the given input pins with the input data
	 * @param InInputPins the input pins to execute (can belong to different nodes)
	 * @param InInputDataViews the matching data views for each input pin
	 * @param OutNodesExecuted the number of nodes that got executed
	 * @return true if the input pins were processed without error
	 */
	bool ExecuteInputPins(TConstArrayView<FDataLinkPinReference> InInputPins, TArray<FConstStructView>&& InInputDataViews, uint16* OutNodesExecuted = nullptr);

	bool ExecuteNode(const UDataLinkNode& InNode);

	FDataLinkNodeInstance& FindOrAddNodeInstance(const UDataLinkNode* InNode);

	/** Called when a node has finished */
	bool ProcessFinishedNode(const UDataLinkNode* InNode);

	/** Called when the entire graph execution has completed */
	void Finish(EDataLinkExecutionResult InStatus, FConstStructView InOutputData);

	//~ Begin FGCObject
	virtual FString GetReferencerName() const override;
	virtual void AddReferencedObjects(FReferenceCollector& InCollector) override;
	//~ End FGCObject

	/** Unique Identifier for this Executor */
	const FGuid ExecutorId;

#if WITH_DATALINK_CONTEXT
	/** Context string for additional information when logging */
	FString ContextName;
#endif

	/** Object responsible for this execution */
	TObjectPtr<UObject> ContextObject;

	/** The data link instance of this execution */
	FDataLinkInstance Instance;

	/** Sink where all the data for this execution is queried and stored */
	TSharedPtr<FDataLinkSink> Sink;

	/** Delegate to call when the execution completes */
	FOnDataLinkExecutionFinished OnFinishDelegate;

	/** Map of a Node to its Instance (Input, Output and Instance Data) for this execution */
	mutable TMap<TObjectPtr<const UDataLinkNode>, FDataLinkNodeInstance> NodeInstanceMap;

	bool bRunning = false;
};
