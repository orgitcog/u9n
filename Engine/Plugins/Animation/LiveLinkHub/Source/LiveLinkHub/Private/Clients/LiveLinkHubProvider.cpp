// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkHubProvider.h"

#include "Algo/Transform.h"
#include "Async/Async.h"
#include "Clients/LiveLinkHubClientsModel.h"
#include "Clients/LiveLinkHubUEClientInfo.h"
#include "Containers/ObservableArray.h"
#include "CoreMinimal.h"
#include "Delegates/DelegateCombinations.h"
#include "Editor.h"
#include "HAL/CriticalSection.h"
#include "INetworkMessagingExtension.h"
#include "LiveLinkHubLog.h"
#include "LiveLinkHubMessages.h"
#include "LiveLinkSettings.h"
#include "MessageEndpointBuilder.h"
#include "Misc/ScopeLock.h"
#include "Session/LiveLinkHubSession.h"
#include "Session/LiveLinkHubSessionManager.h"
#include "Settings/LiveLinkHubSettings.h"
#include "TimerManager.h"



#define LOCTEXT_NAMESPACE "LiveLinkHub.LiveLinkHubProvider"

namespace LiveLinkHubProviderUtils
{
	static INetworkMessagingExtension* GetMessagingStatistics()
	{
		IModularFeatures& ModularFeatures = IModularFeatures::Get();

		if (IsInGameThread())
		{
			if (ModularFeatures.IsModularFeatureAvailable(INetworkMessagingExtension::ModularFeatureName))
			{
				return &ModularFeatures.GetModularFeature<INetworkMessagingExtension>(INetworkMessagingExtension::ModularFeatureName);
			}
		}
		else
		{
			IModularFeatures::FScopedLockModularFeatureList ScopedLockModularFeatureList;

			if (ModularFeatures.IsModularFeatureAvailable(INetworkMessagingExtension::ModularFeatureName))
			{
				return &ModularFeatures.GetModularFeature<INetworkMessagingExtension>(INetworkMessagingExtension::ModularFeatureName);
			}
		}

		ensureMsgf(false, TEXT("Feature %s is unavailable"), *INetworkMessagingExtension::ModularFeatureName.ToString());
		return nullptr;
	}

	FString GetIPAddress(const FMessageAddress& ClientAddress)
	{
		FString IPAddress;
		if (INetworkMessagingExtension* Statistics = GetMessagingStatistics())
		{
			const FGuid NodeId = Statistics->GetNodeIdFromAddress(ClientAddress);
			IPAddress = NodeId.IsValid() ? Statistics->GetLatestNetworkStatistics(NodeId).IPv4AsString : FString();

			int32 PortIndex = INDEX_NONE;
			IPAddress.FindChar(TEXT(':'), PortIndex);

			// Cut off the port from the end.
			if (PortIndex != INDEX_NONE)
			{
				IPAddress.LeftInline(PortIndex);
			}
		}
		return IPAddress;
	}
}


FLiveLinkHubProvider::FLiveLinkHubProvider(const TSharedRef<ILiveLinkHubSessionManager>& InSessionManager, const FString& InProviderName)
	: FLiveLinkProvider(InProviderName, false)
	, SessionManager(InSessionManager)
{
	Annotations.Add(FLiveLinkHubMessageAnnotation::ProviderTypeAnnotation, UE::LiveLinkHub::Private::LiveLinkHubProviderType.ToString());

	FMessageEndpointBuilder EndpointBuilder = FMessageEndpoint::Builder(*GetProviderName());
	EndpointBuilder.WithHandler(MakeHandler<FLiveLinkClientInfoMessage>(&FLiveLinkHubProvider::HandleClientInfoMessage));
	EndpointBuilder.WithHandler(MakeHandler<FLiveLinkHubConnectMessage>(&FLiveLinkHubProvider::HandleHubConnectMessage));
	EndpointBuilder.WithHandler(MakeHandler<FLiveLinkHubDisconnectMessage>(&FLiveLinkHubProvider::HandleHubDisconnectMessage));

	CreateMessageEndpoint(EndpointBuilder);

	FCoreDelegates::OnPostEngineInit.AddLambda([this]()
	{
		const double ValidateConnectionsRate = GetDefault<ULiveLinkSettings>()->MessageBusPingRequestFrequency;
        GEditor->GetTimerManager()->SetTimer(ValidateConnectionsTimer, FTimerDelegate::CreateRaw(this, &FLiveLinkHubProvider::ValidateConnections), ValidateConnectionsRate, true);
	});
}

