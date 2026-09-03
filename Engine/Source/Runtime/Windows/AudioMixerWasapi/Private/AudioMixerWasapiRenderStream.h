// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AudioMixer.h"	
#include "IAudioMixerWasapiDeviceManager.h"
#include "WasapiAudioFormat.h"

#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/AllowWindowsPlatformAtomics.h"

#include "Microsoft/COMPointer.h"

THIRD_PARTY_INCLUDES_START
#include <mmdeviceapi.h>			// IMMNotificationClient
#include <audiopolicy.h>			// IAudioSessionEvents
THIRD_PARTY_INCLUDES_END

#include "Windows/HideWindowsPlatformAtomics.h"
#include "Windows/HideWindowsPlatformTypes.h"

DECLARE_DELEGATE(FAudioMixerReadNextBufferDelegate);

namespace Audio
{
	/**
	 * FAudioMixerWasapiRenderStream
	 */
	class FAudioMixerWasapiRenderStream : public IDeviceRenderCallback
	{
	public:

		FAudioMixerWasapiRenderStream();
		virtual ~FAudioMixerWasapiRenderStream();

		virtual bool InitializeHardware(const FWasapiRenderStreamParams& InParams);
		virtual bool TeardownHardware();
		virtual bool IsInitialized() const;
		virtual int32 GetNumFrames(const int32 InNumRequestedFrames) const;
		virtual bool OpenAudioStream(const FWasapiRenderStreamParams& InParams, HANDLE InEventHandle);
		virtual bool CloseAudioStream();
		virtual bool StartAudioStream();
		virtual bool StopAudioStream();
		virtual void SubmitBuffer(const uint8* Buffer, const SIZE_T InNumFrames) { }
		virtual void SubmitDirectOutBuffer(const int32 InChannelIndex, const FAlignedFloatBuffer& InBuffer) { }

		static uint32 GetMinimumBufferSize(const uint32 InSampleRate);

	protected:
		
		/** COM pointer to the WASAPI audio client object. */
		TComPtr<IAudioClient3> AudioClient;

		/** COM pointer to the WASAPI render client object. */
		TComPtr<IAudioRenderClient> RenderClient;

		/** Holds the audio format configuration for this stream. */
		FWasapiAudioFormat AudioFormat;

		/** Indicates if this object has been successfully initialized. */
		bool bIsInitialized = false;

		/** The state of the output audio stream. */
		EAudioOutputStreamState::Type StreamState = EAudioOutputStreamState::Closed;

		/** Render output device info. */
		FWasapiRenderStreamParams RenderStreamParams;

		/** The default callback period for this WASAPI render device. */
		uint32 DefaultDevicePeriod = 0;

		/** Number of frames of audio data which will be used for each audio callback. This value is 
		    determined by the WASAPI audio client and can be equal or greater than the number of frames requested. */
		uint32 NumFramesPerDeviceBuffer = 0;

		/** Accumulates errors that occur in the audio callback. */
		uint32 CallbackBufferErrors = 0;
	};
}
