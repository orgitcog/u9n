// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Config/LiveLinkHubTemplateTokens.h"
#include "UObject/Object.h"
#include "LiveLinkHubMessages.h"
#include "Misc/FrameRate.h"
#include "Templates/SubclassOf.h"

#include "LiveLinkHubSettings.generated.h"

/**
 * Settings for LiveLinkHub.
 */
UCLASS(config=Engine, defaultconfig)
class LIVELINKHUB_API ULiveLinkHubSettings : public UObject
{
	GENERATED_BODY()

public:
	ULiveLinkHubSettings();
	
	virtual void PostInitProperties() override;
	
	/** Parse templates and set example output fields. */
	void CalculateExampleOutput();

	/** Get the naming tokens for Live Link Hub. */
	TObjectPtr<ULiveLinkHubNamingTokens> GetNamingTokens() const;
	
protected:
	//~ Begin UObject interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	//~ End UObject interface
	
public:
	/** Config to apply when starting LiveLinkHub. */
	UPROPERTY(config, EditAnywhere, Category = "LiveLinkHub")
	FFilePath StartupConfig;

	/** If enabled, discovered clients will be automatically added to the current session. */
	UPROPERTY(config, EditAnywhere, Category="LiveLinkHub", DisplayName = "Auto Connect Mode")
	ELiveLinkHubAutoConnectMode AutoConnectClients = ELiveLinkHubAutoConnectMode::LocalOnly;

	/** The size in megabytes to buffer when streaming a recording. */
	UPROPERTY(config, EditAnywhere, Category="LiveLinkHub", meta = (ClampMin = "1", UIMin = "1"))
	int32 PlaybackFrameBufferSizeMB = 100;

	/** Number of frames to buffer at once. */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category="LiveLinkHub", meta = (ClampMin = "2", UIMin = "2"))
	int32 PlaybackBufferBatchSize = 5;

	/** Maximum number of frame ranges to store in history while scrubbing. Increasing can make scrubbing faster but temporarily use more memory. */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category="LiveLinkHub")
	int32 PlaybackMaxBufferRangeHistory = 25;
	
	/** Which project settings sections to display when opening the settings viewer. */
	UPROPERTY(config)
	TArray<FName> ProjectSettingsToDisplay;

	/** If this is enabled, invalid subjects will be removed after loading a session. */
	UPROPERTY(config, EditAnywhere, Category = "LiveLinkHub")
	bool bRemoveInvalidSubjectsAfterLoadingSession = false;

	/** Whether to show the app's frame rate in the top right corner. */
	UPROPERTY(config, EditAnywhere, Category = "LiveLinkHub")
	bool bShowFrameRate = false;

	/** Whether to show memory usage in the top right corner. */
	UPROPERTY(config, EditAnywhere, Category = "LiveLinkHub")
	bool bShowMemoryUsage = true;

	/** How much RAM (in MB) the program can use before showing a warning. */
	UPROPERTY(config, EditAnywhere, Category = "LiveLinkHub", DisplayName = "Memory Warning Threshold (MB)")
	float ShowMemoryWarningThresholdMB = 8000.0;

	/**
	 * - Experimental - If this is disabled, LiveLinkHub's LiveLink Client will tick outside of the game thread.
	 * This allows processing LiveLink frame snapshots without the risk of being blocked by the game / ui thread.
	 * Note that this should only be relevant for virtual subjects since data is already forwarded to UE outside of the game thread.
	 */
	UPROPERTY(config, EditAnywhere, Category = "LiveLinkHub", meta = (ConfigRestartRequired = true))
	bool bTickOnGameThread = false;

	/** Target framerate for ticking LiveLinkHub. */
	UPROPERTY(config, EditAnywhere, Category="LiveLinkHub", meta = (ConfigRestartRequired = true, ClampMin="15.0"))
	float TargetFrameRate = 60.0f;

	/** Whether to prompt the user to pick a save directory after doing a recording. */
	UPROPERTY(config, EditAnywhere, Category = "LiveLinkHub")
	bool bPromptSaveAsOnRecord = false;

	/** Maximum time in seconds to wait for sources to clean up. Increase this value if you notice that some sources are incorrectly cleaned up when switching a config. */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category="LiveLinkHub", meta = (ClampMin="0.0"))
	float SourceMaxCleanupTime = 0.25f;
	
	/** The filename template to use when creating recordings. */
	UPROPERTY(config, EditAnywhere, Category="Templates")
	FString FilenameTemplate = TEXT("{session}_{slate}_tk{take}");

	/** Example parsed output of the template. */
	UPROPERTY(VisibleAnywhere, Category="Templates", DisplayName="Output")
	FString FilenameOutput;
	
	/** Placeholder for a list of the automatic tokens, set from the customization. */
	UPROPERTY(VisibleAnywhere, Category="Templates")
	FText AutomaticTokens;

private:
	/**
	 * Naming tokens for Live Link, instantiated each load based on the naming tokens class.
	 * This isn't serialized to the config file, and exists here for singleton-like access.
	 */
	UPROPERTY(Instanced, Transient)
	mutable TObjectPtr<ULiveLinkHubNamingTokens> NamingTokens;
};

/**
 * User Settings for LiveLinkHub.
 */
UCLASS(config = EditorPerProjectUserSettings)
class LIVELINKHUB_API ULiveLinkHubUserSettings : public UObject
{
	GENERATED_BODY()

public:
	/** Which directories to scan to discover layouts */
	UPROPERTY(config, EditAnywhere, Category = "LiveLinkHub")
	TArray<FString> LayoutDirectories;

	/** The last directory of the a config that was saved or loaded. */
	UPROPERTY(config)
	FString LastConfigDirectory;
};
