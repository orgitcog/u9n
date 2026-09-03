// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTreeViewModel.h"

#include "Customizations/StateTreeEditorNodeUtils.h"
#include "StateTree.h"
#include "StateTreeEditorData.h"
#include "StateTreeDelegates.h"
#include "Debugger/StateTreeDebugger.h"
#include "Editor.h"
#include "ScopedTransaction.h"
#include "HAL/PlatformApplicationMisc.h"
#include "UnrealExporter.h"
#include "Exporters/Exporter.h"
#include "Factories.h"
#include "StateTreeTaskBase.h"
#include "StateTreeConditionBase.h"
#include "StateTreeConsiderationBase.h"
#include "StateTreePropertyHelpers.h"
#include "StateTreeScopedEditorDataFixer.h"
#include "Misc/NotNull.h"

#define LOCTEXT_NAMESPACE "StateTreeEditor"

namespace UE::StateTree::Editor
{
	class FStateTreeStateTextFactory : public FCustomizableTextObjectFactory
	{
	public:
		FStateTreeStateTextFactory()
			: FCustomizableTextObjectFactory(GWarn)
		{}

		virtual bool CanCreateClass(UClass* InObjectClass, bool& bOmitSubObjs) const override
		{
			return InObjectClass->IsChildOf(UStateTreeState::StaticClass())
				|| InObjectClass->IsChildOf(UStateTreeClipboardBindings::StaticClass());
		}

		virtual void ProcessConstructedObject(UObject* NewObject) override
		{
			if (UStateTreeState* State = Cast<UStateTreeState>(NewObject))
			{
				States.Add(State);
			}
			else if (UStateTreeClipboardBindings* Bindings = Cast<UStateTreeClipboardBindings>(NewObject))
			{
				ClipboardBindings = Bindings;
			}
		}

	public:
		TArray<UStateTreeState*> States;
		UStateTreeClipboardBindings* ClipboardBindings = nullptr;
	};


	void CollectBindingsCopiesRecursive(UStateTreeEditorData* TreeData, UStateTreeState* State, TArray<FStateTreePropertyPathBinding>& AllBindings)
	{
		if (!State)
		{
			return;
		}
		
		TreeData->VisitStateNodes(*State, [TreeData, &AllBindings](const UStateTreeState* State, const FStateTreeBindableStructDesc& Desc, const FStateTreeDataView Value)
		{
			TArray<const FPropertyBindingBinding*> NodeBindings;
			TreeData->GetPropertyEditorBindings()->FPropertyBindingBindingCollection::GetBindingsFor(Desc.ID, NodeBindings);
			Algo::Transform(NodeBindings, AllBindings, [](const FPropertyBindingBinding* BindingPtr) { return *static_cast<const FStateTreePropertyPathBinding*>(BindingPtr); });
			return EStateTreeVisitor::Continue;
		});

		for (UStateTreeState* ChildState : State->Children)
		{
			CollectBindingsCopiesRecursive(TreeData, ChildState, AllBindings);
		}
	}

	FString ExportStatesToText(UStateTreeEditorData* TreeData, const TArrayView<UStateTreeState*> States)
	{
		if (States.IsEmpty())
		{
			return FString();
		}

		// Clear the mark state for saving.
		UnMarkAllObjects(EObjectMark(OBJECTMARK_TagExp | OBJECTMARK_TagImp));

		FStringOutputDevice Archive;
		const FExportObjectInnerContext Context;

		UStateTreeClipboardBindings* ClipboardBindings = NewObject<UStateTreeClipboardBindings>();
		check(ClipboardBindings);

		for (UStateTreeState* State : States)
		{
			UObject* ThisOuter = State->GetOuter();
			UExporter::ExportToOutputDevice(&Context, State, nullptr, Archive, TEXT("copy"), 0, PPF_ExportsNotFullyQualified | PPF_Copy | PPF_Delimited, false, ThisOuter);

			CollectBindingsCopiesRecursive(TreeData, State, ClipboardBindings->Bindings);
		}

		UExporter::ExportToOutputDevice(&Context, ClipboardBindings, nullptr, Archive, TEXT("copy"), 0, PPF_ExportsNotFullyQualified | PPF_Copy | PPF_Delimited, false);

		return *Archive;
	}

	void CollectStateLinks(const UStruct* Struct, void* Memory, TArray<FStateTreeStateLink*>& Links)
	{
		for (TPropertyValueIterator<FStructProperty> It(Struct, Memory); It; ++It)
		{
			if (It->Key->Struct == TBaseStructure<FStateTreeStateLink>::Get())
			{
				FStateTreeStateLink* StateLink = static_cast<FStateTreeStateLink*>(const_cast<void*>(It->Value));
				Links.Add(StateLink);
			}
		}
	}

	// todo: Should refactor it into FStateTreeScopedEditorDataFixer
	void FixNodesAfterDuplication(TArrayView<FStateTreeEditorNode> Nodes, TMap<FGuid, FGuid>& IDsMap, TArray<FStateTreeStateLink*>& Links)
	{
		for (FStateTreeEditorNode& Node : Nodes)
		{
			const FGuid NewNodeID = FGuid::NewGuid();
			IDsMap.Emplace(Node.ID, NewNodeID);
			Node.ID = NewNodeID;

			if (Node.Node.IsValid())
			{
				CollectStateLinks(Node.Node.GetScriptStruct(), Node.Node.GetMutableMemory(), Links);
			}
			if (Node.Instance.IsValid())
			{
				CollectStateLinks(Node.Instance.GetScriptStruct(), Node.Instance.GetMutableMemory(), Links);
			}
			if (Node.InstanceObject)
			{
				CollectStateLinks(Node.InstanceObject->GetClass(), Node.InstanceObject, Links);
			}
		}
	}

	// todo: Should refactor it into FStateTreeScopedEditorDataFixer
	void FixStateAfterDuplication(UStateTreeState* State, UStateTreeState* NewParentState, TMap<FGuid, FGuid>& IDsMap, TArray<FStateTreeStateLink*>& Links, TArray<UStateTreeState*>& NewStates)
	{
		State->Modify();

		const FGuid NewStateID = FGuid::NewGuid();
		IDsMap.Emplace(State->ID, NewStateID);
		State->ID = NewStateID;
		
		const FGuid NewParametersID = FGuid::NewGuid();
		IDsMap.Emplace(State->Parameters.ID, NewParametersID);
		State->Parameters.ID = NewParametersID;
		
		State->Parent = NewParentState;
		NewStates.Add(State);
		
		if (State->Type == EStateTreeStateType::Linked)
		{
			Links.Emplace(&State->LinkedSubtree);
		}

		FixNodesAfterDuplication(TArrayView<FStateTreeEditorNode>(&State->SingleTask, 1), IDsMap, Links);
		FixNodesAfterDuplication(State->Tasks, IDsMap, Links);
		FixNodesAfterDuplication(State->EnterConditions, IDsMap, Links);
		FixNodesAfterDuplication(State->Considerations, IDsMap, Links);

		for (FStateTreeTransition& Transition : State->Transitions)
		{
			// Transition Ids are not used by nodes so no need to add to 'IDsMap'
			Transition.ID = FGuid::NewGuid();

			FixNodesAfterDuplication(Transition.Conditions, IDsMap, Links);
			Links.Emplace(&Transition.State);
		}

		for (UStateTreeState* Child : State->Children)
		{
			FixStateAfterDuplication(Child, State, IDsMap, Links, NewStates);
		}
	}

