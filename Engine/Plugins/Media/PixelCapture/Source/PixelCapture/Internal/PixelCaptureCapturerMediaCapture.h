// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Deque.h"
#include "MediaCapture.h"
#include "MediaOutput.h"
#include "PixelCaptureBufferFormat.h"
#include "PixelCaptureCapturer.h"

#include "PixelCaptureCapturerMediaCapture.generated.h"

UCLASS(BlueprintType)
class PIXELCAPTURE_API UPixelCaptureMediaOuput : public UMediaOutput
{
	GENERATED_BODY()

public:
	void									 SetRequestedSize(FIntPoint InRequestedSize) { RequestedSize = InRequestedSize; }
	virtual FIntPoint						 GetRequestedSize() const override { return RequestedSize; }
	virtual EPixelFormat					 GetRequestedPixelFormat() const override { return EPixelFormat::PF_B8G8R8A8; }
	virtual EMediaCaptureConversionOperation GetConversionOperation(EMediaCaptureSourceType InSourceType) const override { return EMediaCaptureConversionOperation::CUSTOM; }

private:
	FIntPoint RequestedSize;
};

UCLASS(BlueprintType)
class PIXELCAPTURE_API UPixelCaptureMediaCapture : public UMediaCapture
{
	GENERATED_BODY()

public:
	void AddOutputFrame(TSharedPtr<IPixelCaptureOutputFrame> InOutputFrame)
	{
		OutputFrames.PushLast(InOutputFrame);
	}

	void RemoveLastOutputFrame()
	{
		OutputFrames.PopLast();
	}

	void SetFormat(int32 InFormat)
	{
		Format = InFormat;
	}

	DECLARE_EVENT_OneParam(UPixelCaptureMediaCapture, FOnCaptureComplete, TSharedPtr<IPixelCaptureOutputFrame>);
	FOnCaptureComplete OnCaptureComplete;

protected:
	virtual bool InitializeCapture() override
	{
		SetState(EMediaCaptureState::Capturing);
		return true;
	}

	virtual void OnRHIResourceCaptured_AnyThread(
		const FCaptureBaseData&								   BaseData,
		TSharedPtr<FMediaCaptureUserData, ESPMode::ThreadSafe> UserData,
		FTextureRHIRef										   Texture) override;

	virtual void OnRHIResourceCaptured_RenderingThread(
		FRHICommandListImmediate& RHICmdList,
		const FCaptureBaseData& BaseData,
		TSharedPtr<FMediaCaptureUserData, ESPMode::ThreadSafe> UserData,
		FTextureRHIRef Texture) override;

	virtual void OnFrameCaptured_AnyThread(
		const FCaptureBaseData&								   BaseData,
		TSharedPtr<FMediaCaptureUserData, ESPMode::ThreadSafe> UserData,
		const FMediaCaptureResourceData&					   ResourceData) override;

	virtual void OnCustomCapture_RenderingThread(
		FRDGBuilder&										   GraphBuilder,
		const FCaptureBaseData&								   InBaseData,
		TSharedPtr<FMediaCaptureUserData, ESPMode::ThreadSafe> InUserData,
		FRDGTextureRef										   InSourceTexture,
		FRDGTextureRef										   OutputTexture,
		const FRHICopyTextureInfo&							   CopyInfo,
		FVector2D											   CropU,
		FVector2D											   CropV) override;

	virtual void OnFrameCaptured_RenderingThread(
		const FCaptureBaseData& InBaseData,
		TSharedPtr<FMediaCaptureUserData,ESPMode::ThreadSafe> InUserData,
		void* InBuffer,
		int32 Width,
		int32 Height,
		int32 BytesPerRow) override;

	virtual bool ShouldCaptureRHIResource() const override
	{
		return Format == PixelCaptureBufferFormat::FORMAT_RHI;
	}

	virtual bool SupportsAnyThreadCapture() const
	{
#if PLATFORM_MAC
		// On Mac, Media Capture must capture cpu frames on the render thread. 
		// This is because MediaCapture Lock_Unsafe uses RHIMapStagingSurface 
		// which needs to run on the rendering thread with Metal.
		return Format == PixelCaptureBufferFormat::FORMAT_RHI;
#else // PLATFORM_MAC
		return true;
#endif // PLATFORM_MAC
	}

	virtual ETextureCreateFlags GetOutputTextureFlags() const override;
	virtual void				WaitForGPU(FRHITexture* InRHITexture) override;

private:
	int32					  Format = PixelCaptureBufferFormat::FORMAT_UNKNOWN;

	TDeque<FGPUFenceRHIRef> Fences;

	TDeque<TSharedPtr<IPixelCaptureOutputFrame>> OutputFrames;
};

/**
 * A MediaIO based capturer that will copy and convert RHI texture frames.
 * Input: FPixelCaptureInputFrameRHI
 * Output: FPixelCaptureOutputFrameRHI/FPixelCaptureOutputFrameI420
 */
class PIXELCAPTURE_API FPixelCaptureCapturerMediaCapture : public FPixelCaptureCapturer, public TSharedFromThis<FPixelCaptureCapturerMediaCapture>
{
public:
	/**
	 * Creates a new Capturer capturing the input frame at the given scale.
	 * @param InScale The scale of the resulting output capture.
	 */
	static TSharedPtr<FPixelCaptureCapturerMediaCapture> Create(float InScale, int32 InFormat);
	virtual ~FPixelCaptureCapturerMediaCapture() override;

protected:
	virtual FString					  GetCapturerName() const override { return "MediaCapture Copy"; }
	virtual IPixelCaptureOutputFrame* CreateOutputBuffer(int32 InputWidth, int32 InputHeight) override;
	virtual void					  BeginProcess(const IPixelCaptureInputFrame& InputFrame, TSharedPtr<IPixelCaptureOutputFrame> OutputBuffer) override;

private:
	uint64 LastFrameCounterRenderThread = 0;
	float Scale = 1.0f;
	int32 Format = PixelCaptureBufferFormat::FORMAT_UNKNOWN;
	bool  bMediaCaptureInitialized = false;

	FPixelCaptureCapturerMediaCapture(float InScale, int32 InFormat);
	void InitializeMediaCapture();

	void InvalidateOutputBuffer(TSharedPtr<IPixelCaptureOutputFrame> OutputBuffer);

	UPROPERTY(Transient)
	TObjectPtr<UPixelCaptureMediaCapture> MediaCapture = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UPixelCaptureMediaOuput> MediaOutput = nullptr;
};
