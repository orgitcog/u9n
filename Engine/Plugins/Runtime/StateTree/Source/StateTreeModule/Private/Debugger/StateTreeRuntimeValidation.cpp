// Copyright Epic Games, Inc. All Rights Reserved.

#include "Debugger/StateTreeRuntimeValidation.h"
#include "Debugger/StateTreeRuntimeValidationInstanceData.h"

#if WITH_STATETREE_DEBUG
#include "HAL/IConsoleManager.h"
#include "StateTree.h"
#endif

namespace UE::StateTree::Debug
{
#if WITH_STATETREE_DEBUG

namespace Private
{
bool bRuntimeValidationContext = true;
static FAutoConsoleVariableRef CVarRuntimeValidationContext(
	TEXT("StateTree.RuntimeValidation.Context"),
	bRuntimeValidationContext,
	TEXT("Test if the context creation parameters are the same between each creation of StateTreeExecutionContext.")
);

bool bRuntimeValidationDoesNewerVersionExists = true;
static FAutoConsoleVariableRef CVarRuntimeValidationDoesNewerVersionExists(
	TEXT("StateTree.RuntimeValidation.DoesNewerVersionExists"),
	bRuntimeValidationDoesNewerVersionExists,
	TEXT("Test if a StateTreeExecutionContext started with an old version of a blueprint type.")
);

bool bRuntimeValidationEnterExitState = false;
static FAutoConsoleVariableRef CVarRuntimeValidationEnterExitState(
	TEXT("StateTree.RuntimeValidation.EnterExitState"),
	bRuntimeValidationEnterExitState,
	TEXT("Test that if a node get a EnterState, it will receive an ExitState.\n"
		"Test that if a node get a ExitState, it did receive an EnterState before.")
);

FString NodeToString(const UObject* Obj, FGuid Id)
{
	TStringBuilderWithBuffer<TCHAR, 128> Buffer;
	if (Obj)
	{
		Obj->GetPathName(nullptr, Buffer);
	}
	Buffer << TEXT(':');
	Buffer << Id;
	return Buffer.ToString();
}
} // namespace Private

FRuntimeValidationInstanceData::~FRuntimeValidationInstanceData()
{
	if (Private::bRuntimeValidationEnterExitState && UObjectInitialized() && !IsEngineExitRequested())
	{
		for (const FNodeStatePair& Pair : NodeStates)
		{
			if (EnumHasAllFlags(Pair.State, EState::BetweenEnterExitState))
			{
				ensureAlwaysMsgf(false, TEXT("Tree exited. Missing ExitState on %s."), *Private::NodeToString(StateTree.Get(), Pair.NodeID));
				Private::bRuntimeValidationEnterExitState = false;
			}
		}
	}
}

void FRuntimeValidationInstanceData::SetContext(const UObject* InNewOwner, const UStateTree* InNewStateTree)
{
	TWeakObjectPtr<const UStateTree> NewStateTree = InNewStateTree;
	TWeakObjectPtr<const UObject> NewOwner = InNewOwner;
	if (Private::bRuntimeValidationContext)
	{
		if (StateTree.IsValid() && StateTree != NewStateTree)
		{
			ensureAlwaysMsgf(false, TEXT("StateTree runtime check failed: The StateTree '%s' is different from the previously set '%s'.\n"
				"Make sure you initialize FStateTreeExecutionContext with the same value every time.\n"
				"Auto deactivate Runtime check StateTree.RuntimeValidation.Context to prevent reporting the same error multiple times.")
				, InNewStateTree ? *InNewStateTree->GetFullName() : TEXT("StateTree"), *StateTree.Get()->GetFullName());
			Private::bRuntimeValidationContext = false;
		}
		if (Owner.IsValid() && Owner != NewOwner)
		{
			ensureAlwaysMsgf(false, TEXT("StateTree runtime check failed: The owner '%s' is different from the previously set '%s'.\n"
				"Make sure you initialize FStateTreeExecutionContext with the same values every time.\n"
				"Auto deactivate Runtime check StateTree.RuntimeValidation.Context to prevent reporting the same error multiple times.")
				, InNewOwner ? *InNewOwner->GetFullName() : TEXT("owner"), *Owner.Get()->GetFullName());
			Private::bRuntimeValidationContext = false;
		}
	}

	ValidatesTreeNodes(InNewStateTree);

	StateTree = NewStateTree;
	Owner = NewOwner;
}

void FRuntimeValidationInstanceData::NodeEnterState(FGuid NodeID, FActiveFrameID FrameID)
{
	FNodeStatePair* Found = NodeStates.FindByPredicate([&NodeID, &FrameID](const FNodeStatePair& Other)
	{
		return Other.NodeID == NodeID && Other.FrameID == FrameID;
	});
	if (Found)
	{
		if (Private::bRuntimeValidationEnterExitState && EnumHasAllFlags(Found->State, EState::BetweenEnterExitState))
		{
			ensureAlwaysMsgf(false, TEXT("StateTree runtime check failed: EnterState executed on node %s without an ExitState.\n"
				"Auto deactivate Runtime check StateTree.RuntimeValidation.EnterExitState to prevent reporting the same error multiple times.")
				, *Private::NodeToString(Owner.Get(), NodeID));
			Private::bRuntimeValidationEnterExitState = false;
		}
		EnumAddFlags(Found->State, EState::BetweenEnterExitState);
	}
	else
	{
		NodeStates.Add(FNodeStatePair{.NodeID = NodeID, .FrameID = FrameID, .State = EState::BetweenEnterExitState });
	}
}

void FRuntimeValidationInstanceData::NodeExitState(FGuid NodeID, FActiveFrameID FrameID)
{
	FNodeStatePair* Found = NodeStates.FindByPredicate([&NodeID, &FrameID](const FNodeStatePair& Other)
	{
		return Other.NodeID == NodeID && Other.FrameID == FrameID;
	});
	if (Found)
	{
		if (Private::bRuntimeValidationEnterExitState && !EnumHasAllFlags(Found->State, EState::BetweenEnterExitState))
		{
			ensureAlwaysMsgf(false, TEXT("StateTree runtime check failed: ExitState executed on node %s without an EnterState.\n"
				"Auto deactivate Runtime check StateTree.RuntimeValidation.EnterExitState to prevent reporting the same error multiple times.")
				, *Private::NodeToString(Owner.Get(), NodeID));
			Private::bRuntimeValidationEnterExitState = false;
		}
		EnumRemoveFlags(Found->State, EState::BetweenEnterExitState);
	}
	else if (Private::bRuntimeValidationEnterExitState)
	{
		ensureAlwaysMsgf(false, TEXT("StateTree runtime check failed: ExitState executed on node %s without an EnterState.\n"
			"Auto deactivate Runtime check StateTree.RuntimeValidation.EnterExitState to prevent reporting the same error multiple times.")
			, *Private::NodeToString(Owner.Get(), NodeID));
		Private::bRuntimeValidationEnterExitState = false;
	}
}

void FRuntimeValidationInstanceData::ValidatesTreeNodes(const UStateTree* InNewStateTree) const
{
	if (Private::bRuntimeValidationDoesNewerVersionExists)
	{
		if (InNewStateTree && InNewStateTree->IsReadyToRun())
		{
			auto DoesNewerVersionExists = [](const UObject* InstanceDataType)
			{
				// Is the class/scriptstruct a blueprint that got replaced by another class.
				bool bHasNewerVersionExistsFlag = InstanceDataType->HasAnyFlags(RF_NewerVersionExists);
				if (!bHasNewerVersionExistsFlag)
				{
					if (const UClass* InstanceDataClass = Cast<UClass>(InstanceDataType))
					{
						bHasNewerVersionExistsFlag = InstanceDataClass->HasAnyClassFlags(CLASS_NewerVersionExists);
					}
					else if (const UScriptStruct* InstanceDataStruct = Cast<UScriptStruct>(InstanceDataType))
					{
						bHasNewerVersionExistsFlag = (InstanceDataStruct->StructFlags & STRUCT_NewerVersionExists) != 0;
					}
				}
				return bHasNewerVersionExistsFlag;
			};
			{
				const FStateTreeInstanceData& InstanceData = InNewStateTree->GetDefaultInstanceData();
				const int32 InstanceDataNum = InstanceData.Num();
				for (int32 Index = 0; Index < InstanceDataNum; ++Index)
				{
					bool bFailed = false;
					const UObject* InstanceObject = nullptr;
					if (InstanceData.IsObject(Index))
					{
						InstanceObject = InstanceData.GetObject(Index);
						bFailed = DoesNewerVersionExists(InstanceObject)
							|| (InstanceObject && DoesNewerVersionExists(InstanceObject->GetClass()));
					}
					else
					{
						InstanceObject = InstanceData.GetStruct(Index).GetScriptStruct();
						bFailed = DoesNewerVersionExists(InstanceObject);
					}

					if (bFailed)
					{
						ensureAlwaysMsgf(false, TEXT("StateTree runtime check failed: The data '%s' has a newest version.\n"
							"It should be detected in StateTree::Link.\n"
							"Auto deactivate Runtime check StateTree.RuntimeValidation.DoesNewerVersionExists to prevent reporting the same error multiple times.")
							, *InstanceObject->GetFullName());
						Private::bRuntimeValidationDoesNewerVersionExists = false;
					}
				}
			}

			for (FConstStructView NodeView : InNewStateTree->GetNodes())
			{
				const FStateTreeNodeBase* Node = NodeView.GetPtr<const FStateTreeNodeBase>();
				if (Node)
				{
					const UStruct* DesiredInstanceDataType = Node->GetInstanceDataType();
					if (DoesNewerVersionExists(DesiredInstanceDataType))
					{
						ensureAlwaysMsgf(false, TEXT("StateTree runtime check failed: The node '%s' has a newest version.\n"
							"It should be detected in StateTree::Link.\n"
							"Auto deactivate Runtime check StateTree.RuntimeValidation.DoesNewerVersionExists to prevent reporting the same error multiple times.")
							, *DesiredInstanceDataType->GetFullName());
						Private::bRuntimeValidationDoesNewerVersionExists = false;
					}
				}
			}
		}
	}
}
#endif //WITH_STATETREE_DEBUG

/**
 * 
 */


#if WITH_STATETREE_DEBUG
FRuntimeValidation::FRuntimeValidation(TNotNull<FRuntimeValidationInstanceData*> Instance)
	: RuntimeValidationData(Instance)
{
}

FRuntimeValidationInstanceData* FRuntimeValidation::GetInstanceData() const
{
	return RuntimeValidationData;
}

void FRuntimeValidation::SetContext(const UObject* Owner, const UStateTree* StateTree) const
{
	if (RuntimeValidationData)
	{
		RuntimeValidationData->SetContext(Owner, StateTree);
	}
}

#else

void FRuntimeValidation::SetContext(const UObject*, const UStateTree*) const
{
}

#endif //WITH_STATETREE_DEBUG

} // UE::StateTree::Debug


