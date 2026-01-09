// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkMessageBusSource.h"

#include "ClientNetworkStatisticsModel.h"
#include "ILiveLinkClient.h"
#include "ILiveLinkModule.h"
#include "INetworkMessagingExtension.h"
#include "LiveLinkCompression.h"
#include "LiveLinkHeartbeatEmitter.h"
#include "LiveLinkLog.h"
#if WITH_LIVELINK_DISCOVERY_MANAGER_THREAD
#include "LiveLinkMessageBusDiscoveryManager.h"
#endif
#include "LiveLinkMessageBusSourceSettings.h"
#include "LiveLinkMessages.h"
#include "LiveLinkRole.h"
#include "LiveLinkRoleTrait.h"
#include "LiveLinkSettings.h"

#include "MessageEndpointBuilder.h"
#include "Misc/App.h"
#include "Roles/LiveLinkAnimationTypes.h"


int32 GLiveLinkMessageBusSourceReconnectAfterTimeout = 1;
static FAutoConsoleVariableRef CVarLiveLinkMessageBusSourceReconnectAfterTimeout(
	TEXT("LiveLink.MessageBus.Source.ReconnectAfterTimeout"),
	GLiveLinkMessageBusSourceReconnectAfterTimeout,
	TEXT("When enabled, when the connection times out, it will try to re-connect instead of removing the source."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarLiveLinkSupportCompressPayloads(
	TEXT("LiveLink.SupportCompressPayloads"), 1,
	TEXT("Whether to add the annotation indicating that we support compressed animation data. Can be set to 0 to simulate that compressed payloads are not supported."),
	ECVF_RenderThreadSafe);


FText FLiveLinkMessageBusSource::ValidSourceStatus()
{
	return NSLOCTEXT("LiveLinkMessageBusSource", "ActiveStatus", "Active");
}

FText FLiveLinkMessageBusSource::InvalidSourceStatus()
{
	return NSLOCTEXT("LiveLinkMessageBusSource", "InvalidConnection", "Waiting for connection");
}

FText FLiveLinkMessageBusSource::TimeoutSourceStatus()
{
	return NSLOCTEXT("LiveLinkMessageBusSource", "TimeoutStatus", "Not responding");
}

FLiveLinkMessageBusSource::FLiveLinkMessageBusSource(const FText& InSourceType, const FText& InSourceMachineName, const FMessageAddress& InConnectionAddress, double InMachineTimeOffset)
	: ConnectionAddress(InConnectionAddress)
	, bIsValid(false)
	, Client(nullptr)
	, SourceType(InSourceType)
	, SourceMachineName(InSourceMachineName)
	, ConnectionLastActive(0.0)
	, MachineTimeOffset(InMachineTimeOffset)
	, bDiscovering(false)
	, bInitialized(false )
{}

void FLiveLinkMessageBusSource::InitializeSettings(ULiveLinkSourceSettings* Settings)
{
}

void FLiveLinkMessageBusSource::ReceiveClient(ILiveLinkClient* InClient, FGuid InSourceGuid)
{
	Client = InClient;
	SourceGuid = InSourceGuid;

	Initialize();
}

void FLiveLinkMessageBusSource::Update()
{
	if (!ConnectionAddress.IsValid())
	{
#if WITH_LIVELINK_DISCOVERY_MANAGER_THREAD
		StartDiscovering();

		FLiveLinkMessageBusDiscoveryManager& DiscoveryManager = ILiveLinkModule::Get().GetMessageBusDiscoveryManager();
		for (const FProviderPollResultPtr& Result : DiscoveryManager.GetDiscoveryResults())
		{
			if (Client->GetSourceType(SourceGuid).ToString() == Result->Name)
			{
				ConnectionAddress = Result->Address;
				SourceMachineName = FText::FromString(Result->MachineName);
				MachineTimeOffset = Result->MachineTimeOffset;
				StopDiscovering();
				SendConnectMessage();
				UpdateConnectionLastActive();
				break;
			}
		}
#endif
	}
	else
	{
		const double HeartbeatTimeout = GetDefault<ULiveLinkSettings>()->GetMessageBusHeartbeatTimeout();
		const double CurrentTime = FApp::GetCurrentTime();

		float BytesLastSecond = 0.f;
		if (TOptional<FMessageTransportStatistics> Statistics = UE::LiveLinkHub::FClientNetworkStatisticsModel::GetLatestNetworkStatistics(ConnectionAddress))
		{
			BytesLastSecond = Statistics->TotalBytesReceived - AccumulatedBytes;
			AccumulatedBytes = Statistics->TotalBytesReceived;
		}

		if (CurrentTime - LastThroughputUpdate > 1.0)
		{
			LastThroughputUpdate = CurrentTime;
			CachedThroughput = BytesLastSecond / 1'000;
		}

		bIsValid = CurrentTime - ConnectionLastActive < HeartbeatTimeout;
		if (!bIsValid)
		{
			const double DeadSourceTimeout = GetDeadSourceTimeout();
			if (CurrentTime - ConnectionLastActive > DeadSourceTimeout)
			{
				RequestSourceShutdown();

				if (GLiveLinkMessageBusSourceReconnectAfterTimeout)
				{
					Initialize();
				}
				else
				{
					Client->RemoveSource(SourceGuid);
				}
			}
		}
	}
}

void FLiveLinkMessageBusSource::Initialize()
{
	if (bInitialized)
	{
		return;
	}

	for (const TSubclassOf<ULiveLinkRole>& RoleClass : FLiveLinkRoleTrait::GetRoles())
	{
		RoleInstances.Add(RoleClass->GetDefaultObject<ULiveLinkRole>());
	}

	MessageEndpoint = CreateAndInitializeMessageEndpoint();

	if (ConnectionAddress.IsValid())
	{
		SendConnectMessage();
	}
	else
	{
		StartDiscovering();
		bIsValid = false;
	}

	UpdateConnectionLastActive();

	bInitialized = true;
}

void FLiveLinkMessageBusSource::InternalHandleMessage(const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	UpdateConnectionLastActive();

	if (!Context->IsValid())
	{
		return;
	}

	if (bIsShuttingDown)
	{
		return;
	}

	UScriptStruct* MessageTypeInfo = Context->GetMessageTypeInfo().Get();
	if (MessageTypeInfo == nullptr)
	{
		return;
	}

	const bool bIsStaticData = MessageTypeInfo->IsChildOf(FLiveLinkBaseStaticData::StaticStruct());
	const bool bIsFrameData = MessageTypeInfo->IsChildOf(FLiveLinkBaseFrameData::StaticStruct());
	const bool bIsSerializedData = MessageTypeInfo->IsChildOf(FLiveLinkSerializedFrameData::StaticStruct());

	if (!bIsStaticData && !bIsFrameData && !bIsSerializedData)
	{
		return;
	}

	FName SubjectName = NAME_None;
	if (const FString* SubjectNamePtr = Context->GetAnnotations().Find(FLiveLinkMessageAnnotation::SubjectAnnotation))
	{
		SubjectName = *(*SubjectNamePtr);
	}
	if (SubjectName == NAME_None)
	{
		static const FName NAME_InvalidSubject = "LiveLinkMessageBusSource_InvalidSubject";
		FLiveLinkLog::ErrorOnce(NAME_InvalidSubject, FLiveLinkSubjectKey(SourceGuid, NAME_None), TEXT("No Subject Name was provided for connection '%s'"), *GetSourceMachineName().ToString());
		return;
	}

	// Find the role.
	TSubclassOf<ULiveLinkRole> SubjectRole;
	if (bIsStaticData)
	{
		// Check if it's in the Annotation first
		FName RoleName = NAME_None;
		if (const FString* RoleNamePtr = Context->GetAnnotations().Find(FLiveLinkMessageAnnotation::RoleAnnotation))
		{
			RoleName = *(*RoleNamePtr);
		}

		for (TWeakObjectPtr<ULiveLinkRole> WeakRole : RoleInstances)
		{
			if (ULiveLinkRole* Role = WeakRole.Get())
			{
				if (RoleName != NAME_None)
				{
					if (RoleName == Role->GetClass()->GetFName())
					{
						if (bIsStaticData && MessageTypeInfo->IsChildOf(Role->GetStaticDataStruct()))
						{
							SubjectRole = Role->GetClass();
							break;
						}
						if (bIsFrameData && MessageTypeInfo->IsChildOf(Role->GetFrameDataStruct()))
						{
							SubjectRole = Role->GetClass();
							break;
						}
					}
				}
				else
				{
					if (Role->GetStaticDataStruct() == MessageTypeInfo)
					{
						SubjectRole = Role->GetClass();
						break;
					}
				}
			}
		}

		if (SubjectRole.Get() == nullptr)
		{
			static const FName NAME_InvalidRole = "LiveLinkMessageBusSource_InvalidRole";
			FLiveLinkLog::ErrorOnce(NAME_InvalidRole, FLiveLinkSubjectKey(SourceGuid, SubjectName), TEXT("No Role was provided or found for subject '%s' with connection '%s'"), *SubjectName.ToString(), *GetSourceMachineName().ToString());
			return;
		}

	}

	const FLiveLinkSubjectKey SubjectKey(SourceGuid, SubjectName);
	if (bIsStaticData)
	{
		InitializeAndPushStaticData_AnyThread(SubjectName, SubjectRole, SubjectKey, Context, MessageTypeInfo);
	}
	else
	{
		InitializeAndPushFrameData_AnyThread(SubjectName, SubjectKey, Context, MessageTypeInfo);
	}
}

bool FLiveLinkMessageBusSource::IsSourceStillValid() const
{
	return ConnectionAddress.IsValid() && bIsValid;
}

FText FLiveLinkMessageBusSource::GetSourceStatus() const
{
	if (!ConnectionAddress.IsValid())
	{
		return InvalidSourceStatus();
	}
	else if (IsSourceStillValid())
	{
		return ValidSourceStatus();
	}
	return TimeoutSourceStatus();
}

FText FLiveLinkMessageBusSource::GetSourceToolTip() const
{
	const FString TemplateString = FString::Printf(TEXT("Throughput: %.1f KB/s"), CachedThroughput);
	return FText::FromString(TemplateString);
}

TSubclassOf<ULiveLinkSourceSettings> FLiveLinkMessageBusSource::GetSettingsClass() const
{
	return ULiveLinkMessageBusSourceSettings::StaticClass();
}

void FLiveLinkMessageBusSource::StartHeartbeatEmitter()
{
	FLiveLinkHeartbeatEmitter& HeartbeatEmitter = ILiveLinkModule::Get().GetHeartbeatEmitter();
	HeartbeatEmitter.StartHeartbeat(ConnectionAddress, MessageEndpoint);
}

TSharedPtr<FMessageEndpoint, ESPMode::ThreadSafe> FLiveLinkMessageBusSource::CreateAndInitializeMessageEndpoint()
{
	FMessageEndpointBuilder EndpointBuilder = FMessageEndpoint::Builder(GetSourceName());
	InitializeMessageEndpoint(EndpointBuilder);
	return EndpointBuilder;
}

void FLiveLinkMessageBusSource::InitializeAndPushStaticData_AnyThread(FName SubjectName,
																	  TSubclassOf<ULiveLinkRole> SubjectRole,
																	  const FLiveLinkSubjectKey& SubjectKey,
																	  const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context,
																	  UScriptStruct* MessageTypeInfo)
{
	check(MessageTypeInfo->IsChildOf(FLiveLinkBaseStaticData::StaticStruct()));

	FLiveLinkStaticDataStruct DataStruct(MessageTypeInfo);
	DataStruct.InitializeWith(MessageTypeInfo, reinterpret_cast<const FLiveLinkBaseStaticData*>(Context->GetMessage()));
	PushClientSubjectStaticData_AnyThread(SubjectKey, SubjectRole, MoveTemp(DataStruct));
}

void FLiveLinkMessageBusSource::InitializeAndPushFrameData_AnyThread(FName SubjectName,
																	 const FLiveLinkSubjectKey& SubjectKey,
																	 const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context,
																	 UScriptStruct* MessageTypeInfo)
{
	FLiveLinkFrameDataStruct DataStruct;

	if (MessageTypeInfo && MessageTypeInfo->IsChildOf(FLiveLinkSerializedFrameData::StaticStruct()))
	{
		// Extract the message from the compressed serialized data.
		const FLiveLinkSerializedFrameData* SerializedMessage = reinterpret_cast<const FLiveLinkSerializedFrameData*>(Context->GetMessage());
		FStructOnScope Payload;
		SerializedMessage->GetPayload(Payload);

		// Special case: We want to send anim data.
		if (Payload.GetStruct() == FLiveLinkFloatAnimationFrameData::StaticStruct())
		{
			const FLiveLinkFloatAnimationFrameData* FloatAnimData = reinterpret_cast<const FLiveLinkFloatAnimationFrameData*>(Payload.GetStructMemory());
			FLiveLinkAnimationFrameData DoubleFrameData = FLiveLinkFloatAnimationFrameData::ToAnimData(*FloatAnimData);

			DataStruct.InitializeWith(FLiveLinkAnimationFrameData::StaticStruct(), &DoubleFrameData);
			DataStruct.GetBaseData()->WorldTime = DoubleFrameData.WorldTime.GetOffsettedTime();
		}
		else
		{
			check(Payload.GetStruct()->IsChildOf<FLiveLinkBaseFrameData>());

			const FLiveLinkBaseFrameData* Message = reinterpret_cast<const FLiveLinkBaseFrameData*>(Payload.GetStructMemory());
			DataStruct.InitializeWith((UScriptStruct*)Payload.GetStruct(), Message);
			DataStruct.GetBaseData()->WorldTime = Message->WorldTime.GetOffsettedTime();
		}
	}
	else if (MessageTypeInfo && MessageTypeInfo->IsChildOf(FLiveLinkBaseFrameData::StaticStruct()))
	{
		const FLiveLinkBaseFrameData* Message = reinterpret_cast<const FLiveLinkBaseFrameData*>(Context->GetMessage());
		DataStruct.InitializeWith(MessageTypeInfo, Message);
		DataStruct.GetBaseData()->WorldTime = Message->WorldTime.GetOffsettedTime();
	}
	else
	{
		static const FName NAME_InvalidFrameData = "LiveLinkMessageBusSource_InvalidFrameData";
		FLiveLinkLog::ErrorOnce(NAME_InvalidFrameData, SubjectKey, TEXT("Invalid frame data was provided for '%s' with connection '%s'"), *SubjectName.ToString(), *GetSourceMachineName().ToString());
	}

	PushClientSubjectFrameData_AnyThread(SubjectKey, MoveTemp(DataStruct));
}

double FLiveLinkMessageBusSource::GetDeadSourceTimeout() const
{
	return GetDefault<ULiveLinkSettings>()->GetMessageBusTimeBeforeRemovingDeadSource();
}

void FLiveLinkMessageBusSource::PushClientSubjectStaticData_AnyThread(const FLiveLinkSubjectKey& SubjectKey,
																	  TSubclassOf<ULiveLinkRole> Role,
																	  FLiveLinkStaticDataStruct&& StaticData)
{
	Client->PushSubjectStaticData_AnyThread(SubjectKey, Role, MoveTemp(StaticData));
}

void FLiveLinkMessageBusSource::PushClientSubjectFrameData_AnyThread(const FLiveLinkSubjectKey& SubjectKey,
																	 FLiveLinkFrameDataStruct&& FrameData)
{
	Client->PushSubjectFrameData_AnyThread(SubjectKey, MoveTemp(FrameData));
}

void FLiveLinkMessageBusSource::HandleHeartbeat(const FLiveLinkHeartbeatMessage& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	UpdateConnectionLastActive();
}

void FLiveLinkMessageBusSource::HandleClearSubject(const FLiveLinkClearSubject& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	UpdateConnectionLastActive();

	if (!Message.SubjectName.IsNone())
	{
		const FLiveLinkSubjectKey SubjectKey(SourceGuid, Message.SubjectName);
		Client->RemoveSubject_AnyThread(SubjectKey);
	}
}

FORCEINLINE void FLiveLinkMessageBusSource::UpdateConnectionLastActive()
{
	FScopeLock ConnectionTimeLock(&ConnectionLastActiveSection);

	ConnectionLastActive = FPlatformTime::Seconds();
}

void FLiveLinkMessageBusSource::StartDiscovering()
{
#if WITH_LIVELINK_DISCOVERY_MANAGER_THREAD
	if (bDiscovering)
	{
		return;
	}

	FLiveLinkMessageBusDiscoveryManager& DiscoveryManager = ILiveLinkModule::Get().GetMessageBusDiscoveryManager();
	DiscoveryManager.AddDiscoveryMessageRequest();
#endif
}

void FLiveLinkMessageBusSource::StopDiscovering()
{
#if WITH_LIVELINK_DISCOVERY_MANAGER_THREAD
	if (!bDiscovering)
	{
		return;
	}

	FLiveLinkMessageBusDiscoveryManager& DiscoveryManager = ILiveLinkModule::Get().GetMessageBusDiscoveryManager();
	DiscoveryManager.RemoveDiscoveryMessageRequest();
#endif
}

void FLiveLinkMessageBusSource::SendConnectMessage()
{
	FLiveLinkConnectMessage* ConnectMessage = FMessageEndpoint::MakeMessage<FLiveLinkConnectMessage>();
	ConnectMessage->LiveLinkVersion = ILiveLinkClient::LIVELINK_VERSION;

	TMap<FName, FString> Annotations;
	AddAnnotations(Annotations);
	SendMessage(ConnectMessage, Annotations);
	StartHeartbeatEmitter();
	bIsValid = true;
	bIsShuttingDown = false;
}

bool FLiveLinkMessageBusSource::RequestSourceShutdown()
{
	if (!bInitialized)
	{
		return true;
	}

	StopDiscovering();

	FLiveLinkHeartbeatEmitter& HeartbeatEmitter = ILiveLinkModule::Get().GetHeartbeatEmitter();
	HeartbeatEmitter.StopHeartbeat(ConnectionAddress, MessageEndpoint);

	// Disable the Endpoint message handling since the message could keep it alive a bit.
	if (MessageEndpoint.IsValid())
	{
		MessageEndpoint->Disable();
	}
	MessageEndpoint.Reset();
	ConnectionAddress.Invalidate();

	RoleInstances.Empty();

	bInitialized = false;
	bIsShuttingDown = true;

	return true;
}

const FName& FLiveLinkMessageBusSource::GetSourceName() const
{
	static FName Name(TEXT("LiveLinkMessageBusSource"));
	return Name;
}

void FLiveLinkMessageBusSource::InitializeMessageEndpoint(FMessageEndpointBuilder& EndpointBuilder)
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	MessageEndpoint = EndpointBuilder
	.Handling<FLiveLinkHeartbeatMessage>(this, &FLiveLinkMessageBusSource::HandleHeartbeat)
	.Handling<FLiveLinkClearSubject>(this, &FLiveLinkMessageBusSource::HandleClearSubject)
	.ReceivingOnAnyThread()
	.WithCatchall(this, &FLiveLinkMessageBusSource::InternalHandleMessage);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

void FLiveLinkMessageBusSource::AddAnnotations(TMap<FName, FString>& InOutAnnotations) const
{
	if (CVarLiveLinkSupportCompressPayloads.GetValueOnAnyThread())
	{
		// The presence of this flag in the annotation will inform our provider that we support receiving compressed animation.
		InOutAnnotations.Add({ FLiveLinkMessageAnnotation::CompressedPayloadSupport, TEXT("") });
	}
}

