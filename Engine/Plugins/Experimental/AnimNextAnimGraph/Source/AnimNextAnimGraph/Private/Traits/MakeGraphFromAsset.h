// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TraitCore/Trait.h"
#include "TraitInterfaces/IGraphFactory.h"

#include "MakeGraphFromAsset.generated.h"

USTRUCT(meta = (DisplayName = "Make Graph From Asset"))
struct FAnimNextMakeGraphFromAssetTraitSharedData : public FAnimNextTraitSharedData
{
	GENERATED_BODY()
};

namespace UE::AnimNext
{
	/**
	 * FMakeGraphFromAssetTrait
	 *
	 * A trait that makes an AnimNext animation graph from an asset
	 */
	struct FMakeGraphFromAssetTrait : FAdditiveTrait, IGraphFactory
	{
		DECLARE_ANIM_TRAIT(FMakeGraphFromAssetTrait, FAdditiveTrait)

		using FSharedData = FAnimNextMakeGraphFromAssetTraitSharedData;

		struct FInstanceData : FTrait::FInstanceData
		{
		};

		// IGraphFactory impl
		virtual const UAnimNextAnimationGraph* GetGraphFromObject(FExecutionContext& Context, const TTraitBinding<IGraphFactory>& Binding, const UObject* InObject, FAnimNextDataInterfacePayload& InOutPayload) const;
	};
}
