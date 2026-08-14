// Copyright Epic Games, Inc. All Rights Reserved.

#include "FootageIngest/Utils/MetaHumanMediaSourceReader.h"
#include "MetaHumanCaptureSourceLog.h"

#include "IMediaAudioSample.h"
#include "IMediaTextureSample.h"

#if PLATFORM_WINDOWS && !UE_SERVER

#include "Windows/WindowsHWrapper.h"
#include "Windows/AllowWindowsPlatformTypes.h"

THIRD_PARTY_INCLUDES_START

#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <propvarutil.h>
#include "Microsoft/COMPointer.h"

THIRD_PARTY_INCLUDES_END

#include "Windows/HideWindowsPlatformTypes.h"

struct FMetaHumanMediaAudioSample : public IMediaAudioSample
{
	//  IMediaAudioSample methods for the sample returned by Next()
	virtual const void* GetBuffer() override { return Buffer.GetData(); }
	virtual uint32 GetChannels() const override { return Channels; }
	virtual FTimespan GetDuration() const override { return Duration; }
	virtual EMediaAudioSampleFormat GetFormat() const override { return Format; }
	virtual uint32 GetSampleRate() const override { return SampleRate; }
	virtual uint32 GetFrames() const override { return Frames; }
	virtual FMediaTimeStamp GetTime() const override { return Time; }

	TArray<uint8> Buffer;
	uint32 Channels;
	FTimespan Duration;
	EMediaAudioSampleFormat Format;
	uint32 Frames;
	uint32 SampleRate;
	FMediaTimeStamp Time;
};

class FMetaHumanMediaAudioSourceReader : public IMetaHumanMediaAudioSourceReader
{
public:
	FMetaHumanMediaAudioSourceReader();
	virtual ~FMetaHumanMediaAudioSourceReader() override;

	virtual bool Open(const FString& URL) override;
	virtual FTimespan GetTotalDuration() const override;
	virtual IMediaAudioSample* Next() override;	// The sample is valid till the next call to Next() or Close()
	virtual EMediaAudioSampleFormat GetFormat() const override;
	virtual uint32 GetChannels() const override;
	virtual uint32 GetSampleRate() const override;
	virtual void Close() override;

protected:
	FTimespan TotalDuration;
	FMetaHumanMediaAudioSample Sample;

	TComPtr<IMFSourceReader> SourceReader;
};


FMetaHumanMediaAudioSourceReader::FMetaHumanMediaAudioSourceReader() : Sample()
{
}

FMetaHumanMediaAudioSourceReader::~FMetaHumanMediaAudioSourceReader()
{
}

