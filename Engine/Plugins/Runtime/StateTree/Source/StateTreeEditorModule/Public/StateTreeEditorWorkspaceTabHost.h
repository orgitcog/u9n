// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Textures/SlateIcon.h"
#include "Framework/Docking/TabManager.h"

class SDockTab;

namespace UE::StateTreeEditor
{
	struct FMinorWorkspaceTabConfig
	{
		FName ID;
		FText Label;
		FText Tooltip;
		FSlateIcon Icon;
		FName UISystemID;
	};

	struct FSpawnedWorkspaceTab
	{
		FName TabID;
		TWeakPtr<SDockTab> DockTab;
	};

	// Interface required for re-using the same tab management across different AssetEditors
	class STATETREEEDITORMODULE_API FWorkspaceTabHost : public TSharedFromThis<FWorkspaceTabHost>
	{
	public:
		static const FLazyName BindingTabId;
		static const FLazyName DebuggerTabId;
		static const FLazyName OutlinerTabId;
		static const FLazyName SearchTabId;
		static const FLazyName StatisticsTabId;

	public:
		virtual ~FWorkspaceTabHost() = default;

		FOnSpawnTab CreateSpawnDelegate(FName TabID);
		TConstArrayView<FMinorWorkspaceTabConfig> GetTabConfigs() const;

		TConstArrayView<FSpawnedWorkspaceTab> GetSpawnedTabs() const
		{
			return SpawnedTabs;
		}

		TSharedRef<SDockTab> Spawn(FName TabID);

		DECLARE_MULTICAST_DELEGATE_OneParam(FOnWorkspaceSpawnTab, FSpawnedWorkspaceTab);
		FOnWorkspaceSpawnTab OnTabSpawned;
		FOnWorkspaceSpawnTab OnTabClosed;

	private:
		TSharedRef<SDockTab> HandleSpawnDelegate(const FSpawnTabArgs& Args, FName TabID);
		void HandleTabClosed(TSharedRef<SDockTab>);
		TArray<FSpawnedWorkspaceTab> SpawnedTabs;
	};
}
