// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EpicRtcVideoBuffer.h"
#include "PixelCaptureBufferI420.h"

namespace UE::PixelStreaming2
{
	class FEpicRtcVideoBufferI420 : public FEpicRtcVideoBuffer
	{
	public:
		FEpicRtcVideoBufferI420(TSharedPtr<FPixelCaptureBufferI420> Buffer)
			: Buffer(Buffer)
		{
		}

		virtual ~FEpicRtcVideoBufferI420() = default;

	public:
		// Begin FEpicRtcVideoBuffer
		virtual int32 GetBufferFormat() override
		{
			return PixelCaptureBufferFormat::FORMAT_I420;
		}
		// End FEpicRtcVideoBuffer

		// Begin EpicRtcVideoBufferInterface
		virtual void* GetData() override
		{
			return Buffer->GetMutableData();
		}

		virtual int GetWidth() override
		{
			return Buffer->GetWidth();
		}

		virtual int GetHeight() override
		{
			return Buffer->GetHeight();
		}
		// End EpicRtcVideoBufferInterface

		TSharedPtr<FPixelCaptureBufferI420> GetBuffer()
		{
			return Buffer;
		}

	private:
		TSharedPtr<FPixelCaptureBufferI420> Buffer;
	};
} // namespace UE::PixelStreaming2