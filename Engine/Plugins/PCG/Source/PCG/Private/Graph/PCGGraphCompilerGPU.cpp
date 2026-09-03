// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/PCGGraphCompilerGPU.h"

#include "PCGGraph.h"
#include "PCGModule.h"
#include "PCGPin.h"
#include "Compute/PCGComputeCommon.h"
#include "Compute/PCGComputeKernel.h"
#include "Compute/PCGComputeKernelSource.h"
#include "Compute/PCGPinPropertiesGPU.h"
#include "Compute/DataInterfaces/PCGComputeDataInterface.h"
#include "Compute/DataInterfaces/PCGCustomKernelDataInterface.h"
#include "Compute/DataInterfaces/PCGDataCollectionDataInterface.h"
#include "Compute/DataInterfaces/PCGDataCollectionUploadDataInterface.h"
#include "Compute/DataInterfaces/PCGDataLabelResolverDataInterface.h"
#include "Compute/DataInterfaces/PCGDebugDataInterface.h"
#include "Compute/DataInterfaces/PCGLandscapeDataInterface.h"
#include "Compute/DataInterfaces/PCGStaticMeshDataInterface.h"
#include "Compute/DataInterfaces/PCGTextureDataInterface.h"
#include "Compute/DataInterfaces/PCGVirtualTextureDataInterface.h"
#include "Compute/Elements/PCGComputeGraphElement.h"
#include "Elements/PCGStaticMeshSpawnerKernel.h"
#include "Graph/PCGGPUGraphCompilationContext.h"
#include "Graph/PCGGraphCompiler.h"

#include "ComputeFramework/ComputeKernel.h"
#include "HAL/IConsoleManager.h"
#include "Misc/App.h"
#include "Shader/ShaderTypes.h"
#include "UObject/Package.h"

#if WITH_EDITOR
namespace PCGGraphCompilerGPU
{
	static TAutoConsoleVariable<bool> CVarEnableDebugging(
		TEXT("pcg.GraphExecution.GPU.EnableDebugging"),
		false,
		TEXT("Enable verbose logging of GPU compilation and execution."));
}

#define PCG_DATA_DESCRIPTION_LOGGING 0

void FPCGGraphCompilerGPU::LabelConnectedGPUNodeIslands(
	const TArray<FPCGGraphTask>& InCompiledTasks,
	const TSet<FPCGTaskId>& InGPUCompatibleTaskIds,
	const FTaskToSuccessors& InTaskSuccessors,
	const TMap<FPCGTaskId, UPCGComputeKernel*>& InTaskIdToKernel,
	TArray<uint32>& OutIslandIDs)
{
	OutIslandIDs.SetNumZeroed(InCompiledTasks.Num());

	// Traverses task inputs and successors and assigns the given island ID to each one. Memoized via output OutIslandIDs.
	auto FloodFillIslandID = [&InCompiledTasks, &InTaskSuccessors, &InGPUCompatibleTaskIds, &OutIslandIDs, &InTaskIdToKernel](FPCGTaskId InTaskId, int InIslandID, FPCGTaskId InTraversedFromTaskId, auto&& RecursiveCall) -> void
	{
		check(InTaskId != InTraversedFromTaskId);

		OutIslandIDs[InTaskId] = InIslandID;

		for (const FPCGGraphTaskInput& Input : InCompiledTasks[InTaskId].Inputs)
		{
			if (Input.TaskId != InTraversedFromTaskId && OutIslandIDs[Input.TaskId] == 0 && InGPUCompatibleTaskIds.Contains(Input.TaskId))
			{
				RecursiveCall(Input.TaskId, InIslandID, InTaskId, RecursiveCall);
			}
		}

		if (const TArray<FPCGTaskId>* Successors = InTaskSuccessors.Find(InTaskId))
		{
			for (FPCGTaskId Successor : *Successors)
			{
				if (Successor != InTraversedFromTaskId && OutIslandIDs[Successor] == 0 && InGPUCompatibleTaskIds.Contains(Successor))
				{
					RecursiveCall(Successor, InIslandID, InTaskId, RecursiveCall);
				}
			}
		}
	};

	for (FPCGTaskId GPUTaskId : InGPUCompatibleTaskIds)
	{
		if (OutIslandIDs[GPUTaskId] == 0)
		{
			// Really doesn't matter what the island IDs are so just use ID of first task encountered in island.
			const uint32 IslandID = static_cast<uint32>(GPUTaskId);
			FloodFillIslandID(GPUTaskId, IslandID, InvalidPCGTaskId, FloodFillIslandID);
		}
	}
}

void FPCGGraphCompilerGPU::CollectGPUNodeSubsets(
	const TArray<FPCGGraphTask>& InCompiledTasks,
	const FTaskToSuccessors& InTaskSuccessors,
	const TSet<FPCGTaskId>& InGPUCompatibleTaskIds,
	const TMap<FPCGTaskId, UPCGComputeKernel*>& InTaskIdToKernel,
	TArray<TSet<FPCGTaskId>>& OutNodeSubsetsToConvertToCFGraph)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGGraphCompilerGPU::CollectGPUNodeSubsets);

	// Identifies connected sets of GPU nodes, giving each a non-zero ID value.
	TArray<uint32> ConnectedGPUNodeIslandIDs;
	LabelConnectedGPUNodeIslands(InCompiledTasks, InGPUCompatibleTaskIds, InTaskSuccessors, InTaskIdToKernel, ConnectedGPUNodeIslandIDs);

	// Any new island IDs will be created from the task count which will be larger than any island IDs presently set.
	uint32 NextIslandId = InCompiledTasks.Num();

	// Cache dependencies. Since our islands are only ever split (rather than replaced or removed), the dependency on each island ID is invariant.
	TMap<TPair<FPCGTaskId, /*IslandId*/uint32>, /*bIsDependent*/bool> CPUNodeIsDependentOnIslandCached;

	// Visit tasks in execution order so that splits happen as upstream to minimize island splits.
	FPCGGraphCompiler::VisitTasksInExecutionOrder(
		InCompiledTasks,
		InTaskSuccessors,
		[&InCompiledTasks, &InTaskSuccessors, &InGPUCompatibleTaskIds, &ConnectedGPUNodeIslandIDs, &InTaskIdToKernel, &NextIslandId, &CPUNodeIsDependentOnIslandCached](FPCGTaskId InTaskId) -> bool
		{
			auto PropagateIslandIDDownstream = [&InCompiledTasks, &InTaskSuccessors, &InGPUCompatibleTaskIds, &ConnectedGPUNodeIslandIDs](FPCGTaskId InTaskId, uint32 InOldIslandID, uint32 InNewIslandID, auto&& RecursiveCall) -> void
			{
				ConnectedGPUNodeIslandIDs[InTaskId] = InNewIslandID;

				if (const TArray<FPCGTaskId>* Successors = InTaskSuccessors.Find(InTaskId))
				{
					for (FPCGTaskId Successor : *Successors)
					{
						if (ConnectedGPUNodeIslandIDs[Successor] == InOldIslandID && InGPUCompatibleTaskIds.Contains(Successor))
						{
							RecursiveCall(Successor, InOldIslandID, InNewIslandID, RecursiveCall);
						}
					}
				}
			};

			// GPU task. Split at output if the kernel requests it.
			if (InGPUCompatibleTaskIds.Contains(InTaskId))
			{
				UPCGComputeKernel* const* FoundKernel = InTaskIdToKernel.Find(InTaskId);
				if (FoundKernel && (*FoundKernel)->SplitGraphAtOutput())
				{
					if (const TArray<FPCGTaskId>* Successors = InTaskSuccessors.Find(InTaskId))
					{
						for (FPCGTaskId SuccessorTaskId : *Successors)
						{
							if (InGPUCompatibleTaskIds.Contains(SuccessorTaskId))
							{
								const uint32 SuccessorIsland = ConnectedGPUNodeIslandIDs[SuccessorTaskId];

								if (ConnectedGPUNodeIslandIDs[InTaskId] == SuccessorIsland)
								{
									// Propagate a new island ID to all downstream GPU tasks within the island.
									PropagateIslandIDDownstream(SuccessorTaskId, SuccessorIsland, NextIslandId++, PropagateIslandIDDownstream);
								}
							}
						}
					}
				}
			}
			// Otherwise CPU task. For every CPU node that has one or more downstream GPU node connected, check for a GPU -> CPU -> GPU pattern where data flows
			// from a GPU node island to the CPU and then back to the same island. For such cases, we traverse the entire tree of GPU nodes that
			// are in the island and downstream of the CPU node and bump their island ID - splitting the island into a portion that is independent
			// of the CPU node and a portion that is dependent on it, so that we can read the data back to CPU, execute the CPU portion, then
			// re-upload to GPU.
			else if (const TArray<FPCGTaskId>* Successors = InTaskSuccessors.Find(InTaskId))
			{
				for (FPCGTaskId SuccessorTaskId : *Successors)
				{
					if (!InGPUCompatibleTaskIds.Contains(SuccessorTaskId))
					{
						continue;
					}

					const uint32 SuccessorIsland = ConnectedGPUNodeIslandIDs[SuccessorTaskId];

					// Recursively check entire node tree upstream of this CPU node to see if it can be fed by any node in the GPU island.
					auto CPUNodeIsDependentOnIsland = [&InCompiledTasks, &InTaskSuccessors, &InGPUCompatibleTaskIds, &ConnectedGPUNodeIslandIDs, &CPUNodeIsDependentOnIslandCached](FPCGTaskId InTaskId, uint32 InIslandID, auto&& RecursiveCall) -> bool
					{
						if (bool* CachedValue = CPUNodeIsDependentOnIslandCached.Find({ InTaskId, InIslandID }))
						{
							return *CachedValue;
						}

						// Is this task is part of the specified island.
						bool bIsDependent = ConnectedGPUNodeIslandIDs[InTaskId] == InIslandID;

						if (!bIsDependent)
						{
							// Check upstream tasks recursively.
							for (const FPCGGraphTaskInput& Input : InCompiledTasks[InTaskId].Inputs)
							{
								if (RecursiveCall(Input.TaskId, InIslandID, RecursiveCall))
								{
									bIsDependent = true;
									break;
								}
							}
						}

						CPUNodeIsDependentOnIslandCached.Add({ InTaskId, InIslandID }, bIsDependent);

						return bIsDependent;
					};

					if (CPUNodeIsDependentOnIsland(InTaskId, SuccessorIsland, CPUNodeIsDependentOnIsland))
					{
						// Propagate a new island ID to all downstream GPU tasks within the island.
						PropagateIslandIDDownstream(SuccessorTaskId, SuccessorIsland, NextIslandId++, PropagateIslandIDDownstream);
					}
				}
			}

			return true;
		});

	// Island IDs now correctly identify subsets of nodes that will be assembled into compute graphs for GPU execution.
	for (FPCGTaskId TaskId = 0; TaskId < InCompiledTasks.Num(); ++TaskId)
	{
		if (ConnectedGPUNodeIslandIDs[TaskId] != 0)
		{
			TSet<FPCGTaskId> GPUNodeSubset;

			const uint32 IslandId = ConnectedGPUNodeIslandIDs[TaskId];

			for (FPCGTaskId OtherTaskId = TaskId; OtherTaskId < InCompiledTasks.Num(); ++OtherTaskId)
			{
				if (ConnectedGPUNodeIslandIDs[OtherTaskId] == IslandId)
				{
					GPUNodeSubset.Add(OtherTaskId);

					ConnectedGPUNodeIslandIDs[OtherTaskId] = 0;
				}
			}

			OutNodeSubsetsToConvertToCFGraph.Add(MoveTemp(GPUNodeSubset));
		}
	}
}

