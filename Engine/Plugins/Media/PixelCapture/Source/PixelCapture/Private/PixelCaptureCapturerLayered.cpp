// Copyright Epic Games, Inc. All Rights Reserved.

#include "PixelCaptureCapturerLayered.h"
#include "PixelCaptureCapturer.h"
#include "PixelCapturePrivate.h"
#include "Misc/ScopeLock.h"

TSharedPtr<FPixelCaptureCapturerLayered> FPixelCaptureCapturerLayered::Create(IPixelCaptureCapturerSource* InCapturerSource, int32 InDestinationFormat, TArray<float> LayerScales)
{
	return TSharedPtr<FPixelCaptureCapturerLayered>(new FPixelCaptureCapturerLayered(InCapturerSource, InDestinationFormat, LayerScales));
}

FPixelCaptureCapturerLayered::FPixelCaptureCapturerLayered(IPixelCaptureCapturerSource* InCapturerSource, int32 InDestinationFormat, TArray<float> InLayerScales)
	: CapturerSource(InCapturerSource)
	, DestinationFormat(InDestinationFormat)
	, LayerScales(InLayerScales)
{
}

TSharedPtr<IPixelCaptureOutputFrame> FPixelCaptureCapturerLayered::ReadOutput(int32 LayerIndex)
{
	FScopeLock LayersLock(&LayersGuard);
	if (!LayerCapturers.IsEmpty() && LayerCapturers.Num() > LayerIndex)
	{
		return LayerCapturers[LayerIndex]->ReadOutput();
	}

	UE_LOG(LogPixelCapture, Warning, TEXT("No capturer configured for LayerIndex (%d)!"), LayerIndex);
	return nullptr;
}

void FPixelCaptureCapturerLayered::AddLayer(float Scale)
{
	TSharedPtr<FPixelCaptureCapturer> CaptureProcess = CapturerSource->CreateCapturer(DestinationFormat, Scale);
	CaptureProcess->OnComplete.AddSP(AsShared(), &FPixelCaptureCapturerLayered::OnCaptureComplete);
	LayerCapturers.Add(CaptureProcess);
}

void FPixelCaptureCapturerLayered::OnCaptureComplete()
{
	OnComplete.Broadcast();
}

void FPixelCaptureCapturerLayered::Capture(const IPixelCaptureInputFrame& SourceFrame)
{
	// work on a temp list so we dont over lock
	TArray<TSharedPtr<FPixelCaptureCapturer>> TempLayerCapturers;
	{
		FScopeLock LayersLock(&LayersGuard);

		// initial setup. Should only be called on the first frame.
		// We do this lazily because CreateCaptureProcess is a pure virtual function
		// so we cannot call it in our constructor. Another option would be to
		// require the user to call a SetupLayers method or something but that
		// could be prone to errors.
		if (LayerCapturers.IsEmpty())
		{
			for (auto& Scale : LayerScales)
			{
				AddLayer(Scale);
			}
		}

		TempLayerCapturers = LayerCapturers;
	}

	// capture the frame for encoder use
	for (auto& LayerCapturer : TempLayerCapturers)
	{
		LayerCapturer->Capture(SourceFrame);
	}
}
