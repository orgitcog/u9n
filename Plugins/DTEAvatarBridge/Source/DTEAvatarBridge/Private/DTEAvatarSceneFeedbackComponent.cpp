// Copyright Deep Tree Echo. All Rights Reserved.
#include "DTEAvatarSceneFeedbackComponent.h"

#include "DTEAvatarBridgeMath.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/KismetRenderingLibrary.h"

namespace
{
    constexpr int32 MinimumFeedbackResolution = 8;
    constexpr int32 MaximumFeedbackResolution = 64;
    constexpr float MinimumFeedbackRateHz = 0.1f;
    constexpr float MaximumFeedbackRateHz = 10.0f;

    float PixelLuminance(const FColor& Color)
    {
        // Capture uses 8-bit final color. Treat channels consistently as normalized
        // bytes; this metric is intentionally a lightweight observation, not HDR metering.
        return DTEAvatarBridge::Math::Clamp01(
            (0.2126f * static_cast<float>(Color.R)
            + 0.7152f * static_cast<float>(Color.G)
            + 0.0722f * static_cast<float>(Color.B)) / 255.0f);
    }
}

UDTEAvatarSceneFeedbackComponent::UDTEAvatarSceneFeedbackComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PostUpdateWork;

    // No implicit capture: enabling capture is an explicit game/Blueprint choice.
    bCaptureEveryFrame = false;
    bCaptureOnMovement = false;
    CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
}

void UDTEAvatarSceneFeedbackComponent::BeginPlay()
{
    Super::BeginPlay();
    check(IsInGameThread());

    ConfigureFeedbackCapture(FeedbackResolution, FeedbackCaptureRateHz);
    if (bFeedbackCaptureEnabled)
    {
        EnsureFeedbackTarget();
    }
}

void UDTEAvatarSceneFeedbackComponent::TickComponent(
    const float DeltaTime,
    const ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    check(IsInGameThread());

    if (!bFeedbackCaptureEnabled || !DTEAvatarBridge::Math::IsCaptureConfigurationWithinLimits(FeedbackResolution, FeedbackCaptureRateHz))
    {
        return;
    }

    const float SafeDeltaTime = DTEAvatarBridge::Math::ClampNonNegative(DeltaTime);
    const float IntervalSeconds = 1.0f / FeedbackCaptureRateHz;
    SecondsSinceLastCapture += SafeDeltaTime;
    if (SecondsSinceLastCapture < IntervalSeconds)
    {
        return;
    }

    // CaptureFeedbackNow resets this timer before capture work. This makes the
    // configured limit apply to manual requests too and avoids hitch bursts.
    CaptureFeedbackNow();
}

void UDTEAvatarSceneFeedbackComponent::SetFeedbackCaptureEnabled(const bool bEnabled)
{
    check(IsInGameThread());
    if (bFeedbackCaptureEnabled == bEnabled)
    {
        return;
    }
    bFeedbackCaptureEnabled = bEnabled;
    ResetTemporalState();

    if (bFeedbackCaptureEnabled)
    {
        EnsureFeedbackTarget();
    }
}

void UDTEAvatarSceneFeedbackComponent::ConfigureFeedbackCapture(const int32 RequestedResolution, const float RequestedRateHz)
{
    check(IsInGameThread());
    const int32 SafeResolution = FMath::Clamp(RequestedResolution, MinimumFeedbackResolution, MaximumFeedbackResolution);
    const float FiniteRate = DTEAvatarBridge::Math::FiniteOr(RequestedRateHz, 5.0f);
    const float SafeRate = FMath::Clamp(FiniteRate, MinimumFeedbackRateHz, MaximumFeedbackRateHz);
    const bool bResolutionChanged = FeedbackResolution != SafeResolution;

    FeedbackResolution = SafeResolution;
    FeedbackCaptureRateHz = SafeRate;
    // Preserve elapsed time: repeated configuration must not unlock extra captures.
    if (bResolutionChanged)
    {
        ResetTemporalState();
        if (OwnedFeedbackTarget)
        {
            EnsureFeedbackTarget();
        }
    }
}

void UDTEAvatarSceneFeedbackComponent::SetPredictedLuminance(const float InPredictedLuminance)
{
    check(IsInGameThread());
    if (!DTEAvatarBridge::Math::IsFinite(InPredictedLuminance))
    {
        ClearPredictedLuminance();
        return;
    }

    PredictedLuminance = DTEAvatarBridge::Math::Clamp01(InPredictedLuminance);
    bHasPrediction = true;
}

void UDTEAvatarSceneFeedbackComponent::ClearPredictedLuminance()
{
    check(IsInGameThread());
    PredictedLuminance = 0.0f;
    bHasPrediction = false;
}

bool UDTEAvatarSceneFeedbackComponent::CaptureFeedbackNow()
{
    check(IsInGameThread());
    if (!bFeedbackCaptureEnabled
        || !DTEAvatarBridge::Math::IsCaptureConfigurationWithinLimits(FeedbackResolution, FeedbackCaptureRateHz)
        || !EnsureFeedbackTarget())
    {
        return false;
    }

    const float IntervalSeconds = 1.0f / FeedbackCaptureRateHz;
    if (SecondsSinceLastCapture < IntervalSeconds)
    {
        return false;
    }

    // Reset before issuing work so failures cannot cause a per-frame retry loop.
    SecondsSinceLastCapture = 0.0f;

    // The public Kismet helper synchronously reads the tiny target after capture.
    // This is purposefully limited to explicit, low-rate feedback, never per frame.
    CaptureScene();
    TArray<FColor> Pixels;
    const int32 ExpectedPixelCount = FeedbackResolution * FeedbackResolution;
    Pixels.Reserve(ExpectedPixelCount);
    if (!UKismetRenderingLibrary::ReadRenderTarget(this, OwnedFeedbackTarget, Pixels) || Pixels.Num() != ExpectedPixelCount)
    {
        return false;
    }

    UpdateFeedbackFromPixels(Pixels);
    return true;
}

