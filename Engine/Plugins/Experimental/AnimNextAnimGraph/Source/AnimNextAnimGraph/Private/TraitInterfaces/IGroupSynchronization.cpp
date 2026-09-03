// Copyright Epic Games, Inc. All Rights Reserved.

#include "TraitInterfaces/IGroupSynchronization.h"

#include "TraitCore/ExecutionContext.h"

namespace UE::AnimNext
{
	AUTO_REGISTER_ANIM_TRAIT_INTERFACE(IGroupSynchronization)

#if WITH_EDITOR
	const FText& IGroupSynchronization::GetDisplayName() const
	{
		static FText InterfaceName = NSLOCTEXT("TraitInterfaces", "TraitInterface_IGroupSynchronization_Name", "Group Synchronization");
		return InterfaceName;
	}
	const FText& IGroupSynchronization::GetDisplayShortName() const
	{
		static FText InterfaceShortName = NSLOCTEXT("TraitInterfaces", "TraitInterface_IGroupSynchronization_ShortName", "GRS");
		return InterfaceShortName;
	}
#endif // WITH_EDITOR

	FSyncGroupParameters IGroupSynchronization::GetGroupParameters(FExecutionContext& Context, const TTraitBinding<IGroupSynchronization>& Binding) const
	{
		TTraitBinding<IGroupSynchronization> SuperBinding;
		if (Binding.GetStackInterfaceSuper(SuperBinding))
		{
			return SuperBinding.GetGroupParameters(Context);
		}

		return FSyncGroupParameters();
	}

	void IGroupSynchronization::AdvanceBy(FExecutionContext& Context, const TTraitBinding<IGroupSynchronization>& Binding, float DeltaTime, bool bDispatchEvents) const
	{
		TTraitBinding<IGroupSynchronization> SuperBinding;
		if (Binding.GetStackInterfaceSuper(SuperBinding))
		{
			SuperBinding.AdvanceBy(Context, DeltaTime, bDispatchEvents);
		}
	}
}
