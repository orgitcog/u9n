// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "epic_rtc/common/defines.h"
#include "epic_rtc/common/common.h"

#include "epic_rtc/core/video/video_frame.h"

#pragma pack(push, 8)

/**
 * Represents the video track. Exposes methods to send and receive video data.
 */
class EpicRtcVideoTrackInterface : public EpicRtcRefCountInterface
{
public:
    /**
     * Gets instance Id.
     * @return Id.
     */
    virtual EPICRTC_API EpicRtcStringView GetId() = 0;

    /**
     * Mute/unmute the track.
     * @param InMuted State, pass true to mute, false to unmute.
     */
    virtual EPICRTC_API void Mute(EpicRtcBool muted) = 0;

    /**
     * Stop track. Works with the local tracks only.
     */
    virtual EPICRTC_API void Stop() = 0;

    /**
     * Subscribe to remote track.
     */
    virtual EPICRTC_API void Subscribe() = 0;

    /**
     * Unsubscribe from remote track.
     */
    virtual EPICRTC_API void Unsubscribe() = 0;

    /**
     * Pop frame for processing.
     * @return Returns next available frame.
     */
    virtual EPICRTC_API EpicRtcVideoFrame PopFrame() = 0;

    /**
     * Supply frame for processing.
     * @param InFrame Frame to process.
     * @return False if error pushing frame.
     */
    virtual EPICRTC_API EpicRtcBool PushFrame(const EpicRtcVideoFrame& frame) = 0;

    /**
     * Indicates the track belongs to the remote participant.
     * @return True if the track belongs to the remote participant.
     */
    virtual EPICRTC_API EpicRtcBool IsRemote() = 0;

    /**
     * Gets track state.
     * @return State of the track.
     */
    virtual EPICRTC_API EpicRtcTrackState GetState() = 0;

    /**
     * Get track subscription state.
     * @return Subscription state of the track.
     */
    virtual EPICRTC_API EpicRtcTrackSubscriptionState GetSubscriptionState() = 0;

    /**
     * Force video to generate a new key frame.
     * @param rids Array of rids of the videos to generate key frames for. Pass zero rids to generate key frames for all videos.
     */
    virtual EPICRTC_API void GenerateKeyFrame(const EpicRtcStringViewSpan rids) = 0;

    EpicRtcVideoTrackInterface(const EpicRtcVideoTrackInterface&) = delete;
    EpicRtcVideoTrackInterface& operator=(const EpicRtcVideoTrackInterface&) = delete;

protected:
    EPICRTC_API EpicRtcVideoTrackInterface() = default;
    virtual EPICRTC_API ~EpicRtcVideoTrackInterface() = default;
};

#pragma pack(pop)
