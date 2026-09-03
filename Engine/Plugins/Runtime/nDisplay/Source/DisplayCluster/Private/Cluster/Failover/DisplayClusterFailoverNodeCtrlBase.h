// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Cluster/Failover/IDisplayClusterFailoverNodeController.h"
#include "DisplayClusterConfigurationTypes_Enums.h"

class IDisplayClusterClusterNodeController;
struct FDisplayClusterSessionInfo;


/**
 * Base failover node controller class
 */
class FDisplayClusterFailoverNodeCtrlBase
	: public IDisplayClusterFailoverNodeController
{
public:

	FDisplayClusterFailoverNodeCtrlBase(TSharedRef<IDisplayClusterClusterNodeController>& InNodeController);

protected:

	/** Access to the active node controller */
	const TSharedRef<IDisplayClusterClusterNodeController>& GetNodeController() const
	{
		return NodeController;
	}

private:

	/** Active node controller */
	TSharedRef<IDisplayClusterClusterNodeController> NodeController;
};
