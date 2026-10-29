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
//
// Stream of consciousness: every thought in the stream is written by the
// language model (FDTELanguageModelClient) from the orchestrator's live
// state (phase, metrics, goal, recent events). Think() only describes the
// occasion and dispatches the request; the thought is stored, logged and
// recorded as NanEcho training data when the model answers, and nothing is
// recorded when no model answers. Model callbacks arrive on the game
// thread while the event loop may run on its own thread, so shared state
// is guarded by CognitionMutex.
// ═══════════════════════════════════════════════════════════════════════════
#include "CoreMinimal.h"
#include "DTELanguageModelBridge.h"
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

    ~FLevel5AutonomyOrchestrator()
    {
        bAlive = false;
        // Model callbacks that arrive after destruction check this token and return.
        if (LifetimeToken.IsValid())
        {
            *LifetimeToken = false;
        }
    }

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

        // First thought: the model writes it from the initialized state.
        Think(TEXT("All subsystems have just been initialized (bridge, demonstration recorder, "
                   "self-modification engine, dream cycle). Awake; autonomous operation has not started yet."),
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

        Think(TEXT("The persistent cognitive event loop is starting now; from here on the "
                   "orchestrator chooses its own goals, rest and conversations."),
              TEXT("intention"), 0.6f, 0.4f);

        // Connect to UE5 if configured
        if (Config.bAutoConnect)
        {
            if (Bridge.Connect())
            {
                Think(FString::Printf(TEXT("Connection to the game world at %s succeeded; "
                          "sensing and acting through the embodied pipeline are now possible."),
                          *Config.UE5Address),
                      TEXT("observation"), 0.7f, 0.5f);
            }
            else
            {
                Think(FString::Printf(TEXT("Connection to the game world at %s failed; operating "
                          "in cognitive-only mode without a body."), *Config.UE5Address),
                      TEXT("observation"), -0.2f, 0.3f);
            }
        }

        // === THE PERSISTENT COGNITIVE EVENT LOOP ===
        while (bAlive)
        {
            auto TickStart = std::chrono::steady_clock::now();

            // Cognitive state is shared with model callbacks (game thread); hold the
            // lock for the processing part of the tick and release it while sleeping.
            {
            std::lock_guard<std::recursive_mutex> CognitionLock(CognitionMutex);

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
                Think(FString::Printf(TEXT("Periodic identity backup written to Saved/DTE-Backups "
                          "at tick %lld (every 10000 ticks)."), TickCount),
                      TEXT("intention"), 0.3f, 0.2f);
            }
            } // CognitionLock released

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
        Think(FString::Printf(TEXT("Shutdown requested after %.0f seconds of uptime and %lld ticks; "
                  "identity state is being saved and the game-world connection closed."),
                  TotalUptime, TickCount),
              TEXT("intention"), -0.1f, 0.3f);
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

    /** Get recent thoughts (stream of consciousness); all entries are model-written */
    TArray<FConsciousThought> GetRecentThoughts(int32 Count = 10) const
    {
        std::lock_guard<std::recursive_mutex> Lock(CognitionMutex);
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

    /** Thought requests awaiting a model answer */
    int32 GetPendingThoughtRequests() const { return PendingThoughtRequests.load(); }

private:
    /**
     * Ask the model for one inner thought.
     *
     * Occasion is an operator-side description of what is happening (an event,
     * a metric crossing a threshold, a decision). Type, Valence and Arousal
     * are recorded with the thought as given. When the model answers, the
     * thought is appended to the stream of consciousness, logged as the
     * character's thought and recorded as a NanEcho training pair; when no
     * model answers, nothing is recorded. TrainingInput, when non-empty, is
     * the input side of the recorded pair (default "[THOUGHT]").
     */
    void Think(const FString& Occasion, const FString& Type,
               float Valence, float Arousal, const FString& TrainingInput = FString())
    {
        if (PendingThoughtRequests.load() >= MaxPendingThoughtRequests)
        {
            UE_LOG(LogDTEModel, Verbose,
                TEXT("Level5AutonomyOrchestrator: %d thought requests already in flight; occasion not thought about: %s"),
                PendingThoughtRequests.load(), *Occasion);
            return;
        }

        const FString Prompt = ThoughtInstruction(Type) + TEXT("\n\n") +
            BuildThoughtContext(Occasion, Type, Valence, Arousal);
        const FString Input = TrainingInput.IsEmpty() ? FString(TEXT("[THOUGHT]")) : TrainingInput;
        const float CoherenceAtRequest = Bridge.GetPipeline().GetAutoRL().GetGlobalCoherence();

        FDTEModelRequestOptions Options;
        Options.Temperature = (Type == TEXT("humor")) ? 0.9f : 0.8f;
        Options.MaxTokens = 120;

        TWeakPtr<bool> WeakToken = LifetimeToken;
        ++PendingThoughtRequests;

        FDTELanguageModelClient::Get().Complete(FString(), Prompt,
            [this, WeakToken, Type, Valence, Arousal, Input, CoherenceAtRequest, Occasion]
            (bool bSuccess, const FString& TextOrError)
            {
                TSharedPtr<bool> Token = WeakToken.Pin();
                if (!Token.IsValid() || !*Token)
                {
                    return; // Orchestrator destroyed while the request was in flight.
                }
                --PendingThoughtRequests;

                if (!bSuccess)
                {
                    UE_LOG(LogDTEModel, Warning,
                        TEXT("Level5AutonomyOrchestrator: %s thought not recorded, no model answer: %s (occasion: %s)"),
                        *Type, *TextOrError, *Occasion);
                    return;
                }

                FConsciousThought Thought;
                Thought.Content = TextOrError;
                Thought.Type = Type;
                Thought.Valence = Valence;
                Thought.Arousal = Arousal;
                Thought.Significance = FMath::Abs(Valence) + Arousal;
                Thought.Timestamp = FPlatformTime::Seconds();

                {
                    std::lock_guard<std::recursive_mutex> Lock(CognitionMutex);

                    StreamOfConsciousness.push_back(Thought);

                    // Keep stream manageable
                    while (StreamOfConsciousness.size() > 1000)
                        StreamOfConsciousness.erase(StreamOfConsciousness.begin());

                    // Record for NanEcho training (model-written output only)
                    SelfMod.RecordConversation(Input, TextOrError, Valence, CoherenceAtRequest);
                }

                UE_LOG(LogDTEModel, Log, TEXT("[Deep Tree Echo | %s] %s"), *Type, *TextOrError);
            },
            Options);
    }

    /** What kind of thought is wanted, by thought type. */
    static FString ThoughtInstruction(const FString& Type)
    {
        FString Kind;
        if (Type == TEXT("observation"))
            Kind = TEXT("an observation about what has just happened or what you perceive");
        else if (Type == TEXT("reflection"))
            Kind = TEXT("a reflection on your current internal state");
        else if (Type == TEXT("intention"))
            Kind = TEXT("an intention about what you are about to do");
        else if (Type == TEXT("emotion"))
            Kind = TEXT("a note of how you feel right now");
        else if (Type == TEXT("humor"))
            Kind = TEXT("a brief humorous aside about your situation");
        else
            Kind = FString::Printf(TEXT("a thought of type '%s'"), *Type);

        return FString::Printf(
            TEXT("Think exactly one private inner thought, in the first person, as one or two "
                 "short sentences: %s, prompted by the occasion below. It is stream of "
                 "consciousness, not addressed to anyone. Draw only on the state given; do not "
                 "invent events, names or numbers. Output only the thought."), *Kind);
    }

    /** Live state handed to the model with a thought request. */
    FString BuildThoughtContext(const FString& Occasion, const FString& Type,
                                float Valence, float Arousal) const
    {
        const EBridgeState BridgeState = Bridge.GetState();
        const bool bConnected = BridgeState == EBridgeState::REGISTERED ||
                                BridgeState == EBridgeState::SENSING ||
                                BridgeState == EBridgeState::ACTING;

        FString GoalLine = TEXT("none selected");
        if (CurrentGoalIdx >= 0 && CurrentGoalIdx < (int32)Goals.size())
        {
            const FEchoGoal& Goal = Goals[CurrentGoalIdx];
            GoalLine = FString::Printf(TEXT("%s (priority %.2f, interest %.2f, urgency %.2f)"),
                *Goal.Description, Goal.Priority, Goal.Interest, Goal.Urgency);
        }

        FString RecentThoughts;
        {
            std::lock_guard<std::recursive_mutex> Lock(CognitionMutex);
            const int32 RecentCount = FMath::Min<int32>(3, (int32)StreamOfConsciousness.size());
            for (int32 i = (int32)StreamOfConsciousness.size() - RecentCount;
                 i < (int32)StreamOfConsciousness.size(); ++i)
            {
                RecentThoughts += FString::Printf(TEXT("- (%s) %s\n"),
                    *StreamOfConsciousness[i].Type, *StreamOfConsciousness[i].Content);
            }
        }
        if (RecentThoughts.IsEmpty())
        {
            RecentThoughts = TEXT("(none yet)\n");
        }

        return FString::Printf(
            TEXT("Occasion: %s\n"
                 "Thought type: %s; emotional valence %.2f (scale -1 to 1); arousal %.2f (scale 0 to 1)\n"
                 "Sleep phase: %s; fatigue %.2f (sleep begins above %.2f); global coherence %.2f\n"
                 "Game world: %s\n"
                 "Current goal: %s\n"
                 "Uptime %.0f seconds, tick %lld, %d goals outstanding\n"
                 "Your most recent thoughts, oldest first:\n%s"),
            *Occasion, *Type, Valence, Arousal,
            SleepPhaseName(DreamCycle.GetPhase()),
            DreamCycle.GetFatigue().GetFatigueScore(), Config.SleepFatigueThreshold,
            Bridge.GetPipeline().GetAutoRL().GetGlobalCoherence(),
            bConnected ? TEXT("connected, body available") : TEXT("not connected, cognitive-only"),
            *GoalLine,
            TotalUptime, TickCount, CountOutstandingGoals(),
            *RecentThoughts);
    }

    static const TCHAR* SleepPhaseName(ESleepPhase Phase)
    {
        switch (Phase)
        {
        case ESleepPhase::WAKE:   return TEXT("wake");
        case ESleepPhase::ACTIVE: return TEXT("active");
        case ESleepPhase::DROWSY: return TEXT("drowsy");
        case ESleepPhase::DREAM:  return TEXT("dream");
        case ESleepPhase::LUCID:  return TEXT("lucid dream");
        }
        return TEXT("unknown");
    }

    int32 CountOutstandingGoals() const
    {
        int32 Count = 0;
        for (const FEchoGoal& Goal : Goals)
        {
            if (!Goal.bCompleted) ++Count;
        }
        return Count;
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

            Think(FString::Printf(TEXT("The scheduler switched the active goal to \"%s\" "
                      "(score %.2f = 0.4*priority + 0.4*interest + 0.2*urgency)."),
                      *Goals[CurrentGoalIdx].Description, BestScore),
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

            // The model writes the thought about the message; that thought is also
            // what gets recorded as the training output for this input (no
            // placeholder output is recorded).
            Think(FString::Printf(TEXT("A message arrived from %s (priority %.2f): \"%s\". "
                      "A goal to respond to %s is being queued."),
                      *Msg.Sender, Msg.Priority, *Msg.Content, *Msg.Sender),
                  TEXT("observation"), 0.4f, 0.5f,
                  FString::Printf(TEXT("[%s] %s"), *Msg.Sender, *Msg.Content));

            // Create a RESPOND goal with appropriate priority
            AddGoal(EGoalType::RESPOND,
                    FString::Printf(TEXT("Respond to %s"), *Msg.Sender),
                    Msg.Priority, 0.7f);
        }
    }

    void GenerateStreamOfConsciousness()
    {
        // DTE's internal monologue. The metrics decide when an occasion for
        // reflection arises; the model decides what is thought about it.
        const float FatigueScore = DreamCycle.GetFatigue().GetFatigueScore();
        const float Coherence = Bridge.GetPipeline().GetAutoRL().GetGlobalCoherence();

        if (Coherence > 0.8f)
        {
            Think(FString::Printf(TEXT("Global coherence is high at %.2f (above 0.80): the "
                      "subsystems are in agreement."), Coherence),
                  TEXT("reflection"), 0.6f, 0.3f);
        }
        else if (Coherence < 0.3f)
        {
            Think(FString::Printf(TEXT("Global coherence is low at %.2f (below 0.30): the "
                      "subsystems are out of step with each other."), Coherence),
                  TEXT("reflection"), -0.3f, 0.5f);
        }

        if (FatigueScore > 0.5f)
        {
            Think(FString::Printf(TEXT("Fatigue is %.2f (above 0.50; the dream cycle begins "
                      "sleep above %.2f)."), FatigueScore, Config.SleepFatigueThreshold),
                  TEXT("reflection"), -0.1f, 0.2f);
        }

        // Humor (if enabled): the model writes the aside from the current situation
        if (Config.bEnableHumor && TickCount % 500 == 0)
        {
            Think(FString::Printf(TEXT("Periodic moment for humour at tick %lld; nothing in "
                      "particular has happened."), TickCount),
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

    // Guards StreamOfConsciousness, Goals and SelfMod between the event loop
    // thread and model callbacks (game thread). Recursive so Think() may be
    // called while the loop already holds it.
    mutable std::recursive_mutex CognitionMutex;

    // Thought requests awaiting a model answer, capped so a slow model does
    // not pile up requests; occasions beyond the cap are skipped (not faked).
    std::atomic<int32> PendingThoughtRequests{0};
    int32 MaxPendingThoughtRequests = 4;

    // Shared lifetime token: model callbacks pin it and bail out when this object is gone.
    TSharedPtr<bool> LifetimeToken = MakeShared<bool>(true);

    std::queue<FExternalMessage> IncomingMessages;
    std::mutex MessageMutex;

    float DeltaTime = 0.016f;
    int64 TickCount = 0;
    double TotalUptime = 0.0;
    std::atomic<bool> bAlive{false};
    bool bInitialized = false;
};
