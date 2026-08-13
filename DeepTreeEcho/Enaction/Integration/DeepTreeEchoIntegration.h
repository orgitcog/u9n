#pragma once

/**
 * Deep-Tree-Echo Integration - Master Integration Component
 * 
 * Integrates all Deep-Tree-Echo subsystems:
 * - Cosmos State Machine (System 1-5 transitions)
 * - Holistic Metamodel (Organizational dynamics)
 * - Cognitive Cycle Manager (12-step cycle with 3 streams)
 * - Entelechy Framework (Vital actualization)
 * - Wisdom Cultivation (Relevance realization)
 * 
 * This component orchestrates the interplay between all subsystems
 * to create a unified cognitive architecture for 4E embodied cognition.
 */

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "../Cosmos/CosmosStateMachine.h"
#include "../../Metamodel/HolisticMetamodel.h"
#include "../../Cognitive/CognitiveCycleManager.h"
#include "../Entelechy/EntelechyFramework.h"
#include "../../Wisdom/WisdomCultivation.h"
#include "DeepTreeEchoIntegration.generated.h"

// ========================================
// STRUCTURES
// ========================================

/**
 * Integration Metrics
 */
USTRUCT(BlueprintType)
struct FIntegrationMetrics
{
    GENERATED_BODY()

    /** Overall system coherence (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float SystemCoherence = 0.0f;

    /** Cross-subsystem integration (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float CrossIntegration = 0.0f;

    /** Cognitive efficiency (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float CognitiveEfficiency = 0.0f;

    /** Embodiment quality (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float EmbodimentQuality = 0.0f;

    /** 4E integration score (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float FourEIntegration = 0.0f;

    /** Total processing cycles */
    UPROPERTY(BlueprintReadWrite)
    int64 TotalCycles = 0;
};

/**
 * 4E Cognition State (Embodied, Embedded, Enacted, Extended)
 */
USTRUCT(BlueprintType)
struct FFourECognitionState
{
    GENERATED_BODY()

    /** Embodied cognition - body-based processing */
    UPROPERTY(BlueprintReadWrite)
    float Embodied = 0.0f;

    /** Embedded cognition - environmental coupling */
    UPROPERTY(BlueprintReadWrite)
    float Embedded = 0.0f;

    /** Enacted cognition - action-perception loops */
    UPROPERTY(BlueprintReadWrite)
    float Enacted = 0.0f;

    /** Extended cognition - tool/environment extension */
    UPROPERTY(BlueprintReadWrite)
    float Extended = 0.0f;

    /** Overall 4E integration */
    UPROPERTY(BlueprintReadWrite)
    float Integration = 0.0f;
};

/**
 * Subsystem Status
 */
USTRUCT(BlueprintType)
struct FSubsystemStatus
{
    GENERATED_BODY()

    /** Cosmos State Machine active */
    UPROPERTY(BlueprintReadWrite)
    bool bCosmosActive = false;

    /** Holistic Metamodel active */
    UPROPERTY(BlueprintReadWrite)
    bool bMetamodelActive = false;

    /** Cognitive Cycle Manager active */
    UPROPERTY(BlueprintReadWrite)
    bool bCognitiveCycleActive = false;

    /** Entelechy Framework active */
    UPROPERTY(BlueprintReadWrite)
    bool bEntelechyActive = false;

    /** Wisdom Cultivation active */
    UPROPERTY(BlueprintReadWrite)
    bool bWisdomActive = false;

    /** All subsystems active */
    UPROPERTY(BlueprintReadWrite)
    bool bAllActive = false;
};