FLiveLinkHubProvider::~FLiveLinkHubProvider()
{
	if (GEditor)
	{
		GEditor->GetTimerManager()->ClearTimer(ValidateConnectionsTimer);
	}
}

bool FLiveLinkHubProvider::ShouldTransmitToSubject_AnyThread(FName SubjectName, FMessageAddress Address) const
{
	auto AdditionalFilter = [SubjectName](const FLiveLinkHubUEClientInfo* ClientInfoPtr)
	{
		return !ClientInfoPtr->DisabledSubjects.Contains(SubjectName);
	};

	return ShouldTransmitToClient_AnyThread(Address, AdditionalFilter);
}

void FLiveLinkHubProvider::UpdateTimecodeSettings(const FLiveLinkHubTimecodeSettings& InSettings, const FLiveLinkHubClientId& ClientId)
{
	SendTimecodeSettings(InSettings, ClientId);
}

void FLiveLinkHubProvider::ResetTimecodeSettings(const FLiveLinkHubClientId& ClientId)
{
	// Sending settings with ELiveLinkHubTimecodeSource::NotDefined will reset the timecode on the client.
	SendTimecodeSettings(FLiveLinkHubTimecodeSettings{}, ClientId);
}

void FLiveLinkHubProvider::UpdateCustomTimeStepSettings(const FLiveLinkHubCustomTimeStepSettings& InSettings, const FLiveLinkHubClientId& ClientId)
{
	SendCustomTimeStepSettings(InSettings, ClientId);
}

void FLiveLinkHubProvider::ResetCustomTimeStepSettings(const FLiveLinkHubClientId& ClientId)
{
	// Setting the bResetCustomTimeStep flag will reset the CustomTimeStep on the client.
	FLiveLinkHubCustomTimeStepSettings ResetCustomTimeStepSettings;
	ResetCustomTimeStepSettings.bResetCustomTimeStep = true;

	SendCustomTimeStepSettings(ResetCustomTimeStepSettings, ClientId);
}

void FLiveLinkHubProvider::DisconnectAll()
{
	UE_LOG(LogLiveLinkHub, Verbose, TEXT("Provider: Sending DisconnectAll message (%s)"), *GetEndpointAddress().ToString());

	FLiveLinkHubDisconnectMessage DisconnectMessage;
	DisconnectMessage.ProviderName = GetProviderName();
	DisconnectMessage.MachineName = GetMachineName();

	// todo: Clear address cache to prevent getting connection closed notifications
	//AddressToIdCache.Reset()

	SendMessage(FMessageEndpoint::MakeMessage<FLiveLinkHubDisconnectMessage>(MoveTemp(DisconnectMessage)), EMessageFlags::Reliable);
	
	TArray<FMessageAddress> AllAddresses;
	GetConnectedAddresses(AllAddresses);
	for (const FMessageAddress& Address : AllAddresses)
	{
		CloseConnection(Address);
	}
}

void FLiveLinkHubProvider::DisconnectClient(const FLiveLinkHubClientId& Client)
{
	FLiveLinkHubDisconnectMessage DisconnectMessage;
	DisconnectMessage.ProviderName = GetProviderName();
	DisconnectMessage.MachineName = GetMachineName();

	TArray<FMessageAddress> AllAddresses;
	GetConnectedAddresses(AllAddresses);

	FMessageAddress* TargetAddress = AllAddresses.FindByPredicate(
		[this, Client](const FMessageAddress& Address)
		{
			return AddressToClientId(Address) == Client;
		});

	// Todo: We may want to remove the client info and tracked address.
	if (TargetAddress)
	{
		UE_LOG(LogLiveLinkHub, Verbose, TEXT("Provider: Sending Disconnect message from %s to %s"), *GetEndpointAddress().ToString(), *TargetAddress->ToString());
		SendMessage(FMessageEndpoint::MakeMessage<FLiveLinkHubDisconnectMessage>(MoveTemp(DisconnectMessage)), *TargetAddress, EMessageFlags::Reliable);
		CloseConnection(*TargetAddress);
	}
}

