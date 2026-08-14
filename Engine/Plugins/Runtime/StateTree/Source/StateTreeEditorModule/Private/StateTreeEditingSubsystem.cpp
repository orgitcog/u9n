// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTreeEditingSubsystem.h"

#include "SStateTreeView.h"
#include "StateTreeCompiler.h"
#include "StateTreeCompilerLog.h"
#include "StateTreeDelegates.h"
#include "StateTreeEditorData.h"
#include "StateTreeEditorModule.h"
#include "StateTreeObjectHash.h"
#include "StateTreeTaskBase.h"
#include "UObject/UObjectGlobals.h"


UStateTreeEditingSubsystem::UStateTreeEditingSubsystem()
{
	PostGarbageCollectHandle = FCoreUObjectDelegates::GetPostGarbageCollect().AddUObject(this, &UStateTreeEditingSubsystem::HandlePostGarbageCollect);
}

void UStateTreeEditingSubsystem::BeginDestroy()
{
	FCoreUObjectDelegates::GetPostGarbageCollect().Remove(PostGarbageCollectHandle);
	Super::BeginDestroy();
}

bool UStateTreeEditingSubsystem::CompileStateTree(TNotNull<UStateTree*> InStateTree, FStateTreeCompilerLog& InOutLog)
{
	ValidateStateTree(InStateTree);
	
	FStateTreeCompiler Compiler(InOutLog);

	const bool bCompilationResult = Compiler.Compile(*InStateTree);
	if(bCompilationResult)
	{
		const uint32 EditorDataHash = CalculateStateTreeHash(InStateTree);

		// Success
		InStateTree->LastCompiledEditorDataHash = EditorDataHash;
		UE::StateTree::Delegates::OnPostCompile.Broadcast(*InStateTree);
		UE_LOG(LogStateTreeEditor, Log, TEXT("Compile StateTree '%s' succeeded."), *InStateTree->GetFullName());
	}
	else
	{
		// Make sure not to leave stale data on failed compile.
		InStateTree->ResetCompiled();
		InStateTree->LastCompiledEditorDataHash = 0;
		
		UE_LOG(LogStateTreeEditor, Error, TEXT("Failed to compile '%s', errors follow."), *InStateTree->GetFullName());
		InOutLog.DumpToLog(LogStateTreeEditor);
	}

	// Notify the UI that something changed. Make sure to not request a new viewmodel. That way, we are not creating new viewmodel when cooking/PIE.
	if (UStateTreeEditingSubsystem* StateTreeEditingSubsystem = GEditor ? GEditor->GetEditorSubsystem<UStateTreeEditingSubsystem>() : nullptr)
	{
		if (TSharedPtr<FStateTreeViewModel> ViewModel = StateTreeEditingSubsystem->StateTreeViewModels.FindRef(FObjectKey(InStateTree)))
		{
			ViewModel->NotifyAssetChangedExternally();
		}
	}

	return bCompilationResult;
}

TSharedRef<FStateTreeViewModel> UStateTreeEditingSubsystem::FindOrAddViewModel(TNotNull<UStateTree*> InStateTree)
{
	const FObjectKey StateTreeKey = InStateTree;
	TSharedPtr<FStateTreeViewModel> ViewModelPtr = StateTreeViewModels.FindRef(StateTreeKey);
	if (ViewModelPtr)
	{
		// The StateTree could be re-instantiated. Can occur when the object is destroyed and recreated in a pool or when reloaded in editor.
		//The object might have the same pointer value or the same path but it's a new object and all weakptr are now invalid.
		if (ViewModelPtr->GetStateTree() == InStateTree)
		{
			return ViewModelPtr.ToSharedRef();
		}
		else
		{
			StateTreeViewModels.Remove(StateTreeKey);
			ViewModelPtr = nullptr;
		}
	}

	ValidateStateTree(InStateTree);

	TSharedRef<FStateTreeViewModel> SharedModel = StateTreeViewModels.Add(StateTreeKey, MakeShared<FStateTreeViewModel>()).ToSharedRef();
	UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(InStateTree->EditorData);
	SharedModel->Init(EditorData);

	return SharedModel;
}

