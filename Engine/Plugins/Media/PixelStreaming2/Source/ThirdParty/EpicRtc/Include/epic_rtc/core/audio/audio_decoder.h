// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "epic_rtc/common/defines.h"
#include "epic_rtc/common/common.h"

#include "epic_rtc/core/audio/audio_format.h"
#include "epic_rtc/core/audio/audio_frame.h"
#include "epic_rtc/core/audio/audio_decoder_config.h"

#pragma pack(push, 8)

/**
 * Interface to describe a EpicRTC compatible Audio Decoder
 */
class EpicRtcAudioDecoderInterface : public EpicRtcRefCountInterface
{
public:
    /**
     * Get uniquely identifiable Encoder implementation name
     */
    virtual EPICRTC_API EpicRtcStringView GetName() const = 0;

    /**
     * Get current configuration of decoder instance
     */
    virtual EPICRTC_API EpicRtcAudioDecoderConfig GetConfig() const = 0;

    /**
     * Set configuration of decoder instance
     * @note be careful when manually setting this as it is likely set automatically internal of the API
     */
    virtual EPICRTC_API EpicRtcMediaResult SetConfig(const EpicRtcAudioDecoderConfig& inAudioDecoderConfig) = 0;

    /**
     * Function that does actual encoding of audio expected to be blocking and synchronous
     * @return EpicRtcAudioFrame memory could be accessed asynchronously so memory should only be deallocated with Release method
     */
    virtual EPICRTC_API EpicRtcAudioFrame Decode(EpicRtcEncodedAudioFrame& inEncodedAudioFrame) = 0;

    /**
     * Resets decoder to zeroed state
     */
    virtual EPICRTC_API void Reset() = 0;

    /**
     * Internal usage only, overload if you know what you are doing
     */
    virtual EPICRTC_API EpicRtcBool IsInbuilt() const { return false; }
};

/**
 * Describes how to initialize a custom Audio Encoder that has passed into EpicRTC
 */
class EpicRtcAudioDecoderInitializerInterface : public EpicRtcRefCountInterface
{
public:
    virtual EPICRTC_API EpicRtcErrorCode CreateDecoder(const EpicRtcAudioCodecInfo& codecInfo, EpicRtcAudioDecoderInterface** outDecoder) = 0;
    virtual EPICRTC_API EpicRtcAudioCodecInfoArrayInterface* GetSupportedCodecs() = 0;
};

#pragma pack(pop)
