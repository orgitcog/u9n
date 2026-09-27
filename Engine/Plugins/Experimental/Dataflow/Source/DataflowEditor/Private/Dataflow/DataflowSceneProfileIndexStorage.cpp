// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataflowSceneProfileIndexStorage.h"
#include "Dataflow/DataflowConstructionScene.h"
#include "Dataflow/DataflowSimulationScene.h"
#include "Dataflow/DataflowEditorOptions.h"
#include "AssetViewerSettings.h"


namespace UE::Dataflow::Private
{
	int32 GetProfileIndex(const FString& SearchName)
	{
		if (UAssetViewerSettings* AssetViewerSettings = UAssetViewerSettings::Get())
		{
			const int32 FoundIndex = AssetViewerSettings->Profiles.IndexOfByPredicate([SearchName](const FPreviewSceneProfile& Profile)
			{
				return Profile.ProfileName == SearchName;
			});

			return FoundIndex;
		}

		return INDEX_NONE;
	}
}

FDataflowConstructionSceneProfileIndexStorage::FDataflowConstructionSceneProfileIndexStorage(FDataflowConstructionScene* ConstructionScene) :
	ConstructionScene(ConstructionScene)
{
	checkf(ConstructionScene, TEXT("Received invalid construction scene pointer"));
}

void FDataflowConstructionSceneProfileIndexStorage::StoreProfileIndex(int32 Index)
{
	if (UAssetViewerSettings* AssetViewerSettings = UAssetViewerSettings::Get())
	{
		if (AssetViewerSettings->Profiles.IsValidIndex(Index))
		{
			if (UDataflowEditorOptions* const Options = UDataflowEditorOptions::StaticClass()->GetDefaultObject<UDataflowEditorOptions>())
			{
				Options->ConstructionProfileName = AssetViewerSettings->Profiles[Index].ProfileName;
				Options->SaveConfig();
			}

			ConstructionScene->SetCurrentProfileIndex(Index);
		}
	}
}

int32 FDataflowConstructionSceneProfileIndexStorage::RetrieveProfileIndex()
{
	if (const UDataflowEditorOptions* const Options = UDataflowEditorOptions::StaticClass()->GetDefaultObject<UDataflowEditorOptions>())
	{
		const int32 FoundIndex = UE::Dataflow::Private::GetProfileIndex(Options->ConstructionProfileName);
		if (FoundIndex != ConstructionScene->GetCurrentProfileIndex() && (FoundIndex != INDEX_NONE))
		{
			ConstructionScene->SetCurrentProfileIndex(FoundIndex);
		}
		return FoundIndex;
	}
	return INDEX_NONE;
}


FDataflowSimulationSceneProfileIndexStorage::FDataflowSimulationSceneProfileIndexStorage(FDataflowSimulationScene* SimulationScene) :
	SimulationScene(SimulationScene)
{
	checkf(SimulationScene, TEXT("Received invalid simulation scene pointer"));
}

void FDataflowSimulationSceneProfileIndexStorage::StoreProfileIndex(int32 Index)
{
	if (UAssetViewerSettings* AssetViewerSettings = UAssetViewerSettings::Get())
	{
		if (AssetViewerSettings->Profiles.IsValidIndex(Index))
		{
			if (UDataflowEditorOptions* const Options = UDataflowEditorOptions::StaticClass()->GetDefaultObject<UDataflowEditorOptions>())
			{
				Options->SimulationProfileName = AssetViewerSettings->Profiles[Index].ProfileName;
				Options->SaveConfig();
			}

			SimulationScene->SetCurrentProfileIndex(Index);
		}
	}
}

int32 FDataflowSimulationSceneProfileIndexStorage::RetrieveProfileIndex()
{
	if (const UDataflowEditorOptions* const Options = UDataflowEditorOptions::StaticClass()->GetDefaultObject<UDataflowEditorOptions>())
	{
		const int32 FoundIndex = UE::Dataflow::Private::GetProfileIndex(Options->SimulationProfileName);
		if (FoundIndex != SimulationScene->GetCurrentProfileIndex() && (FoundIndex != INDEX_NONE))
		{
			SimulationScene->SetCurrentProfileIndex(FoundIndex);
		}
		return FoundIndex;
	}

	return INDEX_NONE;
}
