// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EdGraph/EdGraph.h"

#include "DataflowSubGraph.generated.h"

struct FDataflowSubGraphInputNode;
struct FDataflowSubGraphOutputNode;

class UDataflowSubGraph;

struct FDataflowSubGraphDelegates
{
	/** Called when variables are edited ( add, remove, changetype , setvalue ) */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnDataflowSubGraphLoaded, const UDataflowSubGraph&);
	static DATAFLOWENGINE_API FOnDataflowSubGraphLoaded OnSubGraphLoaded;
};

UCLASS(MinimalAPI)
class UDataflowSubGraph : public UEdGraph
{
	GENERATED_UCLASS_BODY()

public:
	FDataflowSubGraphInputNode* GetInputNode() const;
	FDataflowSubGraphOutputNode* GetOutputNode() const;

	DATAFLOWENGINE_API bool IsForEachSubGraph() const;
	DATAFLOWENGINE_API void SetForEachSubGraph(bool bValue);

	bool IsLoaded() const { return bIsLoaded; }
	const FGuid& GetSubGraphGuid() const { return SubGraphGuid; }

private:
	virtual void PostLoad() override;

	/**
	* Guid that uniquely identify this SubGraph
	* this allow to identify the SubGraph across renames for example 
	*/
	UPROPERTY()
	FGuid SubGraphGuid; 

	UPROPERTY()
	bool bIsForEach = false;

	bool bIsLoaded : 1 = false;
};