bool FMetaHumanMediaAudioSourceReader::Open(const FString& URL)
{
	TComPtr<IMFMediaType> AudioMediaTypeIn;
	TComPtr<IMFMediaType> AudioMediaTypeOut;

	HRESULT Result = ::MFCreateSourceReaderFromURL(*URL, nullptr, &SourceReader);
	if (FAILED(Result))
	{
		UE_LOG(LogMetaHumanCaptureSource, Error, TEXT("Cannot open the audio file %s: %d"), *URL, Result);
		return false;
	}

	PROPVARIANT DurationVar;
	int64 DurationTicks;	// 10,000,000 == 1.0e7 units per second. Each unit corresponds to 100 nanoseconds.

	Result = SourceReader->GetPresentationAttribute(MF_SOURCE_READER_MEDIASOURCE, MF_PD_DURATION, &DurationVar);
	if (FAILED(Result))
	{
		UE_LOG(LogMetaHumanCaptureSource, Error, TEXT("Failed to retrieve the video file duration: %d"), Result);
		return false;
	}

	Result = ::PropVariantToInt64(DurationVar, &DurationTicks);
	PropVariantClear(&DurationVar);

	if (FAILED(Result))
	{
		UE_LOG(LogMetaHumanCaptureSource, Error, TEXT("Failed to retrieve the video file duration value: %d"), Result);
		return false;
	}

	TotalDuration = FTimespan(DurationTicks);

	Result = SourceReader->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS, false);
	if (FAILED(Result))
	{
		UE_LOG(LogMetaHumanCaptureSource, Error, TEXT("Stream Selection Failed: %d"), Result);
		return false;
	}

	Result = SourceReader->SetStreamSelection(MF_SOURCE_READER_FIRST_AUDIO_STREAM, true);
	if (FAILED(Result))
	{
		UE_LOG(LogMetaHumanCaptureSource, Error, TEXT("Stream Selection Failed: %d"), Result);
		return false;
	}

	Result = ::MFCreateMediaType(&AudioMediaTypeIn);
	if (FAILED(Result))
	{
		UE_LOG(LogMetaHumanCaptureSource, Error, TEXT("Media Type Configuration Failed: %d"), Result);
		return false;
	}

	Result = AudioMediaTypeIn->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
	if (FAILED(Result))
	{
		UE_LOG(LogMetaHumanCaptureSource, Error, TEXT("Media Type Configuration Failed: %d"), Result);
		return false;
	}

	Result = AudioMediaTypeIn->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
	if (FAILED(Result))
	{
		UE_LOG(LogMetaHumanCaptureSource, Error, TEXT("Media Type Configuration Failed: %d"), Result);
		return false;
	}

	Result = AudioMediaTypeIn->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
	if (FAILED(Result))
	{
		UE_LOG(LogMetaHumanCaptureSource, Error, TEXT("Media Type Configuration Failed: %d"), Result);
		return false;
	}


	Result = SourceReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, AudioMediaTypeIn);
	if (FAILED(Result))
	{
		UE_LOG(LogMetaHumanCaptureSource, Error, TEXT("Media Type Configuration Failed!: %d"), Result);
		return false;
	}

	Result = SourceReader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, &AudioMediaTypeOut);
	if (FAILED(Result))
	{
		UE_LOG(LogMetaHumanCaptureSource, Error, TEXT("Media Type Configuration Failed!: %d"), Result);
		return false;
	}

	WAVEFORMATEX* WaveFormatOut;
	uint32 WaveFormatSize;

	Result = ::MFCreateWaveFormatExFromMFMediaType(AudioMediaTypeOut, &WaveFormatOut, &WaveFormatSize);
	if (FAILED(Result))
	{
		UE_LOG(LogMetaHumanCaptureSource, Error, TEXT("Failed to retrieve the video file duration: %d"), Result);
		return false;
	}

	Sample.Channels = WaveFormatOut->nChannels;
	Sample.SampleRate = WaveFormatOut->nSamplesPerSec;
	Sample.Format = EMediaAudioSampleFormat::Int16;

	::CoTaskMemFree(WaveFormatOut);

	return true;
}

FTimespan FMetaHumanMediaAudioSourceReader::GetTotalDuration() const
{
	return TotalDuration;
}

