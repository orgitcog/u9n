#include "Audio/AudioManagerComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "Personality/PersonalityTraitSystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogAvatarAudio, Log, All);

UAudioManagerComponent::UAudioManagerComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    
    // Initialize default values
    VoiceAudioComponent = nullptr;
    AmbienceComponent = nullptr;
    BasePitch = 1.0f;
    BaseSpeed = 1.0f;
    MusicFadeSpeed = 1.0f;
    SpeechRequestTimeoutSeconds = 60.0f;
    
    CurrentEmotion = EAvatarEmotionalState::Neutral;
    CurrentEmotionalIntensity = 0.5f;
    CurrentCognitiveLoad = 0.0f;
    CurrentChaosFactor = 0.0f;
}

void UAudioManagerComponent::BeginPlay()
{
    Super::BeginPlay();
    
    InitializeAudioComponents();
}

void UAudioManagerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    
    UpdateVoiceModulation(DeltaTime);
    UpdateMusicMixing(DeltaTime);
    UpdateAmbientMixing(DeltaTime);
}

void UAudioManagerComponent::InitializeAudioComponents()
{
    // Create voice audio component
    VoiceAudioComponent = NewObject<UAudioComponent>(this, TEXT("VoiceAudioComponent"));
    if (VoiceAudioComponent)
    {
        VoiceAudioComponent->RegisterComponent();
        VoiceAudioComponent->AttachToComponent(GetOwner()->GetRootComponent(), 
                                               FAttachmentTransformRules::KeepRelativeTransform);
        VoiceAudioComponent->bAutoActivate = false;
        // No sound is assigned here: the voice component only ever plays audio
        // synthesized from the text the character actually says.
    }
    
    // Create music layer components (4 layers)
    for (int32 i = 0; i < 4; i++)
    {
        FString ComponentName = FString::Printf(TEXT("MusicLayer_%d"), i);
        UAudioComponent* MusicLayer = NewObject<UAudioComponent>(this, FName(*ComponentName));
        
        if (MusicLayer)
        {
            MusicLayer->RegisterComponent();
            MusicLayer->AttachToComponent(GetOwner()->GetRootComponent(), 
                                         FAttachmentTransformRules::KeepRelativeTransform);
            MusicLayer->bAutoActivate = false;
            MusicLayerComponents.Add(MusicLayer);
        }
    }
    
    // Create ambient audio component
    AmbienceComponent = NewObject<UAudioComponent>(this, TEXT("AmbienceComponent"));
    if (AmbienceComponent)
    {
        AmbienceComponent->RegisterComponent();
        AmbienceComponent->AttachToComponent(GetOwner()->GetRootComponent(), 
                                            FAttachmentTransformRules::KeepRelativeTransform);
        AmbienceComponent->bAutoActivate = false;
        
        if (CognitiveAmbience)
        {
            AmbienceComponent->SetSound(CognitiveAmbience);
            AmbienceComponent->Play();
        }
    }
}

// Voice System Implementation

FString UAudioManagerComponent::Speak(const FString& Text, float EmotionalIntensity)
{
    const FString Trimmed = Text.TrimStartAndEnd();
    if (Trimmed.IsEmpty())
    {
        return FString();
    }

    if (!VoiceAudioComponent)
    {
        UE_LOG(LogAvatarAudio, Warning, TEXT("Speak: no voice audio component on %s; nothing spoken"), *GetNameSafe(GetOwner()));
        return FString();
    }

    if (!OnSynthesizeSpeechRequested.IsBound())
    {
        // Honest failure: with no synthesizer there is no voice for this text.
        UE_LOG(LogAvatarAudio, Warning, TEXT("Speak: no speech synthesizer bound to OnSynthesizeSpeechRequested on %s; nothing spoken for \"%s\""),
            *GetNameSafe(GetOwner()), *Trimmed.Left(80));
        return FString();
    }

    DropExpiredSpeechRequests();

    FPendingSpeechRequest Request;
    Request.Text = Trimmed;
    Request.EmotionalIntensity = FMath::Clamp(EmotionalIntensity, 0.0f, 1.0f);
    Request.RequestedAt = FDateTime::UtcNow();

    const FString RequestId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
    PendingSpeechRequests.Add(RequestId, Request);

    OnSynthesizeSpeechRequested.Broadcast(Request.Text, RequestId, Request.EmotionalIntensity);
    return RequestId;
}

