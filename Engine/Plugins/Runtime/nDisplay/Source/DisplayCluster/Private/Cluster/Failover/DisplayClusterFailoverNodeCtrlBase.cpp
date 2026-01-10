// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cluster/Failover/DisplayClusterFailoverNodeCtrlBase.h"


FDisplayClusterFailoverNodeCtrlBase::FDisplayClusterFailoverNodeCtrlBase(TSharedRef<IDisplayClusterClusterNodeController>& InNodeController)
	: NodeController(InNodeController)
{
}
