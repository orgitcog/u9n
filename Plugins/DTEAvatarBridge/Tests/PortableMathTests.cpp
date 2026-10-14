#include "DTEAvatarBridgeMath.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace
{
    int Failures = 0;

    void Check(const bool Condition, const char* Message)
    {
        if (!Condition)
        {
            ++Failures;
            std::cerr << "FAILED: " << Message << '\n';
        }
    }

    bool Near(const float A, const float B, const float Tolerance = 0.0001f)
    {
        return std::fabs(A - B) <= Tolerance;
    }

    void TestNonFiniteAndRangeHandling()
    {
        using namespace DTEAvatarBridge::Math;
        TelemetryInput Input;
        Input.Speed = std::numeric_limits<float>::infinity();
        Input.Threat = std::numeric_limits<float>::quiet_NaN();
        Input.Novelty = -3.0f;
        Input.Reward = 2.0f;

        const MappedTelemetry Mapped = MapTelemetry(Input, MappingConfig{600.0f});
        Check(Near(Mapped.Speed, 0.0f), "infinite speed becomes zero");
        Check(Near(Mapped.NormalizedSpeed, 0.0f), "infinite speed normalizes to zero");
        Check(Near(Mapped.Threat, 0.0f), "NaN threat becomes zero");
        Check(Near(Mapped.Novelty, 0.0f), "negative novelty clamps to zero");
        Check(Near(Mapped.Reward, 1.0f), "reward above one clamps to one");
        Check(Mapped.SuggestedGait == Gait::Idle, "zero normalized speed selects idle");
        Check(Near(NormalizeSpeed(300.0f, 0.0f), 0.0f), "zero max speed remains safe");
    }

    void TestDeterministicMappings()
    {
        using namespace DTEAvatarBridge::Math;
        const TelemetryInput Input{300.0f, 0.60f, 0.80f, 0.40f};
        const MappedTelemetry First = MapTelemetry(Input, MappingConfig{600.0f});
        const MappedTelemetry Second = MapTelemetry(Input, MappingConfig{600.0f});

        Check(Near(First.Speed, 300.0f), "speed preserves bounded physical magnitude");
        Check(Near(First.NormalizedSpeed, 0.5f), "speed normalizes deterministically");
        Check(First.SuggestedGait == Gait::Stealth, "threat threshold selects stealth");
        Check(Near(First.Wonder, 0.7f), "wonder mapping is documented weighted sum");
        Check(Near(First.Awe, 0.488f), "awe mapping is deterministic product");
        Check(Near(First.WingSpread, 0.75f), "wing spread mapping is deterministic");
        Check(Near(First.EmissiveStrength, 0.55f), "emissive mapping is deterministic");
        Check(Near(First.Wonder, Second.Wonder) && First.SuggestedGait == Second.SuggestedGait,
            "identical input produces identical mapping");
        Check(SuggestGait(0.5f, 0.8f) == Gait::Combat, "combat threshold is inclusive");
        Check(SuggestGait(0.5f, 0.25f) == Gait::Cautious, "cautious threshold is inclusive");
    }

    void TestFirstFramePredictionErrorSemantics()
    {
        using namespace DTEAvatarBridge::Math;
        const float FirstFrame[] = {0.2f, 0.6f, 1.0f, 0.0f};
        const FrameComparison First = CompareLuminanceFrames(FirstFrame, nullptr, 4, false, 0.8f, true);
        Check(Near(First.NormalizedLuminance, 0.45f), "first frame mean luminance is observed");
        Check(Near(First.MotionEnergy, 0.0f), "first frame motion is zero");
        Check(Near(First.PredictionError, 0.0f), "first frame prediction error is explicitly zero");

        const float SecondFrame[] = {0.4f, 0.8f, 1.0f, 0.0f};
        const FrameComparison Second = CompareLuminanceFrames(SecondFrame, FirstFrame, 4, true, 0.8f, true);
        Check(Near(Second.NormalizedLuminance, 0.55f), "second frame mean luminance is observed");
        Check(Near(Second.MotionEnergy, 0.1f), "motion energy is mean absolute frame delta");
        Check(Near(Second.PredictionError, 0.25f), "prediction error starts on second observed frame");
    }

    void TestVariableDeltaTimeSmoothing()
    {
        using namespace DTEAvatarBridge::Math;
        const float OneStep = ExponentialSmoothing(0.0f, 1.0f, 0.2f, 0.4f);
        float TwoSteps = ExponentialSmoothing(0.0f, 1.0f, 0.05f, 0.4f);
        TwoSteps = ExponentialSmoothing(TwoSteps, 1.0f, 0.15f, 0.4f);
        Check(Near(OneStep, TwoSteps), "exponential smoothing is partition-invariant for variable DeltaTime");
        Check(Near(ExponentialSmoothing(0.25f, 1.0f, 0.0f, 0.4f), 0.25f), "zero DeltaTime preserves current value");
        Check(Near(ExponentialSmoothing(0.25f, 1.0f, -1.0f, 0.4f), 0.25f), "negative DeltaTime preserves current value");
        Check(Near(ExponentialSmoothing(0.25f, 1.0f, 0.1f, 0.0f), 1.0f), "zero time constant snaps safely");
    }

    void TestCaptureLimits()
    {
        using namespace DTEAvatarBridge::Math;
        Check(IsCaptureConfigurationWithinLimits(32, 5.0f), "default capture configuration is allowed");
        Check(IsCaptureConfigurationWithinLimits(8, 0.1f), "inclusive lower capture limits are allowed");
        Check(IsCaptureConfigurationWithinLimits(64, 10.0f), "inclusive upper capture limits are allowed");
        Check(!IsCaptureConfigurationWithinLimits(7, 5.0f), "too-small capture resolution is rejected");
        Check(!IsCaptureConfigurationWithinLimits(65, 5.0f), "too-large capture resolution is rejected");
        Check(!IsCaptureConfigurationWithinLimits(32, 10.1f), "too-fast capture rate is rejected");
        Check(!IsCaptureConfigurationWithinLimits(32, std::numeric_limits<float>::quiet_NaN()), "NaN capture rate is rejected");
    }
}

int main()
{
    TestNonFiniteAndRangeHandling();
    TestDeterministicMappings();
    TestFirstFramePredictionErrorSemantics();
    TestVariableDeltaTimeSmoothing();
    TestCaptureLimits();

    if (Failures == 0)
    {
        std::cout << "DTEAvatarBridge portable math tests passed" << std::endl;
        return EXIT_SUCCESS;
    }

    std::cerr << Failures << " test assertion(s) failed" << std::endl;
    return EXIT_FAILURE;
}
