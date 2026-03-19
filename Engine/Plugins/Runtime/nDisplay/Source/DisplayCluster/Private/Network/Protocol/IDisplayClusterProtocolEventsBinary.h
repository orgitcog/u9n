// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Network/DisplayClusterNetworkTypes.h"

struct FDisplayClusterClusterEventBinary;


/**
 * Binary cluster events protocol.
 */
class IDisplayClusterProtocolEventsBinary
{
public:

	virtual ~IDisplayClusterProtocolEventsBinary() = default;

public:

	/** Emits binary cluster event */
	virtual EDisplayClusterCommResult EmitClusterEventBinary(const FDisplayClusterClusterEventBinary& Event) = 0;
};
