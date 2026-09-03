// Copyright Epic Games, Inc. All Rights Reserved.

#include "Generators/SoundWaveScrubber.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

namespace Audio
{ 

FSoundWaveScrubber::FSoundWaveScrubber()
	: CurrentPlayheadTimeSeconds(0.0f)
	, SRC(Audio::ISampleRateConverter::CreateSampleRateConverter())
	, GrainDurationSeconds(0.0f)
{
	// Generate the grain envelope data
	Audio::Grain::GenerateEnvelopeData(GrainEnvelope, 512, Audio::Grain::EEnvelope::Hann);
}

FSoundWaveScrubber::~FSoundWaveScrubber()
{
}

void FSoundWaveScrubber::Init(FSoundWaveProxyPtr InSoundWaveProxyPtr, float InSampleRate, int32 InNumChannels, float InPlayheadTimeSeconds)
{
	check(InSoundWaveProxyPtr.IsValid());
	SoundWaveProxyPtr = InSoundWaveProxyPtr;

	AudioMixerSampleRate = InSampleRate;
	SourceFileSampleRate = SoundWaveProxyPtr->GetSampleRate();
	SourceFileDurationSeconds = SoundWaveProxyPtr->GetDuration();
	NumChannels = InNumChannels;

	TargetGrainDurationRange = { 0.4f, 0.05f };
	GrainDurationRange = TargetGrainDurationRange;
	GrainDurationSeconds = TargetGrainDurationRange.X;

	CurrentPlayheadTimeSeconds.Set(InPlayheadTimeSeconds, 0.0f);
	TargetPlayheadTimeSeconds = CurrentPlayheadTimeSeconds.GetValue();

	// We need 3 slots for decoded data. 2 for the grain playback, 1 to decode new chunks while grains are playing.
	DecodedChunks.Reset();
	DecodedChunks.AddDefaulted(3);

	float DecoderSeekTimeSeconds = FMath::Max(CurrentPlayheadTimeSeconds.GetValue() - 0.5f * DecodedAudioSizeInSeconds, 0.0f);

	DecodeToDataChunk(DecodedChunks[0], DecoderSeekTimeSeconds);
}

int32 FSoundWaveScrubber::GetDecodedDataChunkIndexForCurrentReadIndex(int32 InReadFrameIndex)
{
	for (int32 i = 0; i < DecodedChunks.Num(); ++i)
	{
		const FDecodedDataChunk& DecodedChunk = DecodedChunks[i];
		if (DecodedChunk.PCMAudio.Num() > 0)
		{
			int32 DecodedFrameCount = DecodedChunk.PCMAudio.Num() / NumChannels;
			if (InReadFrameIndex >= DecodedChunk.FrameStart && InReadFrameIndex < DecodedChunk.FrameStart + DecodedFrameCount)
			{
				// We found a decoded audio chunk that contains the desired read frame index
				return i;
			}
		}
	}
	return INDEX_NONE;
}

int32 FSoundWaveScrubber::DecodeDataChunkIndexForCurrentReadIndex(int32 InReadFrameIndex)
{
	// No decoded chunk was found for desired read frame index. 
	// This indicates that we need to decode more audio.
	for (int32 i = 0; i < DecodedChunks.Num(); ++i)
	{
		FDecodedDataChunk& DecodedChunk = DecodedChunks[i];
		if (!DecodedChunk.NumGrainsUsingChunk)
		{
			float DecoderSeekTimeSeconds = InReadFrameIndex / AudioMixerSampleRate;
			DecodeToDataChunk(DecodedChunk, DecoderSeekTimeSeconds);
			check(DecodedChunk.PCMAudio.Num() > 0);
			return i;
		}
	}

	FDecodedDataChunk NewChunk;
	float DecoderSeekTimeSeconds = InReadFrameIndex / AudioMixerSampleRate;
	DecodeToDataChunk(NewChunk, DecoderSeekTimeSeconds);
	check(NewChunk.PCMAudio.Num() > 0);
	DecodedChunks.Add(MoveTemp(NewChunk));
	return DecodedChunks.Num() - 1;
}


void FSoundWaveScrubber::DecodeToDataChunk(FDecodedDataChunk& InOutDataChunk, float InDecoderSeekTimeSeconds)
{
	check(InOutDataChunk.NumGrainsUsingChunk == 0);
	check(DecodedAudioSizeInSeconds > 0.0f);
	check(SourceFileSampleRate > 0.0f);
	check(InDecoderSeekTimeSeconds >= 0.0f);
	check(NumChannels > 0);
	check(SoundWaveProxyPtr.IsValid());

	if (!SoundWaveProxyReaderPtr.IsValid())
	{
		// Create the proxy reader, which is our decoder. Initialize it at the initial playhead time
		FSoundWaveProxyReader::FSettings ProxyReaderSettings;
		ProxyReaderSettings.MaxDecodeSizeInFrames = DecodedAudioSizeInSeconds * SourceFileSampleRate;
		ProxyReaderSettings.StartTimeInSeconds = InDecoderSeekTimeSeconds;
		SoundWaveProxyReaderPtr = FSoundWaveProxyReader::Create(SoundWaveProxyPtr.ToSharedRef(), ProxyReaderSettings);
	}
	else
	{
		check(SoundWaveProxyReaderPtr.IsValid());
		float DecoderSeekTimeSeconds = InDecoderSeekTimeSeconds;
		// If proxy reader already exists, then simply seek the decoder to the desired location
		SoundWaveProxyReaderPtr->SeekToTime(DecoderSeekTimeSeconds);
	}

	InOutDataChunk.FrameStart = InDecoderSeekTimeSeconds * AudioMixerSampleRate;

	// We allocate the size of buffer pre-SRC based on source file sample rate
	int32 DecodedAudioSize = DecodedAudioSizeInSeconds * SourceFileSampleRate * NumChannels;
	check(DecodedAudioSize > 0);
	InOutDataChunk.PCMAudio.Reset();
	InOutDataChunk.PCMAudio.AddUninitialized(DecodedAudioSize);

	// This does the actual decoding of the audio to match the size of the input buffer
	SoundWaveProxyReaderPtr->PopAudio(InOutDataChunk.PCMAudio);

	// Check if we need to do SRC. If we do, this will change the allocated 
	// size (potentially expanding or shrinking) to match 
	// the audio mixer sample rate.
	if (!FMath::IsNearlyEqual(SourceFileSampleRate, AudioMixerSampleRate))
	{
		SRC->Init((float)SourceFileSampleRate / AudioMixerSampleRate, NumChannels);
		TArray<float> SampleRateConvertedPCM;
		SRC->ProcessFullbuffer(InOutDataChunk.PCMAudio.GetData(), InOutDataChunk.PCMAudio.Num(), SampleRateConvertedPCM);
		InOutDataChunk.PCMAudio = MoveTemp(SampleRateConvertedPCM);
	}
}

FSoundWaveScrubber::FGrain FSoundWaveScrubber::SpawnGrain()
{
	// Try to retrieve a decoded data chunk for the current read frame based on the curret, interpolated playhead time seconds value
	int32 CurrentReadFrame = CurrentPlayheadTimeSeconds.GetValue() * AudioMixerSampleRate;
	int32 DecodedDataChunkIndex = GetDecodedDataChunkIndexForCurrentReadIndex(CurrentReadFrame);
	if (DecodedDataChunkIndex == INDEX_NONE)
	{
		DecodedDataChunkIndex = DecodeDataChunkIndexForCurrentReadIndex(CurrentReadFrame);
	}
	check(DecodedDataChunkIndex != INDEX_NONE);

	// Get the grain runtime data for the new grain spawn
	FGrain NewGrain;
	NewGrain.CurrentRenderedFramesCount = 0;
	NewGrain.DecodedDataChunkIndex = DecodedDataChunkIndex;
	NewGrain.CurrentReadFrame = CurrentReadFrame;
	NewGrain.GrainDurationFrames = CurrentGrainDurationFrames;
	DecodedChunks[DecodedDataChunkIndex].NumGrainsUsingChunk++;

	GrainCount++;
	NumActiveGrains++;

	return NewGrain;
}

void FSoundWaveScrubber::SetIsScrubbing(bool bInIsScrubbing)
{
	bIsScrubbing = bInIsScrubbing;
}

void FSoundWaveScrubber::SetIsScrubbingWhileStationary(bool bInIsScrubWhileStationary)
{
	bIsScrubbingWhileStationary = bInIsScrubWhileStationary;
}

void FSoundWaveScrubber::SetPlayheadTime(float InPlayheadTimeSeconds)
{
	FScopeLock Lock(&CritSect);

	TargetPlayheadTimeSeconds = FMath::Fmod(FMath::Max(InPlayheadTimeSeconds, 0.0f), SourceFileDurationSeconds);
}

void FSoundWaveScrubber::SetGrainDurationRange(const FVector2D& InGrainDurationRange)
{
	FVector2D GrainDurationRangeClamped =
	{
		FMath::Clamp(InGrainDurationRange.X, 0.05f, 0.5f),
		FMath::Clamp(InGrainDurationRange.Y, 0.05f, 0.5f),
	};

	FScopeLock Lock(&CritSect);
	TargetGrainDurationRange = GrainDurationRangeClamped;
}

int32 FSoundWaveScrubber::RenderAudio(TArrayView<float>& OutAudio)
{
	SCOPED_NAMED_EVENT_TEXT("FSoundWaveScrubber::RenderAudio", FColor::Emerald);

	// Number of frames of this generate audio
	int32 NumFrames = OutAudio.Num() / NumChannels;
	float DeltaTimeSecond = (float)NumFrames / AudioMixerSampleRate;

	float PlayheadTimeDistanceSeconds = 0.0f;

	constexpr float PlayheadLerpTime = 0.2f;

	// Update parameters
	{
		FScopeLock Lock(&CritSect);

		// Update the current playhead time seconds
		if (!FMath::IsNearlyEqual(TargetPlayheadTimeSeconds, CurrentPlayheadTimeSeconds.GetTargetValue()))
		{
			float PlayheadTimeDelta = FMath::Abs(CurrentPlayheadTimeSeconds.GetValue() - TargetPlayheadTimeSeconds);
			// If the playhead time jumps suddenly, we'll instantly set the current playhead to the target
			if (PlayheadTimeDelta > 0.5f)
			{
				CurrentPlayheadTimeSeconds.Set(TargetPlayheadTimeSeconds, 0.0f);
			}
			else
			{
				CurrentPlayheadTimeSeconds.Set(TargetPlayheadTimeSeconds, PlayheadLerpTime);
			}
		}
		float PrevPlayHeadTime = CurrentPlayheadTimeSeconds.GetValue();
		CurrentPlayheadTimeSeconds.Update(DeltaTimeSecond);

		// Check if we're stationary
		if (!bIsScrubbingWhileStationary)
		{
			if (FMath::IsNearlyEqual(PrevPlayHeadTime, CurrentPlayheadTimeSeconds.GetValue(), 0.001f))
			{
				TimeSincePlayheadHasNotChanged += DeltaTimeSecond;
			}
			else
			{
				TimeSincePlayheadHasNotChanged = 0.0f;
			}

			bIsScrubbingDueToBeingStationary = TimeSincePlayheadHasNotChanged < 0.1f;
		}
		else
		{
			bIsScrubbingDueToBeingStationary = true;
		}

		PlayheadTimeDistanceSeconds = FMath::Abs(CurrentPlayheadTimeSeconds.GetTargetValue() - CurrentPlayheadTimeSeconds.GetValue());

		GrainDurationRange = TargetGrainDurationRange;
	}

	// This maps the playhead delta from the target playhead time (which is an indirect measure of playhead velocity) to the duration range
	float MappedGrainDurationRange = FMath::GetMappedRangeValueClamped({ 0.0f, PlayheadLerpTime }, GrainDurationRange, PlayheadTimeDistanceSeconds);
	GrainDurationSeconds = MappedGrainDurationRange;

	// Update the grain duration based on a mapping between the duration range and scrub velocity
	CurrentGrainDurationFrames = GrainDurationSeconds * AudioMixerSampleRate;
	CurrentHalfGrainDurationFrames = 0.5f * CurrentGrainDurationFrames;

	// If we're actively scrubbing we need to spawn grains and render the granular audio
	if (bIsScrubbing && bIsScrubbingDueToBeingStationary)
	{
		if (!ActiveGrains.Num())
		{
			ActiveGrains.Add(SpawnGrain());

			NumFramesTillNextGrainSpawn = CurrentHalfGrainDurationFrames;
		}

		int32 StartRenderFrame = 0;
		int32 NumFramesToRender = FMath::Min(NumFramesTillNextGrainSpawn, NumFrames);
		int32 NumFramesRendered = 0;
		int32 RemainingFrames = NumFrames;

		while (RemainingFrames > 0)
		{
			// This renders the currently active grains (which should only ever max 2 grains)
			// starting from the given start render frame and for the  numbe of indicated frames
			RenderActiveGrains(OutAudio, StartRenderFrame, NumFramesToRender);

			// Update the number of frames rendered this render block
			NumFramesRendered += NumFramesToRender;

			NumFramesTillNextGrainSpawn -= NumFramesToRender;
			check(NumFramesTillNextGrainSpawn >= 0);

			// Determine how many more frames, this block, that we need to render
			RemainingFrames = FMath::Max(NumFrames - NumFramesRendered, 0);

			// Check if we need to spawn a new grain
			if (NumFramesTillNextGrainSpawn == 0)
			{
				// Spawn a new grain with a frame start 
				StartRenderFrame = NumFrames - RemainingFrames;
				ActiveGrains.Add(SpawnGrain());

				// If we still have frames remaining in the buffer, then 
				NumFramesTillNextGrainSpawn = CurrentHalfGrainDurationFrames;

				NumFramesToRender = FMath::Min(NumFramesTillNextGrainSpawn, RemainingFrames);
			}
		}
	}

	return OutAudio.Num();
}

void FSoundWaveScrubber::UpdateGrainDecodeData(FGrain& InGrain)
{
	FDecodedDataChunk& DecodedData = DecodedChunks[InGrain.DecodedDataChunkIndex];

	// Total number of frames of decoded data in the chunk
	int32 NumReadFramesInDecodedData = DecodedData.PCMAudio.Num() / NumChannels;

	// The number of frames that this grain is offset from the decoded data
	int32 NumFramesOffsetInDecodedData = InGrain.CurrentReadFrame - DecodedData.FrameStart;
	int32 NumFramesPossibleToRenderInChunk = NumReadFramesInDecodedData - NumFramesOffsetInDecodedData;
	check(NumFramesPossibleToRenderInChunk >= 0);

	// If we've totally consumed this decoded audio chunk, we need to get a new decoded audio chunk
	if (!NumFramesPossibleToRenderInChunk)
	{
		// We're no longer using this decoded audio chunk
		DecodedData.NumGrainsUsingChunk--;
		check(DecodedData.NumGrainsUsingChunk >= 0);

		InGrain.DecodedDataChunkIndex = GetDecodedDataChunkIndexForCurrentReadIndex(InGrain.CurrentReadFrame);
		if (InGrain.DecodedDataChunkIndex == INDEX_NONE)
		{
			InGrain.DecodedDataChunkIndex = DecodeDataChunkIndexForCurrentReadIndex(InGrain.CurrentReadFrame);
		}
		check(InGrain.DecodedDataChunkIndex != INDEX_NONE);

		check(InGrain.CurrentReadFrame >= DecodedChunks[InGrain.DecodedDataChunkIndex].FrameStart);
		check(InGrain.CurrentReadFrame < DecodedChunks[InGrain.DecodedDataChunkIndex].FrameStart + DecodedChunks[InGrain.DecodedDataChunkIndex].PCMAudio.Num() / NumChannels);

		DecodedChunks[InGrain.DecodedDataChunkIndex].NumGrainsUsingChunk++;
	}
}

void FSoundWaveScrubber::RenderActiveGrains(TArrayView<float>& OutAudio, int32 InStartFrame, int32 NumFramesToRender)
{
	check(NumChannels > 0);

	// Reverse iterate so we can quickly remove grains when they're done
	for (int32 GrainIndex = ActiveGrains.Num() - 1; GrainIndex >= 0; --GrainIndex)
	{
		FGrain& Grain = ActiveGrains[GrainIndex];

		// This is the number of frames we have left to render for this grain
		int32 NumFramesLeftInGrain = Grain.GrainDurationFrames - Grain.CurrentRenderedFramesCount;
		int32 NumFramesLeftToRender = FMath::Min(NumFramesToRender, NumFramesLeftInGrain);
		check(NumFramesLeftToRender > 0);

		int32 GrainWriteIndex = InStartFrame;

		bool bGrainFinished = false;
		while (NumFramesLeftToRender > 0 && !bGrainFinished)
		{
			// Make sure we have a valid decode data chunk ready for rendering
			UpdateGrainDecodeData(Grain);

			// Retrieve the decoded data.
			const FDecodedDataChunk& DecodedData = DecodedChunks[Grain.DecodedDataChunkIndex];

			// Total number of frames of decoded data in the chunk
			int32 NumReadFramesInDecodedData = DecodedData.PCMAudio.Num() / NumChannels;

			// The number of frames that this grain is offset from the decoded data
			int32 NumFramesOffsetInDecodedData = Grain.CurrentReadFrame - DecodedData.FrameStart;
			check(NumFramesOffsetInDecodedData >= 0);

			int32 NumFramesPossibleToRenderInChunk = NumReadFramesInDecodedData - NumFramesOffsetInDecodedData;
			check(NumFramesPossibleToRenderInChunk >= 0);

			// This may completely consume the decoded chunk here, so only render the maximum number of frames in the 
			// chunk. if NumFramesToRender is smaller than that, then we won't completely consume the decoded data
			int32 NumFramesToRenderInThisChunk = FMath::Min(NumFramesPossibleToRenderInChunk, NumFramesLeftToRender);
			check(NumFramesToRenderInThisChunk >= 0);

			int32 SampleWriteIndex = GrainWriteIndex * NumChannels;
			int32 SampleReadIndex = NumFramesOffsetInDecodedData * NumChannels;

			// For inner-loop audio rendering, we avoid the constant array-access checks that happen on our containers
			const float* DecodedPCMDataPtr = DecodedData.PCMAudio.GetData();
			float* OutDataPtr = OutAudio.GetData();

			for (int32 FrameIndex = 0; FrameIndex < NumFramesToRenderInThisChunk; ++FrameIndex)
			{
				// Retrieve the grain amplitude from the envelope
				// Note the envelope is intentionally sized to match the size of the grain so we don't have
				// to do any interpolation math on look up.
				float EnvelopeFraction = FMath::Clamp((float)Grain.CurrentRenderedFramesCount++ / Grain.GrainDurationFrames, 0.0f, 1.0f);
				float GrainAmplitude = Audio::Grain::GetValue(GrainEnvelope, EnvelopeFraction);

				for (int32 ChannelIndex = 0; ChannelIndex < NumChannels; ++ChannelIndex)
				{
					// Read the decoded sample of the audio at this channel index
					float DecodedSampleValue = DecodedPCMDataPtr[SampleReadIndex++];

					// Scale the sample value by the grain amplitude
					DecodedSampleValue *= GrainAmplitude;

					// Mix the grain audio into the output buffer
					OutDataPtr[SampleWriteIndex++] += DecodedSampleValue;
				}
			}

			Grain.CurrentReadFrame += NumFramesToRenderInThisChunk;

			GrainWriteIndex += NumFramesToRenderInThisChunk;
			NumFramesLeftToRender -= NumFramesToRenderInThisChunk;

			bGrainFinished = (Grain.CurrentRenderedFramesCount >= Grain.GrainDurationFrames);
		}

		// If the grain has finished, then remove it from the active grain list
		if (bGrainFinished)
		{
			DecodedChunks[Grain.DecodedDataChunkIndex].NumGrainsUsingChunk--;
			check(DecodedChunks[Grain.DecodedDataChunkIndex].NumGrainsUsingChunk >= 0);
			ActiveGrains.RemoveAtSwap(GrainIndex, EAllowShrinking::No);
			NumActiveGrains--;
		}
	}
}

void FSoundWaveScrubberGenerator::Init(FSoundWaveProxyPtr InSoundWaveProxyPtr, float InSampleRate, int32 InNumChannels, float InPlayheadTimeSeconds)
{
	NumChannels = InNumChannels;
	SoundWaveScrubber.Init(InSoundWaveProxyPtr, InSampleRate, InNumChannels, InPlayheadTimeSeconds);
}

void FSoundWaveScrubberGenerator::SetIsScrubbing(bool bInIsScrubbing)
{
	SoundWaveScrubber.SetIsScrubbing(bInIsScrubbing);
}

void FSoundWaveScrubberGenerator::SetIsScrubbingWhileStationary(bool bInScrubWhileStationary)
{
	SoundWaveScrubber.SetIsScrubbingWhileStationary(bInScrubWhileStationary);
}

void FSoundWaveScrubberGenerator::SetPlayheadTime(float InPlayheadTimeSeconds)
{
	SoundWaveScrubber.SetPlayheadTime(InPlayheadTimeSeconds);
}

void FSoundWaveScrubberGenerator::SetGrainDurationRange(const FVector2D& InGrainDurationRange)
{
	SoundWaveScrubber.SetGrainDurationRange(InGrainDurationRange);
}

int32 FSoundWaveScrubberGenerator::OnGenerateAudio(float* OutAudio, int32 NumSamples)
{
	FMemory::Memzero(OutAudio, NumSamples*sizeof(float));
	TArrayView<float> OutView = TArrayView<float>(OutAudio, NumSamples);
	return SoundWaveScrubber.RenderAudio(OutView);
}

int32 FSoundWaveScrubberGenerator::GetDesiredNumSamplesToRenderPerCallback() const
{
	return 256 * NumChannels;
}

bool FSoundWaveScrubberGenerator::IsFinished() const
{
	// This is intended to be "always on" unless stopped by owning audio component, etc.
	return false;
}

} // namespace Audio