	// Removes states from the array which are children of any other state.
	void RemoveContainedChildren(TArray<UStateTreeState*>& States)
	{
		TSet<UStateTreeState*> UniqueStates;
		for (UStateTreeState* State : States)
		{
			UniqueStates.Add(State);
		}

		for (int32 i = 0; i < States.Num(); )
		{
			UStateTreeState* State = States[i];

			// Walk up the parent state sand if the current state
			// exists in any of them, remove it.
			UStateTreeState* StateParent = State->Parent;
			bool bShouldRemove = false;
			while (StateParent)
			{
				if (UniqueStates.Contains(StateParent))
				{
					bShouldRemove = true;
					break;
				}
				StateParent = StateParent->Parent;
			}

			if (bShouldRemove)
			{
				States.RemoveAt(i);
			}
			else
			{
				i++;
			}
		}
	}

	// Returns true if the state is child of parent state.
	bool IsChildOf(const UStateTreeState* ParentState, const UStateTreeState* State)
	{
		for (const UStateTreeState* Child : ParentState->Children)
		{
			if (Child == State)
			{
				return true;
			}
			if (IsChildOf(Child, State))
			{
				return true;
			}
		}
		return false;
	}

namespace Private
{
	/* Short-lived helper struct for node manipulation in the editor */
	struct FStateTreeStateNodeEditorHandle
	{
		const TNotNull<UStateTreeEditorData*> EditorData;
		const TNotNull<UStateTreeState*> OwnerState;

	private:
		FStringView NodePath;
		int32 ArrayIndex = INDEX_NONE;
		void* TargetArray = nullptr;
		void* TargetNode = nullptr;
		bool bIsTransition = false;

	public:
		FStateTreeStateNodeEditorHandle(const TNotNull<UStateTreeEditorData*> InEditorData, const TNotNull<UStateTreeState*> InOwnerState, const FGuid& NodeID)
			: EditorData(InEditorData),
			OwnerState(InOwnerState)
		{
			auto FindNode = [Self = this, &NodeID]<typename T>(T& Nodes, FStringView Path)
			{
				for (int32 Index = 0; Index < Nodes.Num(); ++Index)
				{
					if (NodeID == Nodes[Index].ID)
					{
						Self->ArrayIndex = Index;
						Self->TargetArray = &Nodes;
						Self->TargetNode = &Nodes[Self->ArrayIndex];
						Self->NodePath = Path;

						return true;
					}
				}

				return false;
			};

			if (!NodeID.IsValid())
			{
				return;
			}

			if (FindNode(OwnerState->EnterConditions, TEXT("EnterConditions")))
			{
				return;
			}

			if (FindNode(OwnerState->Tasks, TEXT("Tasks")))
			{
				return;
			}

			if (NodeID == OwnerState->SingleTask.ID)
			{
				NodePath = TEXT("SingleTask");
				TargetNode = &OwnerState->SingleTask;
				return;
			}

			if (FindNode(OwnerState->Considerations, TEXT("Considerations")))
			{
				return;
			}

			if (FindNode(OwnerState->Transitions, TEXT("Transitions")))
			{
				bIsTransition = true;
				return;
			}
		}

		bool IsValid() const
		{
			return TargetNode != nullptr;
		}

		bool IsTransition() const
		{
			return bIsTransition;
		}

		FStateTreeEditorNode& GetEditorNode() const
		{
			check(IsValid() && !IsTransition());

			return *static_cast<FStateTreeEditorNode*>(TargetNode);
		}

		TArray<FStateTreeEditorNode>& GetEditorNodeArray() const
		{
			check(IsValid() && !IsTransition() && GetNodeIndex() != INDEX_NONE);

			return *static_cast<TArray<FStateTreeEditorNode>*>(TargetArray);
		}

		FStateTreeTransition& GetTransition() const
		{
			check(IsValid() && IsTransition());

			return *static_cast<FStateTreeTransition*>(TargetNode);
		}

		TArray<FStateTreeTransition>& GetTransitionArray() const
		{
			check(IsValid() && IsTransition() && GetNodeIndex() != INDEX_NONE);

			return *static_cast<TArray<FStateTreeTransition>*>(TargetArray);
		}

		FStringView GetNodePath() const
		{
			return NodePath;
		}

		int32 GetNodeIndex() const
		{
			return ArrayIndex;
		}
	};
}

};

FStateTreeViewModel::FStateTreeViewModel()
	: TreeDataWeak(nullptr)
#if WITH_STATETREE_TRACE_DEBUGGER
	, Debugger(MakeShareable(new FStateTreeDebugger))
#endif // WITH_STATETREE_TRACE_DEBUGGER
{
}

FStateTreeViewModel::~FStateTreeViewModel()
{
	if (GEditor)
	{
	    GEditor->UnregisterForUndo(this);
	}

	UE::StateTree::Delegates::OnIdentifierChanged.RemoveAll(this);
}

void FStateTreeViewModel::Init(UStateTreeEditorData* InTreeData)
{
	TreeDataWeak = InTreeData;

	GEditor->RegisterForUndo(this);

	UE::StateTree::Delegates::OnIdentifierChanged.AddSP(this, &FStateTreeViewModel::HandleIdentifierChanged);
	
#if WITH_STATETREE_TRACE_DEBUGGER
	UE::StateTree::Delegates::OnBreakpointsChanged.AddSP(this, &FStateTreeViewModel::HandleBreakpointsChanged);
	UE::StateTree::Delegates::OnPostCompile.AddSP(this, &FStateTreeViewModel::HandlePostCompile);

	Debugger->SetAsset(GetStateTree());
	BindToDebuggerDelegates();
	RefreshDebuggerBreakpoints();
#endif // WITH_STATETREE_TRACE_DEBUGGER
}

const UStateTree* FStateTreeViewModel::GetStateTree() const
{
	if (const UStateTreeEditorData* TreeData = TreeDataWeak.Get())
	{
		return TreeData->GetTypedOuter<UStateTree>();
	}

	return nullptr;
}

const UStateTreeEditorData* FStateTreeViewModel::GetStateTreeEditorData() const
{
	return TreeDataWeak.Get();
}

const UStateTreeState* FStateTreeViewModel::GetStateByID(const FGuid StateID) const
{
	if (const UStateTreeEditorData* TreeData = TreeDataWeak.Get())
	{
		return const_cast<UStateTreeState*>(TreeData->GetStateByID(StateID));
	}
	return nullptr;
}

UStateTreeState* FStateTreeViewModel::GetMutableStateByID(const FGuid StateID) const
{
	if (UStateTreeEditorData* TreeData = TreeDataWeak.Get())
	{
		return TreeData->GetMutableStateByID(StateID);
	}
	return nullptr;
}

void FStateTreeViewModel::HandleIdentifierChanged(const UStateTree& StateTree) const
{
	if (GetStateTree() == &StateTree)
	{
		OnAssetChanged.Broadcast();
	}
}

#if WITH_STATETREE_TRACE_DEBUGGER
bool FStateTreeViewModel::CanAddStateBreakpoint(const EStateTreeBreakpointType Type) const
{
	const UStateTreeEditorData* EditorData = TreeDataWeak.Get();
	if (!ensure(EditorData != nullptr))
	{
		return false;
	}

	for (const TWeakObjectPtr<UStateTreeState>& WeakState : SelectedStates)
	{
		if (const UStateTreeState* State = WeakState.Get())
		{
			if (EditorData->HasBreakpoint(State->ID, Type) == false)
			{
				return true;
			}
		}
	}

	return false;
}

