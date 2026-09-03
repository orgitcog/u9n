// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StateTreeExecutionExtension.generated.h"

class UObject;
class UStateTree;
struct FStateTreeInstanceStorage;
struct FStateTreeTransitionDelayedState;
struct FStateTreeReferenceOverrides;


/** Used by the execution context or a weak execution context to extend their functionalities. */
USTRUCT()
struct FStateTreeExecutionExtension
{
	GENERATED_BODY()

public:
	struct FContextParameters
	{
		FContextParameters(UObject& Owner, const UStateTree& StateTree, FStateTreeInstanceStorage& InstanceData)
			: Owner(Owner)
			, StateTree(StateTree)
			, InstanceData(InstanceData)
		{}
		FContextParameters(TNotNull<UObject*> Owner, TNotNull<const UStateTree*> StateTree, FStateTreeInstanceStorage& InstanceData)
			: Owner(*Owner)
			, StateTree(*StateTree)
			, InstanceData(InstanceData)
		{
		}
		UObject& Owner;
		const UStateTree& StateTree;
		FStateTreeInstanceStorage& InstanceData;
	};

	virtual ~FStateTreeExecutionExtension() = default;

	/** Prefix that will be used by STATETREE_LOG and STATETREE_CLOG, using Entity description. */
	virtual FString GetInstanceDescription(const FContextParameters& Context) const
	{
		return Context.Owner.GetName();
	}

	/** Callback when the execution context request the tree to wakeup from a schedule tick sleep. */
	virtual void ScheduleNextTick(const FContextParameters& Context)
	{
	}

	/** Callback when the overrides are set to the execution context . */
	virtual void OnLinkedStateTreeOverridesSet(const FContextParameters& Context, const FStateTreeReferenceOverrides& Overrides)
	{
	}
};