void FPCGGraphCompilerGPU::ExpandGPUNodeKernelsToTasks(
	FPCGGPUCompilationContext& InOutContext,
	TSet<FPCGTaskId>& InOutGPUCompatibleTaskIds,
	TArray<FPCGGraphTask>& InOutCompiledTasks,
	TMap<FPCGTaskId, UPCGComputeKernel*>& OutTaskIdToKernel)
{
	const int NumTasksBefore = InOutCompiledTasks.Num();

	// Build successors map, only for nodes that are downstream from one or more of the relevant GPU tasks.
	FTaskToSuccessors TaskSuccessors;
	TaskSuccessors.Reserve(InOutCompiledTasks.Num());
	for (FPCGTaskId TaskId = 0; TaskId < InOutCompiledTasks.Num(); ++TaskId)
	{
		for (const FPCGGraphTaskInput& TaskInput : InOutCompiledTasks[TaskId].Inputs)
		{
			if (InOutGPUCompatibleTaskIds.Contains(TaskInput.TaskId))
			{
				TaskSuccessors.FindOrAdd(TaskInput.TaskId).AddUnique(TaskId);
			}
		}
	}

	// Local to loop body, but hoisted for performance.
	TArray<UPCGComputeKernel*> NodeKernels;
	TArray<FPCGKernelEdge> NodeKernelEdges;
	TMap<const UPCGComputeKernel*, FPCGTaskId> KernelToTaskId;
	TArray<FPCGPinProperties> KernelInputs;
	TArray<FPCGPinPropertiesGPU> UpstreamKernelOutputs;

	KernelToTaskId.Reserve(InOutGPUCompatibleTaskIds.Num());
	OutTaskIdToKernel.Reserve(InOutGPUCompatibleTaskIds.Num());

	// Process one GPU task at a time.
	for (FPCGTaskId TaskId : InOutGPUCompatibleTaskIds)
	{
		const UPCGSettings* Settings = InOutCompiledTasks[TaskId].Node ? InOutCompiledTasks[TaskId].Node->GetSettings() : nullptr;
		if (!ensure(Settings))
		{
			continue;
		}

		NodeKernels.Reset();
		NodeKernelEdges.Reset();
		Settings->CreateKernels(InOutContext, GetTransientPackage(), NodeKernels, NodeKernelEdges);

		ensureMsgf(NodeKernels.Remove(nullptr) == 0, TEXT("Settings '%s' returned one or more null kernels."), *Settings->GetName());

		if (NodeKernels.IsEmpty())
		{
			ensureMsgf(false, TEXT("PCG GPU compiler: Settings '%s' did not emit any kernels, check implementation of UPCGSettings::CreateKernels()."), *Settings->GetName());
			continue;
		}

		if (NodeKernelEdges.IsEmpty())
		{
			continue;
		}

		for (FPCGKernelEdge& KernelEdge : NodeKernelEdges)
		{
			if (!KernelEdge.IsConnectedToNodeInput() && !KernelEdge.IsConnectedToNodeOutput())
			{
				if (UPCGComputeKernel* UpstreamKernel = KernelEdge.GetUpstreamKernel())
				{
					UpstreamKernel->AddInternalPin(KernelEdge.UpstreamPin.Label);
				}

				if (UPCGComputeKernel* DownstreamKernel = KernelEdge.GetDownstreamKernel())
				{
					DownstreamKernel->AddInternalPin(KernelEdge.DownstreamPin.Label);
				}
			}
		}

		KernelToTaskId.Reset();

		// Create a new task for each kernel emitted by the node.
		while (!NodeKernels.IsEmpty())
		{
			// Find a kernel that is ready to process (does not depend on another kernel in this node).
			// Number of kernels is likely to be small so brute force loop over them.
			int ReadyKernelIndex = 0;

			for (int Index = 0; Index < NodeKernels.Num(); ++Index)
			{
				// Look for an edge connected to an upstream kernel that is yet to be processed.
				const FPCGKernelEdge* BlockingEdge = NodeKernelEdges.FindByPredicate([Index, &NodeKernels, &KernelToTaskId](const FPCGKernelEdge& InEdge)
				{
					const UPCGComputeKernel* UpstreamKernel = InEdge.UpstreamPin.Kernel;
					return InEdge.DownstreamPin.Kernel == NodeKernels[Index] && UpstreamKernel && !KernelToTaskId.Contains(UpstreamKernel);
				});

				if (!BlockingEdge)
				{
					// No pending kernels, ready to execute this one.
					ReadyKernelIndex = Index;
					break;
				}
			}

			if (ReadyKernelIndex == INDEX_NONE)
			{
				ensureMsgf(false, TEXT("Compilation did not make progress, %d kernels were not processed."), NodeKernels.Num());
				break;
			}

			UPCGComputeKernel* Kernel = NodeKernels[ReadyKernelIndex];

			// Create a new task for this kernel.
			FPCGTaskId NewKernelTaskId = InOutCompiledTasks.Num();

			FPCGGraphTask TaskCopy = InOutCompiledTasks[TaskId];
			TaskCopy.NodeId = NewKernelTaskId;
			TaskCopy.Inputs.Reset();

			FPCGGraphTask& NewKernelTask = InOutCompiledTasks.Add_GetRef(MoveTemp(TaskCopy));

			// Update maps.
			KernelToTaskId.Add(Kernel, NewKernelTaskId);
			OutTaskIdToKernel.Add(NewKernelTaskId, Kernel);

			// Get kernel inputs in preparation for wiring.
			KernelInputs.Reset();
			Kernel->GetInputPins(KernelInputs);

			// Wire inputs
			for (const FPCGPinProperties& KernelInputProps : KernelInputs)
			{
				const FPCGKernelEdge* Edge = NodeKernelEdges.FindByPredicate([Kernel, &KernelInputProps](const FPCGKernelEdge& Edge)
				{
					return Edge.DownstreamPin.Kernel == Kernel && Edge.DownstreamPin.Label == KernelInputProps.Label;
				});

				if (!Edge)
				{
					continue;
				}

				if (const UPCGComputeKernel* UpstreamKernel = Edge->UpstreamPin.Kernel)
				{
					checkf(KernelToTaskId.Contains(UpstreamKernel), TEXT("Missing kernel '%s', was kernel emitted from CreateKernels()?"), *UpstreamKernel->GetName());

					// Connection from an upstream kernel from this node.
					const FPCGTaskId UpstreamTaskId = KernelToTaskId[UpstreamKernel];

					FPCGGraphTaskInput& TaskInput = NewKernelTask.Inputs.AddDefaulted_GetRef();
					TaskInput.TaskId = UpstreamTaskId;
					TaskInput.bProvideData = true;
					TaskInput.DownstreamPin = KernelInputProps;

					// Look for output pin of upstream kernel in order to set the input upstream pin properties.
					UpstreamKernelOutputs.Reset();
					Kernel->GetOutputPins(UpstreamKernelOutputs);

					const FPCGPinPropertiesGPU* UpstreamOutput = UpstreamKernelOutputs.FindByPredicate([Edge](const FPCGPinPropertiesGPU& InUpstreamOutput)
					{
						return InUpstreamOutput.Label == Edge->UpstreamPin.Label;
					});

					if (ensure(UpstreamOutput))
					{
						TaskInput.UpstreamPin = *UpstreamOutput;
					}
				}
				// Edge from an upstream node/task. Correlates to an input for the original node task.
				else
				{
					check(Edge->IsConnectedToNodeInput());
					const FName NodePinLabel = Edge->UpstreamPin.Label;

					const FPCGGraphTaskInput* OriginalTaskInput = InOutCompiledTasks[TaskId].Inputs.FindByPredicate([NodePinLabel, &KernelInputProps](const FPCGGraphTaskInput& TaskInput)
					{
						return TaskInput.DownstreamPin && TaskInput.DownstreamPin->Label == NodePinLabel;
					});

					if (ensure(OriginalTaskInput))
					{
						// Match - add a new input wire to the new task.
						FPCGGraphTaskInput& NewTaskInput = NewKernelTask.Inputs.Add_GetRef(*OriginalTaskInput);

						// Update the pin properties with those specified by the kernel.
						NewTaskInput.DownstreamPin = KernelInputProps;
					}
				}
			}

			NodeKernels.RemoveAt(ReadyKernelIndex);
		}

		// Now rewire all tasks downstream of the original task to the appropriate kernel outputs.
		if (const TArray<FPCGTaskId>* Successors = TaskSuccessors.Find(TaskId))
		{
			for (FPCGTaskId SuccessorTaskId : *Successors)
			{
				for (FPCGGraphTaskInput& Input : InOutCompiledTasks[SuccessorTaskId].Inputs)
				{
					if (Input.TaskId == TaskId && Input.UpstreamPin)
					{
						// Outgoing edge. Rewire to appropriate kernel.
						for (FPCGKernelEdge& Edge : NodeKernelEdges)
						{
							if (Edge.IsConnectedToNodeOutput() && Edge.DownstreamPin.Label == Input.UpstreamPin->Label && ensure(Edge.UpstreamPin.Kernel))
							{
								Input.TaskId = KernelToTaskId[Edge.UpstreamPin.Kernel];
							}
						}
					}
				}
			}
		}
	}

	const int NumTasksAfter = InOutCompiledTasks.Num();

	TArray<FPCGTaskId> OldIdToNewId;

	// All the original GPU tasks have now been replaced with a task for each kernel. Remove them without rewiring.
	FPCGGraphCompiler::CullTasks(
		InOutCompiledTasks,
		/*bAddPassthroughWires=*/false,
		[&InOutGPUCompatibleTaskIds](const FPCGGraphTask& InTask)
		{
			return InOutGPUCompatibleTaskIds.Contains(InTask.NodeId);
		},
		&OldIdToNewId);

	check(OldIdToNewId.Num() == NumTasksAfter);

	// Refresh GPU compatible task IDs to reflect shifted indices after culling.
	{
		InOutGPUCompatibleTaskIds.Reset();

		for (FPCGTaskId OldGPUTaskId = NumTasksBefore; OldGPUTaskId < NumTasksAfter; ++OldGPUTaskId)
		{
			const FPCGTaskId RemappedId = OldIdToNewId[OldGPUTaskId];
			check(RemappedId != INDEX_NONE);

			InOutGPUCompatibleTaskIds.Add(RemappedId);
		}
	}

	// Remap task IDs of the task->kernel map to reflect shifted indices after culling.
	{
		TMap<FPCGTaskId, UPCGComputeKernel*> TaskIdToKernelBeforeCull = MoveTemp(OutTaskIdToKernel);
		OutTaskIdToKernel.Reset();

		for (TPair<FPCGTaskId, UPCGComputeKernel*>& Entry : TaskIdToKernelBeforeCull)
		{
			const FPCGTaskId RemappedId = OldIdToNewId[Entry.Key];
			check(RemappedId != INDEX_NONE);

			OutTaskIdToKernel.Add(RemappedId, Entry.Value);
		}
	}
}

