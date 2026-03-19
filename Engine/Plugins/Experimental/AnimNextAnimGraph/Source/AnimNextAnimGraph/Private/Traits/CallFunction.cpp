// Copyright Epic Games, Inc. All Rights Reserved.

#include "Traits/CallFunction.h"

#include "Graph/AnimNextAnimationGraph.h"
#include "Graph/AnimNextGraphContextData.h"
#include "Graph/AnimNextGraphInstance.h"
#include "TraitCore/ExecutionContext.h"
#include "TraitCore/NodeInstance.h"
#include "AnimNextAnimGraphStats.h"
#include "AnimNextExecuteContext.h"

DEFINE_STAT(STAT_AnimNext_CallFunction);

namespace UE::AnimNext
{
	AUTO_REGISTER_ANIM_TRAIT(FCallFunctionTrait)

	// Trait implementation boilerplate
	#define TRAIT_INTERFACE_ENUMERATOR(GeneratorMacro) \
		GeneratorMacro(IUpdate)

	GENERATE_ANIM_TRAIT_IMPLEMENTATION(FCallFunctionTrait, TRAIT_INTERFACE_ENUMERATOR, NULL_ANIM_TRAIT_INTERFACE_ENUMERATOR, NULL_ANIM_TRAIT_EVENT_ENUMERATOR)

	#undef TRAIT_INTERFACE_ENUMERATOR

	void FCallFunctionTrait::OnBecomeRelevant(FUpdateTraversalContext& Context, const TTraitBinding<IUpdate>& Binding, const FTraitUpdateState& TraitState) const
	{
		CallFunctionForMatchingSite(Binding, TraitState, EAnimNextCallFunctionCallSite::BecomeRelevant);
		IUpdate::OnBecomeRelevant(Context, Binding, TraitState);
	}

	void FCallFunctionTrait::PreUpdate(FUpdateTraversalContext& Context, const TTraitBinding<IUpdate>& Binding, const FTraitUpdateState& TraitState) const
	{
		CallFunctionForMatchingSite(Binding,  TraitState, EAnimNextCallFunctionCallSite::PreUpdate);
		IUpdate::PreUpdate(Context, Binding, TraitState);
	}

	void FCallFunctionTrait::PostUpdate(FUpdateTraversalContext& Context, const TTraitBinding<IUpdate>& Binding, const FTraitUpdateState& TraitState) const
	{
		IUpdate::PostUpdate(Context, Binding, TraitState);
		CallFunctionForMatchingSite(Binding, TraitState,  EAnimNextCallFunctionCallSite::PostUpdate);
	}

	void FCallFunctionTrait::CallFunctionForMatchingSite(const TTraitBinding<IUpdate>& InBinding, const FTraitUpdateState& InTraitState, EAnimNextCallFunctionCallSite InCallSite) const
	{
		const FSharedData& SharedData = *InBinding.GetSharedData<FSharedData>(); 
		if(SharedData.CallSite != InCallSite || SharedData.FunctionEvent.IsNone())
		{
			return;
		}

		SCOPE_CYCLE_COUNTER(STAT_AnimNext_CallFunction);

		FNodeInstance* ParentNodeInstance = InBinding.GetTraitPtr().GetNodeInstance();
		FAnimNextGraphInstance& GraphInstance = ParentNodeInstance->GetOwner();
		const UAnimNextAnimationGraph* AnimationGraph = GraphInstance.GetAnimationGraph();
		if(AnimationGraph == nullptr)
		{
			return;
		}

		// TODO: const_cast: Can ExecuteVM be made const?
		URigVM* VM = const_cast<URigVM*>(AnimationGraph->GetRigVM());
		if (VM == nullptr)
		{
			return;
		}

		FRigVMExtendedExecuteContext& ExtendedExecuteContext = GraphInstance.GetExtendedExecuteContext();
		FAnimNextExecuteContext& AnimNextContext = GraphInstance.GetExtendedExecuteContext().GetPublicDataSafe<FAnimNextExecuteContext>();

		// Propagate delta time
		AnimNextContext.SetDeltaTime(InTraitState.GetDeltaTime());

		FAnimNextGraphContextData ContextData(GraphInstance.GetModuleInstance(), &GraphInstance);
		UE::AnimNext::FScopedExecuteContextData ContextDataScope(AnimNextContext, ContextData);

		VM->ExecuteVM(ExtendedExecuteContext, SharedData.FunctionEvent);
	}
}
