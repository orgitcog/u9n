// Copyright Epic Games, Inc. All Rights Reserved.

#include "RivermaxMediaCapture.h"

#include "Async/Async.h"
#include "Async/Fundamental/Task.h"
#include "IRivermaxCoreModule.h"
#include "IRivermaxManager.h"
#include "IRivermaxOutputStream.h"
#include "RenderGraphUtils.h"
#include "RivermaxMediaLog.h"
#include "RivermaxMediaOutput.h"
#include "RivermaxMediaUtils.h"
#include "RivermaxShaders.h"
#include "RivermaxTracingUtils.h"
#include "Slate/SceneViewport.h"
#include "Widgets/SViewport.h"

#if WITH_EDITOR
#include "AnalyticsEventAttribute.h"
#include "EngineAnalytics.h"
#endif


DECLARE_GPU_STAT(Rmax_Capture);
DECLARE_GPU_STAT(Rmax_FrameReservation);


/* namespace RivermaxMediaCaptureDevice
*****************************************************************************/

#if WITH_EDITOR
namespace RivermaxMediaCaptureAnalytics
{
	/**
	 * @EventName MediaFramework.RivermaxCaptureStarted
	 * @Trigger Triggered when a Rivermax capture of the viewport or render target is started.
	 * @Type Client
	 * @Owner MediaIO Team
	 */
	void SendCaptureEvent(const FIntPoint& Resolution, const FFrameRate FrameRate, const FString& CaptureType)
	{
		if (FEngineAnalytics::IsAvailable())
		{
			TArray<FAnalyticsEventAttribute> EventAttributes;
			EventAttributes.Add(FAnalyticsEventAttribute(TEXT("CaptureType"), CaptureType));
			EventAttributes.Add(FAnalyticsEventAttribute(TEXT("ResolutionWidth"), FString::Printf(TEXT("%d"), Resolution.X)));
			EventAttributes.Add(FAnalyticsEventAttribute(TEXT("ResolutionHeight"), FString::Printf(TEXT("%d"), Resolution.Y)));
			EventAttributes.Add(FAnalyticsEventAttribute(TEXT("FrameRate"), FrameRate.ToPrettyText().ToString()));
			FEngineAnalytics::GetProvider().RecordEvent(TEXT("MediaFramework.RivermaxCaptureStarted"), EventAttributes);
		}
	}
}
#endif

namespace UE::RivermaxMediaCaptureUtil
{
	void GetOutputEncodingInfo(ERivermaxMediaOutputPixelFormat InPixelFormat, const FIntPoint& InSize, uint32& OutBytesPerElement, uint32& OutElementsPerFrame)
	{
		using namespace UE::RivermaxCore;

		const ESamplingType SamplingType = UE::RivermaxMediaUtils::Private::MediaOutputPixelFormatToRivermaxSamplingType(InPixelFormat);
		const FVideoFormatInfo Info = FStandardVideoFormat::GetVideoFormatInfo(SamplingType);

		// Compute horizontal byte count (stride) of aligned resolution
		const uint32 PixelAlignment = Info.PixelGroupCoverage;
		const FIntPoint AlignedResolution = UE::RivermaxMediaUtils::Private::GetAlignedResolution(Info, InSize);
		const uint32 PixelCount = AlignedResolution.X * AlignedResolution.Y;
		const uint32 PixelGroupCount = PixelCount / PixelAlignment;
		const uint32 FrameByteCount = PixelGroupCount * Info.PixelGroupSize;

		switch (InPixelFormat)
		{
		case ERivermaxMediaOutputPixelFormat::PF_8BIT_YUV422:
		{
			OutBytesPerElement = sizeof(UE::RivermaxShaders::FRGBToYUV8Bit422CS::FYUV8Bit422Buffer);
			break;
		}
		case ERivermaxMediaOutputPixelFormat::PF_10BIT_YUV422:
		{
			OutBytesPerElement = sizeof(UE::RivermaxShaders::FRGBToYUV10Bit422LittleEndianCS::FYUV10Bit422LEBuffer);
			break;
		}
		case ERivermaxMediaOutputPixelFormat::PF_8BIT_RGB:
		{
			OutBytesPerElement = sizeof(UE::RivermaxShaders::FRGBToRGB8BitCS::FRGB8BitBuffer);
			break;
		}
		case ERivermaxMediaOutputPixelFormat::PF_10BIT_RGB:
		{
			OutBytesPerElement = sizeof(UE::RivermaxShaders::FRGBToRGB10BitCS::FRGB10BitBuffer);
			break;
		}
		case ERivermaxMediaOutputPixelFormat::PF_12BIT_RGB:
		{
			OutBytesPerElement = sizeof(UE::RivermaxShaders::FRGBToRGB12BitCS::FRGB12BitBuffer);
			break;
		}
		case ERivermaxMediaOutputPixelFormat::PF_FLOAT16_RGB:
		{
			OutBytesPerElement = sizeof(UE::RivermaxShaders::FRGBToRGB16fCS::FRGB16fBuffer);
			break;
		}
		}

		// Shader encoding might not align with pixel group size so we need to have enough elements to represent the last pixel group
		OutElementsPerFrame = FMath::CeilToInt32((float)FrameByteCount / OutBytesPerElement);
	}
}


