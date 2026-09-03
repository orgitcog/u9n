// Copyright Epic Games, Inc. All Rights Reserved.

#include "SceneStateMachineCompiler.h"
#include "Algo/Count.h"
#include "EdGraphUtilities.h"
#include "ISceneStateMachineCompilerContext.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/EnumerateRange.h"
#include "Nodes/SceneStateMachineConduitNode.h"
#include "Nodes/SceneStateMachineEntryNode.h"
#include "Nodes/SceneStateMachineExitNode.h"
#include "Nodes/SceneStateMachineStateNode.h"
#include "Nodes/SceneStateMachineTransitionNode.h"
#include "Nodes/SceneStateTransitionResultNode.h"
#include "SceneState.h"
#include "SceneStateGeneratedClass.h"
#include "SceneStateMachine.h"
#include "SceneStateMachineConduitCompiler.h"
#include "SceneStateMachineGraph.h"
#include "SceneStateMachineTaskCompiler.h"
#include "SceneStateMachineTransitionCompiler.h"
#include "SceneStateTransitionGraph.h"
#include "SceneStateUtils.h"
#include "Tasks/SceneStateBlueprintableTask.h"
#include "UObject/UObjectThreadContext.h"

namespace UE::SceneState::Editor
{

namespace Private
{

// Converts Transition Parameters to map of absolute index to the parameter property bag, only if they're valid.
// A map is used because most likely there will be much more transitions without parameters than transitions with parameters.
TMap<uint16, FInstancedPropertyBag> ConvertParameterMap(FSceneStateRange InTransitionRange, TArrayView<FInstancedPropertyBag> InParametersList)
{
	TMap<uint16, FInstancedPropertyBag> TransitionParameterMap;
	TransitionParameterMap.Reserve(Algo::CountIf(InParametersList,
		[](const FInstancedPropertyBag& InParameters)
		{
			return InParameters.IsValid();
		}));

	for (TEnumerateRef<FInstancedPropertyBag> Parameters : EnumerateRange(InParametersList))
	{
		if (Parameters->IsValid())
		{
			// Convert the index to absolute
			const uint16 AbsoluteIndex = InTransitionRange.Index + Parameters.GetIndex();
			TransitionParameterMap.Add(AbsoluteIndex, MoveTemp(*Parameters));
		}
	}
	return TransitionParameterMap;
}

} // Private

FStateMachineCompiler::FStateMachineCompiler(USceneStateMachineGraph* InGraph, IStateMachineCompilerContext& InCompilerContext)
	: StateMachineGraph(InGraph)
	, Context(InCompilerContext)
{
	GeneratedClass = InCompilerContext.GetGeneratedClass();
	check(StateMachineGraph && GeneratedClass);
	Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(InGraph);
}

FSceneStateMachine FStateMachineCompiler::Compile()
{
	check(StateMachineGraph && GeneratedClass);

	FSceneStateMachine StateMachine;

	USceneStateMachineEntryNode* EntryNode = StateMachineGraph->GetEntryNode();
	if (!EntryNode)
	{
		return StateMachine;
	}

	USceneStateMachineStateNode* EntryState = EntryNode->GetStateNode();
	if (!EntryState)
	{
		return StateMachine;
	}

	StateNodesToProcess.Reset();
	StateNodesToProcess.Emplace(EntryState);

	StateMachine.EntryIndex = 0;
	StateMachine.Parameters = StateMachineGraph->Parameters;

	// Step #1: Compile States
	while (!StateNodesToProcess.IsEmpty())
	{
		if (const USceneStateMachineStateNode* StateNode = StateNodesToProcess.Pop())
		{
			CompileState(StateNode);	
		}
	}

	// Step #2: Compile Conduits
	for (const USceneStateMachineConduitNode* ConduitNode : ConduitNodesToCompile)
	{
		check(ConduitNodeIndexMap.Contains(ConduitNode));
		CompileConduit(ConduitNode);
	}

	// Step #3: Compile State Transitions
	// State array is filled with all the states, so they can be identified by indices now
	for (const TPair<FObjectKey, uint16>& Pair : StateNodeIndexMap)
	{
		const USceneStateMachineStateNode* StateNode = CastChecked<USceneStateMachineStateNode>(Pair.Key.ResolveObjectPtr());

		FSceneState& SceneState = States[Pair.Value];
		CompileStateTransitions(SceneState, StateNode);
	}

	StateMachine.StateRange.Index = GeneratedClass->States.Num();
	StateMachine.StateRange.Count = States.Num();

	GeneratedClass->States.Append(MoveTemp(States));
	GeneratedClass->StateMetadata.Append(MoveTemp(StateMetadata));

	States.Reset();
	StateMetadata.Reset();

	// Upgrade the map to absolute indices before baking it to the generated class' map
	ToAbsoluteIndexMap(StateNodeIndexMap, StateMachine.StateRange.Index);
	GeneratedClass->StateNodeToIndex.Append(MoveTemp(StateNodeIndexMap));

	StateMachine.ConduitRange.Index = GeneratedClass->Conduits.Num();
	StateMachine.ConduitRange.Count = Conduits.Num();

	GeneratedClass->Conduits.Append(MoveTemp(Conduits));
	GeneratedClass->ConduitLinks.Append(MoveTemp(ConduitLinks));

	Conduits.Reset();
	ConduitLinks.Reset();

	return StateMachine;
}

void FStateMachineCompiler::CompileState(const USceneStateMachineStateNode* InStateNode)
{
	if (!InStateNode->IsNodeEnabled())
	{
		return;
	}

	if (StateNodeIndexMap.Contains(InStateNode))
	{
		return;
	}

	const int32 StateIndex = States.AddDefaulted();
	const int32 StateMetadataIndex = StateMetadata.AddDefaulted();

	check(StateIndex == StateMetadataIndex);

	FSceneState& NewState = States[StateIndex];

	FSceneStateMetadata& NewStateMetadata = StateMetadata[StateIndex];
	NewStateMetadata.StateName = InStateNode->GetNodeName().ToString();

	StateNodeIndexMap.Add(InStateNode, StateIndex);

	CompileSubStateMachines(NewState, InStateNode);
	CompileTasks(NewState, InStateNode);
	CompileEventHandlers(NewState, InStateNode);

	// Gather more states to process by following the linked transitions
	const TArray<USceneStateMachineTransitionNode*> TransitionNodes = InStateNode->GatherTransitions(/*bInSortList*/true);
	FollowTransitions(TransitionNodes);
}

void FStateMachineCompiler::FollowTransitions(TConstArrayView<USceneStateMachineTransitionNode*> InExitTransitions)
{
	StateNodesToProcess.Reserve(StateNodesToProcess.Num() + InExitTransitions.Num());

	for (USceneStateMachineTransitionNode* const TransitionNode : InExitTransitions)
	{
		check(TransitionNode);

		const USceneStateMachineNode* TargetNode = TransitionNode->GetTargetNode();

		if (const USceneStateMachineStateNode* TargetStateNode = Cast<USceneStateMachineStateNode>(TargetNode))
		{
			StateNodesToProcess.Add(TargetStateNode);
		}
		else if (const USceneStateMachineConduitNode* TargetConduitNode = Cast<USceneStateMachineConduitNode>(TargetNode))
		{
			AddConduitToCompile(TargetConduitNode);
		}
	}
}

void FStateMachineCompiler::CompileSubStateMachines(FSceneState& InNewState, const USceneStateMachineStateNode* InStateNode)
{
	// Compile Sub State Machine Graphs into Runtime State Machines owned by this new State
	TArray<UEdGraph*> SubGraphs = InStateNode->GetSubGraphs();

	TArray<FSceneStateMachine> SubStateMachines;
	SubStateMachines.Reserve(SubGraphs.Num());

	TMap<FObjectKey, uint16> StateMachineGraphToIndex;
	StateMachineGraphToIndex.Reserve(SubGraphs.Num());

	for (UEdGraph* SubGraph : SubGraphs)
	{
		USceneStateMachineGraph* NewStateMachineGraph = Cast<USceneStateMachineGraph>(SubGraph);
		if (!NewStateMachineGraph)
		{
			continue;	
		}

		FStateMachineCompiler StateMachineCompiler(NewStateMachineGraph, Context);
		FSceneStateMachine NewStateMachine = StateMachineCompiler.Compile();
		if (NewStateMachine.IsValid())
		{
			StateMachineGraphToIndex.Emplace(NewStateMachineGraph, StateMachineGraphToIndex.Num());
			SubStateMachines.Emplace(MoveTemp(NewStateMachine));
		}
	}

	InNewState.StateMachineRange.Index = GeneratedClass->StateMachines.Num();
	InNewState.StateMachineRange.Count = SubStateMachines.Num();
	GeneratedClass->StateMachines.Append(MoveTemp(SubStateMachines));

	// Upgrade the map to absolute indices before baking it to the generated class' map
	ToAbsoluteIndexMap(StateMachineGraphToIndex, InNewState.StateMachineRange.Index);
	GeneratedClass->StateMachineGraphToIndex.Append(MoveTemp(StateMachineGraphToIndex));
}

void FStateMachineCompiler::AddConduitToCompile(const USceneStateMachineConduitNode* InConduitNode)
{
	if (!InConduitNode->IsNodeEnabled())
	{
		return;
	}

	// Conduit already added to nodes to process list
	if (ConduitNodeIndexMap.Contains(InConduitNode))
	{
		return;
	}

	// No exit transitions, skip compile
	TArray<USceneStateMachineTransitionNode*> ConduitTransitions = InConduitNode->GatherTransitions(/*bInSortList*/true);
	if (ConduitTransitions.IsEmpty())
	{
		return;
	}

	// Add defaulted conduit for now for indexing/discovery.
	const int32 ConduitIndex = ConduitNodesToCompile.Add(InConduitNode);
	ConduitNodeIndexMap.Add(InConduitNode, ConduitIndex);

	// Gather more nodes to process by following the exit transitions of the conduit
	FollowTransitions(ConduitTransitions);
}

void FStateMachineCompiler::CompileTasks(FSceneState& InNewState, const USceneStateMachineStateNode* InStateNode)
{
	FStateMachineTaskCompiler::FCompileResult TaskCompileResult;
	{
		FStateMachineTaskCompiler TaskCompiler(InStateNode->GetTaskPin(), /*Outer*/GeneratedClass);
		TaskCompiler.Compile(TaskCompileResult);
	}

	InNewState.TaskRange.Index = GeneratedClass->Tasks.Num();
	InNewState.TaskRange.Count = TaskCompileResult.Tasks.Num();

	// Convert the prerequisite range from relative to absolute
	const uint16 TaskPrerequisiteIndex = GeneratedClass->TaskPrerequisites.Num();
	for (FInstancedStruct& Task : TaskCompileResult.Tasks)
	{
		Task.GetMutable<FSceneStateTask>().PrerequisiteRange.Index += TaskPrerequisiteIndex;
	}

	// todo: bigger chunks to reduce FInstancedStructContainer realloc
	GeneratedClass->Tasks.Append(MoveTemp(TaskCompileResult.Tasks));
	GeneratedClass->TaskInstances.Append(TaskCompileResult.TaskInstances);
	GeneratedClass->TaskMetadata.Append(MoveTemp(TaskCompileResult.TaskMetadata));
	GeneratedClass->TaskPrerequisites.Append(MoveTemp(TaskCompileResult.TaskPrerequisites));

	InNewState.InstanceTaskObjects(GeneratedClass
		, GetStructViews(GeneratedClass->TaskInstances, InNewState.TaskRange)
		, TaskCompileResult.TaskInstances
		, [this](FObjectDuplicationParameters& InParams)->UObject*
		{
			return this->DuplicateObject(InParams);
		});

	ToAbsoluteIndexMap(TaskCompileResult.TaskToIndexMap, InNewState.TaskRange.Index);
	GeneratedClass->TaskNodeToIndex.Append(MoveTemp(TaskCompileResult.TaskToIndexMap));
}

void FStateMachineCompiler::CompileEventHandlers(FSceneState& InNewState, const USceneStateMachineStateNode* InStateNode)
{
	InNewState.EventHandlerRange.Index = GeneratedClass->EventHandlers.Num();
	InNewState.EventHandlerRange.Count = InStateNode->EventHandlers.Num();

	GeneratedClass->EventHandlers.Append(InStateNode->EventHandlers);
}

void FStateMachineCompiler::FinishTransitionCompilation(FSceneStateRange& OutTransitionRange, FStateMachineTransitionCompileResult&& InCompileResult)
{
	OutTransitionRange.Index = GeneratedClass->Transitions.Num();
	OutTransitionRange.Count = InCompileResult.Transitions.Num();

	GeneratedClass->Transitions.Append(MoveTemp(InCompileResult.Transitions));
	GeneratedClass->TransitionLinks.Append(MoveTemp(InCompileResult.Links));
	GeneratedClass->TransitionMetadata.Append(MoveTemp(InCompileResult.Metadata));
	GeneratedClass->TransitionParameters.Append(Private::ConvertParameterMap(OutTransitionRange, InCompileResult.Parameters));
}

void FStateMachineCompiler::CompileStateTransitions(FSceneState& InNewState, const USceneStateMachineStateNode* InStateNode)
{
	FStateMachineTransitionCompileResult TransitionCompileResult;

	const FStateMachineTransitionCompiler::FCompileParams CompileParams
		{
			.Context = Context,
			.Node = InStateNode,
			.StateNodeIndexMap = StateNodeIndexMap,
			.ConduitNodeIndexMap = ConduitNodeIndexMap,
		};

	FStateMachineTransitionCompiler TransitionCompiler(CompileParams);
	TransitionCompiler.Compile(TransitionCompileResult);

	FinishTransitionCompilation(InNewState.TransitionRange, MoveTemp(TransitionCompileResult));
}

void FStateMachineCompiler::CompileConduit(const USceneStateMachineConduitNode* InConduitNode)
{
	FStateMachineConduitCompileResult ConduitCompileResult;

	const FStateMachineConduitCompiler::FCompileParams CompileParams
		{
			.Context = Context,
			.ConduitNode = InConduitNode,
			.StateNodeIndexMap = StateNodeIndexMap,
			.ConduitNodeIndexMap = ConduitNodeIndexMap,
		};

	FStateMachineConduitCompiler ConduitCompiler(CompileParams);
	if (!ConduitCompiler.Compile(ConduitCompileResult))
	{
		return;
	}

	const uint16 ConduitIndex = ConduitNodeIndexMap[InConduitNode];

	Conduits.Insert(MoveTemp(ConduitCompileResult.Conduit), ConduitIndex);
	ConduitLinks.Insert(MoveTemp(ConduitCompileResult.ConduitLink), ConduitIndex);

	FinishTransitionCompilation(Conduits[ConduitIndex].TransitionRange, MoveTemp(ConduitCompileResult.TransitionCompileResult));
}

UObject* FStateMachineCompiler::DuplicateObject(FObjectDuplicationParameters& InDuplicationParams)
{
	TMap<UObject*, UObject*> DuplicationMap;

	// if recompiling the BP on load, skip post load and defer it to the loading process
	FUObjectSerializeContext* LinkerLoadingContext = nullptr;

	if (Blueprint && Blueprint->bIsRegeneratingOnLoad)
	{
		if (FLinkerLoad* Linker = Blueprint->GetLinker())
		{
			LinkerLoadingContext = FUObjectThreadContext::Get().GetSerializeContext();
		}
		InDuplicationParams.bSkipPostLoad = true;
		InDuplicationParams.CreatedObjects = &DuplicationMap;
	}

	UObject* DuplicateObject = StaticDuplicateObjectEx(InDuplicationParams);

	// if we have anything in here after duplicate, then hook them in the loading process so they get post loaded
	if (LinkerLoadingContext)
	{
		TArray<UObject*> DupObjects;
		DuplicationMap.GenerateValueArray(DupObjects);
		LinkerLoadingContext->AddUniqueLoadedObjects(DupObjects);
	}

	return DuplicateObject;
}

} // UE::SceneState::Editor
