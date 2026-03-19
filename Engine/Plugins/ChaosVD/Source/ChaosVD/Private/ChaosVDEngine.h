// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "Containers/Ticker.h"
#include "Misc/Guid.h"
#include "Trace/ChaosVDTraceManager.h"

#include "ChaosVDEngine.generated.h"

namespace Chaos::VisualDebugger
{
	struct FChaosVDOptionalDataChannel;
}

class FChaosVDTraceManager;
class FChaosVDPlaybackController;
class FChaosVDScene;
class FChaosVisualDebuggerMainUI;

/** Enumeration of the available modes controlling how data is loaded into CVD */
UENUM()
enum class EChaosVDLoadRecordedDataMode : uint8
{
	/** This mode will unload any CVD recording currently loaded before loading the selected file */
	SingleSource,
	/** CVD will load and merge the data of the selected recording into the currently loaded recording */
	MultiSource
};

DECLARE_MULTICAST_DELEGATE_OneParam(FSessionStateChangedDelegate, const FChaosVDTraceSessionDescriptor& InSessionDescriptor)

/** Core Implementation of the visual debugger - Owns the systems that are not UI */
class FChaosVDEngine : public FTSTickerObjectBase, public TSharedFromThis<FChaosVDEngine>
{
public:
	FChaosVDEngine()
	{
		InstanceGUID = FGuid::NewGuid();
	}

	void Initialize();
	void CloseActiveTraceSessions();
	void StopActiveTraceSessions();

	void DeInitialize();

	virtual bool Tick(float DeltaTime) override;

	const FGuid& GetInstanceGuid() const
	{
		return InstanceGUID;
	}

	TSharedPtr<FChaosVDScene>& GetCurrentScene()
	{
		return CurrentScene;
	}

	TSharedPtr<FChaosVDPlaybackController>& GetPlaybackController()
	{
		return PlaybackController;
	}

	TArrayView<FChaosVDTraceSessionDescriptor> GetCurrentSessionDescriptors()
	{
		return CurrentSessionDescriptors;
	}

	bool HasAnyLiveSessionActive() const;

	void LoadRecording(const FString& FilePath, EChaosVDLoadRecordedDataMode LoadingMode = EChaosVDLoadRecordedDataMode::MultiSource);
	void LoadCombinedMultiRecording(const FString& FilePath);

	bool ConnectToLiveSession(uint32 SessionID, const FString& InSessionAddress, EChaosVDLoadRecordedDataMode LoadingMode = EChaosVDLoadRecordedDataMode::MultiSource);

	void OpenSession(const FChaosVDTraceSessionDescriptor& SessionDescriptor, EChaosVDLoadRecordedDataMode LoadingMode = EChaosVDLoadRecordedDataMode::MultiSource);

	bool SaveOpenSessionToCombinedFile(const FString& InTargetFilePath = FString());

	bool CanCombineOpenSessions() const;
	
	FSessionStateChangedDelegate& OnSessionOpened()
	{
		return OnSessionOpenedDelegate;
	}

	FSessionStateChangedDelegate& OnSessionClosed()
	{
		return OnSessionClosedDelegate;
	}

private:

	void LoadRecording_Internal(const TFunction<FString(const TSharedPtr<FChaosVDRecording>&)>&, EChaosVDLoadRecordedDataMode LoadingMode = EChaosVDLoadRecordedDataMode::MultiSource);

	void RestoreDataChannelsEnabledStateFromSave();
	void UpdateSavedDataChannelsEnabledState(TWeakPtr<Chaos::VisualDebugger::FChaosVDOptionalDataChannel> DataChannelChanged);

	void UpdateRecentFilesList(const FString& InFilename);

	FGuid InstanceGUID;

	TArray<FChaosVDTraceSessionDescriptor> CurrentSessionDescriptors;

	TSharedPtr<FChaosVDScene> CurrentScene;
	TSharedPtr<FChaosVDPlaybackController> PlaybackController;
	
	bool bIsInitialized = false;

	FDelegateHandle LiveSessionStoppedDelegateHandle;

	FDelegateHandle DataChannelStateUpdatedHandle;
	
	FSessionStateChangedDelegate OnSessionOpenedDelegate;
	FSessionStateChangedDelegate OnSessionClosedDelegate;
};
