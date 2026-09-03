#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AvatarAgentInterface.generated.h"

// Forward declarations
class UPersonalityTraitSystem;
class UNeurochemicalSimulationComponent;
class UAvatar9PServer;
class UAvatarAtomSpaceClient;
class UAvatarCommunicationManager;

/**
 * Agent State Types
 */
UENUM(BlueprintType)
enum class EAgentState : uint8
{
    Idle UMETA(DisplayName = "Idle"),
    Processing UMETA(DisplayName = "Processing"),
    Executing UMETA(DisplayName = "Executing Task"),
    Waiting UMETA(DisplayName = "Waiting for Input"),
    Communicating UMETA(DisplayName = "Communicating"),
    Learning UMETA(DisplayName = "Learning"),
    Sleeping UMETA(DisplayName = "Sleeping/Resting"),
    Error UMETA(DisplayName = "Error State")
};

/**
 * Task Priority
 */
UENUM(BlueprintType)
enum class ETaskPriority : uint8
{
    Background UMETA(DisplayName = "Background"),
    Low UMETA(DisplayName = "Low"),
    Normal UMETA(DisplayName = "Normal"),
    High UMETA(DisplayName = "High"),
    Critical UMETA(DisplayName = "Critical"),
    Emergency UMETA(DisplayName = "Emergency")
};

/**
 * Task Status
 */
UENUM(BlueprintType)
enum class ETaskStatus : uint8
{
    Pending UMETA(DisplayName = "Pending"),
    InProgress UMETA(DisplayName = "In Progress"),
    Completed UMETA(DisplayName = "Completed"),
    Failed UMETA(DisplayName = "Failed"),
    Cancelled UMETA(DisplayName = "Cancelled"),
    Blocked UMETA(DisplayName = "Blocked")
};

/**
 * Agent Capability Types
 */
UENUM(BlueprintType)
enum class EAgentCapability : uint8
{
    Visualization UMETA(DisplayName = "3D Visualization"),
    Communication UMETA(DisplayName = "Communication"),
    Reasoning UMETA(DisplayName = "Reasoning"),
    Learning UMETA(DisplayName = "Learning"),
    Memory UMETA(DisplayName = "Memory"),
    Emotion UMETA(DisplayName = "Emotional Expression"),
    Motion UMETA(DisplayName = "Motion/Animation"),
    Speech UMETA(DisplayName = "Speech"),
    Perception UMETA(DisplayName = "Perception")
};

/**
 * Agent Task structure
 */
USTRUCT(BlueprintType)
struct FAgentTask
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString TaskId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Description;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString AssignedBy; // Agent ID that assigned this task

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ETaskPriority Priority;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ETaskStatus Status;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FDateTime CreatedAt;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FDateTime Deadline;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TMap<FString, FString> Parameters;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> Dependencies;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Progress; // 0.0 - 1.0

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Result;

    FAgentTask()
        : Priority(ETaskPriority::Normal)
        , Status(ETaskStatus::Pending)
        , CreatedAt(FDateTime::Now())
        , Deadline(FDateTime::MaxValue())
        , Progress(0.0f)
    {}
};

/**
 * Perception structure for sharing sensory information
 */
USTRUCT(BlueprintType)
struct FAgentPerception
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString PerceptionId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString PerceiverAgentId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString PerceptionType; // visual, auditory, emotional, cognitive

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Content;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Confidence;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Salience;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FDateTime Timestamp;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector Location;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TMap<FString, FString> Metadata;

    FAgentPerception()
        : Confidence(1.0f)
        , Salience(0.5f)
        , Timestamp(FDateTime::Now())
        , Location(FVector::ZeroVector)
    {}
};

/**
 * Knowledge Graph structure for shared reasoning
 */
USTRUCT(BlueprintType)
struct FKnowledgeGraph
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString GraphId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> NodeIds;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TMap<FString, FString> NodeLabels;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> EdgeDescriptions; // "node1->node2:relation"

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Topic;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Confidence;

    FKnowledgeGraph()
        : Confidence(0.8f)
    {}
};

/**
 * Emotional Context for expression
 */
