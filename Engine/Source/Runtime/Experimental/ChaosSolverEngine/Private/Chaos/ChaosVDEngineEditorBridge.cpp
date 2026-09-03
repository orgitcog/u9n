// Copyright Epic Games, Inc. All Rights Reserved.

#include "Chaos/ChaosVDEngineEditorBridge.h"

#include "GameFramework/PlayerController.h"

#if WITH_CHAOS_VISUAL_DEBUGGER

#include "ChaosVDRuntimeModule.h"
#include "ChaosVisualDebugger/ChaosVDTraceMacros.h"
#include "ChaosVisualDebugger/ChaosVisualDebuggerTrace.h"
#include "DataWrappers/ChaosVDCollisionDataWrappers.h"
#include "Engine/Engine.h"
#include "Serialization/MemoryWriter.h"

void FChaosVDEngineEditorBridge::AddOnScreenRecordingMessage()
{
	if (!GEngine)
	{
		return;
	}

	static FText ChaosVDRecordingStartedMessage = NSLOCTEXT("ChaosVisualDebugger", "OnScreenChaosVDRecordingStartedMessage", "Chaos Visual Debugger recording in progress...");

	if (CVDRecordingMessageKey == 0)
	{
		CVDRecordingMessageKey = GetTypeHash(ChaosVDRecordingStartedMessage.ToString());
	}
	
	// Add a long duration value, we will remove the message manually when the recording stops
	constexpr float MessageDurationSeconds = 3600.0f;
	GEngine->AddOnScreenDebugMessage(CVDRecordingMessageKey, MessageDurationSeconds, FColor::Red,ChaosVDRecordingStartedMessage.ToString());
}

void FChaosVDEngineEditorBridge::RemoveOnScreenRecordingMessage()
{
	if (!GEngine)
	{
		return;
	}

	if (CVDRecordingMessageKey != 0)
	{
		GEngine->RemoveOnScreenDebugMessage(CVDRecordingMessageKey);
	}
}

void FChaosVDEngineEditorBridge::HandleCVDRecordingStarted()
{
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float DeltaTime)
	{
		// We need to wait at least one frame to serialize the collision channel names to ensure the Archive header used for the whole session is traced
		FChaosVDEngineEditorBridge::Get().SerializeCollisionChannelsNames();
		return false;
	}));

	AddOnScreenRecordingMessage();

	BroadcastSessionStatus(FApp::GetDeltaTime());

	constexpr float UpdateInterval = 0.5f;
	RecordingStatusUpdateHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FChaosVDEngineEditorBridge::BroadcastSessionStatus), UpdateInterval);
}

void FChaosVDEngineEditorBridge::HandleCVDRecordingStopped()
{
	RemoveOnScreenRecordingMessage();

	FTSTicker::GetCoreTicker().RemoveTicker(RecordingStatusUpdateHandle);
	BroadcastSessionStatus(FApp::GetDeltaTime());
}

void FChaosVDEngineEditorBridge::HandleCVDRecordingStartFailed(const FText& InFailureReason) const
{
#if !WITH_EDITOR
	// In non-editor builds we don't have an error pop-up, therefore we want to show the error message on screen
	FText ErrorMessage = FText::FormatOrdered(NSLOCTEXT("ChaosVisualDebugger","StartRecordingFailedOnScreenMessage", "Failed to start CVD recording. {0}"), InFailureReason);

	constexpr float MessageDurationSeconds = 4.0f;
	GEngine->AddOnScreenDebugMessage(CVDRecordingMessageKey, MessageDurationSeconds, FColor::Red, ErrorMessage.ToString());
#endif
}

void FChaosVDEngineEditorBridge::HandlePIEStarted(UGameInstance* GameInstance)
{
	// If we were already recording, show the message
	if (FChaosVDRuntimeModule::Get().IsRecording())
	{
		HandleCVDRecordingStarted();
	}
}

void FChaosVDEngineEditorBridge::HandleDataChannelChanged(TWeakPtr<FChaosVDDataDataChannel> ChannelWeakPtr)
{
	if (TSharedPtr<FChaosVDDataDataChannel> DataChannelPtr = ChannelWeakPtr.Pin())
	{
		FChaosVDChannelStateChangeResponseMessage NewChannelState;
		NewChannelState.InstanceID = FApp::GetInstanceId();
		NewChannelState.NewState.bIsEnabled = DataChannelPtr->IsChannelEnabled();
		NewChannelState.NewState.ChannelName = DataChannelPtr->GetId().ToString();
		NewChannelState.NewState.bCanChangeChannelState = DataChannelPtr->CanChangeEnabledState();
		
		RemoteSessionsManager->PublishDataChannelStateChangeUpdate(NewChannelState);
	}
}

