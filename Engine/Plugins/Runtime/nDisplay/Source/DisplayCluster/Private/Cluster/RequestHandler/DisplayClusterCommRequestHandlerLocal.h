// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Cluster/RequestHandler/DisplayClusterCommRequestHandlerBase.h"
#include "Misc/CoreMiscDefines.h"
#include "Network/DisplayClusterNetworkTypes.h"


/**
 * Local request handler
 * 
 * Handles comm requests that were sent by this node. Also encapsulates
 * interaction with cached synchronization data (failover/recovery).
 */
class FDisplayClusterCommRequestHandlerLocal
	: public FDisplayClusterCommRequestHandlerBase
{
public:

	UE_NONCOPYABLE(FDisplayClusterCommRequestHandlerLocal);

public:

	/** Singleton access */
	static FDisplayClusterCommRequestHandlerLocal& Get();

private:

	FDisplayClusterCommRequestHandlerLocal() = default;
	virtual ~FDisplayClusterCommRequestHandlerLocal() = default;

public:

	//~ Begin IDisplayClusterProtocolClusterSync
	virtual EDisplayClusterCommResult WaitForGameStart() override;
	virtual EDisplayClusterCommResult WaitForFrameStart() override;
	virtual EDisplayClusterCommResult WaitForFrameEnd() override;
	virtual EDisplayClusterCommResult GetTimeData(double& OutDeltaTime, double& OutGameTime, TOptional<FQualifiedFrameTime>& OutFrameTime) override;
	virtual EDisplayClusterCommResult GetObjectsData(const EDisplayClusterSyncGroup InSyncGroup, TMap<FString, FString>& OutObjectsData) override;
	virtual EDisplayClusterCommResult GetEventsData(TArray<TSharedPtr<FDisplayClusterClusterEventJson>>& OutJsonEvents, TArray<TSharedPtr<FDisplayClusterClusterEventBinary>>& OutBinaryEvents) override;
	virtual EDisplayClusterCommResult GetNativeInputData(TMap<FString, FString>& OutNativeInputData) override;
	//~ End IDisplayClusterProtocolClusterSync
};
