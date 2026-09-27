// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StructUtils/StructView.h"
#include "TraitCore/ITraitInterface.h"
#include "TraitCore/TraitBinding.h"

#define UE_API ANIMNEXTANIMGRAPH_API

class UAnimNextAnimationGraph;
struct FAnimNextDataInterfacePayload;

namespace UE::AnimNext
{
	/**
	 * IGraphFactory
	 *
	 * This interface translates an object and payload struct into a UAnimNextAnimationGraph and payload instance(s), ready for instantiation
	 */
	struct IGraphFactory : ITraitInterface
	{
		DECLARE_ANIM_TRAIT_INTERFACE(IGraphFactory)

		// Gets a graph and a set of native interfaces from an object
		// @param	InObject				The object to manufacture a graph for (e.g. AnimSequence). If this is a UAnimNextAnimationGraph, it should be returned verbatim
		// @param	InOutGraphPayload			Payload that can be used to communicate with a graph instance
		// @return	The graph to be used.
		UE_API virtual const UAnimNextAnimationGraph* GetGraphFromObject(FExecutionContext& Context, const TTraitBinding<IGraphFactory>& Binding, const UObject* InObject, FAnimNextDataInterfacePayload& InOutPayload) const;

		// Creates data interface payload to bind when creating a graph for an object
		// @param	InObject					The object (e.g. AnimSequence). If this is a UAnimNextAnimationGraph, it should be the same object as InGraph
		// @param	InOutGraphPayload			Payload that can be used to communicate with a graph instance
		UE_API virtual void CreatePayloadForObject(FExecutionContext& Context, const TTraitBinding<IGraphFactory>& Binding, const UObject* InObject, FAnimNextDataInterfacePayload& InOutPayload) const;

		// Gets a graph and a set of native interfaces from an object
		// Falls back to using user settings if no interface is found in the current stack, or if an interface was found but returns nullptr
		// @param	InObject				The object to manufacture a graph for (e.g. AnimSequence). If this is a UAnimNextAnimationGraph, it should be returned verbatim
		// @param	InOutGraphPayload			Payload that can be used to communicate with a graph instance
		// @return	The graph to be used.
		static UE_API const UAnimNextAnimationGraph* GetGraphFromObjectWithFallback(FExecutionContext& Context, const FTraitBinding& InBinding, const UObject* InObject, FAnimNextDataInterfacePayload& InOutPayload);

#if WITH_EDITOR
		UE_API virtual const FText& GetDisplayName() const override;
		UE_API virtual const FText& GetDisplayShortName() const override;
#endif // WITH_EDITOR
	};

	/**
	 * Specialization for trait binding.
	 */
	template<>
	struct TTraitBinding<IGraphFactory> : FTraitBinding
	{
		// @see IGraphFactory::GetGraphFromObject
		const UAnimNextAnimationGraph* GetGraphFromObject(FExecutionContext& Context, const UObject* InObject, FAnimNextDataInterfacePayload& InOutPayload) const
		{
			return GetInterface()->GetGraphFromObject(Context, *this, InObject, InOutPayload);
		}

		// @see IGraphFactory::CreatePayloadForObject
		void CreatePayloadForObject(FExecutionContext& Context, const UObject* InObject, FAnimNextDataInterfacePayload& InOutPayload) const
		{
			GetInterface()->CreatePayloadForObject(Context, *this, InObject, InOutPayload);
		}

	protected:
		const IGraphFactory* GetInterface() const { return GetInterfaceTyped<IGraphFactory>(); }
	};
}

#undef UE_API
