#include "Animation/CustomAnimNotifies.h"
#include "Kismet/GameplayStatics.h"
#include "Components/SkeletalMeshComponent.h"
#include "Audio/AudioManagerComponent.h"
#include "Avatar/AvatarMaterialManager.h"
#include "Avatar/Avatar3DComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "TimerManager.h"

// UAnimNotify_TriggerAudioCue

UAnimNotify_TriggerAudioCue::UAnimNotify_TriggerAudioCue()
{
    Volume = 1.0f;
    PitchMultiplier = 1.0f;
    bAttachToSocket = false;
}

void UAnimNotify_TriggerAudioCue::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation)
{
    if (!AudioCue || !MeshComp)
    {
        return;
    }

    AActor* Owner = MeshComp->GetOwner();
    if (!Owner)
    {
        return;
    }

    if (bAttachToSocket && AttachSocketName != NAME_None)
    {
        UAudioComponent* AudioComp = UGameplayStatics::SpawnSoundAttached(
            AudioCue,
            MeshComp,
            AttachSocketName,
            FVector::ZeroVector,
            EAttachLocation::SnapToTarget,
            false,
            Volume,
            PitchMultiplier
        );
    }
    else
    {
        UGameplayStatics::PlaySoundAtLocation(
            MeshComp->GetWorld(),
            AudioCue,
            Owner->GetActorLocation(),
            Volume,
            PitchMultiplier
        );
    }
}

// UAnimNotify_TriggerParticleEffect

UAnimNotify_TriggerParticleEffect::UAnimNotify_TriggerParticleEffect()
{
    LocationOffset = FVector::ZeroVector;
    RotationOffset = FRotator::ZeroRotator;
    Scale = FVector(1.0f, 1.0f, 1.0f);
    bAttachToSocket = true;
}

void UAnimNotify_TriggerParticleEffect::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation)
{
    if (!ParticleSystem || !MeshComp)
    {
        return;
    }

    if (bAttachToSocket && SocketName != NAME_None)
    {
        UNiagaraFunctionLibrary::SpawnSystemAttached(
            ParticleSystem,
            MeshComp,
            SocketName,
            LocationOffset,
            RotationOffset,
            Scale,
            EAttachLocation::KeepRelativeOffset,
            true
        );
    }
    else
    {
        FVector SpawnLocation = MeshComp->GetOwner()->GetActorLocation() + LocationOffset;
        FRotator SpawnRotation = MeshComp->GetOwner()->GetActorRotation() + RotationOffset;

        UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            MeshComp->GetWorld(),
            ParticleSystem,
            SpawnLocation,
            SpawnRotation,
            Scale
        );
    }
}

// UAnimNotify_TriggerMaterialEffect

UAnimNotify_TriggerMaterialEffect::UAnimNotify_TriggerMaterialEffect()
{
    ParameterValue = 1.0f;
    Duration = 1.0f;
    bFadeOut = true;
    MaterialSlot = EAvatarMaterialSlot::Skin;
}

