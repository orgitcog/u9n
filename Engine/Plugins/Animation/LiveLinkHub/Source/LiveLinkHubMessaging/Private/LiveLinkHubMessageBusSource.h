// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ILiveLinkHubMessagingModule.h"
#include "LiveLinkHubMessages.h"
#include "LiveLinkHubMessageBusSourceSettings.h"
#include "LiveLinkMessageBusSource.h"

class UWorld;
enum class EMapChangeType : uint8;
struct FLiveLinkClientInfoMessage;
struct FMessageAddress;
struct FLiveLinkHubTimecodeSettings;

/** LiveLink Message bus source that is connected to a livelink hub. */
class FLiveLinkHubMessageBusSource : public FLiveLinkMessageBusSource
{
public:
	FLiveLinkHubMessageBusSource(const FText& InSourceType, const FText& InSourceMachineName, const FMessageAddress& InConnectionAddress, double InMachineTimeOffset, FLiveLinkHubInstanceId InInstanceId);
	virtual ~FLiveLinkHubMessageBusSource();

protected:
	//~ Begin FLiveLinkMessageBusSource interface
	virtual void InitializeAndPushStaticData_AnyThread(FName SubjectName, TSubclassOf<ULiveLinkRole> SubjectRole, const FLiveLinkSubjectKey& SubjectKey, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context, UScriptStruct* MessageTypeInfo) override;
	virtual void InitializeMessageEndpoint(FMessageEndpointBuilder& EndpointBuilder);
	virtual double GetDeadSourceTimeout() const override;
	virtual void SendConnectMessage() override;
	virtual TSubclassOf<ULiveLinkSourceSettings> GetSettingsClass() const override { return ULiveLinkHubMessageBusSourceSettings::StaticClass(); }
	virtual bool RequestSourceShutdown() override;
	//~ End FLiveLinkMessageBusSource

private:
	/** Handle a custom time step settings message and update the engine's custom time step settings accordingly. */
	void HandleCustomTimeStepSettings(const FLiveLinkHubCustomTimeStepSettings& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context);
	/** Handle a timecode settings message and update the engine's timecode settings accordingly. */
	void HandleTimecodeSettings(const FLiveLinkHubTimecodeSettings& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context);
	/** Handle a disconnect request from the provider. */
	void HandleDisconnectMessage(const FLiveLinkHubDisconnectMessage& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context);
	/** Send information about this UE client to the connected provider */
	void SendClientInfoMessage();
	/** Handler called on map changed to update the livelink hub. */
	void OnMapChanged(UWorld* World, EMapChangeType ChangeType);
	/** Gather information about this client to put in a client info struct. */
	FLiveLinkClientInfoMessage CreateLiveLinkClientInfo() const;
	/** ID of this instance if running inside LiveLinkHub. */
	const FLiveLinkHubInstanceId InstanceId;
	/** If this source is in the process of disconnecting. */
	bool bDisconnecting = false;
};
