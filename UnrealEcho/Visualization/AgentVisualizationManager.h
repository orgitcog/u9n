#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AgentVisualizationManager.generated.h"

// Forward declarations
class AAgentAvatarActor;
class USharedVisualizationSpace;
class UAvatar9PServer;
class UDeepTreeEchoRenderingSubsystem;

/**
 * Agent Visualization State
 * Represents the visual state of an AGI-OS agent
 */
USTRUCT(BlueprintType)
struct FAgentVisualizationState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString AgentPath; // 9P path: /mnt/agents/{agent-name}

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString AgentName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString AgentType; // "deep-tree-echo", "eliza", "generic"

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector WorldPosition;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FRotator WorldRotation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor AuraColor;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float AuraIntensity;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float AttentionLevel; // 0-1, how focused the agent is

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ActivityLevel; // 0-1, how active the agent is

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TMap<FString, float> EmotionalState; // emotion -> intensity

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> ActiveCapabilities;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsOnline;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FDateTime LastUpdate;

    FAgentVisualizationState()
        : WorldPosition(FVector::ZeroVector)
        , WorldRotation(FRotator::ZeroRotator)
        , AuraColor(FLinearColor::White)
        , AuraIntensity(1.0f)
        , AttentionLevel(0.5f)
        , ActivityLevel(0.5f)
        , bIsOnline(false)
        , LastUpdate(FDateTime::Now())
    {}
};

/**
 * Agent Visual Style Configuration
 */
USTRUCT(BlueprintType)
struct FAgentVisualStyle
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString StyleName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftObjectPtr<USkeletalMesh> AvatarMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftObjectPtr<UAnimBlueprint> AnimationBlueprint;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftObjectPtr<UMaterialInterface> BaseMaterial;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor DefaultAuraColor;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float DefaultScale;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bEnableDetailedFacialExpressions;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bEnablePhysicsSimulation;

    FAgentVisualStyle()
        : DefaultAuraColor(FLinearColor(0.2f, 0.6f, 1.0f, 1.0f))
        , DefaultScale(1.0f)
        , bEnableDetailedFacialExpressions(true)
        , bEnablePhysicsSimulation(true)
    {}
};

/**
 * Communication Link between agents
 */
USTRUCT(BlueprintType)
struct FCommunicationLink
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString LinkId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString SourceAgentPath;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString TargetAgentPath;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Strength; // 0-1, communication intensity

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor LinkColor;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString MessageType; // "query", "response", "broadcast", "private"

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ActivityPulse; // 0-1, current activity on this link

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MessageCount;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FDateTime LastActivity;

    FCommunicationLink()
        : Strength(0.5f)
        , LinkColor(FLinearColor(0.4f, 0.8f, 1.0f, 0.7f))
        , ActivityPulse(0.0f)
        , MessageCount(0)
        , LastActivity(FDateTime::Now())
    {}
};

/**
 * Performance metrics for visualization
 */
USTRUCT(BlueprintType)
struct FVisualizationPerformanceMetrics
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    int32 ActiveAgentCount;

    UPROPERTY(BlueprintReadOnly)
    int32 ActiveLinkCount;

    UPROPERTY(BlueprintReadOnly)
    float CurrentFPS;

    UPROPERTY(BlueprintReadOnly)
    float AverageFrameTime; // ms

    UPROPERTY(BlueprintReadOnly)
    float GPUTime; // ms

    UPROPERTY(BlueprintReadOnly)
    float CPUTime; // ms

    UPROPERTY(BlueprintReadOnly)
    float MemoryUsageMB;

    UPROPERTY(BlueprintReadOnly)
    float StateUpdateLatency; // ms for 9P updates

    UPROPERTY(BlueprintReadOnly)
    bool bIsPerformanceOptimal; // Maintaining target FPS

    FVisualizationPerformanceMetrics()
        : ActiveAgentCount(0)
        , ActiveLinkCount(0)
        , CurrentFPS(60.0f)
        , AverageFrameTime(16.67f)
        , GPUTime(8.0f)
        , CPUTime(4.0f)
        , MemoryUsageMB(0.0f)
        , StateUpdateLatency(1.0f)
        , bIsPerformanceOptimal(true)
    {}
};

