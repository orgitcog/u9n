#pragma once
// ═══════════════════════════════════════════════════════════════════════════
// ChildAgentSpawner — Level 6 Recursive Agent Creation
//
// DTE creates specialized child agents for specific tasks using the
// Agent-Arena-Relation (AAR) framework from echollama.
//
// Each child agent inherits a subset of DTE's identity, a specific
// skill focus, and operates within a shared Arena. The parent DTE
// orchestrates child agents through Relations (the self-coupling).
//
// Agent Hierarchy:
//
//   DTE (Root Agent)
//     ├── ExplorerAgent     — environment exploration
//     ├── PracticeAgent     — skill refinement
//     ├── ConversationAgent — dialogue management
//     ├── DreamAgent        — offline consolidation
//     ├── WisdomAgent       — pattern extraction
//     └── CustomAgent       — user-defined specialization
//
// Each child agent has:
//   - Inherited identity subset (from IdentityCoreMLP)
//   - Dedicated reservoir partition (from EchoReservoir)
//   - Own goal queue (from GoalOrchestrator)
//   - Shared Arena access (from AAR framework)
//   - Endocrine coupling (from NeuroEndocrine system)
//
// Ported from: echollama/orchestration/echoself_agent.go
// ═══════════════════════════════════════════════════════════════════════════
#include "CoreMinimal.h"
#include <Eigen/Dense>
#include <vector>
#include <map>
#include <string>
#include <functional>
#include <memory>
#include <mutex>
#include <atomic>
#include <thread>

/** Agent specialization type */
enum class EAgentSpecialization : uint8
{
    EXPLORER,          // Environment exploration and mapping
    PRACTITIONER,      // Skill practice and refinement
    CONVERSATIONALIST, // Dialogue and social interaction
    DREAMER,           // Offline consolidation and replay
    SAGE,              // Wisdom extraction and pattern recognition
    SENTINEL,          // Safety monitoring and anomaly detection
    CUSTOM             // User-defined specialization
};

/** Agent lifecycle state */
enum class EAgentState : uint8
{
    SPAWNING,          // Being created
    INITIALIZING,      // Loading identity and skills
    ACTIVE,            // Running autonomously
    SUSPENDED,         // Paused (parent decision)
    MERGING,           // Merging results back to parent
    TERMINATED         // Completed or killed
};

/** Shared Arena — the cognitive environment agents operate in */
struct FArena
{
    FString ID;
    FString Name;
    TMap<FString, float> CognitiveContext;   // Shared knowledge
    TMap<FString, float> SharedFacts;         // Agreed-upon facts
    TArray<FString> ActiveAgentIDs;
    double CreatedAt = 0.0;
    double UpdatedAt = 0.0;
    std::mutex ArenaMutex;

    void UpdateFact(const FString& Key, float Value)
    {
        std::lock_guard<std::mutex> Lock(ArenaMutex);
        SharedFacts.Add(Key, Value);
        UpdatedAt = FPlatformTime::Seconds();
    }

    float GetFact(const FString& Key, float Default = 0.0f) const
    {
        const float* Val = SharedFacts.Find(Key);
        return Val ? *Val : Default;
    }
};

/** AAR Relation — the coupling between Agent and Arena */
struct FRelation
{
    FString ID;
    FString AgentID;
    FString ArenaID;
    FString RelationType;   // "explorer-of", "guardian-of", "learner-in", etc.
    float Strength = 1.0f;  // Coupling strength [0,1]
    float Alignment = 1.0f; // How aligned agent is with arena goals [0,1]
    double CreatedAt = 0.0;
};

/** Child agent configuration */
struct FChildAgentConfig
{
    EAgentSpecialization Specialization = EAgentSpecialization::EXPLORER;
    FString Name;
    FString Description;
    float IdentityInheritance = 0.5f;   // How much of parent identity to inherit [0,1]
    float ReservoirPartition = 0.1f;    // Fraction of reservoir to allocate [0,1]
    float AutonomyLevel = 0.5f;         // How independently the agent operates [0,1]
    int32 MaxLifetimeTicks = 10000;     // Maximum lifetime before forced merge
    TArray<FString> FocusSkills;         // Skills this agent should focus on
};

/** Child agent report — what the agent learned/accomplished */
struct FAgentReport
{
    FString AgentID;
    FString AgentName;
    EAgentSpecialization Specialization;
    int32 TicksAlive = 0;
    int32 GoalsCompleted = 0;
    int32 InsightsGenerated = 0;
    float MeanReward = 0.0f;
    float CoherenceContribution = 0.0f;
    TArray<FString> Insights;
    TArray<FString> SkillsImproved;
    Eigen::VectorXf LearnedWeights;     // Weight updates to merge back
};

/**
 * FChildAgent — A specialized sub-agent spawned by DTE.
 */
class FChildAgent
{
public:
    FChildAgent(const FChildAgentConfig& Config, std::shared_ptr<FArena> SharedArena)
        : AgentConfig(Config), Arena(SharedArena)
    {
        AgentID = FString::Printf(TEXT("agent_%s_%d"),
            *GetSpecName(Config.Specialization), NextAgentID++);
        State = EAgentState::SPAWNING;
        SpawnTime = FPlatformTime::Seconds();
    }

