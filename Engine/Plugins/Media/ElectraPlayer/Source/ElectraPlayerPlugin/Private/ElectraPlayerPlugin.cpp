// Copyright Epic Games, Inc. All Rights Reserved.

#include "ElectraPlayerPlugin.h"

#include "Misc/Optional.h"
#include "IMediaEventSink.h"
#include "IMediaOptions.h"
#include "IMediaMetadataItem.h"
#include "MediaSamples.h"
#include "MediaPlayerOptions.h"

#include "IElectraPlayerRuntimeModule.h"
#include "IElectraPlayerPluginModule.h"
#include "IElectraMetadataSample.h"
#include "IElectraSubtitleSample.h"

#include "MediaSubtitleDecoderOutput.h"
#include "MediaMetaDataDecoderOutput.h"

using namespace Electra;

//-----------------------------------------------------------------------------

std::atomic<uint32> FElectraPlayerPlugin::NextPlayerUniqueID { 0 };

//-----------------------------------------------------------------------------

namespace ElectraMediaOptions
{
	static const FName GetSafeMediaOptions(TEXT("GetSafeMediaOptions"));
	static const FName ElectraNoPreloading(TEXT("ElectraNoPreloading"));
	static const FName PlaylistProperties(TEXT("playlist_properties"));
	static const FName ElectraInitialBitrate(TEXT("ElectraInitialBitrate"));
	static const FName MaxElectraVerticalResolution(TEXT("MaxElectraVerticalResolution"));
	static const FName MaxElectraVerticalResolutionOf60fpsVideos(TEXT("MaxElectraVerticalResolutionOf60fpsVideos"));
	static const FName ElectraLivePresentationOffset(TEXT("ElectraLivePresentationOffset"));
	static const FName ElectraLiveAudioPresentationOffset(TEXT("ElectraLiveAudioPresentationOffset"));
	static const FName ElectraLiveUseConservativePresentationOffset(TEXT("ElectraLiveUseConservativePresentationOffset"));
	static const FName ElectraThrowErrorWhenRebuffering(TEXT("ElectraThrowErrorWhenRebuffering"));
	static const FName ElectraGetDenyStreamCode(TEXT("ElectraGetDenyStreamCode"));
	static const FName MaxResolutionForMediaStreaming(TEXT("MaxResolutionForMediaStreaming"));
	static const FName ElectraMaxStreamingBandwidth(TEXT("ElectraMaxStreamingBandwidth"));
	static const FName ElectraPlayerDataCache(TEXT("ElectraPlayerDataCache"));
	static const FName Mimetype(TEXT("mimetype"));
	static const FName CodecOptions[] = { TEXT("excluded_codecs_video"), TEXT("excluded_codecs_audio"), TEXT("excluded_codecs_subtitles"), TEXT("preferred_codecs_video"), TEXT("preferred_codecs_audio"),TEXT("preferred_codecs_subtitles") };
	static const FName ElectraGetPlaylistData(TEXT("ElectraGetPlaylistData"));
	static const FName ElectraGetLicenseKeyData(TEXT("ElectraGetLicenseKeyData"));
	static const FName ElectraGetPlaystartPosFromSeekPositions(TEXT("ElectraGetPlaystartPosFromSeekPositions"));

	static const FName KeyUniquePlayerID(TEXT("UniquePlayerID"));
	static const FName OptionKeyParseTimecodeInfo(TEXT("parse_timecode_info"));
}

//-----------------------------------------------------------------------------

FElectraPlayerPlugin::FElectraPlayerPlugin()
{
	// Make sure a few assumptions are correct...
	static_assert((int32)EMediaEvent::MediaBuffering == (int32)IElectraPlayerAdapterDelegate::EPlayerEvent::MediaBuffering, "check alignment of both enums");
	static_assert((int32)EMediaEvent::MediaBufferingComplete == (int32)IElectraPlayerAdapterDelegate::EPlayerEvent::MediaBufferingComplete, "check alignment of both enums");
	static_assert((int32)EMediaEvent::MediaClosed == (int32)IElectraPlayerAdapterDelegate::EPlayerEvent::MediaClosed, "check alignment of both enums");
	static_assert((int32)EMediaEvent::MediaConnecting == (int32)IElectraPlayerAdapterDelegate::EPlayerEvent::MediaConnecting, "check alignment of both enums");
	static_assert((int32)EMediaEvent::MediaOpened == (int32)IElectraPlayerAdapterDelegate::EPlayerEvent::MediaOpened, "check alignment of both enums");
	static_assert((int32)EMediaEvent::MediaOpenFailed == (int32)IElectraPlayerAdapterDelegate::EPlayerEvent::MediaOpenFailed, "check alignment of both enums");
	static_assert((int32)EMediaEvent::PlaybackEndReached == (int32)IElectraPlayerAdapterDelegate::EPlayerEvent::PlaybackEndReached, "check alignment of both enums");
	static_assert((int32)EMediaEvent::PlaybackResumed == (int32)IElectraPlayerAdapterDelegate::EPlayerEvent::PlaybackResumed, "check alignment of both enums");
	static_assert((int32)EMediaEvent::PlaybackSuspended == (int32)IElectraPlayerAdapterDelegate::EPlayerEvent::PlaybackSuspended, "check alignment of both enums");
	static_assert((int32)EMediaEvent::SeekCompleted == (int32)IElectraPlayerAdapterDelegate::EPlayerEvent::SeekCompleted, "check alignment of both enums");
	static_assert((int32)EMediaEvent::TracksChanged == (int32)IElectraPlayerAdapterDelegate::EPlayerEvent::TracksChanged, "check alignment of both enums");
	static_assert((int32)EMediaEvent::MetadataChanged == (int32)IElectraPlayerAdapterDelegate::EPlayerEvent::MetadataChanged, "check alignment of both enums");
	static_assert((int32)EMediaEvent::Internal_PurgeVideoSamplesHint == (int32)IElectraPlayerAdapterDelegate::EPlayerEvent::Internal_PurgeVideoSamplesHint, "check alignment of both enums");
	static_assert((int32)EMediaEvent::Internal_VideoSamplesAvailable == (int32)IElectraPlayerAdapterDelegate::EPlayerEvent::Internal_VideoSamplesAvailable, "check alignment of both enums");
	static_assert((int32)EMediaEvent::Internal_VideoSamplesUnavailable == (int32)IElectraPlayerAdapterDelegate::EPlayerEvent::Internal_VideoSamplesUnavailable, "check alignment of both enums");
	static_assert((int32)EMediaEvent::Internal_AudioSamplesAvailable == (int32)IElectraPlayerAdapterDelegate::EPlayerEvent::Internal_AudioSamplesAvailable, "check alignment of both enums");
	static_assert((int32)EMediaEvent::Internal_AudioSamplesUnavailable == (int32)IElectraPlayerAdapterDelegate::EPlayerEvent::Internal_AudioSamplesUnavailable, "check alignment of both enums");

	static_assert((int32)EMediaState::Closed == (int32)IElectraPlayerInterface::EPlayerState::Closed, "check alignment of both enums");
	static_assert((int32)EMediaState::Error == (int32)IElectraPlayerInterface::EPlayerState::Error, "check alignment of both enums");
	static_assert((int32)EMediaState::Paused == (int32)IElectraPlayerInterface::EPlayerState::Paused, "check alignment of both enums");
	static_assert((int32)EMediaState::Playing == (int32)IElectraPlayerInterface::EPlayerState::Playing, "check alignment of both enums");
	static_assert((int32)EMediaState::Preparing == (int32)IElectraPlayerInterface::EPlayerState::Preparing, "check alignment of both enums");
	static_assert((int32)EMediaState::Stopped == (int32)IElectraPlayerInterface::EPlayerState::Stopped, "check alignment of both enums");

	static_assert((int32)EMediaStatus::None == (int32)IElectraPlayerInterface::EPlayerStatus::None, "check alignment of both enums");
	static_assert((int32)EMediaStatus::Buffering == (int32)IElectraPlayerInterface::EPlayerStatus::Buffering, "check alignment of both enums");
	static_assert((int32)EMediaStatus::Connecting == (int32)IElectraPlayerInterface::EPlayerStatus::Connecting, "check alignment of both enums");

	static_assert((int32)EMediaTrackType::Audio == (int32)IElectraPlayerInterface::EPlayerTrackType::Audio, "check alignment of both enums");
	static_assert((int32)EMediaTrackType::Video == (int32)IElectraPlayerInterface::EPlayerTrackType::Video, "check alignment of both enums");

	static_assert((int32)EMediaRateThinning::Unthinned == (int32)IElectraPlayerInterface::EPlayRateType::Unthinned, "check alignment of both enums");
	static_assert((int32)EMediaRateThinning::Thinned == (int32)IElectraPlayerInterface::EPlayRateType::Thinned, "check alignment of both enums");

	static_assert((int32)EMediaTimeRangeType::Absolute == (int32)IElectraPlayerInterface::ETimeRangeType::Absolute, "check alignment of both enums");
	static_assert((int32)EMediaTimeRangeType::Current == (int32)IElectraPlayerInterface::ETimeRangeType::Current, "check alignment of both enums");

	static_assert(IMediaPlayerLifecycleManagerDelegate::ResourceFlags_Decoder == IElectraPlayerInterface::ResourceFlags_Decoder, "check alignment of both enums");
	static_assert(IMediaPlayerLifecycleManagerDelegate::ResourceFlags_OutputBuffers == IElectraPlayerInterface::ResourceFlags_OutputBuffers, "check alignment of both enums");
	static_assert(IMediaPlayerLifecycleManagerDelegate::ResourceFlags_Any == IElectraPlayerInterface::ResourceFlags_Any, "check alignment of both enums");
	static_assert(IMediaPlayerLifecycleManagerDelegate::ResourceFlags_All == IElectraPlayerInterface::ResourceFlags_All, "check alignment of both enums");
}

