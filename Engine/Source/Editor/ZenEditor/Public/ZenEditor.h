// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Templates/SharedPointer.h"
#include "Templates/UniquePtr.h"
#include "IDerivedDataCacheNotifications.h"

class FSpawnTabArgs;
class SDerivedDataCacheSettingsDialog;
class IDerivedDataCacheNotifications;
class SDockTab;
class SWidget;
class SWindow;

/**
 * The module holding all of the UI related pieces for DerivedData
 */
class ZENEDITOR_API FZenEditor : public IModuleInterface
{
public:

	/**
	 * Called right after the module DLL has been loaded and the module object has been created
	 */
	virtual void StartupModule() override;

	/**
	 * Called before the module is unloaded, right before the module object is destroyed.
	 */
	virtual void ShutdownModule() override;

	bool IsZenEnabled() const;
	TSharedRef<SWidget>	CreateStatusBarWidget();

	void ShowResourceUsageTab();
	void ShowCacheStatisticsTab();
	void ShowZenServerStatusTab();
	void StartZenServer();
	void StopZenServer();
	void RestartZenServer();

private:

	TSharedPtr<SWidget> CreateResourceUsageDialog();
	TSharedPtr<SWidget> CreateCacheStatisticsDialog();
	TSharedPtr<SWidget> CreateZenStoreDialog();

	TSharedRef<SDockTab> CreateResourceUsageTab(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> CreateCacheStatisticsTab(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> CreateZenServerStatusTab(const FSpawnTabArgs& Args);

	TWeakPtr<SDockTab> ResourceUsageTab;
	TWeakPtr<SDockTab> CacheStatisticsTab;
	TWeakPtr<SDockTab> ZenServerStatusTab;

	TSharedPtr<SWindow>	SettingsWindow;
	TSharedPtr<SDerivedDataCacheSettingsDialog> SettingsDialog;
	TUniquePtr<IDerivedDataCacheNotifications>	DerivedDataCacheNotifications;
};


