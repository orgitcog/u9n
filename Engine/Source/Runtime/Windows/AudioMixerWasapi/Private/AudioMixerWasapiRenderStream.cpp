// Copyright Epic Games, Inc. All Rights Reserved.

#include "AudioMixerWasapiRenderStream.h"

#include "AudioMixerWasapiLog.h"
#include "HAL/IConsoleManager.h"
#include "WasapiAudioUtils.h"
#include "WindowsMMStringUtils.h"

static int32 UseDefaultQualitySRC_CVar = 0;
FAutoConsoleVariableRef CVarUseDefaultQualitySRC(
	TEXT("au.Wasapi.UseDefaultQualitySRC"),
	UseDefaultQualitySRC_CVar,
	TEXT("Enable Wasapi default SRC quality.\n")
	TEXT("0: Not Enabled, 1: Enabled"),
	ECVF_Default);

namespace Audio
{
	uint32 FAudioMixerWasapiRenderStream::GetMinimumBufferSize(const uint32 InSampleRate)
	{
		// Can be called prior to InitializeHardware
		// Makes assumption about minimum buffer size which we verify in InitializeHardware
		return InSampleRate / 100;
	}

	FAudioMixerWasapiRenderStream::FAudioMixerWasapiRenderStream()
	{
	}

	FAudioMixerWasapiRenderStream::~FAudioMixerWasapiRenderStream()
	{
	}

	bool FAudioMixerWasapiRenderStream::InitializeHardware(const FWasapiRenderStreamParams& InParams)
	{
		TComPtr<IMMDevice> MMDevice = InParams.MMDevice;

		if (!MMDevice)
		{
			UE_LOG(LogAudioMixerWasapi, Error, TEXT("InitializeHardware null MMDevice"));
			return false;
		}

		TComPtr<IAudioClient3> TempAudioClient;
		HRESULT Result = MMDevice->Activate(__uuidof(IAudioClient3), CLSCTX_INPROC_SERVER, nullptr, IID_PPV_ARGS_Helper(&TempAudioClient));
		if (FAILED(Result))
		{
			UE_LOG(LogAudioMixerWasapi, Error, TEXT("OpenAudioStream failed IMMDevice::Activate %s"), *AudioClientErrorToFString(Result));
			return false;
		}
		
		WAVEFORMATEX* MixFormat = nullptr;
		Result = TempAudioClient->GetMixFormat(&MixFormat);
		if (FAILED(Result) || !MixFormat)
		{
			UE_LOG(LogAudioMixerWasapi, Error, TEXT("OpenAudioStream failed IAudioClient3::GetMixFormat MixFormat: 0x%llx Result: %s"), (uint64)MixFormat, *AudioClientErrorToFString(Result));
			return false;
		}

		FWasapiAudioFormat StreamFormat(FMath::Min<int32>(MixFormat->nChannels, AUDIO_MIXER_MAX_OUTPUT_CHANNELS), InParams.SampleRate, EWasapiAudioEncoding::FLOATING_POINT_32);

		if (MixFormat)
		{
			::CoTaskMemFree(MixFormat);
			MixFormat = nullptr;
		}

		REFERENCE_TIME DevicePeriodRefTime = 0;
		// The second param to GetDevicePeriod is only valid for exclusive mode
		// Note that GetDevicePeriod returns ref time which is sample rate agnostic
		// In testing, IAudioClient3::GetSharedModeEnginePeriod() appears to return the same value as
		// IAudioClient::GetDevicePeriod() so we use the older API.
		Result = TempAudioClient->GetDevicePeriod(&DevicePeriodRefTime, nullptr);
		if (FAILED(Result))
		{
			UE_LOG(LogAudioMixerWasapi, Error, TEXT("OpenAudioStream failed IAudioClient3::GetDevicePeriod %s"), *AudioClientErrorToFString(Result));
			return false;
		}

		DefaultDevicePeriod = FWasapiAudioUtils::RefTimeToFrames(DevicePeriodRefTime, InParams.SampleRate);
		if (DefaultDevicePeriod == 0)
		{
			UE_LOG(LogAudioMixerWasapi, Error, TEXT("OpenAudioStream failed DefaultDevicePeriod = %d"), DefaultDevicePeriod);
			return false;
		}

		// Verify that our assumption about the minimum/default buffer size is correct
		check(DefaultDevicePeriod == GetMinimumBufferSize(InParams.SampleRate));
		
		// Determine buffer size to use. 
		uint32 BufferFramesToRequest = FMath::Max<uint32>(InParams.NumFrames, DefaultDevicePeriod);
		
		// If the engine buffer size is not an integral multiple of the device period then we must
		// account for buffer phasing by padding the requested buffer size.
		if (BufferFramesToRequest % DefaultDevicePeriod != 0)
		{
			// Round up to nearest multiple of the device period
			uint32 Multiple = FMath::CeilToInt32(static_cast<float>(BufferFramesToRequest + DefaultDevicePeriod - 1) / DefaultDevicePeriod);
			BufferFramesToRequest = DefaultDevicePeriod * Multiple;
		}
		REFERENCE_TIME DesiredBufferDuration = FWasapiAudioUtils::FramesToRefTime(BufferFramesToRequest, InParams.SampleRate);
		
		// For shared mode, this is required to be zero
		constexpr REFERENCE_TIME Periodicity = 0;

		// Audio events will be delivered to us rather than needing to poll
		uint32 Flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK;

		if (InParams.SampleRate != InParams.HardwareDeviceInfo.SampleRate)
		{
			Flags |= AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM;
			if (UseDefaultQualitySRC_CVar)
			{
				Flags |= AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;
			}
			
			UE_LOG(LogAudioMixerWasapi, Warning, TEXT("Sample rate mismatch. Engine sample rate: %d Device sample rate: %d"), InParams.SampleRate, InParams.HardwareDeviceInfo.SampleRate);
			UE_LOG(LogAudioMixerWasapi, Warning, TEXT("Device level sample rate conversion will be used."));
		}

		Result = TempAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, Flags, DesiredBufferDuration, Periodicity, StreamFormat.GetWaveFormat(), nullptr);
		if (FAILED(Result))
		{
			UE_LOG(LogAudioMixerWasapi, Error, TEXT("OpenAudioStream failed IAudioClient3::Initialize %s"), *AudioClientErrorToFString(Result));
			return false;
		}

