// Copyright Epic Games, Inc. All Rights Reserved.

#include "AudioMixerWasapi.h"

#include "Async/Async.h"
#include "ScopedCom.h"
#include "WasapiAggregateDeviceMgr.h"
#include "WasapiDefaultDeviceMgr.h"

namespace Audio
{
	FAudioMixerWasapi::FAudioMixerWasapi()
	{
	}

	FAudioMixerWasapi::~FAudioMixerWasapi()
	{
	}

	void FAudioMixerWasapi::CreateDeviceManager(const bool bInUseAggregateDevice, TUniquePtr<IAudioMixerWasapiDeviceManager>& InDeviceManager)
	{
		if (bInUseAggregateDevice)
		{
			InDeviceManager = MakeUnique<FWasapiAggregateDeviceMgr>();
		}
		else
		{
			InDeviceManager = MakeUnique<FWasapiDefaultDeviceMgr>();
		}

		ensure(InDeviceManager);
	}

	bool FAudioMixerWasapi::InitializeHardware()
	{
		SCOPED_NAMED_EVENT(FAudioMixerWasapi_InitializeHardware, FColor::Blue);

		RegisterDeviceChangedListener();
		
		if (IAudioMixer::ShouldRecycleThreads())
		{
			// Pre-create the null render device thread so we can simply wake it up when needed.
			// Give it nothing to do, with a slow tick as the default, but ask it to wait for a signal to wake up.
			CreateNullDeviceThread([] {}, 1.0f, true);
		}

		return true;
	}

	bool FAudioMixerWasapi::TeardownHardware()
	{
		if (!bIsInitialized)
		{
			AUDIO_PLATFORM_LOG_ONCE(TEXT("FAudioMixerWasapi::TeardownHardware failed...not initialized."), Warning);
			return false;
		}

		// Lock prior to changing state to avoid race condition if there happens to be an in-flight device swap
		FScopeLock Lock(&DeviceSwapCriticalSection);

		if (DeviceManager.IsValid() && !DeviceManager->TeardownHardware())
		{
			AUDIO_PLATFORM_LOG_ONCE(TEXT("FAudioMixerWasapi::TeardownHardware DeviceManager->TeardownHardware() failed."), Warning);
		}

		bIsInitialized = false;

		return true;
	}

	bool FAudioMixerWasapi::IsInitialized() const
	{
		return bIsInitialized;
	}

	int32 FAudioMixerWasapi::GetNumFrames(const int32 InNumRequestedFrames)
	{
		if (DeviceManager.IsValid())
		{
			return DeviceManager->GetNumFrames(InNumRequestedFrames);
		}

		return InNumRequestedFrames;
	}

	bool FAudioMixerWasapi::GetNumOutputDevices(uint32& OutNumOutputDevices)
	{
		SCOPED_NAMED_EVENT(FAudioMixerWasapi_GetNumOutputDevices, FColor::Blue);

		OutNumOutputDevices = 0;

		if (const IAudioPlatformDeviceInfoCache* Cache = GetDeviceInfoCache())
		{
			OutNumOutputDevices = Cache->GetAllActiveOutputDevices().Num();
			return true;
		}
		else
		{
			AUDIO_PLATFORM_LOG_ONCE(TEXT("FAudioMixerWasapi device cache not initialized"), Warning);
			return false;
		}
	}

	bool FAudioMixerWasapi::GetOutputDeviceInfo(const uint32 InDeviceIndex, FAudioPlatformDeviceInfo& OutInfo)
	{
		SCOPED_NAMED_EVENT(FAudioMixerWasapi_GetOutputDeviceInfo, FColor::Blue);

		if (const IAudioPlatformDeviceInfoCache* Cache = GetDeviceInfoCache())
		{
			if (InDeviceIndex == AUDIO_MIXER_DEFAULT_DEVICE_INDEX)
			{
				if (TOptional<FAudioPlatformDeviceInfo> Defaults = Cache->FindDefaultOutputDevice())
				{
					OutInfo = *Defaults;
					return true;
				}
			}
			else
			{
				TArray<FAudioPlatformDeviceInfo> ActiveDevices = Cache->GetAllActiveOutputDevices();
				if (ActiveDevices.IsValidIndex(InDeviceIndex))
				{
					OutInfo = ActiveDevices[InDeviceIndex];
					return true;
				}
			}
		}

		return false;
	}