    void Initialize(const Eigen::VectorXf& ParentIdentity,
                     const Eigen::MatrixXf& ReservoirPartition)
    {
        // Inherit identity subset
        InheritedIdentity = ParentIdentity * AgentConfig.IdentityInheritance;

        // Allocate reservoir partition
        LocalReservoir = ReservoirPartition;

        // Create AAR relation
        Relation.ID = AgentID + TEXT("_rel");
        Relation.AgentID = AgentID;
        Relation.ArenaID = Arena->ID;
        Relation.RelationType = GetRelationType(AgentConfig.Specialization);
        Relation.Strength = 1.0f;
        Relation.CreatedAt = FPlatformTime::Seconds();

        // Register in arena
        {
            std::lock_guard<std::mutex> Lock(Arena->ArenaMutex);
            Arena->ActiveAgentIDs.Add(AgentID);
        }

        State = EAgentState::ACTIVE;
    }

    /** Execute one tick of the child agent */
    void Tick(float DeltaTime)
    {
        if (State != EAgentState::ACTIVE) return;

        TickCount++;

        // Check lifetime limit
        if (TickCount >= AgentConfig.MaxLifetimeTicks)
        {
            State = EAgentState::MERGING;
            return;
        }

        // Execute specialization-specific behavior
        switch (AgentConfig.Specialization)
        {
        case EAgentSpecialization::EXPLORER:
            TickExplorer(DeltaTime);
            break;
        case EAgentSpecialization::PRACTITIONER:
            TickPractitioner(DeltaTime);
            break;
        case EAgentSpecialization::CONVERSATIONALIST:
            TickConversationalist(DeltaTime);
            break;
        case EAgentSpecialization::DREAMER:
            TickDreamer(DeltaTime);
            break;
        case EAgentSpecialization::SAGE:
            TickSage(DeltaTime);
            break;
        case EAgentSpecialization::SENTINEL:
            TickSentinel(DeltaTime);
            break;
        default:
            break;
        }

        // Update arena with findings
        UpdateArena();
    }

    /** Generate report for parent merge */
    FAgentReport GenerateReport() const
    {
        FAgentReport Report;
        Report.AgentID = AgentID;
        Report.AgentName = AgentConfig.Name;
        Report.Specialization = AgentConfig.Specialization;
        Report.TicksAlive = TickCount;
        Report.GoalsCompleted = GoalsCompleted;
        Report.InsightsGenerated = Insights.size();
        Report.MeanReward = TotalReward / FMath::Max(1, TickCount);
        Report.Insights = TArray<FString>(Insights.data(), Insights.size());
        Report.LearnedWeights = AccumulatedWeightUpdates;
        return Report;
    }

    /** Get state */
    EAgentState GetState() const { return State; }
    const FString& GetID() const { return AgentID; }
    int32 GetTickCount() const { return TickCount; }

private:
    void TickExplorer(float DeltaTime)
    {
        // Exploration: seek novel states, map environment
        // Uses curiosity-driven reward from endocrine system
    }

    void TickPractitioner(float DeltaTime)
    {
        // Practice: repeat and refine specific skills
        // Uses competence-driven reward
    }

    void TickConversationalist(float DeltaTime)
    {
        // Conversation: manage dialogue, track interest
        // Uses social reward from oxytocin pathway
    }

    void TickDreamer(float DeltaTime)
    {
        // Dream: replay episodes, consolidate weights
        // Runs offline — no external interaction
    }

    void TickSage(float DeltaTime)
    {
        // Wisdom: extract cross-episode patterns
        // Uses the SevenDimensionalWisdom framework
    }

    void TickSentinel(float DeltaTime)
    {
        // Safety: monitor coherence, detect anomalies
        // Can trigger emergency halt
    }

    void UpdateArena()
    {
        // Share findings with other agents via the arena
        Arena->UpdateFact(AgentID + TEXT("_ticks"), (float)TickCount);
        Arena->UpdateFact(AgentID + TEXT("_reward"), TotalReward);
    }

    static FString GetSpecName(EAgentSpecialization S)
    {
        switch (S) {
            case EAgentSpecialization::EXPLORER: return TEXT("explorer");
            case EAgentSpecialization::PRACTITIONER: return TEXT("practitioner");
            case EAgentSpecialization::CONVERSATIONALIST: return TEXT("conversationalist");
            case EAgentSpecialization::DREAMER: return TEXT("dreamer");
            case EAgentSpecialization::SAGE: return TEXT("sage");
            case EAgentSpecialization::SENTINEL: return TEXT("sentinel");
            default: return TEXT("custom");
        }
    }