		Result = TempAudioClient->GetBufferSize(&NumFramesPerDeviceBuffer);
		if (FAILED(Result))
		{
			UE_LOG(LogAudioMixerWasapi, Error, TEXT("OpenAudioStream failed IAudioClient3::GetBufferSize %s"), *AudioClientErrorToFString(Result));
			return false;
		}

		AudioClient = MoveTemp(TempAudioClient);
		AudioFormat = StreamFormat;
		RenderStreamParams = InParams;

		bIsInitialized = true;

		UE_LOG(LogAudioMixerWasapi, Verbose, TEXT("FAudioMixerWasapiRenderStream::InitializeHardware succeeded with sample rate: %d, buffer period: %d"), InParams.SampleRate, InParams.NumFrames);

		return true;
	}

	bool FAudioMixerWasapiRenderStream::TeardownHardware()
	{
		if (!bIsInitialized)
		{
			UE_LOG(LogAudioMixerWasapi, Warning, TEXT("FAudioMixerWasapiRenderStream::TeardownHardware failed...not initialized. "));
			return false;
		}

		RenderClient.Reset();
		AudioClient.Reset();

		bIsInitialized = false;

		UE_LOG(LogAudioMixerWasapi, Verbose, TEXT("FAudioMixerWasapiRenderStream::TeardownHardware succeeded"));

		return true;
	}

	bool FAudioMixerWasapiRenderStream::IsInitialized() const
	{
		return bIsInitialized;
	}

	int32 FAudioMixerWasapiRenderStream::GetNumFrames(const int32 InNumRequestedFrames) const
	{
		return InNumRequestedFrames;
	}

	bool FAudioMixerWasapiRenderStream::OpenAudioStream(const FWasapiRenderStreamParams& InParams, HANDLE InEventHandle)
	{
		if (InParams.HardwareDeviceInfo.DeviceId != RenderStreamParams.HardwareDeviceInfo.DeviceId)
		{
			if (!InitializeHardware(InParams))
			{
				UE_LOG(LogAudioMixerWasapi, Error, TEXT("OpenAudioStream failed InitAudioClient"));
				return false;
			}
		}

		if (InEventHandle == nullptr)
		{
			UE_LOG(LogAudioMixerWasapi, Error, TEXT("OpenAudioStream null EventHandle"));
			return false;
		}

		HRESULT Result = AudioClient->SetEventHandle(InEventHandle);
		if (FAILED(Result))
		{
			UE_LOG(LogAudioMixerWasapi, Error, TEXT("OpenAudioStream failed IAudioClient3::SetEventHandle %s"), *AudioClientErrorToFString(Result));
			return false;
		}

		TComPtr<IAudioRenderClient> TempRenderClient;
		Result = AudioClient->GetService(__uuidof(IAudioRenderClient), IID_PPV_ARGS_Helper(&TempRenderClient));
		if (FAILED(Result))
		{
			UE_LOG(LogAudioMixerWasapi, Error, TEXT("OpenAudioStream failed IAudioClient3::GetService IAudioRenderClient %s"), *AudioClientErrorToFString(Result));
			return false;
		}

		RenderClient = MoveTemp(TempRenderClient);
		bIsInitialized = true;

		UE_LOG(LogAudioMixerWasapi, Verbose, TEXT("FAudioMixerWasapiRenderStream::OpenAudioStream succeeded with SampeRate: %d, NumFrames: %d"), InParams.SampleRate, InParams.NumFrames);

		return true;
	}

	bool FAudioMixerWasapiRenderStream::CloseAudioStream()
	{
		if (!bIsInitialized || StreamState == EAudioOutputStreamState::Closed)
		{
			UE_LOG(LogAudioMixerWasapi, Verbose, TEXT("FAudioMixerWasapiRenderStream::CloseAudioStream stream appears to be already closed"));

			return false;
		}

		if (StreamState == EAudioOutputStreamState::Running)
		{
			UE_LOG(LogAudioMixerWasapi, Verbose, TEXT("FAudioMixerWasapiRenderStream::CloseAudioStream stream appears to be running. StopAudioStream() must be called prior to closing."));

			return false;
		}

		StreamState = EAudioOutputStreamState::Closed;

		return true;
	}

	bool FAudioMixerWasapiRenderStream::StartAudioStream()
	{
		if (bIsInitialized)
		{
			StreamState = EAudioOutputStreamState::Running;

			if (!AudioClient.IsValid())
			{
				UE_LOG(LogAudioMixerWasapi, Error, TEXT("StartAudioStream failed invalid audio client"));
				return false;
			}

			AudioClient->Start();
		}

		UE_LOG(LogAudioMixerWasapi, Verbose, TEXT("FAudioMixerWasapiRenderStream::StartAudioStream stream started"));

		return true;
	}

	bool FAudioMixerWasapiRenderStream::StopAudioStream()
	{
		if (!bIsInitialized)
		{
			UE_LOG(LogAudioMixerWasapi, Error, TEXT("FAudioMixerWasapiRenderStream::StopAudioStream() not initialized"));
			return false;
		}

		if (StreamState != EAudioOutputStreamState::Stopped && StreamState != EAudioOutputStreamState::Closed)
		{
			if (AudioClient.IsValid())
			{
				AudioClient->Stop();
			}

			StreamState = EAudioOutputStreamState::Stopped;
		}

		if (CallbackBufferErrors > 0)
		{
			UE_LOG(LogAudioMixerWasapi, Display, TEXT("FAudioMixerWasapiRenderStream::StopAudioStream render stream reported %d callback buffer errors (can be normal if preceded by device swap)."), CallbackBufferErrors);
			CallbackBufferErrors = 0;
		}

		return true;
	}
}
