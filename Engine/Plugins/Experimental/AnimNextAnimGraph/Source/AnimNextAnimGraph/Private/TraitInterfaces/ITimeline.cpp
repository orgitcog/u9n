// Copyright Epic Games, Inc. All Rights Reserved.

#include "TraitInterfaces/ITimeline.h"

#include "TraitCore/ExecutionContext.h"

namespace UE::AnimNext
{
	AUTO_REGISTER_ANIM_TRAIT_INTERFACE(ITimeline)

#if WITH_EDITOR
	const FText& ITimeline::GetDisplayName() const
	{
		static FText InterfaceName = NSLOCTEXT("TraitInterfaces", "TraitInterface_ITimeline_Name", "Timeline");
		return InterfaceName;
	}
	const FText& ITimeline::GetDisplayShortName() const
	{
		static FText InterfaceShortName = NSLOCTEXT("TraitInterfaces", "TraitInterface_ITimeline_ShortName", "TIM");
		return InterfaceShortName;
	}
#endif // WITH_EDITOR

	void ITimeline::GetSyncMarkers(const FExecutionContext& Context, const TTraitBinding<ITimeline>& Binding, FTimelineSyncMarkerArray& OutSyncMarkers) const
	{
		TTraitBinding<ITimeline> SuperBinding;
		if (Binding.GetStackInterfaceSuper(SuperBinding))
		{
			return SuperBinding.GetSyncMarkers(Context, OutSyncMarkers);
		}
	}

	FTimelineState ITimeline::GetState(const FExecutionContext& Context, const TTraitBinding<ITimeline>& Binding) const
	{
		TTraitBinding<ITimeline> SuperBinding;
		if (Binding.GetStackInterfaceSuper(SuperBinding))
		{
			return SuperBinding.GetState(Context);
		}

		return FTimelineState();
	}

	FTimelineDelta ITimeline::GetDelta(const FExecutionContext& Context, const TTraitBinding<ITimeline>& Binding) const
	{
		TTraitBinding<ITimeline> SuperBinding;
		if (Binding.GetStackInterfaceSuper(SuperBinding))
		{
			return SuperBinding.GetDelta(Context);
		}

		return FTimelineDelta();
	}
}
