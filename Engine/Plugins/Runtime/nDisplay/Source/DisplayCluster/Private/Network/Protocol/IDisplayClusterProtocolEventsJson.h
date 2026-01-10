// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Network/DisplayClusterNetworkTypes.h"

struct FDisplayClusterClusterEventJson;


/**
 * JSON cluster events protocol
 */
class IDisplayClusterProtocolEventsJson
{
public:

	virtual ~IDisplayClusterProtocolEventsJson() = default;

public:

	/** Emits JSON cluster event */
	virtual EDisplayClusterCommResult EmitClusterEventJson(const FDisplayClusterClusterEventJson& Event) = 0;
};