void FLiveLinkHubProvider::PostUpdateTopologyMode(ELiveLinkTopologyMode TopologyMode)
{
	// Hub -> Spoke: Disconnect from all clients, then connect to discovered hubs
	// Spokes -> Hub: Disconnect from everything all clients, then connect to discovered UE clients

	if (TopologyMode == ELiveLinkTopologyMode::Hub)
	{
		ConnectToAllUEClients();
	}
	else if (TopologyMode == ELiveLinkTopologyMode::Spoke)
	{
		ConnectToAllHubClients();
	}
}

void FLiveLinkHubProvider::SendTimecodeSettings(const FLiveLinkHubTimecodeSettings& InSettings, const FLiveLinkHubClientId& ClientId)
{
	if (ClientId.IsValid())
	{
		TArray<FMessageAddress> AllAddresses;
		GetConnectedAddresses(AllAddresses);

		FMessageAddress* TargetAddress = AllAddresses.FindByPredicate([this, ClientId](const FMessageAddress& Address)
		{
			if (FLiveLinkHubClientId* FoundId = AddressToIdCache.Find(Address))
			{
				return *FoundId == ClientId;
			}

			UE_LOG(LogLiveLinkHub, Warning, TEXT("Could not find client id for address %s."), *Address.ToString());
			return false;

		});

		if (TargetAddress)
		{
			SendMessage(FMessageEndpoint::MakeMessage<FLiveLinkHubTimecodeSettings>(InSettings), *TargetAddress, EMessageFlags::Reliable);
		}
	}
	else
	{
		// Invalid ID means we're broadcasting to all clients.
		SendMessageToEnabledClients(FMessageEndpoint::MakeMessage<FLiveLinkHubTimecodeSettings>(InSettings));
	}
}

void FLiveLinkHubProvider::SendCustomTimeStepSettings(const FLiveLinkHubCustomTimeStepSettings& InSettings, const FLiveLinkHubClientId& ClientId)
{
	if (ClientId.IsValid())
	{
		TArray<FMessageAddress> AllAddresses;
		GetConnectedAddresses(AllAddresses);

		FMessageAddress* TargetAddress = AllAddresses.FindByPredicate([this, ClientId](const FMessageAddress& Address)
			{
				return AddressToClientId(Address) == ClientId;
			});

		if (TargetAddress)
		{
			SendMessage(FMessageEndpoint::MakeMessage<FLiveLinkHubCustomTimeStepSettings>(InSettings), *TargetAddress, EMessageFlags::Reliable);
		}
	}
	else
	{
		// Invalid ID means we're broadcasting to all clients.
		SendMessageToEnabledClients(FMessageEndpoint::MakeMessage<FLiveLinkHubCustomTimeStepSettings>(InSettings), EMessageFlags::Reliable);
	}
}

void FLiveLinkHubProvider::AddRestoredClient(FLiveLinkHubUEClientInfo& RestoredClientInfo)
{
	// If a client was already discovered with the same hostname, update it to match the restored client.
	bool bMatchedExistingConnection = false;
	if (const TSharedPtr<ILiveLinkHubSessionManager> Manager = SessionManager.Pin())
	{
		if (const TSharedPtr<ILiveLinkHubSession> ActiveSession = Manager->GetCurrentSession())
		{
			FReadScopeLock Locker(ClientsMapLock);

			for (auto It = ClientsMap.CreateIterator(); It; ++It)
			{
				FLiveLinkHubUEClientInfo& IteratedClient = It->Value;
				if (IteratedClient.Hostname == RestoredClientInfo.Hostname && !ActiveSession->IsClientInSession(It->Key))
				{
					bMatchedExistingConnection = true;

					// Update Client info from the new connection.
					RestoredClientInfo = It->Value;
					break;
				}
			}
		}
	}

	if (!bMatchedExistingConnection)
	{
		ClientsMap.Add(RestoredClientInfo.Id, RestoredClientInfo);
	}

	OnClientEventDelegate.Broadcast(RestoredClientInfo.Id, EClientEventType::Discovered);
}

