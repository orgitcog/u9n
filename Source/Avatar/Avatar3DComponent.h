#pragma once

#include "CoreMinimal.h"
#include "Components/SkeletalMeshComponent.h"
#include "Avatar/FacialAnimationSystem.h"
#include "Avatar/GestureSystem.h"
#include "Avatar/EmotionalAuraComponent.h"
#include "Avatar3DComponent.generated.h"

// Forward declarations
class UFacialAnimationSystem;
class UGestureSystem;
class UEmotionalAuraComponent;
class UCognitiveVisualizationComponent;

/**
 * Core 3D Avatar Component for Deep Tree Echo
 * Integrates facial animation, gesture, emotional aura, and cognitive visualization systems
 * Provides bridge between neurochemical state and visual avatar expression
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class DEEPTREECHO_API UAvatar3DComponent : public USkeletalMeshComponent
{
    GENERATED_BODY()

public:
    UAvatar3DComponent();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // ===== Sub-Component References =====

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Avatar|Subsystems")
    class UFacialAnimationSystem* FacialSystem;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Avatar|Subsystems")
    class UGestureSystem* GestureSystem;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Avatar|Subsystems")
    class UEmotionalAuraComponent* EmotionalAura;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Avatar|Subsystems")
    class UCognitiveVisualizationComponent* CognitiveViz;

    // ===== Emotional State Control =====

    /** Set the avatar's emotional state with synchronized facial, body, and aura response */
    UFUNCTION(BlueprintCallable, Category = "Avatar|Emotion")
    void SetEmotionalState(EEmotionState Emotion, float Intensity = 1.0f, float TransitionTime = 0.5f);

    /** Trigger a gesture animation */
    UFUNCTION(BlueprintCallable, Category = "Avatar|Gesture")
    void TriggerGesture(EGestureType Gesture, float Intensity = 1.0f);

    /** Set body language state */
    UFUNCTION(BlueprintCallable, Category = "Avatar|Gesture")
    void SetBodyLanguage(EBodyLanguageState BodyLanguage, float TransitionTime = 0.5f);

    // ===== Neurochemical Integration =====

    /** Apply neurochemical state to avatar visual expression */
    UFUNCTION(BlueprintCallable, Category = "Avatar|Neurochemical")
    void ApplyNeurochemicalState(float Dopamine, float Serotonin, float Cortisol, float Oxytocin);

    /** Set cognitive load for visualization */
    UFUNCTION(BlueprintCallable, Category = "Avatar|Cognitive")
    void SetCognitiveLoad(float Load);

    /** Trigger echo resonance effect (memory/cognitive pulse) */
    UFUNCTION(BlueprintCallable, Category = "Avatar|DeepTreeEcho")
    void TriggerEchoResonance(float Intensity = 1.0f, float Duration = 0.5f);

protected:
    // ===== Internal Update Methods =====

    void UpdateFacialAnimation(float DeltaTime);
    void UpdateGestures(float DeltaTime);
    void UpdateEmotionalAura(float DeltaTime);
    void UpdateCognitiveVisualization(float DeltaTime);

private:
    // ===== State Tracking =====

    /** Current emotional intensity level */
    UPROPERTY()
    float CurrentEmotionalIntensity = 0.5f;

    /** Current cognitive load level (0-1) */
    UPROPERTY()
    float CurrentCognitiveLoad = 0.0f;

    /** Last applied neurochemical balance (Dopamine, Serotonin, Cortisol, Oxytocin) */
    UPROPERTY()
    FVector4 LastNeurochemicalBalance = FVector4(0.5f, 0.5f, 0.3f, 0.5f);
};
