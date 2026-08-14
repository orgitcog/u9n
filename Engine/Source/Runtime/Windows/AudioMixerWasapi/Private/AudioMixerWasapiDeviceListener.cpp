// Copyright Epic Games, Inc. All Rights Reserved.

#include "AudioMixerWasapi.h"
#include "AudioMixer.h"
#include "AudioDeviceNotificationSubsystem.h"
#include "Misc/ScopeRWLock.h"

#include <atomic>

#include "AudioDeviceManager.h"
#include "Microsoft/COMPointer.h"
#include "ScopedCom.h"					// FScopedComString

#include "Windows/AllowWindowsPlatformTypes.h"
THIRD_PARTY_INCLUDES_START
// Linkage for  Windows GUIDs included by Notification/DeviceInfoCache, otherwise they are extern.
#include <initguid.h>
THIRD_PARTY_INCLUDES_END
#include "Windows/HideWindowsPlatformTypes.h"

#include "WindowsMMNotificationClient.h"
#include "WindowsMMDeviceInfoCache.h"
#include "WindowsMMStringUtils.h"

namespace Audio
{	
	TSharedPtr<FWindowsMMNotificationClient> WasapiWinNotificationClient;

	void FAudioMixerWasapi::RegisterDeviceChangedListener()
	{
		if (!WasapiWinNotificationClient.IsValid())
		{
			// Shared (This is a COM object, so we don't delete it, just decrement the ref counter).
			WasapiWinNotificationClient = TSharedPtr<FWindowsMMNotificationClient>(
				new FWindowsMMNotificationClient, 
				[](FWindowsMMNotificationClient* InPtr) { InPtr->ReleaseClient(); }
			);
		}
		if (!DeviceInfoCache.IsValid())
		{
			// Wasapi backend supports aggregate devices, provided it is enabled in FAudioDeviceManager
			bool bIsAggregateDeviceSupported = FAudioDeviceManager::IsAggregateDeviceSupportEnabled();

			// Setup device info cache.
			DeviceInfoCache = MakeUnique<FWindowsMMDeviceCache>(bIsAggregateDeviceSupported);
			WasapiWinNotificationClient->RegisterDeviceChangedListener(static_cast<FWindowsMMDeviceCache*>(DeviceInfoCache.Get()));
		}

		WasapiWinNotificationClient->RegisterDeviceChangedListener(this);
	}

	void FAudioMixerWasapi::UnregisterDeviceChangedListener()
	{
		if (WasapiWinNotificationClient.IsValid())
		{
			if (DeviceInfoCache.IsValid())
			{
				// Unregister and kill cache.
				WasapiWinNotificationClient->UnRegisterDeviceDeviceChangedListener(static_cast<FWindowsMMDeviceCache*>(DeviceInfoCache.Get()));
				
				DeviceInfoCache.Reset();
			}
			
			WasapiWinNotificationClient->UnRegisterDeviceDeviceChangedListener(this);
		}
	}

	void FAudioMixerWasapi::OnDefaultCaptureDeviceChanged(const EAudioDeviceRole InAudioDeviceRole, const FString& DeviceId)
	{
		if (UAudioDeviceNotificationSubsystem* AudioDeviceNotifySubsystem = UAudioDeviceNotificationSubsystem::Get())
		{
			AudioDeviceNotifySubsystem->OnDefaultCaptureDeviceChanged(InAudioDeviceRole, DeviceId);
		}
	}

	void FAudioMixerWasapi::OnDefaultRenderDeviceChanged(const EAudioDeviceRole InAudioDeviceRole, const FString& DeviceId)
	{		
		// There's 3 defaults in windows (communications, console, multimedia). These technically can all be different devices.		
		// However, the Windows UX only allows console+multimedia to be toggle as a pair. This means you get two notifications
		// for default device changing typically. To prevent a trouble trigger we only listen to "Console" here. For more information on 
		// device roles: https://docs.microsoft.com/en-us/windows/win32/coreaudio/device-roles
		
		if (InAudioDeviceRole == EAudioDeviceRole::Console)
		{
			UE_LOG(LogAudioMixer, Display, TEXT("FAudioMixerWasapi: Changing default audio render device to new device: Role=%s, DeviceName=%s, InstanceID=%d"), 
				Audio::ToString(InAudioDeviceRole), *WasapiWinNotificationClient->GetFriendlyName(DeviceId), InstanceID);

			// Ignore if not listening for events. This means the user has specified a specific device
			// to use and is not interested in following the current system default device.
			if (GetIsListeningForDeviceEvents())
			{
				RequestDeviceSwap(DeviceId, /* force */true, TEXT("FAudioMixerWasapi::OnDefaultRenderDeviceChanged"));
			}
		}

		if (UAudioDeviceNotificationSubsystem* AudioDeviceNotifSubsystem = UAudioDeviceNotificationSubsystem::Get())
		{
			AudioDeviceNotifSubsystem->OnDefaultRenderDeviceChanged(InAudioDeviceRole, DeviceId);
		}
	}

