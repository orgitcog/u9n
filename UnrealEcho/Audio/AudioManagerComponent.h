#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Sound/SoundBase.h"
#include "AudioManagerComponent.generated.h"

// Forward declarations
class UAudioComponent;
class UAvatarAnimInstance;

UENUM(BlueprintType)
enum class EAvatarEmotionalState : uint8
{
    Neutral UMETA(DisplayName = "Neutral"),
    Happy UMETA(DisplayName = "Happy"),
    Sad UMETA(DisplayName = "Sad"),
    Excited UMETA(DisplayName = "Excited"),
    Angry UMETA(DisplayName = "Angry"),
    Flirty UMETA(DisplayName = "Flirty"),
    Surprised UMETA(DisplayName = "Surprised")
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UNREALENGINE_API UAudioManagerComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UAudioManagerComponent();

    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // Voice System
    UFUNCTION(BlueprintCallable, Category = "Audio|Voice")
    void Speak(const FString& Text, float EmotionalIntensity = 0.5f);

    UFUNCTION(BlueprintCallable, Category = "Audio|Voice")
    void SetVoicePitch(float Pitch);

    UFUNCTION(BlueprintCallable, Category = "Audio|Voice")
    void SetVoiceSpeed(float Speed);

    // Sound Effects
    UFUNCTION(BlueprintCallable, Category = "Audio|SFX")
    void PlayGestureSound(FName GestureName);

    UFUNCTION(BlueprintCallable, Category = "Audio|SFX")
    void PlayEmotionalSound(EAvatarEmotionalState Emotion, float Intensity);

    UFUNCTION(BlueprintCallable, Category = "Audio|SFX")
    void PlayCognitiveSound(float CognitiveLoad);

    UFUNCTION(BlueprintCallable, Category = "Audio|SFX")
    void PlayEchoResonanceSound(float Intensity);

    // Music System
    UFUNCTION(BlueprintCallable, Category = "Audio|Music")
    void UpdateMusicLayers(EAvatarEmotionalState Emotion, float Intensity);

    UFUNCTION(BlueprintCallable, Category = "Audio|Music")
    void SetMusicIntensity(float Intensity);

    // Ambient System
    UFUNCTION(BlueprintCallable, Category = "Audio|Ambient")
    void UpdateAmbientSound(float CognitiveLoad, float ChaosFactor);

protected:
    // Voice System Properties
    UPROPERTY(EditAnywhere, Category = "Audio|Voice")
    USoundBase* VoiceBase;

    UPROPERTY()
    UAudioComponent* VoiceAudioComponent;

    UPROPERTY(EditAnywhere, Category = "Audio|Voice", meta = (ClampMin = "0.5", ClampMax = "2.0"))
    float BasePitch;

    UPROPERTY(EditAnywhere, Category = "Audio|Voice", meta = (ClampMin = "0.5", ClampMax = "2.0"))
    float BaseSpeed;

    // Sound Effect Libraries
    UPROPERTY(EditAnywhere, Category = "Audio|SFX")
    TMap<FName, USoundBase*> GestureSounds;

    UPROPERTY(EditAnywhere, Category = "Audio|SFX")
    TMap<EAvatarEmotionalState, USoundBase*> EmotionalSounds;

    UPROPERTY(EditAnywhere, Category = "Audio|SFX")
    USoundBase* CognitiveProcessingSound;

    UPROPERTY(EditAnywhere, Category = "Audio|SFX")
    USoundBase* EchoResonanceSound;

    // Music System Properties
    UPROPERTY(EditAnywhere, Category = "Audio|Music")
    TMap<EAvatarEmotionalState, USoundBase*> MusicThemes;

    UPROPERTY()
    TArray<UAudioComponent*> MusicLayerComponents;

    UPROPERTY(EditAnywhere, Category = "Audio|Music", meta = (ClampMin = "0.1", ClampMax = "5.0"))
    float MusicFadeSpeed;

    // Ambient System Properties
    UPROPERTY(EditAnywhere, Category = "Audio|Ambient")
    USoundBase* CognitiveAmbience;

    UPROPERTY(EditAnywhere, Category = "Audio|Ambient")
    USoundBase* ChaosAmbience;

    UPROPERTY()
    UAudioComponent* AmbienceComponent;

private:
    void InitializeAudioComponents();
    void UpdateVoiceModulation(float DeltaTime);
    void UpdateMusicMixing(float DeltaTime);
    void UpdateAmbientMixing(float DeltaTime);

    // Current state
    EAvatarEmotionalState CurrentEmotion;
    float CurrentEmotionalIntensity;
    float CurrentCognitiveLoad;
    float CurrentChaosFactor;
};
