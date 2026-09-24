// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkHubMessageBusSource.h"

#include "Containers/Ticker.h"
#include "Engine/Level.h"
#include "Engine/SystemTimeTimecodeProvider.h"
#include "Engine/World.h"
#include "Features/IModularFeatures.h"
#include "HAL/PlatformProcess.h"
#include "ILiveLinkClient.h"
#include "LiveLinkClient.h"
#include "ILiveLinkModule.h"
#include "LiveLinkHubMessagingModule.h"
#include "LiveLinkHubMessages.h"
#include "LiveLinkMessages.h"
#include "LiveLinkTimecodeProvider.h"
#include "LiveLinkTypes.h"
#include "MessageEndpoint.h"
#include "MessageEndpointBuilder.h"
#include "Misc/App.h"
#include "Misc/EngineVersion.h"

#if WITH_EDITOR
#include "LevelEditor.h"
#endif

#include <limits>

FLiveLinkHubMessageBusSource::FLiveLinkHubMessageBusSource(const FText& InSourceType, const FText& InSourceMachineName, const FMessageAddress& InConnectionAddress, double InMachineTimeOffset, FLiveLinkHubInstanceId InInstanceId)
	: FLiveLinkMessageBusSource(InSourceType, InSourceMachineName, InConnectionAddress, InMachineTimeOffset)
	, InstanceId(InInstanceId)
{
#if WITH_EDITOR
	if (FLevelEditorModule* LevelEditorModule = FModuleManager::GetModulePtr<FLevelEditorModule>("LevelEditor"))
	{
		LevelEditorModule->OnMapChanged().AddRaw(this, &FLiveLinkHubMessageBusSource::OnMapChanged);
	}
#endif
}

FLiveLinkHubMessageBusSource::~FLiveLinkHubMessageBusSource()
{
#if WITH_EDITOR
	if (FLevelEditorModule* LevelEditorModule = FModuleManager::GetModulePtr<FLevelEditorModule>("LevelEditor"))
	{
		LevelEditorModule->OnMapChanged().RemoveAll(this);
	}
#endif
}

double FLiveLinkHubMessageBusSource::GetDeadSourceTimeout() const
{
	// Don't remove livelink hub sources that have hit the heartbeat timeout.
	return std::numeric_limits<double>::max();
}

void FLiveLinkHubMessageBusSource::SendConnectMessage()
{
	UE_LOG(LogLiveLinkHubMessaging, Verbose, TEXT("MessageBusSource (%s): Sending connect message to %s"), *MessageEndpoint->GetAddress().ToString(), *ConnectionAddress.ToString());

	FLiveLinkHubConnectMessage* ConnectMessage = FMessageEndpoint::MakeMessage<FLiveLinkHubConnectMessage>();
	ConnectMessage->ClientInfo = CreateLiveLinkClientInfo();

	TMap<FName, FString> Annotations;
	AddAnnotations(Annotations);

	SendMessage(ConnectMessage, Annotations);
	StartHeartbeatEmitter();
	bIsValid = true;
}

bool FLiveLinkHubMessageBusSource::RequestSourceShutdown()
{
	UE_LOG(LogLiveLinkHubMessaging, Verbose, TEXT("MessageBusSource(%s) : Sending disconnect message to %s"), *MessageEndpoint->GetAddress().ToString(), *ConnectionAddress.ToString());

	// Don't send the disconnect message if this source is shutting down because it's handling a disconnect message.
	if (!bDisconnecting)
	{
		// Otherwise, inform our connected provider that it should remove this source.
		FLiveLinkHubDisconnectMessage Message{ GetSourceType().ToString(), GetSourceMachineName().ToString() };
		SendMessage(FMessageEndpoint::MakeMessage<FLiveLinkHubDisconnectMessage>(MoveTemp(Message)));
	}

	return FLiveLinkMessageBusSource::RequestSourceShutdown();
}

void FLiveLinkHubMessageBusSource::SendClientInfoMessage()
{
	SendMessage(FMessageEndpoint::MakeMessage<FLiveLinkClientInfoMessage>(CreateLiveLinkClientInfo()));
}

void FLiveLinkHubMessageBusSource::OnMapChanged(UWorld* World, EMapChangeType ChangeType)
{
	SendClientInfoMessage();
}

