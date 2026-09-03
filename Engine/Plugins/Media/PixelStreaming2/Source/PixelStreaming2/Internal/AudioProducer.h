// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Audio.h"
#include "ISubmixBufferListener.h"
#include "IPixelStreaming2AudioProducer.h"

namespace UE::PixelStreaming2
{
	class FPatchInputProxy;

	/**
	 * An audio input capable of listening to UE submix's as well as receiving user audio via the PushAudio method.
	 * Any received audio will be passed into the Parent's PushAudio method
	 */
	class PIXELSTREAMING2_API FAudioProducer : public ISubmixBufferListener, public IPixelStreaming2AudioProducer
	{
	public:
		static TSharedPtr<FAudioProducer> Create(Audio::FDeviceId AudioDeviceId, TSharedPtr<FPatchInputProxy> PatchInput);
		static TSharedPtr<FAudioProducer> Create(TSharedPtr<FPatchInputProxy> PatchInput);
		virtual ~FAudioProducer() = default;

		// For users to manually push non-submix audio into
		virtual void PushAudio(const float* AudioData, int32 NumSamples, int32 NumChannels, int32 SampleRate) override;

		// ISubmixBufferListener interface
		virtual void OnNewSubmixBuffer(const USoundSubmix* OwningSubmix, float* AudioData, int32 NumSamples, int32 NumChannels, const int32 SampleRate, double AudioClock) override;

		void ToggleMuted() { bIsMuted = !bIsMuted; }

	protected:
		FAudioProducer(TSharedPtr<FPatchInputProxy> PatchInput);

	protected:
		TSharedPtr<FPatchInputProxy> PatchInput;
		bool						 bIsMuted = false;
	};
} // namespace UE::PixelStreaming2