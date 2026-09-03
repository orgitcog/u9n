// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/AnimNextAnimGraph.h"
#include "Graph/AnimNextGraphInstance.h"

bool FAnimNextAnimGraph::IsEqualForInjectionSiteChange(const FAnimNextAnimGraph& InOther) const
{
	return Asset == InOther.Asset &&
			HostGraph == InOther.HostGraph;
}
