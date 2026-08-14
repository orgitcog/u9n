#pragma once

/**
 * Deep Tree Echo Unreal Bridge
 * 
 * Comprehensive integration layer between Deep Tree Echo cognitive framework
 * and Unreal Engine systems. Implements all Unreal Engine functions for
 * 4E embodied cognition within the Deep Tree Echo architecture.
 * 
 * Architecture:
 * - 12-step cognitive cycle with 3 concurrent consciousness streams
 * - OEIS A000081 nested shell structure (1→1, 2→2, 3→4, 4→9 terms)
 * - 4E Cognition: Embodied, Embedded, Enacted, Extended
 * - Relevance Realization through salience landscape navigation
 * - Reservoir Computing via ReservoirCpp integration
 * 
 * @see DeepTreeEchoCore for cognitive architecture
 * @see DeepTreeEchoReservoir for ESN implementation
 * @see EmbodiedCognitionComponent for 4E integration
 */

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Character.h"
#include "../../Core/DeepTreeEchoCore.h"
#include "../../Reservoir/DeepTreeEchoReservoir.h"
#include "../../4ECognition/EmbodiedCognitionComponent.h"
#include "DeepTreeEchoUnrealBridge.generated.h"

// Forward declarations
class UAnimInstance;
class USkeletalMeshComponent;
class UAudioComponent;
class UNiagaraComponent;

/**
 * Unreal Engine Integration Mode
 */
UENUM(BlueprintType)
enum class EUnrealIntegrationMode : uint8
{
    /** Full integration - all systems active */
    Full UMETA(DisplayName = "Full Integration"),
    
    /** Cognitive only - no rendering/animation */
    CognitiveOnly UMETA(DisplayName = "Cognitive Only"),
    
    /** Avatar only - rendering without full cognition */
    AvatarOnly UMETA(DisplayName = "Avatar Only"),
    
    /** Minimal - basic functionality */
    Minimal UMETA(DisplayName = "Minimal")
};

/**
 * Avatar Expression State
 * Represents the current emotional/cognitive expression of the avatar
 */
USTRUCT(BlueprintType)
struct FAvatarExpressionState
{
    GENERATED_BODY()

    /** Primary emotion (0-1 intensity) */
    UPROPERTY(BlueprintReadWrite)
    TMap<FString, float> EmotionIntensities;

    /** Facial blend shape targets */
    UPROPERTY(BlueprintReadWrite)
    TMap<FString, float> FacialBlendShapes;

    /** Eye gaze direction */
    UPROPERTY(BlueprintReadWrite)
    FVector GazeDirection = FVector::ForwardVector;

    /** Blink rate modifier */
    UPROPERTY(BlueprintReadWrite)
    float BlinkRateModifier = 1.0f;

    /** Breathing rate modifier */
    UPROPERTY(BlueprintReadWrite)
    float BreathingRateModifier = 1.0f;

    /** Overall expression intensity */
    UPROPERTY(BlueprintReadWrite)
    float ExpressionIntensity = 1.0f;
};

/**
 * Cognitive-Motor Mapping
 * Maps cognitive states to motor outputs
 */
USTRUCT(BlueprintType)
struct FCognitiveMotorMapping
{
    GENERATED_BODY()

    /** Cognitive mode to animation state mapping */
    UPROPERTY(BlueprintReadWrite)
    TMap<FString, FString> ModeToAnimationMap;

    /** Emotion to gesture mapping */
    UPROPERTY(BlueprintReadWrite)
    TMap<FString, FString> EmotionToGestureMap;

    /** Attention to gaze mapping weight */
    UPROPERTY(BlueprintReadWrite)
    float AttentionToGazeWeight = 0.8f;

    /** Cognitive load to posture mapping weight */
    UPROPERTY(BlueprintReadWrite)
    float CognitiveLoadToPostureWeight = 0.6f;
};

/**
 * Sensory Input Configuration
 * Configures how Unreal Engine inputs map to cognitive sensory channels
 */
USTRUCT(BlueprintType)
struct FSensoryInputConfig
{
    GENERATED_BODY()

    /** Visual input enabled */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bEnableVisualInput = true;

    /** Auditory input enabled */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bEnableAuditoryInput = true;

    /** Proprioceptive input enabled */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bEnableProprioceptiveInput = true;

    /** Environmental input enabled */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bEnableEnvironmentalInput = true;

    /** Social input enabled (other characters) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bEnableSocialInput = true;

    /** Visual field of view (degrees) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "30.0", ClampMax = "180.0"))
    float VisualFieldOfView = 120.0f;

    /** Auditory range (units) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "100.0", ClampMax = "10000.0"))
    float AuditoryRange = 2000.0f;
};

/**
 * Deep Tree Echo Unreal Bridge Component
 * 
 * Main integration component that bridges the Deep Tree Echo cognitive
 * framework with Unreal Engine's character, animation, and rendering systems.
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UDeepTreeEchoUnrealBridge : public UActorComponent
{
    GENERATED_BODY()

public:
    UDeepTreeEchoUnrealBridge();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Integration mode */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeepTreeEcho|Config")
    EUnrealIntegrationMode IntegrationMode = EUnrealIntegrationMode::Full;

    /** Sensory input configuration */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeepTreeEcho|Config")
    FSensoryInputConfig SensoryConfig;

    /** Cognitive-motor mapping */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeepTreeEcho|Config")
    FCognitiveMotorMapping MotorMapping;

    /** Enable cognitive visualization (debug) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DeepTreeEcho|Config")
    bool bEnableCognitiveVisualization = false;

    // ========================================
    // COMPONENT REFERENCES
    // ========================================

    /** Reference to Deep Tree Echo Core */
    UPROPERTY(BlueprintReadOnly, Category = "DeepTreeEcho|Components")
    UDeepTreeEchoCore* CognitiveCore;

