// Copyright Epic Games, Inc. All Rights Reserved.

#include "TraitInterfaces/IGraphFactory.h"

#include "AnimNextAnimGraphSettings.h"
#include "TraitCore/ExecutionContext.h"

namespace UE::AnimNext
{
	AUTO_REGISTER_ANIM_TRAIT_INTERFACE(IGraphFactory)

#if WITH_EDITOR
	const FText& IGraphFactory::GetDisplayName() const
	{
		static FText InterfaceName = NSLOCTEXT("TraitInterfaces", "TraitInterface_IGraphFactory_Name", "Graph Factory");
		return InterfaceName;
	}
	const FText& IGraphFactory::GetDisplayShortName() const
	{
		static FText InterfaceShortName = NSLOCTEXT("TraitInterfaces", "TraitInterface_IGraphFactory_ShortName", "GF");
		return InterfaceShortName;
	}
#endif // WITH_EDITOR

	const UAnimNextAnimationGraph* IGraphFactory::GetGraphFromObject(FExecutionContext& Context, const TTraitBinding<IGraphFactory>& Binding, const UObject* InObject, FAnimNextDataInterfacePayload& InOutPayload) const
	{
		TTraitBinding<IGraphFactory> SuperBinding;
		if (Binding.GetStackInterfaceSuper(SuperBinding))
		{
			return SuperBinding.GetGraphFromObject(Context, InObject, InOutPayload);
		}

		return nullptr;
	}

	void IGraphFactory::CreatePayloadForObject(FExecutionContext& Context, const TTraitBinding<IGraphFactory>& Binding, const UObject* InObject, FAnimNextDataInterfacePayload& InOutPayload) const
	{
		TTraitBinding<IGraphFactory> SuperBinding;
		if (Binding.GetStackInterfaceSuper(SuperBinding))
		{
			return SuperBinding.CreatePayloadForObject(Context, InObject, InOutPayload);
		}
	}

	const UAnimNextAnimationGraph* IGraphFactory::GetGraphFromObjectWithFallback(FExecutionContext& Context, const FTraitBinding& InBinding, const UObject* InObject, FAnimNextDataInterfacePayload& InOutPayload)
	{
		// Graph creation works as follows:
		//	- Caller needs to create and populate the native interface for the specified object (e.g. anim sequence)
		//	  The caller knows the object it has and how to populate that native interface
		//	- We then ask the trait stack to populate other native interfaces it knows about (e.g. sync group trait)
		//	- Now we have everything we need to create the graph, ask the trait stack to do so
		//		- If the trait stack fails to create a graph, create one using our user settings
		//	- Now that we have a graph, create any remaining payloads that might be missing

		const UAnimNextAnimationGraph* AnimationGraph = nullptr;

		TTraitBinding<IGraphFactory> GraphFactoryBinding;
		if (InBinding.GetStackInterface<IGraphFactory>(GraphFactoryBinding))
		{
			// Ask trait stack to create payloads it cares about
			GraphFactoryBinding.CreatePayloadForObject(Context, InObject, InOutPayload);

			// Ask trait stack to create a graph from our object/payload
			AnimationGraph = GraphFactoryBinding.GetGraphFromObject(Context, InObject, InOutPayload);
		}

		// If we failed, fallback to our user settings
		if (AnimationGraph == nullptr)
		{
			AnimationGraph = GetDefault<UAnimNextAnimGraphSettings>()->GetGraphFromObject(InObject, InOutPayload);
		}

		// Create missing payloads, if any
		if (AnimationGraph != nullptr)
		{
			GetDefault<UAnimNextAnimGraphSettings>()->GetNativePayloadFromGraph(InObject, AnimationGraph, InOutPayload);
		}

		return AnimationGraph;
	}
}
