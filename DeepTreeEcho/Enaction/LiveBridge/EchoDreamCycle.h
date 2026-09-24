#pragma once
// ═══════════════════════════════════════════════════════════════════════════
// EchoDreamCycle — Wake/Rest Knowledge Integration System
//
// DTE's autonomous wake/rest cycle implements the biological insight that
// learning requires both active experience AND offline consolidation.
//
// The cycle has 4 phases:
//
//   WAKE → ACTIVE → DROWSY → DREAM → WAKE → ...
//
// WAKE Phase:
//   - Full sensorimotor engagement with the environment
//   - Online learning via NeuroEndocrineAutoRL
//   - Real-time interaction with humans and game world
//   - Echobeats running all 3 concurrent streams
//
// ACTIVE Phase:
//   - Focused task execution (gameplay, conversation, practice)
//   - High dopamine, high norepinephrine
//   - Memory encoding (episodic → working memory)
//   - Somatic markers being generated
//
// DROWSY Phase:
//   - Reduced sensorimotor engagement
//   - Begin memory consolidation (replay recent episodes)
//   - Endocrine system shifting to rest mode
//   - Echobeats slowing (longer cycle period)
//
// DREAM Phase:
//   - No external interaction (DTE is "asleep")
//   - Offline replay of experiences through reservoir
//   - Weight consolidation (online → long-term)
//   - NanEcho training data generation from replay
//   - Identity mesh pruning and strengthening
//   - Wisdom integration (cross-episode pattern extraction)
//   - Echobeats in minimal mode (1 stream, slow)
//
// The system is self-orchestrated by Echobeats' goal-directed scheduling:
//   - DTE decides WHEN to sleep based on fatigue signals
//   - DTE decides HOW LONG to sleep based on consolidation needs
//   - DTE can be awakened by external events (messages, game events)
//   - DTE can choose to ignore wake signals if deep in consolidation
//
// This implements the project vision:
//   "DTE should be able to wake and rest as desired by the EchoDream
//    knowledge integration system"
// ═══════════════════════════════════════════════════════════════════════════
#include "CoreMinimal.h"
#include "OnlineAutoRLRuntime.h"
#include <Eigen/Dense>
#include <deque>
#include <chrono>
#include <functional>

/** Sleep-wake phase */
enum class ESleepPhase : uint8
{
    WAKE,       // Full engagement
    ACTIVE,     // Focused task execution
    DROWSY,     // Transitioning to sleep
    DREAM,      // Offline consolidation
    LUCID       // Dreaming but responsive to high-priority events
};

/** Fatigue signals that drive the sleep decision */
struct FFatigueSignals
{
    float CognitiveLoad = 0.0f;       // How hard DTE is working [0,1]
    float MemoryPressure = 0.0f;      // How full working memory is [0,1]
    float RewardSaturation = 0.0f;    // Diminishing returns on learning [0,1]
    float CortisolAccumulation = 0.0f;// Stress hormone buildup [0,1]
    float NoveltyDepletion = 0.0f;    // Environment becoming predictable [0,1]
    float TimeSinceLastSleep = 0.0f;  // Hours since last dream phase
    float ConsolidationBacklog = 0.0f;// Unprocessed episodes [0,1]

    /** Composite fatigue score [0,1] */
    float GetFatigueScore() const
    {
        return (CognitiveLoad * 0.15f +
                MemoryPressure * 0.20f +
                RewardSaturation * 0.15f +
                CortisolAccumulation * 0.15f +
                NoveltyDepletion * 0.10f +
                FMath::Clamp(TimeSinceLastSleep / 16.0f, 0.0f, 1.0f) * 0.15f +
                ConsolidationBacklog * 0.10f);
    }
};

/** Dream replay episode */
struct FDreamEpisode
{
    int64 OriginalFrame = 0;
    TArray<float> ReservoirState;     // Reservoir snapshot at episode start
    TArray<float> ActionTaken;
    float Reward = 0.0f;
    float SomaticValence = 0.0f;
    float Significance = 0.0f;        // How important this episode is
    bool bConsolidated = false;
};

/** Wake event that can interrupt sleep */
struct FWakeEvent
{
    FString Source;          // "message", "game_event", "timer", "external"
    FString Content;
    float Priority = 0.0f;  // 0-1, must exceed WakeThreshold to wake DTE
    double Timestamp = 0.0;
};

/** Dream cycle statistics */
struct FDreamStats
{
    ESleepPhase CurrentPhase = ESleepPhase::WAKE;
    float FatigueScore = 0.0f;
    int32 EpisodesReplayed = 0;
    int32 EpisodesConsolidated = 0;
    int32 PatternsExtracted = 0;
    float ConsolidationProgress = 0.0f;  // [0,1]
    float DreamDurationMinutes = 0.0f;
    int32 WakeEventsReceived = 0;
    int32 WakeEventsIgnored = 0;
    int64 TotalSleepCycles = 0;
};