	bool FAudioMixerWasapi::GetDefaultOutputDeviceIndex(uint32& OutDefaultDeviceIndex) const
	{
		OutDefaultDeviceIndex = AUDIO_MIXER_DEFAULT_DEVICE_INDEX;
		return true;
	}

	bool FAudioMixerWasapi::InitStreamParams(const uint32 InDeviceIndex, const int32 InNumBufferFrames, const int32 InNumBuffers, const int32 InSampleRate, TArray<FWasapiRenderStreamParams>& OutParams)
	{
		SCOPED_NAMED_EVENT(FAudioMixerWasapi_InitStreamParams, FColor::Blue);

		FAudioPlatformDeviceInfo DeviceInfo;
		if (!GetOutputDeviceInfo(InDeviceIndex, DeviceInfo))
		{
			UE_LOG(LogAudioMixer, Error, TEXT("FAudioMixerWasapi::InitStreamParams unable to find default device"));
			return false;
		}

		return InitStreamParams(DeviceInfo, InNumBufferFrames, InNumBuffers, InSampleRate, OutParams);
	}
	
	bool FAudioMixerWasapi::InitStreamParams(const FAudioPlatformDeviceInfo& InDeviceInfo, const int32 InNumBufferFrames, const int32 InNumBuffers, const int32 InSampleRate, TArray<FWasapiRenderStreamParams>& OutParams) const
	{
		SCOPED_NAMED_EVENT(FAudioMixerWasapi_InitStreamParams, FColor::Blue);
		check(GetDeviceInfoCache());

		if (GetDeviceInfoCache()->IsAggregateHardwareDeviceId(*InDeviceInfo.DeviceId))
		{
			if (const IAudioPlatformDeviceInfoCache* Cache = GetDeviceInfoCache())
			{
				const FName DeviceId = *InDeviceInfo.DeviceId;
				// We use the HardwareId as the DeviceId for aggregate devices which is used by
				// GetAggregateDeviceInfo to gather all the logical devices belonging to this aggregate.
				TArray<FAudioPlatformDeviceInfo> AggregateDevice = Cache->GetLogicalAggregateDevices(DeviceId, EDeviceEndpointType::Render);

				for (const FAudioPlatformDeviceInfo& AggregateDeviceInfo : AggregateDevice)
				{
					TComPtr<IMMDevice> MMDevice = GetMMDevice(AggregateDeviceInfo.DeviceId);
					if (!MMDevice)
					{
						UE_LOG(LogAudioMixer, Error, TEXT("FAudioMixerWasapi::InitStreamParams null MMDevice"));
						return false;
					}

					OutParams.Emplace(FWasapiRenderStreamParams(MMDevice, AggregateDeviceInfo, InNumBufferFrames, InNumBuffers, InSampleRate));
				}
			}
		}
		else
		{
			const TComPtr<IMMDevice> MMDevice = GetMMDevice(InDeviceInfo.DeviceId);
			if (!MMDevice)
			{
				UE_LOG(LogAudioMixer, Error, TEXT("FAudioMixerWasapi::InitStreamParams null MMDevice"));
				return false;
			}

			OutParams.Emplace(FWasapiRenderStreamParams(MMDevice, InDeviceInfo, InNumBufferFrames, InNumBuffers, InSampleRate));
		}

		return true;
	}

