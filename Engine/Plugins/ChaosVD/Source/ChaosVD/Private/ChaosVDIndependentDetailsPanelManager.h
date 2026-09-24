// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ChaosVDTabsIDs.h"
#include "Templates/SharedPointer.h"

class SDockTab;
class FChaosVDStandAloneObjectDetailsTab;
class SChaosVDMainTab;

/**
 * Manager class that handles any selection independent details panel tab.
 */
class FChaosVDIndependentDetailsPanelManager : public TSharedFromThis<FChaosVDIndependentDetailsPanelManager>
{
public:
	FChaosVDIndependentDetailsPanelManager(const TSharedRef<SChaosVDMainTab>& InMainTab);

	/** Returns a shared ptr to a new selection independent details panel tab or null if no panels are available */
	TSharedPtr<FChaosVDStandAloneObjectDetailsTab> GetAvailableStandAloneDetailsPanelTab();

private:

	void HandleTabDestroyed(TSharedRef<SDockTab> Tab, FName TabID);

	TArray<FName> AvailableDetailsPanelIDs;
	
	TWeakPtr<SChaosVDMainTab> MainTab;
};