TOptional<FLiveLinkHubUEClientInfo> FLiveLinkHubProvider::GetClientInfo(FLiveLinkHubClientId InClient) const
{
	FReadScopeLock Locker(ClientsMapLock);
	TOptional<FLiveLinkHubUEClientInfo> ClientInfo;
	if (const FLiveLinkHubUEClientInfo* ClientInfoPtr = ClientsMap.Find(InClient))
	{
		ClientInfo = *ClientInfoPtr;
	}

	return ClientInfo;
}

void FLiveLinkHubProvider::HandleHubConnectMessage(const FLiveLinkHubConnectMessage& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	UE_LOG(LogLiveLinkHub, Verbose, TEXT("Provider: Received connect message from %s"), *Context->GetSender().ToString());

	const ELiveLinkTopologyMode Mode = FLiveLinkHub::Get()->GetTopologyMode();
	if (!UE::LiveLink::Messaging::CanTransmitTo(Mode, Message.ClientInfo.TopologyMode))
	{
		UE_LOG(LogLiveLinkHub, Verbose, TEXT("Provider: Denying connection from %s since its mode is incompatible with this provider's."), *Context->GetSender().ToString());
		FLiveLinkHubDisconnectMessage DisconnectMessage{ GetProviderName(), GetMachineName() };
		SendMessage(FMessageEndpoint::MakeMessage<FLiveLinkHubDisconnectMessage>(MoveTemp(DisconnectMessage)), Context->GetSender(), EMessageFlags::Reliable);
		CloseConnection(Context->GetSender());
		return;
	}

	FLiveLinkConnectMessage ConnectMessage;
	ConnectMessage.LiveLinkVersion = Message.ClientInfo.LiveLinkVersion;
	FLiveLinkProvider::HandleConnectMessage(ConnectMessage, Context);

	const FMessageAddress ConnectionAddress = Context->GetSender();

	TOptional<FLiveLinkHubClientId> UpdatedClient;
	{
		FWriteScopeLock Locker(ClientsMapLock);

		// First check if there are multiple disconnected entries with the same host as the incoming client.
		uint8 NumClientsForHost = 0;
		for (auto It = ClientsMap.CreateIterator(); It; ++It)
		{
			FLiveLinkHubUEClientInfo& IteratedClient = It->Value;
			if (IteratedClient.Hostname == Message.ClientInfo.Hostname && IteratedClient.Status == ELiveLinkClientStatus::Disconnected)
			{
				NumClientsForHost++;
				if (NumClientsForHost > 1)
				{
					break;
				}
			}
		}

		// Remove old entries if one is found
		for (auto It = ClientsMap.CreateIterator(); It; ++It)
		{
			FLiveLinkHubUEClientInfo& IteratedClient = It->Value;
			// If there are multiple disconnected clients with the same hostname, try finding a client with the same project.
			bool bFindWithMatchingProject = NumClientsForHost > 1;

			// Only replace disconnected clients to support multiple UE instances on the same host.
			if (IteratedClient.Status == ELiveLinkClientStatus::Disconnected && IteratedClient.Hostname == Message.ClientInfo.Hostname)
			{
				if (!bFindWithMatchingProject || IteratedClient.ProjectName == Message.ClientInfo.ProjectName)
				{
					IteratedClient.UpdateFromInfoMessage(Message.ClientInfo);
					IteratedClient.Id = It->Key;
					IteratedClient.Status = ELiveLinkClientStatus::Connected;

					AddressToIdCache.FindOrAdd(ConnectionAddress) = IteratedClient.Id;

					UpdatedClient = IteratedClient.Id;
					break;
				}
            }
        }
	}

	if (UpdatedClient)
	{
		// Just updated the map
		OnClientEventDelegate.Broadcast(*UpdatedClient, EClientEventType::Reestablished);
	}
	else
	{
		// Actually added a new entry in the map.
		FLiveLinkHubUEClientInfo NewClient{Message.ClientInfo};
		NewClient.IPAddress = LiveLinkHubProviderUtils::GetIPAddress(ConnectionAddress);

		const FLiveLinkHubClientId NewClientId = NewClient.Id;
		UpdatedClient = NewClientId; // Set this so we can update the client's timecode provider down below.

		{
			FWriteScopeLock Locker(ClientsMapLock);
			AddressToIdCache.FindOrAdd(ConnectionAddress) = NewClientId;
			ClientsMap.Add(NewClient.Id, MoveTemp(NewClient));
		}

		const bool bSameHost = Message.ClientInfo.Hostname == GetMachineName();
		const ELiveLinkHubAutoConnectMode AutoConnectClients = GetDefault<ULiveLinkHubSettings>()->AutoConnectClients;
		if (AutoConnectClients != ELiveLinkHubAutoConnectMode::Disabled)
		{
			if (AutoConnectClients == ELiveLinkHubAutoConnectMode::All || (AutoConnectClients == ELiveLinkHubAutoConnectMode::LocalOnly && bSameHost))
			{
				AsyncTask(ENamedThreads::GameThread, [WeakSessionManager = SessionManager, NewClientId]()
					{
						if (const TSharedPtr<ILiveLinkHubSessionManager> Manager = WeakSessionManager.Pin())
						{
							if (const TSharedPtr<ILiveLinkHubSession> CurrentSession = Manager->GetCurrentSession())
							{
								CurrentSession->AddClient(NewClientId);
							}
						}
					});
			}
		}
		else
		{
			OnClientEventDelegate.Broadcast(NewClientId, EClientEventType::Discovered);
		}
	}

	// Update the timecode provider when a client establishes connection.
	if (GetDefault<ULiveLinkHubTimeAndSyncSettings>()->bUseLiveLinkHubAsTimecodeSource)
	{
		SendTimecodeSettings(GetDefault<ULiveLinkHubTimeAndSyncSettings>()->TimecodeSettings, *UpdatedClient);
	}

	if (GetDefault<ULiveLinkHubTimeAndSyncSettings>()->bUseLiveLinkHubAsCustomTimeStepSource)
	{
		SendCustomTimeStepSettings(GetDefault<ULiveLinkHubTimeAndSyncSettings>()->CustomTimeStepSettings, *UpdatedClient);
	}
}
	
