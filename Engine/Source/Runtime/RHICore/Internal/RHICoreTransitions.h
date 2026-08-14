// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RHITransition.h"
#include "RHIContext.h"

namespace UE::RHICore
{

struct FResourceState
{
	FResourceState(IRHIComputeContext& Context, ERHIPipeline InSrcPipelines, ERHIPipeline InDstPipelines, const FRHITransitionInfo& Info)
		: AccessBefore(Info.AccessBefore)
		, AccessAfter(Info.AccessAfter)
		, SrcPipelines(InSrcPipelines)
		, DstPipelines(InDstPipelines)
	{
		if (FRHIViewableResource* ViewableResource = GetViewableResource(Info))
		{
			if (AccessBefore == ERHIAccess::Unknown || AccessBefore == ERHIAccess::Discard)
			{
				SrcPipelines = Context.GetTrackedPipelines(ViewableResource);
				AccessBefore = Context.GetTrackedAccess(ViewableResource);
			}

			if (AccessAfter == ERHIAccess::Unknown)
			{
				AccessAfter = Context.GetTrackedAccess(ViewableResource);
			}

			check(AccessBefore != ERHIAccess::Unknown);
			check(AccessAfter != ERHIAccess::Unknown);
		}
	}

	ERHIAccess AccessBefore;
	ERHIAccess AccessAfter;

	// SrcPipeline may differ from the declared RHI SrcPipeline when CollapseToSinglePipeline is used, in which case this will be ERHIPipeline::All.
	ERHIPipeline SrcPipelines;
	ERHIPipeline DstPipelines;
};

} //! UE::RHICore