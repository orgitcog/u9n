// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanPipelineMediaPlayerWMFReaderNode.h"

#include "Windows/AllowWindowsPlatformTypes.h"
THIRD_PARTY_INCLUDES_START
  #include <mfapi.h>
  #include <mfidl.h>
  #include <mfreadwrite.h>
THIRD_PARTY_INCLUDES_END
#include "Windows/HideWindowsPlatformTypes.h"

#pragma comment(lib, "mfplat")
#pragma comment(lib, "mf")
#pragma comment(lib, "mfreadwrite")
#pragma comment(lib, "mfuuid")
#pragma comment(lib, "gdiplus")

namespace UE::MetaHuman::Pipeline
{

class FMediaPlayerWMFReaderNodeImpl
{
public:

	// Keep IMFSourceReader out of public header. Cant even use something like:
	// class IMFSourceReader *Reader = nullptr;
	// in public header as there seems to be ambiguity over whether its 
	// a class or struct.
	IMFSourceReader* Reader = nullptr;
};

FMediaPlayerWMFReaderNode::FMediaPlayerWMFReaderNode(const FString& InName) : FMediaPlayerNode("MediaPlayerWMFReader", InName)
{
	Impl = MakeShared<FMediaPlayerWMFReaderNodeImpl>();
}

bool FMediaPlayerWMFReaderNode::Start(const TSharedPtr<FPipelineData>& InPipelineData)
{
	if (!Impl->Reader)
	{
		InPipelineData->SetErrorNodeCode(ErrorCode::NoVideoPlayer);
		InPipelineData->SetErrorNodeMessage(TEXT("Failed to setup video player"));

		return false;
	}

	return true;
}

bool FMediaPlayerWMFReaderNode::Process(const TSharedPtr<FPipelineData>& InPipelineData)
{
	double Start = FPlatformTime::Seconds();
	bool bOk = false;

	IMFSample* VideoSample = nullptr;
	IMFMediaBuffer* Buffer = nullptr;
	uint8* Data = nullptr;
	DWORD Length;
	int32 Stride = 0;

	HRESULT Result;

	FUEImageDataType Image;
	FAudioDataType Audio;
	FQualifiedFrameTime ImageSampleTime;
	FQualifiedFrameTime AudioSampleTime;
	FMetaHumanLocalLiveLinkSubject::ETimeSource ImageSampleTimeSource = FMetaHumanLocalLiveLinkSubject::ETimeSource::NotSet;
	FMetaHumanLocalLiveLinkSubject::ETimeSource AudioSampleTimeSource = FMetaHumanLocalLiveLinkSubject::ETimeSource::NotSet;

	bool bDroppedFrame = false; // Not supported for WMFReader player

	while (true)
	{
		if (*bAbort)
		{
			goto done;
		}

		if (bIsFirstFrame && FPlatformTime::Seconds() > Start + SampleTimeout) // Only timeout on first frame - sample may not be delivered if game thread is blocked
		{
			InPipelineData->SetErrorNodeCode(ErrorCode::VideoTimeout);
			InPipelineData->SetErrorNodeMessage(TEXT("Timeout sampling video"));
			goto done;
		}

		DWORD StreamIndex, Flags;
		LONGLONG TimeStamp;

		Result = Impl->Reader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &StreamIndex, &Flags, &TimeStamp, &VideoSample);
		if (FAILED(Result))
		{
			InPipelineData->SetErrorNodeCode(ErrorCode::FailedToGetVideoSample);
			InPipelineData->SetErrorNodeMessage(TEXT("Failed to get video sample"));
			goto done;
		}

		if (VideoSample)
		{
			FMetaHumanLocalLiveLinkSubject::GetSampleTime(FrameRate, ImageSampleTime, ImageSampleTimeSource);

			break;
		}

		FPlatformProcess::Sleep(SampleWaitTime);
	}

	bIsFirstFrame = false;

	Result = VideoSample->ConvertToContiguousBuffer(&Buffer);
	if (FAILED(Result))
	{
		InPipelineData->SetErrorNodeCode(ErrorCode::FailedToGetVideoSampleBuffer);
		InPipelineData->SetErrorNodeMessage(TEXT("Failed to get video sample buffer"));
		goto done;
	}

	Result = Buffer->GetCurrentLength(&Length);
	if (FAILED(Result))
	{
		InPipelineData->SetErrorNodeCode(ErrorCode::FailedToGetVideoSampleLength);
		InPipelineData->SetErrorNodeMessage(TEXT("Failed to get video sample length"));
		goto done;
	}

	Result = Buffer->Lock(&Data, nullptr, &Length);
	if (FAILED(Result))
	{
		InPipelineData->SetErrorNodeCode(ErrorCode::FailedToGetVideoSampleData);
		InPipelineData->SetErrorNodeMessage(TEXT("Failed to get video sample buffer data"));
		goto done;
	}

	// Memory buffer returned by Lock above has no padding
	if (Format == EMediaTextureSampleFormat::CharNV12)
	{
		Stride = Width;
	}
	else if (Format == EMediaTextureSampleFormat::CharYUY2 || Format == EMediaTextureSampleFormat::CharUYVY)
	{
		Stride = Width * 2;
	}
	else if (Format == EMediaTextureSampleFormat::CharBGRA)
	{
		Stride = Width * 4;
	}
	else
	{
		check(false);
	}

	ConvertSample(FIntPoint(Width, Height), Stride, Format, Data, Image);

