// Copyright Epic Games, Inc. All Rights Reserved.

#include "Iris/ReplicationSystem/Filtering/AlwaysRelevantNetObjectFilter.h"

void UAlwaysRelevantNetObjectFilter::OnInit(const FNetObjectFilterInitParams& Params)
{
}

void UAlwaysRelevantNetObjectFilter::OnDeinit()
{
}

bool UAlwaysRelevantNetObjectFilter::AddObject(uint32 ObjectIndex, FNetObjectFilterAddObjectParams& Params)
{
	return true;
}

void UAlwaysRelevantNetObjectFilter::RemoveObject(uint32 ObjectIndex, const FNetObjectFilteringInfo& Info)
{
}

void UAlwaysRelevantNetObjectFilter::UpdateObjects(FNetObjectFilterUpdateParams& Params)
{
}

void UAlwaysRelevantNetObjectFilter::Filter(FNetObjectFilteringParams& Params)
{
	// Allow everything
	Params.OutAllowedObjects.SetAllBits();
}

void UAlwaysRelevantNetObjectFilter::OnMaxInternalNetRefIndexIncreased(uint32 NewMaxInternalIndex)
{
}