bool FStateTreeViewModel::CanRemoveStateBreakpoint(const EStateTreeBreakpointType Type) const
{
	const UStateTreeEditorData* EditorData = TreeDataWeak.Get();
	if (!ensure(EditorData != nullptr))
	{
		return false;
	}

	for (const TWeakObjectPtr<UStateTreeState>& WeakState : SelectedStates)
	{
		if (const UStateTreeState* State = WeakState.Get())
		{
			if (EditorData->HasBreakpoint(State->ID, Type))
			{
				return true;
			}
		}
	}

	return false;
}


ECheckBoxState FStateTreeViewModel::GetStateBreakpointCheckState(const EStateTreeBreakpointType Type) const
{
	const bool bCanAdd = CanAddStateBreakpoint(Type);
	const bool bCanRemove = CanRemoveStateBreakpoint(Type);
	if (bCanAdd && bCanRemove)
	{
		return ECheckBoxState::Undetermined;
	}

	if (bCanRemove)
	{
		return ECheckBoxState::Checked;
	}

	if (bCanAdd)
	{
		return ECheckBoxState::Unchecked;
	}

	// Should not happen since action is not visible in this case
	return ECheckBoxState::Undetermined;
}

void FStateTreeViewModel::HandleEnableStateBreakpoint(EStateTreeBreakpointType Type)
{
	TArray<UStateTreeState*> ValidatedSelectedStates;
	GetSelectedStates(ValidatedSelectedStates);
	if (ValidatedSelectedStates.IsEmpty())
	{
		return;
	}

	UStateTreeEditorData* EditorData = TreeDataWeak.Get();
	if (!ensure(EditorData != nullptr))
	{
		return;
	}

	TBitArray<> HasBreakpoint;
	HasBreakpoint.Reserve(ValidatedSelectedStates.Num());
	for (const UStateTreeState* SelectedState : ValidatedSelectedStates)
	{
		HasBreakpoint.Add(SelectedState != nullptr && EditorData->HasBreakpoint(SelectedState->ID, Type));
	}

	check(HasBreakpoint.Num() == ValidatedSelectedStates.Num());

	// Process CanAdd first so in case of undetermined state (mixed selection) we add by default. 
	if (CanAddStateBreakpoint(Type))
	{
		const FScopedTransaction Transaction(LOCTEXT("AddStateBreakpoint", "Add State Breakpoint(s)"));
		EditorData->Modify();
		for (int Index = 0; Index < ValidatedSelectedStates.Num(); ++Index)
		{
			const UStateTreeState* SelectedState = ValidatedSelectedStates[Index];
			if (HasBreakpoint[Index] == false && SelectedState != nullptr)
			{
				EditorData->AddBreakpoint(SelectedState->ID, Type);	
			}
		}
	}
	else if (CanRemoveStateBreakpoint(Type))
	{
		const FScopedTransaction Transaction(LOCTEXT("RemoveStateBreakpoint", "Remove State Breakpoint(s)"));
		EditorData->Modify();
		for (int Index = 0; Index < ValidatedSelectedStates.Num(); ++Index)
		{
			const UStateTreeState* SelectedState = ValidatedSelectedStates[Index];
			if (HasBreakpoint[Index] && SelectedState != nullptr)
			{
				EditorData->RemoveBreakpoint(SelectedState->ID, Type);	
			}
		}
	}
}

UStateTreeState* FStateTreeViewModel::FindStateAssociatedToBreakpoint(FStateTreeDebuggerBreakpoint Breakpoint) const
{
	UStateTreeEditorData* EditorData = TreeDataWeak.Get();
	if (EditorData == nullptr)
	{
		return nullptr;
	}
	const UStateTree* StateTree = GetStateTree();
	if (StateTree == nullptr)
	{
		return nullptr;
	}

	UStateTreeState* StateTreeState = nullptr;

	if (const FStateTreeStateHandle* StateHandle = Breakpoint.ElementIdentifier.TryGet<FStateTreeStateHandle>())
	{
		const FGuid StateId = StateTree->GetStateIdFromHandle(*StateHandle);
		StateTreeState = EditorData->GetMutableStateByID(StateId);
	}
	else if (const FStateTreeDebuggerBreakpoint::FStateTreeTaskIndex* TaskIndex = Breakpoint.ElementIdentifier.TryGet<FStateTreeDebuggerBreakpoint::FStateTreeTaskIndex>())
	{
		const FGuid TaskId = StateTree->GetNodeIdFromIndex(TaskIndex->Index);

		EditorData->VisitHierarchy([&TaskId, &StateTreeState](UStateTreeState& State, UStateTreeState* /*ParentState*/)
			{
				for (const FStateTreeEditorNode& EditorNode : State.Tasks)
				{
					if (EditorNode.ID == TaskId)
					{
						StateTreeState = &State;
						return EStateTreeVisitor::Break;
					}
				}
				return EStateTreeVisitor::Continue;
			});
	}
	else if (const FStateTreeDebuggerBreakpoint::FStateTreeTransitionIndex* TransitionIndex = Breakpoint.ElementIdentifier.TryGet<FStateTreeDebuggerBreakpoint::FStateTreeTransitionIndex>())
	{
		const FGuid TransitionId = StateTree->GetTransitionIdFromIndex(TransitionIndex->Index);

		EditorData->VisitHierarchy([&TransitionId, &StateTreeState](UStateTreeState& State, UStateTreeState* /*ParentState*/)
			{
				for (const FStateTreeTransition& StateTransition : State.Transitions)
				{
					if (StateTransition.ID == TransitionId)
					{
						StateTreeState = &State;
						return EStateTreeVisitor::Break;
					}
				}
				return EStateTreeVisitor::Continue;
			});
	}

	return StateTreeState;
}

void FStateTreeViewModel::HandleBreakpointsChanged(const UStateTree& StateTree)
{
	if (GetStateTree() == &StateTree)
	{
		RefreshDebuggerBreakpoints();
	}
}

void FStateTreeViewModel::HandlePostCompile(const UStateTree& StateTree)
{
	if (GetStateTree() == &StateTree)
	{
		RefreshDebuggerBreakpoints();
	}
}

void FStateTreeViewModel::RefreshDebuggerBreakpoints()
{
	const UStateTree* StateTree = GetStateTree();
	const UStateTreeEditorData* TreeData = TreeDataWeak.Get();
	if (StateTree != nullptr && TreeData != nullptr)
	{
		Debugger->ClearAllBreakpoints();

		for (const FStateTreeEditorBreakpoint& Breakpoint : TreeData->Breakpoints)
		{
			// Test if the ID is associated to a task
			const FStateTreeIndex16 Index = StateTree->GetNodeIndexFromId(Breakpoint.ID);
			if (Index.IsValid())
			{
				Debugger->SetTaskBreakpoint(Index, Breakpoint.BreakpointType);
			}
			else
			{
				// Then test if the ID is associated to a State
				FStateTreeStateHandle StateHandle = StateTree->GetStateHandleFromId(Breakpoint.ID);
				if (StateHandle.IsValid())
				{
					Debugger->SetStateBreakpoint(StateHandle, Breakpoint.BreakpointType);
				}
				else
				{
					// Then test if the ID is associated to a transition
					const FStateTreeIndex16 TransitionIndex = StateTree->GetTransitionIndexFromId(Breakpoint.ID);
					if (TransitionIndex.IsValid())
					{
						Debugger->SetTransitionBreakpoint(TransitionIndex, Breakpoint.BreakpointType);
					}
				}
			}
		}
	}
}

#endif // WITH_STATETREE_TRACE_DEBUGGER