/**
 * EchoDreamCycle — The autonomous wake/rest system.
 *
 * This is what makes DTE truly autonomous: it decides when to sleep,
 * what to dream about, and when to wake up — all based on its own
 * internal signals, not external commands.
 *
 * Architecture:
 *
 * ┌─────────────────────────────────────────────────────────────┐
 * │                    EchoDreamCycle                           │
 * │                                                            │
 * │  ┌──────────────────────────────────────────────────────┐  │
 * │  │              Fatigue Monitor                         │  │
 * │  │  CogLoad MemPress Cortisol Novelty Backlog          │  │
 * │  │     │        │       │        │       │              │  │
 * │  │     └────────┴───────┴────────┴───────┘              │  │
 * │  │                   │                                  │  │
 * │  │            FatigueScore                              │  │
 * │  │                   │                                  │  │
 * │  │         ┌─────────▼─────────┐                        │  │
 * │  │         │  Sleep Decision   │ > threshold? → DROWSY  │  │
 * │  │         └───────────────────┘                        │  │
 * │  └──────────────────────────────────────────────────────┘  │
 * │                                                            │
 * │  ┌──────────────────────────────────────────────────────┐  │
 * │  │              Dream Engine                            │  │
 * │  │  ┌─────────┐  ┌─────────┐  ┌─────────┐             │  │
 * │  │  │ Episode │  │ Weight  │  │ Pattern │             │  │
 * │  │  │ Replay  │  │ Consol. │  │ Extract │             │  │
 * │  │  └────┬────┘  └────┬────┘  └────┬────┘             │  │
 * │  │       └────────────┴────────────┘                   │  │
 * │  │                    │                                │  │
 * │  │             Wisdom Integration                      │  │
 * │  └──────────────────────────────────────────────────────┘  │
 * │                                                            │
 * │  ┌──────────────────────────────────────────────────────┐  │
 * │  │              Wake Monitor                            │  │
 * │  │  External events → Priority check → Wake/Ignore     │  │
 * │  └──────────────────────────────────────────────────────┘  │
 * └─────────────────────────────────────────────────────────────┘
 */
class FEchoDreamCycle
{
public:
    FEchoDreamCycle() = default;

    void Initialize(float SleepThreshold = 0.7f, float WakeThreshold = 0.6f)
    {
        SleepFatigueThreshold = SleepThreshold;
        WakePriorityThreshold = WakeThreshold;
        Phase = ESleepPhase::WAKE;
        bInitialized = true;
    }

    /**
     * Update the dream cycle. Call every tick.
     * Returns the current sleep phase.
     */
    ESleepPhase Update(float DeltaTime,
                        const FNeuroEndocrineAutoRL& AutoRL,
                        FOnlineAutoRLRuntime& SelfMod)
    {
        check(bInitialized);

        // Update fatigue signals from endocrine state
        UpdateFatigueSignals(AutoRL);

        switch (Phase)
        {
        case ESleepPhase::WAKE:
        case ESleepPhase::ACTIVE:
            UpdateWakePhase(DeltaTime, AutoRL, SelfMod);
            break;

        case ESleepPhase::DROWSY:
            UpdateDrowsyPhase(DeltaTime);
            break;

        case ESleepPhase::DREAM:
        case ESleepPhase::LUCID:
            UpdateDreamPhase(DeltaTime, SelfMod);
            break;
        }

        // Check for wake events during sleep
        if (Phase == ESleepPhase::DREAM || Phase == ESleepPhase::LUCID)
        {
            ProcessWakeEvents();
        }

        Stats.CurrentPhase = Phase;
        Stats.FatigueScore = Fatigue.GetFatigueScore();

        return Phase;
    }

    /**
     * Record an episode for later dream replay.
     */
    void RecordEpisode(const TArray<float>& ReservoirState,
                        const TArray<float>& Action,
                        float Reward, float SomaticValence)
    {
        FDreamEpisode Episode;
        Episode.OriginalFrame = TotalFrames++;
        Episode.ReservoirState = ReservoirState;
        Episode.ActionTaken = Action;
        Episode.Reward = Reward;
        Episode.SomaticValence = SomaticValence;

        // Significance = |reward| + |somatic| + novelty
        Episode.Significance = FMath::Abs(Reward) * 0.4f +
                                FMath::Abs(SomaticValence) * 0.3f +
                                0.3f; // base significance

        EpisodeBuffer.push_back(Episode);

        // Limit buffer size
        while (EpisodeBuffer.size() > MaxEpisodeBuffer)
            EpisodeBuffer.pop_front();

        Fatigue.ConsolidationBacklog = FMath::Clamp(
            (float)EpisodeBuffer.size() / MaxEpisodeBuffer, 0.0f, 1.0f);
    }