	bool FAudioMixerWasapi::OpenAudioStream(const FAudioMixerOpenStreamParams& Params)
	{
		SCOPED_NAMED_EVENT(FAudioMixerWasapi_OpenAudioStream, FColor::Green);
		check(GetDeviceInfoCache());

		OpenStreamParams = Params;

		AudioStreamInfo.Reset();

		AudioStreamInfo.OutputDeviceIndex = OpenStreamParams.OutputDeviceIndex;
		AudioStreamInfo.NumOutputFrames = OpenStreamParams.NumFrames;
		AudioStreamInfo.NumBuffers = OpenStreamParams.NumBuffers;
		AudioStreamInfo.AudioMixer = OpenStreamParams.AudioMixer;

		// If the user has selected a specific audio device (not the system default), then
		// ignore device change events.
		SetIsListeningForDeviceEvents(Params.bUseSystemAudioDevice);

		TArray<FWasapiRenderStreamParams> StreamParams;
		if (!InitStreamParams(OpenStreamParams.OutputDeviceIndex, OpenStreamParams.NumFrames, OpenStreamParams.NumBuffers, OpenStreamParams.SampleRate, StreamParams))
		{
			return false;
		}

		// Adopt the first device info. In the case of an aggregate device, all of the sub-devices will
		// be identical because they belong to the same physical device.
		AudioStreamInfo.DeviceInfo = StreamParams[0].HardwareDeviceInfo;

		// Set the current device name
		const bool bIsAggregateDevice = GetDeviceInfoCache()->IsAggregateHardwareDeviceId(*Params.AudioDeviceId);
		if (bIsAggregateDevice)
		{
			CurrentDeviceName = ExtractAggregateDeviceName(AudioStreamInfo.DeviceInfo.Name);
		}
		else
		{
			CurrentDeviceName = AudioStreamInfo.DeviceInfo.Name;
		}

		// Create and initialize the device manager
		CreateDeviceManager(bIsAggregateDevice, DeviceManager);
		
		// The ReadNextBufferCallback life cycle is tied to this object. It is ultimately bound to a delegate
		// in the render stream object which will be unbound in TeardownHardware, prior to 'this' being deallocated.
		TFunction<void()> ReadNextBufferCallback = [this](){ ReadNextBuffer(); };
		if (!ensure(DeviceManager && DeviceManager->InitializeHardware(StreamParams, ReadNextBufferCallback)))
		{
			AUDIO_PLATFORM_LOG_ONCE(TEXT("FAudioMixerWasapi::OpenAudioStream DeviceManager->InitializeHardware() failed"), Warning);
			return false;
		}

		// Assign the total number of direct out channels based on the device manager. For WasapiDefaultDeviceMgr this will be 0 
		// and for WasapiAggregateDeviceMgr this will be the total of all the channels less the main outs (first 8 channels).
		AudioStreamInfo.DeviceInfo.NumDirectOutChannels = DeviceManager->GetNumDirectOutChannels();
		
		if (!ensure(DeviceManager.IsValid() && DeviceManager->OpenAudioStream(StreamParams)))
		{
			UE_LOG(LogAudioMixer, Error, TEXT("FAudioMixerWasapi::OpenAudioStream DeviceManager->OpenAudioStream() failed"));
			return false;
		}

		// Store the device ID here in case it is removed. We can switch back if the device comes back.
		if (Params.bRestoreIfRemoved)
		{
			SetOriginalAudioDeviceId(AudioStreamInfo.DeviceInfo.DeviceId);
		}

		bIsInitialized = true;

		UE_LOG(LogAudioMixer, Display, TEXT("FAudioMixerWasapi initialized: SampleRate=%d NumChannels=%d NumDirectOutChannels=%d bIsAggregateDevice=%d"), 
			OpenStreamParams.SampleRate, AudioStreamInfo.DeviceInfo.NumChannels, AudioStreamInfo.DeviceInfo.NumDirectOutChannels, bIsAggregateDevice);

		return true;
	}

	bool FAudioMixerWasapi::CloseAudioStream()
	{
		if (!bIsInitialized || AudioStreamInfo.StreamState == EAudioOutputStreamState::Closed)
		{
			return false;
		}

		// Lock prior to changing state to avoid race condition if there happens to be an in-flight device swap
		FScopeLock Lock(&DeviceSwapCriticalSection);

		// If we're closing the stream, we're not interested in the results of the device swap. 
		// Reset the handle to the future.
		ResetActiveDeviceSwapFuture();

		if (DeviceManager.IsValid() && !DeviceManager->CloseAudioStream())
		{
			UE_LOG(LogAudioMixer, Warning, TEXT("FAudioMixerWasapi::CloseAudioStream CloseAudioStream failed"));
		}

		AudioStreamInfo.StreamState = EAudioOutputStreamState::Closed;

		return true;
	}