IMediaAudioSample* FMetaHumanMediaAudioSourceReader::Next()
{
	TComPtr<IMFSample> MFSample;
	TComPtr<IMFMediaBuffer> Buffer;
	DWORD Flags;
	LONGLONG TimeStamp;
	LONGLONG Duration;

	HRESULT Result = SourceReader->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, nullptr, &Flags, &TimeStamp, &MFSample);
	if (FAILED(Result))
	{
		UE_LOG(LogMetaHumanCaptureSource, Error, TEXT("Failed to read the next sample: %d"), Result);
		return nullptr;
	}

	if (Flags & (MF_SOURCE_READERF_ERROR | MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED))
	{
		UE_LOG(LogMetaHumanCaptureSource, Error, TEXT("Failed to read the next sample: Flags(%d)"), Flags);
		return nullptr;
	}

	if (Flags & MF_SOURCE_READERF_ENDOFSTREAM)
	{
		//	Finished reading the stream
		return nullptr;
	}

	if (!MFSample)
	{
		UE_LOG(LogMetaHumanCaptureSource, Error, TEXT("Failed to read the next sample"));
		return nullptr;
	}

	Result = MFSample->GetSampleTime(&TimeStamp);
	if (FAILED(Result))
	{
		UE_LOG(LogMetaHumanCaptureSource, Error, TEXT("Failed to read the audio sample time: %d"), Result);
		return nullptr;
	}
	Sample.Time = FMediaTimeStamp(FTimespan(TimeStamp));

	Result = MFSample->GetSampleDuration(&Duration);
	if (FAILED(Result))
	{
		UE_LOG(LogMetaHumanCaptureSource, Error, TEXT("Failed to read the audio sample duration: %d"), Result);
		return nullptr;
	}
	Sample.Duration = FTimespan(Duration);

	Result = MFSample->ConvertToContiguousBuffer(&Buffer);
	if (FAILED(Result))
	{
		UE_LOG(LogMetaHumanCaptureSource, Error, TEXT("Failed to retrieve the audio sample information: %d"), Result);
		return nullptr;
	}

	uint8* AudioData;
	unsigned long AudioDataSize = 0;

	Result = Buffer->Lock(&AudioData, nullptr, &AudioDataSize);
	if (FAILED(Result))
	{
		UE_LOG(LogMetaHumanCaptureSource, Error, TEXT("Failed to retrieve the audio data: %d"), Result);
		return nullptr;
	}

	Sample.Frames = AudioDataSize / (Sample.Channels * sizeof(int16));
	Sample.Buffer.SetNum(AudioDataSize);
	FMemory::Memcpy(Sample.Buffer.GetData(), AudioData, AudioDataSize);

	Buffer->Unlock();

	return &Sample;
}

EMediaAudioSampleFormat FMetaHumanMediaAudioSourceReader::GetFormat() const
{
	return Sample.Format;
}

uint32 FMetaHumanMediaAudioSourceReader::GetChannels() const
{
	return Sample.Channels;
}

uint32 FMetaHumanMediaAudioSourceReader::GetSampleRate() const
{
	return Sample.SampleRate;
}

void FMetaHumanMediaAudioSourceReader::Close()
{
	SourceReader.Reset();
}

TSharedPtr<IMetaHumanMediaAudioSourceReader, ESPMode::ThreadSafe> IMetaHumanMediaAudioSourceReader::Create()
{
	return MakeShared<FMetaHumanMediaAudioSourceReader, ESPMode::ThreadSafe>();
}

struct FMetaHumanMediaTextureSample : public IMediaTextureSample
{
	//  IMediaTextureSample methods for the sample returned by Next()

	virtual const void* GetBuffer() override
	{
		return Buffer.GetData();
	}

	virtual FIntPoint GetDim() const override
	{
		return Dim;
	}

	virtual FTimespan GetDuration() const override
	{
		return Duration;
	}

	virtual EMediaTextureSampleFormat GetFormat() const override
	{
		return EMediaTextureSampleFormat::Undefined; // Undefined implies MJPEG for now
	}

	virtual FIntPoint GetOutputDim() const override
	{
		return Dim;
	}

	virtual uint32 GetStride() const override
	{
		return Buffer.Num();
	}

	virtual FMediaTimeStamp GetTime() const override
	{
		return Time;
	}

	virtual bool IsCacheable() const override
	{
		return false;
	}

	virtual EMediaOrientation GetOrientation() const override
	{
		return Orientation;
	}

#if WITH_ENGINE
	virtual FRHITexture* GetTexture() const override
	{
		return nullptr;
	}
#endif // WITH_ENGINE

	virtual bool IsOutputSrgb() const override
	{
		return true;
	}

	TArray<uint8> Buffer;
	FIntPoint Dim;
	FTimespan Duration;
	EMediaTextureSampleFormat Format;
	FIntPoint OutputDim;
	FMediaTimeStamp Time;
	EMediaOrientation Orientation;
};

class FMetaHumanMediaVideoSourceReader : public IMetaHumanMediaVideoSourceReader
{
public:
	FMetaHumanMediaVideoSourceReader();
	virtual ~FMetaHumanMediaVideoSourceReader() override;