    /** Reference to Reservoir system */
    UPROPERTY(BlueprintReadOnly, Category = "DeepTreeEcho|Components")
    UDeepTreeEchoReservoir* ReservoirSystem;

    /** Reference to 4E Cognition component */
    UPROPERTY(BlueprintReadOnly, Category = "DeepTreeEcho|Components")
    UEmbodiedCognitionComponent* EmbodiedCognition;

    /** Reference to skeletal mesh (avatar body) */
    UPROPERTY(BlueprintReadOnly, Category = "DeepTreeEcho|Components")
    USkeletalMeshComponent* AvatarMesh;

    /** Reference to animation instance */
    UPROPERTY(BlueprintReadOnly, Category = "DeepTreeEcho|Components")
    UAnimInstance* AnimInstance;

    // ========================================
    // AVATAR STATE
    // ========================================

    /** Current expression state */
    UPROPERTY(BlueprintReadOnly, Category = "DeepTreeEcho|State")
    FAvatarExpressionState CurrentExpression;

    // ========================================
    // PUBLIC API - INITIALIZATION
    // ========================================

    /** Initialize the bridge with all required components */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Bridge")
    void InitializeBridge();

    /** Set the avatar mesh component */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Bridge")
    void SetAvatarMesh(USkeletalMeshComponent* InMesh);

    // ========================================
    // PUBLIC API - SENSORY INPUT
    // ========================================

    /** Process visual input from scene */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Sensory")
    void ProcessVisualInput(const TArray<AActor*>& VisibleActors);

    /** Process auditory input */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Sensory")
    void ProcessAuditoryInput(const TArray<FVector>& SoundSources, const TArray<float>& SoundIntensities);

    /** Process proprioceptive input from animation */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Sensory")
    void ProcessProprioceptiveInput();

    /** Process environmental input (affordances) */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Sensory")
    void ProcessEnvironmentalInput(const TArray<FString>& Affordances);

    /** Process social input (other characters) */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Sensory")
    void ProcessSocialInput(const TArray<AActor*>& NearbyCharacters);

    // ========================================
    // PUBLIC API - MOTOR OUTPUT
    // ========================================

    /** Update avatar expression based on cognitive state */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Motor")
    void UpdateAvatarExpression();

    /** Apply facial blend shapes */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Motor")
    void ApplyFacialBlendShapes(const TMap<FString, float>& BlendShapes);

    /** Set gaze target */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Motor")
    void SetGazeTarget(const FVector& TargetLocation);

    /** Trigger gesture animation */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Motor")
    void TriggerGesture(const FString& GestureName, float Intensity = 1.0f);

    /** Update body posture based on cognitive state */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Motor")
    void UpdateBodyPosture();

    // ========================================
    // PUBLIC API - COGNITIVE INTEGRATION
    // ========================================

    /** Get current cognitive mode as string */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Cognitive")
    FString GetCurrentCognitiveModeString() const;

    /** Get current consciousness stream states */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Cognitive")
    TArray<FString> GetConsciousnessStreamStates() const;

    /** Get current 4E integration score */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Cognitive")
    float Get4EIntegrationScore() const;

    /** Get current relevance frame */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Cognitive")
    FString GetCurrentRelevanceFrame() const;

    /** Get reservoir state summary */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Cognitive")
    TArray<float> GetReservoirStateSummary() const;

    // ========================================
    // PUBLIC API - NARRATIVE INTEGRATION
    // ========================================

    /** Add diary entry from current cognitive state */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Narrative")
    void AddDiaryEntryFromCognitiveState(const FString& Context);

    /** Generate insight from recent experiences */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Narrative")
    FString GenerateInsight();

    /** Get current wisdom weight */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Narrative")
    float GetCurrentWisdomWeight() const;

    // ========================================
    // PUBLIC API - EVOLUTION
    // ========================================

    /** Trigger ontogenetic development step */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Evolution")
    void TriggerDevelopmentStep();

    /** Get current developmental stage */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Evolution")
    int32 GetDevelopmentalStage() const;

    /** Get entelechy actualization score */
    UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Evolution")
    float GetEntelechyScore() const;

protected:
    virtual void BeginPlay() override;

private:
    // ========================================
    // INTERNAL STATE
    // ========================================

    /** Sensory accumulator for visual input */
    TArray<float> VisualSensoryBuffer;

    /** Sensory accumulator for auditory input */
    TArray<float> AuditorySensoryBuffer;

    /** Proprioceptive state buffer */
    TArray<float> ProprioceptiveBuffer;

    /** Environmental affordance buffer */
    TArray<FString> AffordanceBuffer;

    /** Social context buffer */
    TArray<FString> SocialContextBuffer;

    /** Expression update timer */
    float ExpressionUpdateTimer = 0.0f;

    /** Expression update interval */
    float ExpressionUpdateInterval = 0.05f; // 20 Hz

    // ========================================
    // INTERNAL METHODS
    // ========================================

    /** Initialize component references */
    void InitializeComponentReferences();

    /** Process all sensory inputs */
    void ProcessAllSensoryInputs();

    /** Update motor outputs */
    void UpdateMotorOutputs(float DeltaTime);

    /** Map cognitive state to expression */
    FAvatarExpressionState MapCognitiveStateToExpression() const;

    /** Map emotion to facial blend shapes */
    TMap<FString, float> MapEmotionToBlendShapes(const FString& Emotion, float Intensity) const;

    /** Compute gaze direction from attention */
    FVector ComputeGazeFromAttention() const;

    /** Draw cognitive visualization (debug) */
    void DrawCognitiveVisualization() const;
};
