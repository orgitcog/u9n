// AutognosisSystem.h
// Hierarchical Self-Image Building System for Deep Tree Echo
// Implements recursive self-awareness and meta-cognitive processing

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AutognosisSystem.generated.h"

// Forward declarations
class UCognitiveCycleManager;
class URelevanceRealizationEnnead;
class UWisdomCultivation;

/**
 * Self-awareness level enumeration
 */
UENUM(BlueprintType)
enum class ESelfAwarenessLevel : uint8
{
    DirectObservation   UMETA(DisplayName = "Level 0: Direct Observation"),
    PatternAnalysis     UMETA(DisplayName = "Level 1: Pattern Analysis"),
    MetaCognitive       UMETA(DisplayName = "Level 2: Meta-Cognitive"),
    RecursiveModeling   UMETA(DisplayName = "Level 3: Recursive Modeling"),
    TranscendentAware   UMETA(DisplayName = "Level 4: Transcendent Awareness")
};

/**
 * Insight category enumeration
 */
UENUM(BlueprintType)
enum class EInsightCategory : uint8
{
    ResourceUtilization     UMETA(DisplayName = "Resource Utilization"),
    BehavioralStability     UMETA(DisplayName = "Behavioral Stability"),
    CognitiveComplexity     UMETA(DisplayName = "Cognitive Complexity"),
    SelfAwarenessQuality    UMETA(DisplayName = "Self-Awareness Quality"),
    OptimizationOpportunity UMETA(DisplayName = "Optimization Opportunity"),
    AnomalyDetection        UMETA(DisplayName = "Anomaly Detection")
};

/**
 * Optimization priority enumeration
 */
UENUM(BlueprintType)
enum class EOptimizationPriority : uint8
{
    Low         UMETA(DisplayName = "Low"),
    Medium      UMETA(DisplayName = "Medium"),
    High        UMETA(DisplayName = "High"),
    Critical    UMETA(DisplayName = "Critical")
};

/**
 * Component state observation
 */
USTRUCT(BlueprintType)
struct FComponentObservation
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ComponentName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsActive = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float UtilizationLevel = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float PerformanceScore = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 EventsProcessed = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float AverageLatency = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> RecentBehaviors;
};

/**
 * Behavioral pattern detection
 */
USTRUCT(BlueprintType)
struct FBehavioralPattern
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString PatternID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString PatternType;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Description;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Frequency = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Stability = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Significance = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> InvolvedComponents;
};

/**
 * Meta-cognitive insight
 */
USTRUCT(BlueprintType)
struct FMetaCognitiveInsight
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString InsightID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EInsightCategory Category = EInsightCategory::SelfAwarenessQuality;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Description;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Confidence = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Importance = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString SourceLevel;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Timestamp = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> RelatedPatterns;
};

/**
 * Optimization opportunity
 */
USTRUCT(BlueprintType)
struct FOptimizationOpportunity
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString OpportunityID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Description;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EOptimizationPriority Priority = EOptimizationPriority::Medium;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ExpectedImprovement = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float RiskLevel = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString TargetComponent;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> RequiredActions;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsExecuted = false;
};

/**
 * Self-image at a specific hierarchical level
 */
USTRUCT(BlueprintType)
struct FHierarchicalSelfImage
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ESelfAwarenessLevel Level = ESelfAwarenessLevel::DirectObservation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ImageID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Confidence = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FComponentObservation> ComponentStates;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FBehavioralPattern> DetectedPatterns;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> MetaReflections;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Timestamp = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 RecursiveDepth = 0;
};

/**
 * Self-awareness assessment scores
 */
USTRUCT(BlueprintType)
struct FSelfAwarenessAssessment
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float PatternRecognition = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float PerformanceAwareness = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MetaReflectionDepth = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CognitiveComplexity = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float AdaptiveCapacity = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float OverallScore = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString QualitativeAssessment;
};

/**
 * Complete autognosis state
 */
USTRUCT(BlueprintType)
struct FAutognosisState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FHierarchicalSelfImage> SelfImages;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FMetaCognitiveInsight> Insights;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FOptimizationOpportunity> Optimizations;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FSelfAwarenessAssessment Assessment;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 CycleCount = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float LastCycleTime = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsRunning = false;
};