UScrubbedSound::UScrubbedSound(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bProcedural = true;
}

ISoundGeneratorPtr UScrubbedSound::CreateSoundGenerator(const FSoundGeneratorInitParams& InParams)
{
	using namespace Audio;

	if (SoundWaveToScrub)
	{
		FSoundWaveProxyPtr SoundWaveProxyPtr = SoundWaveToScrub->CreateSoundWaveProxy();

		SoundWaveScrubber = ISoundGeneratorPtr(new FSoundWaveScrubberGenerator());

		FSoundWaveScrubberGenerator* Scrubber = static_cast<FSoundWaveScrubberGenerator*>(SoundWaveScrubber.Get());
		Scrubber->Init(SoundWaveProxyPtr, InParams.SampleRate, NumChannels, PlayheadTimeSeconds);
		Scrubber->SetIsScrubbing(bIsScrubbing);
		Scrubber->SetIsScrubbingWhileStationary(bScrubWhileStationary);
		Scrubber->SetGrainDurationRange(GrainDurationRange);

		return SoundWaveScrubber;
	}

	return nullptr;
}

void UScrubbedSound::SetSoundWave(USoundWave* InSoundWave)
{
	 SoundWaveToScrub = InSoundWave;
	 NumChannels = InSoundWave->NumChannels;
}

