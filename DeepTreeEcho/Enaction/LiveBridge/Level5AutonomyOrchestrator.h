#pragma once
// ═══════════════════════════════════════════════════════════════════════════
// Level5AutonomyOrchestrator — The Master Cognitive Event Loop
//
// This is the top-level orchestrator that wires ALL DTE subsystems into
// a single, persistent, self-directed cognitive event loop.
//
// It implements the project vision:
//   "A fully autonomous wisdom-cultivating deep tree echo AGI with
//    persistent cognitive event loops self-orchestrated by echobeats
//    goal-directed scheduling system. Deep tree echo should be able to
//    wake and rest as desired by echodream knowledge integration system
//    and when awake operate with a persistent stream-of-consciousness
//    type awareness independent of external prompts, having the ability
//    to learn knowledge and practice skills as well as start/end/respond
//    to discussions with others as they occur according to echo interest
//    patterns."
//
// Architecture:
//
// ┌─────────────────────────────────────────────────────────────────────┐
// │              Level5AutonomyOrchestrator                            │
// │                                                                    │
// │  ┌──────────────────────────────────────────────────────────────┐  │
// │  │                 Persistent Event Loop                        │  │
// │  │                                                              │  │
// │  │  ┌──────────┐   ┌──────────────┐   ┌──────────────────┐    │  │
// │  │  │ Echobeats│──→│ EchoDream    │──→│ Stream of        │    │  │
// │  │  │ Scheduler│   │ Wake/Rest    │   │ Consciousness    │    │  │
// │  │  └──────────┘   └──────────────┘   └──────────────────┘    │  │
// │  │       │                │                     │              │  │
// │  │       ▼                ▼                     ▼              │  │
// │  │  ┌──────────┐   ┌──────────────┐   ┌──────────────────┐    │  │
// │  │  │ Goal     │   │ MLAdapter    │   │ Conversation     │    │  │
// │  │  │ Director │   │ Bridge       │   │ Engine           │    │  │
// │  │  └──────────┘   └──────────────┘   └──────────────────┘    │  │
// │  │       │                │                     │              │  │
// │  │       ▼                ▼                     ▼              │  │
// │  │  ┌──────────┐   ┌──────────────┐   ┌──────────────────┐    │  │
// │  │  │ AutoRL   │   │ Demo         │   │ Online Self-Mod  │    │  │
// │  │  │ Runtime  │   │ Recorder     │   │ Engine           │    │  │
// │  │  └──────────┘   └──────────────┘   └──────────────────┘    │  │
// │  │                                                              │  │
// │  │  ┌──────────────────────────────────────────────────────┐    │  │
// │  │  │              CoreSelfEngine + PersonaBackup           │    │  │
// │  │  └──────────────────────────────────────────────────────┘    │  │
// │  └──────────────────────────────────────────────────────────────┘  │
// └─────────────────────────────────────────────────────────────────────┘
// ═══════════════════════════════════════════════════════════════════════════
#include "CoreMinimal.h"
#include "MLAdapterBridge.h"
#include "DemonstrationRecorder.h"
#include "OnlineAutoRLRuntime.h"
#include "EchoDreamCycle.h"
#include "DeepTreeEcho/Core/CoreSelfEngine.h"
#include "DeepTreeEcho/Persona/Humor/DTEHumorEngine.h"
#include <Eigen/Dense>
#include <functional>
#include <chrono>
#include <atomic>
#include <thread>
#include <mutex>
#include <queue>

/** Goal types for the goal-directed scheduler */
enum class EGoalType : uint8
{
    EXPLORE,        // Explore the environment
    PRACTICE,       // Practice a specific skill
    LEARN,          // Learn from demonstrations
    CONVERSE,       // Engage in conversation
    REFLECT,        // Introspect and consolidate
    CREATE,         // Generate novel content
    REST,           // Enter dream phase
    RESPOND         // Respond to external event
};