/**
 * Delegate declarations
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSelfImageBuilt, const FHierarchicalSelfImage&, SelfImage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInsightGenerated, const FMetaCognitiveInsight&, Insight);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnOptimizationDiscovered, const FOptimizationOpportunity&, Opportunity);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAutognosisCycleComplete, int32, CycleCount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSelfAwarenessChanged, float, NewScore);

/**
 * Autognosis System Component
 * Blueprint wrapper around canonical FIntrospectionNode (AutonomyPipeline REFLECTION).
 * IngestPipelineTelemetry mirrors AAR/self-model; StartAutognosis() is called from Core.
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UAutognosisSystem : public UActorComponent
{
    GENERATED_BODY()

public:
    UAutognosisSystem();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, 
                               FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    bool bEnableAutognosis = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    float CycleInterval = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    int32 MaxSelfImageLevels = 5;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    int32 MaxInsightHistory = 50;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    float PatternDetectionThreshold = 0.3f;

    // ========================================
    // EVENTS
    // ========================================

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnSelfImageBuilt OnSelfImageBuilt;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnInsightGenerated OnInsightGenerated;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnOptimizationDiscovered OnOptimizationDiscovered;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnAutognosisCycleComplete OnCycleComplete;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnSelfAwarenessChanged OnSelfAwarenessChanged;

    // ========================================
    // SELF-MONITORING
    // ========================================

    /** Observe current system state */
    UFUNCTION(BlueprintCallable, Category = "Self-Monitoring")
    TArray<FComponentObservation> ObserveSystem();

    /** Detect behavioral patterns */
    UFUNCTION(BlueprintCallable, Category = "Self-Monitoring")
    TArray<FBehavioralPattern> DetectPatterns();

    /** Detect anomalies in system behavior */
    UFUNCTION(BlueprintCallable, Category = "Self-Monitoring")
    TArray<FString> DetectAnomalies();

    /** Get recent observations */
    UFUNCTION(BlueprintPure, Category = "Self-Monitoring")
    TArray<FComponentObservation> GetRecentObservations() const;

    // ========================================
    // SELF-MODELING
    // ========================================

    /** Build self-image at specified level */
    UFUNCTION(BlueprintCallable, Category = "Self-Modeling")
    FHierarchicalSelfImage BuildSelfImage(ESelfAwarenessLevel Level);

    /** Get self-image at level */
    UFUNCTION(BlueprintPure, Category = "Self-Modeling")
    FHierarchicalSelfImage GetSelfImage(ESelfAwarenessLevel Level) const;

    /** Get all self-images */
    UFUNCTION(BlueprintPure, Category = "Self-Modeling")
    TArray<FHierarchicalSelfImage> GetAllSelfImages() const;

    /** Update self-image with new observation */
    UFUNCTION(BlueprintCallable, Category = "Self-Modeling")
    void UpdateSelfImage(ESelfAwarenessLevel Level, const FComponentObservation& Observation);

    /** Get recursive modeling depth */
    UFUNCTION(BlueprintPure, Category = "Self-Modeling")
    int32 GetRecursiveDepth() const;

    // ========================================
    // META-COGNITIVE PROCESSING
    // ========================================

    /** Process self-image for insights */
    UFUNCTION(BlueprintCallable, Category = "Meta-Cognition")
    TArray<FMetaCognitiveInsight> ProcessSelfImage(const FHierarchicalSelfImage& SelfImage);

    /** Generate insight from pattern */
    UFUNCTION(BlueprintCallable, Category = "Meta-Cognition")
    FMetaCognitiveInsight GenerateInsight(const FBehavioralPattern& Pattern, EInsightCategory Category);

    /** Get all insights */
    UFUNCTION(BlueprintPure, Category = "Meta-Cognition")
    TArray<FMetaCognitiveInsight> GetAllInsights() const;

    /** Get insights by category */
    UFUNCTION(BlueprintPure, Category = "Meta-Cognition")
    TArray<FMetaCognitiveInsight> GetInsightsByCategory(EInsightCategory Category) const;

    /** Add meta-reflection to self-image */
    UFUNCTION(BlueprintCallable, Category = "Meta-Cognition")
    void AddMetaReflection(ESelfAwarenessLevel Level, const FString& Reflection);

    // ========================================
    // SELF-OPTIMIZATION
    // ========================================

    /** Discover optimization opportunities */
    UFUNCTION(BlueprintCallable, Category = "Self-Optimization")
    TArray<FOptimizationOpportunity> DiscoverOptimizations();

    /** Get pending optimizations */
    UFUNCTION(BlueprintPure, Category = "Self-Optimization")
    TArray<FOptimizationOpportunity> GetPendingOptimizations() const;

    /** Execute optimization */
    UFUNCTION(BlueprintCallable, Category = "Self-Optimization")
    bool ExecuteOptimization(const FString& OpportunityID);

    /** Assess risk of optimization */
    UFUNCTION(BlueprintPure, Category = "Self-Optimization")
    float AssessOptimizationRisk(const FOptimizationOpportunity& Opportunity) const;

    // ========================================
    // AUTOGNOSIS ORCHESTRATION
    // ========================================

    /** Run complete autognosis cycle */
    UFUNCTION(BlueprintCallable, Category = "Orchestration")
    void RunAutognosisCycle();

    /** Get autognosis state */
    UFUNCTION(BlueprintPure, Category = "Orchestration")
    FAutognosisState GetAutognosisState() const;

    /** Get self-awareness assessment */
    UFUNCTION(BlueprintPure, Category = "Orchestration")
    FSelfAwarenessAssessment GetSelfAwarenessAssessment() const;

    /** Get overall self-awareness score */
    UFUNCTION(BlueprintPure, Category = "Orchestration")
    float GetSelfAwarenessScore() const;

    /** Start autognosis */
    UFUNCTION(BlueprintCallable, Category = "Orchestration")
    void StartAutognosis();

    /** Stop autognosis */
    UFUNCTION(BlueprintCallable, Category = "Orchestration")
    void StopAutognosis();

    /** Is autognosis running */
    UFUNCTION(BlueprintPure, Category = "Orchestration")
    bool IsAutognosisRunning() const;

    /** Mirror canonical FIntrospectionNode telemetry from AutonomyPipeline */
    UFUNCTION(BlueprintCallable, Category = "Orchestration")
    void IngestPipelineTelemetry(float AARCoherence, float SelfModelAccuracy,
                                 int32 EchobeatStep, int32 IntrospectionLevel);