void FStateTreeViewModel::NotifyAssetChangedExternally() const
{
	OnAssetChanged.Broadcast();
}

void FStateTreeViewModel::NotifyStatesChangedExternally(const TSet<UStateTreeState*>& ChangedStates, const FPropertyChangedEvent& PropertyChangedEvent) const
{
	OnStatesChanged.Broadcast(ChangedStates, PropertyChangedEvent);
}

TArray<TObjectPtr<UStateTreeState>>* FStateTreeViewModel::GetSubTrees() const
{
	UStateTreeEditorData* TreeData = TreeDataWeak.Get();
	return TreeData != nullptr ? &TreeData->SubTrees : nullptr;
}

int32 FStateTreeViewModel::GetSubTreeCount() const
{
	UStateTreeEditorData* TreeData = TreeDataWeak.Get();
	return TreeData != nullptr ? TreeData->SubTrees.Num() : 0;
}

void FStateTreeViewModel::GetSubTrees(TArray<TWeakObjectPtr<UStateTreeState>>& OutSubtrees) const
{
	OutSubtrees.Reset();
	if (UStateTreeEditorData* TreeData = TreeDataWeak.Get())
	{
		for (UStateTreeState* Subtree : TreeData->SubTrees)
		{
			OutSubtrees.Add(Subtree);
		}
	}
}

void FStateTreeViewModel::GetLinkStates(FGuid StateID, TArray<FGuid>& LinkingIn, TArray<FGuid>& LinkedOut) const
{
	const UStateTreeState* State = GetStateByID(StateID);
	if (State == nullptr)
	{
		return;
	}

	if (UStateTreeEditorData* TreeData = TreeDataWeak.Get())
	{
		TreeData->VisitHierarchy([&LinkingIn, &LinkedOut, StateID = State->ID](UStateTreeState& State, UStateTreeState* ParentState)
			{
				if (State.ID == StateID)
				{
					return EStateTreeVisitor::Continue;
				}
				if (State.Type == EStateTreeStateType::Linked && StateID == State.LinkedSubtree.ID)
				{
					LinkingIn.AddUnique(State.ID);
				}
				else
				{
					for (const FStateTreeTransition& Transition : State.Transitions)
					{
						if (Transition.State.ID == StateID)
						{
							LinkingIn.AddUnique(State.ID);
						}
					}
				}
				return EStateTreeVisitor::Continue;
			});

		if (State->Type == EStateTreeStateType::Linked)
		{
			LinkedOut.AddUnique(State->LinkedSubtree.ID);
		}

		for (const FStateTreeTransition& Transition : State->Transitions)
		{
			LinkedOut.AddUnique(Transition.State.ID);
		}
	}
}

void FStateTreeViewModel::PostUndo(bool bSuccess)
{
	// TODO: see if we can narrow this down.
	OnAssetChanged.Broadcast();
}

void FStateTreeViewModel::PostRedo(bool bSuccess)
{
	OnAssetChanged.Broadcast();
}

void FStateTreeViewModel::ClearSelection()
{
	if (SelectedStates.IsEmpty())
	{
		return;
	}

	SelectedStates.Reset();

	const TArray<TWeakObjectPtr<UStateTreeState>> SelectedStatesArr;
	OnSelectionChanged.Broadcast(SelectedStatesArr);
}

void FStateTreeViewModel::SetSelection(UStateTreeState* Selected)
{
	if (SelectedStates.Num() == 1 && SelectedStates.Contains(Selected))
	{
		return;
	}

	SelectedStates.Reset();
	if (Selected != nullptr)
	{
		SelectedStates.Add(Selected);
	}

	TArray<TWeakObjectPtr<UStateTreeState>> SelectedStatesArr;
	SelectedStatesArr.Add(Selected);
	OnSelectionChanged.Broadcast(SelectedStatesArr);
}

void FStateTreeViewModel::DeleteNode(TWeakObjectPtr<UStateTreeState> State, const FGuid& ID)
{
	if (UStateTreeEditorData* EditorData = TreeDataWeak.Get())
	{
		if (UStateTreeState* OwnerState = State.Get())
		{
			const UE::StateTree::Editor::Private::FStateTreeStateNodeEditorHandle StateNodeHandle(EditorData, OwnerState, ID);
			// If the op can not be executed, don't even start the transaction
			if (!StateNodeHandle.IsValid())
			{
				return;
			}

			auto DeleteFunc = [&StateNodeHandle](TNotNull<UStateTreeState*> InOwnerState, TNotNull<UStateTreeEditorData*> InEditorData, const FStateTreeEditPropertyPath& InPropertyPath)
				{
					StateNodeHandle.EditorData->Modify();

					UE::StateTreeEditor::FScopedEditorDataFixer DataFixer(InEditorData);
					DataFixer.bRemoveInvalidBindings = true;

					if (StateNodeHandle.IsTransition())
					{
						StateNodeHandle.GetTransitionArray().RemoveAt(StateNodeHandle.GetNodeIndex());
					}
					else
					{
						StateNodeHandle.GetEditorNodeArray().RemoveAt(StateNodeHandle.GetNodeIndex());
					}
				};

			UE::StateTree::PropertyHelpers::ModifyStateInPreAndPostEdit(
				LOCTEXT("DeleteNodeTransaction", "Delete Node"),
				StateNodeHandle.OwnerState,
				StateNodeHandle.EditorData,
				StateNodeHandle.GetNodePath(),
				DeleteFunc,
				StateNodeHandle.GetNodeIndex(),
				EPropertyChangeType::ArrayRemove);


			OnStateNodesChanged.Broadcast(OwnerState);
		}
	}
}

void FStateTreeViewModel::DeleteAllNodes(TWeakObjectPtr<UStateTreeState> State, const FGuid& ID)
{
	if (UStateTreeEditorData* EditorData = TreeDataWeak.Get())
	{
		if (UStateTreeState* OwnerState = State.Get())
		{
			UE::StateTree::Editor::Private::FStateTreeStateNodeEditorHandle StateNodeHandle(EditorData, OwnerState, ID);
			// If the op can not be executed, don't even start the transaction
			if (!StateNodeHandle.IsValid())
			{
				return;
			}

			auto DeleteAllFunc = [&StateNodeHandle](const TNotNull<UStateTreeState*> InOwnerState, const TNotNull<UStateTreeEditorData*> InEditorData, const FStateTreeEditPropertyPath& InPropertyPath)
				{
					StateNodeHandle.EditorData->Modify();

					UE::StateTreeEditor::FScopedEditorDataFixer DataFixer(InEditorData);
					DataFixer.bRemoveInvalidBindings = true;

					if (StateNodeHandle.IsTransition())
					{
						StateNodeHandle.GetTransitionArray().Empty();
					}
					else
					{
						StateNodeHandle.GetEditorNodeArray().Empty();
					}
				};

			UE::StateTree::PropertyHelpers::ModifyStateInPreAndPostEdit(
				LOCTEXT("DeleteAllNodesTransaction", "Delete All Nodes"),
				StateNodeHandle.OwnerState,
				StateNodeHandle.EditorData,
				StateNodeHandle.GetNodePath(),
				DeleteAllFunc,
				INDEX_NONE, // Pass Invalid Index to Array Clear Op
				EPropertyChangeType::ArrayClear);


			OnStateNodesChanged.Broadcast(OwnerState);
		}
	}
}