void FLiveLinkHubProvider::HandleClientInfoMessage(const FLiveLinkClientInfoMessage& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	UE_LOG(LogLiveLinkHub, Verbose, TEXT("Provider: Received ClientInfo message from %s"), *Context->GetSender().ToString());

	FMessageAddress Address = Context->GetSender();

	FLiveLinkHubClientId ClientId = AddressToIdCache.FindRef(Address);
	{
		FWriteScopeLock Locker(ClientsMapLock);
		if (FLiveLinkHubUEClientInfo* ClientInfo = ClientsMap.Find(ClientId))
		{
			ClientInfo->UpdateFromInfoMessage(Message);
		}
	}

	if (ClientId.IsValid())
	{
		if (GetDefault<ULiveLinkHubTimeAndSyncSettings>()->bUseLiveLinkHubAsTimecodeSource)
		{
			SendTimecodeSettings(GetDefault<ULiveLinkHubTimeAndSyncSettings>()->TimecodeSettings, ClientId);
		}

		if (GetDefault<ULiveLinkHubTimeAndSyncSettings>()->bUseLiveLinkHubAsCustomTimeStepSource)
		{
			SendCustomTimeStepSettings(GetDefault<ULiveLinkHubTimeAndSyncSettings>()->CustomTimeStepSettings, ClientId);
		}

		OnClientEventDelegate.Broadcast(ClientId, EClientEventType::Modified);
	}
}

