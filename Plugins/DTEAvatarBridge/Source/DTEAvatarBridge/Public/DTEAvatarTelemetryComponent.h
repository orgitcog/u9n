// Copyright Deep Tree Echo. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DTEAvatarTelemetryComponent.generated.h"

/** Suggested presentation state only; this bridge never drives character movement. */
UENUM(BlueprintType)
enum class EDTEAvatarGait : uint8
{
    Idle UMETA(DisplayName = "Idle"),
    Explore UMETA(DisplayName = "Explore"),
    Cautious UMETA(DisplayName = "Cautious"),
    Stealth UMETA(DisplayName = "Stealth"),
    Combat UMETA(DisplayName = "Combat")
};

/** Sanitized, bounded state intended for animation graphs, materials, and other presentation consumers. */
USTRUCT(BlueprintType)
struct DTEAVATARBRIDGE_API FDTEAvatarTelemetry
{
    GENERATED_BODY()

    /** World-speed input after finite/non-negative validation and the configured ceiling. */
    UPROPERTY(BlueprintReadOnly, Category = "DTE|Telemetry")
    float Speed = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "DTE|Telemetry", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float NormalizedSpeed = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "DTE|Telemetry", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float NormalizedThreat = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "DTE|Telemetry", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float NormalizedNovelty = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "DTE|Telemetry", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float NormalizedReward = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "DTE|Telemetry")
    EDTEAvatarGait SuggestedGait = EDTEAvatarGait::Idle;

    UPROPERTY(BlueprintReadOnly, Category = "DTE|Expression", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float Wonder = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "DTE|Expression", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float Awe = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "DTE|Expression", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float Alert = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "DTE|Wings", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float WingSpread = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "DTE|Materials", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float EmissiveStrength = 0.0f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDTEAvatarTelemetryUpdated, const FDTEAvatarTelemetry&, Telemetry);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDTEAvatarGaitChanged, EDTEAvatarGait, PreviousGait, EDTEAvatarGait, NewGait);

/**
 * Narrow, dependency-free seam between cognition and avatar presentation.
 * Existing u9n systems may call SetCognitiveTelemetry on the game thread and/or
 * bind the delegates. This module does not include or require cognitive headers.
 */
UCLASS(ClassGroup = (DTE), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class DTEAVATARBRIDGE_API UDTEAvatarTelemetryComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UDTEAvatarTelemetryComponent();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    /** Input speed at or above this value maps to 1.0. Values are never allowed above the ceiling. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DTE|Telemetry", meta = (ClampMin = "1.0", UIMin = "1.0"))
    float MaximumSpeed = 600.0f;

    /** Exponential-smoothing time constant. 0 applies a new target on the next tick. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DTE|Telemetry", meta = (ClampMin = "0.0", UIMin = "0.0"))
    float SmoothingTimeConstant = 0.12f;

    UPROPERTY(BlueprintReadOnly, Category = "DTE|Telemetry")
    FDTEAvatarTelemetry CurrentTelemetry;

    UPROPERTY(BlueprintAssignable, Category = "DTE|Telemetry")
    FDTEAvatarTelemetryUpdated OnTelemetryUpdated;

    UPROPERTY(BlueprintAssignable, Category = "DTE|Telemetry")
    FDTEAvatarGaitChanged OnSuggestedGaitChanged;

    /**
     * The sole cognition input seam. Non-finite values are replaced with zero;
     * normalized values are clamped to [0, 1]. This does not cause movement.
     */
    UFUNCTION(BlueprintCallable, Category = "DTE|Telemetry")
    void SetCognitiveTelemetry(float Speed, float NormalizedThreat, float NormalizedNovelty, float NormalizedReward);

    UFUNCTION(BlueprintPure, Category = "DTE|Telemetry")
    FDTEAvatarTelemetry GetCurrentTelemetry() const;

    /** Clear targets and output a deterministic neutral state. */
    UFUNCTION(BlueprintCallable, Category = "DTE|Telemetry")
    void ResetTelemetry();

private:
    FDTEAvatarTelemetry TargetTelemetry;
    bool bHasReceivedTelemetry = false;

    void UpdateSmoothedTelemetry(float DeltaTime);
    void PublishIfChanged(const FDTEAvatarTelemetry& PreviousTelemetry);
};
