// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Iris/ReplicationSystem/Filtering/NetObjectFilter.h"
#include "AlwaysRelevantNetObjectFilter.generated.h"

UCLASS(transient, MinimalAPI)
class UAlwaysRelevantNetObjectFilterConfig final : public UNetObjectFilterConfig
{
	GENERATED_BODY()
};

UCLASS()
class UAlwaysRelevantNetObjectFilter final : public UNetObjectFilter
{
	GENERATED_BODY()

protected:
	// UNetObjectFilter interface
	IRISCORE_API virtual void OnInit(const FNetObjectFilterInitParams&) override;
	IRISCORE_API virtual void OnDeinit() override;
	IRISCORE_API virtual bool AddObject(uint32 ObjectIndex, FNetObjectFilterAddObjectParams&) override;
	IRISCORE_API virtual void RemoveObject(uint32 ObjectIndex, const FNetObjectFilteringInfo&) override;
	IRISCORE_API virtual void UpdateObjects(FNetObjectFilterUpdateParams&) override;
	IRISCORE_API virtual void Filter(FNetObjectFilteringParams&) override;
	IRISCORE_API virtual void OnMaxInternalNetRefIndexIncreased(uint32 NewMaxInternalIndex) override;
};