TSharedRef<SWidget> UStateTreeEditingSubsystem::GetStateTreeView(TSharedRef<FStateTreeViewModel> InViewModel, const TSharedRef<FUICommandList>& TreeViewCommandList)
{
	return SNew(SStateTreeView, InViewModel, TreeViewCommandList);
}

void UStateTreeEditingSubsystem::ValidateStateTree(TNotNull<UStateTree*> InStateTree)
{
	auto FixChangedStateLinkName = [](FStateTreeStateLink& StateLink, const TMap<FGuid, FName>& IDToName) -> bool
	{
		if (StateLink.ID.IsValid())
		{
			const FName* Name = IDToName.Find(StateLink.ID);
			if (Name == nullptr)
			{
				// Missing link, we'll show these in the UI
				return false;
			}
			if (StateLink.Name != *Name)
			{
				// Name changed, fix!
				StateLink.Name = *Name;
				return true;
			}
		}
		return false;
	};

	auto ValidateLinkedStates = [FixChangedStateLinkName](const UStateTree& StateTree)
	{
		UStateTreeEditorData* TreeData = Cast<UStateTreeEditorData>(StateTree.EditorData);
		if (!TreeData)
		{
			return;
		}

		// Make sure all state links are valid and update the names if needed.

		// Create ID to state name map.
		TMap<FGuid, FName> IDToName;

		TreeData->VisitHierarchy([&IDToName](const UStateTreeState& State, UStateTreeState* /*ParentState*/)
		{
			IDToName.Add(State.ID, State.Name);
			return EStateTreeVisitor::Continue;
		});
		
		// Fix changed names.
		TreeData->VisitHierarchy([&IDToName, FixChangedStateLinkName](UStateTreeState& State, UStateTreeState* /*ParentState*/)
		{
			constexpr bool bMarkDirty = false;
			State.Modify(bMarkDirty);
			if (State.Type == EStateTreeStateType::Linked)
			{
				FixChangedStateLinkName(State.LinkedSubtree, IDToName);
			}
					
			for (FStateTreeTransition& Transition : State.Transitions)
			{
				FixChangedStateLinkName(Transition.State, IDToName);
			}

			return EStateTreeVisitor::Continue;
		});
	};

	auto FixEditorData = [](TNotNull<UStateTree*> StateTree)
	{
		UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData);
		// The schema is defined in the EditorData. If we can't find the editor data (probably because the class doesn't exist anymore), then try the compiled schema in the state tree asset.
		TSubclassOf<const UStateTreeSchema> SchemaClass;
		if (EditorData && EditorData->Schema)
		{
			SchemaClass = EditorData->Schema->GetClass();
		}
		else if (StateTree->GetSchema())
		{
			SchemaClass = StateTree->GetSchema()->GetClass();
		}

		if (SchemaClass.Get() == nullptr)
		{
			UE_LOG(LogStateTreeEditor, Error, TEXT("The state tree '%s' does not have a schema."), *StateTree->GetPathName());
			return;
		}

		TNonNullSubclassOf<UStateTreeEditorData> EditorDataClass = FStateTreeEditorModule::GetModule().GetEditorDataClass(SchemaClass.Get());
		if (EditorData == nullptr)
		{
			EditorData = NewObject<UStateTreeEditorData>(StateTree, EditorDataClass.Get(), FName(), RF_Transactional);
			EditorData->AddRootState();
			EditorData->Schema = NewObject<UStateTreeSchema>(EditorData, SchemaClass.Get());
			EditorData->EditorBindings.SetBindingsOwner(EditorData);

			constexpr bool bMarkDirty = false;
			StateTree->Modify(bMarkDirty);
			StateTree->EditorData = EditorData;
		}
		else if (!EditorData->IsA(EditorDataClass.Get()))
		{
			// The current EditorData is not of the correct type. The data needs to be patched by the schema desired editor data subclass.
			UStateTreeEditorData* PreviousEditorData = EditorData;
			EditorData = CastChecked<UStateTreeEditorData>(StaticDuplicateObject(EditorData, StateTree, FName(), RF_Transactional, EditorDataClass.Get()));
			if (EditorData->SubTrees.Num() == 0)
			{
				EditorData->AddRootState();
			}
			if (EditorData->Schema == nullptr || !EditorData->Schema->IsA(SchemaClass.Get()))
			{
				EditorData->Schema = NewObject<UStateTreeSchema>(EditorData, SchemaClass.Get());
			}
			EditorData->EditorBindings.SetBindingsOwner(EditorData);

			constexpr bool bMarkDirty = false;
			StateTree->Modify(bMarkDirty);
			StateTree->EditorData = EditorData;

			// Trash the previous EditorData
			FName TrashName = MakeUniqueObjectName(GetTransientPackage(), UStateTree::StaticClass(), *FString::Printf(TEXT("TRASH_%s"), *UStateTree::StaticClass()->GetName()));
			UStateTree* TransientOuter = NewObject<UStateTree>(GetTransientPackage(), TrashName, RF_Transient);
			ERenameFlags RenameFlags = REN_DoNotDirty | REN_DontCreateRedirectors;
			PreviousEditorData->Rename(*TrashName.ToString(), TransientOuter, RenameFlags);
			PreviousEditorData->SetFlags(RF_Transient);
		}
	};

	auto UpdateParents = [](const UStateTree& StateTree)
	{
		UStateTreeEditorData* TreeData = Cast<UStateTreeEditorData>(StateTree.EditorData);
		if (!TreeData)
		{
			return;
		}

		TreeData->ReparentStates();
	};

	auto ApplySchema =[](const UStateTree& StateTree)
	{
		UStateTreeEditorData* TreeData = Cast<UStateTreeEditorData>(StateTree.EditorData);
		if (!TreeData)
		{
			return;
		
		}
		const UStateTreeSchema* Schema = TreeData->Schema;
		if (!Schema)
		{
			return;
		}
		
		// Clear evaluators if not allowed.
		if (Schema->AllowEvaluators() == false && TreeData->Evaluators.Num() > 0)
		{
			UE_LOG(LogStateTreeEditor, Warning, TEXT("%s: Resetting Evaluators due to current schema restrictions."), *GetNameSafe(&StateTree));
			TreeData->Evaluators.Reset();
		}

		TreeData->VisitHierarchy([&StateTree, Schema](UStateTreeState& State, UStateTreeState* /*ParentState*/)
		{
			constexpr bool bMarkDirty = false;
			State.Modify(bMarkDirty);

			// Clear enter conditions if not allowed.
			if (Schema->AllowEnterConditions() == false && State.EnterConditions.Num() > 0)
			{
				UE_LOG(LogStateTreeEditor, Warning, TEXT("%s: Resetting Enter Conditions in state %s due to current schema restrictions."), *GetNameSafe(&StateTree), *GetNameSafe(&State));
				State.EnterConditions.Reset();
			}

			// Clear Utility if not allowed
			if (Schema->AllowUtilityConsiderations() == false && State.Considerations.Num() > 0)
			{
				UE_LOG(LogStateTreeEditor, Warning, TEXT("%s: Resetting Utility Considerations in state %s due to current schema restrictions."), *GetNameSafe(&StateTree), *GetNameSafe(&State));
				State.Considerations.Reset();
			}

			// Keep single and many tasks based on what is allowed.
			if (Schema->AllowMultipleTasks() == false)
			{
				if (State.Tasks.Num() > 0)
				{
					State.Tasks.Reset();
					UE_LOG(LogStateTreeEditor, Warning, TEXT("%s: Resetting Tasks in state %s due to current schema restrictions."), *GetNameSafe(&StateTree), *GetNameSafe(&State));
				}
				
				// Task name is the same as state name.
				if (FStateTreeTaskBase* Task = State.SingleTask.Node.GetMutablePtr<FStateTreeTaskBase>())
				{
					Task->Name = State.Name;
				}
			}
			else
			{
				if (State.SingleTask.Node.IsValid())
				{
					State.SingleTask.Reset();
					UE_LOG(LogStateTreeEditor, Warning, TEXT("%s: Resetting Single Task in state %s due to current schema restrictions."), *GetNameSafe(&StateTree), *GetNameSafe(&State));
				}
			}
			
			return EStateTreeVisitor::Continue;
		});
	};

	auto RemoveUnusedBindings = [](const UStateTree& StateTree)
	{
		UStateTreeEditorData* TreeData = Cast<UStateTreeEditorData>(StateTree.EditorData);
		if (!TreeData)
		{
			return;
		}

		TMap<FGuid, const FPropertyBindingDataView> AllStructValues;
		TreeData->GetAllStructValues(AllStructValues);
		TreeData->GetPropertyEditorBindings()->RemoveInvalidBindings(AllStructValues);
	};

	auto UpdateLinkedStateParameters = [](const UStateTree& StateTree)
	{
		UStateTreeEditorData* TreeData = Cast<UStateTreeEditorData>(StateTree.EditorData);
		if (!TreeData)
		{
			return;
		}

		const EStateTreeVisitor Result = TreeData->VisitHierarchy([](UStateTreeState& State, UStateTreeState* /*ParentState*/)
		{
			if (State.Type == EStateTreeStateType::Linked
				|| State.Type == EStateTreeStateType::LinkedAsset)
			{
				constexpr bool bMarkDirty = false;
				State.Modify(bMarkDirty);
				State.UpdateParametersFromLinkedSubtree();
			}
			return EStateTreeVisitor::Continue;
		});
	};

	auto UpdateTransactionalFlags = [](TNotNull<UStateTree*> StateTree)
	{
		UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData);
		if (EditorData)
		{
			for (UStateTreeState* SubTree : EditorData->SubTrees)
			{
				TArray<UStateTreeState*> Stack;

				Stack.Add(SubTree);
				while (!Stack.IsEmpty())
				{
					if (UStateTreeState* State = Stack.Pop())
					{
						State->SetFlags(RF_Transactional);

						for (UStateTreeState* ChildState : State->Children)
						{
							Stack.Add(ChildState);
						}
					}
				}
			}
		}
	};

	FixEditorData(InStateTree);

	if (InStateTree->EditorData)
		{
			constexpr bool bMarkDirty = false;
		InStateTree->EditorData->Modify(bMarkDirty);
	}

	UpdateParents(*InStateTree);
	ApplySchema(*InStateTree);
	RemoveUnusedBindings(*InStateTree);
	ValidateLinkedStates(*InStateTree);
	UpdateLinkedStateParameters(*InStateTree);
	UpdateTransactionalFlags(InStateTree);
}

uint32 UStateTreeEditingSubsystem::CalculateStateTreeHash(TNotNull<const UStateTree*> InStateTree)
{
	uint32 EditorDataHash = 0;
	if (InStateTree->EditorData != nullptr)
	{
		FStateTreeObjectCRC32 Archive;
		EditorDataHash = Archive.Crc32(InStateTree->EditorData, 0);
	}

	return EditorDataHash;
}

void UStateTreeEditingSubsystem::HandlePostGarbageCollect()
{
	// Remove the stale viewmodels
	for (TMap<FObjectKey, TSharedPtr<FStateTreeViewModel>>::TIterator It(StateTreeViewModels); It; ++It)
	{
		if (!It.Key().ResolveObjectPtr())
		{
			It.RemoveCurrent();
		}
		else if (!It.Value() || !It.Value()->GetStateTree())
		{
			It.RemoveCurrent();
		}
	}
}
