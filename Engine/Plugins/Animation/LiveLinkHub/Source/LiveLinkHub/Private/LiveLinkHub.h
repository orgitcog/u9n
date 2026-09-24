// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LiveLinkHubApplication.h"

#include "ILiveLinkHubMessagingModule.h"
#include "LiveLinkTypes.h"
#include "LiveLinkHubMessages.h"
#include "Misc/Optional.h"
#include "Templates/SubclassOf.h"
#include "Templates/UniquePtr.h"
#include "Toolkits/FConsoleCommandExecutor.h"

class FLiveLinkHubClient;
class FLiveLinkHubClientsController;
class FLiveLinkHubPlaybackController;
class FLiveLinkHubRecordingController;
class FLiveLinkHubRecordingListController;
class FLiveLinkHubSubjectController;
class FLiveLinkHubWindowController;
struct FLiveLinkSubjectKey;
class FLiveLinkHubProvider;
class ILiveLinkHubSessionManager;
class SWindow;
class ULiveLinkRole;

DECLARE_TS_MULTICAST_DELEGATE_OneParam(FOnTopologyModeChanged, ELiveLinkTopologyMode /*NewMode*/);

/**
 * Main interface for the live link hub.
 */
class ILiveLinkHub
{
public:
	virtual ~ILiveLinkHub() = default;

	/** Whether the hub is currently playing a recording. */
	virtual bool IsInPlayback() const = 0;
	/** Whether the hub is currently recording livelink data. */
	virtual bool IsRecording() const = 0;
};

/**
 * Implementation of the live link hub.
 * Contains the apps' different components and is responsible for handling communication between them.
 */
class FLiveLinkHub : public ILiveLinkHub, public FLiveLinkHubApplication
{
public:
	/** Get the livelinkhub instance that's held by LiveLinkHubModule. May return a nullptr when the module is shutting down. */ 
	static TSharedPtr<FLiveLinkHub> Get();

	virtual ~FLiveLinkHub() override;

	//~ Begin ILiveLinkHub interface
	virtual bool IsInPlayback() const override;
	virtual bool IsRecording() const override;
	//~ End ILiveLinkHub interface


public:
	/** 
	 * Initialize LiveLinkHub. */
	void Initialize(class FLiveLinkHubTicker& Ticker);
	/** Tick the hub. */
	void Tick();
	const FLiveLinkHubInstanceId& GetId() const { return InstanceId; }
	/** Get the root window that hosts the hub's slate application. */
	TSharedRef<SWindow> GetRootWindow() const;
	/** Get the livelink provider used to rebroadcast livelink data to connected UE clients. */
	TSharedPtr<FLiveLinkHubProvider> GetLiveLinkProvider() const;
	/** Get the controller that manages recording livelink data. */
	TSharedPtr<FLiveLinkHubRecordingController> GetRecordingController() const;
	/** Get the recording list controller, that handles displaying livelink recording assets. */
	TSharedPtr<FLiveLinkHubRecordingListController> GetRecordingListController() const;
	/** Get the controller that manages playing back livelink data. */
	TSharedPtr<FLiveLinkHubPlaybackController> GetPlaybackController() const;
	/** Get the controller that manages clients. */
	TSharedPtr<FLiveLinkHubClientsController> GetClientsController() const;
	/** Get the live link hub command list. */
	TSharedPtr<FUICommandList> GetCommandList() const { return GetToolkitCommands(); }
	/** Get the session manager. */
	TSharedPtr<ILiveLinkHubSessionManager> GetSessionManager() const;
	/** 
	 * Get whether the hub should act as a hub or as a spoke. 
	 * Hubs can receive data from spokes, and transmit it to UE clients.
	 * Spokes can only transmit data to Hubs.
	 */
	ELiveLinkTopologyMode GetTopologyMode() const;
	/** Set the topology mode for this instance. */
	void SetTopologyMode(ELiveLinkTopologyMode Mode);
	/** Toggle the topology mode between Hub and Spoke. */
	void ToggleTopologyMode();
	/** Returns whether the topology mode can change for this instance. */
	bool CanSetTopologyMode() const;
	/** Delegate called when the topology mode changed. */
	FOnTopologyModeChanged& OnTopologyModeChanged() { return TopologyModeChangedDelegate; }

