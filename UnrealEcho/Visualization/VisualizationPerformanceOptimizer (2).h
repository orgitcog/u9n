#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AgentVisualizationManager.h"
#include "VisualizationPerformanceOptimizer.generated.h"

// Forward declarations
class UAgentVisualizationManager;
class UDeepTreeEchoRenderingSubsystem;

/**
 * Performance Budget for visualization
 */
USTRUCT(BlueprintType)
struct FVisualizationBudget
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float TargetFrameTimeMs; // 16.67ms for 60 FPS

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float VisualizationBudgetMs; // Max time for visualization

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float StateUpdateBudgetMs; // Max time for 9P updates

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ParticleBudgetMs; // Max time for particle systems

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MaxActiveParticleSystems;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MaxVisibleAgents;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MaxCommunicationLinks;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MemoryBudgetMB;

    FVisualizationBudget()
        : TargetFrameTimeMs(16.67f) // 60 FPS
        , VisualizationBudgetMs(8.0f)
        , StateUpdateBudgetMs(2.0f)
        , ParticleBudgetMs(3.0f)
        , MaxActiveParticleSystems(10)
        , MaxVisibleAgents(10)
        , MaxCommunicationLinks(20)
        , MemoryBudgetMB(100.0f)
    {}
};

/**
 * Quality Preset for quick configuration
 */
UENUM(BlueprintType)
enum class EVisualizationQualityPreset : uint8
{
    Performance UMETA(DisplayName = "Performance"),
    Balanced UMETA(DisplayName = "Balanced"),
    Quality UMETA(DisplayName = "Quality"),
    Cinematic UMETA(DisplayName = "Cinematic"),
    Custom UMETA(DisplayName = "Custom")
};

/**
 * Optimization Strategy
 */
UENUM(BlueprintType)
enum class EOptimizationStrategy : uint8
{
    Aggressive UMETA(DisplayName = "Aggressive"),
    Moderate UMETA(DisplayName = "Moderate"),
    Conservative UMETA(DisplayName = "Conservative"),
    Adaptive UMETA(DisplayName = "Adaptive")
};

/**
 * Agent Priority for rendering
 */
USTRUCT(BlueprintType)
struct FAgentRenderPriority
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString AgentPath;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Priority; // 0-1, higher = more important

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bAlwaysRender;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MinLODLevel; // Never go below this LOD

    FAgentRenderPriority()
        : Priority(0.5f)
        , bAlwaysRender(false)
        , MinLODLevel(0)
    {}
};

/**
 * Optimization Report
 */
USTRUCT(BlueprintType)
struct FOptimizationReport
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FDateTime Timestamp;

    UPROPERTY(BlueprintReadOnly)
    float CurrentFPS;

    UPROPERTY(BlueprintReadOnly)
    float AverageFrameTimeMs;

    UPROPERTY(BlueprintReadOnly)
    float GPUTimeMs;

    UPROPERTY(BlueprintReadOnly)
    float VisualizationTimeMs;

    UPROPERTY(BlueprintReadOnly)
    float MemoryUsageMB;

    UPROPERTY(BlueprintReadOnly)
    int32 ActiveAgentCount;

    UPROPERTY(BlueprintReadOnly)
    int32 VisibleAgentCount;

    UPROPERTY(BlueprintReadOnly)
    int32 CulledAgentCount;

    UPROPERTY(BlueprintReadOnly)
    int32 LODReductionCount;

    UPROPERTY(BlueprintReadOnly)
    TArray<FString> AppliedOptimizations;

    UPROPERTY(BlueprintReadOnly)
    bool bMeetingTargetFPS;

    FOptimizationReport()
        : Timestamp(FDateTime::Now())
        , CurrentFPS(60.0f)
        , AverageFrameTimeMs(16.67f)
        , GPUTimeMs(8.0f)
        , VisualizationTimeMs(4.0f)
        , MemoryUsageMB(50.0f)
        , ActiveAgentCount(0)
        , VisibleAgentCount(0)
        , CulledAgentCount(0)
        , LODReductionCount(0)
        , bMeetingTargetFPS(true)
    {}
};

/**
 * Instancing Configuration for batched rendering
 */
USTRUCT(BlueprintType)
struct FAgentInstancingConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bEnableInstancing;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MinInstanceCount; // Min agents before instancing kicks in

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bInstanceSimilarAgents; // Instance agents with same mesh

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bInstanceAuras; // Instance aura effects

    FAgentInstancingConfig()
        : bEnableInstancing(true)
        , MinInstanceCount(3)
        , bInstanceSimilarAgents(true)
        , bInstanceAuras(true)
    {}
};

