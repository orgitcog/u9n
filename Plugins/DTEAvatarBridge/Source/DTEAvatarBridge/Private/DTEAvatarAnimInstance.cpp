// Copyright Deep Tree Echo. All Rights Reserved.
#include "DTEAvatarAnimInstance.h"

#include "GameFramework/Actor.h"

void UDTEAvatarAnimInstance::NativeInitializeAnimation()
{
    Super::NativeInitializeAnimation();
    check(IsInGameThread());
    ResolveTelemetrySource();
}

void UDTEAvatarAnimInstance::NativeUpdateAnimation(const float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);
    check(IsInGameThread());

    ResolveTelemetrySource();
    if (IsValid(TelemetrySource))
    {
        Telemetry = TelemetrySource->GetCurrentTelemetry();
    }
}

void UDTEAvatarAnimInstance::SetTelemetrySource(UDTEAvatarTelemetryComponent* InTelemetrySource)
{
    check(IsInGameThread());
    TelemetrySource = InTelemetrySource;
}

void UDTEAvatarAnimInstance::ResolveTelemetrySource()
{
    check(IsInGameThread());
    if (IsValid(TelemetrySource))
    {
        return;
    }

    if (AActor* Owner = GetOwningActor())
    {
        TelemetrySource = Owner->FindComponentByClass<UDTEAvatarTelemetryComponent>();
    }
}