	//~ FAssetEditorToolkit interface
	virtual void MapToolkitCommands() override;
	virtual void OnClose() override;

private:
	/** Complete the initialization after core engine systems have been initialized. */
	void InitializePostEngineInit();

	//~ LiveLink Client delegates
	void OnStaticDataReceived_AnyThread(const FLiveLinkSubjectKey& InSubjectKey, TSubclassOf<ULiveLinkRole> InRole, const FLiveLinkStaticDataStruct& InStaticDataStruct) const;
	void OnFrameDataReceived_AnyThread(const FLiveLinkSubjectKey& InSubjectKey, const FLiveLinkFrameDataStruct& InFrameDataStruct) const;
	void OnSubjectMarkedPendingKill_AnyThread(const FLiveLinkSubjectKey& InSubjectKey) const;
	//~ LiveLink Client delegates

	/** Register settings relevant to the livelink hub. */
	void RegisterLiveLinkHubSettings();
	/** Unregister settings relevant to the livelink hub. */
	void UnregisterLiveLinkHubSettings();

	/** Create a new config. */
	void NewConfig();

	/** Save an existing config to a new file. */
	void SaveConfigAs();

	/** Whether the Save command can be used. */
	bool CanSaveConfig() const;
	
	/** Save the config to the current file. */
	void SaveConfig();

	/** Open an existing config. */
	void OpenConfig();

	/** Open the folder containing logs for the app. */
	void OpenLogsFolder();

	/** Open the help menu. */
	void OpenAboutMenu();

	/** Handles adding application modes that are registered after LLH has finished initialization. */
	void OnModularFeatureRegistered(const FName& Type, class IModularFeature* ModularFeature);

	/** Delegate called when a LiveLinkHubSubjectSettings modifies an outbound name. We need to notify connected clients of this change. */
	void OnSubjectOutboundNameModified(const FLiveLinkSubjectKey& SubjectKey, const FString& PreviousOutboundName, const FString& NewOutboundName) const;
	
private:
	/** Implements the logic to manage the clients tabs. */
	TSharedPtr<FLiveLinkHubClientsController> ClientsController;
	/**  Implements the logic for triggering recording. */
	TSharedPtr<FLiveLinkHubRecordingController> RecordingController;
	/** Implements the logic for displaying the list of recordings. */
	TSharedPtr<FLiveLinkHubRecordingListController> RecordingListController;
	/** Implements the logic for triggering the playback of a livelink recording. */
	TSharedPtr<FLiveLinkHubPlaybackController> PlaybackController;
	/** Implements the controller responsible for displaying and managing subject data. */
	TSharedPtr<FLiveLinkHubSubjectController> SubjectController;
	/** Controller responsible for creating and managing the app's slate windows. */
	TSharedPtr<FLiveLinkHubWindowController> WindowController;
	/** Object responsible for managing sessions.  */
	TSharedPtr<ILiveLinkHubSessionManager> SessionManager;
	/** LiveLinkHub's livelink client. */
	TSharedPtr<FLiveLinkHubClient> LiveLinkHubClient;
	/** LiveLinkProvider used to transfer data to connected UE clients. */
	TSharedPtr<FLiveLinkHubProvider> LiveLinkProvider;
	/** Handles execution of commands. */
	TUniquePtr<FConsoleCommandExecutor> CommandExecutor;
	/** The last opened config path. */
	FString LastConfigPath;
	/** LiveLinkHub instance Id, used to disambiguate different instances. */
	FLiveLinkHubInstanceId InstanceId = FLiveLinkHubInstanceId{ FGuid::NewGuid() };
	/** Override topology mode set through the command line. Cannot be changed at runtime. */
	TOptional<ELiveLinkTopologyMode> OverrideTopologyMode;
	/** Delegate called when the topology mode changed. */
	FOnTopologyModeChanged TopologyModeChangedDelegate;

	friend class FLiveLinkHubModule;
};