bool FElectraPlayerPlugin::Initialize(IMediaEventSink& InEventSink,
	FElectraPlayerSendAnalyticMetricsDelegate& InSendAnalyticMetricsDelegate,
	FElectraPlayerSendAnalyticMetricsPerMinuteDelegate& InSendAnalyticMetricsPerMinuteDelegate,
	FElectraPlayerReportVideoStreamingErrorDelegate& InReportVideoStreamingErrorDelegate,
	FElectraPlayerReportSubtitlesMetricsDelegate& InReportSubtitlesFileMetricsDelegate)
{
	CallbackPointerLock.Lock();
	EventSink = &InEventSink;
	CallbackPointerLock.Unlock();

	OutputTexturePool = MakeShareable(new FElectraTextureSamplePool);

	MediaSamples.Reset(new FMediaSamples);

	PlayerResourceDelegate = MakeShareable(PlatformCreatePlayerResourceDelegate());

	Player = MakeShareable(FElectraPlayerRuntimeFactory::CreatePlayer(SharedThis(this), InSendAnalyticMetricsDelegate, InSendAnalyticMetricsPerMinuteDelegate, InReportVideoStreamingErrorDelegate, InReportSubtitlesFileMetricsDelegate));

	bMetadataChanged = false;
	CurrentMetadata.Reset();
	return true;
}

FElectraPlayerPlugin::~FElectraPlayerPlugin()
{
	CallbackPointerLock.Lock();
	EventSink = nullptr;
	OptionInterface.Reset();
	CallbackPointerLock.Unlock();
	if (Player.IsValid())
	{
		Player->CloseInternal(true);
		Player.Reset();
	}
	PlayerResourceDelegate.Reset();
	MediaSamples.Reset();
}

//-----------------------------------------------------------------------------

class FElectraBinarySample : public IElectraBinarySample
{
public:
	virtual ~FElectraBinarySample() = default;
	virtual const void* GetData() override							{ return Metadata->GetData(); }
	virtual uint32 GetSize() const override							{ return Metadata->GetSize(); }
	virtual FGuid GetGUID() const override							{ return IElectraBinarySample::GetSampleTypeGUID(); }
	virtual const FString& GetSchemeIdUri() const override			{ return Metadata->GetSchemeIdUri(); }
	virtual const FString& GetValue() const override				{ return Metadata->GetValue(); }
	virtual const FString& GetID() const override					{ return Metadata->GetID(); }

	virtual EDispatchedMode GetDispatchedMode() const override
	{
		switch(Metadata->GetDispatchedMode())
		{
			default:
			case IMetaDataDecoderOutput::EDispatchedMode::OnReceive:
			{
				return FElectraBinarySample::EDispatchedMode::OnReceive;
			}
			case IMetaDataDecoderOutput::EDispatchedMode::OnStart:
			{
				return FElectraBinarySample::EDispatchedMode::OnStart;
			}
		}
	}

	virtual EOrigin GetOrigin() const override
	{
		switch(Metadata->GetOrigin())
		{
			default:
			case IMetaDataDecoderOutput::EOrigin::TimedMetadata:
			{
				return FElectraBinarySample::EOrigin::TimedMetadata;
			}
			case IMetaDataDecoderOutput::EOrigin::EventStream:
			{
				return FElectraBinarySample::EOrigin::EventStream;
			}
			case IMetaDataDecoderOutput::EOrigin::InbandEventStream:
			{
				return FElectraBinarySample::EOrigin::InbandEventStream;
			}
		}
	}

	virtual FMediaTimeStamp GetTime() const override
	{
		FDecoderTimeStamp ts = Metadata->GetTime();
		return FMediaTimeStamp(ts.Time, ts.SequenceIndex);
	}

	virtual FTimespan GetDuration() const override
	{
		FTimespan Duration = Metadata->GetDuration();
		// A zero duration might cause the metadata sample fall through the cracks later
		// so set it to a short 1ms instead.
		if (Duration.IsZero())
		{
			Duration = FTimespan::FromMilliseconds(1);
		}
		return Duration;
	}

