// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/MemStack.h"
#include "TraitCore/ExecutionContext.h"
#include "TraitCore/ITraitInterface.h"
#include "TraitCore/TraitBinding.h"

namespace UE::AnimNext
{
	// An array of children pointers
	// We reserve a small amount inline and spill on the memstack
	using FChildrenArray = TArray<FWeakTraitPtr, TInlineAllocator<8, TMemStackAllocator<>>>;

	/**
	 * IHierarchy
	 * 
	 * This interface exposes hierarchy traversal information to navigate the graph.
	 */
	struct IHierarchy : ITraitInterface
	{
		DECLARE_ANIM_TRAIT_INTERFACE(IHierarchy)

		// Returns the number of children of the trait implementation (not the whole stack)
		// Includes inactive children
		ANIMNEXTANIMGRAPH_API virtual uint32 GetNumChildren(const FExecutionContext& Context, const TTraitBinding<IHierarchy>& Binding) const;

		// Appends weak handles to any children we wish to traverse on the trait implementation (not the whole stack).
		// Traits are responsible for allocating and releasing child instance data.
		// Empty handles can be appended.
		ANIMNEXTANIMGRAPH_API virtual void GetChildren(const FExecutionContext& Context, const TTraitBinding<IHierarchy>& Binding, FChildrenArray& Children) const;

		// Queries the trait stack and calls GetChildren for each trait, appending the result.
		ANIMNEXTANIMGRAPH_API static void GetStackChildren(const FExecutionContext& Context, const FTraitStackBinding& Binding, FChildrenArray& Children);

		// Queries the trait stack of the specified binding and calls GetChildren for each trait, appending the result.
		static void GetStackChildren(const FExecutionContext& Context, const FTraitBinding& Binding, FChildrenArray& Children);

		// Queries the trait stack and calls GetNumChildren for each trait, accumulating the result.
		ANIMNEXTANIMGRAPH_API static uint32 GetNumStackChildren(const FExecutionContext& Context, const FTraitStackBinding& Binding);

		// Queries the trait stack of the specified binding and calls GetNumChildren for each trait, accumulating the result.
		static uint32 GetNumStackChildren(const FExecutionContext& Context, const FTraitBinding& Binding);

		// Queries the trait stack and then its forwarding children for a trait implementing the specified interface.
		// The trait child found implementing the interface is returned
		template<class TraitInterface>
		static bool GetForwardedStackInterface(const FExecutionContext& Context, const FTraitStackBinding& StackBinding, FTraitStackBinding& OutStackBinding, TTraitBinding<TraitInterface>& OutBinding)
		{
			// Start with the supplied stack
			OutStackBinding = StackBinding;
			while (true)
			{
				if (OutStackBinding.GetInterface<TraitInterface>(OutBinding))
				{
					return true;
				}

				// Forward to child stack, if any
				FWeakTraitPtr ForwardingChild = GetStackForwardingChild(Context, OutStackBinding);
				if (!ForwardingChild.IsValid())
				{
					return false;
				}
				ensure(Context.GetStack(ForwardingChild, OutStackBinding));
			}
		}

		// Queries the trait's forwarding children for a trait implementing the specified interface.
		// The first child found implementing the interface is returned
		template<class TraitInterface>
		static bool GetForwardedStackInterface(const FExecutionContext& Context, const FTraitBinding& Binding, FTraitStackBinding& OutStackBinding, TTraitBinding<TraitInterface>& OutBinding)
		{
			return Binding.IsValid() ? GetForwardedStackInterface(Context, *Binding.GetStack(), OutStackBinding, OutBinding) : false;
		}

	private:
		// Queries the trait stack of the specified binding and calls GetForwardingChild for each trait.
		// If there is exactly one child, it is returned.
		ANIMNEXTANIMGRAPH_API static FWeakTraitPtr GetStackForwardingChild(const FExecutionContext& Context, const FTraitStackBinding& Binding);

	public:
#if WITH_EDITOR
		ANIMNEXTANIMGRAPH_API virtual const FText& GetDisplayName() const override;
		ANIMNEXTANIMGRAPH_API virtual const FText& GetDisplayShortName() const override;
#endif // WITH_EDITOR
	};

	/**
	 * Specialization for trait binding.
	 */
	template<>
	struct TTraitBinding<IHierarchy> : FTraitBinding
	{
		// @see IHierarchy::GetNumChildren
		uint32 GetNumChildren(const FExecutionContext& Context) const
		{
			return GetInterface()->GetNumChildren(Context, *this);
		}

		// @see IHierarchy::GetChildren
		void GetChildren(const FExecutionContext& Context, FChildrenArray& Children) const
		{
			GetInterface()->GetChildren(Context, *this, Children);
		}
		
	protected:
		const IHierarchy* GetInterface() const { return GetInterfaceTyped<IHierarchy>(); }
	};

	//////////////////////////////////////////////////////////////////////////
	// Inline implementations

	inline void IHierarchy::GetStackChildren(const FExecutionContext& Context, const FTraitBinding& Binding, FChildrenArray& Children)
	{
		if (Binding.IsValid())
		{
			GetStackChildren(Context, *Binding.GetStack(), Children);
		}
	}

	inline uint32 IHierarchy::GetNumStackChildren(const FExecutionContext& Context, const FTraitBinding& Binding)
	{
		return Binding.IsValid() ? GetNumStackChildren(Context, *Binding.GetStack()) : 0;
	}
}
