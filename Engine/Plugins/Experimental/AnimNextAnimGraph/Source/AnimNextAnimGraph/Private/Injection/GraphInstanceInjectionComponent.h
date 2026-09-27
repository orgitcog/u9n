// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InjectionInfo.h"
#include "Graph/GraphInstanceComponent.h"

namespace UE::AnimNext
{
	/**
	 * FGraphInstanceInjectionComponent
	 * This component maintains injection info for a graph
	 */
	struct FGraphInstanceInjectionComponent : public FGraphInstanceComponent
	{
		DECLARE_ANIM_GRAPH_INSTANCE_COMPONENT(FGraphInstanceInjectionComponent)

		explicit FGraphInstanceInjectionComponent(FAnimNextGraphInstance& InOwnerInstance);

		const FInjectionInfo& GetInjectionInfo() const
		{
			return InjectionInfo;
		}

	private:
		FInjectionInfo InjectionInfo;
	};
}