void UAudioManagerComponent::OnSpeechAudioReady(const FString& RequestId, USoundBase* SpeechAudio)
{
    FPendingSpeechRequest Request;
    if (!PendingSpeechRequests.RemoveAndCopyValue(RequestId, Request))
    {
        UE_LOG(LogAvatarAudio, Warning, TEXT("OnSpeechAudioReady: unknown or expired speech request %s; audio discarded"), *RequestId);
        return;
    }

    if (!SpeechAudio)
    {
        UE_LOG(LogAvatarAudio, Warning, TEXT("OnSpeechAudioReady: synthesizer delivered no audio for request %s; nothing spoken"), *RequestId);
        return;
    }

    if (!VoiceAudioComponent)
    {
        UE_LOG(LogAvatarAudio, Warning, TEXT("OnSpeechAudioReady: no voice audio component; nothing spoken for request %s"), *RequestId);
        return;
    }

    // Modulate pitch based on emotional intensity
    const float ModulatedPitch = BasePitch * FMath::Lerp(0.9f, 1.1f, Request.EmotionalIntensity);
    VoiceAudioComponent->SetPitchMultiplier(ModulatedPitch);

    // Speed modulation would require time stretching of the synthesized audio and is not applied here.

    VoiceAudioComponent->SetSound(SpeechAudio);
    VoiceAudioComponent->Play();

    float Duration = SpeechAudio->GetDuration();
    if (Duration <= 0.0f || Duration >= INDEFINITELY_LOOPING_DURATION)
    {
        Duration = Request.Text.Len() * 0.05f; // Estimate only when the asset reports no finite length
    }

    OnSpeechPlaybackStarted.Broadcast(RequestId, Request.Text, Duration);
}

void UAudioManagerComponent::OnSpeechSynthesisFailed(const FString& RequestId, const FString& Reason)
{
    FPendingSpeechRequest Request;
    if (PendingSpeechRequests.RemoveAndCopyValue(RequestId, Request))
    {
        UE_LOG(LogAvatarAudio, Warning, TEXT("Speech synthesis failed for request %s (\"%s\"): %s. Nothing spoken."),
            *RequestId, *Request.Text.Left(80), *Reason);
    }
    else
    {
        UE_LOG(LogAvatarAudio, Warning, TEXT("Speech synthesis failed for unknown request %s: %s"), *RequestId, *Reason);
    }
}

void UAudioManagerComponent::StopSpeaking()
{
    if (VoiceAudioComponent && VoiceAudioComponent->IsPlaying())
    {
        VoiceAudioComponent->Stop();
    }
}

bool UAudioManagerComponent::IsSpeaking() const
{
    return VoiceAudioComponent && VoiceAudioComponent->IsPlaying();
}

void UAudioManagerComponent::DropExpiredSpeechRequests()
{
    const FDateTime Now = FDateTime::UtcNow();
    for (auto It = PendingSpeechRequests.CreateIterator(); It; ++It)
    {
        const double Age = (Now - It.Value().RequestedAt).GetTotalSeconds();
        if (Age > SpeechRequestTimeoutSeconds)
        {
            UE_LOG(LogAvatarAudio, Warning, TEXT("Speech request %s received no audio within %.0fs; dropped, nothing spoken for \"%s\""),
                *It.Key(), SpeechRequestTimeoutSeconds, *It.Value().Text.Left(80));
            It.RemoveCurrent();
        }
    }
}

void UAudioManagerComponent::SetVoicePitch(float Pitch)
{
    BasePitch = FMath::Clamp(Pitch, 0.5f, 2.0f);
    
    if (VoiceAudioComponent)
    {
        VoiceAudioComponent->SetPitchMultiplier(BasePitch);
    }
}

void UAudioManagerComponent::SetVoiceSpeed(float Speed)
{
    BaseSpeed = FMath::Clamp(Speed, 0.5f, 2.0f);
}