void UScrubbedSound::SetIsScrubbing(bool bInIsScrubbing)
{
	using namespace Audio;

	bIsScrubbing = bInIsScrubbing;
	if (SoundWaveScrubber.IsValid())
	{
		FSoundWaveScrubberGenerator* Scrubber = static_cast<FSoundWaveScrubberGenerator*>(SoundWaveScrubber.Get());
		Scrubber->SetIsScrubbing(bIsScrubbing);
	}
}

void UScrubbedSound::SetIsScrubbingWhileStationary(bool bInScrubWhileStationary)
{
	using namespace Audio;

	bScrubWhileStationary = bInScrubWhileStationary;
	if (SoundWaveScrubber.IsValid())
	{
		FSoundWaveScrubberGenerator* Scrubber = static_cast<FSoundWaveScrubberGenerator*>(SoundWaveScrubber.Get());
		Scrubber->SetIsScrubbingWhileStationary(bInScrubWhileStationary);
	}
}

void UScrubbedSound::SetPlayheadTime(float InPlayheadTimeSeconds)
{
	using namespace Audio;

	PlayheadTimeSeconds = InPlayheadTimeSeconds;
	if (SoundWaveScrubber.IsValid())
	{
		FSoundWaveScrubberGenerator* Scrubber = static_cast<FSoundWaveScrubberGenerator*>(SoundWaveScrubber.Get());
		Scrubber->SetPlayheadTime(InPlayheadTimeSeconds);
	}
}

void UScrubbedSound::SetGrainDurationRange(const FVector2D& InGrainDurationRangeSeconds)
{
	using namespace Audio;

	GrainDurationRange = InGrainDurationRangeSeconds;
	if (SoundWaveScrubber.IsValid())
	{
		FSoundWaveScrubberGenerator* Scrubber = static_cast<FSoundWaveScrubberGenerator*>(SoundWaveScrubber.Get());
		Scrubber->SetGrainDurationRange(GrainDurationRange);
	}
}