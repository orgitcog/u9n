// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DataLinkEdGraph.h"

class UDataLinkEdNode;
class UDataLinkGraph;
class UDataLinkNode;
enum class EDataLinkGraphCompileStatus : uint8;

class FDataLinkGraphCompiler
{
public:
	explicit FDataLinkGraphCompiler(UDataLinkGraph* InDataLinkGraph);

	EDataLinkGraphCompileStatus Compile();

private:
	void CleanExistingGraph();

	bool CompileNodes();

	bool CreateCompiledNodes();

	UDataLinkNode* CompileNode(UDataLinkNode* InTemplateNode);

	void LinkNodes();

	bool SetInputOutputNodes();

	void AddGraphEntryNodes(const UDataLinkNode* InNode);

	UDataLinkNode* FindCompiledNode(const UDataLinkEdNode* InEdNode) const;

	TObjectPtr<UDataLinkGraph> DataLinkGraph;

	TObjectPtr<UDataLinkEdGraph> DataLinkEdGraph;

	TObjectPtr<const UDataLinkEdNode> OutputEdNode;

	/** Map of the Editor Node to its Compiled Node */
	TMap<const UDataLinkEdNode*, UDataLinkNode*> EdToCompiledMap;
};