	virtual bool Open(const FString& URL) override;
	virtual FTimespan GetTotalDuration() const override;
	virtual IMediaTextureSample* Next() override;
	virtual FIntPoint GetDim() const override;
	virtual EMediaTextureSampleFormat GetFormat() const override;
	virtual void SetDefaultOrientation(EMediaOrientation InOrientation) override;
	virtual void Close() override;

protected:
	FTimespan TotalDuration;
	FMetaHumanMediaTextureSample Sample;

	TComPtr<IMFSourceReader> SourceReader;
};

FMetaHumanMediaVideoSourceReader::FMetaHumanMediaVideoSourceReader() : Sample()
{
}

FMetaHumanMediaVideoSourceReader::~FMetaHumanMediaVideoSourceReader()
{
}

bool FMetaHumanMediaVideoSourceReader::Open(const FString& URL)
{
	TComPtr<IMFMediaType> VideoMediaType;
	TComPtr<IMFAttributes> Attributes;

	HRESULT Result = ::MFCreateAttributes(&Attributes, 0);
	if (FAILED(Result))
	{
		UE_LOG(LogMetaHumanCaptureSource, Error, TEXT("Reader configuration failed: %d"), Result);
		return false;
	}

	Result = ::MFCreateSourceReaderFromURL(*URL, Attributes, &SourceReader);
	if (FAILED(Result))
	{
		UE_LOG(LogMetaHumanCaptureSource, Error, TEXT("Cannot open the video file %s: %d"), *URL, Result);
		return false;
	}

	PROPVARIANT DurationVar;
	int64 DurationTicks;	// 10,000,000 == 1.0e7 units per second. Each unit corresponds to 100 nanoseconds.

	Result = SourceReader->GetPresentationAttribute(MF_SOURCE_READER_MEDIASOURCE, MF_PD_DURATION, &DurationVar);
	if (FAILED(Result))
	{
		UE_LOG(LogMetaHumanCaptureSource, Error, TEXT("Failed to retrieve the video file duration: %d"), Result);
		return false;
	}

	Result = ::PropVariantToInt64(DurationVar, &DurationTicks);
	PropVariantClear(&DurationVar);

	if (FAILED(Result))
	{
		UE_LOG(LogMetaHumanCaptureSource, Error, TEXT("Failed to retrieve the video file duration value: %d"), Result);
		return false;
	}

	TotalDuration = FTimespan(DurationTicks);

	Result = SourceReader->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS, false);
	if (FAILED(Result))
	{
		UE_LOG(LogMetaHumanCaptureSource, Error, TEXT("Stream Selection Failed: %d"), Result);
		return false;
	}

	Result = SourceReader->SetStreamSelection(MF_SOURCE_READER_FIRST_VIDEO_STREAM, true);
	if (FAILED(Result))
	{
		UE_LOG(LogMetaHumanCaptureSource, Error, TEXT("Stream Selection Failed: %d"), Result);
		return false;
	}

	Result = SourceReader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, &VideoMediaType);
	if (FAILED(Result))
	{
		UE_LOG(LogMetaHumanCaptureSource, Error, TEXT("Media Type Configuration Failed: %d"), Result);
		return false;
	}

	UINT32 VideoWidth = 0;
	UINT32 VideoHeight = 0;
	GUID VideoSubType;

	Result = ::MFGetAttributeSize(VideoMediaType, MF_MT_FRAME_SIZE, &VideoWidth, &VideoHeight);
	if (FAILED(Result))
	{
		UE_LOG(LogMetaHumanCaptureSource, Error, TEXT("Media Type Configuration Failed: %d"), Result);
		return false;
	}

	Result = VideoMediaType->GetGUID(MF_MT_SUBTYPE, &VideoSubType);
	if (FAILED(Result))
	{
		UE_LOG(LogMetaHumanCaptureSource, Error, TEXT("Media Type Configuration Failed: %d"), Result);
		return false;
	}

	if (VideoSubType != MFVideoFormat_MJPG)
	{
		UE_LOG(LogMetaHumanCaptureSource, Error, TEXT("Only MJPG video format is currently supported"));
		return false;
	}

	Result = SourceReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, VideoMediaType);
	if (FAILED(Result))
	{
		UE_LOG(LogMetaHumanCaptureSource, Error, TEXT("Media Type Configuration Failed: %d"), Result);
		return false;
	}

	Sample.Dim.X = VideoWidth;
	Sample.Dim.Y = VideoHeight;

	return true;
}