void FPCGGraphCompilerGPU::CreateGatherTasksAtGPUInputs(UPCGGraph* InGraph, const TSet<FPCGTaskId>& InGPUCompatibleTaskIds, TArray<FPCGGraphTask>& InOutCompiledTasks)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGGraphCompilerGPU::CreateGatherTasksAtGPUInputs);

	using FOriginalInputPinKey = TPair<FPCGTaskId /* Original GPU task */, FName /* Input pin label */>;

	// These are local to loop below but hoisted here for efficiency.
	TSet<FOriginalInputPinKey> EncounteredInputPins; // TODO this is heavyweight, could use simple array.
	TMap<FOriginalInputPinKey, FPCGTaskId /* Gather task */> InputPinToGatherTask;

	// Add all compute graph task inputs and outputs.
	for (FPCGTaskId GPUTaskId : InGPUCompatibleTaskIds)
	{
		EncounteredInputPins.Empty(EncounteredInputPins.Num());
		InputPinToGatherTask.Empty(InputPinToGatherTask.Num());

		// First pass - create gather tasks for all original input pins which have more than one incident edge.
		// This is so we can gather on the CPU (much more efficient than going it on the GPU).
		for (int InputIndex = 0; InputIndex < InOutCompiledTasks[GPUTaskId].Inputs.Num(); ++InputIndex)
		{
			// Helper to get current input. We avoid simply taking a local reference as InOutCompiledTasks can be modified below.
			auto CurrentInput = [&InOutCompiledTasks, InputIndex, GPUTaskId]() -> FPCGGraphTaskInput&
			{
				return InOutCompiledTasks[GPUTaskId].Inputs[InputIndex];
			};

			if (!CurrentInput().DownstreamPin.IsSet())
			{
				continue;
			}

			const FOriginalInputPinKey PinKey = { GPUTaskId, CurrentInput().DownstreamPin->Label };

			// If already created a gather task, then nothing more to do for this pin.
			if (const FPCGTaskId* GatherTaskId = InputPinToGatherTask.Find(PinKey))
			{
				continue;
			}

			// If we're encountering pin for first time, register it.
			if (!EncounteredInputPins.Contains(PinKey))
			{
				EncounteredInputPins.Add(PinKey);
				continue;
			}

			// Second time we've encountered this input pin - create a gather element because we need one edge connected to
			// each virtual input pin, so that we can obtain the data items from the input data collection using the unique
			// virtual pin label at execution time.
			const FPCGTaskId GatherTaskId = InOutCompiledTasks.Num();
			FPCGGraphTask& GatherTask = InOutCompiledTasks.Emplace_GetRef();
			GatherTask.NodeId = GatherTaskId;
			GatherTask.ParentId = InOutCompiledTasks[GPUTaskId].ParentId;
			GatherTask.ElementSource = EPCGElementSource::Gather;

			InputPinToGatherTask.Add(PinKey, GatherTaskId);
		}

		EncounteredInputPins.Empty(EncounteredInputPins.Num());

		// Second pass - wire up the newly added gather tasks once we have the full picture of which edges are affected.
		for (int InputIndex = 0; InputIndex < InOutCompiledTasks[GPUTaskId].Inputs.Num(); ++InputIndex)
		{
			// Helper to get input rather than reference because we modify the inputs array below.
			auto CurrentGPUTaskInput = [&InOutCompiledTasks, InputIndex, GPUTaskId]() -> FPCGGraphTaskInput&
			{
				return InOutCompiledTasks[GPUTaskId].Inputs[InputIndex];
			};

			if (!CurrentGPUTaskInput().DownstreamPin)
			{
				continue;
			}

			const FOriginalInputPinKey PinKey = { GPUTaskId, CurrentGPUTaskInput().DownstreamPin->Label };

			if (const FPCGTaskId* GatherTaskId = InputPinToGatherTask.Find(PinKey))
			{
				// Wire the upstream output pin to the gather task input.
				{
					FPCGGraphTaskInput& GatherTaskInput = InOutCompiledTasks[*GatherTaskId].Inputs.Add_GetRef(CurrentGPUTaskInput());
					if (GatherTaskInput.DownstreamPin)
					{
						GatherTaskInput.DownstreamPin->Label = PCGPinConstants::DefaultInputLabel;
					}
				}

				if (!EncounteredInputPins.Contains(PinKey))
				{
					// First time we're encountering this input pin, wire it to the gather task output.
					EncounteredInputPins.Add(PinKey);

					CurrentGPUTaskInput().TaskId = *GatherTaskId;

					// Wire the gather task output pin to the downstream GPU task input pin.
					if (CurrentGPUTaskInput().UpstreamPin)
					{
						CurrentGPUTaskInput().UpstreamPin->Label = PCGPinConstants::DefaultOutputLabel;
					}
				}
				else
				{
					// Input pin already encountered, already wired to gather task. Remove this input.
					InOutCompiledTasks[GPUTaskId].Inputs.RemoveAt(InputIndex);
					--InputIndex;
				}
			}
		}
	}
}

void FPCGGraphCompilerGPU::SetupVirtualPins(
	const TSet<FPCGTaskId>& InCollapsedTasks,
	const TArray<FPCGGraphTask>& InCompiledTasks,
	const FTaskToSuccessors& InTaskSuccessors,
	FOriginalToVirtualPin& OutOriginalToVirtualPin)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGGraphCompilerGPU::SetupVirtualPins);

	// Used to construct unique input/output labels, ultimately consumed in graph executor in BuildTaskInput and PostExecute for input/output respectively.
	int InputCount = 0;
	int OutputCount = 0;

	// Add all compute graph task inputs and outputs.
	for (FPCGTaskId GPUTaskId : InCollapsedTasks)
	{
		// First input edges to the compute graph.
		for (const FPCGGraphTaskInput& Input : InCompiledTasks[GPUTaskId].Inputs)
		{
			if (InCollapsedTasks.Contains(Input.TaskId))
			{
				continue;
			}

			// Only graph edges associated with pins are considered.
			if (Input.DownstreamPin.IsSet())
			{
				const FName VirtualLabel = *FString::Format(TEXT("{0}-VirtualIn{1}"), { Input.DownstreamPin->Label.ToString(), InputCount++ });
				const bool bIsInputPin = true;
				OutOriginalToVirtualPin.Add({ GPUTaskId, Input.DownstreamPin->Label, bIsInputPin }, VirtualLabel);
			}
		}

		// Create virtual pin labels for all output pins of GPU nodes. We could create them only for output pins that internal->external edges, however
		// it is helpful for special cases like inspection-only to always have virtual labels for all external output pins.
		// TODO update to use kernels once the GPU compiler flow is refactored.
		if (const UPCGSettings* Settings = InCompiledTasks[GPUTaskId].Node ? InCompiledTasks[GPUTaskId].Node->GetSettings() : nullptr)
		{
			for (const FPCGPinProperties& PinProps : Settings->OutputPinProperties())
			{
				const FNodePin PinKey = { GPUTaskId, PinProps.Label, /*Pin is input*/false };
				if (!OutOriginalToVirtualPin.Contains(PinKey))
				{
					const FName VirtualLabel = *FString::Format(TEXT("{0}-VirtualOut{1}"), { PinProps.Label.ToString(), OutputCount++ });
					OutOriginalToVirtualPin.Add(PinKey, VirtualLabel);
				}
			}
		}
	}
}

void FPCGGraphCompilerGPU::WireComputeGraphTask(
	FPCGTaskId InGPUGraphTaskId,
	const TSet<FPCGTaskId>& InCollapsedTasks,
	TArray<FPCGGraphTask>& InOutCompiledTasks,
	const FTaskToSuccessors& InTaskSuccessors,
	const FOriginalToVirtualPin& InOriginalToVirtualPin)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGGraphCompilerGPU::WireComputeGraphTask);

	FPCGGraphTask& GPUGraphTask = InOutCompiledTasks[InGPUGraphTaskId];

	// Add all compute graph task inputs and outputs.
	for (FPCGTaskId GPUTaskId : InCollapsedTasks)
	{
		// First find CPU to GPU edges and wire in the GPU graph node inputs.
		for (const FPCGGraphTaskInput& Input : InOutCompiledTasks[GPUTaskId].Inputs)
		{
			if (InCollapsedTasks.Contains(Input.TaskId))
			{
				continue;
			}

			FPCGGraphTaskInput& AddedInput = GPUGraphTask.Inputs.Add_GetRef(Input);

			// TODO is pinless fine with skipping?
			if (AddedInput.DownstreamPin.IsSet())
			{
				// Get the compute graph virtual pin corresponding to this pin, if one was created.
				const FName* VirtualPinLabel = InOriginalToVirtualPin.Find({ GPUTaskId, AddedInput.DownstreamPin->Label, /*bIsInputPin=*/true });

				if (ensure(VirtualPinLabel))
				{
					AddedInput.DownstreamPin->Label = *VirtualPinLabel;
				}
			}
		}

		if (!InTaskSuccessors.Contains(GPUTaskId))
		{
			continue;
		}

		// Next consider GPU to CPU edges to wire in the GPU graph node outputs.
		for (FPCGTaskId Successor : InTaskSuccessors[GPUTaskId])
		{
			if (InCollapsedTasks.Contains(Successor))
			{
				continue;
			}

			// Rewire inputs of this downstream CPU node to the outputs of the compute graph task.
			FPCGGraphTask& DownstreamCPUNode = InOutCompiledTasks[Successor];

			// Order matters here! We can never reorder inputs as it will impact execution.
			const int InputCountBefore = DownstreamCPUNode.Inputs.Num();
			for (int SuccessorInputIndex = 0; SuccessorInputIndex < InputCountBefore; ++SuccessorInputIndex)
			{
				// Implementation note: we modify the Inputs array in this loop, so don't take a reference to the current element.

				// Skip irrelevant edges.
				if (DownstreamCPUNode.Inputs[SuccessorInputIndex].TaskId != GPUTaskId)
				{
					continue;
				}

				// Wire downstream CPU node to compute graph task.
				FPCGGraphTaskInput InputCopy = DownstreamCPUNode.Inputs[SuccessorInputIndex];

				InputCopy.TaskId = InGPUGraphTaskId;

				if (DownstreamCPUNode.Inputs[SuccessorInputIndex].UpstreamPin.IsSet())
				{
					// Get the compute graph virtual pin corresponding to this pin, if one was created.
					const FName* VirtualPinLabel = InOriginalToVirtualPin.Find({ GPUTaskId, InputCopy.UpstreamPin->Label, /*Pin is input*/false });

					if (ensure(VirtualPinLabel))
					{
						// Wire to the existing virtual output pin.
						InputCopy.UpstreamPin->Label = *VirtualPinLabel;
					}
				}

				DownstreamCPUNode.Inputs.Add(MoveTemp(InputCopy));
			}
		}
	}
}