void FStateTreeViewModel::CopyNode(TWeakObjectPtr<UStateTreeState> State, const FGuid& ID)
{
	if (UStateTreeEditorData* EditorData = TreeDataWeak.Get())
	{
		if (UStateTreeState* OwnerState = State.Get())
		{
			UE::StateTree::Editor::Private::FStateTreeStateNodeEditorHandle StateNodeHandle(EditorData, OwnerState, ID);
			// If the op can not be executed, don't even start the transaction
			if (!StateNodeHandle.IsValid())
			{
				return;
			}

			const UScriptStruct* BaseStruct = nullptr;
			if (StateNodeHandle.IsTransition())
			{
				BaseStruct = FStateTreeTransition::StaticStruct();
			}
			else
			{
				BaseStruct = FStateTreeEditorNode::StaticStruct();
			}

			// Use PPF_Copy so that all properties get copied.
			FString Value;
			constexpr EPropertyPortFlags PortFlags = PPF_Copy;
			BaseStruct->ExportText(Value, StateNodeHandle.IsTransition() ? (void*)&StateNodeHandle.GetTransition() : (void*)&StateNodeHandle.GetEditorNode(), nullptr, nullptr, PortFlags, nullptr);

			FPlatformApplicationMisc::ClipboardCopy(*Value);
		}
	}
}

void FStateTreeViewModel::PasteNode(TWeakObjectPtr<UStateTreeState> State, const FGuid& ID)
{
	if (UStateTreeEditorData* EditorData = TreeDataWeak.Get())
	{
		if (UStateTreeState* OwnerState = State.Get())
		{
			UE::StateTree::Editor::Private::FStateTreeStateNodeEditorHandle StateNodeHandle(EditorData, OwnerState, ID);
			// If the op can not be executed, don't even start the transaction
			if (!StateNodeHandle.IsValid())
			{
				return;
			}

			// For Editor Node, we have a validation process which might early out if failed. Dry run with a Temp.
			FStateTreeEditorNode TempEditorNode;
			if (!StateNodeHandle.IsTransition())
			{
				static const UScriptStruct* TaskBaseStruct = FStateTreeTaskBase::StaticStruct();
				static const UScriptStruct* ConditionBaseStruct = FStateTreeConditionBase::StaticStruct();
				static const UScriptStruct* ConsiderationBaseStruct = FStateTreeConsiderationBase::StaticStruct();

				const UScriptStruct* BaseNodeScriptStruct = nullptr;
				const UScriptStruct* NodeScriptStruct = StateNodeHandle.GetEditorNode().Node.GetScriptStruct();
				if (NodeScriptStruct->IsChildOf(TaskBaseStruct))
				{
					BaseNodeScriptStruct = TaskBaseStruct;
				}
				else if (NodeScriptStruct->IsChildOf(ConditionBaseStruct))
				{
					BaseNodeScriptStruct = ConditionBaseStruct;
				}
				else if (NodeScriptStruct->IsChildOf(ConsiderationBaseStruct))
				{
					BaseNodeScriptStruct = ConsiderationBaseStruct;
				}

				const bool bSuccess = UE::StateTreeEditor::EditorNodeUtils::ImportTextAsNode(BaseNodeScriptStruct, StateNodeHandle.EditorData, TempEditorNode);
				// If the op can not be executed, don't even start the transaction
				if (!bSuccess)
				{
					return;
				}
			}

			auto PasteFunc = [&StateNodeHandle, &TempEditorNode](const TNotNull<UStateTreeState*> InOwnerState, const TNotNull<UStateTreeEditorData*> InEditorData, const FStateTreeEditPropertyPath& InPropertyPath)
				{
					InEditorData->Modify();

					UE::StateTreeEditor::FScopedEditorDataFixer DataFixer(StateNodeHandle.EditorData);
					constexpr bool bShouldCopyBindings = true;
					constexpr bool bShouldReinstantiateInstanceData = true;
					constexpr bool bShouldRegenerateGUID = true;

					if (StateNodeHandle.IsTransition())
					{
						FString PastedText;
						FPlatformApplicationMisc::ClipboardPaste(PastedText);

						const UScriptStruct* TransitionStruct = FStateTreeTransition::StaticStruct();
						TransitionStruct->ImportText(*PastedText, &StateNodeHandle.GetTransition(), nullptr, PPF_None, nullptr, TransitionStruct->GetName());
						DataFixer.TransitionsToFix.Emplace(StateNodeHandle.OwnerState, StateNodeHandle.GetTransition(), bShouldCopyBindings, bShouldReinstantiateInstanceData, bShouldRegenerateGUID);
					}
					else
					{
						StateNodeHandle.GetEditorNode() = TempEditorNode;
						DataFixer.EditorNodesToFix.Emplace(StateNodeHandle.OwnerState, StateNodeHandle.GetEditorNode(), bShouldCopyBindings, bShouldReinstantiateInstanceData, bShouldRegenerateGUID);
					}
				};

			UE::StateTree::PropertyHelpers::ModifyStateInPreAndPostEdit(
				LOCTEXT("PasteNodeTransaction", "Paste Node"),
				StateNodeHandle.OwnerState,
				StateNodeHandle.EditorData,
				StateNodeHandle.GetNodePath(),
				PasteFunc,
				INDEX_NONE, // Value Set Op, skip the index
				EPropertyChangeType::ValueSet);

			OnStateNodesChanged.Broadcast(OwnerState);
		}
	}
}

void FStateTreeViewModel::DuplicateNode(TWeakObjectPtr<UStateTreeState> State, const FGuid& ID)
{
	if (UStateTreeEditorData* EditorData = TreeDataWeak.Get())
	{
		if (UStateTreeState* OwnerState = State.Get())
		{
			UE::StateTree::Editor::Private::FStateTreeStateNodeEditorHandle StateNodeHandle(EditorData, OwnerState, ID);
			// If the op can not be executed, don't even start the transaction
			if (!StateNodeHandle.IsValid())
			{
				return;
			}

			auto DuplicateFunc = [&StateNodeHandle](const TNotNull<UStateTreeState*> InOwnerState, const TNotNull<UStateTreeEditorData*> InEditorData, const FStateTreeEditPropertyPath& InPropertyPath)
				{
					InEditorData->Modify();

					UE::StateTreeEditor::FScopedEditorDataFixer DataFixer(StateNodeHandle.EditorData);

					constexpr bool bShouldCopyBindings = true;
					constexpr bool bShouldReinstantiateInstanceData = true;
					constexpr bool bShouldRegenerateGUID = true;

					if (StateNodeHandle.IsTransition())
					{
						// Make a temp copy
						const FStateTreeTransition TempTransition = StateNodeHandle.GetTransition();
						FStateTreeTransition& NewTransition = StateNodeHandle.GetTransitionArray().Insert_GetRef(TempTransition, StateNodeHandle.GetNodeIndex());

						DataFixer.TransitionsToFix.Emplace(StateNodeHandle.OwnerState, NewTransition, bShouldCopyBindings, bShouldReinstantiateInstanceData, bShouldRegenerateGUID);
					}
					else
					{
						// Make a temp copy
						const FStateTreeEditorNode TempEditorNode = StateNodeHandle.GetEditorNode();
						FStateTreeEditorNode& NewEditorNode = StateNodeHandle.GetEditorNodeArray().Insert_GetRef(TempEditorNode, StateNodeHandle.GetNodeIndex());
						DataFixer.EditorNodesToFix.Emplace(StateNodeHandle.OwnerState, NewEditorNode, bShouldCopyBindings, bShouldReinstantiateInstanceData, bShouldRegenerateGUID);
					}
				};

			UE::StateTree::PropertyHelpers::ModifyStateInPreAndPostEdit(
				LOCTEXT("DuplicateNodeTransaction", "Duplicate Node"),
				StateNodeHandle.OwnerState,
				StateNodeHandle.EditorData,
				StateNodeHandle.GetNodePath(),
				DuplicateFunc,
				StateNodeHandle.GetNodeIndex(),
				EPropertyChangeType::Duplicate);

			OnStateNodesChanged.Broadcast(OwnerState);
		}
	}
}

