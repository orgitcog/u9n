// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AlphaBlend.h"
#include "TraitCore/ITraitInterface.h"
#include "TraitCore/TraitBinding.h"

#define UE_API ANIMNEXTANIMGRAPH_API

class IBlendProfileInterface;
class UCurveFloat;

namespace UE::AnimNext
{
	/**
	 * ISmoothBlendPerBone
	 *
	 * This interface exposes blend smoothing per bone related information.
	 */
	struct ISmoothBlendPerBone : ITraitInterface
	{
		DECLARE_ANIM_TRAIT_INTERFACE(ISmoothBlendPerBone)

		// Returns the desired blend profile for the specified child
		UE_API virtual TSharedPtr<const IBlendProfileInterface> GetBlendProfile(FExecutionContext& Context, const TTraitBinding<ISmoothBlendPerBone>& Binding, int32 ChildIndex) const;

#if WITH_EDITOR
		UE_API virtual const FText& GetDisplayName() const override;
		UE_API virtual const FText& GetDisplayShortName() const override;
#endif // WITH_EDITOR
	};

	/**
	 * Specialization for trait binding.
	 */
	template<>
	struct TTraitBinding<ISmoothBlendPerBone> : FTraitBinding
	{
		// @see ISmoothBlendPerBone::GetBlendProfile
		const TSharedPtr<const IBlendProfileInterface> GetBlendProfile(FExecutionContext& Context, int32 ChildIndex) const
		{
			return GetInterface()->GetBlendProfile(Context, *this, ChildIndex);
		}

	protected:
		const ISmoothBlendPerBone* GetInterface() const { return GetInterfaceTyped<ISmoothBlendPerBone>(); }
	};
}

#undef UE_API