void FPCGGraphCompilerGPU::BuildComputeGraphTask(
	FPCGGPUCompilationContext& InOutContext,
	FPCGTaskId InGPUGraphTaskId,
	UPCGGraph* InGraph,
	uint32 InGridSize,
	uint32 InComputeGraphIndex,
	const TSet<FPCGTaskId>& InCollapsedTasks,
	const TArray<FPCGTaskId>& InComputeElementTasks,
	const FTaskToSuccessors& InTaskSuccessors,
	TArray<FPCGGraphTask>& InOutCompiledTasks,
	const FOriginalToVirtualPin& InOriginalToVirtualPin,
	TMap<FPCGTaskId, UPCGComputeKernel*>& InTaskIdToKernel)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGGraphCompilerGPU::BuildComputeGraphTask);

	check(InGraph);

	FPCGGraphCompiler& GraphCompiler = InOutContext.GetGraphCompiler();
	UPCGComputeGraph* ComputeGraph = GraphCompiler.GetComputeGraph(InGraph, InGridSize, InComputeGraphIndex);

	// If the graph does not exist already, create it.
	// todo_pcg: If the graph does exist already, then we throw the kernels we created away, which is pretty wasteful, even if only happens
	// on first graph compilation. We could probably defer creation of these objects until they're really needed by storing "createinfo" data.
	// Or possibly the caching of compute graphs could be done per pcg-graph, rather than per-compute-graph, and we could avoid doing any GPU
	// compilation work if already cached.
	if (!ComputeGraph)
	{
		// TODO: There is a race condition here where two threads can try to find this graph at the same time and both fail, so both create the graph.
		// This is fine, as they'll both create the same graph and place it at the same compute graph index. However it would be ideal perf-wise to
		// avoid this, as creating the compute graph can be expensive.
		
		// Create a new compute graph.
		UObject* ComputeGraphOuter = InOutContext.GetGraphCompiler().IsCooking() ? InGraph : Cast<UObject>(GetTransientPackage());
		const FName GraphName = MakeUniqueObjectName(InGraph, UPCGComputeGraph::StaticClass(), InGraph->GetFName());
		ComputeGraph = InOutContext.NewObject_AnyThread<UPCGComputeGraph>(ComputeGraphOuter, GraphName);
		InOutContext.SetStaticAttributeTable(&ComputeGraph->GetStaticAttributeTable());

		// Not incredibly useful for us - DG adds GetComponentSource()->GetComponentClass() object which allows it to bind at execution time by class.
		// But execution code requires it currently.
		ComputeGraph->Bindings.Add(UPCGDataBinding::StaticClass());

		BuildComputeGraphStaticData(InOutCompiledTasks, InCollapsedTasks, InTaskIdToKernel, ComputeGraph);

		FPinToDataInterface OutputPinDataInterfaces;

		CreateDataInterfaces(InOutContext, InOutCompiledTasks, InCollapsedTasks, InComputeElementTasks, InTaskSuccessors, InOriginalToVirtualPin, InTaskIdToKernel, ComputeGraph, OutputPinDataInterfaces);

		CompileComputeGraph(InOutContext, InOutCompiledTasks, InCollapsedTasks, InComputeElementTasks, InTaskSuccessors, InOriginalToVirtualPin, OutputPinDataInterfaces, InTaskIdToKernel, ComputeGraph);

		// Remove empty strings (at execution time empty string is placed in table slot 0).
		// NOTE: This can scramble order but order is not important at this stage (key values created at execution time in data binding).
		ComputeGraph->StringTable.RemoveAllSwap([](const FString& InString) { return InString.IsEmpty(); });

		for (FPCGTaskId TaskId : InCollapsedTasks)
		{
			// Re-outer kernels to compute graph.
			InTaskIdToKernel[TaskId]->Rename(nullptr, ComputeGraph);
		}

		// Clear out the current static attribute table.
		InOutContext.SetStaticAttributeTable(nullptr);
	}

	const int32 ComputeGraphIndex = InOutContext.AddCompiledComputeGraph(ComputeGraph);
	ensure(ComputeGraphIndex == InComputeGraphIndex);

	if (GraphCompiler.IsCooking())
	{
		TObjectPtr<UPCGComputeGraphSettings> Settings = NewObject<UPCGComputeGraphSettings>(InGraph);
		Settings->ComputeGraphIndex = ComputeGraphIndex;
		InOutCompiledTasks[InGPUGraphTaskId].ElementSource = EPCGElementSource::FromCookedSettings;
		InOutCompiledTasks[InGPUGraphTaskId].CookedSettings = Settings;
	}
	else
	{
		InOutCompiledTasks[InGPUGraphTaskId].Element = MakeShared<FPCGComputeGraphElement>(ComputeGraphIndex);
	}

	if (FApp::CanEverRender() && IsInGameThread() && ensure(ComputeGraph))
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UpdateResources);

		// Compile shader resources and create render proxies.
		ComputeGraph->UpdateResources();
	}
}

void FPCGGraphCompilerGPU::BuildComputeGraphStaticData(
	const TArray<FPCGGraphTask>& InCompiledTasks,
	const TSet<FPCGTaskId>& InCollapsedTasks,
	const TMap<FPCGTaskId, UPCGComputeKernel*>& InTaskIdToKernel,
	UPCGComputeGraph* InOutComputeGraph)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGGraphCompilerGPU::BuildComputeGraphStaticData);

	for (FPCGTaskId TaskId : InCollapsedTasks)
	{
		if (const UPCGComputeKernel* Kernel = InTaskIdToKernel[TaskId]; ensure(Kernel))
		{
			// Record attributes.
			TArray<FPCGKernelAttributeKey> AttributeKeys;
			Kernel->GetKernelAttributeKeys(AttributeKeys);

			for (const FPCGKernelAttributeKey& Key : AttributeKeys)
			{
				if (InOutComputeGraph->StaticAttributeTable.AddAttribute(Key) == INDEX_NONE)
				{
					UE_LOG(LogPCG, Error, TEXT("FPCGGraphCompilerGPU: Static attribute table exceeded maximum size (%d), use the 'Dump Data Descriptions' setting on the GPU node(s) to list attributes that are present."), PCGComputeConstants::MAX_NUM_CUSTOM_ATTRS);
					break;
				}
			}

			// Record strings.
			Kernel->AddStaticCreatedStrings(InOutComputeGraph->StringTable);
		}
	}

	ensure(InOutComputeGraph->StaticAttributeTable.Num() <= PCGComputeConstants::MAX_NUM_CUSTOM_ATTRS);
}