	bool FAudioMixerWasapi::StartAudioStream()
	{
		bool bDidStartAudioStream = false;
		
		if (bIsInitialized)
		{
			if (DeviceManager.IsValid() && DeviceManager->IsInitialized())
			{
				bDidStartAudioStream = DeviceManager->StartAudioStream();
			}
			else
			{
				check(!bIsUsingNullDevice);
				StartRunningNullDevice();
			}
			
			// Can be called during device swap when AudioRenderEvent can be null
			if (nullptr == AudioRenderEvent)
			{
				// This will set AudioStreamInfo.StreamState to EAudioOutputStreamState::Running
				BeginGeneratingAudio();
			}
			else
			{
				AudioStreamInfo.StreamState = EAudioOutputStreamState::Running;
			}
		}

		return bDidStartAudioStream;
	}

	bool FAudioMixerWasapi::StopAudioStream()
	{
		if (!bIsInitialized)
		{
			AUDIO_PLATFORM_LOG_ONCE(TEXT("FAudioMixerWasapi::StopAudioStream() not initialized."), Warning);
			return false;
		}

		// Lock prior to changing state to avoid race condition if there happens to be an in-flight device swap
		FScopeLock Lock(&DeviceSwapCriticalSection);

		UE_LOG(LogAudioMixer, Display, TEXT("FAudioMixerWasapi::StopAudioStream() InstanceID=%d, StreamState=%d"),
			InstanceID, static_cast<int32>(AudioStreamInfo.StreamState));

		if (AudioStreamInfo.StreamState != EAudioOutputStreamState::Stopped && AudioStreamInfo.StreamState != EAudioOutputStreamState::Closed)
		{
			// Shutdown the AudioRenderThread if we're running or mid-device swap
			if (AudioStreamInfo.StreamState == EAudioOutputStreamState::Running ||
				AudioStreamInfo.StreamState == EAudioOutputStreamState::SwappingDevice)
			{
				StopGeneratingAudio();
			}

			if (DeviceManager.IsValid())
			{
				DeviceManager->StopAudioStream();
			}

			check(AudioStreamInfo.StreamState == EAudioOutputStreamState::Stopped);
		}
		
		if (bIsUsingNullDevice)
		{
			StopRunningNullDevice();
		}

		return true;
	}

	FAudioPlatformDeviceInfo FAudioMixerWasapi::GetPlatformDeviceInfo() const
	{
		return AudioStreamInfo.DeviceInfo;
	}

	void FAudioMixerWasapi::SubmitBuffer(const uint8* InBuffer)
	{
		SCOPED_NAMED_EVENT(FAudioMixerWasapi_SubmitBuffer, FColor::Blue);

		if (DeviceManager.IsValid())
		{
			DeviceManager->SubmitBuffer(InBuffer, OpenStreamParams.NumFrames);
		}
	}

	void FAudioMixerWasapi::SubmitDirectOutBuffer(const int32 InDirectOutIndex, const FAlignedFloatBuffer& InBuffer)
	{
		SCOPED_NAMED_EVENT(FAudioMixerWasapi_SubmitDirectOutBuffer, FColor::Green);
		
		if (DeviceManager.IsValid())
		{
			DeviceManager->SubmitDirectOutBuffer(InDirectOutIndex, InBuffer);
		}
	}

	FString FAudioMixerWasapi::GetDefaultDeviceName()
	{
		return FString();
	}

	FAudioPlatformSettings FAudioMixerWasapi::GetPlatformSettings() const
	{
#if WITH_ENGINE
		return FAudioPlatformSettings::GetPlatformSettings(FPlatformProperties::GetRuntimeSettingsClassName());
#else
		return FAudioPlatformSettings();
#endif // WITH_ENGINE
	}