void FChaosVDEngineEditorBridge::SerializeCollisionChannelsNames()
{
	TArray<uint8> CollisionChannelsDataBuffer;
	FMemoryWriter MemWriterAr(CollisionChannelsDataBuffer);

	FChaosVDCollisionChannelsInfoContainer CollisionChannelInfoContainer;

	if (UCollisionProfile* CollisionProfileData = UCollisionProfile::Get())
	{
		constexpr int32 MaxSupportedChannels = 32;
		for (int32 ChannelIndex = 0; ChannelIndex < MaxSupportedChannels; ++ChannelIndex)
		{
			FChaosVDCollisionChannelInfo Info;
			Info.DisplayName = CollisionProfileData->ReturnChannelNameFromContainerIndex(ChannelIndex).ToString();
			Info.CollisionChannel = ChannelIndex;
			Info.bIsTraceType = CollisionProfileData->ConvertToTraceType(static_cast<ECollisionChannel>(ChannelIndex)) != TraceTypeQuery_MAX;
			CollisionChannelInfoContainer.CustomChannelsNames[ChannelIndex] = Info;
		}
	}

	Chaos::VisualDebugger::WriteDataToBuffer(CollisionChannelsDataBuffer, CollisionChannelInfoContainer);

	CVD_TRACE_BINARY_DATA(CollisionChannelsDataBuffer, FChaosVDCollisionChannelsInfoContainer::WrapperTypeName);
}

FChaosVDEngineEditorBridge& FChaosVDEngineEditorBridge::Get()
{
	static FChaosVDEngineEditorBridge CVDEngineEditorBridge;
	return CVDEngineEditorBridge;
}

bool FChaosVDEngineEditorBridge::BroadcastSessionStatus(float DeltaTime)
{
	FChaosVDRuntimeModule& RuntimeModule = FChaosVDRuntimeModule::Get();

	FChaosVDRecordingStatusMessage StatusMessage;
	StatusMessage.InstanceId = FApp::GetInstanceId();
	StatusMessage.bIsRecording = RuntimeModule.IsRecording();
	StatusMessage.ElapsedTime = RuntimeModule.GetAccumulatedRecordingTime();
	StatusMessage.TraceDetails = RuntimeModule.GetCurrentTraceSessionDetails();

	RemoteSessionsManager->PublishRecordingStatusUpdate(StatusMessage);

	return true;
}

void FChaosVDEngineEditorBridge::Initialize()
{
	FChaosVDRuntimeModule& Module = FChaosVDRuntimeModule::Get();

	FChaosVDTraceDetails TraceDetails = Module.GetCurrentTraceSessionDetails();

	RemoteSessionsManager->Initialize();

	RecordingStartedHandle = FChaosVDRuntimeModule::Get().RegisterRecordingStartedCallback(FChaosVDRecordingStateChangedDelegate::FDelegate::CreateRaw(this, &FChaosVDEngineEditorBridge::HandleCVDRecordingStarted));
	RecordingStoppedHandle = FChaosVDRuntimeModule::Get().RegisterRecordingStopCallback(FChaosVDRecordingStateChangedDelegate::FDelegate::CreateRaw(this, &FChaosVDEngineEditorBridge::HandleCVDRecordingStopped));
	RecordingStartFailedHandle = FChaosVDRuntimeModule::Get().RegisterRecordingStartFailedCallback(FChaosVDRecordingStartFailedDelegate::FDelegate::CreateRaw(this, &FChaosVDEngineEditorBridge::HandleCVDRecordingStartFailed));

	Chaos::VisualDebugger::FChaosVDDataChannelsManager::Get().OnChannelStateChanged().AddRaw(this, &FChaosVDEngineEditorBridge::HandleDataChannelChanged);

#if WITH_EDITOR
	PIEStartedHandle = FWorldDelegates::OnPIEStarted.AddRaw(this, &FChaosVDEngineEditorBridge::HandlePIEStarted);
#endif

	// If we were already recording, show the message
	if (FChaosVDRuntimeModule::Get().IsRecording())
	{
		HandleCVDRecordingStarted();
	}
}

void FChaosVDEngineEditorBridge::TearDown()
{
	FTSTicker::GetCoreTicker().RemoveTicker(RecordingStatusUpdateHandle);

	// Note: This works during engine shutdown because the Module Manager doesn't free the dll on module unload to account for use cases like this
	// If this appears in a callstack crash it means that assumption changed or was not correct to begin with.
	// A possible solution is just check if the module is loaded querying the module manager just using the module's name
	if (FChaosVDRuntimeModule::IsLoaded())
	{
		FChaosVDRuntimeModule::Get().RemoveRecordingStartedCallback(RecordingStartedHandle);
		FChaosVDRuntimeModule::Get().RemoveRecordingStopCallback(RecordingStoppedHandle);

#if WITH_EDITOR
		 FWorldDelegates::OnPIEStarted.Remove(PIEStartedHandle);
#endif

		// Make sure of removing the message from the screen in case the recording didn't quite stopped yet
		if (FChaosVDRuntimeModule::Get().IsRecording())
		{
			HandleCVDRecordingStopped();
		}

		RemoteSessionsManager->Shutdown();
	}
}
#else

FChaosVDEngineEditorBridge& FChaosVDEngineEditorBridge::Get()
{
	static FChaosVDEngineEditorBridge CVDEngineEditorBridge;
	return CVDEngineEditorBridge;
}

#endif
