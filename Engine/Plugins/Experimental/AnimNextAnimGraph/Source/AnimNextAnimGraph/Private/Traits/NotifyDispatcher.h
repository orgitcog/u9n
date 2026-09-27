// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Module/ModuleEvents.h"
#include "TraitCore/Trait.h"
#include "TraitInterfaces/ITimelinePlayer.h"
#include "TraitInterfaces/IUpdate.h"
#include "Animation/AnimNotifyQueue.h"
#include "Module/AnimNextModuleInstanceComponent.h"

#include "NotifyDispatcher.generated.h"

struct FAnimNotifyEventReference;
class USkeletalMeshComponent;

namespace UE::AnimNext
{
	struct FNotifyQueueDispatchEvent;
}

USTRUCT(meta = (DisplayName = "Notify Dispatcher"))
struct FAnimNextNotifyDispatcherTraitSharedData : public FAnimNextTraitSharedData
{
	GENERATED_BODY()

	// Latent pin support boilerplate
	#define TRAIT_LATENT_PROPERTIES_ENUMERATOR(GeneratorMacro)

	GENERATE_TRAIT_LATENT_PROPERTIES(FAnimNextNotifyDispatcherTraitSharedData, TRAIT_LATENT_PROPERTIES_ENUMERATOR)
	#undef TRAIT_LATENT_PROPERTIES_ENUMERATOR
};

USTRUCT()
struct FAnimNextNotifyDispatcherComponent : public FAnimNextModuleInstanceComponent
{
	GENERATED_BODY()

	FAnimNextNotifyDispatcherComponent() = default;

	// FAnimNextModuleInstanceComponent interface
	virtual void OnInitialize() override;
	virtual void OnTraitEvent(FAnimNextTraitEvent& Event) override;
	virtual void OnEndExecution(float InDeltaTime) override;

	// Triggers a single anim notify in the dispatcher
	void TriggerSingleAnimNotify(float InDeltaTime, UE::AnimNext::FNotifyQueueDispatchEvent* InDispatcher, const FAnimNotifyEventReference& EventReference);

	// Notify queue to dispatch
	UPROPERTY()
	FAnimNotifyQueue NotifyQueue;

	TArray<const FAnimNotifyEvent*> ActiveAnimNotifyState;

	UPROPERTY()
	TArray<FAnimNotifyEventReference> ActiveAnimNotifyEventReference;

	// Skeletal mesh component to 'fake' dispatch from
	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> SkeletalMeshComponent;
};

namespace UE::AnimNext
{
	/**
	 * FNotifyDispatcherTrait
	 * 
	 * A trait that dispatches notifies according to a timeline advancing
	 */
	struct FNotifyDispatcherTrait : FAdditiveTrait, ITimelinePlayer
	{
		DECLARE_ANIM_TRAIT(FNotifyDispatcherTrait, FAdditiveTrait)

		using FSharedData = FAnimNextNotifyDispatcherTraitSharedData;

		struct FInstanceData : FTrait::FInstanceData
		{
		};

		// ITimelinePlayer impl
		virtual void AdvanceBy(FExecutionContext& Context, const TTraitBinding<ITimelinePlayer>& Binding, float DeltaTime, bool bDispatchEvents) const override;
	};

	// Event that dispatches to the module
	struct FNotifyDispatchEvent : public FAnimNextTraitEvent
	{
		DECLARE_ANIM_TRAIT_EVENT(FNotifyDispatchEvent, FAnimNextTraitEvent)

		// Notifies to be dispatched
		TArray<FAnimNotifyEventReference> Notifies;

		// Weight at dispatch time
		float Weight = 1.0f;
	};

	// Event that dispatches from the module to gameplay
	struct FNotifyQueueDispatchEvent : public FAnimNextModule_ActionEvent
	{
		DECLARE_ANIM_TRAIT_EVENT(FNotifyQueueDispatchEvent, FAnimNextModule_ActionEvent)

		// FAnimNextModule_ActionEvent interface
		virtual bool IsThreadSafe() const override { return bIsThreadSafe; }
		virtual void Execute() const override;

		// The various events to dispatch
		TArray<FAnimNotifyEventReference> EventsToNotify;
		TArray<FAnimNotifyEventReference> EventsToEnd;
		TArray<FAnimNotifyEventReference> EventsToBegin;
		TArray<FAnimNotifyEventReference> EventsToTick;

		// Skeletal mesh component to dispatch as
		TWeakObjectPtr<USkeletalMeshComponent> WeakSkeletalMeshComponent;

		// Delta time to apply to notifies
		float DeltaSeconds = 0.0f;

		// Whether this queue thread safe to dispatch
		bool bIsThreadSafe = false;
	};
}