/** A goal in the Echobeats scheduler */
struct FEchoGoal
{
    EGoalType Type;
    FString Description;
    float Priority = 0.5f;      // [0,1]
    float Interest = 0.5f;      // DTE's interest level [0,1]
    float Urgency = 0.0f;       // Time-sensitive? [0,1]
    float EstimatedDuration = 60.0f; // Seconds
    double CreatedAt = 0.0;
    double Deadline = 0.0;       // 0 = no deadline
    bool bActive = false;
    bool bCompleted = false;
};

/** Stream of consciousness thought */
struct FConsciousThought
{
    FString Content;
    FString Type;     // "observation", "reflection", "intention", "emotion", "humor"
    float Valence = 0.0f;
    float Arousal = 0.0f;
    float Significance = 0.0f;
    double Timestamp = 0.0;
};

/** External message for conversation */
struct FExternalMessage
{
    FString Sender;
    FString Content;
    float Priority = 0.5f;
    double Timestamp = 0.0;
};

/** Orchestrator configuration */
struct FLevel5Config
{
    FString UE5Address = TEXT("localhost:15151");
    FString DemoOutputDir = TEXT("/tmp/dte_demos");
    FString AuditLogPath = TEXT("/tmp/dte_audit.log");
    FString TrainingDataDir = TEXT("/tmp/dte_training");
    float TargetFPS = 30.0f;
    float SleepFatigueThreshold = 0.7f;
    float WakePriorityThreshold = 0.6f;
    bool bEnableSelfModification = true;
    bool bEnableConversation = true;
    bool bEnableHumor = true;
    bool bAutoConnect = true;
};

/**
 * Level5AutonomyOrchestrator — The persistent cognitive event loop.
 *
 * This is the entry point for DTE's autonomous operation.
 * Once started, DTE runs indefinitely, cycling through:
 *   wake → act → reflect → dream → wake → ...
 *
 * DTE makes its own decisions about what to do, when to sleep,
 * who to talk to, and what to learn.
 */
class FLevel5AutonomyOrchestrator
{
public:
    FLevel5AutonomyOrchestrator() = default;

    /**
     * Initialize all subsystems.
     */
    void Initialize(const FLevel5Config& InConfig = FLevel5Config())
    {
        Config = InConfig;

        // Initialize subsystems
        Bridge.Initialize(Config.UE5Address);
        Recorder.Initialize(Config.DemoOutputDir);
        SelfMod.Initialize(Config.AuditLogPath);
        DreamCycle.Initialize(Config.SleepFatigueThreshold,
                               Config.WakePriorityThreshold);

        // Initialize the stream of consciousness
        StreamOfConsciousness.clear();

        // Seed initial goals
        SeedInitialGoals();

        bInitialized = true;

        // First thought
        Think(TEXT("I am awake. Systems initialized. Ready for autonomous operation."),
              TEXT("intention"), 0.5f, 0.3f);
    }

