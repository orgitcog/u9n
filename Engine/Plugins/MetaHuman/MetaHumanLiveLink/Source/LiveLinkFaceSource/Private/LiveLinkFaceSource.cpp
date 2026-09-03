// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkFaceSource.h"

#include "Async/Async.h"
#include "Common/UdpSocketBuilder.h"
#include "Common/UdpSocketReceiver.h"
#include "InterpolationProcessor/LiveLinkBasicFrameInterpolateProcessor.h"
#include "Sockets.h"
#include "UObject/Package.h"
#include "Roles/LiveLinkBasicRole.h"
#include "EngineAnalytics.h"
#include "Engine/Engine.h"

#include "LiveLinkFaceSourceSettings.h"

#include "LiveLinkFacePacket.h"
#include "LiveLinkFaceSubjectSettings.h"
#include "LiveLinkFaceSourceDefaults.h"

DEFINE_LOG_CATEGORY(LogLiveLinkFaceSource);

#define LOCTEXT_NAMESPACE "FLiveLinkFaceSource"

FLiveLinkFaceSource::FLiveLinkFaceSource(const FString& InConnectionString)
	: Status(Disconnected)
	, ConnectionString(InConnectionString)
	, ControlValueScalingFactor(UINT16_MAX)
{
	// If analytics shutdowns while source is running, because editor was closed, ensure analytics are still sent.
	// It would be too late to try to send them in Stop function.
#if WITH_EDITOR
	AnalyticsShutdownHandler = FEngineAnalytics::OnShutdownEngineAnalytics.AddRaw(this, &FLiveLinkFaceSource::SendAnalytics);
#endif
}

FLiveLinkFaceSource::~FLiveLinkFaceSource()
{
#if WITH_EDITOR
	FEngineAnalytics::OnShutdownEngineAnalytics.Remove(AnalyticsShutdownHandler);
#endif

	UE_LOG(LogLiveLinkFaceSource, Verbose, TEXT("Destroying Source"))
	Stop();
}

void FLiveLinkFaceSource::ReceiveClient(ILiveLinkClient* InClient, FGuid InSourceGuid)
{
	LiveLinkClient = InClient;
	LiveLinkClient->OnLiveLinkSubjectAdded().AddSP(this, &FLiveLinkFaceSource::SubjectAdded);
	
	SourceGuid = InSourceGuid;
}

bool FLiveLinkFaceSource::IsSourceStillValid() const
{
	return Status == Connected;
}

bool FLiveLinkFaceSource::RequestSourceShutdown()
{
	Stop();
	return true;
}

FText FLiveLinkFaceSource::GetSourceType() const
{
	return LOCTEXT("SourceType", "Live Link Face");
}

FText FLiveLinkFaceSource::GetSourceMachineName() const
{
	return Control.IsValid() ? Control->GetServerName() : FText();
}

FText FLiveLinkFaceSource::GetSourceStatus() const
{
	switch (Status)
	{
	case Disconnected:
		return LOCTEXT("DisconnectedSourceStatus", "Disconnected");
	case Connecting:
		return LOCTEXT("ConnectingSourceStatus", "Connecting");
	case Connected:
		return LOCTEXT("ConnectedSourceStatus", "Connected");
	default:
		return LOCTEXT("UnknownSourceStatus", "Unknown");
	}
}

TSubclassOf<ULiveLinkSourceSettings> FLiveLinkFaceSource::GetSettingsClass() const
{
	return ULiveLinkFaceSourceSettings::StaticClass();
}

void FLiveLinkFaceSource::InitializeSettings(ULiveLinkSourceSettings* InSettings)
{
	ULiveLinkFaceSourceSettings* LiveLinkFaceSourceSettings = Cast<ULiveLinkFaceSourceSettings>(InSettings);
	LiveLinkFaceSourceSettings->Init(this, ConnectionString);

	InitUdpReceiver();
	
	// Only connect to the server if the address is valid.
	// This should only be the case if we have loaded a preset and populated the settings with a valid connection string.
	if (!LiveLinkFaceSourceSettings->IsAddressValid())
	{
		return;
	}

	Connect(LiveLinkFaceSourceSettings);
}