	virtual TOptional<FMediaTimeStamp> GetTrackBaseTime() const	override
	{
		TOptional<FMediaTimeStamp> ms;
		TOptional<FDecoderTimeStamp> ts = Metadata->GetTime();
		if (ts.IsSet())
		{
			ms = FMediaTimeStamp(ts.GetValue().Time, ts.GetValue().SequenceIndex);
		}
		return ms;
	}

	IMetaDataDecoderOutputPtr Metadata;
};

//-----------------------------------------------------------------------------

class FElectraSubtitleSample : public IElectraSubtitleSample
{
public:
	virtual FGuid GetGUID() const override
	{
		return IElectraSubtitleSample::GetSampleTypeGUID();
	}

	virtual FMediaTimeStamp GetTime() const override
	{
		FDecoderTimeStamp ts = Subtitle->GetTime();
		return FMediaTimeStamp(ts.Time, ts.SequenceIndex);
	}

	virtual FTimespan GetDuration() const override
	{
		return Subtitle->GetDuration();
	}

	virtual TOptional<FVector2D> GetPosition() const override
	{
		return TOptional<FVector2D>();
	}

	virtual FText GetText() const override
	{
		FUTF8ToTCHAR cnv((const ANSICHAR*)Subtitle->GetData().GetData(), Subtitle->GetData().Num());
		FString UTF8Text = FString::ConstructFromPtrSize(cnv.Get(), cnv.Length());
		return FText::FromString(UTF8Text);
	}

	virtual EMediaOverlaySampleType GetType() const override
	{
		return EMediaOverlaySampleType::Subtitle;
	}

	ISubtitleDecoderOutputPtr Subtitle;
};

//-----------------------------------------------------------------------------

class FStreamMetadataItem : public IMediaMetadataItem
{
public:
	FStreamMetadataItem(const TSharedPtr<Electra::IMediaStreamMetadata::IItem, ESPMode::ThreadSafe>& InItem) : Item(InItem.ToSharedRef())
	{ }
	virtual ~FStreamMetadataItem()
	{ }
	const FString& GetLanguageCode() const override
	{ return Item->GetLanguageCode(); }
	const FString& GetMimeType() const override
	{ return Item->GetMimeType(); }
	const FVariant& GetValue() const override
	{ return Item->GetValue(); }
private:
	TSharedRef<Electra::IMediaStreamMetadata::IItem, ESPMode::ThreadSafe> Item;
};

//-----------------------------------------------------------------------------

void FElectraPlayerPlugin::BlobReceived(const TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe>& InBlobData, IElectraPlayerAdapterDelegate::EBlobResultType InResultType, int32 InResultCode, const Electra::FParamDict* InExtraInfo)
{
}

Electra::FVariantValue FElectraPlayerPlugin::QueryOptions(EOptionType Type, const Electra::FVariantValue& Param)
{
	CallbackPointerLock.Lock();
	TSharedPtr<IElectraSafeMediaOptionInterface, ESPMode::ThreadSafe> SafeOptionInterface = OptionInterface.Pin();
	CallbackPointerLock.Unlock();
	if (SafeOptionInterface.IsValid())
	{
		IElectraSafeMediaOptionInterface::FScopedLock SafeLock(SafeOptionInterface);
		IMediaOptions *SafeOptions = SafeOptionInterface->GetMediaOptionInterface();
		if (SafeOptions)
		{
			switch(Type)
			{
				case EOptionType::MaxVerticalStreamResolution:
				{
					return FVariantValue((int64)SafeOptions->GetMediaOption(ElectraMediaOptions::MaxResolutionForMediaStreaming, (int64)0));
				}

				case EOptionType::MaxBandwidthForStreaming:
				{
					return FVariantValue((int64)SafeOptions->GetMediaOption(ElectraMediaOptions::ElectraMaxStreamingBandwidth, (int64)0));
				}

				case EOptionType::PlayListData:
				{
					if (SafeOptions->HasMediaOption(ElectraMediaOptions::ElectraGetPlaylistData))
					{
						check(Param.IsType(FVariantValue::EDataType::TypeFString));
						return FVariantValue(SafeOptions->GetMediaOption(ElectraMediaOptions::ElectraGetPlaylistData, Param.GetFString()));
					}
					break;
				}

				case EOptionType::LicenseKeyData:
				{
					if (SafeOptions->HasMediaOption(ElectraMediaOptions::ElectraGetLicenseKeyData))
					{
						check(Param.IsType(FVariantValue::EDataType::TypeFString));
						return FVariantValue(SafeOptions->GetMediaOption(ElectraMediaOptions::ElectraGetLicenseKeyData, Param.GetFString()));
					}
					break;
				}

				case EOptionType::CustomAnalyticsMetric:
				{
					check(Param.IsType(FVariantValue::EDataType::TypeFString));
					if (Param.IsType(FVariantValue::EDataType::TypeFString))
					{
						FName OptionKey(*Param.GetFString());
						if (SafeOptions->HasMediaOption(OptionKey))
						{
							return FVariantValue(SafeOptions->GetMediaOption(OptionKey, FString()));
						}
					}
					break;
				}

				case EOptionType::PlaystartPosFromSeekPositions:
				{
					if (SafeOptions->HasMediaOption(ElectraMediaOptions::ElectraGetPlaystartPosFromSeekPositions))
					{
						check(Param.IsType(FVariantValue::EDataType::TypeSharedPointer));

						TSharedPtr<TArray<FTimespan>, ESPMode::ThreadSafe> PosArray = Param.GetSharedPointer<TArray<FTimespan>>();
						if (PosArray.IsValid())
						{
							TSharedPtr<FElectraSeekablePositions, ESPMode::ThreadSafe> Res = StaticCastSharedPtr<FElectraSeekablePositions, IMediaOptions::FDataContainer, ESPMode::ThreadSafe>(SafeOptions->GetMediaOption(ElectraMediaOptions::ElectraGetPlaystartPosFromSeekPositions, MakeShared<FElectraSeekablePositions, ESPMode::ThreadSafe>(*PosArray)));
							if (Res.IsValid() && Res->Data.Num())
							{
								return FVariantValue(int64(Res->Data[0].GetTicks())); // return HNS
							}
						}
						return FVariantValue();
					}
					break;
				}

				default:
				{
					break;
				}
			}
		}
	}
	return FVariantValue();
}


void FElectraPlayerPlugin::SendMediaEvent(EPlayerEvent Event)
{
	if (Event == EPlayerEvent::MetadataChanged)
	{
		SetMetadataChanged();
	}
	FScopeLock lock(&CallbackPointerLock);
	if (EventSink)
	{
		EventSink->ReceiveMediaEvent((EMediaEvent)Event);
	}
}


