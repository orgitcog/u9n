// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_CHAOS_VISUAL_DEBUGGER

#include "ChaosVDRecordingDetails.h"
#include "Templates/SharedPointer.h"
#include "Modules/ModuleInterface.h"
#include "ProfilingDebugging/TraceAuxiliary.h"
#include "Containers/Ticker.h"
#include "Misc/ScopeRWLock.h"
#include "Misc/TransactionallySafeRWLock.h"

#define UE_API CHAOSVDRUNTIME_API

struct FChaosVDRecording;
class FText;

/* Option flags that controls what should be recorded when doing a full capture **/
enum class EChaosVDFullCaptureFlags : int32
{
	Geometry = 1 << 0,
	Particles = 1 << 1,
};
ENUM_CLASS_FLAGS(EChaosVDFullCaptureFlags)


DECLARE_MULTICAST_DELEGATE(FChaosVDRecordingStateChangedDelegate)
DECLARE_MULTICAST_DELEGATE_OneParam(FChaosVDCaptureRequestDelegate, EChaosVDFullCaptureFlags)
DECLARE_MULTICAST_DELEGATE_OneParam(FChaosVDRecordingStartFailedDelegate, const FText&)

class FChaosVDRuntimeModule : public IModuleInterface
{
public:

	static UE_API FChaosVDRuntimeModule& Get();
	static UE_API bool IsLoaded();
	
	UE_API virtual void StartupModule() override;
	UE_API virtual void ShutdownModule() override;

	/** Starts a CVD recording by starting a Trace session. It will stop any existing trace session
	 * @param Args : Arguments array provided by the commandline. Used to determine if we want to record to file or a local trace server
	 */
	UE_API void StartRecording(TConstArrayView<FString> Args);
	
	/* Stops an active recording */
	UE_API void StopRecording();

	/** Returns true if we are currently recording a Physics simulation */
	bool IsRecording() const { return bIsRecording; }

	/** Returns a unique ID used to be used to identify CVD (Chaos Visual Debugger) data */
	UE_API int32 GenerateUniqueID();

	static FDelegateHandle RegisterRecordingStartedCallback(const FChaosVDRecordingStateChangedDelegate::FDelegate& InCallback)
	{
		UE::TWriteScopeLock WriteLock(DelegatesRWLock);
		return RecordingStartedDelegate.Add(InCallback);
	}

	static FDelegateHandle RegisterRecordingStopCallback(const FChaosVDRecordingStateChangedDelegate::FDelegate& InCallback)
	{
		UE::TWriteScopeLock WriteLock(DelegatesRWLock);
		return RecordingStopDelegate.Add(InCallback);
	}

	static FDelegateHandle RegisterRecordingStartFailedCallback(const FChaosVDRecordingStartFailedDelegate::FDelegate& InCallback)
	{
		UE::TWriteScopeLock WriteLock(DelegatesRWLock);
		return RecordingStartFailedDelegate.Add(InCallback);
	}
	
	static FDelegateHandle RegisterFullCaptureRequestedCallback(const FChaosVDCaptureRequestDelegate::FDelegate& InCallback)
	{
		UE::TWriteScopeLock WriteLock(DelegatesRWLock);
		return PerformFullCaptureDelegate.Add(InCallback);
	}

	static bool RemoveRecordingStartedCallback(const FDelegateHandle& InDelegateToRemove)
	{
		UE::TWriteScopeLock WriteLock(DelegatesRWLock);
		return RecordingStartedDelegate.Remove(InDelegateToRemove);
	}

	static bool RemoveRecordingStopCallback(const FDelegateHandle& InDelegateToRemove)
	{
		UE::TWriteScopeLock WriteLock(DelegatesRWLock);
		return RecordingStopDelegate.Remove(InDelegateToRemove);
	}

	static bool RemoveRecordingStartFailedCallback(const FDelegateHandle& InDelegateToRemove)
	{
		UE::TWriteScopeLock WriteLock(DelegatesRWLock);
		return RecordingStartFailedDelegate.Remove(InDelegateToRemove);
	}
	
	static bool RemoveFullCaptureRequestedCallback(const FDelegateHandle& InDelegateToRemove)
	{
		UE::TWriteScopeLock WriteLock(DelegatesRWLock);
		return PerformFullCaptureDelegate.Remove(InDelegateToRemove);
	}

	/** Returns the accumulated recording time in seconds since the recording started */
	float GetAccumulatedRecordingTime() const { return AccumulatedRecordingTime; }

	/** Returns the full path of the active recording file*/
	UE_API FString GetLastRecordingFileNamePath() const;

	UE_API FChaosVDTraceDetails GetCurrentTraceSessionDetails() const;

	EChaosVDRecordingMode GetCurrentRecordingMode() const
	{
		return CurrentRecordingMode;
	}

private:

	/** Stops the current Trace session */
	UE_API void StopTrace();
	/** Finds a valid file name for a new file - Used to generate the file name for the Trace recording */
	UE_API void GenerateRecordingFileName(FString& OutFileName);

	/** Queues a full Capture of the simulation on the next frame */
	UE_API bool RequestFullCapture(float DeltaTime);

	/** Queues a full Capture of the simulation on the next frame */
	UE_API bool RecordingTimerTick(float DeltaTime);

	/** Used to handle stop requests to the active trace session that were not done by us
	 * That is a possible scenario because Trace is shared by other In-Editor tools
	 */
	UE_API void HandleTraceStopRequest(FTraceAuxiliary::EConnectionType TraceType, const FString& TraceDestination);

	UE_API bool WaitForTraceSessionDisconnect();

	UE_API void EnableRequiredTraceChannels();
	UE_API void SaveAndDisabledCurrentEnabledTraceChannels();
	UE_API void RestoreTraceChannelsToPreRecordingState();

	bool bIsRecording = false;
	bool bRequestedStop = false;

	float AccumulatedRecordingTime = 0.0f;

	FTSTicker::FDelegateHandle FullCaptureRequesterHandle;
	FTSTicker::FDelegateHandle RecordingTimerHandle;

	static UE_API FChaosVDRecordingStateChangedDelegate RecordingStartedDelegate;
	static UE_API FChaosVDRecordingStateChangedDelegate RecordingStopDelegate;
	static UE_API FChaosVDRecordingStartFailedDelegate RecordingStartFailedDelegate;
	static UE_API FChaosVDCaptureRequestDelegate PerformFullCaptureDelegate;

	std::atomic<int32> LastGeneratedID;

	FString LastRecordingFileNamePath;

	TMap<FString, bool> OriginalTraceChannelsState;

	EChaosVDRecordingMode CurrentRecordingMode = EChaosVDRecordingMode::Invalid;

	static UE_API FTransactionallySafeRWLock DelegatesRWLock;
};
#endif

#undef UE_API 