	InPipelineData->SetData<FUEImageDataType>(Pins[0], MoveTemp(Image));
	InPipelineData->SetData<FAudioDataType>(Pins[1], MoveTemp(Audio));
	InPipelineData->SetData<FQualifiedFrameTime>(Pins[2], ImageSampleTime);
	InPipelineData->SetData<FQualifiedFrameTime>(Pins[3], AudioSampleTime);
	InPipelineData->SetData<bool>(Pins[4], bDroppedFrame);
	InPipelineData->SetData<int32>(Pins[5], static_cast<uint8>(ImageSampleTimeSource));
	InPipelineData->SetData<int32>(Pins[6], static_cast<uint8>(AudioSampleTimeSource));

	bOk = true;

done:

	SafeRelease(&Buffer);
	SafeRelease(&VideoSample);

	return bOk;
}

bool FMediaPlayerWMFReaderNode::Play(const FString& InVideoURL, int32 InVideoTrack, int32 InVideoTrackFormat,
									 const FString& InAudioURL, int32 InAudioTrack, int32 InAudioTrackFormat)
{	
	IMFAttributes* Config = nullptr;
	IMFActivate** Devices = nullptr;
	uint32 NumDevices = 0;
	IMFMediaSource* Source = nullptr;
	IMFPresentationDescriptor* PresentationDescriptor = nullptr;
	IMFStreamDescriptor* StreamDescriptor = nullptr;
	IMFMediaTypeHandler* MediaTypeHandler = nullptr;
	IMFMediaType* MediaType = nullptr;

	HRESULT Result;
	
	Impl->Reader = nullptr;
	Width = 0;
	Height = 0;
	Format = EMediaTextureSampleFormat::Undefined;

	Result = MFStartup(MF_VERSION);
	if (FAILED(Result))
	{
		goto done;
	}

	Result = MFCreateAttributes(&Config, 1);
	if (FAILED(Result))
	{
		goto done;
	}

	//	const GUID MF_LOW_LATENCY = { 0x9c27891a, 0xed7a, 0x40e1,{ 0x88, 0xe8, 0xb2, 0x27, 0x27, 0xa0, 0x24, 0xee } };
	//	pConfig->SetUINT32(MF_LOW_LATENCY, TRUE);

	Result = Config->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
	if (FAILED(Result))
	{
		goto done;
	}

	Result = MFEnumDeviceSources(Config, &Devices, &NumDevices);
	if (FAILED(Result))
	{
		goto done;
	}

	for (uint32 Index = 0; Index < NumDevices; ++Index)
	{
		WCHAR* DeviceLink = nullptr;
		uint32 DeviceLinkSize;

		Result = Devices[Index]->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, &DeviceLink, &DeviceLinkSize);
		if (FAILED(Result))
		{
			goto done;
		}

		if (InVideoURL == FString("vidcap://") + DeviceLink)
		{
			Result = Devices[Index]->ActivateObject(IID_PPV_ARGS(&Source));
			if (FAILED(Result))
			{
				goto done;
			}

			Result = Source->CreatePresentationDescriptor(&PresentationDescriptor);
			if (FAILED(Result))
			{
				goto done;
			}

			DWORD StreamCount;
			Result = PresentationDescriptor->GetStreamDescriptorCount(&StreamCount);
			if (FAILED(Result))
			{
				goto done;
			}

			// UE lists streams (tracks) in reverse order to WMF
			BOOL bSelected;
			Result = PresentationDescriptor->GetStreamDescriptorByIndex(StreamCount - 1 - InVideoTrack, &bSelected, &StreamDescriptor);
			if (FAILED(Result))
			{
				goto done;
			}

			Result = StreamDescriptor->GetMediaTypeHandler(&MediaTypeHandler);
			if (FAILED(Result))
			{
				goto done;
			}

			Result = MediaTypeHandler->GetMediaTypeByIndex(InVideoTrackFormat, &MediaType);
			if (FAILED(Result))
			{
				goto done;
			}

			Result = MFGetAttributeSize(MediaType, MF_MT_FRAME_SIZE, &Width, &Height);
			if (FAILED(Result))
			{
				goto done;
			}

			GUID Subtype = GUID_NULL;
			Result = MediaType->GetGUID(MF_MT_SUBTYPE, &Subtype);
			if (FAILED(Result))
			{
				goto done;
			}

			if (Subtype == MFVideoFormat_NV12)
			{
				Format = EMediaTextureSampleFormat::CharNV12;
			}
			else if (Subtype == MFVideoFormat_YUY2)
			{
				Format = EMediaTextureSampleFormat::CharYUY2;
			}
			else if (Subtype == MFVideoFormat_UYVY)
			{
				Format = EMediaTextureSampleFormat::CharUYVY;
			}
			else
			{
				goto done;
			}

			uint32 Numerator = 0;
			uint32 Denominator = 0;
			Result = MFGetAttributeRatio(MediaType, MF_MT_FRAME_RATE, &Numerator, &Denominator);
			if (FAILED(Result))
			{
				goto done;
			}

			FrameRate = FFrameRate(Numerator, Denominator);

			Result = MFCreateSourceReaderFromMediaSource(Source, Config, &Impl->Reader);
			if (FAILED(Result))
			{
				goto done;
			}

			Result = Impl->Reader->SetCurrentMediaType(StreamCount - 1 - InVideoTrack, nullptr, MediaType);
			if (FAILED(Result))
			{
				SafeRelease(&Impl->Reader);
				goto done;
			}

			break;
		}
	}

done:

	SafeRelease(&Config);

	for (uint32 Index = 0; Index < NumDevices; ++Index)
	{
		Devices[Index]->Release();
	}

	CoTaskMemFree(Devices);

	SafeRelease(&Source);
	SafeRelease(&PresentationDescriptor);
	SafeRelease(&StreamDescriptor);
	SafeRelease(&MediaTypeHandler);
	SafeRelease(&MediaType);

	return Impl->Reader != nullptr;
}

bool FMediaPlayerWMFReaderNode::Close()
{
	SafeRelease(&Impl->Reader);

	return true;
}

}