///* URivermaxMediaCapture implementation
//*****************************************************************************/

UE::RivermaxCore::FRivermaxOutputOptions URivermaxMediaCapture::GetOutputOptions() const
{
	return Options;
}

void URivermaxMediaCapture::GetLastPresentedFrameInformation(UE::RivermaxCore::FPresentedFrameInfo& OutFrameInfo) const
{
	if (RivermaxVideoStream.IsValid())
	{
		RivermaxVideoStream->GetLastPresentedFrame(OutFrameInfo);
	}
}

void URivermaxMediaCapture::WaitForGPU(FRHITexture* InRHITexture)
{
	if (bIsActive && ShouldCaptureRHIResource())
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(URivermaxMediaCapture::GPUDirect_WaitForGPU);
		while (!ShaderCompletedRenderingFence->Poll())
		{
			FPlatformProcess::YieldThread();
		}
		ShaderCompletedRenderingFence->Clear();
		GPUWaitCompleteEvent->Trigger();
	}
 
 
}

bool URivermaxMediaCapture::ValidateMediaOutput() const
{
	URivermaxMediaOutput* RivermaxMediaOutput = Cast<URivermaxMediaOutput>(MediaOutput);
	if (!RivermaxMediaOutput)
	{
		UE_LOG(LogRivermaxMedia, Error, TEXT("Can not start the capture. MediaOutput's class is not supported."));
		return false;
	}

	return true;
}

bool URivermaxMediaCapture::InitializeCapture()
{
	URivermaxMediaOutput* RivermaxOutput = CastChecked<URivermaxMediaOutput>(MediaOutput);
	UE::RivermaxCore::FRivermaxOutputOptions MediaOutputOptions = RivermaxOutput->GenerateStreamOptions();
	bool bResult = Initialize(MediaOutputOptions);
#if WITH_EDITOR
	if (bResult)
	{
		RivermaxMediaCaptureAnalytics::SendCaptureEvent(GetDesiredSize(), RivermaxOutput->FrameRate, GetCaptureSourceType());
	}
#endif
	return bResult;
}

bool URivermaxMediaCapture::UpdateSceneViewportImpl(TSharedPtr<FSceneViewport>& InSceneViewport)
{
	return true;
}

bool URivermaxMediaCapture::UpdateRenderTargetImpl(UTextureRenderTarget2D* InRenderTarget)
{
	return true;
}

void URivermaxMediaCapture::StopCaptureImpl(bool bAllowPendingFrameToBeProcess)
{
	bIsActive = false;

	if (RivermaxVideoStream)
	{
		RivermaxVideoStream->Uninitialize();
		RivermaxVideoStream.Reset();
	}

	if (RivermaxAncStream)
	{
		RivermaxAncStream->Uninitialize();
		RivermaxAncStream.Reset();
	}
}

bool URivermaxMediaCapture::ShouldCaptureRHIResource() const
{
	if (RivermaxVideoStream)
	{
		return RivermaxVideoStream->IsGPUDirectSupported();
	}

	return false;
}

bool URivermaxMediaCapture::HasFinishedProcessing() const
{
	return Super::HasFinishedProcessing();
}