/**
 * LOD Configuration for agent avatars
 */
USTRUCT(BlueprintType)
struct FAgentLODSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float LOD0Distance; // Full detail

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float LOD1Distance; // Reduced detail

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float LOD2Distance; // Minimal detail

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CullDistance; // Hide entirely

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bEnableDynamicLOD;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float LODTransitionSpeed;

    FAgentLODSettings()
        : LOD0Distance(500.0f)
        , LOD1Distance(1500.0f)
        , LOD2Distance(3000.0f)
        , CullDistance(5000.0f)
        , bEnableDynamicLOD(true)
        , LODTransitionSpeed(2.0f)
    {}
};

/**
 * Delegates for visualization events
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAgentVisualized, const FString&, AgentPath, AAgentAvatarActor*, Avatar);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAgentRemoved, const FString&, AgentPath);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAgentStateUpdated, const FString&, AgentPath, const FAgentVisualizationState&, State);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCommunicationLinkCreated, const FCommunicationLink&, Link);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPerformanceThresholdCrossed, const FVisualizationPerformanceMetrics&, Metrics);

/**
 * Agent Visualization Manager Component
 *
 * Manages multi-agent visualization in Unreal Engine for AGI-OS integration.
 *
 * Core Features:
 * - Create/destroy visualizations for any AGI-OS agent
 * - Synchronize agent state via 9P protocol
 * - Manage communication link visualization
 * - Performance optimization for 5+ simultaneous agents
 * - LOD system for scalable rendering
 * - Shared visualization space management
 *
 * Target Performance:
 * - 60+ FPS with 5+ active agents
 * - <5ms 9P latency for state updates
 * - <50MB memory overhead for visualization
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UNREALENGINE_API UAgentVisualizationManager : public UActorComponent
{
    GENERATED_BODY()

public:
    UAgentVisualizationManager();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

    // ===== Agent Visualization Creation =====

    /**
     * Create visualization for any AGI-OS agent
     * @param AgentPath 9P path to the agent (e.g., /mnt/agents/eliza-1)
     * @param Style Optional visual style override
     * @return Spawned avatar actor
     */
    UFUNCTION(BlueprintCallable, Category = "AgentVisualization|Create")
    AAgentAvatarActor* CreateAgentVisualization(const FString& AgentPath,
                                                 const FAgentVisualStyle& Style);

    /**
     * Create visualization with default style based on agent type
     */
    UFUNCTION(BlueprintCallable, Category = "AgentVisualization|Create")
    AAgentAvatarActor* CreateAgentVisualizationAuto(const FString& AgentPath);

    /**
     * Remove agent visualization
     */
    UFUNCTION(BlueprintCallable, Category = "AgentVisualization|Create")
    void RemoveAgentVisualization(const FString& AgentPath);

    /**
     * Remove all agent visualizations
     */
    UFUNCTION(BlueprintCallable, Category = "AgentVisualization|Create")
    void RemoveAllAgentVisualizations();

    // ===== State Synchronization =====

    /**
     * Update agent appearance from 9P state
     */
    UFUNCTION(BlueprintCallable, Category = "AgentVisualization|State")
    void UpdateAgentFromState(AAgentAvatarActor* Avatar, const FString& AgentPath);

    /**
     * Force immediate state sync for all agents
     */
    UFUNCTION(BlueprintCallable, Category = "AgentVisualization|State")
    void SyncAllAgentStates();

    /**
     * Get current visualization state for an agent
     */
    UFUNCTION(BlueprintCallable, Category = "AgentVisualization|State")
    FAgentVisualizationState GetAgentVisualizationState(const FString& AgentPath);

    /**
     * Set state update interval
     */
    UFUNCTION(BlueprintCallable, Category = "AgentVisualization|State")
    void SetStateUpdateInterval(float IntervalSeconds);

    // ===== Shared Visualization Space =====

    /**
     * Create shared visualization space for multiple agents
     */
    UFUNCTION(BlueprintCallable, Category = "AgentVisualization|Shared")
    void CreateSharedSpace(const TArray<FString>& AgentPaths);

    /**
     * Add agent to existing shared space
     */
    UFUNCTION(BlueprintCallable, Category = "AgentVisualization|Shared")
    void AddAgentToSharedSpace(const FString& AgentPath);

    /**
     * Remove agent from shared space
     */
    UFUNCTION(BlueprintCallable, Category = "AgentVisualization|Shared")
    void RemoveAgentFromSharedSpace(const FString& AgentPath);

    /**
     * Get shared visualization space component
     */
    UFUNCTION(BlueprintCallable, Category = "AgentVisualization|Shared")
    USharedVisualizationSpace* GetSharedSpace();

    // ===== Communication Link Visualization =====

    /**
     * Create communication link between two agents
     */
    UFUNCTION(BlueprintCallable, Category = "AgentVisualization|Links")
    void CreateCommunicationLink(const FString& SourcePath, const FString& TargetPath,
                                  const FString& MessageType = TEXT("query"));

    /**
     * Update link activity (pulse effect)
     */
    UFUNCTION(BlueprintCallable, Category = "AgentVisualization|Links")
    void PulseCommunicationLink(const FString& SourcePath, const FString& TargetPath,
                                 float Intensity = 1.0f);

    /**
     * Remove communication link
     */
    UFUNCTION(BlueprintCallable, Category = "AgentVisualization|Links")
    void RemoveCommunicationLink(const FString& SourcePath, const FString& TargetPath);

    /**
     * Get all active communication links
     */
    UFUNCTION(BlueprintCallable, Category = "AgentVisualization|Links")
    TArray<FCommunicationLink> GetActiveCommunicationLinks();

    // ===== Agent Positioning =====

    /**
     * Arrange agents in a circle around center
     */
    UFUNCTION(BlueprintCallable, Category = "AgentVisualization|Layout")
    void ArrangeAgentsInCircle(FVector Center, float Radius);

    /**
     * Arrange agents in a grid
     */
    UFUNCTION(BlueprintCallable, Category = "AgentVisualization|Layout")
    void ArrangeAgentsInGrid(FVector Origin, float Spacing, int32 Columns);

    /**
     * Set agent world position
     */
    UFUNCTION(BlueprintCallable, Category = "AgentVisualization|Layout")
    void SetAgentPosition(const FString& AgentPath, FVector Position);

    /**
     * Focus camera on specific agent
     */
    UFUNCTION(BlueprintCallable, Category = "AgentVisualization|Layout")
    void FocusOnAgent(const FString& AgentPath, float TransitionTime = 1.0f);

    // ===== Performance Optimization =====

    /**
     * Get current performance metrics
     */
    UFUNCTION(BlueprintCallable, Category = "AgentVisualization|Performance")
    FVisualizationPerformanceMetrics GetPerformanceMetrics();

    /**
     * Set LOD settings
     */
    UFUNCTION(BlueprintCallable, Category = "AgentVisualization|Performance")
    void SetLODSettings(const FAgentLODSettings& Settings);

    /**
     * Enable/disable performance optimization mode
     */
    UFUNCTION(BlueprintCallable, Category = "AgentVisualization|Performance")
    void SetPerformanceOptimizationEnabled(bool bEnabled);

    /**
     * Set target FPS for optimization
     */
    UFUNCTION(BlueprintCallable, Category = "AgentVisualization|Performance")
    void SetTargetFPS(int32 TargetFPS);

    /**
     * Get maximum recommended agent count for current hardware
     */
    UFUNCTION(BlueprintCallable, Category = "AgentVisualization|Performance")
    int32 GetMaxRecommendedAgentCount();

    // ===== Visual Style Management =====

    /**
     * Register visual style for agent type
     */
    UFUNCTION(BlueprintCallable, Category = "AgentVisualization|Style")
    void RegisterVisualStyle(const FString& AgentType, const FAgentVisualStyle& Style);

    /**
     * Get visual style for agent type
     */
    UFUNCTION(BlueprintCallable, Category = "AgentVisualization|Style")
    FAgentVisualStyle GetVisualStyleForType(const FString& AgentType);

    /**
     * Set global aura intensity multiplier
     */
    UFUNCTION(BlueprintCallable, Category = "AgentVisualization|Style")
    void SetGlobalAuraIntensity(float Intensity);

    // ===== Agent Queries =====

    /**
     * Get all visualized agents
     */
    UFUNCTION(BlueprintCallable, Category = "AgentVisualization|Query")
    TArray<FString> GetAllVisualizedAgentPaths();

    /**
     * Get avatar actor for agent
     */
    UFUNCTION(BlueprintCallable, Category = "AgentVisualization|Query")
    AAgentAvatarActor* GetAvatarForAgent(const FString& AgentPath);

    /**
     * Check if agent is currently visualized
     */
    UFUNCTION(BlueprintCallable, Category = "AgentVisualization|Query")
    bool IsAgentVisualized(const FString& AgentPath);

    /**
     * Get agent count
     */
    UFUNCTION(BlueprintPure, Category = "AgentVisualization|Query")
    int32 GetActiveAgentCount() const { return VisualizedAgents.Num(); }

    // ===== Events =====

    UPROPERTY(BlueprintAssignable, Category = "AgentVisualization|Events")
    FOnAgentVisualized OnAgentVisualized;

    UPROPERTY(BlueprintAssignable, Category = "AgentVisualization|Events")
    FOnAgentRemoved OnAgentRemoved;

    UPROPERTY(BlueprintAssignable, Category = "AgentVisualization|Events")
    FOnAgentStateUpdated OnAgentStateUpdated;

    UPROPERTY(BlueprintAssignable, Category = "AgentVisualization|Events")
    FOnCommunicationLinkCreated OnCommunicationLinkCreated;

    UPROPERTY(BlueprintAssignable, Category = "AgentVisualization|Events")
    FOnPerformanceThresholdCrossed OnPerformanceThresholdCrossed;