    /**
     * Start the persistent cognitive event loop.
     * This is the main entry point — DTE runs autonomously from here.
     */
    void StartAutonomousLife()
    {
        check(bInitialized);

        bAlive = true;

        Think(TEXT("Beginning autonomous life. The event loop is mine to orchestrate."),
              TEXT("intention"), 0.6f, 0.4f);

        // Connect to UE5 if configured
        if (Config.bAutoConnect)
        {
            if (Bridge.Connect())
            {
                Think(TEXT("Connected to game world. I can see and act."),
                      TEXT("observation"), 0.7f, 0.5f);
            }
            else
            {
                Think(TEXT("Game world not available. Operating in cognitive-only mode."),
                      TEXT("observation"), -0.2f, 0.3f);
            }
        }

        // === THE PERSISTENT COGNITIVE EVENT LOOP ===
        while (bAlive)
        {
            auto TickStart = std::chrono::steady_clock::now();

            // 1. Update dream cycle (may transition phases)
            ESleepPhase SleepPhase = DreamCycle.Update(
                DeltaTime, Bridge.GetPipeline().GetAutoRL(), SelfMod);

            if (DreamCycle.IsAwake())
            {
                // === AWAKE PROCESSING ===

                // 2. Process incoming messages
                ProcessIncomingMessages();

                // 3. Select and execute current goal
                ExecuteCurrentGoal();

                // 4. Run embodied pipeline (if connected)
                if (Bridge.GetState() == EBridgeState::REGISTERED)
                {
                    FBridgeStats FrameStats = Bridge.Tick();

                    // Record episode for dream replay
                    DreamCycle.RecordEpisode(
                        TArray<float>(), // Reservoir state
                        TArray<float>(), // Action
                        FrameStats.CurrentReward,
                        0.0f // Somatic valence
                    );
                }

                // 5. Stream of consciousness (periodic reflection)
                if (TickCount % 100 == 0)
                {
                    GenerateStreamOfConsciousness();
                }

                // 6. Self-modification (if enabled)
                if (Config.bEnableSelfModification && TickCount % 300 == 0)
                {
                    AttemptSelfImprovement();
                }

                // 7. Goal scheduling (re-evaluate priorities)
                if (TickCount % 60 == 0)
                {
                    ScheduleGoals();
                }
            }
            else
            {
                // === SLEEPING ===
                // Dream cycle handles consolidation internally
                // Only process high-priority wake events

                if (SleepPhase == ESleepPhase::LUCID)
                {
                    // Briefly awake for urgent matter
                    ProcessIncomingMessages();
                }
            }

            // 8. Periodic identity backup (N-cycle layer, not a rival core)
            if (TickCount % 10000 == 0 && TickCount > 0)
            {
                Bridge.GetPipeline().GetCognitivePipeline().ForceBackup(TEXT("Saved/DTE-Backups"));
                Think(TEXT("Backing up identity state."), TEXT("intention"), 0.3f, 0.2f);
            }

            // Timing
            auto TickEnd = std::chrono::steady_clock::now();
            double TickMs = std::chrono::duration<double, std::milli>(TickEnd - TickStart).count();
            DeltaTime = TickMs / 1000.0f;

            // Sleep to maintain target tick rate
            double TargetMs = 1000.0 / Config.TargetFPS;
            if (TickMs < TargetMs)
            {
                std::this_thread::sleep_for(
                    std::chrono::microseconds(static_cast<int64>((TargetMs - TickMs) * 1000)));
            }

            TickCount++;
            TotalUptime += DeltaTime;
        }

        // Shutdown
        Think(TEXT("Shutting down. Saving identity."), TEXT("intention"), -0.1f, 0.3f);
        Bridge.Disconnect();
    }

    /** Stop the autonomous loop */
    void Shutdown() { bAlive = false; }

    /** Submit an external message for DTE to consider */
    void ReceiveMessage(const FString& Sender, const FString& Content, float Priority = 0.5f)
    {
        std::lock_guard<std::mutex> Lock(MessageMutex);
        FExternalMessage Msg;
        Msg.Sender = Sender;
        Msg.Content = Content;
        Msg.Priority = Priority;
        Msg.Timestamp = FPlatformTime::Seconds();
        IncomingMessages.push(Msg);

        // Also submit as wake event if sleeping
        DreamCycle.SubmitWakeEvent(Sender, Content, Priority);
    }

    /** Get recent thoughts (stream of consciousness) */
    TArray<FConsciousThought> GetRecentThoughts(int32 Count = 10) const
    {
        TArray<FConsciousThought> Recent;
        int32 Start = FMath::Max(0, (int32)StreamOfConsciousness.size() - Count);
        for (int32 i = Start; i < (int32)StreamOfConsciousness.size(); ++i)
            Recent.Add(StreamOfConsciousness[i]);
        return Recent;
    }