void FElectraPlayerPlugin::OnVideoFlush()
{
	TRange<FTimespan> AllTime(FTimespan::MinValue(), FTimespan::MaxValue());
	TSharedPtr<IMediaTextureSample, ESPMode::ThreadSafe> FlushSample;
	while (GetSamples().FetchVideo(AllTime, FlushSample))
	{ }
}


void FElectraPlayerPlugin::OnAudioFlush()
{
	TRange<FTimespan> AllTime(FTimespan::MinValue(), FTimespan::MaxValue());
	TSharedPtr<IMediaAudioSample, ESPMode::ThreadSafe> FlushSample;
	while (GetSamples().FetchAudio(AllTime, FlushSample))
	{ }
}


void FElectraPlayerPlugin::OnSubtitleFlush()
{
	TRange<FTimespan> AllTime(FTimespan::MinValue(), FTimespan::MaxValue());
	TSharedPtr<IMediaOverlaySample, ESPMode::ThreadSafe> FlushSample;
	while (GetSamples().FetchSubtitle(AllTime, FlushSample))
	{ }
}




void FElectraPlayerPlugin::PresentVideoFrame(const FVideoDecoderOutputPtr& InVideoFrame)
{
	FScopeLock SampleLock(&MediaSamplesLock);

	FVideoDecoderOutputPtr VideoFrame = InVideoFrame;
	TSharedPtr<FElectraTextureSamplePool, ESPMode::ThreadSafe> TexturePool = OutputTexturePool;
	if (VideoFrame.IsValid() && TexturePool.IsValid())
	{
		FElectraTextureSampleRef TextureSample = TexturePool->AcquireShared();
		TextureSample->Initialize(VideoFrame.Get());
		MediaSamples->AddVideo(TextureSample);
	}
}


void FElectraPlayerPlugin::PresentAudioFrame(const IAudioDecoderOutputPtr& InAudioFrame)
{
	FScopeLock SampleLock(&MediaSamplesLock);

	IAudioDecoderOutputPtr AudioFrame = InAudioFrame;
	if (AudioFrame.IsValid())
	{
		TSharedRef<FElectraPlayerAudioSample, ESPMode::ThreadSafe> AudioSample = OutputAudioPool.AcquireShared();
		AudioSample->Initialize(InAudioFrame);
		MediaSamples->AddAudio(AudioSample);
	}
}


void FElectraPlayerPlugin::PresentSubtitleSample(const ISubtitleDecoderOutputPtr& InSubtitleSample)
{
	FScopeLock SampleLock(&MediaSamplesLock);

	ISubtitleDecoderOutputPtr Subtitle = InSubtitleSample;
	if (Subtitle.IsValid())
	{
		TSharedRef<FElectraSubtitleSample, ESPMode::ThreadSafe> SubtitleSample = MakeShared<FElectraSubtitleSample, ESPMode::ThreadSafe>();
		SubtitleSample->Subtitle = InSubtitleSample;
		MediaSamples->AddSubtitle(SubtitleSample);
	}
}


void FElectraPlayerPlugin::PresentMetadataSample(const IMetaDataDecoderOutputPtr& InMetadataFrame)
{
	FScopeLock SampleLock(&MediaSamplesLock);

	IMetaDataDecoderOutputPtr MetadataFrame = InMetadataFrame;
	if (MetadataFrame.IsValid())
	{
		TSharedRef<FElectraBinarySample, ESPMode::ThreadSafe> MetaDataSample = MakeShared<FElectraBinarySample, ESPMode::ThreadSafe>();
		MetaDataSample->Metadata = InMetadataFrame;
		MediaSamples->AddMetadata(MetaDataSample);
	}
}


bool FElectraPlayerPlugin::CanReceiveVideoSamples(int32 NumFrames)
{
	FScopeLock SampleLock(&MediaSamplesLock);
	return MediaSamples->CanReceiveVideoSamples(NumFrames);
}


bool FElectraPlayerPlugin::CanReceiveAudioSamples(int32 NumFrames)
{
	FScopeLock SampleLock(&MediaSamplesLock);
	return MediaSamples->CanReceiveAudioSamples(NumFrames);
}

FString FElectraPlayerPlugin::GetVideoAdapterName() const
{
	return GRHIAdapterName;
}


TSharedPtr<IElectraPlayerResourceDelegate, ESPMode::ThreadSafe> FElectraPlayerPlugin::GetResourceDelegate() const
{
	return PlayerResourceDelegate;
}

//-----------------------------------------------------------------------------

// IMediaPlayer interface

FGuid FElectraPlayerPlugin::GetPlayerPluginGUID() const
{
	static FGuid PlayerPluginGUID(0x94ee3f80, 0x8e604292, 0xb4d24dd5, 0xfdade1c2);
	return PlayerPluginGUID;
}

FString FElectraPlayerPlugin::GetInfo() const
{
	return TEXT("No information available");
}

IMediaSamples& FElectraPlayerPlugin::GetSamples()
{
	FScopeLock SampleLock(&MediaSamplesLock);
	return *MediaSamples;
}

FString FElectraPlayerPlugin::GetStats() const
{
	return TEXT("ElectraPlayer: GetStats: <empty>?");
}

IMediaTracks& FElectraPlayerPlugin::GetTracks()
{
	return *this;
}

bool FElectraPlayerPlugin::Open(const FString& Url, const IMediaOptions* Options)
{
	return Open(Url, Options, nullptr);
}