void FLiveLinkFaceSource::Connect(const ULiveLinkFaceSourceSettings* InSettings)
{
	// Remove all subjects in case we are switching to a different server.
	for (FLiveLinkSubjectKey LiveLinkSubjectKey : GetSubjects())
	{
		LiveLinkClient->RemoveSubject_AnyThread(LiveLinkSubjectKey);
	}

	RemoteSubject.Reset();
	
	const FString& Host = InSettings->GetAddress();
	const uint16 Port = InSettings->GetPort();
	
	CustomSubjectName = InSettings->GetSubjectName();
	
	UE_LOG(LogLiveLinkFaceSource, Display, TEXT("Connecting to Host %s Port: %d"), *Host, Port)

	FLiveLinkFaceControl::FOnSelectRemoteSubject OnSelectRemoteSubjectDelegate = FLiveLinkFaceControl::FOnSelectRemoteSubject::CreateSP(this, &FLiveLinkFaceSource::OnSelectRemoteSubject);
	FLiveLinkFaceControl::FOnStreamingStarted OnStreamingStartedDelegate = FLiveLinkFaceControl::FOnStreamingStarted::CreateSP(this, &FLiveLinkFaceSource::OnStreamingStarted);
	
	Control = MakeUnique<FLiveLinkFaceControl>(
		SourceGuid,
		Host,
		Port,
		UdpSocket->GetPortNo(),
		MoveTemp(OnSelectRemoteSubjectDelegate),
		MoveTemp(OnStreamingStartedDelegate));

	Control->Start();

	Status = Connecting;
}

void FLiveLinkFaceSource::Stop()
{
	UE_LOG(LogLiveLinkFaceSource, Verbose, TEXT("Stopping"));

	SendAnalytics();

	if (UdpSocket.IsValid())
	{
		UdpSocket->Close();
		UdpSocket.Reset();
	}
	
	if (UdpReceiver.IsValid())
	{
		UdpReceiver->Stop();
		UdpReceiver.Reset();
	}

	if (Control.IsValid())
	{
		Control->Stop();
		Control.Reset();
	}

	Status = Disconnected;
}

bool FLiveLinkFaceSource::InitUdpReceiver()
{
	// In reality the packets are between 1KB and 2KB
	constexpr int32 ReceiveBufferSize = 2048;
	
	FSocket* Socket = FUdpSocketBuilder(TEXT("Live Link Face Source UDP Socket"))
	                  .AsNonBlocking()
	                  .AsReusable()
	                  .WithReceiveBufferSize(ReceiveBufferSize)
	                  .BoundToAddress(FIPv4Address::Any)
	                  .Build();
	
	if (!Socket)
	{
		UE_LOG(LogLiveLinkFaceSource, Error, TEXT("Failed to create UDP socket"));
		return false;
	}

	
	UdpSocket.Reset(Socket);

	UdpReceiver = MakeUnique<FUdpSocketReceiver>(Socket, FTimespan::FromMilliseconds(100), TEXT("FLiveLinkFaceSource-UdpReceiver"));
	UdpReceiver->OnDataReceived().BindSP(this, &FLiveLinkFaceSource::OnDataReceived);
	
	UdpReceiver->Start();

	return true;
}

void FLiveLinkFaceSource::OnDataReceived(const FArrayReaderPtr& InPayload, const FIPv4Endpoint& InEndpoint)
{
	UE_LOG(LogLiveLinkFaceSource, VeryVerbose, TEXT("Read %lld bytes from %s"), InPayload->TotalSize(), *InEndpoint.ToString());

	FLiveLinkFacePacket Packet;
	const bool bReadResult = Packet.Read(InPayload);

	if (bReadResult == false)
	{
		UE_LOG(LogLiveLinkFaceSource, Error, TEXT("Error reading payload"));
		return;
	}

	if (Status != Connected)
	{
		UE_LOG(LogLiveLinkFaceSource, Verbose, TEXT("Received valid data but the source is not yet in a connected state"));
		return;
	}
	
	ProcessPacket(Packet);
}

const FLiveLinkFaceControl::FRemoteSubject FLiveLinkFaceSource::OnSelectRemoteSubject(
	const FLiveLinkFaceControl::FRemoteSubjects& InRemoteSubjects)
{
	check(!InRemoteSubjects.IsEmpty())
	
	// Right now we only support a single subject, so we will select the first result
	FLiveLinkFaceControl::FRemoteSubject SelectedSubject = InRemoteSubjects[0];

	// If we have specified a subject name in the source UI then override the subject name in the remote subject.
	// This will cause the remote subject name to be updated when streaming is started.
	if (!CustomSubjectName.IsEmpty())
	{
		SelectedSubject.Name = CustomSubjectName;
	}
	else
	{
		// If we're loading from a preset we should already have at least one subject, if so we will use that as the remote subject name.
		TArray<FLiveLinkSubjectKey> ExistingSubjects = GetSubjects();
		if (!ExistingSubjects.IsEmpty())
		{
			SelectedSubject.Name = ExistingSubjects[0].SubjectName.ToString();
		}
	}
	
	return SelectedSubject;
}

