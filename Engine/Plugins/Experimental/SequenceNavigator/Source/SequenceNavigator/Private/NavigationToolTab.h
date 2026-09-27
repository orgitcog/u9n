// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "Widgets/Docking/SDockTab.h"

class FName;
class FTabManager;
class ISequencer;
class IToolkitHost;
class USequencerSettings;

namespace UE::SequenceNavigator
{

class INavigationTool;

/** Manages a Navigation Tool tab, including its visible state settings in USequencerSettings */
class FNavigationToolTab : public TSharedFromThis<FNavigationToolTab>
{
public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnVisibilityChanged, const bool /*bInVisible*/);

	/** @return Returns the auto generated tool tab id using the passed in ISequencer.
	 * Tool tab ids are based on the sequencer instance settings name. */
	static FName StaticToolTabId(const ISequencer& InSequencer);

	FNavigationToolTab(INavigationTool& InOwnerTool);

	void Init();
	void Shutdown();

	/** @return True if the USequencerSettings setting for the Navigation Tool is set to visible */
	bool ShouldShowToolTab() const;

	/** @return True if the Navigation Tool tab is visible */
	bool IsToolTabVisible() const;

	/** Shows or hides the Navigation Tool tab and toggles the state in the saved settings */
	void ShowHideToolTab(const bool bInVisible);

	void ToggleToolTabVisible();

	FOnVisibilityChanged& OnVisibilityChanged() { return VisibilityChangedDelegate; }

protected:
	TSharedPtr<IToolkitHost> GetToolkitHost() const;
	TSharedPtr<FTabManager> GetTabManager() const;
	USequencerSettings* GetSequencerSettings() const;

	void RegisterToolTab();
	void UnregisterToolTab();

	TSharedRef<SDockTab> SpawnToolTab(const FSpawnTabArgs& InArgs);
	void CloseToolTab();

	INavigationTool& OwnerTool;

	FName NavigationToolTabId;

	/** State variable to help the dock tab know when it should save its visibility */
	bool bShuttingDown = false;

	FOnVisibilityChanged VisibilityChangedDelegate;
};

} // namespace UE::SequenceNavigator