FDTEAvatarSceneFeedback UDTEAvatarSceneFeedbackComponent::GetCurrentFeedback() const
{
    return CurrentFeedback;
}

bool UDTEAvatarSceneFeedbackComponent::EnsureFeedbackTarget()
{
    check(IsInGameThread());
    if (!DTEAvatarBridge::Math::IsCaptureConfigurationWithinLimits(FeedbackResolution, FeedbackCaptureRateHz))
    {
        return false;
    }
    if (!OwnedFeedbackTarget)
    {
        OwnedFeedbackTarget = NewObject<UTextureRenderTarget2D>(this, TEXT("DTEAvatarFeedbackTarget"), RF_Transient);
    }
    if (!OwnedFeedbackTarget)
    {
        return false;
    }

    if (OwnedFeedbackTarget->SizeX != FeedbackResolution || OwnedFeedbackTarget->SizeY != FeedbackResolution)
    {
        OwnedFeedbackTarget->ClearColor = FLinearColor::Black;
        OwnedFeedbackTarget->RenderTargetFormat = RTF_RGBA8;
        OwnedFeedbackTarget->bAutoGenerateMips = false;
        OwnedFeedbackTarget->InitCustomFormat(FeedbackResolution, FeedbackResolution, PF_B8G8R8A8, false);
    }

    TextureTarget = OwnedFeedbackTarget;
    return true;
}

void UDTEAvatarSceneFeedbackComponent::ResetTemporalState()
{
    PreviousLuminance.Reset();
    bHasPreviousFrame = false;
    CurrentFeedback = FDTEAvatarSceneFeedback();
}

void UDTEAvatarSceneFeedbackComponent::UpdateFeedbackFromPixels(const TArray<FColor>& Pixels)
{
    check(IsInGameThread());
    const int32 ExpectedPixelCount = FeedbackResolution * FeedbackResolution;
    if (Pixels.Num() != ExpectedPixelCount || ExpectedPixelCount <= 0)
    {
        return;
    }

    TArray<float> CurrentLuminance;
    CurrentLuminance.SetNumUninitialized(ExpectedPixelCount);
    float TopLeftSum = 0.0f;
    float TopRightSum = 0.0f;
    float BottomLeftSum = 0.0f;
    float BottomRightSum = 0.0f;
    int32 TopLeftCount = 0;
    int32 TopRightCount = 0;
    int32 BottomLeftCount = 0;
    int32 BottomRightCount = 0;

    for (int32 Y = 0; Y < FeedbackResolution; ++Y)
    {
        for (int32 X = 0; X < FeedbackResolution; ++X)
        {
            const int32 Index = Y * FeedbackResolution + X;
            const float Luminance = PixelLuminance(Pixels[Index]);
            CurrentLuminance[Index] = Luminance;

            const bool bLeft = X < FeedbackResolution / 2;
            const bool bTop = Y < FeedbackResolution / 2;
            if (bTop && bLeft) { TopLeftSum += Luminance; ++TopLeftCount; }
            else if (bTop) { TopRightSum += Luminance; ++TopRightCount; }
            else if (bLeft) { BottomLeftSum += Luminance; ++BottomLeftCount; }
            else { BottomRightSum += Luminance; ++BottomRightCount; }
        }
    }

    const DTEAvatarBridge::Math::FrameComparison Comparison = DTEAvatarBridge::Math::CompareLuminanceFrames(
        CurrentLuminance.GetData(),
        bHasPreviousFrame ? PreviousLuminance.GetData() : nullptr,
        static_cast<std::size_t>(CurrentLuminance.Num()),
        bHasPreviousFrame,
        PredictedLuminance,
        bHasPrediction);

    CurrentFeedback.NormalizedLuminance = Comparison.NormalizedLuminance;
    CurrentFeedback.MotionEnergy = Comparison.MotionEnergy;
    CurrentFeedback.EfferencePredictionError = Comparison.PredictionError;
    CurrentFeedback.TopLeftMean = TopLeftCount > 0 ? TopLeftSum / static_cast<float>(TopLeftCount) : 0.0f;
    CurrentFeedback.TopRightMean = TopRightCount > 0 ? TopRightSum / static_cast<float>(TopRightCount) : 0.0f;
    CurrentFeedback.BottomLeftMean = BottomLeftCount > 0 ? BottomLeftSum / static_cast<float>(BottomLeftCount) : 0.0f;
    CurrentFeedback.BottomRightMean = BottomRightCount > 0 ? BottomRightSum / static_cast<float>(BottomRightCount) : 0.0f;
    CurrentFeedback.bHasPreviousFrame = bHasPreviousFrame;

    PreviousLuminance = MoveTemp(CurrentLuminance);
    bHasPreviousFrame = true;
    OnSceneFeedbackUpdated.Broadcast(CurrentFeedback);
}