bool FElectraPlayerPlugin::Open(const FString& Url, const IMediaOptions* Options, const FMediaPlayerOptions* InPlayerOptions)
{
	PlayerUniqueID = ++NextPlayerUniqueID;

	if (Options == nullptr)
	{
		UE_LOG(LogElectraPlayerPlugin, Error, TEXT("[%u] IMediaPlayer::Open: Options == nullptr"), PlayerUniqueID);

		FScopeLock lock(&CallbackPointerLock);
		if (EventSink)
		{
			EventSink->ReceiveMediaEvent(EMediaEvent::MediaOpenFailed);
		}

		return false;
	}

	// Get the safe option interface to poll for changes during playback.
	CallbackPointerLock.Lock();
	OptionInterface = StaticCastSharedPtr<IElectraSafeMediaOptionInterface>(Options->GetMediaOption(ElectraMediaOptions::GetSafeMediaOptions, TSharedPtr<IElectraSafeMediaOptionInterface, ESPMode::ThreadSafe>()));
	CallbackPointerLock.Unlock();
	UE_LOG(LogElectraPlayerPlugin, Log, TEXT("[%u] IMediaPlayer::Open"), PlayerUniqueID);

	IElectraPlayerInterface::FPlaystartOptions LocalPlaystartOptions;

	// Get playstart options from passed options, if they exist.
	FName Environment;
	if (InPlayerOptions)
	{
		if (InPlayerOptions->SeekTimeType != EMediaPlayerOptionSeekTimeType::Ignored)
		{
			LocalPlaystartOptions.TimeOffset = InPlayerOptions->SeekTime;
		}
		if (InPlayerOptions->TrackSelection == EMediaPlayerOptionTrackSelectMode::UseLanguageCodes)
		{
			if (InPlayerOptions->TracksByLanguage.Video.Len())
			{
				LocalPlaystartOptions.InitialVideoTrackAttributes.Language_RFC4647 = InPlayerOptions->TracksByLanguage.Video;
				UE_LOG(LogElectraPlayerPlugin, Verbose, TEXT("[%u] IMediaPlayer::Open: Asking for initial video language \"%s\""), PlayerUniqueID, *InPlayerOptions->TracksByLanguage.Video);
			}
			if (InPlayerOptions->TracksByLanguage.Audio.Len())
			{
				LocalPlaystartOptions.InitialAudioTrackAttributes.Language_RFC4647 = InPlayerOptions->TracksByLanguage.Audio;
				UE_LOG(LogElectraPlayerPlugin, Verbose, TEXT("[%u] IMediaPlayer::Open: Asking for initial audio language \"%s\""), PlayerUniqueID, *InPlayerOptions->TracksByLanguage.Audio);
			}
			if (InPlayerOptions->TracksByLanguage.Subtitle.Len())
			{
				LocalPlaystartOptions.InitialSubtitleTrackAttributes.Language_RFC4647 = InPlayerOptions->TracksByLanguage.Subtitle;
				UE_LOG(LogElectraPlayerPlugin, Verbose, TEXT("[%u] IMediaPlayer::Open: Asking for initial subtitle language \"%s\""), PlayerUniqueID, *InPlayerOptions->TracksByLanguage.Subtitle);
			}
		}
		else if (InPlayerOptions->TrackSelection == EMediaPlayerOptionTrackSelectMode::UseTrackOptionIndices)
		{
			LocalPlaystartOptions.InitialAudioTrackAttributes.TrackIndexOverride = InPlayerOptions->Tracks.Audio;
			LocalPlaystartOptions.InitialSubtitleTrackAttributes.TrackIndexOverride = InPlayerOptions->Tracks.Subtitle;
		}
		const FVariant* Env = InPlayerOptions->InternalCustomOptions.Find(MediaPlayerOptionValues::Environment());
		Environment = Env ? Env->GetValue<FName>() : Environment;
	}
	bool bNoPreloading = Options->GetMediaOption(ElectraMediaOptions::ElectraNoPreloading, (bool)false);
	if (bNoPreloading)
	{
		LocalPlaystartOptions.bDoNotPreload = true;
		UE_LOG(LogElectraPlayerPlugin, Verbose, TEXT("[%u] IMediaPlayer::Open: No preloading after opening media"), PlayerUniqueID);
	}

	// Set up options to initialize the internal player with.
	Electra::FParamDict PlayerOptions;
	PlayerOptions.Set(ElectraMediaOptions::KeyUniquePlayerID, Electra::FVariantValue((int64)PlayerUniqueID));
	for(auto &CodecOption : ElectraMediaOptions::CodecOptions)
	{
		FString Value = Options->GetMediaOption(CodecOption, FString());
		if (Value.Len())
		{
			PlayerOptions.Set(CodecOption, Electra::FVariantValue(Value));
		}
	}
	// Required playlist properties?
	FString PlaylistProperties = Options->GetMediaOption(ElectraMediaOptions::PlaylistProperties, FString());
	if (PlaylistProperties.Len())
	{
		PlayerOptions.Set(ElectraMediaOptions::PlaylistProperties, Electra::FVariantValue(PlaylistProperties));
	}

	if (InPlayerOptions && InPlayerOptions->InternalCustomOptions.Find(MediaPlayerOptionValues::ParseTimecodeInfo()))
	{
		PlayerOptions.Set(ElectraMediaOptions::OptionKeyParseTimecodeInfo, FVariantValue());
	}

	// Check for one-time initialization options that can't be changed during playback.
	int64 InitialStreamBitrate = Options->GetMediaOption(ElectraMediaOptions::ElectraInitialBitrate, (int64)-1);
	if (InitialStreamBitrate > 0)
	{
		PlayerOptions.Set(TEXT("initial_bitrate"), Electra::FVariantValue(InitialStreamBitrate));
		UE_LOG(LogElectraPlayerPlugin, Verbose, TEXT("[%u] IMediaPlayer::Open: Using initial bitrate of %d bits/second"), PlayerUniqueID, (int32)InitialStreamBitrate);
	}
	FString MediaMimeType = Options->GetMediaOption(ElectraMediaOptions::Mimetype, FString());
	if (MediaMimeType.Len())
	{
		PlayerOptions.Set(TEXT("mime_type"), Electra::FVariantValue(MediaMimeType));
		UE_LOG(LogElectraPlayerPlugin, Verbose, TEXT("[%u] IMediaPlayer::Open: Setting media mime type to \"%s\""), PlayerUniqueID, *MediaMimeType);
	}
	int64 MaxVerticalHeight = Options->GetMediaOption(ElectraMediaOptions::MaxElectraVerticalResolution, (int64)-1);
	if (MaxVerticalHeight > 0)
	{
		PlayerOptions.Set(TEXT("max_resoY"), Electra::FVariantValue(MaxVerticalHeight));
		UE_LOG(LogElectraPlayerPlugin, Verbose, TEXT("[%u] IMediaPlayer::Open: Limiting vertical resolution to %d for all streams"), PlayerUniqueID, (int32)MaxVerticalHeight);
	}
	int64 MaxVerticalHeightAt60 = Options->GetMediaOption(ElectraMediaOptions::MaxElectraVerticalResolutionOf60fpsVideos, (int64)-1);
	if (MaxVerticalHeightAt60 > 0)
	{
		PlayerOptions.Set(TEXT("max_resoY_above_30fps"), Electra::FVariantValue(MaxVerticalHeightAt60));
		UE_LOG(LogElectraPlayerPlugin, Verbose, TEXT("[%u] IMediaPlayer::Open: Limiting vertical resolution to %d for streams >30fps"), PlayerUniqueID, (int32)MaxVerticalHeightAt60);
	}
	double LiveEdgeDistanceForNormalPresentation = Options->GetMediaOption(ElectraMediaOptions::ElectraLivePresentationOffset, (double)-1.0);
	if (LiveEdgeDistanceForNormalPresentation > 0.0)
	{
		PlayerOptions.Set(TEXT("seekable_range_live_end_offset"), Electra::FVariantValue(Electra::FTimeValue().SetFromSeconds(LiveEdgeDistanceForNormalPresentation)));
		UE_LOG(LogElectraPlayerPlugin, Verbose, TEXT("[%u] IMediaPlayer::Open: Setting distance to live edge for normal presentations to %.3f seconds"), PlayerUniqueID, LiveEdgeDistanceForNormalPresentation);
	}
	double LiveEdgeDistanceForAudioOnlyPresentation = Options->GetMediaOption(ElectraMediaOptions::ElectraLiveAudioPresentationOffset, (double)-1.0);
	if (LiveEdgeDistanceForAudioOnlyPresentation > 0.0)
	{
		PlayerOptions.Set(TEXT("seekable_range_live_end_offset_audioonly"), Electra::FVariantValue(Electra::FTimeValue().SetFromSeconds(LiveEdgeDistanceForAudioOnlyPresentation)));
		UE_LOG(LogElectraPlayerPlugin, Verbose, TEXT("[%u] IMediaPlayer::Open: Setting distance to live edge for audio-only presentation to %.3f seconds"), PlayerUniqueID, LiveEdgeDistanceForAudioOnlyPresentation);
	}
	bool bUseConservativeLiveEdgeDistance = Options->GetMediaOption(ElectraMediaOptions::ElectraLiveUseConservativePresentationOffset, (bool)false);
	if (bUseConservativeLiveEdgeDistance)
	{
		PlayerOptions.Set(TEXT("seekable_range_live_end_offset_conservative"), Electra::FVariantValue(bUseConservativeLiveEdgeDistance));
		UE_LOG(LogElectraPlayerPlugin, Verbose, TEXT("[%u] IMediaPlayer::Open: Using conservative live edge for distance calculation"), PlayerUniqueID);
	}
	bool bThrowErrorWhenRebuffering = Options->GetMediaOption(ElectraMediaOptions::ElectraThrowErrorWhenRebuffering, (bool)false);
	if (bThrowErrorWhenRebuffering)
	{
		PlayerOptions.Set(TEXT("throw_error_when_rebuffering"), Electra::FVariantValue(bThrowErrorWhenRebuffering));
		UE_LOG(LogElectraPlayerPlugin, Verbose, TEXT("[%u] IMediaPlayer::Open: Throw playback error when rebuffering"), PlayerUniqueID);
	}
	FString CDNHTTPStatusDenyStream = Options->GetMediaOption(ElectraMediaOptions::ElectraGetDenyStreamCode, FString());
	if (CDNHTTPStatusDenyStream.Len())
	{
		int32 HTTPStatus = -1;
		LexFromString(HTTPStatus, *CDNHTTPStatusDenyStream);
		if (HTTPStatus > 0 && HTTPStatus < 1000)
		{
			PlayerOptions.Set(TEXT("abr:cdn_deny_httpstatus"), Electra::FVariantValue((int64)HTTPStatus));
			UE_LOG(LogElectraPlayerPlugin, Verbose, TEXT("[%u] IMediaPlayer::Open: CDN HTTP status %d will deny a stream permanently"), PlayerUniqueID, HTTPStatus);
		}
	}

	// Check if there is an environment specified in which this player is used.
	// Certain optimization settings apply for dedicated environments.
	if (Environment == MediaPlayerOptionValues::Environment_Preview() || Environment == MediaPlayerOptionValues::Environment_Sequencer())
	{
		PlayerOptions.Set(TEXT("worker_threads"), Electra::FVariantValue(FString(TEXT("worker"))));
	}

	// Check for options that can be changed during playback and apply them at startup already.
	// If a media source supports the MaxResolutionForMediaStreaming option then we can override the max resolution.
	int64 DefaultValue = 0;
	int64 MaxVerticalStreamResolution = Options->GetMediaOption(ElectraMediaOptions::MaxResolutionForMediaStreaming, DefaultValue);
	if (MaxVerticalStreamResolution != 0)
	{
		UE_LOG(LogElectraPlayerPlugin, Log, TEXT("[%u] IMediaPlayer::Open: Limiting max resolution to %d"), PlayerUniqueID, (int32)MaxVerticalStreamResolution);
		LocalPlaystartOptions.MaxVerticalStreamResolution = (int32)MaxVerticalStreamResolution;
	}

	int64 MaxBandwidthForStreaming = Options->GetMediaOption(ElectraMediaOptions::ElectraMaxStreamingBandwidth, (int64)0);
	if (MaxBandwidthForStreaming > 0)
	{
		UE_LOG(LogElectraPlayerPlugin, Log, TEXT("[%u] Limiting max streaming bandwidth to %d bps"), PlayerUniqueID, (int32)MaxBandwidthForStreaming);
		LocalPlaystartOptions.MaxBandwidthForStreaming = (int32)MaxBandwidthForStreaming;
	}

	bMetadataChanged = false;
	CurrentMetadata.Reset();

	// Check if we can get a segment cache interface for this playback request...
	TSharedPtr<FElectraPlayerDataCacheContainer, ESPMode::ThreadSafe> ElectraPlayerDataCacheContainer;
	TSharedPtr<IMediaOptions::FDataContainer, ESPMode::ThreadSafe> ElectraPlayerDataCacheDefaultValue;
	TSharedPtr<IMediaOptions::FDataContainer, ESPMode::ThreadSafe> DataContainer = Options->GetMediaOption(ElectraMediaOptions::ElectraPlayerDataCache, ElectraPlayerDataCacheDefaultValue);
	if (DataContainer.IsValid())
	{
		ElectraPlayerDataCacheContainer = StaticCastSharedPtr<FElectraPlayerDataCacheContainer, IMediaOptions::FDataContainer, ESPMode::ThreadSafe>(DataContainer);
		if (ElectraPlayerDataCacheContainer.IsValid())
		{
			LocalPlaystartOptions.ExternalDataCache = ElectraPlayerDataCacheContainer->Data;
		}
	}

	return Player->OpenInternal(Url, PlayerOptions, LocalPlaystartOptions, IElectraPlayerInterface::EOpenType::Media);
}