	IAudioPlatformDeviceInfoCache* FAudioMixerWasapi::GetDeviceInfoCache() const
	{
		if (ShouldUseDeviceInfoCache())
		{
			return DeviceInfoCache.Get();
		}

		return nullptr;
	}
	
	bool FAudioMixerWasapi::IsDeviceInfoValid(const FAudioPlatformDeviceInfo& InDeviceInfo) const
	{
		// Device enumeration will not return invalid devices. This
		// is more of a sanity check.
		if (InDeviceInfo.NumChannels > 0 && InDeviceInfo.SampleRate > 0)
		{
			return true;
		}

		return false;
	}

	void FAudioMixerWasapi::OnSessionDisconnect(IAudioMixerDeviceChangedListener::EDisconnectReason InReason)
	{
		// Device has disconnected from current session.
		if (InReason == IAudioMixerDeviceChangedListener::EDisconnectReason::FormatChanged)
		{
			// OnFormatChanged, retry again same device.
			RequestDeviceSwap(GetDeviceId(), /*force*/ true, TEXT("FAudioMixerWasapi::OnSessionDisconnect() - FormatChanged"));		
		}
		else if (InReason == IAudioMixerDeviceChangedListener::EDisconnectReason::DeviceRemoval)
		{
			// Ignore Device Removal, as this is handle by the Device Removal logic in the Notification Client.
		}
		else
		{
			// ServerShutdown, SessionLogoff, SessionDisconnected, ExclusiveModeOverride
			// Attempt a default swap, will likely fail, but then we'll switch to a null device.
			RequestDeviceSwap(TEXT(""), /*force*/ true, TEXT("FAudioMixerWasapi::OnSessionDisconnect() - Other"));
		}
	}
	
	bool FAudioMixerWasapi::CheckThreadedDeviceSwap()
	{
		bool bDidStopGeneratingAudio = false;
#if PLATFORM_WINDOWS
		bDidStopGeneratingAudio = FAudioMixerPlatformSwappable::CheckThreadedDeviceSwap();
#endif //PLATFORM_WINDOWS
		return bDidStopGeneratingAudio;
	}

	bool FAudioMixerWasapi::InitializeDeviceSwapContext(const FString& InRequestedDeviceID, const TCHAR* InReason)
	{
		check(GetDeviceInfoCache());

		// Look up device. Blank name looks up current default.
		const FName NewDeviceId = *InRequestedDeviceID;
		TOptional<FAudioPlatformDeviceInfo> DeviceInfo;
	
		if (TOptional<FAudioPlatformDeviceInfo> TempDeviceInfo = GetDeviceInfoCache()->FindActiveOutputDevice(NewDeviceId))
		{
			if (TempDeviceInfo.IsSet())
			{
				if (IsDeviceInfoValid(*TempDeviceInfo))
				{
					DeviceInfo = MoveTemp(TempDeviceInfo);
				}
				else
				{
					UE_LOG(LogAudioMixer, Display, TEXT("FAudioMixerWasapi::InitializeDeviceSwapContext - Ignoring attempt to switch to device with unsupported params: Channels=%u, SampleRate=%u, Id=%s, Name=%s"),
						(uint32)TempDeviceInfo->NumChannels, (uint32)TempDeviceInfo->SampleRate, *TempDeviceInfo->DeviceId, *TempDeviceInfo->Name);

					return false;
				}
			}
		}
		
		return InitDeviceSwapContextInternal(InRequestedDeviceID, InReason, DeviceInfo);
	}