void FPCGGraphCompilerGPU::CreateDataInterfaces(
	FPCGGPUCompilationContext& InOutContext,
	const TArray<FPCGGraphTask>& InCompiledTasks,
	const TSet<FPCGTaskId>& InCollapsedTasks,
	const TArray<FPCGTaskId>& InComputeElementTasks,
	const FTaskToSuccessors& InTaskSuccessors,
	const FOriginalToVirtualPin& InOriginalToVirtualPin,
	const TMap<FPCGTaskId, UPCGComputeKernel*>& InTaskIdToKernel,
	UPCGComputeGraph* InOutComputeGraph,
	FPinToDataInterface& InOutPinToDataInterface)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGGraphCompilerGPU::CreateDataInterfaces);

	// Create data interfaces which allow kernels to read or write data. Each data interface is associated with a node output pin.
	// For CPU->GPU edges, an upload data interface is created. For GPU->CPU edges, a readback data interface is created.
	auto CreateOutputDataInterface = [&InOutContext, &InCollapsedTasks, &InCompiledTasks, &InTaskIdToKernel, InOutComputeGraph](FPCGTaskId InProducerTaskId, bool bRequiresExport, const FPCGPinProperties& InOutputPinProperties) -> UPCGComputeDataInterface*
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(CreateDataInterface);

		// The kernel that produced this data, if any (otherwise could be CPU node or a node from a different compute graph).
		const UPCGComputeKernel* ProducerKernel = nullptr;
		if (UPCGComputeKernel* const* FoundKernel = InTaskIdToKernel.Find(InProducerTaskId))
		{
			ProducerKernel = *FoundKernel;
		}

		const bool bUpstreamIsGPUTask = InCollapsedTasks.Contains(InProducerTaskId);

		EPCGDataType PinType = InOutputPinProperties.AllowedTypes;

		// Dynamically typed pins could have a different type
		const UPCGSettings* ProducerSettings = InCompiledTasks[InProducerTaskId].Node ? InCompiledTasks[InProducerTaskId].Node->GetSettings() : nullptr;
		const UPCGPin* ProducerPin = InCompiledTasks[InProducerTaskId].Node ? InCompiledTasks[InProducerTaskId].Node->GetOutputPin(InOutputPinProperties.Label) : nullptr;
		if (ProducerSettings && ProducerPin)
		{
			PinType = ProducerSettings->GetCurrentPinTypes(ProducerPin);
		}

		UPCGComputeDataInterface* DataInterface = nullptr;

		switch (PinType)
		{
		case EPCGDataType::Point:
		case EPCGDataType::Param:
		{
			UPCGDataCollectionDataInterface* DataInterfacePCGData = nullptr;

			if (bUpstreamIsGPUTask)
			{
				// Provides data for GPU -> GPU and GPU -> CPU edges.
				DataInterfacePCGData = InOutContext.NewObject_AnyThread<UPCGDataCollectionDataInterface>(InOutComputeGraph);
			}
			else
			{
				// Provides data for CPU -> GPU edge.
				DataInterfacePCGData = InOutContext.NewObject_AnyThread<UPCGDataCollectionUploadDataInterface>(InOutComputeGraph);
			}

			check(DataInterfacePCGData);
			DataInterfacePCGData->SetRequiresExport(bRequiresExport);

			if (ProducerPin)
			{
				DataInterfacePCGData->SetElementCountMultiplier(ProducerKernel ? ProducerKernel->GetElementCountMultiplier(ProducerPin->Properties.Label) : 1);
				DataInterfacePCGData->SetExposeElementCounters(ProducerKernel && ProducerKernel->DoesOutputPinRequireElementCounters(ProducerPin->Properties.Label));
				DataInterfacePCGData->SetRequiresZeroInitialization(ProducerKernel && ProducerKernel->DoesOutputPinRequireZeroInitialization(ProducerPin->Properties.Label));
			}

			DataInterface = DataInterfacePCGData;
			break;
		}
		case EPCGDataType::VirtualTexture:
		{
			DataInterface = InOutContext.NewObject_AnyThread<UPCGVirtualTextureDataInterface>(InOutComputeGraph);
			break;
		}
		case EPCGDataType::BaseTexture:
		case EPCGDataType::Texture:
		case EPCGDataType::RenderTarget:
		{
			UPCGTextureDataInterface* TextureDataInterface = InOutContext.NewObject_AnyThread<UPCGTextureDataInterface>(InOutComputeGraph);
			TextureDataInterface->SetRequiresExport(bRequiresExport);
			TextureDataInterface->SetInitializeFromDataCollection(!bUpstreamIsGPUTask);

			DataInterface = TextureDataInterface;

			break;
		}
		case EPCGDataType::Landscape:
		{
			DataInterface = InOutContext.NewObject_AnyThread<UPCGLandscapeDataInterface>(InOutComputeGraph);
			break;
		}
		case EPCGDataType::StaticMeshResource:
		{
			DataInterface = InOutContext.NewObject_AnyThread<UPCGStaticMeshDataInterface>(InOutComputeGraph);
			break;
		}
		default:
		{
			UE_LOG(LogPCG, Error, TEXT("Unsupported connected upstream pin '%s' on node '%s' with type %s. Consider adding a conversion to a supported type such as Point."),
				*InOutputPinProperties.Label.ToString(),
				InCompiledTasks[InProducerTaskId].Node ? *InCompiledTasks[InProducerTaskId].Node->GetNodeTitle(EPCGNodeTitleType::ListView).ToString() : TEXT("MISSING"),
				StaticEnum<EPCGDataType>() ? *StaticEnum<EPCGDataType>()->GetNameStringByValue(static_cast<int64>(PinType)) : TEXT("MISSING")
			);
			break;
		}
		}

		if (DataInterface)
		{
			DataInterface->ProducerKernel = ProducerKernel;
			DataInterface->SetProducerSettings(ProducerSettings);
			DataInterface->SetOutputPin(InOutputPinProperties.Label);
		}

		return DataInterface;
	};

	// Create DIs for output pins (either output pins within this compute graph or outside).
	for (FPCGTaskId TaskId : InCollapsedTasks)
	{
		// Create DIs for all output pins regardless of outbound connections, because the kernels currently need their outputs to be bound to valid resources.
		if (const UPCGSettings* Settings = InCompiledTasks[TaskId].Node ? InCompiledTasks[TaskId].Node->GetSettings() : nullptr)
		{
			// Create all the output data interfaces.
			for (const FPCGPinProperties& OutputPinProperties : Settings->AllOutputPinProperties())
			{
				if (OutputPinProperties.Label == NAME_None)
				{
					continue;
				}

				if (InOutPinToDataInterface.Contains({ TaskId, OutputPinProperties.Label }))
				{
					ensure(false);
					continue;
				}

				// Request buffer export from compute graph if any downstream task is outside of this compute graph.
				bool bRequiresExport = false;
				if (const TArray<FPCGTaskId>* Successors = InTaskSuccessors.Find(TaskId))
				{
					for (FPCGTaskId Successor : *Successors)
					{
						for (const FPCGGraphTaskInput& Input : InCompiledTasks[Successor].Inputs)
						{
							if (Input.TaskId == TaskId && Input.UpstreamPin && !InCollapsedTasks.Contains(Successor))
							{
								bRequiresExport = true;
								break;
							}
						}
					}
				}

				if (UPCGComputeDataInterface* OutputDI = CreateOutputDataInterface(TaskId, bRequiresExport, OutputPinProperties))
				{
					// Get the compute graph virtual pin corresponding to this pin, if one was created.
					const FName* VirtualPinLabel = InOriginalToVirtualPin.Find({ TaskId, OutputPinProperties.Label, /*bIsInputPin*/false });
					OutputDI->SetOutputPin(OutputPinProperties.Label, VirtualPinLabel);

					InOutComputeGraph->DataInterfaces.Add(OutputDI);

					InOutPinToDataInterface.Add({ TaskId, OutputPinProperties.Label }, OutputDI);

					// Iterate over downstream connections and register each downstream pin.
					if (const TArray<FPCGTaskId>* Successors = InTaskSuccessors.Find(TaskId))
					{
						for (FPCGTaskId Successor : *Successors)
						{
							for (const FPCGGraphTaskInput& Input : InCompiledTasks[Successor].Inputs)
							{
								if (Input.TaskId == TaskId && Input.UpstreamPin && Input.DownstreamPin)
								{
									// Get the compute graph virtual pin corresponding to this pin, if one was created.
									const FName* VirtualInputPinLabel = InOriginalToVirtualPin.Find({ TaskId, Input.DownstreamPin->Label, /*bIsInputPin=*/true });
									OutputDI->AddDownstreamInputPin(Input.DownstreamPin->Label, VirtualInputPinLabel);
								}
							}
						}
					}
				}
			}
		}

		// Create any DIs for upstream nodes outside of this compute graph.
		for (const FPCGGraphTaskInput& Input : InCompiledTasks[TaskId].Inputs)
		{
			// Never wire directly to the GPU-compatible tasks, these are collapsed into compute graphs (and will be culled in a next step).
			if (InCollapsedTasks.Contains(Input.TaskId) || InComputeElementTasks.Contains(Input.TaskId))
			{
				continue;
			}

			if (!Input.DownstreamPin.IsSet())
			{
				continue;
			}

			if (!Input.UpstreamPin.IsSet())
			{
				continue;
			}

			UPCGComputeDataInterface* OutputDI = nullptr;

			if (UPCGComputeDataInterface** FoundOutputDI = InOutPinToDataInterface.Find({ Input.TaskId, Input.UpstreamPin->Label }))
			{
				OutputDI = *FoundOutputDI;
			}
			else if (UPCGComputeDataInterface* NewOutputDI = CreateOutputDataInterface(Input.TaskId, /*bRequiresExport=*/false, *Input.UpstreamPin))
			{
				OutputDI = NewOutputDI;

				// Get the compute graph virtual pin corresponding to this pin, if one was created.
				const FName* VirtualPinLabel = InOriginalToVirtualPin.Find({ Input.TaskId, Input.UpstreamPin->Label, /*bIsInputPin*/false });
				OutputDI->SetOutputPin(Input.UpstreamPin->Label, VirtualPinLabel);

				InOutComputeGraph->DataInterfaces.Add(OutputDI);

				InOutPinToDataInterface.Add({ Input.TaskId, Input.UpstreamPin->Label }, OutputDI);
			}

			if (OutputDI)
			{
				// Get the compute graph virtual pin corresponding to this pin, if one was created.
				const FName* VirtualPinLabel = InOriginalToVirtualPin.Find({ TaskId, Input.DownstreamPin->Label, /*bIsInputPin=*/true });
				OutputDI->AddDownstreamInputPin(Input.DownstreamPin->Label, VirtualPinLabel);
			}
		}
	}
}

