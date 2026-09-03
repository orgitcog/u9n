// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AudioMixer.h"	
#include "AudioMixerWasapiRenderStream.h"
#include "IAudioMixerWasapiDeviceManager.h"
#include "Templates/SharedPointer.h"
#include "Templates/UniquePtr.h"

#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/AllowWindowsPlatformAtomics.h"

#include "Microsoft/COMPointer.h"

THIRD_PARTY_INCLUDES_START
#include <mmdeviceapi.h>			// IMMNotificationClient
#include <audiopolicy.h>			// IAudioSessionEvents
THIRD_PARTY_INCLUDES_END

#include "Windows/HideWindowsPlatformAtomics.h"
#include "Windows/HideWindowsPlatformTypes.h"

namespace Audio
{
	class FWasapiDefaultRenderStream;

	struct FWasapiDeviceSwapContext : public FDeviceSwapContext
	{
		FWasapiDeviceSwapContext() = delete;
		FWasapiDeviceSwapContext(const FString& InRequestedDeviceID, const FString& InReason) :
			FDeviceSwapContext(InRequestedDeviceID, InReason)
		{}
		
		FAudioPlatformSettings PlatformSettings;
		TArray<FWasapiRenderStreamParams> StreamParams;
		TFunction<void()> ReadNextBufferCallback;
		TUniquePtr<IAudioMixerWasapiDeviceManager> OldDeviceManager;
		bool bIsAggregateDevice = false;
	};

	struct FWasapiDeviceSwapResult : public FDeviceSwapResult
	{
		virtual bool IsNewDeviceReady() const override
		{
			return NewDeviceManager.IsValid();
		}

		TUniquePtr<IAudioMixerWasapiDeviceManager> NewDeviceManager;
		bool bIsAggregateDevice = false;
	};

	/**
	 * FAudioMixerWasapi - Wasapi audio backend for Windows and Xbox
	 */
	class FAudioMixerWasapi : public FAudioMixerPlatformSwappable
	{
	public:

		FAudioMixerWasapi();
		virtual ~FAudioMixerWasapi() override;

		//~ Begin IAudioMixerPlatformInterface
		virtual FString GetPlatformApi() const override { return TEXT("WASAPIMixer"); }
		virtual bool InitializeHardware() override;
		virtual bool TeardownHardware() override;
		virtual bool IsInitialized() const override;
		virtual int32 GetNumFrames(const int32 InNumRequestedFrames) override;
		virtual bool GetNumOutputDevices(uint32& OutNumOutputDevices) override;
		virtual bool GetOutputDeviceInfo(const uint32 InDeviceIndex, FAudioPlatformDeviceInfo& OutInfo) override;
		virtual bool GetDefaultOutputDeviceIndex(uint32& OutDefaultDeviceIndex) const override;
		virtual bool OpenAudioStream(const FAudioMixerOpenStreamParams& Params) override;
		virtual bool CloseAudioStream() override;
		virtual bool StartAudioStream() override;
		virtual bool StopAudioStream() override;
		virtual FAudioPlatformDeviceInfo GetPlatformDeviceInfo() const override;
		virtual void SubmitBuffer(const uint8* Buffer) override;
		virtual void SubmitDirectOutBuffer(const int32 InDirectOutIndex, const FAlignedFloatBuffer& InBuffer) override;
		virtual bool DisablePCMAudioCaching() const override { return true; }
		virtual FString GetDefaultDeviceName() override;
		virtual FAudioPlatformSettings GetPlatformSettings() const override;
		virtual IAudioPlatformDeviceInfoCache* GetDeviceInfoCache() const override;
		virtual bool IsDeviceInfoValid(const FAudioPlatformDeviceInfo& InDeviceInfo) const override;
		virtual bool ShouldUseDeviceInfoCache() const override { return true; }
		//~ End IAudioMixerPlatformInterface

		//~ Begin IAudioMixerDeviceChangedListener
		virtual void RegisterDeviceChangedListener() override;
		virtual void UnregisterDeviceChangedListener() override;
		virtual void OnDefaultCaptureDeviceChanged(const EAudioDeviceRole InAudioDeviceRole, const FString& DeviceId) override;
		virtual void OnDefaultRenderDeviceChanged(const EAudioDeviceRole InAudioDeviceRole, const FString& DeviceId) override;
		virtual void OnDeviceAdded(const FString& DeviceId, bool bIsRenderDevice) override;
		virtual void OnDeviceRemoved(const FString& DeviceId, bool bIsRenderDevice) override;
		virtual void OnDeviceStateChanged(const FString& DeviceId, const EAudioDeviceState InState, bool bIsRenderDevice) override;
		virtual void OnSessionDisconnect(Audio::IAudioMixerDeviceChangedListener::EDisconnectReason InReason) override;
		virtual FString GetDeviceId() const override;
		//~ End IAudioMixerDeviceChangedListener

		//~ Begin FAudioMixerPlatformSwappable
		virtual bool InitializeDeviceSwapContext(const FString& InRequestedDeviceID, const TCHAR* InReason) override;
		virtual bool CheckThreadedDeviceSwap() override;
		virtual bool PreDeviceSwap() override;
		virtual void EnqueueAsyncDeviceSwap() override;
		virtual void SynchronousDeviceSwap() override;
		virtual bool PostDeviceSwap() override;
		//~ End FAudioMixerPlatformSwappable

	protected:
		/** Can be used by subclasses to initialize a device swap context by supplying a specific 
		 *  FAudioPlatformDeviceInfo rather than looking it up via the requested device Id.
		 */
		bool InitDeviceSwapContextInternal(const FString& InRequestedDeviceID, const TCHAR* InReason, const TOptional<FAudioPlatformDeviceInfo>& InDeviceInfo);
		
	private:

		/** Cache for holding information about MM audio devices (IMMDevice).  */
		TUniquePtr<IAudioPlatformDeviceInfoCache> DeviceInfoCache;
		
		/** Manages either a single, default device or an aggregate of several devices belonging to the same hardware */
		TUniquePtr<IAudioMixerWasapiDeviceManager> DeviceManager;

		/** Indicates if this object has been successfully initialized. */
		bool bIsInitialized = false;

		/** Device swap context which holds necessary data required to perform a device wap. */
		TUniquePtr<FWasapiDeviceSwapContext> DeviceSwapContext;
		
		/** Fetches an IMMDevice with the given ID. */
		TComPtr<IMMDevice> GetMMDevice(const FString& InDeviceID) const;

		/** Extracts the hardware device name from a logical device name. The OS
		 *  places the hardware name in parentheses at the end of the string).
		 */
		FString ExtractAggregateDeviceName(const FString& InDeviceID) const;
		
		/** Device manager factory */
		static void CreateDeviceManager(const bool bInUseAggregateDevice, TUniquePtr<IAudioMixerWasapiDeviceManager>& InDeviceManager);
		
		/** Initializes a Wasapi stream parameters struct with the give values. */
		bool InitStreamParams(const uint32 InDeviceIndex, const int32 InNumBufferFrames, const int32 InNumBuffers, const int32 InSampleRate, TArray<FWasapiRenderStreamParams>& OutParams);
		bool InitStreamParams(const FAudioPlatformDeviceInfo& InDeviceInfo, const int32 InNumBufferFrames, const int32 InNumBuffers, const int32 InSampleRate, TArray<FWasapiRenderStreamParams>& OutParams) const;
		
		/** Performs a device swap with the given context. Static method enforces no other state sharing occurs. */
		static TUniquePtr<FDeviceSwapResult> PerformDeviceSwap(TUniquePtr<FWasapiDeviceSwapContext>&& InDeviceContext);
	};

 }