	bool FAudioMixerWasapi::InitDeviceSwapContextInternal(const FString& InRequestedDeviceID, const TCHAR* InReason, const TOptional<FAudioPlatformDeviceInfo>& InDeviceInfo)
	{
		check(GetDeviceInfoCache());
		
		// Access to device swap context must be protected by DeviceSwapCriticalSection
		FScopeLock Lock(&DeviceSwapCriticalSection);
		
		if (DeviceSwapContext.IsValid())
		{
			UE_LOG(LogAudioMixer, Display, TEXT("FAudioMixerWasapi::InitDeviceSwapContextInternal - DeviceSwapContext in-flight, ignoring"));
			return false;
		}

		// Create the device swap context which will be valid over the course of the swap
		DeviceSwapContext = MakeUnique<FWasapiDeviceSwapContext>(InRequestedDeviceID, InReason);
		if (!DeviceSwapContext.IsValid())
		{
			UE_LOG(LogAudioMixer, Warning, TEXT("FMixerPlatformWasapi::CreateDeviceSwapContext - failed to create DeviceSwapContext"));
			return false;
		}

		DeviceSwapContext->NewDevice = InDeviceInfo;
		
		const FAudioPlatformSettings EngineSettings = GetPlatformSettings();
		TArray<FWasapiRenderStreamParams> StreamParams;

		if (DeviceSwapContext->NewDevice.IsSet())
		{
			DeviceSwapContext->bIsAggregateDevice = GetDeviceInfoCache()->IsAggregateHardwareDeviceId(*DeviceSwapContext->NewDevice->DeviceId);
			
			if (!InitStreamParams(*DeviceSwapContext->NewDevice, EngineSettings.CallbackBufferFrameSize, EngineSettings.NumBuffers, EngineSettings.SampleRate, StreamParams))
			{
				UE_LOG(LogAudioMixer, Warning, TEXT("FAudioMixerWasapi::InitializeDeviceSwapContext - InitStreamParams() failed"));
				DeviceSwapContext->NewDevice.Reset();

				return false;
			}
		}

		// Initialize remaining fields except for OldDeviceManager which will 
		// happen later in CheckThreadedDeviceSwap from the Game thread.
		DeviceSwapContext->ReadNextBufferCallback = [this](){ ReadNextBuffer(); };
		DeviceSwapContext->StreamParams = StreamParams;
		DeviceSwapContext->PlatformSettings = EngineSettings;

		return true;
	}
	
	void FAudioMixerWasapi::EnqueueAsyncDeviceSwap()
	{
		FScopeLock Lock(&DeviceSwapCriticalSection);
		UE_LOG(LogAudioMixer, Display, TEXT("FAudioMixerWasapi::EnqueueAsyncDeviceSwap - enqueuing async device swap"));
		
		TFunction<TUniquePtr<FDeviceSwapResult>()> AsyncDeviceSwap = [this]() mutable -> TUniquePtr<FDeviceSwapResult>
		{
			// Transfer ownership of DeviceSwapContext to the async task.
			TUniquePtr<FWasapiDeviceSwapContext> TempContext;
			{
				FScopeLock Lock(&DeviceSwapCriticalSection);

				if (AudioStreamInfo.StreamState == EAudioOutputStreamState::SwappingDevice)
				{
					TempContext = MoveTemp(DeviceSwapContext);
				}
			}
			
			return PerformDeviceSwap(MoveTemp(TempContext));
		};
		SetActiveDeviceSwapFuture(Async(EAsyncExecution::TaskGraph, MoveTemp(AsyncDeviceSwap)));
	}

	void FAudioMixerWasapi::SynchronousDeviceSwap()
	{
		FScopeLock Lock(&DeviceSwapCriticalSection);

		// Transfer ownership of DeviceSwapContext memory to the device swap routine
		TUniquePtr<FDeviceSwapResult> DeviceSwapResult = PerformDeviceSwap(TUniquePtr<FWasapiDeviceSwapContext>(MoveTemp(DeviceSwapContext)));

		// Set the promise and future result to replicate what the async task does
		TPromise<TUniquePtr<FDeviceSwapResult>> Promise;
		
		// It's ok if DeviceSwapResult is null here. It indicates an invalid device which will be handled.
		Promise.SetValue(MoveTemp(DeviceSwapResult));
		SetActiveDeviceSwapFuture(Promise.GetFuture());
	}
	