/**
 * Delegate for optimization events
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnOptimizationApplied, const FOptimizationReport&, Report);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBudgetExceeded, const FString&, BudgetType, float, CurrentValue);

/**
 * Visualization Performance Optimizer Component
 *
 * Manages performance optimization for multi-agent visualization to maintain
 * 60+ FPS with 5+ simultaneous agents.
 *
 * Core Features:
 * - Dynamic LOD management based on distance and importance
 * - Frustum culling for off-screen agents
 * - Budget-based rendering limits
 * - Adaptive quality scaling
 * - GPU/CPU load balancing
 * - Memory usage optimization
 * - Instanced rendering for similar agents
 *
 * Target: 60+ FPS with 5+ agents, <50MB memory overhead
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UNREALENGINE_API UVisualizationPerformanceOptimizer : public UActorComponent
{
    GENERATED_BODY()

public:
    UVisualizationPerformanceOptimizer();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

    // ===== Configuration =====

    UFUNCTION(BlueprintCallable, Category = "PerformanceOptimizer|Config")
    void SetVisualizationBudget(const FVisualizationBudget& Budget);

    UFUNCTION(BlueprintPure, Category = "PerformanceOptimizer|Config")
    FVisualizationBudget GetVisualizationBudget() const { return CurrentBudget; }

    UFUNCTION(BlueprintCallable, Category = "PerformanceOptimizer|Config")
    void SetQualityPreset(EVisualizationQualityPreset Preset);

    UFUNCTION(BlueprintPure, Category = "PerformanceOptimizer|Config")
    EVisualizationQualityPreset GetQualityPreset() const { return CurrentPreset; }

    UFUNCTION(BlueprintCallable, Category = "PerformanceOptimizer|Config")
    void SetOptimizationStrategy(EOptimizationStrategy Strategy);

    UFUNCTION(BlueprintCallable, Category = "PerformanceOptimizer|Config")
    void SetInstancingConfig(const FAgentInstancingConfig& Config);

    // ===== Agent Priority =====

    UFUNCTION(BlueprintCallable, Category = "PerformanceOptimizer|Priority")
    void SetAgentPriority(const FString& AgentPath, float Priority);

    UFUNCTION(BlueprintCallable, Category = "PerformanceOptimizer|Priority")
    void SetAgentAlwaysRender(const FString& AgentPath, bool bAlwaysRender);

    UFUNCTION(BlueprintCallable, Category = "PerformanceOptimizer|Priority")
    void SetAgentMinLOD(const FString& AgentPath, int32 MinLOD);

    UFUNCTION(BlueprintCallable, Category = "PerformanceOptimizer|Priority")
    float GetAgentPriority(const FString& AgentPath);

    UFUNCTION(BlueprintCallable, Category = "PerformanceOptimizer|Priority")
    void ClearAgentPriorities();

    // ===== LOD Control =====

    UFUNCTION(BlueprintCallable, Category = "PerformanceOptimizer|LOD")
    void SetGlobalLODBias(int32 Bias);

    UFUNCTION(BlueprintCallable, Category = "PerformanceOptimizer|LOD")
    void SetDynamicLODEnabled(bool bEnabled);

    UFUNCTION(BlueprintCallable, Category = "PerformanceOptimizer|LOD")
    void SetLODDistanceScale(float Scale);

    UFUNCTION(BlueprintCallable, Category = "PerformanceOptimizer|LOD")
    void ForceLODLevel(int32 Level);

    UFUNCTION(BlueprintCallable, Category = "PerformanceOptimizer|LOD")
    void ClearForcedLOD();

    // ===== Culling =====

    UFUNCTION(BlueprintCallable, Category = "PerformanceOptimizer|Culling")
    void SetFrustumCullingEnabled(bool bEnabled);

    UFUNCTION(BlueprintCallable, Category = "PerformanceOptimizer|Culling")
    void SetDistanceCullingEnabled(bool bEnabled);

    UFUNCTION(BlueprintCallable, Category = "PerformanceOptimizer|Culling")
    void SetMaxRenderDistance(float Distance);

    UFUNCTION(BlueprintCallable, Category = "PerformanceOptimizer|Culling")
    void SetOcclusionCullingEnabled(bool bEnabled);

    // ===== Performance Queries =====

    UFUNCTION(BlueprintCallable, Category = "PerformanceOptimizer|Query")
    FOptimizationReport GetOptimizationReport();

    UFUNCTION(BlueprintCallable, Category = "PerformanceOptimizer|Query")
    bool IsMeetingTargetFPS();

    UFUNCTION(BlueprintCallable, Category = "PerformanceOptimizer|Query")
    float GetCurrentVisualizationLoad();

    UFUNCTION(BlueprintCallable, Category = "PerformanceOptimizer|Query")
    int32 GetRecommendedMaxAgents();

    UFUNCTION(BlueprintCallable, Category = "PerformanceOptimizer|Query")
    TArray<FString> GetCulledAgents();

    // ===== Manual Optimization =====

    UFUNCTION(BlueprintCallable, Category = "PerformanceOptimizer|Manual")
    void OptimizeNow();

    UFUNCTION(BlueprintCallable, Category = "PerformanceOptimizer|Manual")
    void ResetOptimizations();

    UFUNCTION(BlueprintCallable, Category = "PerformanceOptimizer|Manual")
    void PauseOptimization(bool bPause);

    UFUNCTION(BlueprintCallable, Category = "PerformanceOptimizer|Manual")
    void ProfileFrame();

    // ===== Effect Control =====

    UFUNCTION(BlueprintCallable, Category = "PerformanceOptimizer|Effects")
    void SetParticleSystemsEnabled(bool bEnabled);

    UFUNCTION(BlueprintCallable, Category = "PerformanceOptimizer|Effects")
    void SetAuraEffectsEnabled(bool bEnabled);

    UFUNCTION(BlueprintCallable, Category = "PerformanceOptimizer|Effects")
    void SetShadowsEnabled(bool bEnabled);

    UFUNCTION(BlueprintCallable, Category = "PerformanceOptimizer|Effects")
    void SetReflectionsEnabled(bool bEnabled);

    // ===== Events =====

    UPROPERTY(BlueprintAssignable, Category = "PerformanceOptimizer|Events")
    FOnOptimizationApplied OnOptimizationApplied;

    UPROPERTY(BlueprintAssignable, Category = "PerformanceOptimizer|Events")
    FOnBudgetExceeded OnBudgetExceeded;

protected:
    // ===== Internal Methods =====

    void ApplyQualityPreset(EVisualizationQualityPreset Preset);
    void UpdatePerformanceMetrics(float DeltaTime);
    void PerformCulling();
    void UpdateLODs();
    void ApplyAdaptiveOptimization();
    void CheckBudgets();
    void CalculateAgentImportance();
    TArray<FString> GetAgentsSortedByPriority();
    float CalculateAgentImportanceScore(const FString& AgentPath);
    void ApplyInstancing();

private:
    // ===== Component References =====

    UPROPERTY()
    UAgentVisualizationManager* VisualizationManager;

    UPROPERTY()
    UDeepTreeEchoRenderingSubsystem* RenderingSubsystem;

    // ===== Configuration =====

    UPROPERTY()
    FVisualizationBudget CurrentBudget;

    UPROPERTY()
    EVisualizationQualityPreset CurrentPreset;

    UPROPERTY()
    EOptimizationStrategy OptimizationStrategy;

    UPROPERTY()
    FAgentInstancingConfig InstancingConfig;

    // ===== Agent Priorities =====

    UPROPERTY()
    TMap<FString, FAgentRenderPriority> AgentPriorities;

    UPROPERTY()
    TMap<FString, float> AgentImportanceScores;

    // ===== LOD State =====

    int32 GlobalLODBias;
    bool bDynamicLODEnabled;
    float LODDistanceScale;
    int32 ForcedLODLevel;
    bool bLODForced;

    // ===== Culling State =====

    bool bFrustumCullingEnabled;
    bool bDistanceCullingEnabled;
    bool bOcclusionCullingEnabled;
    float MaxRenderDistance;

    UPROPERTY()
    TArray<FString> CulledAgents;

    // ===== Performance State =====

    UPROPERTY()
    FOptimizationReport LastReport;

    float FrameTimeAccumulator;
    int32 FrameCount;
    float AverageFrameTime;
    bool bOptimizationPaused;

    // ===== Effect State =====

    bool bParticleSystemsEnabled;
    bool bAuraEffectsEnabled;
    bool bShadowsEnabled;
    bool bReflectionsEnabled;

    // ===== Timers =====

    float OptimizationTimer;
    float CullingTimer;
    float LODTimer;
    float BudgetCheckTimer;
};