//-----------------------------------------------------------------------------
/**
 *
 */
bool FElectraPlayerPlugin::Open(const TSharedRef<FArchive, ESPMode::ThreadSafe>& Archive, const FString& OriginalUrl, const IMediaOptions* /*Options*/)
{
	// we support playback only from an external file, not from a "resource" (e.g. a packaged asset)
	UE_LOG(LogElectraPlayerPlugin, Error, TEXT("[%u] IMediaPlayer::Archive"), PlayerUniqueID);
	unimplemented();
	return false;
}


//-----------------------------------------------------------------------------
/**
*	Internal Close / Shutdown player
*/
void FElectraPlayerPlugin::Close()
{
	CallbackPointerLock.Lock();
	OptionInterface.Reset();
	CallbackPointerLock.Unlock();
	Player->CloseInternal(true);
}

//-----------------------------------------------------------------------------
/**
 *	Tick the player from the game thread
 */
void FElectraPlayerPlugin::TickInput(FTimespan DeltaTime, FTimespan Timecode)
{
	OutputTexturePool->Tick();
	Player->Tick(DeltaTime, Timecode);
}

FVariant FElectraPlayerPlugin::GetMediaInfo(FName InInfoName) const
{
	return Player.IsValid() ? Player->GetMediaInfo(InInfoName).ToFVariant() : FVariant();
}