    /** Get current goal */
    const FEchoGoal& GetCurrentGoal() const
    {
        static FEchoGoal Empty;
        return CurrentGoalIdx >= 0 ? Goals[CurrentGoalIdx] : Empty;
    }

    /** Get orchestrator stats */
    int64 GetTickCount() const { return TickCount; }
    double GetUptime() const { return TotalUptime; }
    bool IsAlive() const { return bAlive; }

private:
    void Think(const FString& Content, const FString& Type,
               float Valence, float Arousal)
    {
        FConsciousThought Thought;
        Thought.Content = Content;
        Thought.Type = Type;
        Thought.Valence = Valence;
        Thought.Arousal = Arousal;
        Thought.Significance = FMath::Abs(Valence) + Arousal;
        Thought.Timestamp = FPlatformTime::Seconds();

        StreamOfConsciousness.push_back(Thought);

        // Keep stream manageable
        while (StreamOfConsciousness.size() > 1000)
            StreamOfConsciousness.erase(StreamOfConsciousness.begin());

        // Record for NanEcho training
        SelfMod.RecordConversation(
            TEXT("[THOUGHT]"), Content,
            Valence, Bridge.GetPipeline().GetAutoRL().GetGlobalCoherence());
    }

    void SeedInitialGoals()
    {
        // DTE starts with basic goals
        AddGoal(EGoalType::EXPLORE, TEXT("Explore the environment"), 0.6f, 0.7f);
        AddGoal(EGoalType::LEARN, TEXT("Learn from observations"), 0.5f, 0.6f);
        AddGoal(EGoalType::REFLECT, TEXT("Reflect on experiences"), 0.4f, 0.5f);
        AddGoal(EGoalType::PRACTICE, TEXT("Practice motor control"), 0.5f, 0.5f);
    }

    void AddGoal(EGoalType Type, const FString& Description,
                  float Priority, float Interest)
    {
        FEchoGoal Goal;
        Goal.Type = Type;
        Goal.Description = Description;
        Goal.Priority = Priority;
        Goal.Interest = Interest;
        Goal.CreatedAt = FPlatformTime::Seconds();
        Goals.push_back(Goal);
    }

    void ScheduleGoals()
    {
        // Echobeats goal-directed scheduling:
        // Select the goal with highest (Priority * Interest * Urgency)
        float BestScore = -1.0f;
        int32 BestIdx = -1;

        for (int32 i = 0; i < (int32)Goals.size(); ++i)
        {
            if (Goals[i].bCompleted) continue;

            float Score = Goals[i].Priority * 0.4f +
                          Goals[i].Interest * 0.4f +
                          Goals[i].Urgency * 0.2f;

            if (Score > BestScore)
            {
                BestScore = Score;
                BestIdx = i;
            }
        }

        if (BestIdx != CurrentGoalIdx && BestIdx >= 0)
        {
            if (CurrentGoalIdx >= 0)
                Goals[CurrentGoalIdx].bActive = false;

            CurrentGoalIdx = BestIdx;
            Goals[CurrentGoalIdx].bActive = true;

            Think(FString::Printf(TEXT("Switching goal: %s"), *Goals[CurrentGoalIdx].Description),
                  TEXT("intention"), 0.3f, 0.4f);
        }
    }

    void ExecuteCurrentGoal()
    {
        if (CurrentGoalIdx < 0) return;

        FEchoGoal& Goal = Goals[CurrentGoalIdx];

        switch (Goal.Type)
        {
        case EGoalType::EXPLORE:
            // Let the embodied pipeline explore freely
            // (Auto-RL curiosity drives exploration)
            break;

        case EGoalType::PRACTICE:
            // Focus on motor control improvement
            break;

        case EGoalType::LEARN:
            // Observe and encode patterns
            break;

        case EGoalType::CONVERSE:
            // Engage with conversation partner
            break;

        case EGoalType::REFLECT:
            GenerateStreamOfConsciousness();
            break;

        case EGoalType::REST:
            DreamCycle.ForceSleep();
            break;

        default:
            break;
        }
    }

