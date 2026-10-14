// Copyright Deep Tree Echo. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Components/SceneCaptureComponent2D.h"
#include "DTEAvatarSceneFeedbackComponent.generated.h"

class UTextureRenderTarget2D;

/** Per-capture visual summary produced from a bounded 8-bit feedback target. */
USTRUCT(BlueprintType)
struct DTEAVATARBRIDGE_API FDTEAvatarSceneFeedback
{
    GENERATED_BODY()

    /** Mean Rec. 709 luminance of the current 8-bit capture, normalized to [0,1]. */
    UPROPERTY(BlueprintReadOnly, Category = "DTE|Feedback")
    float NormalizedLuminance = 0.0f;

    /** Mean absolute per-pixel luminance difference against the previous capture. */
    UPROPERTY(BlueprintReadOnly, Category = "DTE|Feedback")
    float MotionEnergy = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "DTE|Feedback")
    float TopLeftMean = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "DTE|Feedback")
    float TopRightMean = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "DTE|Feedback")
    float BottomLeftMean = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "DTE|Feedback")
    float BottomRightMean = 0.0f;

    /** |observed mean luminance - externally supplied predicted luminance|. Zero on the first frame. */
    UPROPERTY(BlueprintReadOnly, Category = "DTE|Feedback")
    float EfferencePredictionError = 0.0f;

    /** False until one successful capture has been retained for temporal comparison. */
    UPROPERTY(BlueprintReadOnly, Category = "DTE|Feedback")
    bool bHasPreviousFrame = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDTEAvatarSceneFeedbackUpdated, const FDTEAvatarSceneFeedback&, Feedback);

/**
 * A SceneCapture2D that is inert unless explicitly enabled. It owns a transient
 * low-resolution target, samples it at a bounded rate, and performs synchronous
 * game-thread readback only at that rate. It is feedback/telemetry only.
 */
UCLASS(ClassGroup = (DTE), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class DTEAVATARBRIDGE_API UDTEAvatarSceneFeedbackComponent : public USceneCaptureComponent2D
{
    GENERATED_BODY()

public:
    UDTEAvatarSceneFeedbackComponent();

    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    /** Must be enabled explicitly. Default false prevents capture/readback cost. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DTE|Feedback")
    bool bFeedbackCaptureEnabled = false;

    /** Square target edge; runtime values are clamped to [8, 64]. Default: 32. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DTE|Feedback", meta = (ClampMin = "8", ClampMax = "64", UIMin = "8", UIMax = "64"))
    int32 FeedbackResolution = 32;

    /** Capture rate in Hz; runtime values are clamped to [0.1, 10]. Default: 5. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DTE|Feedback", meta = (ClampMin = "0.1", ClampMax = "10.0", UIMin = "0.1", UIMax = "10.0"))
    float FeedbackCaptureRateHz = 5.0f;

    UPROPERTY(BlueprintReadOnly, Category = "DTE|Feedback")
    FDTEAvatarSceneFeedback CurrentFeedback;

    UPROPERTY(BlueprintAssignable, Category = "DTE|Feedback")
    FDTEAvatarSceneFeedbackUpdated OnSceneFeedbackUpdated;

    /** Explicit runtime opt-in/out. Disabling resets temporal comparison state. */
    UFUNCTION(BlueprintCallable, Category = "DTE|Feedback")
    void SetFeedbackCaptureEnabled(bool bEnabled);

    /** Clamps requested values to the documented hard limits and reinitializes the transient target if needed. */
    UFUNCTION(BlueprintCallable, Category = "DTE|Feedback")
    void ConfigureFeedbackCapture(int32 RequestedResolution, float RequestedRateHz);

    /** Optional cognitive prediction of the next observed mean luminance. Non-finite values clear the prediction. */
    UFUNCTION(BlueprintCallable, Category = "DTE|Feedback")
    void SetPredictedLuminance(float InPredictedLuminance);

    UFUNCTION(BlueprintCallable, Category = "DTE|Feedback")
    void ClearPredictedLuminance();

    /**
     * Captures once only when feedback has been explicitly enabled and the
     * configured rate interval has elapsed. Returns false when disabled,
     * rate-limited, invalid, or on a readback failure.
     */
    UFUNCTION(BlueprintCallable, Category = "DTE|Feedback")
    bool CaptureFeedbackNow();

    UFUNCTION(BlueprintPure, Category = "DTE|Feedback")
    FDTEAvatarSceneFeedback GetCurrentFeedback() const;

private:
    UPROPERTY(Transient)
    TObjectPtr<UTextureRenderTarget2D> OwnedFeedbackTarget = nullptr;

    TArray<float> PreviousLuminance;
    float SecondsSinceLastCapture = 0.0f;
    float PredictedLuminance = 0.0f;
    bool bHasPrediction = false;
    bool bHasPreviousFrame = false;

    bool EnsureFeedbackTarget();
    void ResetTemporalState();
    void UpdateFeedbackFromPixels(const TArray<FColor>& Pixels);
};
