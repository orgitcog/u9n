// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkHubMessagingModule.h"

#include "CoreMinimal.h"
#include "LiveLinkHubConnectionManager.h"
#include "LiveLinkHubMessageBusSourceFactory.h"
#include "LiveLinkMessageBusSourceFactory.h"
#include "MessageEndpoint.h"
#include "MessageEndpointBuilder.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/ScopeLock.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogLiveLinkHubMessaging);

namespace Internal
{
	// Source: https://en.cppreference.com/w/cpp/utility/variant/visit
	template<class... Ts>
	struct TOverloaded : Ts...
	{
		using Ts::operator()...;
	};

	template<class... Ts> TOverloaded(Ts...) -> TOverloaded<Ts...>;
}

void FLiveLinkHubMessagingModule::StartupModule()
{
	const bool bIsLiveLinkHubHost = GConfig->GetBoolOrDefault(
		TEXT("LiveLink"), TEXT("bCreateLiveLinkHubInstance"), false, GEngineIni);

	InstanceInfo.TopologyMode = bIsLiveLinkHubHost ? ELiveLinkTopologyMode::Hub : ELiveLinkTopologyMode::UnrealClient;

#if WITH_LIVELINK_DISCOVERY_MANAGER_THREAD
	
	ConnectionManager = MakePimpl<FLiveLinkHubConnectionManager>(InstanceInfo.TopologyMode,
		FLiveLinkHubConnectionManager::FOnGetTopologyMode::CreateRaw(this, &FLiveLinkHubMessagingModule::GetHostTopologyMode),
		FLiveLinkHubConnectionManager::FOnGetInstanceId::CreateRaw(this, &FLiveLinkHubMessagingModule::GetInstanceId)
	);

	GameThreadEndpoint = FMessageEndpoint::Builder(TEXT("LiveLinkHubDiscoveryEndpoint"))
		.Handling<FLiveLinkHubDiscoveryMessage>(this, &FLiveLinkHubMessagingModule::HandleDiscoveryMessage);

	if (GameThreadEndpoint)
	{
		GameThreadEndpoint->Subscribe<FLiveLinkHubDisconnectMessage>();
		GameThreadEndpoint->Subscribe<FLiveLinkHubDiscoveryMessage>();
	}
#endif

	SourceFilterDelegate = ILiveLinkModule::Get().RegisterMessageBusSourceFilter(FOnLiveLinkShouldDisplaySource::CreateRaw(this, &FLiveLinkHubMessagingModule::OnFilterMessageBusSource));
}

void FLiveLinkHubMessagingModule::ShutdownModule()
{ 
	if (ILiveLinkModule* LiveLinkModule = FModuleManager::Get().GetModulePtr<ILiveLinkModule>("LiveLink"))
	{
		LiveLinkModule->UnregisterMessageBusSourceFilter(SourceFilterDelegate);
	}

#if WITH_LIVELINK_DISCOVERY_MANAGER_THREAD
	ConnectionManager.Reset();
#endif
}

void FLiveLinkHubMessagingModule::SetHostTopologyMode(ELiveLinkTopologyMode InMode)
{
	FScopeLock Lock(&InstanceInfoLock);
	InstanceInfo.TopologyMode = InMode;
}

FLiveLinkHubInstanceId FLiveLinkHubMessagingModule::GetInstanceId() const
{
	FScopeLock Lock(&InstanceInfoLock);
	return InstanceInfo.Id;
}

void FLiveLinkHubMessagingModule::SetInstanceId(const FLiveLinkHubInstanceId& Id)
{
	FScopeLock Lock(&InstanceInfoLock);
	InstanceInfo.Id = Id;
}

ELiveLinkTopologyMode FLiveLinkHubMessagingModule::GetHostTopologyMode() const
{
	FScopeLock Lock(&InstanceInfoLock);
	return InstanceInfo.TopologyMode;
}

