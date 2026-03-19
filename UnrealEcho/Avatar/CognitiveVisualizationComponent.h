#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CognitiveVisualizationComponent.generated.h"

UENUM(BlueprintType)
enum class ECognitiveActivityType : uint8
{
    Idle UMETA(DisplayName = "Idle"),
    Thinking UMETA(DisplayName = "Thinking"),
    Learning UMETA(DisplayName = "Learning"),
    Remembering UMETA(DisplayName = "Remembering"),
    Creating UMETA(DisplayName = "Creating"),
    Analyzing UMETA(DisplayName = "Analyzing"),
    Deciding UMETA(DisplayName = "Deciding"),
    Emotional UMETA(DisplayName = "Emotional Processing"),
    Sensory UMETA(DisplayName = "Sensory Processing"),
    Motor UMETA(DisplayName = "Motor Planning")
};

USTRUCT(BlueprintType)
struct FNeuralNode
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cognitive")
    FVector Position = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cognitive")
    float Activation = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cognitive")
    FLinearColor Color = FLinearColor::White;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cognitive")
    float Size = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cognitive")
    int32 Layer = 0;
};

USTRUCT(BlueprintType)
struct FNeuralConnection
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cognitive")
    int32 FromNodeIndex = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cognitive")
    int32 ToNodeIndex = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cognitive")
    float Weight = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cognitive")
    float ActivityLevel = 0.0f;
};

USTRUCT(BlueprintType)
struct FThoughtParticle
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cognitive")
    FVector Position = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cognitive")
    FVector Velocity = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cognitive")
    FLinearColor Color = FLinearColor::White;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cognitive")
    float Lifetime = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cognitive")
    float Size = 3.0f;
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class DEEPTREECHO_API UCognitiveVisualizationComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCognitiveVisualizationComponent();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    /** Set cognitive activity type */
    UFUNCTION(BlueprintCallable, Category = "Cognitive Visualization")
    void SetCognitiveActivity(ECognitiveActivityType Activity, float Intensity = 1.0f);

    /** Get current cognitive activity */
    UFUNCTION(BlueprintPure, Category = "Cognitive Visualization")
    ECognitiveActivityType GetCognitiveActivity() const { return CurrentActivity; }

    /** Trigger thought process visualization */
    UFUNCTION(BlueprintCallable, Category = "Cognitive Visualization")
    void TriggerThoughtProcess(const FVector& StartPosition, const FVector& EndPosition);

    /** Visualize memory access */
    UFUNCTION(BlueprintCallable, Category = "Cognitive Visualization")
    void VisualizeMemoryAccess(int32 MemoryRegion, float AccessIntensity);

    /** Visualize decision making */
    UFUNCTION(BlueprintCallable, Category = "Cognitive Visualization")
    void VisualizeDecisionMaking(const TArray<FString>& Options, int32 SelectedIndex);

    /** Set attention focus point */
    UFUNCTION(BlueprintCallable, Category = "Cognitive Visualization")
    void SetAttentionFocus(const FVector& FocusPoint, float FocusIntensity);

    /** Enable/disable cognitive visualization */
    UFUNCTION(BlueprintCallable, Category = "Cognitive Visualization")
    void SetVisualizationEnabled(bool bEnabled);

    /** Set overall visualization intensity */
    UFUNCTION(BlueprintCallable, Category = "Cognitive Visualization")
    void SetVisualizationIntensity(float Intensity);

    /** Trigger echo resonance effect for memory/cognitive pulses */
    UFUNCTION(BlueprintCallable, Category = "Cognitive Visualization")
    void TriggerResonanceEffect(float Intensity, float Duration);

    /** Visualize memory constellation pattern */
    UFUNCTION(BlueprintCallable, Category = "Cognitive Visualization")
    void VisualizeMemoryConstellation(const TArray<FVector>& MemoryNodes);

    /** Get neural network nodes */
    UFUNCTION(BlueprintPure, Category = "Cognitive Visualization")
    TArray<FNeuralNode> GetNeuralNodes() const { return NeuralNodes; }

private:
    /** Initialize neural network structure */
    void InitializeNeuralNetwork();

    /** Update neural network activity */
    void UpdateNeuralActivity(float DeltaTime);

    /** Update thought particles */
    void UpdateThoughtParticles(float DeltaTime);

    /** Update attention visualization */
    void UpdateAttentionVisualization(float DeltaTime);

    /** Generate thought particle stream */
    void GenerateThoughtParticleStream(const FVector& Start, const FVector& End, const FLinearColor& Color);

    /** Propagate neural activation */
    void PropagateNeuralActivation(int32 StartNodeIndex, float ActivationStrength);

    /** Update cognitive activity patterns */
    void UpdateCognitivePatterns(float DeltaTime);

    /** Current cognitive activity */
    UPROPERTY()
    ECognitiveActivityType CurrentActivity;

    /** Activity intensity */
    float ActivityIntensity;

    /** Neural network nodes */
    UPROPERTY()
    TArray<FNeuralNode> NeuralNodes;

    /** Neural connections */
    UPROPERTY()
    TArray<FNeuralConnection> NeuralConnections;

    /** Thought particles */
    UPROPERTY()
    TArray<FThoughtParticle> ThoughtParticles;

    /** Attention focus point */
    FVector AttentionFocusPoint;

    /** Attention focus intensity */
    float AttentionFocusIntensity;

    /** Has attention focus */
    bool bHasAttentionFocus;

    /** Visualization enabled */
    UPROPERTY(EditAnywhere, Category = "Cognitive Visualization")
    bool bVisualizationEnabled = true;

    /** Neural activity time */
    float NeuralActivityTime;

    /** Cognitive pattern time */
    float CognitivePatternTime;

    /** Memory access indicators */
    TMap<int32, float> MemoryAccessIndicators;

    /** Decision visualization active */
    bool bDecisionVisualizationActive;

    /** Decision options */
    TArray<FString> DecisionOptions;

    /** Selected decision index */
    int32 SelectedDecisionIndex;

    /** Decision visualization timer */
    float DecisionVisualizationTimer;

    /** Overall visualization intensity multiplier */
    UPROPERTY()
    float VisualizationIntensity = 1.0f;

    /** Resonance effect active */
    bool bResonanceActive = false;

    /** Resonance effect intensity */
    float ResonanceIntensity = 0.0f;

    /** Resonance effect timer */
    float ResonanceTimer = 0.0f;

    /** Resonance effect duration */
    float ResonanceDuration = 0.0f;

    /** Memory constellation nodes for visualization */
    TArray<FVector> MemoryConstellationNodes;
};
