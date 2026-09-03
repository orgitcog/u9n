// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanPipelineMediaPlayerWMFNode.h"
#include "MetaHumanLocalLiveLinkSubject.h"

#include "Windows/AllowWindowsPlatformTypes.h"
THIRD_PARTY_INCLUDES_START
  #include <mfapi.h>
  #include <mfidl.h>
  #include <mfreadwrite.h>
  #include <Shlwapi.h>
  #include <Windows.h>
THIRD_PARTY_INCLUDES_END
#include "Windows/HideWindowsPlatformTypes.h"

#pragma comment(lib, "mfplat")
#pragma comment(lib, "mf")
#pragma comment(lib, "mfreadwrite")
#pragma comment(lib, "mfuuid")
#pragma comment(lib, "gdiplus")
#pragma comment(lib, "Shlwapi")



namespace UE::MetaHuman::Pipeline
{

class FMediaPlayerWMFNodeImpl
{
public:

	// Keep IMFMediaSession out of public header. Cant even use something like:
	// class IMFMediaSession *Session = nullptr;
	// in public header as there seems to be ambiguity over whether its 
	// a class or struct.
	IMFMediaSession* Session = nullptr;
};

TAutoConsoleVariable<float> CVarFixedWebcamRate
{
	TEXT("mh.LiveLink.FixedWebcamRate"),
	0,
	TEXT("Run the webcam used in Live Link realtime animation processing at a fixed frame rate."),
	ECVF_Default
};

class SampleGrabberCallback : public IMFSampleGrabberSinkCallback
{
public:

	virtual ~SampleGrabberCallback() = default;

	// IUnknown methods
	HRESULT QueryInterface(REFIID InRRID, void** InPpv);
	ULONG AddRef();
	ULONG Release();

	// IMFClockStateSink methods
	HRESULT OnClockStart(MFTIME, LONGLONG) { return S_OK; }
	HRESULT OnClockStop(MFTIME) { return S_OK; }
	HRESULT OnClockPause(MFTIME) { return S_OK; }
	HRESULT OnClockRestart(MFTIME) { return S_OK; }
	HRESULT OnClockSetRate(MFTIME, float) { return S_OK; }

	// IMFSampleGrabberSinkCallback methods
	HRESULT OnSetPresentationClock(IMFPresentationClock* InPresentationClock) { PresentationClock = InPresentationClock; return S_OK; }
	HRESULT OnProcessSample(REFGUID, DWORD, LONGLONG InSampleTime, LONGLONG, const BYTE* InSampleBuffer, DWORD InSampleSize);
	HRESULT OnShutdown() { return S_OK; }

	FCriticalSection VideoSampleMutex;
	TArray<FVideoSample> VideoSamples;
	FFrameRate FrameRate;

private:

	long RefCount = 1;

