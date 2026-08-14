// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Pipeline/Connection.h"
#include "Pipeline/DataTreeTypes.h"
#include "HAL/IConsoleManager.h"

class UMetaHumanPipelineProcess;

namespace UE::MetaHuman::Pipeline
{

METAHUMANPIPELINECORE_API extern TAutoConsoleVariable<bool> CVarBalancedGPUSelection;

class FNode;
class FPipelineData;

DECLARE_MULTICAST_DELEGATE_OneParam(FFrameComplete, TSharedPtr<FPipelineData> InPipelineData);
DECLARE_MULTICAST_DELEGATE_OneParam(FProcessComplete, TSharedPtr<FPipelineData> InPipelineData);

enum class EPipelineMode
{
	PushSync = 0,
	PushAsync,
	PushSyncNodes,
	PushAsyncNodes,
	Pull
};

class METAHUMANPIPELINECORE_API FPipelineRunParameters
{
public:

	FPipelineRunParameters() = default;

	void SetMode(EPipelineMode InMode);
	EPipelineMode GetMode() const;

	void SetOnFrameComplete(const FFrameComplete& InOnFrameComplete);
	const FFrameComplete& GetOnFrameComplete() const;

	void SetOnProcessComplete(const FProcessComplete& InOnProcessComplete);
	const FProcessComplete& GetOnProcessComplete() const;

	void SetStartFrame(int32 InStartFrame);
	int32 GetStartFrame() const;

	void SetEndFrame(int32 InEndFrame);
	int32 GetEndFrame() const;

	void SetRestrictStartingToGameThread(bool bInRestrictStartingToGameThread);
	bool GetRestrictStartingToGameThread() const;

	void SetProcessNodesInRandomOrder(bool bInProcessNodesInRandomOrder);
	bool GetProcessNodesInRandomOrder() const;

	void SetCheckThreadLimit(bool bInCheckThreadLimit);
	bool GetCheckThreadLimit() const;

	void SetCheckProcessingSpeed(bool bInCheckProcessingSpeed);
	bool GetCheckProcessingSpeed() const;

	void SetVerbosity(ELogVerbosity::Type InVerbosity);
	ELogVerbosity::Type GetVerbosity() const;

	void SetGpuToUse(const FString& InUseGPU);
	void UnsetGpuToUse();
	TOptional<FString> GetGpuToUse() const;

private:

	EPipelineMode Mode = EPipelineMode::PushAsync;

	FFrameComplete OnFrameComplete;
	FProcessComplete OnProcessComplete;

	int32 StartFrame = 0;
	int32 EndFrame = -1;

	bool bRestrictStartingToGameThread = true;
	bool bProcessNodesInRandomOrder = true;
	bool bCheckThreadLimit = true;
	bool bCheckProcessingSpeed = true;

	ELogVerbosity::Type Verbosity = ELogVerbosity::Display;

	TOptional<FString> UseGPU;

	// potentially other termination conditions here like timeouts
};

class METAHUMANPIPELINECORE_API FPipeline
{
public:

	FPipeline();
	~FPipeline();

	// Make class non-copyable - copying is not supported due to how the Process member is protected from garbage collection
	FPipeline(const FPipeline& InOther) = delete;
	FPipeline(FPipeline&& InOther) = delete;
	FPipeline& operator=(const FPipeline& InOther) = delete;

	void Reset();

	template<typename T, typename... InArgTypes>
	TSharedPtr<T> MakeNode(InArgTypes&&... InArgs)
	{
		TSharedPtr<T> Node = MakeShared<T>(Forward<InArgTypes>(InArgs)...);
		Nodes.Add(Node);
		return Node;
	}

	void AddNode(const TSharedPtr<FNode>& InNode);

	void MakeConnection(const TSharedPtr<FNode>& InFrom, const TSharedPtr<FNode>& InTo, int32 InFromGroup = 0, int32 InToGroup = 0);

	void Run(EPipelineMode InPipelineMode, const FFrameComplete& InOnFrameComplete, const FProcessComplete& InOnProcessComplete);
	void Run(const FPipelineRunParameters& InPipelineRunParameters);
	bool IsRunning() const;
	void Cancel();
	int32 GetNodeCount() const;

	FString ToString() const;

	static bool GetPhysicalDeviceLUIDs(FString& OutUEPhysicalDeviceLUID, TArray<FString>& OutAllPhysicalDeviceLUIDs);
	static FString PickPhysicalDevice();

private:

	TArray<TSharedPtr<FNode>> Nodes;
	TArray<FConnection> Connections;

	TWeakObjectPtr<UMetaHumanPipelineProcess> Process = nullptr;

	void StopProcess(bool bInClearMessageQueue = false);
};

}