void FStateTreeViewModel::SetSelection(const TArray<TWeakObjectPtr<UStateTreeState>>& InSelectedStates)
{
	if (SelectedStates.Num() == InSelectedStates.Num() && SelectedStates.Array() == InSelectedStates)
	{
		return;
	}

	SelectedStates.Reset();

	for (const TWeakObjectPtr<UStateTreeState>& State : InSelectedStates)
	{
		if (State.Get())
		{
			SelectedStates.Add(State);
		}
	}

	OnSelectionChanged.Broadcast(InSelectedStates);
}

bool FStateTreeViewModel::IsSelected(const UStateTreeState* State) const
{
	const TWeakObjectPtr<UStateTreeState> WeakState = const_cast<UStateTreeState*>(State);
	return SelectedStates.Contains(WeakState);
}

bool FStateTreeViewModel::IsChildOfSelection(const UStateTreeState* State) const
{
	for (const TWeakObjectPtr<UStateTreeState>& WeakSelectedState : SelectedStates)
	{
		if (const UStateTreeState* SelectedState = Cast<UStateTreeState>(WeakSelectedState.Get()))
		{
			if (SelectedState == State)
			{
				return true;
			}
			
			if (UE::StateTree::Editor::IsChildOf(SelectedState, State))
			{
				return true;
			}
		}
	}
	return false;
}

void FStateTreeViewModel::GetSelectedStates(TArray<UStateTreeState*>& OutSelectedStates) const
{
	OutSelectedStates.Reset();
	for (const TWeakObjectPtr<UStateTreeState>& WeakState : SelectedStates)
	{
		if (UStateTreeState* State = WeakState.Get())
		{
			OutSelectedStates.Add(State);
		}
	}
}

void FStateTreeViewModel::GetSelectedStates(TArray<TWeakObjectPtr<UStateTreeState>>& OutSelectedStates) const
{
	OutSelectedStates.Reset();
	for (const TWeakObjectPtr<UStateTreeState>& WeakState : SelectedStates)
	{
		if (WeakState.Get())
		{
			OutSelectedStates.Add(WeakState);
		}
	}
}

bool FStateTreeViewModel::HasSelection() const
{
	return SelectedStates.Num() > 0;
}

void FStateTreeViewModel::BringNodeToFocus(UStateTreeState* State, const FGuid NodeID)
{
	SetSelection(State);
	OnBringNodeToFocus.Broadcast(State, NodeID);
}

void FStateTreeViewModel::GetPersistentExpandedStates(TSet<TWeakObjectPtr<UStateTreeState>>& OutExpandedStates)
{
	OutExpandedStates.Reset();
	if (UStateTreeEditorData* TreeData = TreeDataWeak.Get())
	{
		for (UStateTreeState* SubTree : TreeData->SubTrees)
		{
			GetExpandedStatesRecursive(SubTree, OutExpandedStates);
		}
	}
}

void FStateTreeViewModel::GetExpandedStatesRecursive(UStateTreeState* State, TSet<TWeakObjectPtr<UStateTreeState>>& OutExpandedStates)
{
	if (State->bExpanded)
	{
		OutExpandedStates.Add(State);
	}
	for (UStateTreeState* Child : State->Children)
	{
		GetExpandedStatesRecursive(Child, OutExpandedStates);
	}
}


void FStateTreeViewModel::SetPersistentExpandedStates(TSet<TWeakObjectPtr<UStateTreeState>>& InExpandedStates)
{
	UStateTreeEditorData* TreeData = TreeDataWeak.Get();
	if (TreeData == nullptr)
	{
		return;
	}

	TreeData->Modify();

	for (TWeakObjectPtr<UStateTreeState>& WeakState : InExpandedStates)
	{
		if (UStateTreeState* State = WeakState.Get())
		{
			State->bExpanded = true;
		}
	}
}


void FStateTreeViewModel::AddState(UStateTreeState* AfterState)
{
	UStateTreeEditorData* TreeData = TreeDataWeak.Get();
	if (TreeData == nullptr)
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("AddStateTransaction", "Add State"));

	UStateTreeState* NewState = NewObject<UStateTreeState>(TreeData, FName(), RF_Transactional);
	UStateTreeState* ParentState = nullptr;

	if (AfterState == nullptr)
	{
		// If no subtrees, add a subtree, or add to the root state.
		if (TreeData->SubTrees.IsEmpty())
		{
			TreeData->Modify();
			TreeData->SubTrees.Add(NewState);
		}
		else
		{
			UStateTreeState* RootState = TreeData->SubTrees[0];
			if (ensureMsgf(RootState, TEXT("%s: Root state is empty."), *GetNameSafe(TreeData->GetOuter())))
			{
				RootState->Modify();
				RootState->Children.Add(NewState);
				NewState->Parent = RootState;
				ParentState = RootState;
			}
		}
	}
	else
	{
		ParentState = AfterState->Parent;
		if (ParentState != nullptr)
		{
			ParentState->Modify();
		}
		else
		{
			TreeData->Modify();
		}

		TArray<TObjectPtr<UStateTreeState>>& ParentArray = ParentState ? ParentState->Children : TreeData->SubTrees;

		const int32 TargetIndex = ParentArray.Find(AfterState);
		if (TargetIndex != INDEX_NONE)
		{
			// Insert After
			ParentArray.Insert(NewState, TargetIndex + 1);
			NewState->Parent = ParentState;
		}
		else
		{
			// Fallback, should never happen.
			ensureMsgf(false, TEXT("%s: Failed to find specified target state %s on state %s while adding new state."), *GetNameSafe(TreeData->GetOuter()), *GetNameSafe(AfterState), *GetNameSafe(ParentState));
			ParentArray.Add(NewState);
			NewState->Parent = ParentState;
		}
	}

	OnStateAdded.Broadcast(ParentState, NewState);
}

void FStateTreeViewModel::AddChildState(UStateTreeState* ParentState)
{
	UStateTreeEditorData* TreeData = TreeDataWeak.Get();
	if (TreeData == nullptr || ParentState == nullptr)
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("AddChildStateTransaction", "Add Child State"));

	UStateTreeState* NewState = NewObject<UStateTreeState>(ParentState, FName(), RF_Transactional);

	ParentState->Modify();
	
	ParentState->Children.Add(NewState);
	NewState->Parent = ParentState;

	OnStateAdded.Broadcast(ParentState, NewState);
}

void FStateTreeViewModel::RenameState(UStateTreeState* State, FName NewName)
{
	if (State == nullptr)
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("RenameTransaction", "Rename"));
	State->Modify();
	State->Name = NewName;

	TSet<UStateTreeState*> AffectedStates;
	AffectedStates.Add(State);

	FProperty* NameProperty = FindFProperty<FProperty>(UStateTreeState::StaticClass(), GET_MEMBER_NAME_CHECKED(UStateTreeState, Name));
	FPropertyChangedEvent PropertyChangedEvent(NameProperty, EPropertyChangeType::ValueSet);
	OnStatesChanged.Broadcast(AffectedStates, PropertyChangedEvent);
}