bool URivermaxMediaCapture::Initialize(const UE::RivermaxCore::FRivermaxOutputOptions& InMediaOutputOptions)
{
	using namespace UE::RivermaxCore;
	bIsActive = false;
	Options = InMediaOutputOptions;
	IRivermaxCoreModule* Module = FModuleManager::GetModulePtr<IRivermaxCoreModule>("RivermaxCore");
	if (Module)
	{
		TArray<char> SDP;
		if (ConfigureCapture() && UE::RivermaxMediaUtils::Private::StreamOptionsToSDPDescription(Options, SDP))
		{
			using namespace UE::RivermaxCore;

			RivermaxVideoStream = Module->CreateOutputStream(ERivermaxStreamType::VIDEO_2110_20_STREAM, SDP);
			if (RivermaxVideoStream)
			{
				bIsActive = RivermaxVideoStream->Initialize(Options, *this);
			}

			if (Options.GetStreamOptions(ERivermaxStreamType::ANC_2110_40_STREAM).IsValid())
			{
				RivermaxAncStream = Module->CreateOutputStream(ERivermaxStreamType::ANC_2110_40_STREAM, SDP);
				if (RivermaxAncStream)
				{
					bIsActive = RivermaxAncStream->Initialize(Options, *this);
				}
			}
		}
	}

	ShaderCompletedRenderingFence = RHICreateGPUFence(TEXT("RmaxRenderingCompleteFence"));
	GPUWaitCompleteEvent = MakeShareable(FPlatformProcess::GetSynchEventFromPool(true /*bIsManualReset*/), [](FEvent* EventToDelete)
		{
			FPlatformProcess::ReturnSynchEventToPool(EventToDelete);
		});

	// This event starts with a triggered state because Render thread expects this to be triggered to issue render commands.
	// Since nothing has been completed yet, this needs to be triggered.
	GPUWaitCompleteEvent->Trigger();

	return bIsActive;
}

bool URivermaxMediaCapture::ConfigureCapture() const
{
	using namespace UE::RivermaxCore;

	// Resolve interface address
	IRivermaxCoreModule* Module = FModuleManager::GetModulePtr<IRivermaxCoreModule>("RivermaxCore");
	if (Module == nullptr)
	{
		return false;
	}

	if (!Module->GetRivermaxManager()->ValidateLibraryIsLoaded())
	{
		return false;
	}
	TSharedPtr<FRivermaxVideoOutputOptions> VideoOptions
		= StaticCastSharedPtr<FRivermaxVideoOutputOptions>(Options.StreamOptions[static_cast<uint8>(ERivermaxStreamType::VIDEO_2110_20_STREAM)]);

	// Video configuration
	if (VideoOptions.IsValid())
	{
		const bool bFoundDevice = Module->GetRivermaxManager()->GetMatchingDevice(VideoOptions->InterfaceAddress, VideoOptions->InterfaceAddress);
		if (bFoundDevice == false)
		{
			UE_LOG(LogRivermaxMedia, Error, TEXT("Could not find a matching interface for IP '%s'"), *VideoOptions->InterfaceAddress);
			return false;
		}
		
		/** Override the size. */
		VideoOptions->Resolution = GetDesiredSize();

		if (VideoOptions->Resolution.X <= 0 || VideoOptions->Resolution.Y <= 0)
		{
			UE_LOG(LogRivermaxMedia, Warning, TEXT("Can't start capture. Invalid resolution requested: %dx%d"), VideoOptions->Resolution.X, VideoOptions->Resolution.Y);
			return false;
		}

		const FVideoFormatInfo Info = FStandardVideoFormat::GetVideoFormatInfo(VideoOptions->PixelFormat);
		VideoOptions->AlignedResolution = UE::RivermaxMediaUtils::Private::GetAlignedResolution(Info, VideoOptions->Resolution);
	}

	TSharedPtr<FRivermaxAncOutputOptions> AncOptions
		= StaticCastSharedPtr<FRivermaxAncOutputOptions>(Options.StreamOptions[static_cast<uint8>(ERivermaxStreamType::ANC_2110_40_STREAM)]);
	if (AncOptions.IsValid())
	{
		const bool bFoundDevice = Module->GetRivermaxManager()->GetMatchingDevice(AncOptions->InterfaceAddress, AncOptions->InterfaceAddress);
		if (bFoundDevice == false)
		{
			UE_LOG(LogRivermaxMedia, Error, TEXT("Could not find a matching interface for IP '%s'"), *AncOptions->InterfaceAddress);
			return false;
		}
	}
	return true;
}