void UAnimNotify_TriggerMaterialEffect::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation)
{
    if (!MeshComp || ParameterName == NAME_None)
    {
        return;
    }

    AActor* Owner = MeshComp->GetOwner();
    if (!Owner)
    {
        return;
    }

    UAvatarMaterialManager* MaterialManager = Owner->FindComponentByClass<UAvatarMaterialManager>();
    if (!MaterialManager)
    {
        return;
    }

    // Set the material parameter immediately
    MaterialManager->SetScalarParameter(MaterialSlot, ParameterName, ParameterValue);

    // If fade-out is enabled and duration is valid, schedule fade-out
    if (bFadeOut && Duration > 0.0f)
    {
        // Create a timer to handle fade-out over duration
        FTimerHandle FadeOutTimerHandle;
        FTimerDelegate FadeOutDelegate;
        
        // Capture necessary variables for the lambda
        const float FadeStepTime = 0.033f; // ~30 updates per second
        const float TotalSteps = Duration / FadeStepTime;
        const float StepDecrement = ParameterValue / TotalSteps;
        
        // Create a shared counter for the fade steps
        TSharedPtr<int32> CurrentStep = MakeShared<int32>(0);
        TSharedPtr<float> CurrentValue = MakeShared<float>(ParameterValue);
        
        FadeOutDelegate.BindLambda([MaterialManager, MaterialSlot, ParameterName, StepDecrement, TotalSteps, CurrentStep, CurrentValue]()
        {
            if (MaterialManager && MaterialManager->IsValidLowLevel())
            {
                (*CurrentStep)++;
                (*CurrentValue) = FMath::Max(0.0f, (*CurrentValue) - StepDecrement);
                
                MaterialManager->SetScalarParameter(MaterialSlot, ParameterName, *CurrentValue);
            }
        });
        
        // Set repeating timer for fade-out effect
        if (UWorld* World = Owner->GetWorld())
        {
            World->GetTimerManager().SetTimer(
                FadeOutTimerHandle,
                FadeOutDelegate,
                FadeStepTime,
                true, // Loop
                0.0f  // Initial delay
            );
            
            // Set a one-time timer to clear the repeating timer after duration
            FTimerHandle CleanupTimerHandle;
            FTimerDelegate CleanupDelegate;
            CleanupDelegate.BindLambda([World, FadeOutTimerHandle]() mutable
            {
                if (World && World->GetTimerManager().IsTimerActive(FadeOutTimerHandle))
                {
                    World->GetTimerManager().ClearTimer(FadeOutTimerHandle);
                }
            });
            
            World->GetTimerManager().SetTimer(
                CleanupTimerHandle,
                CleanupDelegate,
                Duration + 0.1f, // Slightly longer than duration to ensure cleanup
                false // One-shot
            );
        }
    }
}

// UAnimNotify_TriggerGestureSound

UAnimNotify_TriggerGestureSound::UAnimNotify_TriggerGestureSound()
{
}

void UAnimNotify_TriggerGestureSound::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation)
{
    if (!MeshComp || GestureName == NAME_None)
    {
        return;
    }

    AActor* Owner = MeshComp->GetOwner();
    if (!Owner)
    {
        return;
    }

    UAudioManagerComponent* AudioManager = Owner->FindComponentByClass<UAudioManagerComponent>();
    if (AudioManager)
    {
        AudioManager->PlayGestureSound(GestureName);
    }
}

// UAnimNotifyState_MaterialEffectOverTime

UAnimNotifyState_MaterialEffectOverTime::UAnimNotifyState_MaterialEffectOverTime()
{
    StartValue = 0.0f;
    EndValue = 1.0f;
    bFadeIn = true;
    bFadeOut = true;
    CurrentTime = 0.0f;
    NotifyDuration = 0.0f;
}

void UAnimNotifyState_MaterialEffectOverTime::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration)
{
    CurrentTime = 0.0f;
    NotifyDuration = TotalDuration;

    if (!MeshComp || ParameterName == NAME_None)
    {
        return;
    }

    AActor* Owner = MeshComp->GetOwner();
    if (!Owner)
    {
        return;
    }

    UAvatarMaterialManager* MaterialManager = Owner->FindComponentByClass<UAvatarMaterialManager>();
    if (MaterialManager)
    {
        MaterialManager->SetScalarParameter(EAvatarMaterialSlot::Skin, ParameterName, StartValue);
    }
}

void UAnimNotifyState_MaterialEffectOverTime::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime)
{
    CurrentTime += FrameDeltaTime;

    if (!MeshComp || ParameterName == NAME_None || NotifyDuration <= 0.0f)
    {
        return;
    }

    AActor* Owner = MeshComp->GetOwner();
    if (!Owner)
    {
        return;
    }

    UAvatarMaterialManager* MaterialManager = Owner->FindComponentByClass<UAvatarMaterialManager>();
    if (!MaterialManager)
    {
        return;
    }

    // Calculate interpolation alpha
    float Alpha = FMath::Clamp(CurrentTime / NotifyDuration, 0.0f, 1.0f);

    // Apply fade in/out curves
    if (bFadeIn && Alpha < 0.2f)
    {
        Alpha = Alpha / 0.2f;  // Fade in over first 20%
    }
    else if (bFadeOut && Alpha > 0.8f)
    {
        Alpha = 1.0f - ((Alpha - 0.8f) / 0.2f);  // Fade out over last 20%
    }
    else
    {
        Alpha = 1.0f;  // Full intensity in the middle
    }

    // Interpolate between start and end values
    float CurrentValue = FMath::Lerp(StartValue, EndValue, Alpha);

    MaterialManager->SetScalarParameter(EAvatarMaterialSlot::Skin, ParameterName, CurrentValue);
}

