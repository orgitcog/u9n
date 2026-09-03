// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Chaos/ChaosVDRemoteSessionsManager.h"

#if WITH_CHAOS_VISUAL_DEBUGGER

#include "Delegates/IDelegateInstance.h"
#include "HAL/Platform.h"

class UGameInstance;

namespace Chaos
{
	namespace VisualDebugger
	{
		struct FChaosVDOptionalDataChannel;
	}
}

/** Object that bridges the gap between the ChaosVD runtime module and the Engine & CVD editor.
 * As the ChaosVDRuntime module does not have access to the engine module, this object reacts to events and performs necessary operation the runtime module cannot do directly
 */
class FChaosVDEngineEditorBridge
{
public:
	
	FChaosVDEngineEditorBridge() : RemoteSessionsManager(MakeShared<FChaosVDRemoteSessionsManager>())
	{
	}
	
	~FChaosVDEngineEditorBridge(){}

	CHAOSSOLVERENGINE_API static FChaosVDEngineEditorBridge& Get();

	void Initialize();
	void TearDown();

	TSharedPtr<FChaosVDRemoteSessionsManager> GetRemoteSessionsManager()
	{
		return RemoteSessionsManager;
	}

	using FChaosVDDataDataChannel = Chaos::VisualDebugger::FChaosVDOptionalDataChannel;

private:
	void AddOnScreenRecordingMessage();
	void RemoveOnScreenRecordingMessage();
	void HandleCVDRecordingStarted();
	void HandleCVDRecordingStopped();
	void HandleCVDRecordingStartFailed(const FText& InFailureReason) const;
	void HandlePIEStarted(UGameInstance* GameInstance);

	void HandleDataChannelChanged(TWeakPtr<FChaosVDDataDataChannel> ChannelWeakPtr);

	void SerializeCollisionChannelsNames();
	
	bool BroadcastSessionStatus(float DeltaTime);

	FDelegateHandle RecordingStartedHandle;
	FDelegateHandle RecordingStoppedHandle;
	FDelegateHandle RecordingStartFailedHandle;
	uint64 CVDRecordingMessageKey = 0;

#if WITH_EDITOR
	FDelegateHandle PIEStartedHandle;
#endif

	TSharedRef<FChaosVDRemoteSessionsManager> RemoteSessionsManager;
	
	FTSTicker::FDelegateHandle RecordingStatusUpdateHandle;
};
#else

class FChaosVDEngineEditorBridge
{
public:
	CHAOSSOLVERENGINE_API static FChaosVDEngineEditorBridge& Get();
	
	TSharedPtr<FChaosVDRemoteSessionsManager> GetRemoteSessionsManager()
	{
		return nullptr;
	}
};

#endif