protected:
    // ===== Internal Methods =====

    void InitializeDefaultStyles();
    FAgentVisualizationState FetchStateFrom9P(const FString& AgentPath);
    void ApplyStateToAvatar(AAgentAvatarActor* Avatar, const FAgentVisualizationState& State);
    void UpdateCommunicationLinks(float DeltaTime);
    void UpdatePerformanceMetrics(float DeltaTime);
    void OptimizeForCurrentLoad();
    void UpdateAgentLODs();
    FString DetermineAgentType(const FString& AgentPath);

private:
    // ===== Component References =====

    UPROPERTY()
    UAvatar9PServer* Server9P;

    UPROPERTY()
    USharedVisualizationSpace* SharedSpace;

    UPROPERTY()
    UDeepTreeEchoRenderingSubsystem* RenderingSubsystem;

    // ===== Visualized Agents =====

    UPROPERTY()
    TMap<FString, AAgentAvatarActor*> VisualizedAgents;

    UPROPERTY()
    TMap<FString, FAgentVisualizationState> AgentStates;

    // ===== Communication Links =====

    UPROPERTY()
    TArray<FCommunicationLink> CommunicationLinks;

    // ===== Visual Styles =====

    UPROPERTY()
    TMap<FString, FAgentVisualStyle> RegisteredStyles;

    // ===== Performance =====

    UPROPERTY()
    FVisualizationPerformanceMetrics CurrentMetrics;

    UPROPERTY()
    FAgentLODSettings LODSettings;

    // ===== Configuration =====

    UPROPERTY(EditAnywhere, Category = "AgentVisualization|Configuration")
    float StateUpdateInterval;

    UPROPERTY(EditAnywhere, Category = "AgentVisualization|Configuration")
    int32 TargetFPS;

    UPROPERTY(EditAnywhere, Category = "AgentVisualization|Configuration")
    int32 MaxAgentCount;

    UPROPERTY(EditAnywhere, Category = "AgentVisualization|Configuration")
    bool bEnablePerformanceOptimization;

    UPROPERTY(EditAnywhere, Category = "AgentVisualization|Configuration")
    float GlobalAuraIntensityMultiplier;

    UPROPERTY(EditAnywhere, Category = "AgentVisualization|Configuration")
    bool bVerboseLogging;

    // ===== Timers =====

    float StateUpdateTimer;
    float PerformanceCheckTimer;
    float LODUpdateTimer;
};