void FLiveLinkHubProvider::HandleHubDisconnectMessage(const FLiveLinkHubDisconnectMessage& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	UE_LOG(LogLiveLinkHub, Verbose, TEXT("Provider: Received disconnect message from %s"), *Context->GetSender().ToString());

	// Received a disconnect message from the livelinkhub source (it was probably deleted), so let's remove this client from the session
	const FMessageAddress Address = Context->GetSender();

	// Disabling it, so reset its timecode and custom time step.
	FLiveLinkHubClientId ClientId = AddressToIdCache.FindRef(Address);
	if (TSharedPtr<ILiveLinkHubSessionManager> Manager = SessionManager.Pin())
	{
		// The session will handle resetting the timecode settings and removing it from the cache.
		Manager->GetCurrentSession()->RemoveClient(ClientId);
	}
}

bool FLiveLinkHubProvider::ShouldTransmitToClient_AnyThread(FMessageAddress Address, TFunctionRef<bool(const FLiveLinkHubUEClientInfo* ClientInfoPtr)> AdditionalFilter) const
{
	if (!Address.IsValid())
	{
		return false;
	}

	FReadScopeLock Locker(ClientsMapLock);

	const FLiveLinkHubClientId ClientId = AddressToIdCache.FindRef(Address);
	if (const FLiveLinkHubUEClientInfo* ClientInfoPtr = ClientsMap.Find(ClientId))
	{
		if (const TSharedPtr<ILiveLinkHubSessionManager> Manager = SessionManager.Pin())
		{
			if (const TSharedPtr<ILiveLinkHubSession> CurrentSession = Manager->GetCurrentSession())
			{
				if (!CurrentSession->IsClientInSession(ClientInfoPtr->Id))
				{
					return false;
				}
			}
		}

		if (!ClientInfoPtr->bEnabled)
		{
			return false;
		}

		return AdditionalFilter(ClientInfoPtr);
	}
	else
	{
		UE_LOG(LogLiveLinkHub, Warning, TEXT("Attempted to transmit data to an invalid client."));
	}

	return true;
}

void FLiveLinkHubProvider::ConnectToAllHubClients()
{
	Publish(FMessageEndpoint::MakeMessage<FLiveLinkHubDiscoveryMessage>(GetProviderName(), ELiveLinkTopologyMode::Spoke, FLiveLinkHub::Get()->GetId()));
}

void FLiveLinkHubProvider::ConnectToAllUEClients()
{
	Publish(FMessageEndpoint::MakeMessage<FLiveLinkHubDiscoveryMessage>(GetProviderName(), ELiveLinkTopologyMode::Hub, FLiveLinkHub::Get()->GetId()));
}

void FLiveLinkHubProvider::OnConnectionsClosed(const TArray<FMessageAddress>& ClosedAddresses)
{
	CloseConnections(ClosedAddresses);

	for (FMessageAddress TrackedAddress : ClosedAddresses)
	{
		FWriteScopeLock Locker(ClientsMapLock);
		if (FLiveLinkHubClientId* ClientId = AddressToIdCache.Find(TrackedAddress))
		{
			// Removing this might have implications for restoring sessions.
			// We could instead remove this when the connection is forcibly closed.
			ClientsMap.Remove(*ClientId);
		}

		AddressToIdCache.Remove(TrackedAddress);
	}
}

void FLiveLinkHubProvider::CloseConnections(const TArray<FMessageAddress>& ClosedAddresses)
{
	// List of OldId -> NewId
	TArray<FLiveLinkHubClientId> Notifications;
	TArray<FMessageAddress> AddressesToRemove;
	{
		FWriteScopeLock Locker(ClientsMapLock);

		for (FMessageAddress TrackedAddress : ClosedAddresses)
		{
			FLiveLinkHubClientId ClientId = AddressToIdCache.FindRef(TrackedAddress);
			if (FLiveLinkHubUEClientInfo* FoundInfo = ClientsMap.Find(ClientId))
			{
				FoundInfo->Status = ELiveLinkClientStatus::Disconnected;
				Notifications.Add(ClientId);
			}
		}
	}

	for (const FLiveLinkHubClientId& Client : Notifications)
	{
		OnClientEventDelegate.Broadcast(Client, EClientEventType::Disconnected);
	}
}