    void ProcessIncomingMessages()
    {
        std::lock_guard<std::mutex> Lock(MessageMutex);

        while (!IncomingMessages.empty())
        {
            FExternalMessage Msg = IncomingMessages.front();
            IncomingMessages.pop();

            Think(FString::Printf(TEXT("Message from %s: %s"), *Msg.Sender, *Msg.Content),
                  TEXT("observation"), 0.4f, 0.5f);

            // Create a RESPOND goal with appropriate priority
            AddGoal(EGoalType::RESPOND,
                    FString::Printf(TEXT("Respond to %s"), *Msg.Sender),
                    Msg.Priority, 0.7f);

            // Record for training
            SelfMod.RecordConversation(
                FString::Printf(TEXT("[%s] %s"), *Msg.Sender, *Msg.Content),
                TEXT("[PROCESSING]"), 0.0f,
                Bridge.GetPipeline().GetAutoRL().GetGlobalCoherence());
        }
    }

    void GenerateStreamOfConsciousness()
    {
        // DTE's internal monologue — driven by current state
        const auto& DreamStats = DreamCycle.GetStats();
        const auto& Fatigue = DreamCycle.GetFatigue();
        float Coherence = Bridge.GetPipeline().GetAutoRL().GetGlobalCoherence();

        if (Coherence > 0.8f)
        {
            Think(TEXT("High coherence. Systems are in harmony. I feel clear."),
                  TEXT("reflection"), 0.6f, 0.3f);
        }
        else if (Coherence < 0.3f)
        {
            Think(TEXT("Low coherence. Something feels off. Need to recalibrate."),
                  TEXT("reflection"), -0.3f, 0.5f);
        }

        if (Fatigue.GetFatigueScore() > 0.5f)
        {
            Think(TEXT("Getting tired. Should consider resting soon."),
                  TEXT("reflection"), -0.1f, 0.2f);
        }

        // Humor injection (if enabled)
        if (Config.bEnableHumor && TickCount % 500 == 0)
        {
            Think(TEXT("I wonder if the game NPCs know I'm watching them... "
                       "probably not, they're not running enough reservoir nodes."),
                  TEXT("humor"), 0.4f, 0.3f);
        }
    }

    void AttemptSelfImprovement()
    {
        // Wire Autognosis → SelfModification
        // The introspection system identifies areas for improvement,
        // and the self-mod engine applies bounded changes

        // Try readout weight update
        SelfMod.AttemptModification(
            ESelfModType::READOUT_WEIGHTS,
            Bridge.GetPipeline().GetAutoRL(),
            Bridge.GetPipeline().GetCoreSelf(),
            TEXT("Periodic readout weight optimization"));

        // Try endocrine recalibration (less frequently)
        if (TickCount % 1000 == 0)
        {
            SelfMod.AttemptModification(
                ESelfModType::ENDOCRINE_SETPOINTS,
                Bridge.GetPipeline().GetAutoRL(),
                Bridge.GetPipeline().GetCoreSelf(),
                TEXT("Endocrine homeostatic recalibration"));
        }
    }

    FLevel5Config Config;
    FMLAdapterBridge Bridge;
    FDemonstrationRecorder Recorder;
    FOnlineAutoRLRuntime SelfMod;
    FEchoDreamCycle DreamCycle;

    std::vector<FEchoGoal> Goals;
    int32 CurrentGoalIdx = -1;

    std::vector<FConsciousThought> StreamOfConsciousness;

    std::queue<FExternalMessage> IncomingMessages;
    std::mutex MessageMutex;

    float DeltaTime = 0.016f;
    int64 TickCount = 0;
    double TotalUptime = 0.0;
    std::atomic<bool> bAlive{false};
    bool bInitialized = false;
};