void FStateTreeViewModel::RemoveSelectedStates()
{
	UStateTreeEditorData* TreeData = TreeDataWeak.Get();
	if (TreeData == nullptr)
	{
		return;
	}

	TArray<UStateTreeState*> States;
	GetSelectedStates(States);

	// Remove items whose parent also exists in the selection.
	UE::StateTree::Editor::RemoveContainedChildren(States);

	if (States.Num() > 0)
	{
		const FScopedTransaction Transaction(LOCTEXT("DeleteStateTransaction", "Delete State"));

		TSet<UStateTreeState*> AffectedParents;

		for (UStateTreeState* StateToRemove : States)
		{
			if (StateToRemove)
			{
				StateToRemove->Modify();

				UStateTreeState* ParentState = StateToRemove->Parent;
				if (ParentState != nullptr)
				{
					AffectedParents.Add(ParentState);
					ParentState->Modify();
				}
				else
				{
					AffectedParents.Add(nullptr);
					TreeData->Modify();
				}
				
				TArray<TObjectPtr<UStateTreeState>>& ArrayToRemoveFrom = ParentState ? ParentState->Children : TreeData->SubTrees;
				const int32 ItemIndex = ArrayToRemoveFrom.Find(StateToRemove);
				if (ItemIndex != INDEX_NONE)
				{
					ArrayToRemoveFrom.RemoveAt(ItemIndex);
					StateToRemove->Parent = nullptr;
				}

			}
		}

		OnStatesRemoved.Broadcast(AffectedParents);

		ClearSelection();
	}
}

void FStateTreeViewModel::CopySelectedStates()
{
	UStateTreeEditorData* TreeData = TreeDataWeak.Get();
	if (TreeData == nullptr)
	{
		return;
	}

	TArray<UStateTreeState*> States;
	GetSelectedStates(States);
	UE::StateTree::Editor::RemoveContainedChildren(States);
	
	FString ExportedText = UE::StateTree::Editor::ExportStatesToText(TreeData, States);
	
	FPlatformApplicationMisc::ClipboardCopy(*ExportedText);
}

bool FStateTreeViewModel::CanPasteStatesFromClipboard() const
{
	FString TextToImport;
	FPlatformApplicationMisc::ClipboardPaste(TextToImport);

	UE::StateTree::Editor::FStateTreeStateTextFactory Factory;
	return Factory.CanCreateObjectsFromText(TextToImport);
}

void FStateTreeViewModel::PasteStatesFromClipboard(UStateTreeState* AfterState)
{
	UStateTreeEditorData* TreeData = TreeDataWeak.Get();
	if (TreeData == nullptr)
	{
		return;
	}
	
	if (AfterState)
	{
		const int32 Index = AfterState->Parent ? AfterState->Parent->Children.Find(AfterState) : TreeData->SubTrees.Find(AfterState);
		if (Index != INDEX_NONE)
		{
			FString TextToImport;
			FPlatformApplicationMisc::ClipboardPaste(TextToImport);
			
			const FScopedTransaction Transaction(LOCTEXT("PasteStatesTransaction", "Paste State(s)"));
			PasteStatesAsChildrenFromText(TextToImport, AfterState->Parent, Index + 1);
		}
	}
}

void FStateTreeViewModel::PasteStatesAsChildrenFromClipboard(UStateTreeState* ParentState)
{
	UStateTreeEditorData* TreeData = TreeDataWeak.Get();
	if (TreeData == nullptr)
	{
		return;
	}
	
	FString TextToImport;
	FPlatformApplicationMisc::ClipboardPaste(TextToImport);

	const FScopedTransaction Transaction(LOCTEXT("PasteStatesTransaction", "Paste State(s)"));
	PasteStatesAsChildrenFromText(TextToImport, ParentState, INDEX_NONE);
}

void FStateTreeViewModel::PasteStatesAsChildrenFromText(const FString& TextToImport, UStateTreeState* ParentState, const int32 IndexToInsertAt)
{
	UStateTreeEditorData* TreeData = TreeDataWeak.Get();
	if (TreeData == nullptr)
	{
		return;
	}

	UObject* Outer = ParentState ? static_cast<UObject*>(ParentState) : static_cast<UObject*>(TreeData);
	Outer->Modify();

	UE::StateTree::Editor::FStateTreeStateTextFactory Factory;
	Factory.ProcessBuffer(Outer, RF_Transactional, TextToImport);

	TArray<TObjectPtr<UStateTreeState>>& ParentArray = ParentState ? ParentState->Children : TreeData->SubTrees;
	const int32 TargetIndex = (IndexToInsertAt == INDEX_NONE) ? ParentArray.Num() : IndexToInsertAt;
	ParentArray.Insert(Factory.States, TargetIndex);

	TArray<FStateTreeStateLink*> Links;
	TMap<FGuid, FGuid> IDsMap;
	TArray<UStateTreeState*> NewStates;

	for (UStateTreeState* State : Factory.States)
	{		
		UE::StateTree::Editor::FixStateAfterDuplication(State, ParentState, IDsMap, Links, NewStates);
	}

	// Copy property bindings for the duplicated states.
	if (Factory.ClipboardBindings)
	{
		for (FStateTreePropertyPathBinding& Binding : Factory.ClipboardBindings->Bindings)
		{
			if (Binding.GetPropertyFunctionNode().IsValid())
			{
				UE::StateTree::Editor::FixNodesAfterDuplication(TArrayView<FStateTreeEditorNode>(Binding.GetMutablePropertyFunctionNode().GetPtr<FStateTreeEditorNode>(), 1), IDsMap, Links);
			}
		}

		for (const TPair<FGuid, FGuid>& Entry : IDsMap)
		{
			const FGuid OldTargetID = Entry.Key;
			const FGuid NewTargetID = Entry.Value;
			
			for (FStateTreePropertyPathBinding& Binding : Factory.ClipboardBindings->Bindings)
			{
				if (Binding.GetTargetPath().GetStructID() == OldTargetID)
				{
					Binding.GetMutableTargetPath().SetStructID(NewTargetID);

					if (const FGuid* NewSourceID = IDsMap.Find(Binding.GetSourcePath().GetStructID()))
					{
						Binding.GetMutableSourcePath().SetStructID(*NewSourceID);
					}
					
					TreeData->GetPropertyEditorBindings()->AddStateTreeBinding(MoveTemp(Binding));
				}
			}
		}
	}

	// Patch IDs in state links.
	for (FStateTreeStateLink* Link : Links)
	{
		if (FGuid* NewID = IDsMap.Find(Link->ID))
		{
			Link->ID = *NewID;
		}
	}

	for (UStateTreeState* State : NewStates)
	{
		OnStateAdded.Broadcast(State->Parent, State);
	}
}

void FStateTreeViewModel::DuplicateSelectedStates()
{
	UStateTreeEditorData* TreeData = TreeDataWeak.Get();
	if (TreeData == nullptr)
	{
		return;
	}

	TArray<UStateTreeState*> States;
	GetSelectedStates(States);
	UE::StateTree::Editor::RemoveContainedChildren(States);

	if (States.IsEmpty())
	{
		return;
	}
	
	FString ExportedText = UE::StateTree::Editor::ExportStatesToText(TreeData, States);

	// Place duplicates after first selected state.
	UStateTreeState* AfterState = States[0];
	
	const int32 Index = AfterState->Parent ? AfterState->Parent->Children.Find(AfterState) : TreeData->SubTrees.Find(AfterState);
	if (Index != INDEX_NONE)
	{
		const FScopedTransaction Transaction(LOCTEXT("DuplicateStatesTransaction", "Duplicate State(s)"));
		PasteStatesAsChildrenFromText(ExportedText, AfterState->Parent, Index + 1);
	}
}