USTRUCT(BlueprintType)
struct FEmotionalContext
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Valence; // -1 to 1 (negative to positive)

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Arousal; // 0 to 1 (calm to excited)

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Dominance; // 0 to 1 (submissive to dominant)

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString PrimaryEmotion;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> SecondaryEmotions;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Intensity;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Cause;

    FEmotionalContext()
        : Valence(0.0f)
        , Arousal(0.5f)
        , Dominance(0.5f)
        , PrimaryEmotion(TEXT("neutral"))
        , Intensity(0.5f)
    {}
};

/**
 * Swarm Goal for collective action
 */
USTRUCT(BlueprintType)
struct FSwarmGoal
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString GoalId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Description;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> ParticipatingAgents;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FAgentTask> SubTasks;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float OverallProgress;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FDateTime Deadline;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString CoordinatorAgentId;

    FSwarmGoal()
        : OverallProgress(0.0f)
        , Deadline(FDateTime::MaxValue())
    {}
};

/**
 * Agent Info for discovery
 */
USTRUCT(BlueprintType)
struct FAgentInfo
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString AgentId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString AgentName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString AgentType;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<EAgentCapability> Capabilities;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EAgentState CurrentState;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString NamespacePath; // 9P path

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsOnline;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FDateTime LastSeen;

    FAgentInfo()
        : AgentType(TEXT("avatar"))
        , CurrentState(EAgentState::Idle)
        , bIsOnline(true)
        , LastSeen(FDateTime::Now())
    {}
};

