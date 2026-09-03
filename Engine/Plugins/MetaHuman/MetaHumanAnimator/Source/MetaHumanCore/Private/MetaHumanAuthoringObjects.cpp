// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanAuthoringObjects.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"



static TArray<FString> PluginNames = { UE_PLUGIN_NAME, UE_PLUGIN_NAME "Authoring"}; // All possible places MetaHuman assets can be

static bool DoesAssetExistInPlugin(const FString& InPluginName, const FString& InSubPath)
{
	return true;
}

bool FMetaHumanAuthoringObjects::ArePresent()
{
	// Check by attempting to find any one of the authoring objects. Checking for any one
	// of them is sufficient to check for all of them - either they will all be present
	// or none of them will be. So, arbitrarily choose the chin tracking model
	for (const FString& PluginName : PluginNames)
	{
		if (DoesAssetExistInPlugin(PluginName, TEXT("GenericTracker/Chin.Chin")))
		{
			return true;
		}
	}

	return false;
}

bool FMetaHumanAuthoringObjects::FindObject(FString& InOutObjectPath)
{
	bool bWasFound = false;
	bool bHasMoved = false;
	return FindObject(InOutObjectPath, bWasFound, bHasMoved);
}

bool FMetaHumanAuthoringObjects::FindObject(FString& InOutObjectPath, bool& bOutWasFound, bool& bOutHasMoved)
{
	// File name should be a "mounted" name that starts with a plugin name, ie /MetaHuman/Whatever
	if (!InOutObjectPath.StartsWith(TEXT("/")))
	{
		UE_LOG(LogCore, Warning, TEXT("Object path not in expected format: %s"), *InOutObjectPath);
		bOutWasFound = false;
		bOutHasMoved = false;
		return false;
	}

	// Separate off into plugin name and rest of object path
	int32 Index = InOutObjectPath.Find(TEXT("/"), ESearchCase::IgnoreCase, ESearchDir::FromStart, 1);
	if (Index == -1)
	{
		UE_LOG(LogCore, Warning, TEXT("Object path not in expected format: %s"), *InOutObjectPath);
		bOutWasFound = false;
		bOutHasMoved = false;
		return false;
	}

	FString ObjectPluginName = InOutObjectPath.Mid(1, Index - 1);
	FString ObjectSubPath = InOutObjectPath.Mid(Index + 1);

	// Check if object exists in specified plugin
	if (DoesAssetExistInPlugin(ObjectPluginName, ObjectSubPath))
	{
		bOutWasFound = true;
		bOutHasMoved = false;
		return true;
	}

	// Check if object exists in any of the possible plugins
	for (const FString& PluginName : PluginNames)
	{
		if (PluginName != ObjectPluginName)
		{
			if (DoesAssetExistInPlugin(PluginName, ObjectSubPath))
			{
				InOutObjectPath = TEXT("/") + PluginName + TEXT("/") + ObjectSubPath; // Update path with correct plugin name
				bOutWasFound = true;
				bOutHasMoved = true;
				return true;
			}
		}
	}

	bOutWasFound = false;
	bOutHasMoved = false;
	return true;
}