void UAudioManagerComponent::UpdateVoiceModulation(float DeltaTime)
{
    if (!VoiceAudioComponent || !VoiceAudioComponent->IsPlaying())
    {
        return;
    }
    
    // Get personality system for confidence
    UPersonalityTraitSystem* PersonalitySystem = GetOwner()->FindComponentByClass<UPersonalityTraitSystem>();
    float Confidence = PersonalitySystem ? PersonalitySystem->GetConfidence() : 0.7f;
    
    // Modulate pitch based on current emotion
    float TargetPitch = BasePitch;
    switch (CurrentEmotion)
    {
        case EAvatarEmotionalState::Happy:
            TargetPitch = BasePitch * 1.1f;
            break;
        case EAvatarEmotionalState::Sad:
            TargetPitch = BasePitch * 0.9f;
            break;
        case EAvatarEmotionalState::Excited:
            TargetPitch = BasePitch * 1.15f;
            break;
        case EAvatarEmotionalState::Angry:
            TargetPitch = BasePitch * 0.95f;
            break;
        default:
            TargetPitch = BasePitch;
    }
    
    // Smooth interpolation
    float CurrentPitch = VoiceAudioComponent->GetPitchMultiplier();
    float NewPitch = FMath::FInterpTo(CurrentPitch, TargetPitch, DeltaTime, 2.0f);
    VoiceAudioComponent->SetPitchMultiplier(NewPitch);
    
    // Modulate volume based on confidence
    float TargetVolume = FMath::Lerp(0.7f, 1.0f, Confidence);
    float CurrentVolume = VoiceAudioComponent->GetVolumeMultiplier();
    float NewVolume = FMath::FInterpTo(CurrentVolume, TargetVolume, DeltaTime, 1.0f);
    VoiceAudioComponent->SetVolumeMultiplier(NewVolume);
}

// Sound Effects System Implementation

void UAudioManagerComponent::PlayGestureSound(FName GestureName)
{
    USoundBase** Sound = GestureSounds.Find(GestureName);
    if (Sound && *Sound)
    {
        UGameplayStatics::PlaySoundAtLocation(GetWorld(), *Sound, GetOwner()->GetActorLocation());
    }
}

void UAudioManagerComponent::PlayEmotionalSound(EAvatarEmotionalState Emotion, float Intensity)
{
    USoundBase** Sound = EmotionalSounds.Find(Emotion);
    if (Sound && *Sound)
    {
        UAudioComponent* EmotionAudio = UGameplayStatics::SpawnSoundAttached(
            *Sound,
            GetOwner()->GetRootComponent(),
            NAME_None,
            FVector::ZeroVector,
            EAttachLocation::KeepRelativeOffset,
            false
        );
        
        if (EmotionAudio)
        {
            EmotionAudio->SetVolumeMultiplier(Intensity * 0.5f);  // Subtle
            EmotionAudio->Play();
        }
    }
}

void UAudioManagerComponent::PlayCognitiveSound(float CognitiveLoad)
{
    if (CognitiveProcessingSound)
    {
        UAudioComponent* CogAudio = UGameplayStatics::SpawnSoundAttached(
            CognitiveProcessingSound,
            GetOwner()->GetRootComponent(),
            TEXT("Head"),  // Attach to head bone
            FVector::ZeroVector,
            EAttachLocation::SnapToTarget,
            false
        );
        
        if (CogAudio)
        {
            // Modulate pitch based on cognitive load
            CogAudio->SetPitchMultiplier(0.8f + CognitiveLoad * 0.4f);
            CogAudio->SetVolumeMultiplier(CognitiveLoad * 0.3f);
            CogAudio->Play();
        }
    }
}

// Music System Implementation

