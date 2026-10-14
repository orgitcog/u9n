// Copyright Deep Tree Echo. All Rights Reserved.
// UE-independent mapping helpers. This header deliberately does not include Unreal headers.
#pragma once

#include <cmath>
#include <cstddef>

namespace DTEAvatarBridge::Math
{
    enum class Gait : unsigned char
    {
        Idle,
        Explore,
        Cautious,
        Stealth,
        Combat
    };

    struct TelemetryInput
    {
        float Speed = 0.0f;
        float Threat = 0.0f;
        float Novelty = 0.0f;
        float Reward = 0.0f;
    };

    struct MappingConfig
    {
        float MaxSpeed = 600.0f;
    };

    struct MappedTelemetry
    {
        float Speed = 0.0f;
        float NormalizedSpeed = 0.0f;
        float Threat = 0.0f;
        float Novelty = 0.0f;
        float Reward = 0.0f;
        Gait SuggestedGait = Gait::Idle;
        float Wonder = 0.0f;
        float Awe = 0.0f;
        float Alert = 0.0f;
        float WingSpread = 0.0f;
        float EmissiveStrength = 0.0f;
    };

    struct CaptureLimits
    {
        int MinimumResolution = 8;
        int MaximumResolution = 64;
        float MinimumRateHz = 0.1f;
        float MaximumRateHz = 10.0f;
    };

    struct FrameComparison
    {
        float NormalizedLuminance = 0.0f;
        float MotionEnergy = 0.0f;
        float PredictionError = 0.0f;
    };

    inline bool IsFinite(const float Value)
    {
        return std::isfinite(Value) != 0;
    }

    inline float FiniteOr(const float Value, const float Fallback = 0.0f)
    {
        return IsFinite(Value) ? Value : Fallback;
    }

    inline float Clamp01(const float Value)
    {
        const float SafeValue = FiniteOr(Value);
        return SafeValue <= 0.0f ? 0.0f : (SafeValue >= 1.0f ? 1.0f : SafeValue);
    }

    inline float ClampNonNegative(const float Value)
    {
        const float SafeValue = FiniteOr(Value);
        return SafeValue > 0.0f ? SafeValue : 0.0f;
    }

    inline float NormalizeSpeed(const float Speed, const float MaxSpeed)
    {
        const float SafeMaximum = ClampNonNegative(MaxSpeed);
        if (SafeMaximum <= 0.0f)
        {
            return 0.0f;
        }
        return Clamp01(ClampNonNegative(Speed) / SafeMaximum);
    }

    // Stable under variable DeltaTime. A zero/invalid DeltaTime preserves Current.
    inline float ExponentialSmoothing(const float Current, const float Target, const float DeltaTime, const float TimeConstant)
    {
        const float SafeCurrent = FiniteOr(Current);
        const float SafeTarget = FiniteOr(Target);
        const float SafeDeltaTime = ClampNonNegative(DeltaTime);
        const float SafeTimeConstant = ClampNonNegative(TimeConstant);
        if (SafeDeltaTime <= 0.0f)
        {
            return SafeCurrent;
        }
        if (SafeTimeConstant <= 0.0f)
        {
            return SafeTarget;
        }
        const float Alpha = 1.0f - std::exp(-SafeDeltaTime / SafeTimeConstant);
        return SafeCurrent + (SafeTarget - SafeCurrent) * Clamp01(Alpha);
    }

    inline Gait SuggestGait(const float NormalizedSpeed, const float Threat)
    {
        if (NormalizedSpeed <= 0.03f)
        {
            return Gait::Idle;
        }
        if (Threat >= 0.80f)
        {
            return Gait::Combat;
        }
        if (Threat >= 0.55f)
        {
            return Gait::Stealth;
        }
        if (Threat >= 0.25f)
        {
            return Gait::Cautious;
        }
        return Gait::Explore;
    }

    inline MappedTelemetry MapTelemetry(const TelemetryInput& Input, const MappingConfig& Config = MappingConfig())
    {
        MappedTelemetry Result;
        const float SafeMaximumSpeed = ClampNonNegative(Config.MaxSpeed);
        Result.NormalizedSpeed = NormalizeSpeed(Input.Speed, SafeMaximumSpeed);
        Result.Speed = Result.NormalizedSpeed * SafeMaximumSpeed;
        Result.Threat = Clamp01(Input.Threat);
        Result.Novelty = Clamp01(Input.Novelty);
        Result.Reward = Clamp01(Input.Reward);
        Result.SuggestedGait = SuggestGait(Result.NormalizedSpeed, Result.Threat);

        // The mappings are intentionally transparent, bounded, and asset-agnostic.
        Result.Wonder = Clamp01(0.75f * Result.Novelty + 0.25f * Result.Reward);
        Result.Awe = Clamp01(Result.Novelty * (0.35f + 0.65f * Result.Reward));
        Result.Alert = Result.Threat;
        Result.WingSpread = Clamp01(0.15f + 0.45f * Result.Novelty + 0.40f * Result.Threat);
        Result.EmissiveStrength = Clamp01(0.15f + 0.70f * Result.Reward + 0.15f * Result.Novelty);
        return Result;
    }

    inline bool IsCaptureConfigurationWithinLimits(const int Resolution, const float RateHz, const CaptureLimits& Limits = CaptureLimits())
    {
        return Resolution >= Limits.MinimumResolution
            && Resolution <= Limits.MaximumResolution
            && IsFinite(RateHz)
            && RateHz >= Limits.MinimumRateHz
            && RateHz <= Limits.MaximumRateHz;
    }

    // First-frame semantics are explicit: without a previous observation, both
    // motion energy and efference/prediction error are zero. No imagined baseline
    // is introduced before the component has an observed temporal delta.
    inline FrameComparison CompareLuminanceFrames(
        const float* Current,
        const float* Previous,
        const std::size_t Count,
        const bool HasPreviousFrame,
        const float PredictedLuminance,
        const bool HasPrediction)
    {
        FrameComparison Result;
        if (Current == nullptr || Count == 0)
        {
            return Result;
        }

        for (std::size_t Index = 0; Index < Count; ++Index)
        {
            Result.NormalizedLuminance += Clamp01(Current[Index]);
        }
        Result.NormalizedLuminance /= static_cast<float>(Count);

        if (!HasPreviousFrame || Previous == nullptr)
        {
            return Result;
        }

        for (std::size_t Index = 0; Index < Count; ++Index)
        {
            Result.MotionEnergy += std::fabs(Clamp01(Current[Index]) - Clamp01(Previous[Index]));
        }
        Result.MotionEnergy /= static_cast<float>(Count);

        if (HasPrediction)
        {
            Result.PredictionError = std::fabs(Result.NormalizedLuminance - Clamp01(PredictedLuminance));
        }
        return Result;
    }
}
