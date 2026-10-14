// Copyright Deep Tree Echo. All Rights Reserved.
#include "DTEAvatarTelemetryComponent.h"

#include "DTEAvatarBridgeMath.h"
#include "Math/UnrealMathUtility.h"

namespace
{
    EDTEAvatarGait ToUnrealGait(const DTEAvatarBridge::Math::Gait Gait)
    {
        switch (Gait)
        {
        case DTEAvatarBridge::Math::Gait::Explore: return EDTEAvatarGait::Explore;
        case DTEAvatarBridge::Math::Gait::Cautious: return EDTEAvatarGait::Cautious;
        case DTEAvatarBridge::Math::Gait::Stealth: return EDTEAvatarGait::Stealth;
        case DTEAvatarBridge::Math::Gait::Combat: return EDTEAvatarGait::Combat;
        case DTEAvatarBridge::Math::Gait::Idle:
        default: return EDTEAvatarGait::Idle;
        }
    }

    FDTEAvatarTelemetry ToUnrealTelemetry(const DTEAvatarBridge::Math::MappedTelemetry& Input)
    {
        FDTEAvatarTelemetry Output;
        Output.Speed = Input.Speed;
        Output.NormalizedSpeed = Input.NormalizedSpeed;
        Output.NormalizedThreat = Input.Threat;
        Output.NormalizedNovelty = Input.Novelty;
        Output.NormalizedReward = Input.Reward;
        Output.SuggestedGait = ToUnrealGait(Input.SuggestedGait);
        Output.Wonder = Input.Wonder;
        Output.Awe = Input.Awe;
        Output.Alert = Input.Alert;
        Output.WingSpread = Input.WingSpread;
        Output.EmissiveStrength = Input.EmissiveStrength;
        return Output;
    }

    bool HasMeaningfulDifference(const FDTEAvatarTelemetry& A, const FDTEAvatarTelemetry& B)
    {
        constexpr float Tolerance = KINDA_SMALL_NUMBER;
        return !FMath::IsNearlyEqual(A.Speed, B.Speed, Tolerance)
            || !FMath::IsNearlyEqual(A.NormalizedSpeed, B.NormalizedSpeed, Tolerance)
            || !FMath::IsNearlyEqual(A.NormalizedThreat, B.NormalizedThreat, Tolerance)
            || !FMath::IsNearlyEqual(A.NormalizedNovelty, B.NormalizedNovelty, Tolerance)
            || !FMath::IsNearlyEqual(A.NormalizedReward, B.NormalizedReward, Tolerance)
            || !FMath::IsNearlyEqual(A.Wonder, B.Wonder, Tolerance)
            || !FMath::IsNearlyEqual(A.Awe, B.Awe, Tolerance)
            || !FMath::IsNearlyEqual(A.Alert, B.Alert, Tolerance)
            || !FMath::IsNearlyEqual(A.WingSpread, B.WingSpread, Tolerance)
            || !FMath::IsNearlyEqual(A.EmissiveStrength, B.EmissiveStrength, Tolerance)
            || A.SuggestedGait != B.SuggestedGait;
    }
}

UDTEAvatarTelemetryComponent::UDTEAvatarTelemetryComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UDTEAvatarTelemetryComponent::TickComponent(const float DeltaTime, const ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    check(IsInGameThread());

    if (bHasReceivedTelemetry)
    {
        UpdateSmoothedTelemetry(DeltaTime);
    }
}

void UDTEAvatarTelemetryComponent::SetCognitiveTelemetry(
    const float Speed,
    const float NormalizedThreat,
    const float NormalizedNovelty,
    const float NormalizedReward)
{
    check(IsInGameThread());

    DTEAvatarBridge::Math::MappingConfig Config;
    Config.MaxSpeed = MaximumSpeed;

    DTEAvatarBridge::Math::TelemetryInput Input;
    Input.Speed = Speed;
    Input.Threat = NormalizedThreat;
    Input.Novelty = NormalizedNovelty;
    Input.Reward = NormalizedReward;
    TargetTelemetry = ToUnrealTelemetry(DTEAvatarBridge::Math::MapTelemetry(Input, Config));

    // The first valid telemetry packet produces a stable, immediate presentation
    // state rather than a fade-in from an unrelated neutral value.
    if (!bHasReceivedTelemetry)
    {
        const FDTEAvatarTelemetry Previous = CurrentTelemetry;
        CurrentTelemetry = TargetTelemetry;
        bHasReceivedTelemetry = true;
        PublishIfChanged(Previous);
    }
}

