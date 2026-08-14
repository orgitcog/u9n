// Copyright Epic Games, Inc. All Rights Reserved.
#pragma  once

#include "Engine/DeveloperSettings.h"

#include "DirectoryPlaceholderSettings.generated.h"


/**
 * Directory Placeholder Settings
 */
UCLASS(config = EditorPerProjectUserSettings)
class UDirectoryPlaceholderSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UDirectoryPlaceholderSettings() = default;

	/** If enabled, Directory Placeholder assets will be automatically created in new folders, and automatically deleted from folders being deleted */
	UPROPERTY(EditAnywhere, Category = "Default")
	bool bAutomaticallyCreatePlaceholders = true;
};