void UAnimNotifyState_MaterialEffectOverTime::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation)
{
    if (!MeshComp || ParameterName == NAME_None)
    {
        return;
    }

    AActor* Owner = MeshComp->GetOwner();
    if (!Owner)
    {
        return;
    }

    UAvatarMaterialManager* MaterialManager = Owner->FindComponentByClass<UAvatarMaterialManager>();
    if (MaterialManager)
    {
        MaterialManager->SetScalarParameter(EAvatarMaterialSlot::Skin, ParameterName, EndValue);
    }
}

// UAnimNotifyState_ParticleEffectOverTime

UAnimNotifyState_ParticleEffectOverTime::UAnimNotifyState_ParticleEffectOverTime()
{
    LocationOffset = FVector::ZeroVector;
    RotationOffset = FRotator::ZeroRotator;
    SpawnedParticleComponent = nullptr;
}

void UAnimNotifyState_ParticleEffectOverTime::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration)
{
    if (!ParticleSystem || !MeshComp)
    {
        return;
    }

    SpawnedParticleComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
        ParticleSystem,
        MeshComp,
        SocketName,
        LocationOffset,
        RotationOffset,
        FVector(1.0f, 1.0f, 1.0f),
        EAttachLocation::KeepRelativeOffset,
        true
    );
}

void UAnimNotifyState_ParticleEffectOverTime::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime)
{
    // Particle system continues to play
    // Could add dynamic parameter updates here if needed
}

void UAnimNotifyState_ParticleEffectOverTime::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation)
{
    if (SpawnedParticleComponent)
    {
        SpawnedParticleComponent->Deactivate();
        SpawnedParticleComponent = nullptr;
    }
}

// UAnimNotify_TriggerEchoResonance

UAnimNotify_TriggerEchoResonance::UAnimNotify_TriggerEchoResonance()
{
    Intensity = 0.5f;
}

void UAnimNotify_TriggerEchoResonance::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation)
{
    if (!MeshComp)
    {
        return;
    }

    AActor* Owner = MeshComp->GetOwner();
    if (!Owner)
    {
        return;
    }

    // Trigger audio
    UAudioManagerComponent* AudioManager = Owner->FindComponentByClass<UAudioManagerComponent>();
    if (AudioManager)
    {
        AudioManager->PlayEchoResonanceSound(Intensity);
    }

    // Trigger visual effect
    UAvatar3DComponent* Avatar3D = Owner->FindComponentByClass<UAvatar3DComponent>();
    if (Avatar3D)
    {
        Avatar3D->TriggerEchoResonance(Intensity);
    }
}

// UAnimNotify_TriggerCognitiveLoad

UAnimNotify_TriggerCognitiveLoad::UAnimNotify_TriggerCognitiveLoad()
{
    LoadIntensity = 0.5f;
}

void UAnimNotify_TriggerCognitiveLoad::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation)
{
    if (!MeshComp)
    {
        return;
    }

    AActor* Owner = MeshComp->GetOwner();
    if (!Owner)
    {
        return;
    }

    // Trigger audio
    UAudioManagerComponent* AudioManager = Owner->FindComponentByClass<UAudioManagerComponent>();
    if (AudioManager)
    {
        AudioManager->PlayCognitiveSound(LoadIntensity);
    }

    // Trigger visual effect
    UAvatar3DComponent* Avatar3D = Owner->FindComponentByClass<UAvatar3DComponent>();
    if (Avatar3D)
    {
        Avatar3D->UpdateCognitiveLoadVisualization(LoadIntensity);
    }
}