/**
 * Deep-Tree-Echo Integration Component
 * 
 * Master integration component that orchestrates all Deep-Tree-Echo
 * subsystems for unified cognitive processing.
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UDeepTreeEchoIntegration : public UActorComponent
{
    GENERATED_BODY()

public:
    UDeepTreeEchoIntegration();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Enable automatic integration processing */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Integration|Config")
    bool bEnableAutoProcessing = true;

    /** Processing interval in seconds */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Integration|Config", meta = (ClampMin = "0.01", ClampMax = "10.0"))
    float ProcessingInterval = 0.1f;

    /** Enable 4E cognition processing */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Integration|Config")
    bool bEnable4ECognition = true;

    /** Enable cross-subsystem feedback */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Integration|Config")
    bool bEnableCrossSubsystemFeedback = true;

    // ========================================
    // SUBSYSTEM REFERENCES
    // ========================================

    /** Cosmos State Machine component */
    UPROPERTY(BlueprintReadOnly, Category = "Integration|Subsystems")
    UCosmosStateMachine* CosmosStateMachine;

    /** Holistic Metamodel component */
    UPROPERTY(BlueprintReadOnly, Category = "Integration|Subsystems")
    UHolisticMetamodel* HolisticMetamodel;

    /** Cognitive Cycle Manager component */
    UPROPERTY(BlueprintReadOnly, Category = "Integration|Subsystems")
    UCognitiveCycleManager* CognitiveCycleManager;

    /** Entelechy Framework component */
    UPROPERTY(BlueprintReadOnly, Category = "Integration|Subsystems")
    UEntelechyFramework* EntelechyFramework;

    /** Wisdom Cultivation component */
    UPROPERTY(BlueprintReadOnly, Category = "Integration|Subsystems")
    UWisdomCultivation* WisdomCultivation;

    // ========================================
    // STATE
    // ========================================

    /** Current integration metrics */
    UPROPERTY(BlueprintReadOnly, Category = "Integration|State")
    FIntegrationMetrics Metrics;

    /** Current 4E cognition state */
    UPROPERTY(BlueprintReadOnly, Category = "Integration|State")
    FFourECognitionState FourEState;

    /** Current subsystem status */
    UPROPERTY(BlueprintReadOnly, Category = "Integration|State")
    FSubsystemStatus SubsystemStatus;

    // ========================================
    // PUBLIC API - INITIALIZATION
    // ========================================

    /** Initialize the integration system */
    UFUNCTION(BlueprintCallable, Category = "Integration")
    void InitializeIntegration();

    /** Reset all subsystems */
    UFUNCTION(BlueprintCallable, Category = "Integration")
    void ResetAllSubsystems();

    /** Discover and link subsystem components */
    UFUNCTION(BlueprintCallable, Category = "Integration")
    void DiscoverSubsystems();

    // ========================================
    // PUBLIC API - PROCESSING
    // ========================================

    /** Process one integration cycle */
    UFUNCTION(BlueprintCallable, Category = "Integration|Processing")
    void ProcessIntegrationCycle(float DeltaTime);

    /** Synchronize all subsystems */
    UFUNCTION(BlueprintCallable, Category = "Integration|Processing")
    void SynchronizeSubsystems();

    /** Apply cross-subsystem feedback */
    UFUNCTION(BlueprintCallable, Category = "Integration|Processing")
    void ApplyCrossSubsystemFeedback();

    /** Update 4E cognition state */
    UFUNCTION(BlueprintCallable, Category = "Integration|Processing")
    void Update4ECognition();

    // ========================================
    // PUBLIC API - METRICS
    // ========================================

    /** Get integration metrics */
    UFUNCTION(BlueprintCallable, Category = "Integration|Metrics")
    FIntegrationMetrics GetIntegrationMetrics() const;

    /** Get 4E cognition state */
    UFUNCTION(BlueprintCallable, Category = "Integration|Metrics")
    FFourECognitionState Get4ECognitionState() const;

    /** Get subsystem status */
    UFUNCTION(BlueprintCallable, Category = "Integration|Metrics")
    FSubsystemStatus GetSubsystemStatus() const;

    /** Calculate overall system health */
    UFUNCTION(BlueprintCallable, Category = "Integration|Metrics")
    float CalculateSystemHealth() const;

    // ========================================
    // PUBLIC API - SUBSYSTEM ACCESS
    // ========================================

    /** Get current Cosmos system level */
    UFUNCTION(BlueprintCallable, Category = "Integration|Cosmos")
    int32 GetCurrentCosmosLevel() const;

    /** Get current cognitive cycle step */
    UFUNCTION(BlueprintCallable, Category = "Integration|Cognitive")
    int32 GetCurrentCognitiveStep() const;

    /** Get current entelechy stage */
    UFUNCTION(BlueprintCallable, Category = "Integration|Entelechy")
    EEntelechyStage GetCurrentEntelechyStage() const;

    /** Get current wisdom level */
    UFUNCTION(BlueprintCallable, Category = "Integration|Wisdom")
    float GetCurrentWisdomLevel() const;

    /** Get metamodel coherence */
    UFUNCTION(BlueprintCallable, Category = "Integration|Metamodel")
    float GetMetamodelCoherence() const;

    // ========================================
    // PUBLIC API - DIAGNOSTICS
    // ========================================

    /** Generate diagnostic report */
    UFUNCTION(BlueprintCallable, Category = "Integration|Diagnostics")
    TArray<FString> GenerateDiagnosticReport() const;

    /** Check subsystem health */
    UFUNCTION(BlueprintCallable, Category = "Integration|Diagnostics")
    TMap<FString, float> CheckSubsystemHealth() const;

protected:
    virtual void BeginPlay() override;

private:
    /** Accumulated time for processing */
    float AccumulatedTime = 0.0f;

    /** Update subsystem status */
    void UpdateSubsystemStatus();

    /** Calculate integration metrics */
    void CalculateIntegrationMetrics();

    /** Calculate system coherence */
    float CalculateSystemCoherence() const;

    /** Calculate cross-integration */
    float CalculateCrossIntegration() const;

    /** Calculate cognitive efficiency */
    float CalculateCognitiveEfficiency() const;

    /** Calculate embodiment quality */
    float CalculateEmbodimentQuality() const;

    /** Calculate 4E integration */
    float Calculate4EIntegration() const;

    /** Feed Cosmos state to other subsystems */
    void FeedCosmosToSubsystems();

    /** Feed Metamodel state to other subsystems */
    void FeedMetamodelToSubsystems();

    /** Feed Cognitive Cycle state to other subsystems */
    void FeedCognitiveCycleToSubsystems();

    /** Feed Entelechy state to other subsystems */
    void FeedEntelechyToSubsystems();

    /** Feed Wisdom state to other subsystems */
    void FeedWisdomToSubsystems();
};