//-----------------------------------------------------------------------------
/**
	Returns the current metadata, if any.
*/
TSharedPtr<TMap<FString, TArray<TUniquePtr<IMediaMetadataItem>>>, ESPMode::ThreadSafe> FElectraPlayerPlugin::GetMediaMetadata() const
{
	if (bMetadataChanged && Player.IsValid())
	{
		TSharedPtr<TMap<FString, TArray<TSharedPtr<Electra::IMediaStreamMetadata::IItem, ESPMode::ThreadSafe>>>, ESPMode::ThreadSafe> PlayerMeta = Player->GetMediaMetadata();
		if (PlayerMeta.IsValid())
		{
			TSharedPtr<TMap<FString, TArray<TUniquePtr<IMediaMetadataItem>>>, ESPMode::ThreadSafe> NewMeta(new TMap<FString, TArray<TUniquePtr<IMediaMetadataItem>>>);
			for(auto& PlayerMetaItem : *PlayerMeta)
			{
				TArray<TUniquePtr<IMediaMetadataItem>>& NewItemList = NewMeta->Emplace(PlayerMetaItem.Key);
				for(auto& PlayerMetaListItem : PlayerMetaItem.Value)
				{
					if (PlayerMetaListItem.IsValid())
					{
						NewItemList.Emplace(MakeUnique<FStreamMetadataItem>(PlayerMetaListItem));
					}
				}
			}
			bMetadataChanged = false;
			CurrentMetadata = MoveTemp(NewMeta);
		}
	}
	return CurrentMetadata;
}

void FElectraPlayerPlugin::SetMetadataChanged()
{
	bMetadataChanged = true;
}


//-----------------------------------------------------------------------------
/**
	Get special feature flags states
*/
bool FElectraPlayerPlugin::GetPlayerFeatureFlag(EFeatureFlag flag) const
{
	switch(flag)
	{
		case EFeatureFlag::AllowShutdownOnClose:
			return Player->IsKillAfterCloseAllowed();
		case EFeatureFlag::UsePlaybackTimingV2:
			return true;
		case EFeatureFlag::PlayerUsesInternalFlushOnSeek:
			return true;
		case EFeatureFlag::IsTrackSwitchSeamless:
			return true;
		case EFeatureFlag::PlayerSelectsDefaultTracks:
			return true;
		default:
			break;
	}
	return IMediaPlayer::GetPlayerFeatureFlag(flag);
}

//-----------------------------------------------------------------------------
/**
	Set a notification to be signaled once any async tear down of the instance is done
*/
bool FElectraPlayerPlugin::SetAsyncResourceReleaseNotification(IAsyncResourceReleaseNotificationRef AsyncResourceReleaseNotification)
{
	class FAsyncResourceReleaseNotifyContainer : public IElectraPlayerInterface::IAsyncResourceReleaseNotifyContainer
	{
	public:
		FAsyncResourceReleaseNotifyContainer(IAsyncResourceReleaseNotificationRef InAsyncResourceReleaseNotification) : AsyncResourceReleaseNotification(InAsyncResourceReleaseNotification) {}
		virtual void Signal(uint32 ResourceFlags) override { AsyncResourceReleaseNotification->Signal(ResourceFlags); }
	private:
		IAsyncResourceReleaseNotificationRef AsyncResourceReleaseNotification;
	};

	Player->SetAsyncResourceReleaseNotification(new FAsyncResourceReleaseNotifyContainer(AsyncResourceReleaseNotification));
	return true;
}

uint32 FElectraPlayerPlugin::GetNewResourcesOnOpen() const
{
	// Electra will recreate all decoder related resources on each open call
	// (a simplification: we also recreate the texture pool should it change sizes on SOME platforms - but we reoprt the release only per instance, so thisb matches that)
	return IMediaPlayerLifecycleManagerDelegate::ResourceFlags_Decoder;
}

//////////////////////////////////////////////////////////////////////////
// IMediaControl impl

//-----------------------------------------------------------------------------
/**
 *	Currently, we cannot do anything.. well, at least we can play!
 */