void FLiveLinkFaceSource::OnStreamingStarted(const FLiveLinkFaceControl::FRemoteSubject& InRemoteSubject)
{
	Status = Connected;
	RemoteSubject = MakeShared<FLiveLinkFaceControl::FRemoteSubject>(InRemoteSubject);

	bSendAnalytics = true;
	ProcessingStarted = FPlatformTime::Seconds();
	NumAnimationFrames = 0;
	AnalyticsItems.Reset();

	const FString Platform = Control->GetServerPlatform().ToString();

	if (Platform == "iOS" || Platform == "iPadOS")
	{
		AnalyticsItems.Add(TEXT("DeviceType"), "Live Link Face " + Platform);
	}
	else if (Platform.StartsWith("Android"))
	{
		AnalyticsItems.Add(TEXT("DeviceType"), "Live Link Face Android");
	}
	else
	{
		AnalyticsItems.Add(TEXT("DeviceType"), "Live Link Face Unknown (" + Platform + ")");
	}

	AnalyticsItems.Add(TEXT("DeviceModel"), Control->GetServerModel().ToString());

	AsyncTask(ENamedThreads::GameThread, [this, InRemoteSubject]()
	{
		const FLiveLinkSubjectKey& LiveLinkSubjectKey = FLiveLinkSubjectKey(SourceGuid, *InRemoteSubject.Name);
	
		UE_LOG(LogLiveLinkFaceSource, Verbose, TEXT("Streaming started for subject '%s' with %d control values."), *LiveLinkSubjectKey.SubjectName.ToString(), InRemoteSubject.PropertyNames.Num())

		// Check if this Live Link subject already exists with this key which may be the case when loading a pre-set
		const bool bLiveLinkSubjectExists = GetSubjects().ContainsByPredicate([LiveLinkSubjectKey](const FLiveLinkSubjectKey& ExistingKey)
		{
			return ExistingKey == LiveLinkSubjectKey;
		});

		if (!bLiveLinkSubjectExists)
		{
			// Create Live Link Subject
			const TSubclassOf<ULiveLinkRole> Role = ULiveLinkBasicRole::StaticClass();

			ULiveLinkFaceSubjectSettings* SubjectSettings = NewObject<ULiveLinkFaceSubjectSettings>(GetTransientPackage(), ULiveLinkFaceSubjectSettings::StaticClass());
			SubjectSettings->InterpolationProcessor = NewObject<ULiveLinkBasicFrameInterpolationProcessor>(SubjectSettings);
			SubjectSettings->Role = Role;

			const ULiveLinkFaceSourceDefaults* DefaultSettings = GetDefault<ULiveLinkFaceSourceDefaults>();

			SubjectSettings->bHeadOrientation = DefaultSettings->bHeadOrientation;
			SubjectSettings->bHeadTranslation = DefaultSettings->bHeadTranslation;
	
			FLiveLinkSubjectPreset Preset;
			Preset.Key = LiveLinkSubjectKey;
			Preset.Role = Role;
			Preset.Settings = SubjectSettings;
			Preset.bEnabled = true;

			if (!LiveLinkClient->CreateSubject(Preset))
			{
				UE_LOG(LogLiveLinkFaceSource, Warning, TEXT("Failed to create subject"))
			}
		}
		else
		{
			// Push static data to existing subject.
			// The subject may have been created via a preset.
			PushStaticData(LiveLinkSubjectKey, InRemoteSubject.PropertyNames);
		}
	});
}