	TUniquePtr<FDeviceSwapResult> FAudioMixerWasapi::PerformDeviceSwap(TUniquePtr<FWasapiDeviceSwapContext>&& InDeviceContext)
	{
		// Static method enforces no other state sharing occurs with the object that called it. InDeviceContext
		// should have no other references outside of this method so that the device swap operation is isolated.
		SCOPED_NAMED_EVENT(FAudioMixerWasapi_PerformDeviceSwap, FColor::Blue);

		const uint64 StartTimeCycles = FPlatformTime::Cycles64();
		// This runs in an async task whose thread may not have initialized com
		FScopedCoInitialize CoInitialize;

		// No need to lock critical section here since this call has sole ownership of the context
		if (InDeviceContext.IsValid())
		{
			UE_LOG(LogAudioMixer, Display, TEXT("FAudioMixerWasapi::PerformDeviceSwap - AsyncTask Start. Because=%s"), *InDeviceContext->DeviceSwapReason);

			if (InDeviceContext->OldDeviceManager.IsValid())
			{
				// Shutdown the current device manager
				InDeviceContext->OldDeviceManager->StopAudioStream();
				InDeviceContext->OldDeviceManager->CloseAudioStream();
				InDeviceContext->OldDeviceManager->TeardownHardware();
				InDeviceContext->OldDeviceManager.Reset();
				UE_LOG(LogAudioMixer, Display, TEXT("FAudioMixerWasapi::PerformDeviceSwap - successfully shut down previous device manager"));
			}
			else
			{
				UE_LOG(LogAudioMixer, Display, TEXT("FAudioMixerWasapi::PerformDeviceSwap - no device manager running, null renderer must be active"));
			}

			// Don't attempt to create a new setup if there's no devices available.
			if (!InDeviceContext->NewDevice.IsSet() || InDeviceContext->StreamParams.IsEmpty())
			{
				UE_LOG(LogAudioMixer, Display, TEXT("FAudioMixerWasapi::PerformDeviceSwap - no new device to switch to...will run null device"));
				return {};
			}

			TUniquePtr<FWasapiDeviceSwapResult> DeviceSwapResult = MakeUnique<FWasapiDeviceSwapResult>();
			CreateDeviceManager(InDeviceContext->bIsAggregateDevice, DeviceSwapResult->NewDeviceManager);
			
			if (!DeviceSwapResult->NewDeviceManager.IsValid())
			{
				UE_LOG(LogAudioMixer, Warning, TEXT("FAudioMixerWasapi::PerformDeviceSwap - InitializeHardware failed to create new device manager"));
				return {};
			}
			
			if (!DeviceSwapResult->NewDeviceManager->InitializeHardware(InDeviceContext->StreamParams, InDeviceContext->ReadNextBufferCallback))
			{
				UE_LOG(LogAudioMixer, Warning, TEXT("FAudioMixerWasapi::PerformDeviceSwap - InitializeHardware failed while attempting to device swap"));
				return {};
			}
			
			if (!DeviceSwapResult->NewDeviceManager->OpenAudioStream(InDeviceContext->StreamParams))
			{
				UE_LOG(LogAudioMixer, Warning, TEXT("FAudioMixerWasapi::PerformDeviceSwap - OpenAudioStream failed while attempting to device swap"));
				return {};
			}
			
			DeviceSwapResult->SuccessfulDurationMs = FPlatformTime::ToMilliseconds64(FPlatformTime::Cycles64() - StartTimeCycles);
			DeviceSwapResult->DeviceInfo = InDeviceContext->StreamParams[0].HardwareDeviceInfo;
			DeviceSwapResult->bIsAggregateDevice = InDeviceContext->bIsAggregateDevice;
			DeviceSwapResult->SwapReason = InDeviceContext->DeviceSwapReason;
			UE_LOG(LogAudioMixer, Display, TEXT("FAudioMixerWasapi::PerformDeviceSwap - successfully completed device swap"));

			return DeviceSwapResult;
		}
		else
		{
			UE_LOG(LogAudioMixer, Error, TEXT("FAudioMixerWasapi::PerformDeviceSwap - failed due to invalid DeviceSwapContext"));
		}

		return {};
	}
	
