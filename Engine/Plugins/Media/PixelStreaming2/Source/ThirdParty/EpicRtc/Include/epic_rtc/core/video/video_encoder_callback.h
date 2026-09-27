// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "epic_rtc/core/video/video_common.h"
#include "epic_rtc/core/video/video_frame.h"

#pragma pack(push, 8)

class EpicRtcVideoEncoderCallbackInterface : public EpicRtcRefCountInterface
{
public:
    virtual EPICRTC_API EpicRtcVideoEncodedResult Encoded(const EpicRtcEncodedVideoFrame& frame, const EpicRtcCodecSpecificInfo& codecSpecificInfo) = 0;
    virtual EPICRTC_API void OnDroppedFrame(EpicRtcVideoFrameDropReason reason) = 0;

    EpicRtcVideoEncoderCallbackInterface(const EpicRtcVideoEncoderCallbackInterface&) = delete;
    EpicRtcVideoEncoderCallbackInterface& operator=(const EpicRtcVideoEncoderCallbackInterface&) = delete;

protected:
    EPICRTC_API EpicRtcVideoEncoderCallbackInterface() = default;
    virtual EPICRTC_API ~EpicRtcVideoEncoderCallbackInterface() = default;
};

#pragma pack(pop)