/**
 * Delegates for agent events
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTaskReceived, const FAgentTask&, Task);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTaskCompleted, const FString&, TaskId, bool, bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPerceptionShared, const FAgentPerception&, Perception);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAgentDiscovered, const FAgentInfo&, Agent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSwarmGoalUpdated, const FString&, GoalId, float, Progress);

/**
 * Avatar Agent Interface Component
 * Implements ElizaOS agent protocol for multi-agent coordination
 *
 * Agent Swarm Architecture:
 * /mnt/agents/
 * ├── deep-tree-echo/          # This avatar
 * │   ├── capabilities
 * │   ├── state
 * │   ├── tasks
 * │   └── perception
 * ├── eliza-1/                 # Other agents
 * ├── eliza-2/
 * └── swarm/
 *     ├── coordination
 *     ├── shared_goals
 *     └── collective_memory
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UNREALENGINE_API UAvatarAgentInterface : public UActorComponent
{
    GENERATED_BODY()

public:
    UAvatarAgentInterface();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

    // ===== Agent Identity =====

    UFUNCTION(BlueprintCallable, Category = "Agent|Identity")
    FAgentInfo GetAgentInfo() const { return AgentInfo; }

    UFUNCTION(BlueprintCallable, Category = "Agent|Identity")
    void SetAgentName(const FString& Name);

    UFUNCTION(BlueprintCallable, Category = "Agent|Identity")
    EAgentState GetState() const { return CurrentState; }

    UFUNCTION(BlueprintCallable, Category = "Agent|Identity")
    void SetState(EAgentState NewState);

    UFUNCTION(BlueprintCallable, Category = "Agent|Identity")
    TArray<EAgentCapability> GetCapabilities() const;

    // ===== Task Management =====

    UFUNCTION(BlueprintCallable, Category = "Agent|Tasks")
    void ReceiveTask(const FAgentTask& Task);

    UFUNCTION(BlueprintCallable, Category = "Agent|Tasks")
    void UpdateTaskProgress(const FString& TaskId, float Progress);

    UFUNCTION(BlueprintCallable, Category = "Agent|Tasks")
    void CompleteTask(const FString& TaskId, bool bSuccess, const FString& Result);

    UFUNCTION(BlueprintCallable, Category = "Agent|Tasks")
    void CancelTask(const FString& TaskId);

    UFUNCTION(BlueprintCallable, Category = "Agent|Tasks")
    FAgentTask GetTask(const FString& TaskId);

    UFUNCTION(BlueprintCallable, Category = "Agent|Tasks")
    TArray<FAgentTask> GetPendingTasks();

    UFUNCTION(BlueprintCallable, Category = "Agent|Tasks")
    TArray<FAgentTask> GetActiveTasks();

    UFUNCTION(BlueprintCallable, Category = "Agent|Tasks")
    void AssignTaskToAgent(const FString& TargetAgentId, const FAgentTask& Task);

    // ===== Perception Sharing =====

    UFUNCTION(BlueprintCallable, Category = "Agent|Perception")
    void SharePerception(const FAgentPerception& Perception);

    UFUNCTION(BlueprintCallable, Category = "Agent|Perception")
    void ReceivePerception(const FAgentPerception& Perception);

    UFUNCTION(BlueprintCallable, Category = "Agent|Perception")
    TArray<FAgentPerception> GetRecentPerceptions(int32 Count = 10);

    UFUNCTION(BlueprintCallable, Category = "Agent|Perception")
    void BroadcastPerception(const FAgentPerception& Perception);

    // ===== Knowledge Sharing =====

    UFUNCTION(BlueprintCallable, Category = "Agent|Knowledge")
    void ShareKnowledge(const FKnowledgeGraph& Graph);

    UFUNCTION(BlueprintCallable, Category = "Agent|Knowledge")
    void VisualizeSharedKnowledge(const FKnowledgeGraph& Graph);

    UFUNCTION(BlueprintCallable, Category = "Agent|Knowledge")
    FKnowledgeGraph QuerySharedKnowledge(const FString& Topic);

    // ===== Emotional Expression =====

    UFUNCTION(BlueprintCallable, Category = "Agent|Emotion")
    void ExpressEmotionalState(const FEmotionalContext& Context);

    UFUNCTION(BlueprintCallable, Category = "Agent|Emotion")
    FEmotionalContext GetCurrentEmotionalContext();

    UFUNCTION(BlueprintCallable, Category = "Agent|Emotion")
    void SynchronizeEmotionalState(const FString& TargetAgentId);

    // ===== Agent Discovery =====

    UFUNCTION(BlueprintCallable, Category = "Agent|Discovery")
    void DiscoverAgents();

    UFUNCTION(BlueprintCallable, Category = "Agent|Discovery")
    TArray<FAgentInfo> GetKnownAgents();

    UFUNCTION(BlueprintCallable, Category = "Agent|Discovery")
    FAgentInfo GetAgentById(const FString& AgentId);

    UFUNCTION(BlueprintCallable, Category = "Agent|Discovery")
    bool IsAgentOnline(const FString& AgentId);

    UFUNCTION(BlueprintCallable, Category = "Agent|Discovery")
    void PingAgent(const FString& AgentId);

    // ===== Swarm Coordination =====

    UFUNCTION(BlueprintCallable, Category = "Agent|Swarm")
    void JoinSwarmGoal(const FString& GoalId);

    UFUNCTION(BlueprintCallable, Category = "Agent|Swarm")
    void LeaveSwarmGoal(const FString& GoalId);

    UFUNCTION(BlueprintCallable, Category = "Agent|Swarm")
    void ProposeSwarmGoal(const FSwarmGoal& Goal);

    UFUNCTION(BlueprintCallable, Category = "Agent|Swarm")
    FSwarmGoal GetSwarmGoal(const FString& GoalId);

    UFUNCTION(BlueprintCallable, Category = "Agent|Swarm")
    TArray<FSwarmGoal> GetActiveSwarmGoals();

    UFUNCTION(BlueprintCallable, Category = "Agent|Swarm")
    void ReportSwarmProgress(const FString& GoalId, float Progress);

    // ===== Collective Reasoning =====

    UFUNCTION(BlueprintCallable, Category = "Agent|Reasoning")
    void ContributeToReasoning(const FString& Topic, const FString& Contribution);

    UFUNCTION(BlueprintCallable, Category = "Agent|Reasoning")
    FString QueryCollectiveReasoning(const FString& Topic);

    UFUNCTION(BlueprintCallable, Category = "Agent|Reasoning")
    void VoteOnProposal(const FString& ProposalId, bool bApprove);

    // ===== 9P Namespace =====

    UFUNCTION(BlueprintCallable, Category = "Agent|9P")
    void Register9PNamespace();

    UFUNCTION(BlueprintCallable, Category = "Agent|9P")
    FString Handle9PRead(const FString& Path);

    UFUNCTION(BlueprintCallable, Category = "Agent|9P")
    bool Handle9PWrite(const FString& Path, const FString& Data);

    UFUNCTION(BlueprintCallable, Category = "Agent|9P")
    FString GetNamespacePath() const { return NamespacePath; }

    // ===== Avatar-Specific Capabilities =====

    UFUNCTION(BlueprintCallable, Category = "Agent|Avatar")
    void VisualizeAgentInteraction(const FString& OtherAgentId);

    UFUNCTION(BlueprintCallable, Category = "Agent|Avatar")
    void AnimateForTask(const FAgentTask& Task);

    UFUNCTION(BlueprintCallable, Category = "Agent|Avatar")
    void DisplayEmotionalResponse(const FEmotionalContext& Context);

    // ===== Events =====

    UPROPERTY(BlueprintAssignable, Category = "Agent|Events")
    FOnTaskReceived OnTaskReceived;

    UPROPERTY(BlueprintAssignable, Category = "Agent|Events")
    FOnTaskCompleted OnTaskCompleted;

    UPROPERTY(BlueprintAssignable, Category = "Agent|Events")
    FOnPerceptionShared OnPerceptionShared;

    UPROPERTY(BlueprintAssignable, Category = "Agent|Events")
    FOnAgentDiscovered OnAgentDiscovered;

    UPROPERTY(BlueprintAssignable, Category = "Agent|Events")
    FOnSwarmGoalUpdated OnSwarmGoalUpdated;

protected:
    // ===== Internal Methods =====

    void ProcessTaskQueue();
    void UpdateAgentState();
    void HeartbeatToSwarm();
    void ProcessReceivedPerceptions();
    FString SerializeAgentInfo();
    FAgentInfo DeserializeAgentInfo(const FString& Data);

private:
    // ===== Component References =====

    UPROPERTY()
    UPersonalityTraitSystem* PersonalitySystem;

    UPROPERTY()
    UNeurochemicalSimulationComponent* NeurochemicalSystem;

    UPROPERTY()
    UAvatar9PServer* P9Server;

    UPROPERTY()
    UAvatarAtomSpaceClient* AtomSpaceClient;

    UPROPERTY()
    UAvatarCommunicationManager* CommunicationManager;

    // ===== Agent Identity =====

    UPROPERTY()
    FAgentInfo AgentInfo;

    UPROPERTY()
    EAgentState CurrentState;

    UPROPERTY()
    FString NamespacePath;

    // ===== Task Management =====

    UPROPERTY()
    TArray<FAgentTask> TaskQueue;

    UPROPERTY()
    TArray<FAgentTask> ActiveTasks;

    UPROPERTY()
    TArray<FAgentTask> CompletedTasks;

    // ===== Perception =====

    UPROPERTY()
    TArray<FAgentPerception> ReceivedPerceptions;

    UPROPERTY()
    TArray<FAgentPerception> SentPerceptions;

    // ===== Agent Network =====

    UPROPERTY()
    TMap<FString, FAgentInfo> KnownAgents;

    UPROPERTY()
    TMap<FString, FSwarmGoal> ActiveSwarmGoals;

    // ===== Configuration =====

    UPROPERTY(EditAnywhere, Category = "Agent|Configuration")
    float HeartbeatInterval;

    UPROPERTY(EditAnywhere, Category = "Agent|Configuration")
    float TaskProcessingInterval;

    UPROPERTY(EditAnywhere, Category = "Agent|Configuration")
    int32 MaxConcurrentTasks;

    UPROPERTY(EditAnywhere, Category = "Agent|Configuration")
    bool bAutoDiscoverAgents;

    UPROPERTY(EditAnywhere, Category = "Agent|Configuration")
    bool bVerboseLogging;

    // ===== Timers =====

    float HeartbeatTimer;
    float TaskProcessingTimer;
    float DiscoveryTimer;
};