	bool FAudioMixerWasapi::PreDeviceSwap()
	{
		if (DeviceManager.IsValid())
		{
			// Access to device swap context must be protected by DeviceSwapCriticalSection
			FScopeLock Lock(&DeviceSwapCriticalSection);

			if (DeviceSwapContext.IsValid())
			{
				// Finish initializing the device swap context
				check(!DeviceSwapContext->OldDeviceManager);
				DeviceSwapContext->OldDeviceManager = MoveTemp(DeviceManager);
				UE_LOG(LogAudioMixer, Display, TEXT("FAudioMixerWasapi::PreDeviceSwap - Starting swap to [%s]"), DeviceSwapContext->RequestedDeviceId.IsEmpty() ? TEXT("[System Default]") : *DeviceSwapContext->RequestedDeviceId);
			}
			else
			{
				UE_LOG(LogAudioMixer, Warning, TEXT("FAudioMixerWasapi::PreDeviceSwap - null device swap context"));
				return false;
			}
		}
		else
		{
			// This is not an error because the null renderer could be running
			UE_LOG(LogAudioMixer, Display, TEXT("FAudioMixerWasapi::PreDeviceSwap - no device manager (null renderer must be running)"));
		}

		return true;
	}
	
	bool FAudioMixerWasapi::PostDeviceSwap()
	{
		// Once the context is handed off to the device swap routine (either async or synchronous),
		// it should no longer be valid.
		check(!DeviceSwapContext.IsValid());
		bool bDidSucceed = false;

		FWasapiDeviceSwapResult* DeviceSwapResult = StaticCast<FWasapiDeviceSwapResult*>(GetDeviceSwapResult());
			
		if (DeviceSwapResult && DeviceSwapResult->IsNewDeviceReady())
		{
			SCOPED_NAMED_EVENT(FAudioMixerWasapiFAudioMixerWasapi_CheckThreadedDeviceSwap_EndSwap, FColor::Blue);

			FScopeLock Lock(&DeviceSwapCriticalSection);

			// Copy our new Device Info into our active one.
			AudioStreamInfo.DeviceInfo = DeviceSwapResult->DeviceInfo;

			// Set the current device name
			if (DeviceSwapResult->bIsAggregateDevice)
			{
				CurrentDeviceName = ExtractAggregateDeviceName(AudioStreamInfo.DeviceInfo.Name);
			}
			else
			{
				CurrentDeviceName = AudioStreamInfo.DeviceInfo.Name;
			}

			// Display our new XAudio2 Mastering voice details.
			UE_LOG(LogAudioMixer, Display, TEXT("FAudioMixerWasapi::PostDeviceSwap - successful Swap new Device is (NumChannels=%u, SampleRate=%u, DeviceID=%s, Name=%s), Reason=%s, InstanceID=%d, DurationMS=%.2f"),
				(uint32)AudioStreamInfo.DeviceInfo.NumChannels, (uint32)AudioStreamInfo.DeviceInfo.SampleRate, *AudioStreamInfo.DeviceInfo.DeviceId, *AudioStreamInfo.DeviceInfo.Name,
				*DeviceSwapResult->SwapReason, InstanceID, DeviceSwapResult->SuccessfulDurationMs);

			// Reinitialize the output circular buffer to match the buffer math of the new audio device.
			const int32 NumOutputSamples = AudioStreamInfo.NumOutputFrames * AudioStreamInfo.DeviceInfo.NumChannels;
			if (ensure(NumOutputSamples > 0))
			{
				OutputBuffer.Init(AudioStreamInfo.AudioMixer, NumOutputSamples, NumOutputBuffers, AudioStreamInfo.DeviceInfo.Format);
			}

			check(!DeviceManager);
			DeviceManager = MoveTemp(DeviceSwapResult->NewDeviceManager);

			bDidSucceed = true;
		}
		else
		{
			if (!DeviceSwapResult)
			{
				UE_LOG(LogAudioMixer, Error, TEXT("FAudioMixerWasapi::PostDeviceSwap - null device swap result!)"));
			}
			else
			{
				UE_LOG(LogAudioMixer, Error, TEXT("FAudioMixerWasapi::PostDeviceSwap - DeviceSwapResult->IsNewDeviceReady() = %d)"), DeviceSwapResult->IsNewDeviceReady());
			}
		}
		
		ResetActiveDeviceSwapFuture();

		return bDidSucceed;
	}
	
}
