// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanPipelineMediaPlayerUENode.h"
#include "MetaHumanTrace.h"
#include "MetaHumanLocalLiveLinkSubject.h"

#include "UObject/Package.h"

#include "MediaPlayerFacade.h"
#include "MediaBundle.h"

namespace UE::MetaHuman::Pipeline
{

FMediaPlayerUENode::FMediaPlayerUENode(const FString& InName) : FMediaPlayerNode("MediaPlayerUE", InName)
{
}

bool FMediaPlayerUENode::Start(const TSharedPtr<FPipelineData>& InPipelineData)
{
	double Start = FPlatformTime::Seconds();

	if (!VideoURL.IsEmpty() && !VideoPlayer)
	{
		InPipelineData->SetErrorNodeCode(ErrorCode::NoVideoPlayer);
		InPipelineData->SetErrorNodeMessage(TEXT("Failed to setup video player"));

		return false;
	}

	if (!AudioURL.IsEmpty() && !AudioPlayer)
	{
		InPipelineData->SetErrorNodeCode(ErrorCode::NoAudioPlayer);
		InPipelineData->SetErrorNodeMessage(TEXT("Failed to setup audio player"));

		return false;
	}

#if WITH_EDITORONLY_DATA
	if (VideoPlayer)
	{
		VideoPlayer->AffectedByPIEHandling = false;
	}

	if (AudioPlayer)
	{
		AudioPlayer->AffectedByPIEHandling = false;
	}
#endif

	AudioBuffer.Reset();
	AudioBufferSampleRate = -1;
	AudioBufferNumChannels = -1;
	AudioBufferDataSizePerFrame = -1;

	bool bVideoReady = !VideoPlayer;
	bool bAudioReady = !AudioPlayer;

	while (FPlatformTime::Seconds() - Start < StartTimeout)
	{
		if (!bVideoReady && VideoPlayer->GetPlayerFacade()->IsReady())
		{
			if (VideoTrack >= 0)
			{
				if (!VideoPlayer->SelectTrack(EMediaPlayerTrack::Video, VideoTrack))
				{
					InPipelineData->SetErrorNodeCode(ErrorCode::BadVideoTrack);
					InPipelineData->SetErrorNodeMessage(TEXT("Failed to set video track"));

					return false;
				}

				if (VideoTrackFormat >= 0 && !VideoPlayer->SetTrackFormat(EMediaPlayerTrack::Video, VideoTrack, VideoTrackFormat))
				{
					InPipelineData->SetErrorNodeCode(ErrorCode::BadVideoTrackFormat);
					InPipelineData->SetErrorNodeMessage(TEXT("Failed to set video track format"));

					return false;
				}
			}

			bVideoReady = true;
		}
		else if (!bAudioReady && AudioPlayer->GetPlayerFacade()->IsReady())
		{
			if (AudioTrack >= 0)
			{
				if (!AudioPlayer->SelectTrack(EMediaPlayerTrack::Audio, AudioTrack))
				{
					InPipelineData->SetErrorNodeCode(ErrorCode::BadAudioTrack);
					InPipelineData->SetErrorNodeMessage(TEXT("Failed to set audio track"));

					return false;
				}

				if (AudioTrackFormat >= 0 && !AudioPlayer->SetTrackFormat(EMediaPlayerTrack::Audio, AudioTrack, AudioTrackFormat))
				{
					InPipelineData->SetErrorNodeCode(ErrorCode::BadAudioTrackFormat);
					InPipelineData->SetErrorNodeMessage(TEXT("Failed to set audio track format"));

					return false;
				}
			}

			bAudioReady = true;
		}
		else if (bVideoReady && bAudioReady)
		{
			FPlatformProcess::Sleep(FormatWaitTime); // For track/format change to take effect. Could not find a suitable event to be notified of this.

			if (VideoPlayer && !VideoPlayer->Play())
			{
				InPipelineData->SetErrorNodeCode(ErrorCode::FailedToPlayVideo);
				InPipelineData->SetErrorNodeMessage(TEXT("Failed to play video"));

				return false;
			}

			if (AudioPlayer && !AudioPlayer->Play())
			{
				InPipelineData->SetErrorNodeCode(ErrorCode::FailedToPlayAudio);
				InPipelineData->SetErrorNodeMessage(TEXT("Failed to play audio"));

				return false;
			}

			bIsFirstFrame = true;

			return true;
		}
		else
		{
			FPlatformProcess::Sleep(StartWaitTime);
		}
	}

	if (!bVideoReady)
	{
		InPipelineData->SetErrorNodeCode(ErrorCode::VideoTimeout);
		InPipelineData->SetErrorNodeMessage(TEXT("Timeout opening video"));
	}
	else
	{
		InPipelineData->SetErrorNodeCode(ErrorCode::AudioTimeout);
		InPipelineData->SetErrorNodeMessage(TEXT("Timeout opening audio"));
	}

	return false;
}

bool FMediaPlayerUENode::Process(const TSharedPtr<FPipelineData>& InPipelineData)
{
	double Start = FPlatformTime::Seconds();

	TSharedPtr<IMediaTextureSample> VideoSample;
	TSharedPtr<IMediaAudioSample> AudioSample;
	bool bDroppedFrame = false;

	{
		MHA_CPUPROFILER_EVENT_SCOPE_STR("Get Frame");

		while (true)
		{
			if (*bAbort)
			{
				return false;
			}

			if (bIsFirstFrame && FPlatformTime::Seconds() > Start + SampleTimeout) // Only timeout on first frame - sample are not delivered if game thread is blocked
			{
				if (VideoPlayer && !VideoSample)
				{
					InPipelineData->SetErrorNodeCode(ErrorCode::VideoTimeout);
					InPipelineData->SetErrorNodeMessage(TEXT("Timeout sampling video"));
				}
				else
				{
					InPipelineData->SetErrorNodeCode(ErrorCode::AudioTimeout);
					InPipelineData->SetErrorNodeMessage(TEXT("Timeout sampling audio"));
				}

				return false;
			}

			if (VideoPlayer && VideoFrames->Dequeue(VideoSample) && VideoSample.IsValid())
			{
				if (bIsFirstFrame)
				{
					const float Rate = VideoPlayer->GetVideoTrackFrameRate(VideoTrack, VideoTrackFormat);

					int32 Numerator = 0;
					int32 Denominator = 0;

					if (Rate - static_cast<int32>(Rate) > 1.0e-5) // The only fractional frame rates supported are the 29.97 types
					{
						Numerator = FMath::CeilToInt(Rate) * 1000;
						Denominator = 1001;
					}
					else
					{
						Numerator = Rate;
						Denominator = 1;
					}

					FrameRate = FFrameRate(Numerator, Denominator);
				}

				if (bAllowFrameDropping)
				{
					TSharedPtr<IMediaTextureSample> LatestVideoSample;
					while (VideoFrames->Dequeue(LatestVideoSample) && LatestVideoSample.IsValid())
					{
						VideoSample = LatestVideoSample;
						bDroppedFrame = true;
					}
				}
			}

			if (AudioPlayer && AudioFrames->Dequeue(AudioSample) && AudioSample.IsValid())
			{
				check(AudioSample->GetFormat() == EMediaAudioSampleFormat::Int16);

				// Temporary measure. Audio samples come through from underlying UE at about 25Hz.
				// This is with the WFM "Low latency" project setting unchecked. With it checked
				// samples come through faster, at around 100Hz, but seem to have a very large
				// delay on them. This makes the setting unusable at the moment. If this node output 
				// audio samples at 25Hz it would make for jerky animation or whatever was using this
				// data downstream. Instead we buffer the audio and feed it out in parts at a faster rate.
				// For now this is 50Hz since that ties in with immediate use case of realtime audio-to-animation.
				// Maybe this should be a configurable parameter? but ideally I'd like to get away from using
				// it altogether and instead get the "low latency" issues fixed and run at 100Hz.

				if (bIsFirstFrame)
				{
					FrameRate = FFrameRate(50, 1);
				}

				if (AudioBufferDataSizePerFrame < 0)
				{
					AudioBufferSampleRate = AudioSample->GetSampleRate();
					AudioBufferNumChannels = AudioSample->GetChannels();
					AudioBufferDataSizePerFrame = AudioBufferSampleRate / 50.0 * AudioBufferNumChannels;
				}

				const int16* Data = (const int16*)AudioSample->GetBuffer();
				const int32 NumDataItems = AudioSample->GetFrames() * AudioSample->GetChannels();
				const float MaxValue = std::numeric_limits<int16>::max();
				for (int32 DataIndex = 0; DataIndex < NumDataItems; ++DataIndex, ++Data)
				{
					AudioBuffer.Add(*Data / MaxValue);
				}

				if (AudioSample->GetTimecode().IsSet())
				{
					UE_LOG(LogMetaHumanLocalLiveLinkSubject, Warning, TEXT("Ignoring audio sample timecode"));
				}
			}

			if ((!VideoPlayer || VideoSample) && (!AudioPlayer || (AudioBufferDataSizePerFrame > 0 && AudioBuffer.Num() >= AudioBufferDataSizePerFrame)))
			{
				break;
			}

			FPlatformProcess::Sleep(SampleWaitTime);
		}
	}

	bIsFirstFrame = false;

	FUEImageDataType Image;
	FAudioDataType Audio;
	FQualifiedFrameTime ImageSampleTime;
	FQualifiedFrameTime AudioSampleTime;
	FMetaHumanLocalLiveLinkSubject::ETimeSource ImageSampleTimeSource = FMetaHumanLocalLiveLinkSubject::ETimeSource::NotSet;
	FMetaHumanLocalLiveLinkSubject::ETimeSource AudioSampleTimeSource = FMetaHumanLocalLiveLinkSubject::ETimeSource::NotSet;

	if (VideoSample)
	{ 
		FMetaHumanLocalLiveLinkSubject::GetSampleTime(VideoSample->GetTimecode(), FrameRate, ImageSampleTime, ImageSampleTimeSource);

		MHA_CPUPROFILER_EVENT_SCOPE_STR("Video Conversion");

		if (VideoSample->GetFormat() != EMediaTextureSampleFormat::CharNV12 &&
			VideoSample->GetFormat() != EMediaTextureSampleFormat::CharYUY2 &&
			VideoSample->GetFormat() != EMediaTextureSampleFormat::CharUYVY &&
			VideoSample->GetFormat() != EMediaTextureSampleFormat::CharBGRA &&
			!(VideoSample->GetFormat() == EMediaTextureSampleFormat::YUVv210 && VideoSample->GetOutputDim().X % 6 == 0))
		{
			InPipelineData->SetErrorNodeCode(ErrorCode::UnsupportedVideoFormat);
			InPipelineData->SetErrorNodeMessage(FString::Printf(TEXT("Unsupported video format %i"), (int32)VideoSample->GetFormat()));

			return false;
		}

		if (!VideoSample->GetBuffer()) // Some player backends, eg Electra, dont appear to fill in buffer. Maybe their image is in a texture (in GPU memory)
		{
			InPipelineData->SetErrorNodeCode(ErrorCode::NoVideoSampleData);
			InPipelineData->SetErrorNodeMessage(TEXT("No video sample data"));

			return false;
		}

		ConvertSample(VideoSample->GetOutputDim(), VideoSample->GetStride(), VideoSample->GetFormat(), (const uint8*) VideoSample->GetBuffer(), Image);
	}

	if (AudioBufferDataSizePerFrame > 0 && AudioBuffer.Num() >= AudioBufferDataSizePerFrame)
	{
		FMetaHumanLocalLiveLinkSubject::GetSampleTime(FrameRate, AudioSampleTime, AudioSampleTimeSource);

		Audio.NumChannels = AudioBufferNumChannels;
		Audio.SampleRate = AudioBufferSampleRate;

		Audio.Data = TArray<float>(AudioBuffer.GetData(), AudioBufferDataSizePerFrame);
		AudioBuffer = TArray<float>(AudioBuffer.GetData() + AudioBufferDataSizePerFrame, AudioBuffer.Num() - AudioBufferDataSizePerFrame);

		Audio.NumSamples = Audio.Data.Num() / Audio.NumChannels;
	}

	InPipelineData->SetData<FUEImageDataType>(Pins[0], MoveTemp(Image));
	InPipelineData->SetData<FAudioDataType>(Pins[1], MoveTemp(Audio));
	InPipelineData->SetData<FQualifiedFrameTime>(Pins[2], ImageSampleTime);
	InPipelineData->SetData<FQualifiedFrameTime>(Pins[3], AudioSampleTime);
	InPipelineData->SetData<bool>(Pins[4], bDroppedFrame);
	InPipelineData->SetData<int32>(Pins[5], static_cast<uint8>(ImageSampleTimeSource));
	InPipelineData->SetData<int32>(Pins[6], static_cast<uint8>(AudioSampleTimeSource));

	return true;
}

bool FMediaPlayerUENode::End(const TSharedPtr<FPipelineData>& InPipelineData)
{
	AudioBuffer.Reset();

	return true;
}

bool FMediaPlayerUENode::Play(const FString& InVideoURL, int32 InVideoTrack, int32 InVideoTrackFormat,
							  const FString& InAudioURL, int32 InAudioTrack, int32 InAudioTrackFormat)
{
	// Must be done in game thread
	check(IsInGameThread());

	VideoURL = InVideoURL;
	VideoTrack = InVideoTrack;
	VideoTrackFormat = InVideoTrackFormat;

	AudioURL = InAudioURL;
	AudioTrack = InAudioTrack;
	AudioTrackFormat = InAudioTrackFormat;

	bool bOpenedOk = (VideoPlayer || AudioPlayer);

	if (!VideoURL.IsEmpty())
	{
		bIsBundle = VideoURL.StartsWith(BundleURL);
		if (bIsBundle)
		{
			Bundle = LoadObject<UMediaBundle>(GetTransientPackage(), *VideoURL.Mid(BundleURL.Len()));
			if (!Bundle)
			{
				return false;
			}

			VideoPlayer = Bundle->GetMediaPlayer();
			if (!VideoPlayer)
			{
				return false;
			}
		}
		else
		{
			VideoPlayer = NewObject<UMediaPlayer>(GetTransientPackage());
			check(VideoPlayer);
		}

		VideoPlayer->PlayOnOpen = false;

		VideoFrames = MakeShared<FMediaTextureSampleQueue>();
		check(VideoFrames);

		VideoPlayer->GetPlayerFacade()->AddVideoSampleSink(VideoFrames.ToSharedRef());

		bOpenedOk &= bIsBundle ? Bundle->OpenMediaSource() : VideoPlayer->OpenUrl(VideoURL); // OpenUrl is an async call
	}

	if (!AudioURL.IsEmpty())
	{
		bIsBundle = AudioURL.StartsWith(BundleURL);
		if (bIsBundle)
		{
			Bundle = LoadObject<UMediaBundle>(GetTransientPackage(), *AudioURL.Mid(BundleURL.Len()));
			if (!Bundle)
			{
				return false;
			}

			AudioPlayer = Bundle->GetMediaPlayer();
			if (!AudioPlayer)
			{
				return false;
			}
		}
		else
		{
			AudioPlayer = NewObject<UMediaPlayer>(GetTransientPackage());
			check(AudioPlayer);
		}

		AudioPlayer->PlayOnOpen = false;

		AudioFrames = MakeShared<FMediaAudioSampleQueue>();
		check(AudioFrames);

		AudioPlayer->GetPlayerFacade()->AddAudioSampleSink(AudioFrames.ToSharedRef());

		bOpenedOk &= bIsBundle ? Bundle->OpenMediaSource() : AudioPlayer->OpenUrl(AudioURL); // OpenUrl is an async call
	}

	return bOpenedOk;
}

bool FMediaPlayerUENode::Close()
{
	// Must be done in game thread
	check(IsInGameThread());

	if (VideoPlayer && VideoPlayer->IsPlaying()) // IsPlaying prevents crash when closing down on editor exit
	{
		VideoPlayer->Close();
	}

	if (AudioPlayer && AudioPlayer->IsPlaying()) // IsPlaying prevents crash when closing down on editor exit
	{
		AudioPlayer->Close();
	}

	return true;
}

void FMediaPlayerUENode::AddReferencedObjects(FReferenceCollector& InCollector)
{
	InCollector.AddReferencedObject(VideoPlayer);
	InCollector.AddReferencedObject(AudioPlayer);
	InCollector.AddReferencedObject(Bundle);
}

FString FMediaPlayerUENode::GetReferencerName() const
{
	return TEXT("FMediaPlayerNodeUE");
}

}