void URivermaxMediaCapture::AddFrameReservationPass(FRDGBuilder& GraphBuilder)
{
	RHI_BREADCRUMB_EVENT_STAT(GraphBuilder.RHICmdList, Rmax_FrameReservation, "Rmax_FrameReservation");
	
	// Since we are going to enqueue a lambda that can potentially sleep in the RHI thread if the pixels haven't arrived,
	// we dispatch the existing commands before any potential sleep.
	GraphBuilder.RHICmdList.ImmediateFlush(EImmediateFlushType::DispatchToRHIThread);

	// Scene rendering will already be enqueued but capture conversion pass will not
	// Revisit to push slot reservation till last minute
	URivermaxMediaCapture* Capturer = this;
	GraphBuilder.RHICmdList.EnqueueLambda([Capturer, FrameCounter = GFrameCounterRenderThread](FRHICommandList& RHICmdList)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(RmaxFrameReservation);
		TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*UE::RivermaxCore::FRivermaxTracingUtils::RmaxOutMediaCapturePipeTraceEvents[FrameCounter % 10]);

		if (Capturer->bIsActive)
		{
			Capturer->RivermaxVideoStream->ReserveFrame(FrameCounter);
			if (Capturer->RivermaxAncStream.IsValid())
			{
				Capturer->RivermaxAncStream->ReserveFrame(FrameCounter);
			}
		}
	});
}

void URivermaxMediaCapture::OnFrameCapturedInternal_AnyThread(const FCaptureBaseData& InBaseData, TSharedPtr<FMediaCaptureUserData, ESPMode::ThreadSafe> InUserData, void* InBuffer, int32 Width, int32 Height, int32 BytesPerRow)
{
	using namespace UE::RivermaxCore;
	TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*FRivermaxTracingUtils::RmaxOutMediaCapturePipeTraceEvents[InBaseData.SourceFrameNumberRenderThread % 10]);

	TSharedPtr<FRivermaxOutputVideoFrameInfo> NewFrame = MakeShared<FRivermaxOutputVideoFrameInfo>();
	NewFrame->Height = Height;
	NewFrame->Width = Width;
	NewFrame->Stride = BytesPerRow;
	NewFrame->CPUBuffer = InBuffer;
	NewFrame->FrameIdentifier = InBaseData.SourceFrameNumberRenderThread;
	if (RivermaxVideoStream->PushFrame(NewFrame) == false)
	{
		UE_LOG(LogRivermaxMedia, Verbose, TEXT("Failed to push captured frame"));
	}
	if (RivermaxAncStream.IsValid())
	{
		RivermaxAncStream->PushFrame(NewFrame);
	}
}

void URivermaxMediaCapture::OnRHIResourceCapturedInternal_AnyThread(const FCaptureBaseData& InBaseData, TSharedPtr<FMediaCaptureUserData, ESPMode::ThreadSafe> InUserData, FBufferRHIRef InBuffer)
{
	using namespace UE::RivermaxCore;
	TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*FRivermaxTracingUtils::RmaxOutMediaCapturePipeTraceEvents[InBaseData.SourceFrameNumberRenderThread % 10]);
	TSharedPtr<FRivermaxOutputVideoFrameInfo> NewFrame = MakeShared<FRivermaxOutputVideoFrameInfo>();
	NewFrame->FrameIdentifier = InBaseData.SourceFrameNumberRenderThread;
	NewFrame->GPUBuffer = InBuffer;
	if (RivermaxVideoStream->PushFrame(NewFrame) == false)
	{
		UE_LOG(LogRivermaxMedia, Verbose, TEXT("Failed to pushed captured frame"));
	}
	if (RivermaxAncStream.IsValid())
	{
		RivermaxAncStream->PushFrame(NewFrame);
	}
}

