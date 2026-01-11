// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Render/Projection/IDisplayClusterProjectionPolicyFactory.h"

/**
 * Implements projection policy factory for the 'Reference' policy
 */
class FDisplayClusterProjectionReferencePolicyFactory
	: public IDisplayClusterProjectionPolicyFactory
{
public:
	//~Begin IDisplayClusterProjectionPolicyFactory
	virtual TSharedPtr<IDisplayClusterProjectionPolicy, ESPMode::ThreadSafe> Create(
		const FString& ProjectionPolicyId,
		const FDisplayClusterConfigurationProjection* InConfigurationProjectionPolicy) override;
	//~~End IDisplayClusterProjectionPolicyFactory
};