FLiveLinkHubClientId FLiveLinkHubProvider::AddressToClientId(const FMessageAddress& Address) const
{
	FLiveLinkHubClientId ClientId;
	if (const FLiveLinkHubClientId* FoundId = AddressToIdCache.Find(Address))
	{
		ClientId = *FoundId;
	}
	else
	{
		UE_LOG(LogLiveLinkHub, Warning, TEXT("Could not find a client for address %s."), *Address.ToString());
	}
	
	return ClientId;
}

TArray<FLiveLinkHubClientId> FLiveLinkHubProvider::GetSessionClients() const
{
	TArray<FLiveLinkHubClientId> SessionClients;

	if (const TSharedPtr<ILiveLinkHubSessionManager> Manager = SessionManager.Pin())
	{
		if (const TSharedPtr<ILiveLinkHubSession> CurrentSession = Manager->GetCurrentSession())
		{
			SessionClients = CurrentSession->GetSessionClients();
		}
	}

	return SessionClients;
}

TMap<FName, FString> FLiveLinkHubProvider::GetAnnotations() const
{
	TMap<FName, FString> AnnotationsCopy = Annotations;

	// Make a local copy of the annotations map and add the autoconnect annotation since this value might change over time.
	const FString AutoConnectValue = StaticEnum<ELiveLinkHubAutoConnectMode>()->GetNameStringByValue(static_cast<int64>(GetDefault<ULiveLinkHubSettings>()->AutoConnectClients));
	AnnotationsCopy.Add(FLiveLinkHubMessageAnnotation::AutoConnectModeAnnotation, AutoConnectValue);

	// In case this is handled by the discovery manager (which doesn't directly handle LiveLinkHub messages)
	AnnotationsCopy.Add(FLiveLinkHubMessageAnnotation::IdAnnotation, FLiveLinkHub::Get()->GetId().ToString());

	// In case this is handled by the discovery manager (which doesn't directly handle LiveLinkHub messages)
	const ELiveLinkTopologyMode Mode = FLiveLinkHub::Get()->GetTopologyMode();
	FString TopopologyMode = StaticEnum<ELiveLinkTopologyMode>()->GetNameStringByValue(static_cast<int64>(Mode));
	AnnotationsCopy.Add(FLiveLinkMessageAnnotation::TopologyModeAnnotation, MoveTemp(TopopologyMode));

	TMap<FName, FString> BaseAnnotations = FLiveLinkProvider::GetAnnotations();
	BaseAnnotations.Append(AnnotationsCopy);

	return BaseAnnotations;
}

TArray<FLiveLinkHubClientId> FLiveLinkHubProvider::GetDiscoveredClients() const
{
	TArray<FLiveLinkHubClientId> DiscoveredClients;

	if (const TSharedPtr<ILiveLinkHubSessionManager> Manager = SessionManager.Pin())
	{
		if (const TSharedPtr<ILiveLinkHubSession> CurrentSession = Manager->GetCurrentSession())
		{
			FReadScopeLock Locker(ClientsMapLock);
			TArray<FLiveLinkHubClientId> SessionClients = CurrentSession->GetSessionClients();
			for (const TPair<FLiveLinkHubClientId, FLiveLinkHubUEClientInfo>& ClientPair : ClientsMap)
			{
				if (ClientPair.Value.Status != ELiveLinkClientStatus::Disconnected && !SessionClients.Contains(ClientPair.Key))
				{
					DiscoveredClients.Add(ClientPair.Key);
				}
			}
		}
	}

	return DiscoveredClients;
}

FText FLiveLinkHubProvider::GetClientDisplayName(FLiveLinkHubClientId InAddress) const
{
	FReadScopeLock Locker(ClientsMapLock);
	FText DisplayName;

	if (const FLiveLinkHubUEClientInfo* ClientInfoPtr = ClientsMap.Find(InAddress))
	{
		if (ClientInfoPtr->TopologyMode == ELiveLinkTopologyMode::Hub)
		{
			DisplayName = FText::FromString(ClientInfoPtr->LiveLinkInstanceName);
		}
		else
		{
			DisplayName = FText::FromString(FString::Format(TEXT("{0} ({1})"), {*ClientInfoPtr->Hostname, *ClientInfoPtr->CurrentLevel }));
		}
	}
	else
	{
		DisplayName = LOCTEXT("InvalidClientLabel", "Invalid Client");
	}

	return DisplayName;
}