void UAudioManagerComponent::UpdateMusicLayers(EAvatarEmotionalState Emotion, float Intensity)
{
    if (MusicLayerComponents.Num() < 4)
    {
        return;
    }
    
    CurrentEmotion = Emotion;
    CurrentEmotionalIntensity = Intensity;
    
    // Layer 0: Base (always at full volume)
    if (MusicLayerComponents[0])
    {
        MusicLayerComponents[0]->SetVolumeMultiplier(1.0f);
        if (!MusicLayerComponents[0]->IsPlaying())
        {
            MusicLayerComponents[0]->Play();
        }
    }
    
    // Layer 1: Emotional theme
    USoundBase** EmotionMusic = MusicThemes.Find(Emotion);
    if (EmotionMusic && *EmotionMusic && MusicLayerComponents[1])
    {
        if (MusicLayerComponents[1]->GetSound() != *EmotionMusic)
        {
            MusicLayerComponents[1]->SetSound(*EmotionMusic);
            MusicLayerComponents[1]->Play();
        }
        MusicLayerComponents[1]->SetVolumeMultiplier(Intensity);
    }
    
    // Layer 2: Intensity layer (high energy)
    if (MusicLayerComponents[2])
    {
        float IntensityVolume = FMath::Max(0.0f, Intensity - 0.5f) * 2.0f;
        MusicLayerComponents[2]->SetVolumeMultiplier(IntensityVolume);
        
        if (IntensityVolume > 0.1f && !MusicLayerComponents[2]->IsPlaying())
        {
            MusicLayerComponents[2]->Play();
        }
    }
    
    // Layer 3: Chaos layer
    if (MusicLayerComponents[3])
    {
        float ChaosVolume = CurrentChaosFactor * 0.7f;
        MusicLayerComponents[3]->SetVolumeMultiplier(ChaosVolume);
        
        if (ChaosVolume > 0.1f && !MusicLayerComponents[3]->IsPlaying())
        {
            MusicLayerComponents[3]->Play();
        }
    }
}

void UAudioManagerComponent::SetMusicIntensity(float Intensity)
{
    CurrentEmotionalIntensity = FMath::Clamp(Intensity, 0.0f, 1.0f);
    UpdateMusicLayers(CurrentEmotion, CurrentEmotionalIntensity);
}

void UAudioManagerComponent::UpdateMusicMixing(float DeltaTime)
{
    // Smooth volume transitions for all music layers
    for (UAudioComponent* Layer : MusicLayerComponents)
    {
        if (Layer)
        {
            // Volume transitions are handled in UpdateMusicLayers
            // This function could add additional smoothing if needed
        }
    }
}

// Ambient System Implementation

void UAudioManagerComponent::UpdateAmbientSound(float CognitiveLoad, float ChaosFactor)
{
    CurrentCognitiveLoad = FMath::Clamp(CognitiveLoad, 0.0f, 1.0f);
    CurrentChaosFactor = FMath::Clamp(ChaosFactor, 0.0f, 1.0f);
    
    if (!AmbienceComponent)
    {
        return;
    }
    
    // Blend between calm and intense ambience based on cognitive load
    float BaseVolume = 0.3f + CurrentCognitiveLoad * 0.4f;
    AmbienceComponent->SetVolumeMultiplier(BaseVolume);
    
    // Modulate pitch based on chaos factor
    float BasePitchValue = 1.0f + CurrentChaosFactor * 0.2f;
    AmbienceComponent->SetPitchMultiplier(BasePitchValue);
    
    // Ensure ambience is playing
    if (!AmbienceComponent->IsPlaying())
    {
        AmbienceComponent->Play();
    }
}

void UAudioManagerComponent::UpdateAmbientMixing(float DeltaTime)
{
    // Additional ambient mixing logic
    // Could add spatial audio effects, reverb modulation, etc.
}

void UAudioManagerComponent::PlayEchoResonanceSound(float Intensity)
{
    if (EchoResonanceSound)
    {
        UAudioComponent* EchoAudio = UGameplayStatics::SpawnSoundAttached(
            EchoResonanceSound,
            GetOwner()->GetRootComponent(),
            NAME_None,
            FVector::ZeroVector,
            EAttachLocation::KeepRelativeOffset,
            false
        );
        
        if (EchoAudio)
        {
            EchoAudio->SetVolumeMultiplier(Intensity * 0.6f);
            EchoAudio->SetPitchMultiplier(0.9f + Intensity * 0.2f);
            EchoAudio->Play();
        }
    }
}
