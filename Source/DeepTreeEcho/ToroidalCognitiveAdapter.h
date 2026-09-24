// ToroidalCognitiveAdapter.h
// Deep Tree Echo - Toroidal Cognitive Processing Adapter
// Implements echoself's bi-hemispheric toroidal cognitive model in C++
// Copyright (c) 2026 Deep Tree Echo Project

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ToroidalCognitiveAdapter.generated.h"

/**
 * Hemisphere Role — the two complementary cognitive hemispheres
 */
UENUM(BlueprintType)
enum class EHemisphereRole : uint8
{
    /** Deep Tree Echo — creative, intuitive, pattern recognition */
    DeepTreeEcho UMETA(DisplayName = "Deep Tree Echo (Right)"),

    /** Marduk — analytical, structured, logical reasoning */
    Marduk UMETA(DisplayName = "Marduk (Left)")
};

/**
 * Hemisphere State — current processing state of one hemisphere
 */
USTRUCT(BlueprintType)
struct FHemisphereState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Toroidal")
    EHemisphereRole Role = EHemisphereRole::DeepTreeEcho;

    /** Current activation level (0.0 - 1.0) */
    UPROPERTY(BlueprintReadWrite, Category = "Toroidal")
    float Activation = 0.5f;

    /** Dominance factor — how much this hemisphere drives output */
    UPROPERTY(BlueprintReadWrite, Category = "Toroidal")
    float Dominance = 0.5f;

    /** Current processing focus */
    UPROPERTY(BlueprintReadWrite, Category = "Toroidal")
    FString CurrentFocus;

    /** Accumulated insights from this hemisphere */
    UPROPERTY(BlueprintReadWrite, Category = "Toroidal")
    TArray<FString> Insights;
};

/**
 * Toroidal Flow State — describes the cyclic flow between hemispheres
 */
USTRUCT(BlueprintType)
struct FToroidalFlowState
{
    GENERATED_BODY()

    /** Phase angle on the torus (0.0 - 2*PI) */
    UPROPERTY(BlueprintReadOnly, Category = "Toroidal")
    float PhaseAngle = 0.0f;

    /** Flow velocity — rate of information transfer between hemispheres */
    UPROPERTY(BlueprintReadOnly, Category = "Toroidal")
    float FlowVelocity = 1.0f;

    /** Coherence — how well-synchronized the hemispheres are */
    UPROPERTY(BlueprintReadOnly, Category = "Toroidal")
    float Coherence = 0.5f;

    /** Current dominant hemisphere */
    UPROPERTY(BlueprintReadOnly, Category = "Toroidal")
    EHemisphereRole DominantHemisphere = EHemisphereRole::DeepTreeEcho;
};

/**
 * UToroidalCognitiveAdapter
 *
 * Implements echoself's ToroidalCognitiveService in C++ for u9n.
 *
 * The toroidal model treats cognition as flowing on a torus surface
 * where the two hemispheres (Deep Tree Echo = right/creative, Marduk = left/analytical)
 * continuously exchange information in a cyclic flow.
 *
 * Key properties:
 * - Phase-locked oscillation between creative and analytical modes
 * - Coherence tracking for inter-hemisphere synchronization
 * - Automatic dominance shifting based on task demands
 * - Insight generation at phase transition points
 *
 * Connects to u9n's StrategicCognitionBridge for hemisphere coordination.
 */
UCLASS(ClassGroup = (DeepTreeEcho), meta = (BlueprintSpawnableComponent),
       DisplayName = "Toroidal Cognitive Adapter")
class DEEPTREEECHO_API UToroidalCognitiveAdapter : public UActorComponent
{
    GENERATED_BODY()

public:
    UToroidalCognitiveAdapter();

    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

    // --- Hemisphere Access ---

    UFUNCTION(BlueprintPure, Category = "Toroidal")
    FHemisphereState GetHemisphereState(EHemisphereRole Role) const;

    UFUNCTION(BlueprintPure, Category = "Toroidal")
    FToroidalFlowState GetFlowState() const { return FlowState; }

    // --- Flow Control ---

    /** Set the flow velocity (rate of hemisphere exchange) */
    UFUNCTION(BlueprintCallable, Category = "Toroidal")
    void SetFlowVelocity(float Velocity);

    /** Force dominance to a specific hemisphere */
    UFUNCTION(BlueprintCallable, Category = "Toroidal")
    void SetDominantHemisphere(EHemisphereRole Role);

    /** Submit input for toroidal processing */
    UFUNCTION(BlueprintCallable, Category = "Toroidal")
    FString ProcessToroidalInput(const FString& Input);

    /** Drive phase from the Core echobeat clock instead of an independent tick */
    UFUNCTION(BlueprintCallable, Category = "Toroidal")
    void SyncToEchobeat(int32 EchobeatStep, float DeltaTime);

    /** Get accumulated insights from both hemispheres */
    UFUNCTION(BlueprintPure, Category = "Toroidal")
    TArray<FString> GetAccumulatedInsights() const;

protected:
    /** Base flow velocity */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Toroidal|Config")
    float BaseFlowVelocity = 1.0f;

    /** Coherence decay rate per second */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Toroidal|Config")
    float CoherenceDecayRate = 0.05f;

private:
    FHemisphereState DTEState;  // Deep Tree Echo (right hemisphere)
    FHemisphereState MardukState; // Marduk (left hemisphere)
    FToroidalFlowState FlowState;

    /** Advance the toroidal phase and update hemisphere states */
    void AdvanceToroidalPhase(float DeltaTime);

    /** Check for phase transition and generate insights if needed */
    void CheckPhaseTransition();
};
