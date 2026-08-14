// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RigVMCore/RigVMGraphFunctionDefinition.h"
#include "TraitCore/Trait.h"
#include "TraitInterfaces/IUpdate.h"
#include "CallFunction.generated.h"

UENUM()
enum class EAnimNextCallFunctionCallSite : uint8
{
	// Called each time the trait becomes relevant
	BecomeRelevant,

	// Called on update before the trait and any stack super-traits update
	PreUpdate,

	// Called on update after the trait and any stack super-traits update
	PostUpdate,
};

USTRUCT(meta = (DisplayName = "Call Function"))
struct FAnimNextCallFunctionSharedData : public FAnimNextTraitSharedData
{
	GENERATED_BODY()

#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, Category = "Call Function", meta=(Hidden))
	FRigVMGraphFunctionHeader FunctionHeader;
#endif

	/** The function to call */
	UPROPERTY(EditAnywhere, Category = "Call Function", meta=(Hidden))
	FName Function;

	/** Internal event name derived from Function above */
	UPROPERTY(VisibleAnywhere, Category = "Call Function", meta=(Hidden))
	FName FunctionEvent;

	/** The call site to use */
	UPROPERTY(EditAnywhere, Category = "Call Function")
	EAnimNextCallFunctionCallSite CallSite = EAnimNextCallFunctionCallSite::BecomeRelevant;

	// Latent pin support boilerplate
	#define TRAIT_LATENT_PROPERTIES_ENUMERATOR(GeneratorMacro) \
		GeneratorMacro(CallSite) \

	GENERATE_TRAIT_LATENT_PROPERTIES(FAnimNextCallFunctionSharedData, TRAIT_LATENT_PROPERTIES_ENUMERATOR)
	#undef TRAIT_LATENT_PROPERTIES_ENUMERATOR
};

namespace UE::AnimNext
{
/**
 * FCallFunctionTrait
 *
 * A trait that calls a function at the specified update point
 */
struct FCallFunctionTrait : FAdditiveTrait, IUpdate
{
	DECLARE_ANIM_TRAIT(FCallFunctionTrait, FAdditiveTrait)

	using FSharedData = FAnimNextCallFunctionSharedData;

	struct FInstanceData : FTrait::FInstanceData
	{
	};

	// IUpdate impl
	virtual void OnBecomeRelevant(FUpdateTraversalContext& Context, const TTraitBinding<IUpdate>& Binding, const FTraitUpdateState& TraitState) const override;
	virtual void PreUpdate(FUpdateTraversalContext& Context, const TTraitBinding<IUpdate>& Binding, const FTraitUpdateState& TraitState) const override;
	virtual void PostUpdate(FUpdateTraversalContext& Context, const TTraitBinding<IUpdate>& Binding, const FTraitUpdateState& TraitState) const override;

	// Calls the RigVM function that is assigned to us
	void CallFunctionForMatchingSite(const TTraitBinding<IUpdate>& InBinding, const FTraitUpdateState& InTraitState, EAnimNextCallFunctionCallSite InCallSite) const;
};
}