void FPCGGraphCompilerGPU::CompileComputeGraph(
	FPCGGPUCompilationContext& InOutContext,
	const TArray<FPCGGraphTask>& InCompiledTasks,
	const TSet<FPCGTaskId>& InCollapsedTasks,
	const TArray<FPCGTaskId>& InComputeElementTasks,
	const FTaskToSuccessors& InTaskSuccessors,
	const FOriginalToVirtualPin& InOriginalToVirtualPin,
	const FPinToDataInterface& InOutputPinToDataInterface,
	const TMap<FPCGTaskId, UPCGComputeKernel*>& InTaskIdToKernel,
	UPCGComputeGraph* InOutComputeGraph)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGGraphCompilerGPU::CompileComputeGraph);

	// Build compute graph by traversing the set of collapsed tasks in execution order (order of queued kernel invocations matters).
	TSet<FPCGTaskId> RemainingTasks;
	TArray<FPCGTaskId> ReadyTasks;
	RemainingTasks.Reserve(InCollapsedTasks.Num());
	ReadyTasks.Reserve(InCollapsedTasks.Num());

	for (FPCGTaskId TaskId : InCollapsedTasks)
	{
		bool bTaskReady = true;
		for (const FPCGGraphTaskInput& Input : InCompiledTasks[TaskId].Inputs)
		{
			if (InCollapsedTasks.Contains(Input.TaskId))
			{
				bTaskReady = false;
				break;
			}
		}

		if (bTaskReady)
		{
			ReadyTasks.Add(TaskId);
		}
		else
		{
			RemainingTasks.Add(TaskId);
		}
	}

	while (!ReadyTasks.IsEmpty())
	{
		FPCGTaskId TaskId = ReadyTasks.Pop();

		// Queue up any successors that are ready to go (do not depend on any remaining kernel).
		if (const TArray<FPCGTaskId>* Successors = InTaskSuccessors.Find(TaskId))
		{
			for (const FPCGTaskId Successor : *Successors)
			{
				if (!RemainingTasks.Contains(Successor))
				{
					continue;
				}

				bool bTaskReady = true;
				for (const FPCGGraphTaskInput& Input : InCompiledTasks[Successor].Inputs)
				{
					if (RemainingTasks.Contains(Input.TaskId) || ReadyTasks.Contains(Input.TaskId))
					{
						bTaskReady = false;
						break;
					}
				}

				if (bTaskReady)
				{
					ReadyTasks.Add(Successor);
					RemainingTasks.Remove(Successor);
				}
			}
		}

		const UPCGNode* Node = InCompiledTasks[TaskId].Node;

		const UPCGSettings* Settings = Node ? Node->GetSettings() : nullptr;
		check(Settings && Settings->bEnabled && Settings->ShouldExecuteOnGPU());

		struct FInterfaceBinding
		{
			const UComputeDataInterface* DataInterface = nullptr;
			int32 DataInterfaceBindingIndex = INDEX_NONE;
			FName Prefix = NAME_None;
			FName PinLabel = NAME_None;
			FString BindingFunctionName;
		};

		struct FKernelWithDataBindings
		{
			UPCGComputeKernel* Kernel = nullptr;
			TArray<FInterfaceBinding> InputDataBindings;
			TArray<FInterfaceBinding> OutputDataBindings;
		};

		FKernelWithDataBindings KernelWithBindings;

		const int32 KernelIndex = InOutComputeGraph->KernelInvocations.Num();
		KernelWithBindings.Kernel = InTaskIdToKernel[TaskId];
		check(KernelWithBindings.Kernel);
		KernelWithBindings.Kernel->SetKernelIndex(KernelIndex);

		InOutComputeGraph->KernelInvocations.Add(KernelWithBindings.Kernel);

		if (const UPCGStaticMeshSpawnerKernel* SMSpawnerKernel = Cast<UPCGStaticMeshSpawnerKernel>(KernelWithBindings.Kernel))
		{
			InOutComputeGraph->StaticMeshSpawners.Add(SMSpawnerKernel);
		}

		// Populate static data labels. Cannot happen during BuildComputeGraphStaticData() because this relies on kernel index, which is not initialized at that time.
		{
			auto GetDataLabels = [Kernel = KernelWithBindings.Kernel, InOutComputeGraph](const TArray<FPCGPinProperties>& PinProperties)
			{
				for (const FPCGPinProperties& PinProps : PinProperties)
				{
					TArray<FString> DataLabels;
					Kernel->GetDataLabels(PinProps.Label, DataLabels);

					if (!DataLabels.IsEmpty())
					{
						FPCGPinDataLabels& PinDataLabels = InOutComputeGraph->StaticDataLabelsTable.FindOrAdd(Kernel->GetKernelIndex());
						ensure(!PinDataLabels.PinToDataLabels.Contains(PinProps.Label));

						FPCGDataLabels& Labels = PinDataLabels.PinToDataLabels.Add(PinProps.Label);
						Labels.Labels = MoveTemp(DataLabels);
					}
				}
			};

			GetDataLabels(Settings->AllInputPinProperties());
			GetDataLabels(Settings->AllOutputPinProperties());
		}

		struct FDataInterfaceInfo
		{
			FDataInterfaceInfo(int InIndex, FName InPrefix, FName InPinLabel)
				: Index(InIndex)
				, Prefix(InPrefix)
				, PinLabel(InPinLabel)
			{}

			int Index = INDEX_NONE;
			FName Prefix = NAME_None;
			FName PinLabel = NAME_None;
		};

		const int NumInputPins = Settings->InputPinProperties().Num();
		const int NumOutputPins = Settings->OutputPinProperties().Num();

		TArray<FDataInterfaceInfo> InputDataInterfaces;
		TArray<FDataInterfaceInfo> OutputDataInterfaces;
		InputDataInterfaces.Reserve(NumInputPins);
		OutputDataInterfaces.Reserve(NumOutputPins);

		TMap</*OriginalPinLabel*/FName, /*VirtualPinLabel*/FName> OriginalToVirtualInputPin;
		OriginalToVirtualInputPin.Reserve(NumInputPins);

		auto CreateDataResolverDataInterface = [&InOutContext, Kernel = KernelWithBindings.Kernel, InOutComputeGraph](TArray<FDataInterfaceInfo>& InOutDataInterfaces, FName PinLabel, bool bIsInput)
		{
			// Add data label resolver DI if this pin uses any data labels.
			const FPCGPinDataLabels* PinDataLabels = InOutComputeGraph->GetStaticDataLabelsTable().Find(Kernel->GetKernelIndex());
			const FPCGDataLabels* DataLabels = PinDataLabels ? PinDataLabels->PinToDataLabels.Find(PinLabel) : nullptr;

			if (DataLabels && !DataLabels->Labels.IsEmpty())
			{
				UPCGDataLabelResolverDataInterface* Resolver = InOutContext.NewObject_AnyThread<UPCGDataLabelResolverDataInterface>(InOutComputeGraph);
				Resolver->Kernel = Kernel;
				Resolver->PinLabel = PinLabel;
				Resolver->bIsInput = bIsInput;

				const int ResolverIndex = InOutComputeGraph->DataInterfaces.Add(Resolver);
				InOutDataInterfaces.Emplace(ResolverIndex, FName(*PCGComputeHelpers::GetDataLabelResolverName(PinLabel)), /*PinLabel=*/NAME_None);
			}
		};

		// Setup input pin DIs.
		for (const FPCGGraphTaskInput& Input : InCompiledTasks[TaskId].Inputs)
		{
			// Currently the new compute graph tasks are wired into the graph in parallel to each GPU node task. The GPU node tasks
			// will be culled at the end, leaving only the compute graphs. Only create DIs for tasks within this compute graph (in InCollapsedTasks),
			// or for tasks that will not be culled (not in InAllGPUCompatibleTasks).
			const bool bValidInput = InCollapsedTasks.Contains(Input.TaskId) || !InComputeElementTasks.Contains(Input.TaskId);
			if (!bValidInput)
			{
				continue;
			}

			if (!Input.UpstreamPin || !Input.DownstreamPin)
			{
				// Execution-only dependencies not supported currently. Unclear if this should ever be supported for GPU graphs.
				// Writes followed by reads will be protected via barriers added by RDG.
				continue;
			}

			UPCGComputeDataInterface* const* FoundDI = InOutputPinToDataInterface.Find({ Input.TaskId, Input.UpstreamPin->Label });
			if (!FoundDI || !*FoundDI)
			{
				UE_LOG(LogPCG, Warning, TEXT("Failed to find data interface associated with pin '%s'."), *Input.UpstreamPin->Label.ToString());
				continue;
			}

			const int Index = InOutComputeGraph->DataInterfaces.Find(*FoundDI);
			if (Index == INDEX_NONE)
			{
				ensure(false);
				continue;
			}

			InputDataInterfaces.Emplace(Index, /*Prefix=*/Input.DownstreamPin->Label, /*PinLabel=*/Input.DownstreamPin->Label);

			if (const FName* VirtualPin = InOriginalToVirtualPin.Find(FNodePin(TaskId, Input.DownstreamPin->Label, /*IsInput*/true)))
			{
				OriginalToVirtualInputPin.Add(Input.DownstreamPin->Label, *VirtualPin);
			}

			CreateDataResolverDataInterface(InputDataInterfaces, Input.DownstreamPin->Label, /*bIsInput=*/true);
		}

		// Setup output pin DIs. Always bind a DI to every output pin, so kernel always has something to write to.
		for (const FPCGPinProperties& OutputPinProperties : Settings->AllOutputPinProperties())
		{
			if (OutputPinProperties.Label == NAME_None)
			{
				continue;
			}

			UPCGComputeDataInterface* const* FoundDI = InOutputPinToDataInterface.Find({ TaskId, OutputPinProperties.Label });
			if (!FoundDI || !*FoundDI)
			{
				UE_LOG(LogPCG, Warning, TEXT("Failed to find data interface associated with pin '%s'."), *OutputPinProperties.Label.ToString());
				continue;
			}

			const int Index = InOutComputeGraph->DataInterfaces.Find(*FoundDI);
			if (Index == INDEX_NONE)
			{
				ensure(false);
				continue;
			}

			OutputDataInterfaces.Emplace(Index, /*Prefix=*/OutputPinProperties.Label, /*PinLabel=*/OutputPinProperties.Label);
			CreateDataResolverDataInterface(OutputDataInterfaces, OutputPinProperties.Label, /*bIsInput=*/false);
		}

		// Kernel data interface.
		{
			UPCGCustomKernelDataInterface* KernelDI = InOutContext.NewObject_AnyThread<UPCGCustomKernelDataInterface>(InOutComputeGraph);
			KernelDI->SetSettings(Settings);
			KernelDI->Kernel = KernelWithBindings.Kernel;

			const int32 KernelDIIndex = InOutComputeGraph->DataInterfaces.Num();
			InOutComputeGraph->DataInterfaces.Add(KernelDI);

			// @todo_pcg: CustomKernel DI should probably have a prefix, e.g. "Kernel_"
			InputDataInterfaces.Emplace(KernelDIIndex, /*Prefix=*/NAME_None, /*PinLabel=*/NAME_None);
		}

		// Additional DIs created by settings.
		{
			TArray<TObjectPtr<UComputeDataInterface>> AdditionalInputDIs, AdditionalOutputDIs;
			KernelWithBindings.Kernel->CreateAdditionalInputDataInterfaces(InOutContext, InOutComputeGraph, AdditionalInputDIs);
			KernelWithBindings.Kernel->CreateAdditionalOutputDataInterfaces(InOutContext, InOutComputeGraph, AdditionalOutputDIs);

			auto AddAdditionalDataInterface = [InOutComputeGraph](TObjectPtr<UComputeDataInterface> DataInterface) -> int32
			{
				if (ensure(DataInterface))
				{
					const int32 DataInterfaceIndex = InOutComputeGraph->DataInterfaces.Num();
					InOutComputeGraph->DataInterfaces.Add(DataInterface);

					return DataInterfaceIndex;
				}

				return static_cast<int32>(INDEX_NONE);
			};

			for (TObjectPtr<UComputeDataInterface> DataInterface : AdditionalInputDIs)
			{
				// TODO: This could also produce a prefix?
				const int32 DataInterfaceIndex = AddAdditionalDataInterface(DataInterface);

				if (DataInterfaceIndex != INDEX_NONE)
				{
					InputDataInterfaces.Emplace(DataInterfaceIndex, /*Prefix=*/NAME_None, /*PinLabel=*/NAME_None);
				}
			}

			for (TObjectPtr<UComputeDataInterface> DataInterface : AdditionalOutputDIs)
			{
				// TODO: This could also produce a prefix?
				const int32 DataInterfaceIndex = AddAdditionalDataInterface(DataInterface);

				if (DataInterfaceIndex != INDEX_NONE)
				{
					OutputDataInterfaces.Emplace(DataInterfaceIndex, /*Prefix=*/NAME_None, /*PinLabel=*/NAME_None);
				}
			}
		}

		// Debug data interface.
		if (Settings->bPrintShaderDebugValues)
		{
			UPCGDebugDataInterface* DebugDI = InOutContext.NewObject_AnyThread<UPCGDebugDataInterface>(InOutComputeGraph);
			DebugDI->SetDebugBufferSize(Settings->DebugBufferSize);

			const int32 DebugDIIndex = InOutComputeGraph->DataInterfaces.Num();
			InOutComputeGraph->DataInterfaces.Add(DebugDI);

			// TODO: Maybe debug DI should be prefixed with 'Debug', e.g. 'Debug_WriteValue()'.
			OutputDataInterfaces.Emplace(DebugDIIndex, /*Prefix=*/NAME_None, /*PinLabel=*/NAME_None);
		}

		InOutComputeGraph->bLogDataDescriptions |= Settings->bDumpDataDescriptions;

		// Now that all data interfaces added, create the (trivial) binding mapping. All map to primary binding, index 0.
		InOutComputeGraph->DataInterfaceToBinding.SetNumZeroed(InOutComputeGraph->DataInterfaces.Num());

		InOutComputeGraph->KernelToNode.Add(Node);

		auto SetupAllInputBindings = [&KernelWithBindings, InOutComputeGraph](const FDataInterfaceInfo& InDataInterfaceInfo)
		{
			const UComputeDataInterface* DataInterface = InOutComputeGraph->DataInterfaces[InDataInterfaceInfo.Index];
			TArray<FShaderFunctionDefinition> Functions;
			DataInterface->GetSupportedInputs(Functions);

			for (int FuncIndex = 0; FuncIndex < Functions.Num(); ++FuncIndex)
			{
				FInterfaceBinding& Binding = KernelWithBindings.InputDataBindings.Emplace_GetRef();
				Binding.DataInterface = DataInterface;
				Binding.Prefix = InDataInterfaceInfo.Prefix;
				Binding.PinLabel = InDataInterfaceInfo.PinLabel;
				Binding.BindingFunctionName = Functions[FuncIndex].Name;
				Binding.DataInterfaceBindingIndex = FuncIndex;
			}
		};

		auto SetupAllOutputBindings = [&KernelWithBindings, InOutComputeGraph](const FDataInterfaceInfo& InDataInterfaceInfo)
		{
			const UComputeDataInterface* DataInterface = InOutComputeGraph->DataInterfaces[InDataInterfaceInfo.Index];
			TArray<FShaderFunctionDefinition> Functions;
			DataInterface->GetSupportedOutputs(Functions);

			for (int FuncIndex = 0; FuncIndex < Functions.Num(); ++FuncIndex)
			{
				FInterfaceBinding& Binding = KernelWithBindings.OutputDataBindings.Emplace_GetRef();
				Binding.DataInterface = DataInterface;
				Binding.Prefix = InDataInterfaceInfo.Prefix;
				Binding.PinLabel = InDataInterfaceInfo.PinLabel;
				Binding.BindingFunctionName = Functions[FuncIndex].Name;
				Binding.DataInterfaceBindingIndex = FuncIndex;
			}
		};

		// Bind data interfaces.
		for (const FDataInterfaceInfo& DataInterfaceInfo : InputDataInterfaces)
		{
			SetupAllInputBindings(DataInterfaceInfo);
		}

		for (const FDataInterfaceInfo& DataInterfaceInfo : OutputDataInterfaces)
		{
			SetupAllOutputBindings(DataInterfaceInfo);
		}

		{
			UPCGComputeKernelSource* KernelSource = InOutContext.NewObject_AnyThread<UPCGComputeKernelSource>(KernelWithBindings.Kernel);
			KernelWithBindings.Kernel->KernelSource = KernelSource;

			// These could be exposed through PCGSettings API later when the need arises (and/or when GPU feature matures).
			KernelSource->EntryPoint = KernelWithBindings.Kernel->GetEntryPoint();
			KernelSource->GroupSize = FIntVector(PCGComputeConstants::THREAD_GROUP_SIZE, 1, 1);

			// All kernels require ComputeShaderUtils.ush, so inject that before anything else.
			FString Source = TEXT("#include \"/Engine/Private/ComputeShaderUtils.ush\"\n\n") + KernelWithBindings.Kernel->GetCookedSource(InOutContext);
			KernelSource->SetSource(MoveTemp(Source));
			KernelWithBindings.Kernel->GatherAdditionalSources(KernelSource->AdditionalSources);

			if (Settings->bDumpCookedHLSL)
			{
				UE_LOG(LogPCG, Log, TEXT("Cooked HLSL:\n%s\n"), *KernelSource->GetSource());
			}

#if PCG_KERNEL_LOGGING_ENABLED
			if (PCGGraphCompilerGPU::CVarEnableDebugging.GetValueOnAnyThread())
			{
				UE_LOG(LogPCG, Display, TEXT("\n### STATIC METADATA ATTRIBUTE TABLE ###"));
				InOutComputeGraph->StaticAttributeTable.DebugLog();
			}
#endif

			// Add functions for external inputs/outputs which must be fulfilled by DIs
			for (FInterfaceBinding& Binding : KernelWithBindings.InputDataBindings)
			{
				TArray<FShaderFunctionDefinition> Functions;
				Binding.DataInterface->GetSupportedInputs(Functions);
				check(Functions.IsValidIndex(Binding.DataInterfaceBindingIndex));

				FShaderFunctionDefinition FuncDef = Functions[Binding.DataInterfaceBindingIndex];
				for (FShaderParamTypeDefinition& ParamType : FuncDef.ParamTypes)
				{
					// Making sure parameter has type declaration generated
					ParamType.ResetTypeDeclaration();
				}

				KernelSource->ExternalInputs.Emplace(FuncDef);
			}

			for (FInterfaceBinding& Binding : KernelWithBindings.OutputDataBindings)
			{
				TArray<FShaderFunctionDefinition> Functions;
				Binding.DataInterface->GetSupportedOutputs(Functions);
				check(Functions.IsValidIndex(Binding.DataInterfaceBindingIndex));

				FShaderFunctionDefinition FuncDef = Functions[Binding.DataInterfaceBindingIndex];
				for (FShaderParamTypeDefinition& ParamType : FuncDef.ParamTypes)
				{
					// Making sure parameter has type declaration generated
					ParamType.ResetTypeDeclaration();
				}

				KernelSource->ExternalOutputs.Emplace(FuncDef);
			}
		}

		auto AddAllEdgesForKernel = [&KernelWithBindings, InOutComputeGraph, &OriginalToVirtualInputPin](int32 InKernelIndex, bool bInEdgesAreInputs)
		{
			TArray<FInterfaceBinding>& Bindings = bInEdgesAreInputs ? KernelWithBindings.InputDataBindings : KernelWithBindings.OutputDataBindings;

			// Add all graph edges for bindings, which means include all functions that the data interfaces expose.
			for (int KernelBindingIndex = 0; KernelBindingIndex < Bindings.Num(); ++KernelBindingIndex)
			{
				FInterfaceBinding& Binding = Bindings[KernelBindingIndex];

				// Edge in compute graph is more correlated with a binding (it more describes a kernel input or output rather than a connection
				// between two kernels) so we use "binding" terminology.
				const int32 GraphBindingIndex = InOutComputeGraph->GraphEdges.Num();
				FComputeGraphEdge& Edge = InOutComputeGraph->GraphEdges.AddDefaulted_GetRef();

				Edge.KernelIndex = InKernelIndex;
				Edge.KernelBindingIndex = KernelBindingIndex;
				Edge.DataInterfaceIndex = InOutComputeGraph->DataInterfaces.IndexOfByPredicate([&Binding](const UComputeDataInterface* In) { return Binding.DataInterface == In; });
				check(Edge.DataInterfaceIndex != INDEX_NONE);
				Edge.DataInterfaceBindingIndex = Binding.DataInterfaceBindingIndex;
				Edge.bKernelInput = bInEdgesAreInputs;

				UComputeDataInterface* DataInterface = InOutComputeGraph->DataInterfaces[Edge.DataInterfaceIndex];
				check(DataInterface);

				if (Binding.Prefix != NAME_None)
				{
					TArray<FShaderFunctionDefinition> DataInterfaceFunctions;
					if (bInEdgesAreInputs)
					{
						DataInterface->GetSupportedInputs(DataInterfaceFunctions);
					}
					else
					{
						DataInterface->GetSupportedOutputs(DataInterfaceFunctions);
					}

					Edge.BindingFunctionNameOverride = FString::Format(
						TEXT("{0}_{1}"),
						{ Binding.Prefix.ToString(), DataInterfaceFunctions[Edge.DataInterfaceBindingIndex].Name }
					);
				}

				if (Binding.PinLabel != NAME_None)
				{
					// A binding corresponds to a single API within a data interface, like GetNumData() for example. There are multiple bindings
					// per PCG graph edge and we only need to create our mappings for the first binding.
					if (Edge.DataInterfaceBindingIndex == 0)
					{
						const FPCGKernelPin KernelPin(InKernelIndex, Binding.PinLabel, bInEdgesAreInputs);

						InOutComputeGraph->KernelBindingToPinLabel.Add(GraphBindingIndex, Binding.PinLabel);

						if (!InOutComputeGraph->KernelPinToFirstBinding.Contains(KernelPin))
						{
							InOutComputeGraph->KernelPinToFirstBinding.Add(KernelPin, GraphBindingIndex);
						}

						if (const FName* VirtualPin = bInEdgesAreInputs ? OriginalToVirtualInputPin.Find(Binding.PinLabel) : nullptr)
						{
							InOutComputeGraph->CPUDataBindingToVirtualPinLabel.Add(GraphBindingIndex, *VirtualPin);
						}

						UPCGComputeDataInterface* PCGDataInterface = Cast<UPCGComputeDataInterface>(DataInterface);
						if (PCGDataInterface && PCGDataInterface->GraphBindingIndex == INDEX_NONE)
						{
							PCGDataInterface->GraphBindingIndex = GraphBindingIndex;
						}
					}
				}
			}
		};

		AddAllEdgesForKernel(KernelIndex, /*bInEdgesAreInputs=*/true);
		AddAllEdgesForKernel(KernelIndex, /*bInEdgesAreInputs=*/false);
	}

	ensureMsgf(RemainingTasks.IsEmpty(), TEXT("PCG GPU graph compiler did not consume all tasks, %d/%d remaining."), RemainingTasks.Num(), InCollapsedTasks.Num());

	// Setup DownstreamToUpstreamBinding to assist in traversing up kernel->kernel connections.
	// TODO: optimize quadratic loop.
	for (int32 DownstreamEdgeIndex = 0; DownstreamEdgeIndex < InOutComputeGraph->GraphEdges.Num(); ++DownstreamEdgeIndex)
	{
		const FComputeGraphEdge& DownstreamEdge = InOutComputeGraph->GraphEdges[DownstreamEdgeIndex];
		const bool bFirstEdgeOfDI = DownstreamEdge.DataInterfaceBindingIndex == 0;

		// Need only one direction (only need to follow edges upstream).
		if (DownstreamEdge.bKernelInput && bFirstEdgeOfDI)
		{
			// Find corresponding matching edge - different kernel, same data interface, and bound as output.
			for (int32 UpstreamEdgeIndex = 0; UpstreamEdgeIndex < InOutComputeGraph->GraphEdges.Num(); ++UpstreamEdgeIndex)
			{
				const FComputeGraphEdge& UpstreamEdge = InOutComputeGraph->GraphEdges[UpstreamEdgeIndex];
				const bool bFirstEdgeOfUpstreamDI = UpstreamEdge.DataInterfaceBindingIndex == 0;

				if (bFirstEdgeOfUpstreamDI && UpstreamEdge.KernelIndex != DownstreamEdge.KernelIndex && UpstreamEdge.DataInterfaceIndex == DownstreamEdge.DataInterfaceIndex && !UpstreamEdge.bKernelInput)
				{
#if PCG_DATA_DESCRIPTION_LOGGING
					UE_LOG(LogPCG, Warning, TEXT("Connecting edge %d %s (KBD: %d) to edge %d %s (KBD: %d)"),
						DownstreamEdge.KernelIndex,
						*DownstreamEdge.BindingFunctionNameOverride,
						DownstreamEdgeIndex,
						UpstreamEdge.KernelIndex,
						*UpstreamEdge.BindingFunctionNameOverride,
						UpstreamEdge.KernelBindingIndex);

					UE_LOG(LogPCG, Warning, TEXT("\tUpstream is kernel %d, DI '%s', edge index %d ('%s')"),
						UpstreamEdge.KernelIndex,
						*InOutComputeGraph->DataInterfaces[UpstreamEdge.DataInterfaceIndex]->GetName(),
						UpstreamEdgeIndex,
						*InOutComputeGraph->GraphEdges[UpstreamEdge.KernelBindingIndex].BindingFunctionNameOverride);
					UE_LOG(LogPCG, Warning, TEXT("\tDownstream is kernel %d, DI '%s', edge index %d ('%s')"),
						DownstreamEdge.KernelIndex,
						*InOutComputeGraph->DataInterfaces[DownstreamEdge.DataInterfaceIndex]->GetName(),
						DownstreamEdgeIndex,
						*InOutComputeGraph->GraphEdges[DownstreamEdge.KernelBindingIndex].BindingFunctionNameOverride);
#endif

					InOutComputeGraph->DownstreamToUpstreamBinding.Add(DownstreamEdgeIndex, UpstreamEdgeIndex);
				}
			}
		}
	}
}