bool FLiveLinkHubMessagingModule::OnFilterMessageBusSource(UClass* FactoryClass, TSharedPtr<struct FProviderPollResult, ESPMode::ThreadSafe> PollResult)
{
	// Only display Hub/Spoke sources in "LiveLinkHub" section of the add source dropdown.
	bool bValidTopologyMode = false;
	if (FactoryClass == ULiveLinkHubMessageBusSourceFactory::StaticClass())
	{
		bValidTopologyMode = LiveLinkHubConnectionManager::GetPollResultTopologyMode(PollResult) == ELiveLinkTopologyMode::Hub || LiveLinkHubConnectionManager::GetPollResultTopologyMode(PollResult) == ELiveLinkTopologyMode::Spoke;
	}
	else if (FactoryClass == ULiveLinkMessageBusSourceFactory::StaticClass())
	{
		bValidTopologyMode = LiveLinkHubConnectionManager::GetPollResultTopologyMode(PollResult) != ELiveLinkTopologyMode::Hub;
	}

	return bValidTopologyMode && LiveLinkHubConnectionManager::ShouldAcceptConnectionFrom(InstanceInfo.TopologyMode, PollResult, InstanceInfo.Id);
}

void FLiveLinkHubMessagingModule::HandleDiscoveryMessage(const FLiveLinkHubDiscoveryMessage& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	// Before this annotation was added, LiveLinkHub would automatically be added, so we need to keep the previous behavior if we discovered an older LiveLinkHub instance.
	ELiveLinkHubAutoConnectMode AutoConnectMode = ELiveLinkHubAutoConnectMode::Disabled;

	if (const FString* AutoConnectModeAnnotation = Context->GetAnnotations().Find(FLiveLinkHubMessageAnnotation::AutoConnectModeAnnotation))
	{
		int64 AutoConnectModeValue = StaticEnum<ELiveLinkHubAutoConnectMode>()->GetValueByName(**AutoConnectModeAnnotation);
		if (AutoConnectModeValue != INDEX_NONE)
		{
			AutoConnectMode = (ELiveLinkHubAutoConnectMode)AutoConnectModeValue;
		}
	}

	bool bShouldConnect = UE::LiveLink::Messaging::CanReceiveFrom(InstanceInfo.TopologyMode, Message.Mode)
		&& LiveLinkHubConnectionManager::CanConnectTo(Message.MachineName, *Context, InstanceInfo.Id);
	if (bShouldConnect)
	{
		const double MachineTimeOffset = LiveLinkMessageBusHelper::CalculateProviderMachineOffset(Message.CreationTime, Context);

		TSharedPtr<FLiveLinkHubMessageBusSource> LiveLinkSource = MakeShared<FLiveLinkHubMessageBusSource>(FText::FromString(Message.ProviderName), FText::FromString(Message.MachineName), Context->GetSender(), MachineTimeOffset, InstanceInfo.Id);

		ILiveLinkClient* Client = &IModularFeatures::Get().GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);
		Client->AddSource(LiveLinkSource);
	}
}

FLiveLinkHubInstanceId::FLiveLinkHubInstanceId(FGuid Guid)
{
	Id.Set<FGuid>(Guid);
}

FLiveLinkHubInstanceId::FLiveLinkHubInstanceId(FStringView NamedId)
{
	Id.Emplace<FString>(NamedId);
}

FString FLiveLinkHubInstanceId::ToString() const
{
	auto Overload = Internal::TOverloaded{
		[](FGuid InGuid) -> FString 
		{
			TStringBuilder<20> LiveLinkHubName;
			LiveLinkHubName << TEXT("Live Link Hub") << TEXT(" (") << *InGuid.ToString().Right(4).ToLower() << TEXT(")");
			return LiveLinkHubName.ToString();
		},
		[](const FString& InId) -> FString
		{
			return InId;
		}
	};

	return Visit(Overload, Id);
}


IMPLEMENT_MODULE(FLiveLinkHubMessagingModule, LiveLinkHubMessaging);
