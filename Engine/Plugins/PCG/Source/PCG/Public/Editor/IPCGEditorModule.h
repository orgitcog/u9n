// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Editor/IPCGEditorProgressNotification.h"
#include "Editor/PCGSyntaxTokenizerParams.h"
#include "Utils/PCGNodeVisualLogs.h"

#include "Internationalization/Text.h"
#include "Modules/ModuleInterface.h"

struct FPCGStackContext;
class UPCGComponent;

class IPCGEditorModule : public IModuleInterface
{
public:
	PCG_API static IPCGEditorModule* Get();

	virtual TWeakPtr<IPCGEditorProgressNotification> CreateProgressNotification(const FTextFormat& TextFormat, bool bCanCancel) = 0;
	virtual void ReleaseProgressNotification(TWeakPtr<IPCGEditorProgressNotification> InNotification) = 0;
	virtual void SetOutlinerUIRefreshDelay(float InDelay) = 0;
	
#if WITH_EDITOR
	virtual const FPCGNodeVisualLogs& GetNodeVisualLogs() const = 0;
	virtual FPCGNodeVisualLogs& GetNodeVisualLogsMutable() = 0;
	virtual bool CanSelectPartitionActors() const = 0;
	virtual TSharedPtr<class ISyntaxTokenizer> CreateHLSLSyntaxTokenizer(const FPCGSyntaxTokenizerParams& InParams) const = 0;
#endif

protected:
	friend class FPCGEditor;
	friend class FPCGGraphExecutor;
	friend class SPCGEditorGraphDebugObjectTree;
	friend class SPCGEditorGraphFind;
	friend class UPCGGraph;
	friend class UPCGSubsystem;

	virtual void OnScheduleGraph(const FPCGStackContext& StackContext) = 0;
	virtual void OnGraphPreSave(UPCGGraph* Graph, FObjectPreSaveContext ObjectSaveContext) = 0;

	/** Clear any data collected during execution, normally called prior to generating the component. */
	virtual void ClearExecutionMetadata(UPCGComponent* InComponent) = 0;

	virtual void ClearExecutedStacks(const UPCGComponent* InRootComponent) = 0;
	virtual void ClearExecutedStacks(const UPCGGraph* InContainingGraph) = 0;

	/** Get a list of stacks that were executed during the last execution. */
	virtual	TArray<FPCGStackSharedPtr> GetExecutedStacksPtrs(const FPCGStack& BeginningWithStack) = 0;
	virtual TArray<FPCGStackSharedPtr> GetExecutedStacksPtrs(const UPCGComponent* InComponent, const UPCGGraph* InSubgraph, bool bOnlyWithSubgraphAsCurrentFrame = true) = 0;

	virtual void NotifyGraphChanged(UPCGGraph* InGraph, EPCGChangeType ChangeType) = 0;

	PCG_API static void SetEditorModule(IPCGEditorModule* InModule);
};