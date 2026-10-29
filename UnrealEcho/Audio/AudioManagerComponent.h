#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Sound/SoundBase.h"
#include "AudioManagerComponent.generated.h"

// Forward declarations
class UAudioComponent;

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

/** A Speak() request waiting for synthesized audio. */
USTRUCT(BlueprintType)
struct FPendingSpeechRequest
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Audio|Voice")
    FString Text;

    UPROPERTY(BlueprintReadOnly, Category = "Audio|Voice")
    float EmotionalIntensity = 0.5f;

    UPROPERTY(BlueprintReadOnly, Category = "Audio|Voice")
    FDateTime RequestedAt;
};

/**
 * Fired by Speak(). The bound synthesizer turns Text into audio and delivers it
 * with OnSpeechAudioReady(RequestId, Sound), or reports OnSpeechSynthesisFailed.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSynthesizeSpeechRequested, const FString&, Text, const FString&, RequestId, float, EmotionalIntensity);

/** Fired when synthesized speech starts playing (drives lip sync, captions, gaze). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSpeechPlaybackStarted, const FString&, RequestId, const FString&, Text, float, Duration);

/**
 * Audio for the avatar: voice, gesture/emotion sound effects, music layers and ambience.
 *
 * Voice: the character's speech is never a pre-recorded or fixed clip. Speak(Text)
 * only requests synthesis of exactly that text through OnSynthesizeSpeechRequested;
 * the resulting audio is played when it is handed back via OnSpeechAudioReady. If
 * no synthesizer is bound or synthesis fails, nothing is played for that line and a
 * warning is logged. Non-speech sounds (gestures, emotion cues, music, ambience) are
 * ordinary assets and are unaffected.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UNREALENGINE_API UAudioManagerComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UAudioManagerComponent();

    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // Voice System

    /**
     * Request that Text be spoken. Broadcasts OnSynthesizeSpeechRequested with a new
     * RequestId and returns it; audio plays only once OnSpeechAudioReady delivers it.
     * Returns an empty string (and speaks nothing) when Text is empty or no synthesizer is bound.
     */
    UFUNCTION(BlueprintCallable, Category = "Audio|Voice")
    FString Speak(const FString& Text, float EmotionalIntensity = 0.5f);

    /** Called by the bound synthesizer with the audio for RequestId. Plays it as the character's voice. */
    UFUNCTION(BlueprintCallable, Category = "Audio|Voice")
    void OnSpeechAudioReady(const FString& RequestId, USoundBase* SpeechAudio);

    /** Called by the bound synthesizer when it could not produce audio. Nothing is played. */
    UFUNCTION(BlueprintCallable, Category = "Audio|Voice")
    void OnSpeechSynthesisFailed(const FString& RequestId, const FString& Reason);

    /** True when something is bound to OnSynthesizeSpeechRequested. */
    UFUNCTION(BlueprintPure, Category = "Audio|Voice")
    bool IsSpeechSynthesizerBound() const { return OnSynthesizeSpeechRequested.IsBound(); }

    /** Stop the voice that is currently playing, if any. */
    UFUNCTION(BlueprintCallable, Category = "Audio|Voice")
    void StopSpeaking();

    UFUNCTION(BlueprintPure, Category = "Audio|Voice")
    bool IsSpeaking() const;

    /** Number of Speak() requests still waiting for audio. */
    UFUNCTION(BlueprintPure, Category = "Audio|Voice")
    int32 GetPendingSpeechRequestCount() const { return PendingSpeechRequests.Num(); }

    /** The owner (or a TTS component) binds here and synthesizes the given text. */
    UPROPERTY(BlueprintAssignable, Category = "Audio|Voice")
    FOnSynthesizeSpeechRequested OnSynthesizeSpeechRequested;

    UPROPERTY(BlueprintAssignable, Category = "Audio|Voice")
    FOnSpeechPlaybackStarted OnSpeechPlaybackStarted;

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

    /** Plays synthesized speech delivered through OnSpeechAudioReady. Has no sound of its own. */
    UPROPERTY()
    UAudioComponent* VoiceAudioComponent;

    /** Speak() requests awaiting synthesized audio, keyed by RequestId. */
    UPROPERTY(VisibleAnywhere, Category = "Audio|Voice")
    TMap<FString, FPendingSpeechRequest> PendingSpeechRequests;

    /** Requests older than this are dropped (with a warning) when a new one is made. */
    UPROPERTY(EditAnywhere, Category = "Audio|Voice", meta = (ClampMin = "1.0"))
    float SpeechRequestTimeoutSeconds;

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
    void DropExpiredSpeechRequests();
    void UpdateVoiceModulation(float DeltaTime);
    void UpdateMusicMixing(float DeltaTime);
    void UpdateAmbientMixing(float DeltaTime);

    // Current state
    EAvatarEmotionalState CurrentEmotion;
    float CurrentEmotionalIntensity;
    float CurrentCognitiveLoad;
    float CurrentChaosFactor;
};