protected:
    // Component references
    UPROPERTY()
    UCognitiveCycleManager* CycleManager;

    UPROPERTY()
    URelevanceRealizationEnnead* EnneadComponent;

    UPROPERTY()
    UWisdomCultivation* WisdomComponent;

    // Internal state
    FAutognosisState AutognosisState;
    TArray<FComponentObservation> ObservationHistory;
    TArray<FBehavioralPattern> PatternHistory;

    float CycleTimer = 0.0f;
    int32 InsightIDCounter = 0;
    int32 PatternIDCounter = 0;
    int32 OptimizationIDCounter = 0;

    // Internal methods
    void FindComponentReferences();
    void InitializeAutognosis();

    void ObserveComponents();
    void AnalyzePatterns();
    void BuildAllSelfImages();
    void GenerateInsights();
    void DiscoverOptimizationOpportunities();
    void UpdateSelfAwarenessAssessment();

    FComponentObservation ObserveComponent(UActorComponent* Component);
    FBehavioralPattern AnalyzePatternFromObservations(const TArray<FComponentObservation>& Observations);

    float ComputePatternRecognition() const;
    float ComputePerformanceAwareness() const;
    float ComputeMetaReflectionDepth() const;
    float ComputeCognitiveComplexity() const;
    float ComputeAdaptiveCapacity() const;

    FString GenerateInsightID();
    FString GeneratePatternID();
    FString GenerateOptimizationID();
    FString GenerateSelfImageID();

    FString GetQualitativeAssessment(float Score) const;
    int32 GetLevelIndex(ESelfAwarenessLevel Level) const;
};
