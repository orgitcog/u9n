// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <string_view>

namespace EpicRtc::Signalling::JoinOptions
{

    inline static const std::string_view RESOLUTION_CONTROLLER = "resolution_controller";
    inline static const std::string_view SIMULCAST = "simulcast";
    inline static const std::string_view SUBSCRIBE = "subscribe";
    inline static const std::string_view P2P = "p2p";
    inline static const std::string_view DTX = "dtx";
    inline static const std::string_view DTLS_SRTP = "dtls_srtp";
    inline static const std::string_view SW_AEC = "sw_aec";
    inline static const std::string_view OPENSLES = "opensles";
    inline static const std::string_view POWER_MODE = "power_mode";
    inline static const std::string_view AUDIO_RECONNECT = "audio_reconnect";
    inline static const std::string_view DEVICE_RECONNECT = "device_reconnect";
    inline static const std::string_view H264_P2P = "h264_p2p";
    inline static const std::string_view STEREO = "stereo";
    inline static const std::string_view FAILING_STATUS = "failing_status";
    inline static const std::string_view VIDEO_MAX_BITRATE_P2P = "video_max_bitrate_p2p";
    inline static const std::string_view MANUAL_AUDIO = "manual_audio";
    inline static const std::string_view RTX = "rtx";
    inline static const std::string_view VIDEO_CONTENT_TYPE = "video_content_type";
    inline static const std::string_view METAL = "metal";
    inline static const std::string_view ECHO = "echo";
    inline static const std::string_view SPEAKING = "speaking";
    inline static const std::string_view DOMINANT_SPEAKER = "dominant_speaker";
    inline static const std::string_view MULTISTREAM = "multistream";
    inline static const std::string_view SYNC_SUBSCRIBE = "sync_subscribe";
    inline static const std::string_view SCTP = "sctp";
    inline static const std::string_view INTERNALS = "internals";
    inline static const std::string_view RESERVED_AUDIO_STREAMS = "reserved_audio_streams";
    inline static const std::string_view PADDING = "padding";
    inline static const std::string_view TEST_FORCE_PROXIMITY_ROOM = "test_force_proximity_room";
    inline static const std::string_view UNIFIED_PLAN = "unified_plan";
    inline static const std::string_view RAS_CSRC = "ras_csrc";
    inline static const std::string_view SIGNED_AUDIO = "signed_audio";
    inline static const std::string_view SPEAKING_SELF_ONLY = "speaking_self_only";

}  // namespace EpicRtc::Signalling::JoinOptions
