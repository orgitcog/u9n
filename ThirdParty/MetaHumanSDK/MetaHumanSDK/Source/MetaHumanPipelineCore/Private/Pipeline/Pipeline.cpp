// Copyright Epic Games, Inc. All Rights Reserved.

#include "Pipeline/Pipeline.h"
#include "Pipeline/Node.h"
#include "Pipeline/PipelineProcess.h"
#include "Pipeline/PipelineData.h"
#include "Pipeline/Log.h"

#include "Async/TaskGraphInterfaces.h"

namespace UE::MetaHuman::Pipeline
{
	TAutoConsoleVariable<bool> CVarBalancedGPUSelection
	{
		TEXT("mh.Pipeline.BalancedGPUSelection"),
		false,
		TEXT("The MeshTracker pipeline will try to balance out the workload on multiple GPUs."),
		ECVF_Default
	};

void FPipelineRunParameters::SetMode(EPipelineMode InMode)
{
	Mode = InMode;
}

EPipelineMode FPipelineRunParameters::GetMode() const
{
	return Mode;
}

void FPipelineRunParameters::SetOnFrameComplete(const FFrameComplete& InOnFrameComplete)
{
	OnFrameComplete = InOnFrameComplete;
}

const FFrameComplete& FPipelineRunParameters::GetOnFrameComplete() const
{
	return OnFrameComplete;
}

void FPipelineRunParameters::SetOnProcessComplete(const FProcessComplete& InOnProcessComplete)
{
	OnProcessComplete = InOnProcessComplete;
}

const FProcessComplete& FPipelineRunParameters::GetOnProcessComplete() const
{
	return OnProcessComplete;
}

void FPipelineRunParameters::SetStartFrame(int32 InStartFrame)
{
	StartFrame = InStartFrame;
}

int32 FPipelineRunParameters::GetStartFrame() const
{
	return StartFrame;
}

void FPipelineRunParameters::SetEndFrame(int32 InEndFrame)
{
	EndFrame = InEndFrame;
}

int32 FPipelineRunParameters::GetEndFrame() const
{
	return EndFrame;
}

void FPipelineRunParameters::SetRestrictStartingToGameThread(bool bInRestrictStartingToGameThread)
{
	bRestrictStartingToGameThread = bInRestrictStartingToGameThread;
}

bool FPipelineRunParameters::GetRestrictStartingToGameThread() const
{
	return bRestrictStartingToGameThread;
}

void FPipelineRunParameters::SetProcessNodesInRandomOrder(bool bInProcessNodesInRandomOrder)
{
	bProcessNodesInRandomOrder = bInProcessNodesInRandomOrder;
}

bool FPipelineRunParameters::GetProcessNodesInRandomOrder() const
{
	return bProcessNodesInRandomOrder;
}

void FPipelineRunParameters::SetCheckThreadLimit(bool bInCheckThreadLimit)
{
	bCheckThreadLimit = bInCheckThreadLimit;
}

bool FPipelineRunParameters::GetCheckThreadLimit() const
{
	return bCheckThreadLimit;
}

void FPipelineRunParameters::SetCheckProcessingSpeed(bool bInCheckProcessingSpeed)
{
	bCheckProcessingSpeed = bInCheckProcessingSpeed;
}

bool FPipelineRunParameters::GetCheckProcessingSpeed() const
{
	return bCheckProcessingSpeed;
}

void FPipelineRunParameters::SetVerbosity(ELogVerbosity::Type InVerbosity)
{
	Verbosity = InVerbosity;
}

ELogVerbosity::Type FPipelineRunParameters::GetVerbosity() const
{
	return Verbosity;
}

void FPipelineRunParameters::SetGpuToUse(const FString& InUseGPU)
{
	UseGPU = InUseGPU;
}

void FPipelineRunParameters::UnsetGpuToUse()
{
	UseGPU.Reset();
}

TOptional<FString> FPipelineRunParameters::GetGpuToUse() const
{
	return UseGPU;
}


FPipeline::FPipeline()
{
	Process = NewObject<UMetaHumanPipelineProcess>();
	Process->AddToRoot();
}

FPipeline::~FPipeline()
{
	Reset();

	if (Process.IsValid())
	{
		Process->PipelineNowInvalid();
		Process->RemoveFromRoot();
	}

	Process = nullptr;
}

void FPipeline::Reset()
{
	StopProcess();

	Nodes.Reset();
	Connections.Reset();
}

void FPipeline::AddNode(const TSharedPtr<FNode>& InNode)
{
	Nodes.Add(InNode);
}

void FPipeline::MakeConnection(const TSharedPtr<FNode>& InFrom, const TSharedPtr<FNode>& InTo, int32 InFromGroup, int32 InToGroup)
{
	Connections.Add(FConnection(InFrom, InTo, InFromGroup, InToGroup));
}

void FPipeline::Run(EPipelineMode InPipelineMode, const FFrameComplete& InOnFrameComplete, const FProcessComplete& InOnProcessComplete)
{
	FPipelineRunParameters Params;
	Params.SetMode(InPipelineMode);
	Params.SetOnFrameComplete(InOnFrameComplete);
	Params.SetOnProcessComplete(InOnProcessComplete);

	Run(Params);
}

void FPipeline::Run(const FPipelineRunParameters &InPipelineRunParameters)
{
	FPipelineRunParameters Params = InPipelineRunParameters;

	// check if we have available threads to run all nodes
	EPipelineMode Mode = Params.GetMode();

	if (Mode == EPipelineMode::PushSyncNodes || Mode == EPipelineMode::PushAsyncNodes)
	{
		const int32 NumOfRequiredThreads = GetNodeCount() + 1; // +1 to ensure async nodes have somewhere to process
		if (FTaskGraphInterface::Get().GetNumBackgroundThreads() < NumOfRequiredThreads)
		{
			if (Params.GetCheckThreadLimit())
			{
				UE_LOG(LogMetaHumanPipeline, Warning, TEXT("Not enough background threads available: required %i, available %i. The MetaHuman pipeline is going to run on a single thread."), NumOfRequiredThreads, FTaskGraphInterface::Get().GetNumBackgroundThreads());

				if (Mode == EPipelineMode::PushSyncNodes)
				{
					Params.SetMode(EPipelineMode::PushSync);
				}
				else
				{
					Params.SetMode(EPipelineMode::PushAsync);
				}
			}
			else
			{
				TSharedPtr<FPipelineData> PipelineData = MakeShared<FPipelineData>();

				FString ErrorMessage = FString::Format(TEXT("Not enough background threads available: required {0}, available {1}."), { NumOfRequiredThreads, FTaskGraphInterface::Get().GetNumBackgroundThreads() });
				PipelineData->SetExitStatus(UE::MetaHuman::Pipeline::EPipelineExitStatus::InsufficientThreadsForNodes);
				PipelineData->SetErrorMessage(ErrorMessage);

				Params.GetOnProcessComplete().Broadcast(PipelineData);
				return;
			}
		}
	}

	if (Process.IsValid())
	{
		Process->Start(Nodes, Connections, Params);
	}
	else
	{
		TSharedPtr<FPipelineData> PipelineData = MakeShared<FPipelineData>();

		PipelineData->SetExitStatus(UE::MetaHuman::Pipeline::EPipelineExitStatus::OutOfScope);
		PipelineData->SetErrorMessage("Pipeline out of scope");

		Params.GetOnProcessComplete().Broadcast(PipelineData);
	}
}

bool FPipeline::IsRunning() const
{
	return (Process.IsValid() && Process->IsRunning());
}

void FPipeline::Cancel()
{
	StopProcess(true);
}

FString FPipeline::ToString() const
{
	FString Message;

	Message += LINE_TERMINATOR;
	Message += LINE_TERMINATOR;
	Message += "--------------------";
	Message += LINE_TERMINATOR;
	Message += LINE_TERMINATOR;
	Message += TEXT("NODES:");
	Message += LINE_TERMINATOR;

	for (const TSharedPtr<FNode>& Node : Nodes)
	{
		Message += Node->ToString();
		Message += LINE_TERMINATOR;
	}

	Message += "--------------------";
	Message += LINE_TERMINATOR;
	Message += LINE_TERMINATOR;

	return Message;
}

void FPipeline::StopProcess(bool bInClearMessageQueue)
{
	if (Process.IsValid())
	{
		Process->Stop(bInClearMessageQueue);
	}
}

bool FPipeline::GetPhysicalDeviceLUIDs(FString& OutUEPhysicalDeviceLUID, TArray<FString>& OutAllPhysicalDeviceLUIDs)
{	
	return false;
}

FString FPipeline::PickPhysicalDevice()
{
	FString UEGPU;
	TArray<FString> AllGPUs;

	if (FPipeline::GetPhysicalDeviceLUIDs(UEGPU, AllGPUs))
	{
		if (CVarBalancedGPUSelection.GetValueOnAnyThread())
		{
			// Let titan decide what to use
			return FString();
		}
		else
		{
			// Use the same device as UE
			return UEGPU;
		}
	}

	// Unable to pick any valid GPU, fallback to titan
	return FString();
}

int32 FPipeline::GetNodeCount() const
{
	// Total node count is number of nodes added to the pipeline + 2 (for the internal nodes) 
	return Nodes.Num() + 2;
}

}