bool FElectraPlayerPlugin::CanControl(EMediaControl Control) const
{
	EMediaState CurrentState = GetState();
	if (Control == EMediaControl::BlockOnFetch)
	{
		return CurrentState == EMediaState::Playing || CurrentState == EMediaState::Paused;
	}
	else if (Control == EMediaControl::Pause)
	{
		return CurrentState == EMediaState::Playing;
	}
	else if (Control == EMediaControl::Resume)
	{
		return CurrentState == EMediaState::Paused || CurrentState == EMediaState::Stopped;
	}
	else if (Control == EMediaControl::Seek || Control == EMediaControl::Scrub)
	{
		return CurrentState == EMediaState::Playing || CurrentState == EMediaState::Paused || CurrentState == EMediaState::Stopped;
	}
	else if (Control == EMediaControl::PlaybackRange)
	{
		return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
/**
 *	Rate is only real-time
 */
float FElectraPlayerPlugin::GetRate() const
{
	return Player->GetRate();
}

//-----------------------------------------------------------------------------
/**
 *	Expose player state
 */
EMediaState FElectraPlayerPlugin::GetState() const
{
	return (EMediaState)Player->GetState();
}

//-----------------------------------------------------------------------------
/**
 *	Expose player status
 */
EMediaStatus FElectraPlayerPlugin::GetStatus() const
{
	return (EMediaStatus)Player->GetStatus();
}


bool FElectraPlayerPlugin::IsLooping() const
{
	return Player->IsLooping();
}


bool FElectraPlayerPlugin::SetLooping(bool bLooping)
{
	return Player->SetLooping(bLooping);
}

//-----------------------------------------------------------------------------
/**
 *	Only return real-time playback for the moment..
 */
TRangeSet<float> FElectraPlayerPlugin::GetSupportedRates(EMediaRateThinning Thinning) const
{
	return Player->GetSupportedRates(Thinning == EMediaRateThinning::Thinned ? IElectraPlayerInterface::EPlayRateType::Thinned : IElectraPlayerInterface::EPlayRateType::Unthinned);
}


FTimespan FElectraPlayerPlugin::GetTime() const
{
	return Player->GetTime();
}


FTimespan FElectraPlayerPlugin::GetDuration() const
{
	return Player->GetDuration();
}


bool FElectraPlayerPlugin::SetRate(float Rate)
{
	UE_LOG(LogElectraPlayerPlugin, Log, TEXT("[%u] IMediaControls::SetRate(%f)"), PlayerUniqueID, Rate);
	CSV_EVENT(ElectraPlayer, TEXT("Setting Rate"));

	return Player->SetRate(Rate);
}


bool FElectraPlayerPlugin::Seek(const FTimespan& Time, const FMediaSeekParams& InAdditionalParams)
{
	UE_LOG(LogElectraPlayerPlugin, Verbose, TEXT("[%u] IMediaControls::Seek() to %s"), PlayerUniqueID, *Time.ToString(TEXT("%h:%m:%s.%f")));
	CSV_EVENT(ElectraPlayer, TEXT("Seeking"));
	IElectraPlayerInterface::FSeekParam sp;
	sp.SequenceIndex = InAdditionalParams.NewSequenceIndex;
	return Player->Seek(Time, sp);
}

TRange<FTimespan> FElectraPlayerPlugin::GetPlaybackTimeRange(EMediaTimeRangeType InRangeToGet) const
{
	return Player->GetPlaybackRange(static_cast<IElectraPlayerInterface::ETimeRangeType>(InRangeToGet));
}

bool FElectraPlayerPlugin::SetPlaybackTimeRange(const TRange<FTimespan>& InTimeRange)
{
	IElectraPlayerInterface::FPlaybackRange Range;
	Range.Start = InTimeRange.GetLowerBoundValue();
	Range.End = InTimeRange.GetUpperBoundValue();
	Player->SetPlaybackRange(Range);
	return true;
}

bool FElectraPlayerPlugin::QueryCacheState(EMediaCacheState State, TRangeSet<FTimespan>& OutTimeRanges) const
{
	// Note: The data of time ranges returned here will not actually get "cached" as
	//       it is always only transient. We thus report the ranges only for `Loaded` and `Loading`,
	//       but never for `Cached`!
	switch(State)
	{
		case EMediaCacheState::Loaded:
		case EMediaCacheState::Loading:
		case EMediaCacheState::Pending:
		{
			// When asked to provide what's already loaded we look at what we have in the sample queue
			// and add that to the result. These samples have already left the player but are ready
			// for use.
			if (State == EMediaCacheState::Loaded)
			{
				TArray<TRange<FMediaTimeStamp>> QueuedRange;
				FScopeLock SampleLock(&MediaSamplesLock);
				if (MediaSamples.IsValid() && MediaSamples->PeekVideoSampleTimeRanges(QueuedRange) && QueuedRange.Num())
				{
					OutTimeRanges.Add(TRange<FTimespan>(QueuedRange[0].GetLowerBoundValue().Time, QueuedRange.Last().GetUpperBoundValue().Time));
				}
			}

			// Get the data time range from the player. It returns both current and future data in one call, so we
			// separate the result here based on what is being asked for.
			IElectraPlayerInterface::FStreamBufferInfo vidBuf, audBuf;
			bool bHaveVid = Player->GetStreamBufferInformation(vidBuf, IElectraPlayerInterface::EPlayerTrackType::Video);
			bool bHaveAud = !bHaveVid ? Player->GetStreamBufferInformation(audBuf, IElectraPlayerInterface::EPlayerTrackType::Audio) : false;
			const IElectraPlayerInterface::FStreamBufferInfo* Buffer = bHaveVid ? &vidBuf : bHaveAud ? &audBuf : nullptr;
			const TArray<IElectraPlayerInterface::FStreamBufferInfo::FTimeRange>* tr = nullptr;
			if (Buffer)
			{
				switch(State)
				{
					case EMediaCacheState::Loaded:
						tr = &Buffer->TimeEnqueued;
						break;
					case EMediaCacheState::Loading:
						tr = &Buffer->TimeAvailable;
						break;
					case EMediaCacheState::Pending:
						tr = &Buffer->TimeRequested;
						break;
				}
			}
			for(int32 i=0,iMax=tr?tr->Num():0; i<iMax; ++i)
			{
				OutTimeRanges.Add(TRange<FTimespan>(tr->operator[](i).Start.Time, tr->operator[](i).End.Time));
			}
			return true;
		}
	}
	return false;
}


bool FElectraPlayerPlugin::GetAudioTrackFormat(int32 TrackIndex, int32 FormatIndex, FMediaAudioTrackFormat& OutFormat) const
{
	IElectraPlayerInterface::FAudioTrackFormat Format;
	if (!Player->GetAudioTrackFormat(TrackIndex, FormatIndex, Format))
	{
		return false;
	}
	OutFormat.BitsPerSample = Format.BitsPerSample;
	OutFormat.NumChannels = Format.NumChannels;
	OutFormat.SampleRate = Format.SampleRate;
	OutFormat.TypeName = Format.TypeName;
	return true;
}


bool FElectraPlayerPlugin::GetVideoTrackFormat(int32 TrackIndex, int32 FormatIndex, FMediaVideoTrackFormat& OutFormat) const
{
	IElectraPlayerInterface::FVideoTrackFormat Format;
	if (!Player->GetVideoTrackFormat(TrackIndex, FormatIndex, Format))
	{
		return false;
	}
	OutFormat.Dim = Format.Dim;
	OutFormat.FrameRate = Format.FrameRate;
	OutFormat.FrameRates = Format.FrameRates;
	OutFormat.TypeName = Format.TypeName;
	return true;
}


int32 FElectraPlayerPlugin::GetNumTracks(EMediaTrackType TrackType) const
{
	return Player->GetNumTracks((IElectraPlayerInterface::EPlayerTrackType)TrackType);
}


int32 FElectraPlayerPlugin::GetNumTrackFormats(EMediaTrackType TrackType, int32 TrackIndex) const
{
	return Player->GetNumTrackFormats((IElectraPlayerInterface::EPlayerTrackType)TrackType, TrackIndex);
}


int32 FElectraPlayerPlugin::GetSelectedTrack(EMediaTrackType TrackType) const
{
	return Player->GetSelectedTrack((IElectraPlayerInterface::EPlayerTrackType)TrackType);
}


FText FElectraPlayerPlugin::GetTrackDisplayName(EMediaTrackType TrackType, int32 TrackIndex) const
{
	return Player->GetTrackDisplayName((IElectraPlayerInterface::EPlayerTrackType)TrackType, TrackIndex);
}


int32 FElectraPlayerPlugin::GetTrackFormat(EMediaTrackType TrackType, int32 TrackIndex) const
{
	return Player->GetTrackFormat((IElectraPlayerInterface::EPlayerTrackType)TrackType, TrackIndex);
}


FString FElectraPlayerPlugin::GetTrackLanguage(EMediaTrackType TrackType, int32 TrackIndex) const
{
	return Player->GetTrackLanguage((IElectraPlayerInterface::EPlayerTrackType)TrackType, TrackIndex);
}


FString FElectraPlayerPlugin::GetTrackName(EMediaTrackType TrackType, int32 TrackIndex) const
{
	return Player->GetTrackName((IElectraPlayerInterface::EPlayerTrackType)TrackType, TrackIndex);
}


bool FElectraPlayerPlugin::SelectTrack(EMediaTrackType TrackType, int32 TrackIndex)
{
	return Player->SelectTrack((IElectraPlayerInterface::EPlayerTrackType)TrackType, TrackIndex);
}


bool FElectraPlayerPlugin::SetTrackFormat(EMediaTrackType TrackType, int32 TrackIndex, int32 FormatIndex)
{
	return false;
}


bool FElectraPlayerPlugin::SetVideoTrackFrameRate(int32 TrackIndex, int32 FormatIndex, float FrameRate)
{
	return false;
}