void FLiveLinkFaceSource::ProcessPacket(const FLiveLinkFacePacket& InPacket)
{
	const FString SubjectId = InPacket.GetSubjectId();

	if (!RemoteSubject.IsValid())
	{
		UE_LOG(LogLiveLinkFaceSource, Warning, TEXT("Received packet but no remote subject is set"));
		return;
	}

	if (RemoteSubject->Id != InPacket.GetSubjectId())
	{
		UE_LOG(LogLiveLinkFaceSource, Warning, TEXT("Received packet for unknown subject id: %s"), *SubjectId);
		return;
	}

	const FLiveLinkSubjectKey LiveLinkSubjectKey = FLiveLinkSubjectKey(SourceGuid, *RemoteSubject->Name);
	const FString SubjectName = LiveLinkSubjectKey.SubjectName.ToString();
	
	// Check incoming control values count matches our static data
	const TArray<uint16> ControlValues = InPacket.GetControlValues();
	const uint32 ControlValueCount = ControlValues.Num();
	const uint32 ExpectedControlValueCount = RemoteSubject->PropertyNames.Num();
	
	if (ControlValueCount != ExpectedControlValueCount)
	{
		UE_LOG(LogLiveLinkFaceSource, Warning, TEXT("Received an unexpected number of control values for subject '%s'. Received %d Expected %d"), *SubjectName, ControlValueCount, ExpectedControlValueCount);
		Control->RestartConnection();
		return;
	}

	// Does static data exist for this subject key?
	// If not or if the static data is invalid, we are unable to process this packet.
	const FLiveLinkStaticDataStruct* SubjectStaticData = LiveLinkClient->GetSubjectStaticData_AnyThread(LiveLinkSubjectKey);
	if (SubjectStaticData == nullptr || !SubjectStaticData->IsValid())
	{
		UE_LOG(LogLiveLinkFaceSource, Verbose, TEXT("Packet received for subject '%s' but static data has not yet been set."), *SubjectName);
		return;
	}
	
	// Push frame data
	FLiveLinkFrameDataStruct FrameDataStruct = FLiveLinkFrameDataStruct(FLiveLinkBaseFrameData::StaticStruct());
	FLiveLinkBaseFrameData& FrameData = *FrameDataStruct.Cast<FLiveLinkBaseFrameData>();
	FrameData.MetaData.SceneTime = InPacket.GetQualifiedFrameTime();
	PopulatePropertyValues(InPacket.GetControlValues(), FrameData.PropertyValues);

	// Pass on the provided head pose property values if enabled
	ULiveLinkFaceSubjectSettings* SubjectSettings = Cast<ULiveLinkFaceSubjectSettings>(LiveLinkClient->GetSubjectSettings(LiveLinkSubjectKey));
	TArray<float> HeadPose = InPacket.GetHeadPose();
	constexpr int32 HeadPoseValueCount = FLiveLinkFacePacket::HeadPoseValueCount;
	if (HeadPose.Num() != HeadPoseValueCount)
	{
		UE_LOG(LogLiveLinkFaceSource, Error, TEXT("Expected %d head pose values in packet but received %d."), HeadPoseValueCount, HeadPose.Num())
		return;
	}
	
	const bool bHeadOrientation = SubjectSettings->bHeadOrientation;
	const bool bHeadTranslation = SubjectSettings->bHeadTranslation;

	// If either head orientation or translation is enabled then set HeadControlSwitch to 1.0 so that we drive the head movement in the rig.
	// Otherwise we should set the value to 0.0 as we are not providing any head movement information.
	FrameData.PropertyValues.Add(bHeadOrientation || bHeadTranslation); // HeadControlSwitch
	
	// 0 - Roll
	// 1 - Pitch
	// 2 - Yaw
	// 3 - X
	// 4 - Y
	// 5 - Z

	if (bHeadOrientation)
	{
		FrameData.PropertyValues.Add(HeadPose[0]); // Roll
		FrameData.PropertyValues.Add(HeadPose[1]); // Pitch
		FrameData.PropertyValues.Add(HeadPose[2]); // Yaw
	}
	else
	{
		FrameData.PropertyValues.Add(0); // Roll
		FrameData.PropertyValues.Add(0); // Pitch
		FrameData.PropertyValues.Add(0); // Yaw
	}

	int32 HeadPoseMode = 0;
	if (bHeadTranslation)
	{
		HeadPoseMode = 1;
		FrameData.PropertyValues.Add(HeadPose[3]); // X 
		FrameData.PropertyValues.Add(HeadPose[4]); // Y
		FrameData.PropertyValues.Add(HeadPose[5]); // Z
	}
	else
	{
		FrameData.PropertyValues.Add(0); // X
		FrameData.PropertyValues.Add(0); // Y
		FrameData.PropertyValues.Add(0); // Z
	}
	
	FrameData.PropertyValues.Add(RemoteSubject->AnimationVersion); // MHFDSVersion
	FrameData.PropertyValues.Add(1); // DisableFaceOverride

	// Provide the head pose mode value as expected by the head translation preprocessor.
	FrameData.MetaData.StringMetaData.Add("HeadPoseMode", FString::Printf(TEXT("%i"), HeadPoseMode));

	SubjectSettings->PreProcess(*(*SubjectStaticData).Cast<FLiveLinkBaseStaticData>(), FrameData);

	LiveLinkClient->PushSubjectFrameData_AnyThread(LiveLinkSubjectKey, MoveTemp(FrameDataStruct));

	NumAnimationFrames++;
	AnalyticsItems.Add(TEXT("HeadTranslation"), LexToString(bHeadTranslation));
	AnalyticsItems.Add(TEXT("HeadOrientation"), LexToString(bHeadOrientation));
	AnalyticsItems.Add(TEXT("HasCalibrationNeutral"), LexToString(!SubjectSettings->NeutralFrame.IsEmpty()));
	AnalyticsItems.Add(TEXT("HasHeadTranslationNeutral"), LexToString(SubjectSettings->NeutralHeadTranslation.Length() > 0));
}