	IMFPresentationClock* PresentationClock = nullptr;
};

HRESULT SampleGrabberCallback::QueryInterface(REFIID InRRID, void** InPpv)
{
	static const QITAB qit[] =
	{
		QITABENT(SampleGrabberCallback, IMFSampleGrabberSinkCallback),
		QITABENT(SampleGrabberCallback, IMFClockStateSink),
		{ 0 }
	};

	return QISearch(this, qit, InRRID, InPpv);
}

ULONG SampleGrabberCallback::AddRef()
{
	return _InterlockedIncrement(&RefCount);
}

ULONG SampleGrabberCallback::Release()
{
	ULONG NewRefCount = _InterlockedDecrement(&RefCount);

	if (NewRefCount == 0)
	{
		delete this;
	}

	return NewRefCount;
}

HRESULT SampleGrabberCallback::OnProcessSample(REFGUID, DWORD, LONGLONG InSampleTime, LONGLONG, const BYTE* InSampleData, DWORD InSampleSize)
{
	if (PresentationClock)
	{
		MFTIME CurrentTime;
		HRESULT Result = PresentationClock->GetTime(&CurrentTime);

		if (SUCCEEDED(Result))
		{
			FVideoSample VideoSample;
			VideoSample.Data.SetNumUninitialized(InSampleSize);
			FMemory::Memcpy(VideoSample.Data.GetData(), InSampleData, InSampleSize);

			FMetaHumanLocalLiveLinkSubject::GetSampleTime(FrameRate, VideoSample.SampleTime, VideoSample.SampleTimeSource);
			VideoSample.SampleTime.Time -= ((CurrentTime - InSampleTime) / 10000000.0) * FrameRate; // TimeStamp is in 100 nanosecond units

			FScopeLock Lock(&VideoSampleMutex);
			VideoSamples.Add(MoveTemp(VideoSample));
		}
	}

	return S_OK;
}



FMediaPlayerWMFNode::FMediaPlayerWMFNode(const FString& InName) : FMediaPlayerNode("MediaPlayerWMF", InName)
{
	Impl = MakeShared<FMediaPlayerWMFNodeImpl>();
}

bool FMediaPlayerWMFNode::Start(const TSharedPtr<FPipelineData>& InPipelineData)
{
	if (!Impl->Session)
	{
		InPipelineData->SetErrorNodeCode(ErrorCode::NoVideoPlayer);
		InPipelineData->SetErrorNodeMessage(TEXT("Failed to setup video player"));

		return false;
	}

	NodeStart = FPlatformTime::Seconds();
	FixedFPS = CVarFixedWebcamRate.GetValueOnAnyThread();

	return true;
}

bool FMediaPlayerWMFNode::Process(const TSharedPtr<FPipelineData>& InPipelineData)
{
	double Start = FPlatformTime::Seconds();

	FUEImageDataType Image;
	FAudioDataType Audio;
	FQualifiedFrameTime ImageSampleTime;
	FQualifiedFrameTime AudioSampleTime;
	FMetaHumanLocalLiveLinkSubject::ETimeSource ImageSampleTimeSource = FMetaHumanLocalLiveLinkSubject::ETimeSource::NotSet;
	FMetaHumanLocalLiveLinkSubject::ETimeSource AudioSampleTimeSource = FMetaHumanLocalLiveLinkSubject::ETimeSource::NotSet;

	bool bDroppedFrame = false;

	int32 Frame = InPipelineData->GetFrameNumber();

	while (true)
	{
		if (*bAbort)
		{
			return false;
		}

		if (bIsFirstFrame && FPlatformTime::Seconds() > Start + SampleTimeout) // Only timeout on first frame - sample may not be delivered if game thread is blocked
		{
			InPipelineData->SetErrorNodeCode(ErrorCode::VideoTimeout);
			InPipelineData->SetErrorNodeMessage(TEXT("Timeout sampling video"));
			return false;
		}

		{
			FScopeLock Lock(&SampleGrabber->VideoSampleMutex);

			int32 NumVideoSamples = SampleGrabber->VideoSamples.Num();
			if (NumVideoSamples > 0)
			{
				VideoSample = MoveTemp(SampleGrabber->VideoSamples[NumVideoSamples - 1]);
				SampleGrabber->VideoSamples.Reset();

				ImageSampleTime = VideoSample.SampleTime;
				ImageSampleTimeSource = VideoSample.SampleTimeSource;

				bDroppedFrame = NumVideoSamples > 1;

				if (FixedFPS == 0)
				{
					break;
				}
			}
		}

		if (FixedFPS > 0 && FPlatformTime::Seconds() > NodeStart + Frame / FixedFPS && !VideoSample.Data.IsEmpty())
		{
			break;
		}

		FPlatformProcess::Sleep(SampleWaitTime);
	}

	bIsFirstFrame = false;

	ConvertSample(FIntPoint(Width, Height), Stride, Format, VideoSample.Data.GetData(), Image);

	if (FixedFPS > 0 && Width > 50 && Height > 50) // Some indication that its a duplicated frame
	{
		uint8* SampleData = VideoSample.Data.GetData();
		for (int32 Y = 0; Y < 50; ++Y, SampleData += Stride)
		{
			FMemory::Memset(SampleData, 0, 50);
		}
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

bool FMediaPlayerWMFNode::Play(const FString& InVideoURL, int32 InVideoTrack, int32 InVideoTrackFormat,
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
	IMFTopology* Topology = nullptr;
	IMFTopologyNode* SourceNode = nullptr;
	IMFActivate* SinkActivate = nullptr;
	IMFTopologyNode* SinkNode = nullptr;
	HRESULT Result;
	
	Impl->Session = nullptr;
	SampleGrabber = nullptr;
	Width = 0;
	Height = 0;
	Stride = 0;
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
				Stride = Width;
			}
			else if (Subtype == MFVideoFormat_YUY2)
			{
				Format = EMediaTextureSampleFormat::CharYUY2;
				Stride = Width * 2;
			}
			else if (Subtype == MFVideoFormat_UYVY)
			{
				Format = EMediaTextureSampleFormat::CharUYVY;
				Stride = Width * 2;
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

			Result = MediaTypeHandler->SetCurrentMediaType(MediaType);
			if (FAILED(Result))
			{
				goto done;
			}

			Result = MFCreateTopology(&Topology);
			if (FAILED(Result))
			{
				goto done;
			}

			Result = MFCreateTopologyNode(MF_TOPOLOGY_SOURCESTREAM_NODE, &SourceNode);
			if (FAILED(Result))
			{
				goto done;
			}

			Result = SourceNode->SetUnknown(MF_TOPONODE_SOURCE, Source);
			if (FAILED(Result))
			{
				goto done;
			}

			Result = SourceNode->SetUnknown(MF_TOPONODE_PRESENTATION_DESCRIPTOR, PresentationDescriptor);
			if (FAILED(Result))
			{
				goto done;
			}

			Result = SourceNode->SetUnknown(MF_TOPONODE_STREAM_DESCRIPTOR, StreamDescriptor);
			if (FAILED(Result))
			{
				goto done;
			}

			Result = Topology->AddNode(SourceNode);
			if (FAILED(Result))
			{
				goto done;
			}

			Result = MFCreateTopologyNode(MF_TOPOLOGY_OUTPUT_NODE, &SinkNode);
			if (FAILED(Result))
			{
				goto done;
			}

			SampleGrabber = new SampleGrabberCallback();
			SampleGrabber->FrameRate = FrameRate;

			Result = MFCreateSampleGrabberSinkActivate(MediaType, SampleGrabber, &SinkActivate);
			if (FAILED(Result))
			{
				goto done;
			}

			Result = SinkNode->SetObject(SinkActivate);
			if (FAILED(Result))
			{
				goto done;
			}

			Result = SinkNode->SetUINT32(MF_TOPONODE_STREAMID, 0);
			if (FAILED(Result))
			{
				goto done;
			}

			Result = SinkNode->SetUINT32(MF_TOPONODE_NOSHUTDOWN_ON_REMOVE, 0);
			if (FAILED(Result))
			{
				goto done;
			}

			Result = Topology->AddNode(SinkNode);
			if (FAILED(Result))
			{
				goto done;
			}

			Result = SourceNode->ConnectOutput(0, SinkNode, 0);
			if (FAILED(Result))
			{
				goto done;
			}

			Result = MFCreateMediaSession(NULL, &Impl->Session);
			if (FAILED(Result))
			{
				goto done;
			}

			Result = Impl->Session->SetTopology(0, Topology);
			if (FAILED(Result))
			{
				SafeRelease(&Impl->Session);
				goto done;
			}

			PROPVARIANT var;
			PropVariantInit(&var);
			Result = Impl->Session->Start(&GUID_NULL, &var);
			if (FAILED(Result))
			{
				SafeRelease(&Impl->Session);
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
	SafeRelease(&Topology);
	SafeRelease(&SourceNode);
	SafeRelease(&SinkActivate);
	SafeRelease(&SinkNode);
	if (!Impl->Session)
	{
		SafeRelease(&SampleGrabber);
	}

	return Impl->Session != nullptr;
}

bool FMediaPlayerWMFNode::Close()
{
	if (Impl->Session)
	{
		double Start = FPlatformTime::Seconds();
		HRESULT Result;
		
		Result = Impl->Session->Close();
		if (FAILED(Result))
		{
			return false;
		}
		
		bool bClosed = false;
		while (!bClosed)
		{
			if (FPlatformTime::Seconds() > Start + StartTimeout) // Reuse start timeout for close timeout too.
			{
				return false;
			}

			IMFMediaEvent* Event = nullptr;
			Result = Impl->Session->GetEvent(0, &Event);
			if (FAILED(Result))
			{
				return false;
			}

			MediaEventType EventType;
			Result = Event->GetType(&EventType);
			if (FAILED(Result))
			{
				return false;
			}

			if (EventType == MESessionClosed)
			{
				bClosed = true;
			}

			SafeRelease(&Event);
		}

		Result = Impl->Session->Shutdown();
		if (FAILED(Result))
		{
			return false;
		}
	}

	SafeRelease(&SampleGrabber);
	SafeRelease(&Impl->Session);

	return true;
}

}