FTimespan FMetaHumanMediaVideoSourceReader::GetTotalDuration() const
{
	return TotalDuration;
}

IMediaTextureSample* FMetaHumanMediaVideoSourceReader::Next()
{
	TComPtr<IMFSample> MFSample;
	TComPtr<IMFMediaBuffer> Buffer;
	DWORD Flags;
	LONGLONG TimeStamp;
	LONGLONG Duration;
	DWORD BufferSize;
	uint8* BitmapData;

	HRESULT Result = SourceReader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, nullptr, &Flags, &TimeStamp, &MFSample);
	if (FAILED(Result))
	{
		UE_LOG(LogMetaHumanCaptureSource, Error, TEXT("Failed to read the next sample: %d"), Result);
		return nullptr;
	}

	if (Flags & (MF_SOURCE_READERF_ERROR | MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED))
	{
		UE_LOG(LogMetaHumanCaptureSource, Error, TEXT("Failed to read the next sample: Flags(%d)"), Flags);
		return nullptr;
	}

	if (Flags & MF_SOURCE_READERF_ENDOFSTREAM)
	{
		//	Finished reading the stream
		return nullptr;
	}

	if (!MFSample)
	{
		UE_LOG(LogMetaHumanCaptureSource, Error, TEXT("Failed to read the next sample"));
		return nullptr;
	}

	Result = MFSample->GetSampleTime(&TimeStamp);
	if (FAILED(Result))
	{
		UE_LOG(LogMetaHumanCaptureSource, Error, TEXT("Failed to read the video sample time: %d"), Result);
		return nullptr;
	}
	Sample.Time = FMediaTimeStamp(FTimespan(TimeStamp));

	Result = MFSample->GetSampleDuration(&Duration);
	if (FAILED(Result))
	{
		UE_LOG(LogMetaHumanCaptureSource, Error, TEXT("Failed to read the video sample duration: %d"), Result);
		return nullptr;
	}
	Sample.Duration = FTimespan(Duration);

	Result = MFSample->ConvertToContiguousBuffer(&Buffer);
	if (FAILED(Result))
	{
		UE_LOG(LogMetaHumanCaptureSource, Error, TEXT("Failed to retrieve the video sample buffer: %d"), Result);
		return nullptr;
	}

	Result = Buffer->Lock(&BitmapData, nullptr, &BufferSize);

	if (FAILED(Result))
	{
		Buffer->Unlock();

		UE_LOG(LogMetaHumanCaptureSource, Error, TEXT("Failed to retrieve the video data: %d"), Result);
		return nullptr;
	}

	Sample.Buffer.SetNum(BufferSize);
	FMemory::Memcpy(Sample.Buffer.GetData(), BitmapData, BufferSize);

	Buffer->Unlock();

	return &Sample;
}

FIntPoint FMetaHumanMediaVideoSourceReader::GetDim() const
{
	return Sample.Dim;
}

EMediaTextureSampleFormat FMetaHumanMediaVideoSourceReader::GetFormat() const
{
	return Sample.Format;
}

void FMetaHumanMediaVideoSourceReader::SetDefaultOrientation(EMediaOrientation InOrientation)
{
	Sample.Orientation = InOrientation;
}

void FMetaHumanMediaVideoSourceReader::Close()
{
	SourceReader.Reset();
}

TSharedPtr<IMetaHumanMediaVideoSourceReader, ESPMode::ThreadSafe> IMetaHumanMediaVideoSourceReader::Create()
{
	return MakeShared<FMetaHumanMediaVideoSourceReader, ESPMode::ThreadSafe>();
}

#endif   // PLATFORM_WINDOWS && !UE_SERVER