void FLiveLinkFaceSource::PopulatePropertyValues(const TArray<uint16>& InControlValues, TArray<float>& OutPropertyValues)
{
	OutPropertyValues.Reset(InControlValues.Num());
	for (const uint16 ControlValue : InControlValues)
	{
		OutPropertyValues.Add(float(ControlValue) / ControlValueScalingFactor);
	}
}

TArray<FLiveLinkSubjectKey> FLiveLinkFaceSource::GetSubjects() const
{
	return LiveLinkClient->GetSubjects(true, true).FilterByPredicate([this](const FLiveLinkSubjectKey& Key)
	{
		return Key.Source == SourceGuid;
	});
}

void FLiveLinkFaceSource::SubjectAdded(FLiveLinkSubjectKey InSubjectKey)
{
	if (InSubjectKey.Source != SourceGuid)
	{
		// This could be called for subjects created by other sources so we really don't want to do anything with those or react at all.
		return;
	}

	// Ensure that all subjects created via the live link source set bIsLiveProcessing to true.
	// This counts for new sources and those created via a preset.
	ULiveLinkFaceSubjectSettings* SubjectSettings = Cast<ULiveLinkFaceSubjectSettings>(LiveLinkClient->GetSubjectSettings(InSubjectKey));
	SubjectSettings->bIsLiveProcessing = true;

	// If the subject is added as part of a preset this method (SubjectAdded) will be called before we've gathered remote subject information.
	// In this case the OnStreamingStarted method will push the static data to the existing subject.
	// For new sources this method (SubjectAdded) will be called *after* remote subject data has been gathered,
	// in which case we need to push static data at this point.
	if (RemoteSubject.IsValid())
	{
		PushStaticData(InSubjectKey, RemoteSubject->PropertyNames);
	}
}

void FLiveLinkFaceSource::PushStaticData(const FLiveLinkSubjectKey& InSubjectKey, const TArray<FName> InPropertyNames)
{
	FLiveLinkStaticDataStruct StaticDataStruct(FLiveLinkBaseStaticData::StaticStruct());
	FLiveLinkBaseStaticData& StaticData = *StaticDataStruct.Cast<FLiveLinkBaseStaticData>();
	StaticData.PropertyNames = InPropertyNames;

	// Add Head Pose Property Names
	StaticData.PropertyNames.Add("HeadControlSwitch");
	StaticData.PropertyNames.Add("HeadRoll");
	StaticData.PropertyNames.Add("HeadPitch");
	StaticData.PropertyNames.Add("HeadYaw");
	StaticData.PropertyNames.Add("HeadTranslationX");
	StaticData.PropertyNames.Add("HeadTranslationY");
	StaticData.PropertyNames.Add("HeadTranslationZ");

	StaticData.PropertyNames.Add("MHFDSVersion");
	StaticData.PropertyNames.Add("DisableFaceOverride");
		
	LiveLinkClient->PushSubjectStaticData_AnyThread(InSubjectKey, ULiveLinkBasicRole::StaticClass(), MoveTemp(StaticDataStruct));
}

void FLiveLinkFaceSource::SendAnalytics()
{
	if (bSendAnalytics && GEngine->AreEditorAnalyticsEnabled() && FEngineAnalytics::IsAvailable())
	{
		bSendAnalytics = false;

		AnalyticsItems.Add(TEXT("NumAnimationFrames"), LexToString(NumAnimationFrames));
		AnalyticsItems.Add(TEXT("Duration"), LexToString(FPlatformTime::Seconds() - ProcessingStarted));

		TArray<FAnalyticsEventAttribute> AnalyticsEvents;
		for (const TPair<FString, FString>& AnalyticsItem : AnalyticsItems)
		{
			AnalyticsEvents.Add(FAnalyticsEventAttribute(AnalyticsItem.Key, AnalyticsItem.Value));
		}

#if 0
		for (const FAnalyticsEventAttribute& Attr : AnalyticsEvents)
		{
			UE_LOG(LogTemp, Warning, TEXT("[%s] [%s]"), *Attr.GetName(), *Attr.GetValue());
		}
#else
		FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.MetaHumanLiveLinkPlugin.ProcessInfo"), AnalyticsEvents);
#endif
	}
}

#undef LOCTEXT_NAMESPACE
