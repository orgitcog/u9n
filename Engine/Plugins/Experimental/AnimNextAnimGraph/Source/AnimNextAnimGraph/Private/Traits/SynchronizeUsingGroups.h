// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TraitCore/Trait.h"
#include "TraitInterfaces/IGroupSynchronization.h"
#include "TraitInterfaces/IGraphFactory.h"
#include "TraitInterfaces/IUpdate.h"
#include "TraitInterfaces/ITimelinePlayer.h"

#include "SynchronizeUsingGroups.generated.h"

USTRUCT(meta = (DisplayName = "Synchronize Using Groups"))
struct FAnimNextSynchronizeUsingGroupsTraitSharedData : public FAnimNextTraitSharedData
{
	GENERATED_BODY()

	// The group name
	// If no name is provided, this trait is inactive
	UPROPERTY(EditAnywhere, Category = "Default")
	FName GroupName;

	// The role this player can assume within the group
	UPROPERTY(EditAnywhere, Category = "Default")
	EAnimGroupSynchronizationRole GroupRole = EAnimGroupSynchronizationRole::CanBeLeader;

	// The synchronization mode
	UPROPERTY(EditAnywhere, Category = "Default")
	EAnimGroupSynchronizationMode SyncMode = EAnimGroupSynchronizationMode::NoSynchronization;

	// Whether or not to match the group sync point when joining as leader or follower with markers
	// When disabled, the start position of synced sequences must be properly set to avoid pops
	UPROPERTY(EditAnywhere, Category = "Default")
	bool bMatchSyncPoint = true;

	// Latent pin support boilerplate
	#define TRAIT_LATENT_PROPERTIES_ENUMERATOR(GeneratorMacro) \
		GeneratorMacro(GroupName) \
		GeneratorMacro(GroupRole) \
		GeneratorMacro(SyncMode) \
		GeneratorMacro(bMatchSyncPoint) \

	GENERATE_TRAIT_LATENT_PROPERTIES(FAnimNextSynchronizeUsingGroupsTraitSharedData, TRAIT_LATENT_PROPERTIES_ENUMERATOR)
	#undef TRAIT_LATENT_PROPERTIES_ENUMERATOR
};

namespace UE::AnimNext
{
	struct FSyncGroupGraphInstanceComponent;

	/**
	 * FSynchronizeUsingGroupsTrait
	 * 
	 * A trait that synchronizes animation sequence playback using named groups.
	 */
	struct FSynchronizeUsingGroupsTrait : FAdditiveTrait, IUpdate, IGroupSynchronization, ITimelinePlayer, IGraphFactory
	{
		DECLARE_ANIM_TRAIT(FSynchronizeUsingGroupsTrait, FAdditiveTrait)

		using FSharedData = FAnimNextSynchronizeUsingGroupsTraitSharedData;

		struct FInstanceData : FTraitInstanceData
		{
			void Construct(const FExecutionContext& Context, const FTraitBinding& Binding);
			void Destruct(const FExecutionContext& Context, const FTraitBinding& Binding);

			// Cached pointer to our sync group component
			FSyncGroupGraphInstanceComponent* SyncGroupComponent = nullptr;

			// If our sync mode requires a unique group name, we'll cache it and re-use it as it is unique to our instance
			FName UniqueGroupName;

			bool bFreezeTimeline = false;
			bool bHasReachedFullWeight = false;
			bool bHasTimelinePlayer = false;
		};

#if WITH_EDITOR
		// A trait stack has a single timeline, we can't support multiple instances
		virtual bool MultipleInstanceSupport() const override { return false; }
#endif

		// IUpdate impl
		virtual void OnBecomeRelevant(FUpdateTraversalContext& Context, const TTraitBinding<IUpdate>& Binding, const FTraitUpdateState& TraitState) const override;
		virtual void PreUpdate(FUpdateTraversalContext& Context, const TTraitBinding<IUpdate>& Binding, const FTraitUpdateState& TraitState) const override;

		// IGroupSynchronization impl
		virtual FSyncGroupParameters GetGroupParameters(FExecutionContext& Context, const TTraitBinding<IGroupSynchronization>& Binding) const override;
		virtual void AdvanceBy(FExecutionContext& Context, const TTraitBinding<IGroupSynchronization>& Binding, float DeltaTime, bool bDispatchEvents) const override;

		// ITimelinePlayer impl
		virtual void AdvanceBy(FExecutionContext& Context, const TTraitBinding<ITimelinePlayer>& Binding, float DeltaTime, bool bDispatchEvents) const override;

		// IGraphFactory impl
		virtual void CreatePayloadForObject(FExecutionContext& Context, const TTraitBinding<IGraphFactory>& Binding, const UObject* InObject, FAnimNextDataInterfacePayload& InOutPayload) const override;
	};
}