	void FAudioMixerWasapi::OnDeviceAdded(const FString& DeviceId, bool bIsRenderDevice)
	{
		// Ignore changes in capture device.
		if (!bIsRenderDevice)
		{
			return;
		}
		
		// If the device that was added is our original device and our current device is NOT our original device, 
		// move our audio stream to this newly added device.
		const FString AudioDeviceId = GetOriginalAudioDeviceId();
		if (AudioStreamInfo.DeviceInfo.DeviceId != AudioDeviceId && DeviceId == AudioDeviceId)
		{
			UE_LOG(LogAudioMixer, Display, TEXT("FAudioMixerWasapi: Original audio device re-added. Moving audio back to original audio device: DeviceName=%s, bRenderDevice=%d, InstanceID=%d"), 
				*WasapiWinNotificationClient->GetFriendlyName(*AudioDeviceId), (int32)bIsRenderDevice, InstanceID);

			// Ignore if not listening for events. This means the user has specified a specific device
			// to use and is not interested in following the current system default device.
			if (GetIsListeningForDeviceEvents())
			{
				RequestDeviceSwap(AudioDeviceId, /*force */ true, TEXT("FAudioMixerWasapi::OnDeviceAdded"));
			}
		}

		if (UAudioDeviceNotificationSubsystem* AudioDeviceNotifSubsystem = UAudioDeviceNotificationSubsystem::Get())
		{
			AudioDeviceNotifSubsystem->OnDeviceAdded(DeviceId, bIsRenderDevice);
		}
	}

	void FAudioMixerWasapi::OnDeviceRemoved(const FString& DeviceId, bool bIsRenderDevice)
	{
		// Ignore changes in capture device.
		if (!bIsRenderDevice)
		{
			return;
		}
		
		// If the device we're currently using was removed... then switch to the new default audio device.
		if (AudioStreamInfo.DeviceInfo.DeviceId == DeviceId)
		{
			UE_LOG(LogAudioMixer, Warning, TEXT("FAudioMixerWasapi: Audio device removed [%s], falling back to other windows default device. bIsRenderDevice=%d, InstanceID=%d"), 
				*WasapiWinNotificationClient->GetFriendlyName(DeviceId), (int32)bIsRenderDevice, InstanceID);

			// Ignore if not listening for events. This means the user has specified a specific device
			// to use and is not interested in following the current system default device.
			if (GetIsListeningForDeviceEvents())
			{
				RequestDeviceSwap(TEXT(""), /* force */ true, TEXT("FAudioMixerWasapi::OnDeviceRemoved"));
			}
		}

		if (UAudioDeviceNotificationSubsystem* AudioDeviceNotifySubsystem = UAudioDeviceNotificationSubsystem::Get())
		{
			AudioDeviceNotifySubsystem->OnDeviceRemoved(DeviceId, bIsRenderDevice);
		}
	}

	void FAudioMixerWasapi::OnDeviceStateChanged(const FString& DeviceId, const EAudioDeviceState InState, bool bIsRenderDevice)
	{
		// Ignore changes in capture device.
		if (!bIsRenderDevice)
		{
			return;
		}

		// If the device we're currently using was removed and it's not the system default... then switch to the new default audio device.
		// If it is the system default device, then OnDefaultRenderDeviceChanged() will be called to handle this.
		if (AudioStreamInfo.DeviceInfo.DeviceId == DeviceId && !AudioStreamInfo.DeviceInfo.bIsSystemDefault &&
			(InState == EAudioDeviceState::Disabled || InState == EAudioDeviceState::NotPresent || InState == EAudioDeviceState::Unplugged))
		{
			UE_LOG(LogAudioMixer, Display, TEXT("FAudioMixerWasapi::OnDeviceStateChanged: Audio device not available [%s], falling back to other windows default device. InState=%d, bIsRenderDevice=%d, InstanceID=%d"), 
				*WasapiWinNotificationClient->GetFriendlyName(DeviceId), (int32)InState, (int32)bIsRenderDevice, InstanceID);

			// Ignore if not listening for events. This means the user has specified a specific device
			// to use and is not interested in following the current system default device.
			if (GetIsListeningForDeviceEvents())
			{
				RequestDeviceSwap(TEXT(""), /* force */ true, TEXT("FAudioMixerWasapi::OnDeviceStateChanged"));
			}
		}
		
		if (UAudioDeviceNotificationSubsystem* AudioDeviceNotifySubsystem = UAudioDeviceNotificationSubsystem::Get())
		{
			AudioDeviceNotifySubsystem->OnDeviceStateChanged(DeviceId, InState, bIsRenderDevice);
		}
	}
	
	FString FAudioMixerWasapi::GetDeviceId() const
	{
		return AudioStreamInfo.DeviceInfo.DeviceId;
	}

	TComPtr<IMMDevice> FAudioMixerWasapi::GetMMDevice(const FString& InDeviceID) const
	{
		if (WasapiWinNotificationClient)
		{
			return WasapiWinNotificationClient->GetDevice(InDeviceID);
		}

		return TComPtr<IMMDevice>();
	}

	FString FAudioMixerWasapi::ExtractAggregateDeviceName(const FString& InName) const
	{
		return FWindowsMMDeviceCache::ExtractAggregateDeviceName(InName);
	}
}