void FPCGGraphCompilerGPU::CreateGPUNodes(FPCGGraphCompiler& InOutCompiler, UPCGGraph* InGraph, uint32 InGridSize, TArray<FPCGGraphTask>& InOutCompiledTasks)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGGraphCompilerGPU::CreateGPUNodes);

	if (!InGraph)
	{
		ensure(false);
		return;
	}

	FPCGGPUCompilationContext Context(InOutCompiler);

	TSet<FPCGTaskId> GPUCompatibleTaskIds;
	GPUCompatibleTaskIds.Reserve(InOutCompiledTasks.Num());
	for (FPCGTaskId TaskId = 0; TaskId < InOutCompiledTasks.Num(); ++TaskId)
	{
		const UPCGNode* Node = InOutCompiledTasks[TaskId].Node;
		const UPCGSettings* Settings = Node ? Node->GetSettings() : nullptr;
		if (Settings && Settings->ShouldExecuteOnGPU() && Settings->bEnabled)
		{
			GPUCompatibleTaskIds.Add(TaskId);
		}
	}

	if (GPUCompatibleTaskIds.IsEmpty())
	{
		// Nothing to do for this graph.
		return;
	}

	// For input pins at CPU -> GPU boundary, inject gather elements to pre-combine data on CPU side
	// before passing to GPU.
	CreateGatherTasksAtGPUInputs(InGraph, GPUCompatibleTaskIds, InOutCompiledTasks);

	TMap<FPCGTaskId, UPCGComputeKernel*> TaskIdToKernel;

	// Create one or more kernel objects for each GPU node. A graph task will be created for each.
	ExpandGPUNodeKernelsToTasks(Context, GPUCompatibleTaskIds, InOutCompiledTasks, TaskIdToKernel);

	FTaskToSuccessors TaskSuccessors;
	TaskSuccessors.Reserve(InOutCompiledTasks.Num());
	for (FPCGTaskId TaskId = 0; TaskId < InOutCompiledTasks.Num(); ++TaskId)
	{
		for (int InputIndex = 0; InputIndex < InOutCompiledTasks[TaskId].Inputs.Num(); ++InputIndex)
		{
			// Only add unique task successors to avoid storing the data from each output pin
			// multiple times when multiple pins connect to the same downstream node.
			TaskSuccessors.FindOrAdd(InOutCompiledTasks[TaskId].Inputs[InputIndex].TaskId).AddUnique(TaskId);
		}
	}

	TArray<TSet<FPCGTaskId>> NodeSubsetsToConvertToCFGraph;
	CollectGPUNodeSubsets(InOutCompiledTasks, TaskSuccessors, GPUCompatibleTaskIds, TaskIdToKernel, NodeSubsetsToConvertToCFGraph);

	const uint32 NumComputeGraphs = NodeSubsetsToConvertToCFGraph.Num();

	// Mapping from task ID & pin label to a virtual pin label. Compute graphs are executed within a generated element,
	// and the input and output pins of this element must have unique virtual pin labels so that we can parse the data that
	// PCG provides through the input data collection correctly, and route the output data to the downstream pins correctly.
	TArray<FOriginalToVirtualPin> OriginalToVirtualPin;
	OriginalToVirtualPin.SetNum(NumComputeGraphs);

	// Setup mappings from existing pins to compute graph element virtual pins as a prestep before wiring in the compute graph tasks.
	for (uint32 ComputeGraphIndex = 0; ComputeGraphIndex < NumComputeGraphs; ++ComputeGraphIndex)
	{
		SetupVirtualPins(NodeSubsetsToConvertToCFGraph[ComputeGraphIndex], InOutCompiledTasks, TaskSuccessors, OriginalToVirtualPin[ComputeGraphIndex]);
	}

	TArray<FPCGTaskId> ComputeElementTasks;

	// Build each compute graph.
	for (uint32 ComputeGraphIndex = 0; ComputeGraphIndex < NumComputeGraphs; ++ComputeGraphIndex)
	{
		TSet<FPCGTaskId>& NodeSubsetToConvertToCFGraph = NodeSubsetsToConvertToCFGraph[ComputeGraphIndex];

		if (NodeSubsetToConvertToCFGraph.IsEmpty())
		{
			ensure(false);
			continue;
		}

		// Add a new compute graph task. The original GPU tasks will then be culled later below.
		const FPCGTaskId ComputeGraphTaskId = InOutCompiledTasks.Num();
		ComputeElementTasks.Add(ComputeGraphTaskId);
		FPCGGraphTask& ComputeGraphTask = InOutCompiledTasks.Emplace_GetRef();
		ComputeGraphTask.NodeId = ComputeGraphTaskId;

		// All nodes in subset will be from same stack/parent, so assign from any.
		if (auto It = NodeSubsetToConvertToCFGraph.CreateConstIterator(); It)
		{
			FPCGTaskId GPUTaskId = *It;
			ComputeGraphTask.ParentId = InOutCompiledTasks[GPUTaskId].ParentId;
			ComputeGraphTask.StackIndex = InOutCompiledTasks[GPUTaskId].StackIndex;
		}

		// Wire in the compute graph task, side by side with the individual GPU tasks, which will be culled below.
		WireComputeGraphTask(
			ComputeGraphTaskId,
			NodeSubsetToConvertToCFGraph,
			InOutCompiledTasks,
			TaskSuccessors,
			OriginalToVirtualPin[ComputeGraphIndex]);

		// Generate a compute graph from all of the individual GPU tasks.
		BuildComputeGraphTask(
			Context,
			ComputeGraphTaskId,
			InGraph,
			InGridSize,
			ComputeGraphIndex,
			NodeSubsetToConvertToCFGraph,
			ComputeElementTasks,
			TaskSuccessors,
			InOutCompiledTasks,
			OriginalToVirtualPin[ComputeGraphIndex],
			TaskIdToKernel);
	}

	{
		FWriteScopeLock Lock(InOutCompiler.GetCache().GraphToTaskMapLock);
		TMap<uint32, TArray<TObjectPtr<UPCGComputeGraph>>>& GridSizeToComputeGraphs = InOutCompiler.GetCache().TopGraphToComputeGraphMap.FindOrAdd(InGraph);
		TArray<TObjectPtr<UPCGComputeGraph>>& ComputeGraphs = GridSizeToComputeGraphs.FindOrAdd(InGridSize);

		// Replace any existing compute graphs with the newly compiled ones. It's okay if multiple threads do this, because
		// compute graph index order should be deterministic, so different threads will produce the same results.
		ComputeGraphs = MoveTemp(Context.GetCompiledComputeGraphs());
		ensure(ComputeGraphs.Num() == NumComputeGraphs);
	}

	// Now cull all the GPU compatible nodes. The compute graph task are already wired in so we're fine to just delete.
	FPCGGraphCompiler::CullTasks(InOutCompiledTasks, /*bAddPassthroughWires=*/false, [&NodeSubsetsToConvertToCFGraph](const FPCGGraphTask& InTask)
	{
		for (TSet<FPCGTaskId>& NodeSubsetToConvertToCFGraph : NodeSubsetsToConvertToCFGraph)
		{
			if (NodeSubsetToConvertToCFGraph.Contains(InTask.NodeId))
			{
				return true;
			}
		}

		return false;
	});
}

#undef PCG_DATA_DESCRIPTION_LOGGING 

#endif // WITH_EDITOR
