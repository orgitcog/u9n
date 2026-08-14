#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Sound/SoundBase.h"
#include "NiagaraSystem.h"

// Forward declarations
enum class EAvatarMaterialSlot : uint8;

#include "CustomAnimNotifies.generated.h"

/**
 * Animation Notify: Trigger Audio Cue
 * Plays a sound at a specific point in an animation
 */
UCLASS()
class UNREALENGINE_API UAnimNotify_TriggerAudioCue : public UAnimNotify
{
    GENERATED_BODY()

public:
    UAnimNotify_TriggerAudioCue();

    virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
    USoundBase* AudioCue;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float Volume;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio", meta = (ClampMin = "0.5", ClampMax = "2.0"))
    float PitchMultiplier;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
    FName AttachSocketName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
    bool bAttachToSocket;
};

/**
 * Animation Notify: Trigger Particle Effect
 * Spawns a particle system at a specific point in an animation
 */
UCLASS()
class UNREALENGINE_API UAnimNotify_TriggerParticleEffect : public UAnimNotify
{
    GENERATED_BODY()

public:
    UAnimNotify_TriggerParticleEffect();

    virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Particle")
    UNiagaraSystem* ParticleSystem;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Particle")
    FName SocketName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Particle")
    FVector LocationOffset;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Particle")
    FRotator RotationOffset;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Particle")
    FVector Scale;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Particle")
    bool bAttachToSocket;
};

/**
 * Animation Notify: Trigger Material Effect
 * Triggers a material parameter change (e.g., blush, sparkle)
 * If bFadeOut is true, the parameter will fade from ParameterValue to 0 over Duration
 */
UCLASS()
class UNREALENGINE_API UAnimNotify_TriggerMaterialEffect : public UAnimNotify
{
    GENERATED_BODY()

public:
    UAnimNotify_TriggerMaterialEffect();

    virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
    FName ParameterName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
    float ParameterValue;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material", meta = (ClampMin = "0.0"))
    float Duration;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
    bool bFadeOut;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
    EAvatarMaterialSlot MaterialSlot;
};

/**
 * Animation Notify: Trigger Gesture Sound
 * Plays a gesture-specific sound using the AudioManagerComponent
 */
UCLASS()
class UNREALENGINE_API UAnimNotify_TriggerGestureSound : public UAnimNotify
{
    GENERATED_BODY()

public:
    UAnimNotify_TriggerGestureSound();

    virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gesture")
    FName GestureName;
};

/**
 * Animation Notify State: Material Effect Over Time
 * Applies a material effect over a duration (e.g., sustained blush during emote)
 */
UCLASS()
class UNREALENGINE_API UAnimNotifyState_MaterialEffectOverTime : public UAnimNotifyState
{
    GENERATED_BODY()

public:
    UAnimNotifyState_MaterialEffectOverTime();

    virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration) override;
    virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime) override;
    virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
    FName ParameterName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
    float StartValue;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
    float EndValue;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
    bool bFadeIn;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
    bool bFadeOut;

private:
    float CurrentTime;
    float NotifyDuration;
};

/**
 * Animation Notify State: Particle Effect Over Time
 * Spawns and maintains a particle effect for the duration of the notify state
 */
UCLASS()
class UNREALENGINE_API UAnimNotifyState_ParticleEffectOverTime : public UAnimNotifyState
{
    GENERATED_BODY()

public:
    UAnimNotifyState_ParticleEffectOverTime();

    virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration) override;
    virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime) override;
    virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Particle")
    UNiagaraSystem* ParticleSystem;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Particle")
    FName SocketName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Particle")
    FVector LocationOffset;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Particle")
    FRotator RotationOffset;

private:
    UPROPERTY()
    class UNiagaraComponent* SpawnedParticleComponent;
};

/**
 * Animation Notify: Trigger Echo Resonance
 * Triggers the echo resonance effect (visual + audio)
 */
UCLASS()
class UNREALENGINE_API UAnimNotify_TriggerEchoResonance : public UAnimNotify
{
    GENERATED_BODY()

public:
    UAnimNotify_TriggerEchoResonance();

    virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Echo", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float Intensity;
};

/**
 * Animation Notify: Trigger Cognitive Load Effect
 * Triggers cognitive load visualization
 */
UCLASS()
class UNREALENGINE_API UAnimNotify_TriggerCognitiveLoad : public UAnimNotify
{
    GENERATED_BODY()

public:
    UAnimNotify_TriggerCognitiveLoad();

    virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cognitive", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float LoadIntensity;
};