void FStateTreeViewModel::MoveSelectedStatesBefore(UStateTreeState* TargetState)
{
	MoveSelectedStates(TargetState, EStateTreeViewModelInsert::Before);
}

void FStateTreeViewModel::MoveSelectedStatesAfter(UStateTreeState* TargetState)
{
	MoveSelectedStates(TargetState, EStateTreeViewModelInsert::After);
}

void FStateTreeViewModel::MoveSelectedStatesInto(UStateTreeState* TargetState)
{
	MoveSelectedStates(TargetState, EStateTreeViewModelInsert::Into);
}

bool FStateTreeViewModel::CanEnableStates() const
{
	TArray<UStateTreeState*> States;
	GetSelectedStates(States);

	for (const UStateTreeState* State : States)
	{
		// Stop if at least one state can be enabled
		if (State->bEnabled == false)
		{
			return true;
		}
	}

	return false;
}

bool FStateTreeViewModel::CanDisableStates() const
{
	TArray<UStateTreeState*> States;
	GetSelectedStates(States);

	for (const UStateTreeState* State : States)
	{
		// Stop if at least one state can be disabled
		if (State->bEnabled)
		{
			return true;
		}
	}

	return false;
}

void FStateTreeViewModel::SetSelectedStatesEnabled(const bool bEnable)
{
	TArray<UStateTreeState*> States;
	GetSelectedStates(States);

	if (States.Num() > 0)
	{
		const FScopedTransaction Transaction(LOCTEXT("SetStatesEnabledTransaction", "Set State Enabled"));

		for (UStateTreeState* State : States)
		{
			State->Modify();
			State->bEnabled = bEnable;
		}

		OnAssetChanged.Broadcast();
	}
}

void FStateTreeViewModel::MoveSelectedStates(UStateTreeState* TargetState, const EStateTreeViewModelInsert RelativeLocation)
{
	UStateTreeEditorData* TreeData = TreeDataWeak.Get();
	if (TreeData == nullptr || TargetState == nullptr)
	{
		return;
	}

	TArray<UStateTreeState*> States;
	GetSelectedStates(States);

	// Remove child items whose parent also exists in the selection.
	UE::StateTree::Editor::RemoveContainedChildren(States);

	// Remove states which contain target state as child.
	States.RemoveAll([TargetState](const UStateTreeState* State)
	{
		return UE::StateTree::Editor::IsChildOf(State, TargetState);
	});

	if (States.Num() > 0 && TargetState != nullptr)
	{
		const FScopedTransaction Transaction(LOCTEXT("MoveTransaction", "Move"));

		TSet<UStateTreeState*> AffectedParents;
		TSet<UStateTreeState*> AffectedStates;

		UStateTreeState* TargetParent = TargetState->Parent;
		if (RelativeLocation == EStateTreeViewModelInsert::Into)
		{
			AffectedParents.Add(TargetState);
		}
		else
		{
			AffectedParents.Add(TargetParent);
		}
		
		for (int32 i = States.Num() - 1; i >= 0; i--)
		{
			if (UStateTreeState* State = States[i])
			{
				State->Modify();
				AffectedParents.Add(State->Parent);
			}
		}

		if (RelativeLocation == EStateTreeViewModelInsert::Into)
		{
			// Move into
			TargetState->Modify();
		}
		
		for (UStateTreeState* Parent : AffectedParents)
		{
			if (Parent)
			{
				Parent->Modify();
			}
			else
			{
				TreeData->Modify();
			}
		}

		// Add in reverse order to keep the original order.
		for (int32 i = States.Num() - 1; i >= 0; i--)
		{
			if (UStateTreeState* SelectedState = States[i])
			{
				AffectedStates.Add(SelectedState);

				UStateTreeState* SelectedParent = SelectedState->Parent;

				// Remove from current parent
				TArray<TObjectPtr<UStateTreeState>>& ArrayToRemoveFrom = SelectedParent ? SelectedParent->Children : TreeData->SubTrees;
				const int32 ItemIndex = ArrayToRemoveFrom.Find(SelectedState);
				if (ItemIndex != INDEX_NONE)
				{
					ArrayToRemoveFrom.RemoveAt(ItemIndex);
					SelectedState->Parent = nullptr;
				}

				// Insert to new parent
				if (RelativeLocation == EStateTreeViewModelInsert::Into)
				{
					// Into
					TargetState->Children.Insert(SelectedState, /*Index*/0);
					SelectedState->Parent = TargetState;
				}
				else
				{
					TArray<TObjectPtr<UStateTreeState>>& ArrayToMoveTo = TargetParent ? TargetParent->Children : TreeData->SubTrees;
					const int32 TargetIndex = ArrayToMoveTo.Find(TargetState);
					if (TargetIndex != INDEX_NONE)
					{
						if (RelativeLocation == EStateTreeViewModelInsert::Before)
						{
							// Before
							ArrayToMoveTo.Insert(SelectedState, TargetIndex);
							SelectedState->Parent = TargetParent;
						}
						else if (RelativeLocation == EStateTreeViewModelInsert::After)
						{
							// After
							ArrayToMoveTo.Insert(SelectedState, TargetIndex + 1);
							SelectedState->Parent = TargetParent;
						}
					}
					else
					{
						// Fallback, should never happen.
						ensureMsgf(false, TEXT("%s: Failed to find specified target state %s on state %s while moving a state."), *GetNameSafe(TreeData->GetOuter()), *GetNameSafe(TargetState), *GetNameSafe(SelectedParent));
						ArrayToMoveTo.Add(SelectedState);
						SelectedState->Parent = TargetParent;
					}
				}
			}
		}

		OnStatesMoved.Broadcast(AffectedParents, AffectedStates);

		TArray<TWeakObjectPtr<UStateTreeState>> WeakStates;
		for (UStateTreeState* State : States)
		{
			WeakStates.Add(State);
		}

		SetSelection(WeakStates);
	}
}


void FStateTreeViewModel::BindToDebuggerDelegates()
{
#if WITH_STATETREE_TRACE_DEBUGGER
	Debugger->OnActiveStatesChanged.BindSPLambda(this, [this](const FStateTreeTraceActiveStates& NewActiveStates)
	{
		if (NewActiveStates.PerAssetStates.Num() == 0)
		{
			ActiveStates.Empty();
		}
		else if (const UStateTree* OuterStateTree = GetStateTree())
		{
			for (const FStateTreeTraceActiveStates::FAssetActiveStates& AssetActiveStates : NewActiveStates.PerAssetStates)
			{
				// Only track states owned by the StateTree associated to the view model (skip linked assets)
				if (AssetActiveStates.WeakStateTree == OuterStateTree)
				{
					ActiveStates.Reset(AssetActiveStates.ActiveStates.Num());
					for (const FStateTreeStateHandle Handle : AssetActiveStates.ActiveStates)
					{
						ActiveStates.Add(OuterStateTree->GetStateIdFromHandle(Handle));
					}
				}
			}
		}
	});
#endif // WITH_STATETREE_TRACE_DEBUGGER
}

bool FStateTreeViewModel::IsStateActiveInDebugger(const UStateTreeState& State) const
{
#if WITH_STATETREE_TRACE_DEBUGGER
	return ActiveStates.Contains(State.ID);
#else
	return false;
#endif // WITH_STATETREE_TRACE_DEBUGGER
}

#undef LOCTEXT_NAMESPACE
