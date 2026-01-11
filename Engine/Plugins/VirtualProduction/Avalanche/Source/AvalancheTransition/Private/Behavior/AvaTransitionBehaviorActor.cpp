// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaTransitionBehaviorActor.h"
#include "AvaTransitionContext.h"
#include "AvaTransitionEnums.h"
#include "AvaTransitionScene.h"
#include "AvaTransitionSubsystem.h"
#include "AvaTransitionTree.h"
#include "AvaTransitionTreeSchema.h"
#include "Engine/World.h"
#include "IAvaTransitionModule.h"
#include "StateTreeDelegates.h"
#include "StateTreeExecutionContext.h"
#include "StateTreeTaskBase.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

AAvaTransitionBehaviorActor::AAvaTransitionBehaviorActor()
{
	// TickBehavior is called by the Subsystem separately  
	PrimaryActorTick.bCanEverTick = false;

	TransitionTree = CreateDefaultSubobject<UAvaTransitionTree>(TEXT("Transition Logic"));
	StateTreeReference.SetStateTree(TransitionTree);
}

void AAvaTransitionBehaviorActor::PostActorCreated()
{
	Super::PostActorCreated();

	ValidateTransitionTree();

	if (UAvaTransitionSubsystem* const TransitionSubsystem = GetTransitionSubsystem())
	{
		TransitionSubsystem->RegisterTransitionBehavior(GetLevel(), this);
	}
}

void AAvaTransitionBehaviorActor::PostLoad()
{
	Super::PostLoad();
	ValidateTransitionTree();
}

#if WITH_EDITOR
bool AAvaTransitionBehaviorActor::CanEditChange(const FProperty* InProperty) const
{
	return Super::CanEditChange(InProperty);
}

bool AAvaTransitionBehaviorActor::CanEditChange(const FEditPropertyChain& InPropertyChain) const
{
	if (!UObject::CanEditChange(InPropertyChain))
	{
		return false;
	}

	auto GetPropertyName =
		[](TDoubleLinkedList<FProperty*>::TDoubleLinkedListNode* InChainNode)->FName
		{
			if (InChainNode && InChainNode->GetValue())
			{
				return InChainNode->GetValue()->GetFName();
			}
			return NAME_None;	
		};

	const FName MemberPropertyName = GetPropertyName(InPropertyChain.GetActiveMemberNode());
	const FName ActivePropertyName = GetPropertyName(InPropertyChain.GetActiveNode());

	// Disable change for StateTree in FStateTreeReference
	return MemberPropertyName != GET_MEMBER_NAME_CHECKED(AAvaTransitionBehaviorActor, StateTreeReference)
		|| ActivePropertyName != TEXT("StateTree");
}
#endif

UAvaTransitionSubsystem* AAvaTransitionBehaviorActor::GetTransitionSubsystem() const
{
	UWorld* const World = GetWorld();
	return World ? World->GetSubsystem<UAvaTransitionSubsystem>() : nullptr;
}

void AAvaTransitionBehaviorActor::ValidateTransitionTree()
{
	if (!ensureAlwaysMsgf(TransitionTree, TEXT("Transition Tree is null. Cannot not validate tree")))
	{
		return;
	}

	IAvaTransitionModule::FOnValidateTransitionTree& OnValidateTransitionTree = IAvaTransitionModule::Get().GetOnValidateTransitionTree();

	if (!OnValidateTransitionTree.IsBound())
	{
#if WITH_EDITOR
		if (GEditor)
		{
			ensureAlwaysMsgf(false, TEXT("OnValidateTransitionTree expected to be bound by FAvaTransitionEditorModule"));
		}
#endif
		return;
	}

	OnValidateTransitionTree.Execute(TransitionTree);
}