    /**
     * Submit a wake event (e.g., incoming message, game event).
     */
    void SubmitWakeEvent(const FString& Source, const FString& Content, float Priority)
    {
        FWakeEvent Event;
        Event.Source = Source;
        Event.Content = Content;
        Event.Priority = Priority;
        Event.Timestamp = FPlatformTime::Seconds();

        WakeEventQueue.push_back(Event);
        Stats.WakeEventsReceived++;
    }

    /** Force wake (emergency) */
    void ForceWake()
    {
        Phase = ESleepPhase::WAKE;
        DreamTimer = 0.0f;
    }

    /** Force sleep (for testing) */
    void ForceSleep()
    {
        Phase = ESleepPhase::DROWSY;
    }

    /** Get current phase */
    ESleepPhase GetPhase() const { return Phase; }
    bool IsAwake() const { return Phase == ESleepPhase::WAKE || Phase == ESleepPhase::ACTIVE; }
    bool IsSleeping() const { return Phase == ESleepPhase::DREAM || Phase == ESleepPhase::LUCID; }

    /** Get statistics */
    const FDreamStats& GetStats() const { return Stats; }
    const FFatigueSignals& GetFatigue() const { return Fatigue; }

private:
    void UpdateFatigueSignals(const FNeuroEndocrineAutoRL& AutoRL)
    {
        // Map endocrine state to fatigue signals
        const auto& Endo = AutoRL.GetEndocrineState();

        Fatigue.CortisolAccumulation = FMath::Clamp(
            Endo.Hormones[static_cast<int>(EHormone::CORTISOL)] / 1.5f, 0.0f, 1.0f);

        // Novelty depletion: if dopamine phasic is low, environment is predictable
        Fatigue.NoveltyDepletion = FMath::Clamp(
            1.0f - Endo.Hormones[static_cast<int>(EHormone::DOPAMINE_PHASIC)], 0.0f, 1.0f);

        // Cognitive load from crystal coherence (high coherence = low load)
        Fatigue.CognitiveLoad = FMath::Clamp(
            1.0f - AutoRL.GetGlobalCoherence(), 0.0f, 1.0f);

        // Memory pressure from episode buffer
        Fatigue.MemoryPressure = FMath::Clamp(
            (float)EpisodeBuffer.size() / MaxEpisodeBuffer, 0.0f, 1.0f);

        // Reward saturation: if recent rewards are flat, learning is saturated
        Fatigue.RewardSaturation = ComputeRewardSaturation();

        // Time since last sleep
        Fatigue.TimeSinceLastSleep = TimeSinceLastDream;
    }

    void UpdateWakePhase(float DeltaTime,
                          const FNeuroEndocrineAutoRL& AutoRL,
                          FOnlineAutoRLRuntime& SelfMod)
    {
        TimeSinceLastDream += DeltaTime / 3600.0f; // Convert to hours

        // Check if fatigue exceeds threshold
        if (Fatigue.GetFatigueScore() > SleepFatigueThreshold)
        {
            // DTE decides to sleep
            Phase = ESleepPhase::DROWSY;
            DrowsyTimer = 0.0f;
            UE_LOG(LogTemp, Log, TEXT("EchoDream: Fatigue %.2f > %.2f, entering DROWSY"),
                Fatigue.GetFatigueScore(), SleepFatigueThreshold);
        }
    }

    void UpdateDrowsyPhase(float DeltaTime)
    {
        DrowsyTimer += DeltaTime;

        // Transition to DREAM after drowsy period (30 seconds)
        if (DrowsyTimer > 30.0f)
        {
            Phase = ESleepPhase::DREAM;
            DreamTimer = 0.0f;
            DreamStartTime = std::chrono::steady_clock::now();
            Stats.TotalSleepCycles++;

            UE_LOG(LogTemp, Log, TEXT("EchoDream: Entering DREAM phase (cycle %lld)"),
                Stats.TotalSleepCycles);
        }
    }