FDTEAvatarTelemetry UDTEAvatarTelemetryComponent::GetCurrentTelemetry() const
{
    return CurrentTelemetry;
}

void UDTEAvatarTelemetryComponent::ResetTelemetry()
{
    check(IsInGameThread());
    const FDTEAvatarTelemetry Previous = CurrentTelemetry;
    TargetTelemetry = FDTEAvatarTelemetry();
    CurrentTelemetry = FDTEAvatarTelemetry();
    bHasReceivedTelemetry = false;
    PublishIfChanged(Previous);
}

void UDTEAvatarTelemetryComponent::UpdateSmoothedTelemetry(const float DeltaTime)
{
    const FDTEAvatarTelemetry Previous = CurrentTelemetry;
    const float SafeMaximumSpeed = DTEAvatarBridge::Math::ClampNonNegative(MaximumSpeed);
    const float SafeTimeConstant = DTEAvatarBridge::Math::ClampNonNegative(SmoothingTimeConstant);

    auto SmoothUnit = [DeltaTime, SafeTimeConstant](const float Current, const float Target)
    {
        return DTEAvatarBridge::Math::Clamp01(
            DTEAvatarBridge::Math::ExponentialSmoothing(Current, Target, DeltaTime, SafeTimeConstant));
    };

    CurrentTelemetry.Speed = FMath::Clamp(
        DTEAvatarBridge::Math::ExponentialSmoothing(CurrentTelemetry.Speed, TargetTelemetry.Speed, DeltaTime, SafeTimeConstant),
        0.0f,
        SafeMaximumSpeed);
    CurrentTelemetry.NormalizedSpeed = SafeMaximumSpeed > 0.0f
        ? DTEAvatarBridge::Math::Clamp01(CurrentTelemetry.Speed / SafeMaximumSpeed)
        : 0.0f;
    CurrentTelemetry.NormalizedThreat = SmoothUnit(CurrentTelemetry.NormalizedThreat, TargetTelemetry.NormalizedThreat);
    CurrentTelemetry.NormalizedNovelty = SmoothUnit(CurrentTelemetry.NormalizedNovelty, TargetTelemetry.NormalizedNovelty);
    CurrentTelemetry.NormalizedReward = SmoothUnit(CurrentTelemetry.NormalizedReward, TargetTelemetry.NormalizedReward);
    CurrentTelemetry.Wonder = SmoothUnit(CurrentTelemetry.Wonder, TargetTelemetry.Wonder);
    CurrentTelemetry.Awe = SmoothUnit(CurrentTelemetry.Awe, TargetTelemetry.Awe);
    CurrentTelemetry.Alert = SmoothUnit(CurrentTelemetry.Alert, TargetTelemetry.Alert);
    CurrentTelemetry.WingSpread = SmoothUnit(CurrentTelemetry.WingSpread, TargetTelemetry.WingSpread);
    CurrentTelemetry.EmissiveStrength = SmoothUnit(CurrentTelemetry.EmissiveStrength, TargetTelemetry.EmissiveStrength);

    // Gait is a deliberate suggestion derived from the target cognitive state,
    // not a locomotion command and not an interpolated enum.
    CurrentTelemetry.SuggestedGait = TargetTelemetry.SuggestedGait;
    PublishIfChanged(Previous);
}

void UDTEAvatarTelemetryComponent::PublishIfChanged(const FDTEAvatarTelemetry& PreviousTelemetry)
{
    if (!HasMeaningfulDifference(PreviousTelemetry, CurrentTelemetry))
    {
        return;
    }

    OnTelemetryUpdated.Broadcast(CurrentTelemetry);
    if (PreviousTelemetry.SuggestedGait != CurrentTelemetry.SuggestedGait)
    {
        OnSuggestedGaitChanged.Broadcast(PreviousTelemetry.SuggestedGait, CurrentTelemetry.SuggestedGait);
    }
}
