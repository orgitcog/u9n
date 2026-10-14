// Copyright Deep Tree Echo. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "DTEAvatarTelemetryComponent.h"
#include "DTEAvatarAnimInstance.generated.h"

/**
 * Animation-Blueprint parent class that snapshots bridge telemetry during the
 * game-thread NativeUpdateAnimation phase. It deliberately does not override a
 * thread-safe animation update function and never reads UObjects on a worker.
 */
UCLASS(Transient, Blueprintable, BlueprintType)
class DTEAVATARBRIDGE_API UDTEAvatarAnimInstance : public UAnimInstance
{
    GENERATED_BODY()

public:
    virtual void NativeInitializeAnimation() override;
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;

    /** Optional explicit source. If unset, the owning actor is queried once for this component type. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DTE|Telemetry")
    TObjectPtr<UDTEAvatarTelemetryComponent> TelemetrySource = nullptr;

    /** Safe value snapshot for an AnimGraph / Blueprint graph to consume. */
    UPROPERTY(BlueprintReadOnly, Category = "DTE|Telemetry")
    FDTEAvatarTelemetry Telemetry;

    UFUNCTION(BlueprintCallable, Category = "DTE|Telemetry")
    void SetTelemetrySource(UDTEAvatarTelemetryComponent* InTelemetrySource);

protected:
    void ResolveTelemetrySource();
};
