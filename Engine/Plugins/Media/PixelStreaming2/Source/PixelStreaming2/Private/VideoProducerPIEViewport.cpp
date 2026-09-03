// Copyright Epic Games, Inc. All Rights Reserved.

#include "VideoProducerPIEViewport.h"

#include "Engine/GameViewportClient.h"
#include "PixelCaptureInputFrameRHI.h"
#include "RenderingThread.h"
#include "UnrealClient.h"
#include "UtilsAsync.h"

namespace UE::PixelStreaming2
{

	TSharedPtr<FVideoProducerPIEViewport> FVideoProducerPIEViewport::Create()
	{
		TSharedPtr<FVideoProducerPIEViewport> NewInput = TSharedPtr<FVideoProducerPIEViewport>(new FVideoProducerPIEViewport());
		TWeakPtr<FVideoProducerPIEViewport>	  WeakInput = NewInput;

		UE::PixelStreaming2::DoOnGameThread([WeakInput]() {
			if (TSharedPtr<FVideoProducerPIEViewport> Input = WeakInput.Pin())
			{
				Input->DelegateHandle = UGameViewportClient::OnViewportRendered().AddSP(Input.ToSharedRef(), &FVideoProducerPIEViewport::OnViewportRendered);
			}
		});

		return NewInput;
	}

	FVideoProducerPIEViewport::~FVideoProducerPIEViewport()
	{
		if (!IsEngineExitRequested())
		{
			UGameViewportClient::OnViewportRendered().Remove(DelegateHandle);
		}
	}

	void FVideoProducerPIEViewport::OnViewportRendered(FViewport* InViewport)
	{
		if (!InViewport->IsPlayInEditorViewport())
		{
			return;
		}

		const FTextureRHIRef& FrameBuffer = InViewport->GetRenderTargetTexture();
		if (!FrameBuffer)
		{
			return;
		}

		TSharedPtr<FVideoProducerUserData> UserData = MakeShared<FVideoProducerUserData>();
		UserData->ProducerName = TEXT("FVideoProducerPIEViewport");
		UserData->ProductionBeginCycles = FPlatformTime::Cycles64();

		ENQUEUE_RENDER_COMMAND(StreamViewportTextureCommand)
		([&, FrameBuffer, UserData](FRHICommandList& RHICmdList) {
			UserData->ProductionEndCycles = FPlatformTime::Cycles64();
			PushFrame(FPixelCaptureInputFrameRHI(FrameBuffer, UserData));
		});
	}

	FString FVideoProducerPIEViewport::ToString()
	{
		return VideoProducerIdentifiers::FVideoProducerPIEViewport;
	}

} // namespace UE::PixelStreaming2