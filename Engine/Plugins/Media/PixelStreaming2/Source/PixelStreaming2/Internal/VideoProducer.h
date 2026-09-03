// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/DelegateCombinations.h"
#include "IPixelStreaming2VideoProducer.h"
#include "MediaCapture.h"
#include "Templates/SharedPointer.h"

namespace UE::PixelStreaming2
{
	namespace VideoProducerIdentifiers
	{
		PIXELSTREAMING2_API extern const FString FVideoProducer;
		PIXELSTREAMING2_API extern const FString FVideoProducerBackBuffer;
		PIXELSTREAMING2_API extern const FString FVideoProducerMediaCapture;
		PIXELSTREAMING2_API extern const FString FVideoProducerPIEViewport;
		PIXELSTREAMING2_API extern const FString FVideoProducerRenderTarget;
	} // namespace VideoProducerIdentifiers

	class PIXELSTREAMING2_API FVideoProducerUserData : public FMediaCaptureUserData, public FPixelCaptureUserData
	{
	public:
		FVideoProducerUserData() = default;

		uint64	ProductionBeginCycles = 0;
		uint64	ProductionEndCycles = 0;
		FString ProducerName = TEXT("");
	};

	class PIXELSTREAMING2_API FVideoProducer : public IPixelStreaming2VideoProducer
	{
	public:
		static TSharedPtr<FVideoProducer> Create();
		virtual ~FVideoProducer() = default;

		DECLARE_EVENT_OneParam(FVideoProducer, FOnFramePushed, const IPixelCaptureInputFrame&);
		FOnFramePushed OnFramePushed;

		virtual void PushFrame(const IPixelCaptureInputFrame& InputFrame) override;

		virtual FString ToString() override;

		virtual bool IsFrameAlreadyCopied() { return false; }

	protected:
		FVideoProducer() = default;
	};

} // namespace UE::PixelStreaming2