void FLiveLinkHubMessageBusSource::InitializeAndPushStaticData_AnyThread(FName SubjectName, TSubclassOf<ULiveLinkRole> SubjectRole, const FLiveLinkSubjectKey& SubjectKey, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context, UScriptStruct* MessageTypeInfo)
{
	check(MessageTypeInfo->IsChildOf(FLiveLinkBaseStaticData::StaticStruct()));

	FLiveLinkStaticDataStruct DataStruct(MessageTypeInfo);
	DataStruct.InitializeWith(MessageTypeInfo, reinterpret_cast<const FLiveLinkBaseStaticData*>(Context->GetMessage()));

	FLiveLinkClient::FPendingSubjectStatic PendingStaticData;
	PendingStaticData.Role = SubjectRole;
	PendingStaticData.SubjectKey = SubjectKey;
	PendingStaticData.StaticData = MoveTemp(DataStruct);
	PendingStaticData.ExtraMetadata = Context->GetAnnotations();

	static_cast<FLiveLinkClient*>(Client)->PushPendingSubject_AnyThread(MoveTemp(PendingStaticData));
}

void FLiveLinkHubMessageBusSource::InitializeMessageEndpoint(FMessageEndpointBuilder& EndpointBuilder)
{
	FLiveLinkMessageBusSource::InitializeMessageEndpoint(EndpointBuilder);
	EndpointBuilder
		.Handling<FLiveLinkHubTimecodeSettings>(this, &FLiveLinkHubMessageBusSource::HandleTimecodeSettings)
		.Handling<FLiveLinkHubCustomTimeStepSettings>(this, &FLiveLinkHubMessageBusSource::HandleCustomTimeStepSettings)
		.Handling<FLiveLinkHubDisconnectMessage>(this, &FLiveLinkHubMessageBusSource::HandleDisconnectMessage);
}


void FLiveLinkHubMessageBusSource::HandleCustomTimeStepSettings(const FLiveLinkHubCustomTimeStepSettings& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	ExecuteOnGameThread(UE_SOURCE_LOCATION, [Message]() {
		Message.AssignCustomTimeStepToEngine();
	});
}

void FLiveLinkHubMessageBusSource::HandleTimecodeSettings(const FLiveLinkHubTimecodeSettings& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	ExecuteOnGameThread(UE_SOURCE_LOCATION, [Message]() {
		Message.AssignTimecodeSettingsAsProviderToEngine();
	});
}

void FLiveLinkHubMessageBusSource::HandleDisconnectMessage(const FLiveLinkHubDisconnectMessage& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	bDisconnecting = true;

	FGuid SourceId = SourceGuid;
	ExecuteOnGameThread(UE_SOURCE_LOCATION, [SourceId]() {
		ILiveLinkClient* Client = &IModularFeatures::Get().GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);
		Client->RemoveSource(SourceId);
	});
}

FLiveLinkClientInfoMessage FLiveLinkHubMessageBusSource::CreateLiveLinkClientInfo() const
{
	FLiveLinkClientInfoMessage ClientInfo;

	FString CurrentLevelName;
	if (GWorld && GWorld->GetCurrentLevel())
	{
		CurrentLevelName = GWorld->GetName();
	}

	// todo: Distinguish between UE and UEFN.
	ClientInfo.LongName = FString::Printf(TEXT("%s - %s %s"), TEXT("UE"), *FEngineVersion::Current().ToString(EVersionComponent::Patch), FPlatformProcess::ComputerName());
	ClientInfo.Status = ELiveLinkClientStatus::Connected;
	ClientInfo.Hostname = FPlatformProcess::ComputerName();
	ClientInfo.ProjectName = FApp::GetProjectName();
	ClientInfo.CurrentLevel = CurrentLevelName;

	FLiveLinkHubMessagingModule& MessagingModule = FModuleManager::GetModuleChecked<FLiveLinkHubMessagingModule>("LiveLinkHubMessaging");
	ELiveLinkTopologyMode Mode = MessagingModule.GetHostTopologyMode();

	// Only populate this field if this is a Hub.
	if (Mode == ELiveLinkTopologyMode::Hub)
	{
		ClientInfo.LiveLinkInstanceName = InstanceId.ToString();
		ClientInfo.TopologyMode = Mode;
	}

	ClientInfo.LiveLinkVersion = ILiveLinkClient::LIVELINK_VERSION;

	return ClientInfo;
}
