// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "epic_rtc/common/defines.h"
#include "epic_rtc/common/common.h"
#include "epic_rtc/containers/epic_rtc_string.h"
#include "epic_rtc/containers/epic_rtc_string_view.h"
#include "epic_rtc/core/ref_count.h"
#include "epic_rtc/core/session.h"
#include "epic_rtc/core/session_config.h"

#pragma pack(push, 8)

/**
 * Represents the library API instance.
 */
class EpicRtcConferenceInterface : public EpicRtcRefCountInterface
{
public:
    /**
     * Gets instance Id
     * @return Id
     */
    virtual EPICRTC_API EpicRtcStringView GetId() = 0;

    /**
     * Creates session with specified config.
     * @param InConfig Session config.
     * @return New session.
     */
    virtual EPICRTC_API EpicRtcErrorCode CreateSession(const EpicRtcSessionConfig& inConfig, EpicRtcSessionInterface** outSession) = 0;

    /**
     * Gets EpicRtcSessionInterface
     * @param InSessionId Session id
     * @return EMRTC Session or nullptr
     */
    virtual EPICRTC_API EpicRtcErrorCode GetSession(const EpicRtcStringView& inSessionId, EpicRtcSessionInterface** outSession) = 0;

    // FIXME: discuss self-destruction on disconnect
    virtual EPICRTC_API void RemoveSession(const EpicRtcStringView& inSessionId) = 0;

    /**
     * Should be called from thread to process observed events. Returns false if the queue was empty.
     */
    virtual EPICRTC_API EpicRtcBool Tick() = 0;

    /**
     * Indicates whether there are any observed events that require processing with Tick().
     */
    virtual EPICRTC_API EpicRtcBool NeedsTick() = 0;

    /**
     * Enable stats subsystem
     */
    virtual EPICRTC_API void EnableStats() = 0;

    /**
     * Disable stats subsystem
     */
    virtual EPICRTC_API void DisableStats() = 0;
};

#pragma pack(pop)