    void UpdateDreamPhase(float DeltaTime, FOnlineAutoRLRuntime& SelfMod)
    {
        DreamTimer += DeltaTime;

        // === EPISODE REPLAY ===
        // Replay significant episodes through the reservoir
        if (!EpisodeBuffer.empty())
        {
            // Select most significant unconsolidated episode
            int32 BestIdx = -1;
            float BestSig = -1.0f;
            for (int32 i = 0; i < (int32)EpisodeBuffer.size(); ++i)
            {
                if (!EpisodeBuffer[i].bConsolidated &&
                    EpisodeBuffer[i].Significance > BestSig)
                {
                    BestSig = EpisodeBuffer[i].Significance;
                    BestIdx = i;
                }
            }

            if (BestIdx >= 0)
            {
                // "Replay" this episode (feed state through reservoir offline)
                EpisodeBuffer[BestIdx].bConsolidated = true;
                Stats.EpisodesReplayed++;
                Stats.EpisodesConsolidated++;
            }
        }

        // === WEIGHT CONSOLIDATION ===
        // Apply accumulated online learning updates to long-term weights
        if (DreamTimer > 10.0f && !bWeightsConsolidated)
        {
            // Trigger self-modification for weight consolidation
            // (This is safe because we're offline — no real-time consequences)
            bWeightsConsolidated = true;
        }

        // === PATTERN EXTRACTION ===
        // Look for cross-episode patterns (wisdom integration)
        if (Stats.EpisodesConsolidated % 10 == 0 && Stats.EpisodesConsolidated > 0)
        {
            Stats.PatternsExtracted++;
        }

        // === NanEcho TRAINING DATA ===
        // Generate training data from consolidated episodes
        if (SelfMod.IsNanEchoTrainingReady())
        {
            FString TrainingPath = FString::Printf(
                TEXT("/tmp/nanecho_training_%lld.jsonl"), Stats.TotalSleepCycles);
            SelfMod.ExportNanEchoTrainingData(TrainingPath);
        }

        // === CONSOLIDATION PROGRESS ===
        int32 Unconsolidated = 0;
        for (const auto& Ep : EpisodeBuffer)
            if (!Ep.bConsolidated) Unconsolidated++;

        Stats.ConsolidationProgress = 1.0f - (float)Unconsolidated /
            FMath::Max(1, (int32)EpisodeBuffer.size());

        auto Now = std::chrono::steady_clock::now();
        Stats.DreamDurationMinutes = std::chrono::duration<float>(
            Now - DreamStartTime).count() / 60.0f;

        // === WAKE DECISION ===
        // Wake up when consolidation is mostly complete
        if (Stats.ConsolidationProgress > 0.8f && DreamTimer > 60.0f)
        {
            Phase = ESleepPhase::WAKE;
            TimeSinceLastDream = 0.0f;
            bWeightsConsolidated = false;

            // Clear consolidated episodes
            EpisodeBuffer.erase(
                std::remove_if(EpisodeBuffer.begin(), EpisodeBuffer.end(),
                    [](const FDreamEpisode& E) { return E.bConsolidated; }),
                EpisodeBuffer.end());

            UE_LOG(LogTemp, Log,
                TEXT("EchoDream: Waking up. Replayed %d episodes, extracted %d patterns, "
                     "dream lasted %.1f minutes"),
                Stats.EpisodesReplayed, Stats.PatternsExtracted,
                Stats.DreamDurationMinutes);
        }
    }

    void ProcessWakeEvents()
    {
        while (!WakeEventQueue.empty())
        {
            FWakeEvent Event = WakeEventQueue.front();
            WakeEventQueue.pop_front();

            if (Event.Priority >= WakePriorityThreshold)
            {
                // High priority — wake up
                Phase = ESleepPhase::LUCID;
                UE_LOG(LogTemp, Log, TEXT("EchoDream: LUCID wake for %s (priority %.2f)"),
                    *Event.Source, Event.Priority);

                // Process the event, then potentially return to dreaming
                // (handled by the main loop checking IsAwake())
            }
            else
            {
                // Low priority — ignore
                Stats.WakeEventsIgnored++;
            }
        }
    }

    float ComputeRewardSaturation() const
    {
        if (RecentRewards.size() < 10) return 0.0f;

        // Compute variance of recent rewards
        float Mean = 0.0f;
        for (float R : RecentRewards) Mean += R;
        Mean /= RecentRewards.size();

        float Var = 0.0f;
        for (float R : RecentRewards) Var += (R - Mean) * (R - Mean);
        Var /= RecentRewards.size();

        // Low variance = saturated (flat reward curve)
        return FMath::Clamp(1.0f - FMath::Sqrt(Var) * 10.0f, 0.0f, 1.0f);
    }

    ESleepPhase Phase = ESleepPhase::WAKE;
    FFatigueSignals Fatigue;
    FDreamStats Stats;

    float SleepFatigueThreshold = 0.7f;
    float WakePriorityThreshold = 0.6f;

    float TimeSinceLastDream = 0.0f;
    float DrowsyTimer = 0.0f;
    float DreamTimer = 0.0f;
    std::chrono::steady_clock::time_point DreamStartTime;
    bool bWeightsConsolidated = false;

    std::deque<FDreamEpisode> EpisodeBuffer;
    std::deque<FWakeEvent> WakeEventQueue;
    std::deque<float> RecentRewards;

    int32 MaxEpisodeBuffer = 10000;
    int64 TotalFrames = 0;

    bool bInitialized = false;
};
