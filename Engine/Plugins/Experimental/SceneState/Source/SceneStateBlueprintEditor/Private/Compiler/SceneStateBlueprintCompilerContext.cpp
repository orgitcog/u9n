// Copyright Epic Games, Inc. All Rights Reserved.

#include "SceneStateBlueprintCompilerContext.h"
#include "Kismet2/KismetReinstanceUtilities.h"
#include "SceneStateBindingCompiler.h"
#include "SceneStateBlueprint.h"
#include "SceneStateGeneratedClass.h"
#include "SceneStateMachine.h"
#include "SceneStateMachineCompiler.h"
#include "SceneStateMachineGraph.h"
#include "SceneStateTransitionGraphCompiler.h"
#include "SceneStateUtils.h"

namespace UE::SceneState::Editor
{

namespace Private
{

void TrashObject(UObject* InObject, FName InBaseName, bool bInClearFlags, bool bInRecompilingOnLoad)
{
	// Rename will remove the renamed object's linker when moving to a new package so invalidate the export beforehand
	FLinkerLoad::InvalidateExport(InObject);
	InObject->SetFlags(RF_Transient);
	if (bInClearFlags)
	{
		InObject->ClearFlags(RF_Public | RF_Standalone | RF_ArchetypeObject);
	}

	ERenameFlags RenameFlags = REN_DontCreateRedirectors | REN_NonTransactional | REN_DoNotDirty;
	UPackage* const TransientPackage = GetTransientPackage();
	if (InBaseName.IsNone())
	{
		InObject->Rename(nullptr, TransientPackage, RenameFlags);
	}
	else
	{
		FName UniqueName = MakeUniqueObjectName(TransientPackage, InObject->GetClass(), InBaseName);
		InObject->Rename(*UniqueName.ToString(), TransientPackage, RenameFlags);
	}
};

TArray<USceneStateMachineGraph*> GetStateMachines(TConstArrayView<UEdGraph*> InStateMachineGraphs)
{
	TArray<USceneStateMachineGraph*> StateMachineGraphs;
	StateMachineGraphs.Reserve(InStateMachineGraphs.Num());

	for (UEdGraph* Graph : InStateMachineGraphs)
	{
		if (USceneStateMachineGraph* StateMachineGraph = Cast<USceneStateMachineGraph>(Graph))
		{
			StateMachineGraphs.Add(StateMachineGraph);
		}
	}

	return StateMachineGraphs;
}

} // Private
	
FBlueprintCompilerContext::FBlueprintCompilerContext(USceneStateBlueprint* InBlueprint, FCompilerResultsLog& InMessageLog, const FKismetCompilerOptions& InCompilerOptions)
	: FKismetCompilerContext(InBlueprint, InMessageLog, InCompilerOptions)
{
}

void FBlueprintCompilerContext::SpawnNewClass(const FString& InNewClassName)
{
	check(Blueprint);
	NewGeneratedClass = FindObject<USceneStateGeneratedClass>(Blueprint->GetOutermost(), *InNewClassName);

	if (!NewGeneratedClass)
	{
		NewGeneratedClass = NewObject<USceneStateGeneratedClass>(Blueprint->GetOutermost(), *InNewClassName, RF_Public | RF_Transactional);
	}
	else
	{
		// Already existed, but wasn't linked in the Blueprint yet due to load ordering issues
		NewGeneratedClass->ClassGeneratedBy = Blueprint;
		FBlueprintCompileReinstancer::Create(NewGeneratedClass);
	}

	NewClass = NewGeneratedClass;
}

void FBlueprintCompilerContext::OnNewClassSet(UBlueprintGeneratedClass* InClassToUse)
{
	NewGeneratedClass = CastChecked<USceneStateGeneratedClass>(InClassToUse);
}

void FBlueprintCompilerContext::EnsureProperGeneratedClass(UClass*& InOutTargetClass)
{
	if (InOutTargetClass && !InOutTargetClass->UObject::IsA<USceneStateGeneratedClass>())
	{
		check(Blueprint);
		FKismetCompilerUtilities::ConsignToOblivion(InOutTargetClass, Blueprint->bIsRegeneratingOnLoad);
		InOutTargetClass = nullptr;
	}
}

void FBlueprintCompilerContext::CleanAndSanitizeClass(UBlueprintGeneratedClass* InClassToClean, UObject*& InOutOldCDO)
{
	FKismetCompilerContext::CleanAndSanitizeClass(InClassToClean, InOutOldCDO);

	if (USceneStateGeneratedClass* GeneratedClass = Cast<USceneStateGeneratedClass>(InClassToClean))
	{
		GeneratedClass->Reset();
	}
}

void FBlueprintCompilerContext::SaveSubObjectsFromCleanAndSanitizeClass(FSubobjectCollection& OutSubObjectsToSave, UBlueprintGeneratedClass* InClassToClean)
{
	FKismetCompilerContext::SaveSubObjectsFromCleanAndSanitizeClass(OutSubObjectsToSave, InClassToClean);

	// Make sure our typed pointer is set
	check(InClassToClean == NewClass);

	NewGeneratedClass = CastChecked<USceneStateGeneratedClass>(NewClass);
}

void FBlueprintCompilerContext::MergeUbergraphPagesIn(UEdGraph* InUbergraph)
{
	FKismetCompilerContext::MergeUbergraphPagesIn(InUbergraph);

	USceneStateBlueprint* SceneStateBlueprint = CastChecked<USceneStateBlueprint>(Blueprint);

	// Top Level State Machines
	TArray<FSceneStateMachine> StateMachines;
	StateMachines.Reserve(SceneStateBlueprint->StateMachineGraphs.Num());

	TMap<FGuid, uint16> StateMachineIdToIndex;
	StateMachineIdToIndex.Reserve(SceneStateBlueprint->StateMachineGraphs.Num());

	TMap<FObjectKey, uint16> StateMachineGraphToIndex;
	StateMachineGraphToIndex.Reserve(SceneStateBlueprint->StateMachineGraphs.Num());

	const int32 RootStateIndex = NewGeneratedClass->States.AddDefaulted();
	const int32 RootStateMetadataIndex = NewGeneratedClass->StateMetadata.AddDefaulted();

	check(RootStateIndex == RootStateMetadataIndex);

	// Sort the state machines so that the ones that auto run are at the start
	TArray<USceneStateMachineGraph*> StateMachineGraphs = Private::GetStateMachines(SceneStateBlueprint->StateMachineGraphs);
	StateMachineGraphs.StableSort(
		[](const USceneStateMachineGraph& InGraphA, const USceneStateMachineGraph& InGraphB)
		{
			return InGraphA.RunMode < InGraphB.RunMode;
		});

	uint16 AutoRunCount = 0;
	for (USceneStateMachineGraph* StateMachineGraph : StateMachineGraphs)
	{
		FStateMachineCompiler StateMachineCompiler(StateMachineGraph, *this);
		FSceneStateMachine NewStateMachine = StateMachineCompiler.Compile();
		if (!NewStateMachine.IsValid())
		{
			continue;
		}

		const int32 StateMachineIndex = StateMachines.Emplace(MoveTemp(NewStateMachine));

		StateMachineIdToIndex.Emplace(StateMachineGraph->ParametersId, StateMachineIndex);
		StateMachineGraphToIndex.Emplace(StateMachineGraph, StateMachineIndex);

		if (StateMachineGraph->RunMode == ESceneStateMachineRunMode::Auto)
		{
			++AutoRunCount;
		}
	}

	NewGeneratedClass->RootStateIndex = RootStateIndex;

	// Root State only considers the range of the Auto-Run State Machines
	FSceneState& RootState = NewGeneratedClass->States[RootStateIndex];
	RootState.StateMachineRange.Index = NewGeneratedClass->StateMachines.Num();
	RootState.StateMachineRange.Count = AutoRunCount;

	NewGeneratedClass->StateMachines.Append(MoveTemp(StateMachines));

	// Upgrade the map to absolute indices before baking it to the generated class' map
	ToAbsoluteIndexMap(StateMachineIdToIndex, RootState.StateMachineRange.Index);
	NewGeneratedClass->StateMachineIdToIndex.Append(MoveTemp(StateMachineIdToIndex));

	// Upgrade the map to absolute indices before baking it to the generated class' map
	ToAbsoluteIndexMap(StateMachineGraphToIndex, RootState.StateMachineRange.Index);
	NewGeneratedClass->StateMachineGraphToIndex.Append(MoveTemp(StateMachineGraphToIndex));

	UpdateTaskIndices();
	CompileBindings();
}

void FBlueprintCompilerContext::OnPostCDOCompiled(const UObject::FPostCDOCompiledContext& InContext)
{
	FKismetCompilerContext::OnPostCDOCompiled(InContext);
	NewGeneratedClass->ResolveBindings();
}

UBlueprint* FBlueprintCompilerContext::GetBlueprint() const
{
	return Blueprint;
}

USceneStateGeneratedClass* FBlueprintCompilerContext::GetGeneratedClass() const
{
	return NewGeneratedClass;
}

FTransitionGraphCompileResult FBlueprintCompilerContext::CompileTransitionGraph(USceneStateTransitionGraph* InTransitionGraph)
{
	FTransitionGraphCompiler TransitionCompiler(*this, InTransitionGraph);

	const ETransitionGraphCompileReturnCode ReturnCode = TransitionCompiler.Compile();

	FTransitionGraphCompileResult CompileResult;
	CompileResult.ReturnCode = ReturnCode;

	if (ReturnCode == ETransitionGraphCompileReturnCode::Success)
	{
		CompileResult.EventName = TransitionCompiler.GetCustomEventName();
		CompileResult.ResultPropertyName = TransitionCompiler.GetResultPropertyName();
	}

	return CompileResult;
}

void FBlueprintCompilerContext::UpdateTaskIndices()
{
	TConstArrayView<FSceneState> States = NewGeneratedClass->GetStates();

	FInstancedStructContainer& Tasks = NewGeneratedClass->Tasks;

	for (int32 StateIndex = 0; StateIndex < States.Num(); ++StateIndex)
	{
		const FSceneStateRange TaskRange = States[StateIndex].GetTaskRange();
		if (!TaskRange.IsValid() || !Tasks.IsValidIndex(TaskRange.Index) || !Tasks.IsValidIndex(TaskRange.GetLastIndex()))
		{
			continue;
		}

		for (int32 TaskIndex = TaskRange.Index; TaskIndex <= TaskRange.GetLastIndex(); ++TaskIndex)
		{
			FSceneStateTask& Task = Tasks[TaskIndex].Get<FSceneStateTask>();
			Task.TaskIndex = TaskIndex;
			Task.ParentStateIndex = StateIndex;
		}
	}
}

void FBlueprintCompilerContext::CompileBindings()
{
	FBindingCompiler BindingCompiler(*this, CastChecked<USceneStateBlueprint>(Blueprint), NewGeneratedClass);
	BindingCompiler.Compile();
}

} // UE::SceneState::Editor