void URivermaxMediaCapture::OnFrameCaptured_RenderingThread(const FCaptureBaseData& InBaseData, TSharedPtr<FMediaCaptureUserData, ESPMode::ThreadSafe> InUserData, void* InBuffer, int32 Width, int32 Height, int32 BytesPerRow)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(URivermaxMediaCapture::OnFrameCaptured_RenderingThread);
	OnFrameCapturedInternal_AnyThread(InBaseData, InUserData, InBuffer, Width, Height, BytesPerRow);
}

void URivermaxMediaCapture::OnRHIResourceCaptured_RenderingThread(FRHICommandListImmediate& /*RHICmdList*/, const FCaptureBaseData& InBaseData, TSharedPtr<FMediaCaptureUserData, ESPMode::ThreadSafe> InUserData, FBufferRHIRef InBuffer)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(URivermaxMediaCapture::OnRHIResourceCaptured_RenderingThread);
	OnRHIResourceCapturedInternal_AnyThread(InBaseData, InUserData, InBuffer);
}

void URivermaxMediaCapture::OnRHIResourceCaptured_AnyThread(const FCaptureBaseData& InBaseData, TSharedPtr<FMediaCaptureUserData, ESPMode::ThreadSafe> InUserData, FBufferRHIRef InBuffer)
{
	using namespace UE::RivermaxCore;

	TRACE_CPUPROFILER_EVENT_SCOPE(URivermaxMediaCapture::OnRHIResourceCaptured_AnyThread);
	OnRHIResourceCapturedInternal_AnyThread(InBaseData, InUserData, InBuffer);
}

void URivermaxMediaCapture::OnFrameCaptured_AnyThread(const FCaptureBaseData& InBaseData, TSharedPtr<FMediaCaptureUserData, ESPMode::ThreadSafe> InUserData, const FMediaCaptureResourceData& InResourceData)
{
	using namespace UE::RivermaxCore;

	TRACE_CPUPROFILER_EVENT_SCOPE(URivermaxMediaCapture::OnFrameCaptured_AnyThread);
	OnFrameCapturedInternal_AnyThread(InBaseData, InUserData, InResourceData.Buffer, InResourceData.Width, InResourceData.Height, InResourceData.BytesPerRow);
}

FIntPoint URivermaxMediaCapture::GetCustomOutputSize(const FIntPoint& InSize) const
{
	URivermaxMediaOutput* RivermaxOutput = CastChecked<URivermaxMediaOutput>(MediaOutput);

	uint32 BytesPerElement = 0;
	uint32 ElementsPerFrame = 0;
	UE::RivermaxMediaCaptureUtil::GetOutputEncodingInfo(RivermaxOutput->PixelFormat, InSize, BytesPerElement, ElementsPerFrame);
	return FIntPoint(ElementsPerFrame, 1);
}

EMediaCaptureResourceType URivermaxMediaCapture::GetCustomOutputResourceType() const
{
	URivermaxMediaOutput* RivermaxOutput = CastChecked<URivermaxMediaOutput>(MediaOutput);

	switch (RivermaxOutput->PixelFormat)
	{
	case ERivermaxMediaOutputPixelFormat::PF_8BIT_YUV422:
	case ERivermaxMediaOutputPixelFormat::PF_10BIT_YUV422:
	case ERivermaxMediaOutputPixelFormat::PF_8BIT_RGB:
	case ERivermaxMediaOutputPixelFormat::PF_10BIT_RGB:
	case ERivermaxMediaOutputPixelFormat::PF_12BIT_RGB:
	case ERivermaxMediaOutputPixelFormat::PF_FLOAT16_RGB:
	{
		return EMediaCaptureResourceType::Buffer; //we use compute shader for all since output format doesn't match texture formats
	}
	default:
		return EMediaCaptureResourceType::Texture;
	}
}

FRDGBufferDesc URivermaxMediaCapture::GetCustomBufferDescription(const FIntPoint& InDesiredSize) const
{
	URivermaxMediaOutput* RivermaxOutput = CastChecked<URivermaxMediaOutput>(MediaOutput);

	uint32 BytesPerElement = 0;
	uint32 ElementsPerFrame = 0;
	UE::RivermaxMediaCaptureUtil::GetOutputEncodingInfo(RivermaxOutput->VideoStream.PixelFormat, InDesiredSize, BytesPerElement, ElementsPerFrame);
	FRDGBufferDesc Desc = FRDGBufferDesc::CreateStructuredDesc(BytesPerElement, ElementsPerFrame);
	
	// Required when GPUDirect using CUDA will be involved
	Desc.Usage |= EBufferUsageFlags::Shared;
	return Desc;
}