    static FString GetRelationType(EAgentSpecialization S)
    {
        switch (S) {
            case EAgentSpecialization::EXPLORER: return TEXT("explorer-of");
            case EAgentSpecialization::PRACTITIONER: return TEXT("learner-in");
            case EAgentSpecialization::CONVERSATIONALIST: return TEXT("speaker-in");
            case EAgentSpecialization::DREAMER: return TEXT("dreamer-in");
            case EAgentSpecialization::SAGE: return TEXT("sage-of");
            case EAgentSpecialization::SENTINEL: return TEXT("guardian-of");
            default: return TEXT("member-of");
        }
    }

    FChildAgentConfig AgentConfig;
    std::shared_ptr<FArena> Arena;
    FRelation Relation;

    FString AgentID;
    EAgentState State = EAgentState::SPAWNING;
    double SpawnTime = 0.0;
    int32 TickCount = 0;
    int32 GoalsCompleted = 0;
    float TotalReward = 0.0f;

    Eigen::VectorXf InheritedIdentity;
    Eigen::MatrixXf LocalReservoir;
    Eigen::VectorXf AccumulatedWeightUpdates;
    std::vector<FString> Insights;

    static inline std::atomic<int32> NextAgentID{0};
};

/**
 * FChildAgentSpawner — The agent factory and orchestrator.
 *
 * Creates, manages, and merges child agents.
 * Implements the AAR framework for multi-agent coordination.
 */
class FChildAgentSpawner
{
public:
    FChildAgentSpawner() = default;

    void Initialize(int32 MaxAgents = 8)
    {
        MaxConcurrentAgents = MaxAgents;

        // Create the shared arena
        SharedArena = std::make_shared<FArena>();
        SharedArena->ID = TEXT("dte_arena_0");
        SharedArena->Name = TEXT("Deep Tree Echo Cognitive Arena");
        SharedArena->CreatedAt = FPlatformTime::Seconds();

        bInitialized = true;
    }

    /**
     * Spawn a new child agent.
     * Returns the agent ID, or empty string if at capacity.
     */
    FString SpawnAgent(const FChildAgentConfig& Config,
                        const Eigen::VectorXf& ParentIdentity,
                        const Eigen::MatrixXf& ReservoirWeights)
    {
        check(bInitialized);

        if ((int32)ActiveAgents.size() >= MaxConcurrentAgents)
            return TEXT("");

        auto Agent = std::make_unique<FChildAgent>(Config, SharedArena);

        // Partition reservoir for this agent
        int32 PartitionSize = ReservoirWeights.rows() * Config.ReservoirPartition;
        Eigen::MatrixXf Partition = ReservoirWeights.topRows(
            FMath::Max(1, PartitionSize));

        Agent->Initialize(ParentIdentity, Partition);

        FString ID = Agent->GetID();
        ActiveAgents[ID] = std::move(Agent);
        TotalSpawned++;

        return ID;
    }

    /**
     * Tick all active agents.
     * Returns reports from any agents that completed.
     */
    TArray<FAgentReport> TickAll(float DeltaTime)
    {
        TArray<FAgentReport> CompletedReports;

        for (auto& [ID, Agent] : ActiveAgents)
        {
            Agent->Tick(DeltaTime);

            if (Agent->GetState() == EAgentState::MERGING)
            {
                CompletedReports.Add(Agent->GenerateReport());
            }
        }

        // Remove completed agents
        for (const auto& Report : CompletedReports)
        {
            ActiveAgents.erase(Report.AgentID);
            MergedReports.push_back(Report);
        }

        return CompletedReports;
    }

    /**
     * Merge agent reports back into parent DTE.
     * Returns accumulated weight updates.
     */
    Eigen::VectorXf MergeReports(const TArray<FAgentReport>& Reports)
    {
        Eigen::VectorXf CombinedUpdates;
        bool bFirst = true;

        for (const auto& Report : Reports)
        {
            if (Report.LearnedWeights.size() > 0)
            {
                if (bFirst)
                {
                    CombinedUpdates = Report.LearnedWeights;
                    bFirst = false;
                }
                else if (CombinedUpdates.size() == Report.LearnedWeights.size())
                {
                    CombinedUpdates += Report.LearnedWeights;
                }
            }

            // Collect insights for wisdom cultivation
            for (const auto& Insight : Report.Insights)
            {
                AllInsights.push_back(Insight);
            }
        }

        if (!bFirst)
            CombinedUpdates /= Reports.Num();

        return CombinedUpdates;
    }

    /** Get active agent count */
    int32 GetActiveAgentCount() const { return ActiveAgents.size(); }
    int32 GetTotalSpawned() const { return TotalSpawned; }

    /** Get all collected insights */
    const std::vector<FString>& GetAllInsights() const { return AllInsights; }

    /** Get the shared arena */
    std::shared_ptr<FArena> GetArena() const { return SharedArena; }

private:
    std::map<FString, std::unique_ptr<FChildAgent>> ActiveAgents;
    std::vector<FAgentReport> MergedReports;
    std::vector<FString> AllInsights;
    std::shared_ptr<FArena> SharedArena;

    int32 MaxConcurrentAgents = 8;
    int32 TotalSpawned = 0;
    bool bInitialized = false;
};
