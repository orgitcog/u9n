// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TraitCore/TraitPtr.h"
#include "TraitInterfaces/IUpdate.h"
#include "Graph/GraphInstanceComponent.h"

namespace UE::AnimNext
{
	namespace Private
	{
		struct FSyncGroupState;
		struct FSyncGroupUniqueName;
	}

	struct FSyncGroupParameters;

	/**
	 * FSyncGroupGraphInstanceComponent
	 *
	 * This component maintains the necessary state to support group based synchronization.
	 */
	struct FSyncGroupGraphInstanceComponent : public FGraphInstanceComponent
	{
		DECLARE_ANIM_GRAPH_INSTANCE_COMPONENT(FSyncGroupGraphInstanceComponent)

		explicit FSyncGroupGraphInstanceComponent(FAnimNextGraphInstance& InOwnerInstance);
		~FSyncGroupGraphInstanceComponent();

		// Registers the specified trait with group based synchronization
		void RegisterWithGroup(const FSyncGroupParameters& GroupParameters, const FWeakTraitPtr& TraitPtr, const FTraitUpdateState& TraitState);

		// Create a unique group name suitable for spawned sub-graphs to self-synchronize
		// Unique group names are a limited resource, when no longed needed they must be released
		FName CreateUniqueGroupName();

		// Releases a unique group name that is no longer needed. It will be recycled the
		// next time one is needed
		void ReleaseUniqueGroupName(FName GroupName);

		// FGraphInstanceComponent impl
		virtual void PreUpdate(FExecutionContext& Context) override;
		virtual void PostUpdate(FExecutionContext& Context) override;

	private:
		// A map of sync group name -> group index
		TMap<FName, int32> SyncGroupMap;

		// A list of groups and their data
		TArray<Private::FSyncGroupState> SyncGroups;

		// The first free unique group name
		Private::FSyncGroupUniqueName* FirstFreeUniqueGroupName = nullptr;

		// The map of currently used group names
		TMap<FName, Private::FSyncGroupUniqueName*> UsedUniqueGroupNames;

		// A counter tracking the next unique group name to allocate
		int32 UniqueGroupNameCounter = 0;
	};
}