void URivermaxMediaCapture::OnCustomCapture_RenderingThread(FRDGBuilder& GraphBuilder, const FCaptureBaseData& InBaseData, TSharedPtr<FMediaCaptureUserData, ESPMode::ThreadSafe> InUserData, FRDGTextureRef InSourceTexture, FRDGBufferRef OutputBuffer, const FRHICopyTextureInfo& CopyInfo, FVector2D CropU, FVector2D CropV)
{
	using namespace UE::RivermaxCore;
	RDG_EVENT_SCOPE_STAT(GraphBuilder, Rmax_Capture, "Rmax_Capture");
	RDG_GPU_STAT_SCOPE(GraphBuilder, Rmax_Capture);

	TRACE_CPUPROFILER_EVENT_SCOPE(URivermaxMediaCapture::OnCustomCapture_RenderingThread);
	TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*FRivermaxTracingUtils::RmaxOutMediaCapturePipeTraceEvents[GFrameCounterRenderThread % 10]);

	using namespace UE::RivermaxShaders;
	URivermaxMediaOutput* RivermaxOutput = CastChecked<URivermaxMediaOutput>(MediaOutput);

	FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);

	// Rectangle area to use from source. This is used when source render target is bigger than output resolution
	const FIntRect ViewRect(CopyInfo.GetSourceRect());
	constexpr bool bDoLinearToSRGB = false;
	const FIntPoint AlignedBufferSize = Options.GetStreamOptions<FRivermaxVideoOutputOptions>(ERivermaxStreamType::VIDEO_2110_20_STREAM)->AlignedResolution;
	const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(DesiredOutputSize.X, 64);

	switch (RivermaxOutput->VideoStream.PixelFormat)
	{
	case ERivermaxMediaOutputPixelFormat::PF_8BIT_YUV422:
	{
		TShaderMapRef<FRGBToYUV8Bit422CS> ComputeShader(GlobalShaderMap);
		FRGBToYUV8Bit422CS::FParameters* Parameters = ComputeShader->AllocateAndSetParameters(GraphBuilder, InSourceTexture, AlignedBufferSize, ViewRect, DesiredOutputSize, MediaShaders::RgbToYuvRec709Scaled, MediaShaders::YUVOffset8bits, bDoLinearToSRGB, OutputBuffer);

		FComputeShaderUtils::AddPass(
			GraphBuilder
			, RDG_EVENT_NAME("RGBAToYUV8Bit422")
			, ComputeShader
			, Parameters
			, GroupCount);

		break;
	}
	case ERivermaxMediaOutputPixelFormat::PF_10BIT_YUV422:
	{
		TShaderMapRef<FRGBToYUV10Bit422LittleEndianCS> ComputeShader(GlobalShaderMap);
		FRGBToYUV10Bit422LittleEndianCS::FParameters* Parameters = ComputeShader->AllocateAndSetParameters(GraphBuilder, InSourceTexture, AlignedBufferSize, ViewRect, DesiredOutputSize, MediaShaders::RgbToYuvRec709Scaled, MediaShaders::YUVOffset10bits, bDoLinearToSRGB, OutputBuffer);

		FComputeShaderUtils::AddPass(
			GraphBuilder
			, RDG_EVENT_NAME("RGBAToYUV10Bit422LE")
			, ComputeShader
			, Parameters
			, GroupCount);
	
		break;
	}
	case ERivermaxMediaOutputPixelFormat::PF_8BIT_RGB:
	{
		TShaderMapRef<FRGBToRGB8BitCS> ComputeShader(GlobalShaderMap);
		FRGBToRGB8BitCS::FParameters* Parameters = ComputeShader->AllocateAndSetParameters(GraphBuilder, InSourceTexture, AlignedBufferSize, ViewRect, DesiredOutputSize, OutputBuffer);

		FComputeShaderUtils::AddPass(
			GraphBuilder
			, RDG_EVENT_NAME("RGBAToRGB8Bit")
			, ComputeShader
			, Parameters
			, GroupCount);
		
		break;
	}
	case ERivermaxMediaOutputPixelFormat::PF_10BIT_RGB:
	{
		TShaderMapRef<FRGBToRGB10BitCS> ComputeShader(GlobalShaderMap);
		FRGBToRGB10BitCS::FParameters* Parameters = ComputeShader->AllocateAndSetParameters(GraphBuilder, InSourceTexture, AlignedBufferSize, ViewRect, DesiredOutputSize, OutputBuffer);

		FComputeShaderUtils::AddPass(
			GraphBuilder
			, RDG_EVENT_NAME("RGBAToRGB10Bit")
			, ComputeShader
			, Parameters
			, GroupCount);

		break;
	}
	case ERivermaxMediaOutputPixelFormat::PF_12BIT_RGB:
	{
		TShaderMapRef<FRGBToRGB12BitCS> ComputeShader(GlobalShaderMap);
		FRGBToRGB12BitCS::FParameters* Parameters = ComputeShader->AllocateAndSetParameters(GraphBuilder, InSourceTexture, AlignedBufferSize, ViewRect, DesiredOutputSize, OutputBuffer);

		FComputeShaderUtils::AddPass(
			GraphBuilder
			, RDG_EVENT_NAME("RGBAToRGB12Bit")
			, ComputeShader
			, Parameters
			, GroupCount);

		break;
	}
	case ERivermaxMediaOutputPixelFormat::PF_FLOAT16_RGB:
	{
		TShaderMapRef<FRGBToRGB16fCS> ComputeShader(GlobalShaderMap);
		FRGBToRGB16fCS::FParameters* Parameters = ComputeShader->AllocateAndSetParameters(GraphBuilder, InSourceTexture, AlignedBufferSize, ViewRect, DesiredOutputSize, OutputBuffer);

		FComputeShaderUtils::AddPass(
			GraphBuilder
			, RDG_EVENT_NAME("RGBAToRGB16f")
			, ComputeShader
			, Parameters
			, GroupCount);

		break;
	}
	}

	//It is only in case of GPU Direct that we need to manually wait for the work to be completed.
	if (ShouldCaptureRHIResource())
	{
		GraphBuilder.AddPass(
			RDG_EVENT_NAME("RivermaxWriteGPUFence"),
			ERDGPassFlags::NeverCull,
			[CompleteFence = ShaderCompletedRenderingFence, InGPUWaitCompleteEvent = GPUWaitCompleteEvent](FRHICommandList& RHICmdList)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(RivermaxWriteGPUFence);
				// Render thread could get to this point before Fence is reset. This could cause a deadlock, therefore it 
				// is necessary to wait for Sync thread to complete the copy
				InGPUWaitCompleteEvent->Wait();
				InGPUWaitCompleteEvent->Reset();
				RHICmdList.WriteGPUFence(CompleteFence);
			}
		);
	}

	AddFrameReservationPass(GraphBuilder);
}

bool URivermaxMediaCapture::IsReadyForFinishDestroy()
{
	return Super::IsReadyForFinishDestroy();
}

void URivermaxMediaCapture::OnInitializationCompleted(bool bHasSucceed)
{
	if (GetState() != EMediaCaptureState::Stopped)
	{
		SetState(bHasSucceed ? EMediaCaptureState::Capturing : EMediaCaptureState::Error);
	}
}

void URivermaxMediaCapture::OnStreamError()
{
	UE_LOG(LogRivermaxMedia, Error, TEXT("Outputstream has caught an error. Stopping capture."));
	if (GetState() != EMediaCaptureState::Stopped)
	{
		SetState(EMediaCaptureState::Error);
	}
}

void URivermaxMediaCapture::OnPreFrameEnqueue()
{
	// Will need to add some logic in that callback chain for the case where margin wasn't enough
	// For now, we act blindly that frames presented are all the same but we need a way to detect 
	// if it's not and correct it.
	TRACE_CPUPROFILER_EVENT_SCOPE(RmaxOutputSynchronization);
	OnOutputSynchronization.ExecuteIfBound();
}