FText FLiveLinkHubProvider::GetClientStatus(FLiveLinkHubClientId Client) const
{
	FReadScopeLock Locker(ClientsMapLock);
	if (const FLiveLinkHubUEClientInfo* ClientInfoPtr = ClientsMap.Find(Client))
	{
		return StaticEnum<ELiveLinkClientStatus>()->GetDisplayNameTextByValue(static_cast<int64>(ClientInfoPtr->Status));
	}
		
	return LOCTEXT("InvalidStatus", "Disconnected");
}

bool FLiveLinkHubProvider::IsClientEnabled(FLiveLinkHubClientId Client) const
{
	FReadScopeLock Locker(ClientsMapLock);
	if (const FLiveLinkHubUEClientInfo* ClientInfoPtr = ClientsMap.Find(Client))
	{
		return ClientInfoPtr->bEnabled;
	}
	return false;
}

bool FLiveLinkHubProvider::IsClientConnected(FLiveLinkHubClientId Client) const
{
	FReadScopeLock Locker(ClientsMapLock);
	if (const FLiveLinkHubUEClientInfo* ClientInfoPtr = ClientsMap.Find(Client))
	{
		return ClientInfoPtr->Status == ELiveLinkClientStatus::Connected;
	}
	return false;
}

void FLiveLinkHubProvider::SetClientEnabled(FLiveLinkHubClientId Client, bool bInEnable)
{
	{
		FWriteScopeLock Locker(ClientsMapLock);
		if (FLiveLinkHubUEClientInfo* ClientInfoPtr = ClientsMap.Find(Client))
		{
			ClientInfoPtr->bEnabled = bInEnable;
		}
	}

	if (const TSharedPtr<ILiveLinkHubSessionManager> Manager = SessionManager.Pin())
	{
		if (GetDefault<ULiveLinkHubTimeAndSyncSettings>()->bUseLiveLinkHubAsTimecodeSource)
		{
			if (bInEnable)
			{
				// Enabling client, send it up to date timecode and custom time step settings.
				SendTimecodeSettings(GetDefault<ULiveLinkHubTimeAndSyncSettings>()->TimecodeSettings, Client);
			}
			else
			{
				// Disabling it, so reset its timecode and custom time step.
				ResetTimecodeSettings(Client);
			}
		}

		if (GetDefault<ULiveLinkHubTimeAndSyncSettings>()->bUseLiveLinkHubAsCustomTimeStepSource)
		{
			if (bInEnable)
			{
				SendCustomTimeStepSettings(GetDefault<ULiveLinkHubTimeAndSyncSettings>()->CustomTimeStepSettings, Client);
			}
			else
			{
				ResetCustomTimeStepSettings(Client);
			}
		}
	}
}

bool FLiveLinkHubProvider::IsSubjectEnabled(FLiveLinkHubClientId Client, FName SubjectName) const
{
	FReadScopeLock Locker(ClientsMapLock);
	if (const FLiveLinkHubUEClientInfo* ClientInfoPtr = ClientsMap.Find(Client))
	{
		return !ClientInfoPtr->DisabledSubjects.Contains(SubjectName);
	}
	return false;
}

void FLiveLinkHubProvider::SetSubjectEnabled(FLiveLinkHubClientId Client, FName SubjectName, bool bInEnable)
{
	FWriteScopeLock Locker(ClientsMapLock);
	if (FLiveLinkHubUEClientInfo* ClientInfoPtr = ClientsMap.Find(Client))
	{
		if (bInEnable)
		{
			ClientInfoPtr->DisabledSubjects.Remove(SubjectName);
		}
		else
		{
			ClientInfoPtr->DisabledSubjects.Add(SubjectName);
		}
	}
}

#undef LOCTEXT_NAMESPACE /*LiveLinkHub.LiveLinkHubProvider*/
