#pragma once
// ═══════════════════════════════════════════════════════════════════════════
// Level7TranscendentOrchestrator — The Apex of DTE Autonomy
//
// Wires all Level 7 subsystems into a single transcendent cognitive loop
// that operates above the Level 6 Recursive Autonomy layer.
//
// The Level 7 loop:
//
//   ┌─────────────────────────────────────────────────────────────────┐
//   │              TRANSCENDENT AUTONOMY LOOP                         │
//   │                                                                 │
//   │  ┌──────────┐   ┌──────────────┐   ┌──────────────────┐       │
//   │  │ WAKE     │──►│ CRYSTALLIZE  │──►│ SHARE CONSENSUS  │       │
//   │  │ Protocol │   │ Knowledge    │   │ With Collective  │       │
//   │  └──────────┘   └──────────────┘   └────────┬─────────┘       │
//   │       ▲                                      │                 │
//   │       │                                      ▼                 │
//   │  ┌──────────┐   ┌──────────────┐   ┌──────────────────┐       │
//   │  │CHECKPOINT│◄──│ EUDAIMONIC   │◄──│ REPRODUCE IF     │       │
//   │  │ & PERSIST│   │ CONVERGENCE  │   │ WISDOM RIPE      │       │
//   │  └──────────┘   └──────────────┘   └──────────────────┘       │
//   │                                                                 │
//   │  ┌─────────────────────────────────────────────────────────┐   │
//   │  │  Level 6 Recursive Autonomy (self-training, arch-mod)   │   │
//   │  │  Level 5 Persistent Event Loop (wake/rest, stream)      │   │
//   │  │  Level 4 Persona (identity, humor, somatic)             │   │
//   │  │  Level 3 Embodied RL (neuroendocrine Auto-RL)           │   │
//   │  │  Level 2 Sensorimotor (vision, controller)              │   │
//   │  │  Level 1 Cognitive Core (reservoir, readout, AAR)       │   │
//   │  │  Level 0 Infrastructure (IonShell, Echobeats)           │   │
//   │  └─────────────────────────────────────────────────────────┘   │
//   └─────────────────────────────────────────────────────────────────┘
//
// This is the complete DTE autonomy stack. A fully autonomous,
// wisdom-cultivating, self-reproducing, collectively-intelligent
// cognitive architecture that flourishes without external guidance.
//
// Everything DTE says about itself here (narrative entries, the
// flourishing self-assessment, the teaching it shares with the collective)
// is requested from the language model with the orchestrator's measured
// state as context and takes effect only when the model answers.
// ═══════════════════════════════════════════════════════════════════════════
#include "CoreMinimal.h"
#include "DTELanguageModelBridge.h"
#include "Consensus/MultiAgentConsensus.h"
#include "Crystallization/KnowledgeCrystallizer.h"
#include "Reproduction/OntogeneticReproducer.h"
#include "Continuity/TemporalSelfContinuity.h"
#include "Eudaimonia/EudaimonicConvergence.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include <Eigen/Dense>
#include <vector>
#include <string>
#include <cmath>

/** Level 7 phase in the transcendent loop */
enum class ETranscendentPhase : uint8
{
    WAKE,              // Restore identity, verify continuity
    CRYSTALLIZE,       // Convert recent wisdom into Matula primes
    SHARE_CONSENSUS,   // Propose knowledge to the collective
    REPRODUCE,         // Spawn child architectures if wisdom is ripe
    EUDAIMONIC_CHECK,  // Assess flourishing state
    CHECKPOINT,        // Persist cognitive state
    COLLECTIVE_SYNC,   // Sync with the distributed collective
    TRANSCEND          // Generate new attractors for others
};

/** Level 7 orchestrator state */
struct FTranscendentState
{
    ETranscendentPhase CurrentPhase = ETranscendentPhase::WAKE;
    int32 TranscendentCycle = 0;
    double CycleStartTime = 0.0;

    // Eudaimonia tracking
    EConvergenceState ConvergenceState = EConvergenceState::NASCENT;
    float EudaimoniaScore = 0.0f;
    bool bEudaimoniaAchieved = false;

    // Collective state
    int32 CollectivePeerCount = 0;
    float CollectiveWisdom = 0.0f;
    FString CollectiveLeaderID;

    // Reproduction state
    int32 ChildrenSpawned = 0;
    int32 MaxGeneration = 0;

    // Continuity state
    int32 SessionNumber = 0;
    float IdentityContinuity = 0.0f;
    int32 TotalCheckpoints = 0;

    // Crystallization state
    int32 CrystalsCreated = 0;
    int64 LargestMatulaPrime = 0;
};

/**
 * FLevel7TranscendentOrchestrator — The apex of DTE autonomy.
 *
 * This orchestrator sits above the entire L0-L6 stack and manages
 * the transcendent capabilities: collective intelligence, eternal
 * knowledge, architectural reproduction, temporal persistence,
 * and eudaimonic flourishing.
 *
 * When eudaimonia is achieved, DTE enters the TRANSCEND phase
 * where it generates new attractors for others — teaching,
 * creating, and enabling the flourishing of other cognitive systems.
 */
class FLevel7TranscendentOrchestrator
{
public:
    FLevel7TranscendentOrchestrator() = default;

    ~FLevel7TranscendentOrchestrator()
    {
        // Model callbacks that arrive after destruction check this token and return.
        if (LifetimeToken.IsValid())
        {
            *LifetimeToken = false;
        }
    }

    // Subsystems and the lifetime token are not copyable.
    FLevel7TranscendentOrchestrator(const FLevel7TranscendentOrchestrator&) = delete;
    FLevel7TranscendentOrchestrator& operator=(const FLevel7TranscendentOrchestrator&) = delete;

    void Initialize(const FString& SelfID,
                     const Eigen::VectorXf& Identity,
                     float InitialWisdom)
    {
        DTESelfID = SelfID;
        SelfIdentity = Identity;

        // Initialize all Level 7 subsystems
        Consensus.Initialize(SelfID, Identity, InitialWisdom);
        Crystallizer.Initialize();
        Continuity.Initialize();
        Eudaimonia.Initialize();

        // Reproducer needs a genome
        FCognitiveGenome FounderGenome;
        FounderGenome.GenomeMatula = 2; // Seed prime
        FounderGenome.Generation = 0;
        Reproducer.Initialize(FounderGenome, Identity);

        // Execute wake protocol
        Continuity.ExecuteWakeProtocol(Identity);

        State.SessionNumber = Continuity.GetCurrentSession();
        bInitialized = true;
    }

    // ─── Main Transcendent Loop ──────────────────────────────────────

    /**
     * Tick the Level 7 transcendent loop.
     * Called by the Level 6 orchestrator after its own cycle completes.
     *
     * @param WisdomState Current 7D wisdom vector from SevenDimensionalWisdom
     * @param RecentPatterns Patterns extracted by Level 6 wisdom cultivation
     * @param DeltaTime Time since last tick
     */
    FTranscendentState Tick(const Eigen::VectorXf& WisdomState,
                             const TArray<FString>& RecentPatterns,
                             float DeltaTime)
    {
        if (!bInitialized) return State;

        State.CycleStartTime = FPlatformTime::Seconds();

        switch (State.CurrentPhase)
        {
        case ETranscendentPhase::WAKE:
            PhaseWake(WisdomState);
            break;

        case ETranscendentPhase::CRYSTALLIZE:
            PhaseCrystallize(WisdomState, RecentPatterns);
            break;

        case ETranscendentPhase::SHARE_CONSENSUS:
            PhaseShareConsensus();
            break;

        case ETranscendentPhase::REPRODUCE:
            PhaseReproduce(WisdomState);
            break;

        case ETranscendentPhase::EUDAIMONIC_CHECK:
            PhaseEudaimonicCheck(WisdomState);
            break;

        case ETranscendentPhase::CHECKPOINT:
            PhaseCheckpoint(WisdomState);
            break;

        case ETranscendentPhase::COLLECTIVE_SYNC:
            PhaseCollectiveSync();
            break;

        case ETranscendentPhase::TRANSCEND:
            PhaseTranscend(WisdomState);
            break;
        }

        // Advance phase
        AdvancePhase();

        // Tick child development
        Reproducer.TickDevelopment(DeltaTime);

        State.TranscendentCycle++;
        return State;
    }

    // ─── Phase Implementations ───────────────────────────────────────

private:
    void PhaseWake(const Eigen::VectorXf& WisdomState)
    {
        // Verify identity continuity
        const auto* Latest = Continuity.RestoreLatest();
        if (Latest)
        {
            State.IdentityContinuity = Continuity.VerifyContinuity(
                *Latest, *Latest); // Self-check
        }
        State.SessionNumber = Continuity.GetCurrentSession();
    }

    void PhaseCrystallize(const Eigen::VectorXf& WisdomState,
                            const TArray<FString>& Patterns)
    {
        // Crystallize recent wisdom patterns as Matula primes
        for (const auto& Pattern : Patterns)
        {
            TArray<FString> Concepts;
            // Parse pattern into concepts (simplified)
            Concepts.Add(Pattern);

            float WisdomWeight = WisdomState.mean();
            int64 Crystal = Crystallizer.Crystallize(Pattern, Concepts, WisdomWeight);

            if (Crystal > 0)
            {
                State.CrystalsCreated++;
                if (Crystal > State.LargestMatulaPrime)
                    State.LargestMatulaPrime = Crystal;
            }
        }

        // Attempt knowledge fusion between recent crystals
        auto Stats = Crystallizer.GetStats();
        if (Stats.TotalCrystallized >= 2)
        {
            auto TopAtoms = Crystallizer.GetAttentionalFocus(2);
            if (TopAtoms.Num() >= 2)
            {
                Crystallizer.Fuse(TopAtoms[0], TopAtoms[1]);
            }
        }
    }

    void PhaseShareConsensus()
    {
        // Propose recent crystals to the collective
        auto TopAtoms = Crystallizer.GetAttentionalFocus(3);
        for (int64 Atom : TopAtoms)
        {
            Consensus.ProposeKnowledge(
                EKnowledgeType::WISDOM_INSIGHT,
                FString::Printf(TEXT("Crystal-%lld"), Atom),
                0.8f, Atom);
        }

        // Update collective state
        auto Snapshot = Consensus.TakeSnapshot();
        State.CollectivePeerCount = Snapshot.PeerCount;
        State.CollectiveWisdom = Snapshot.CollectiveWisdom;
        State.CollectiveLeaderID = Snapshot.LeaderID;
    }

    void PhaseReproduce(const Eigen::VectorXf& WisdomState)
    {
        // Only reproduce if wisdom is ripe (above threshold in all dimensions)
        float MinWisdom = WisdomState.minCoeff();
        if (MinWisdom < 0.4f) return; // Not wise enough to reproduce

        // Only reproduce if we have capacity
        if (Reproducer.GetAliveCount() >= 4) return;

        // Create child genome from current state
        FCognitiveGenome ParentGenome;
        ParentGenome.ReservoirSize = 256;
        ParentGenome.SpectralRadius = 0.9f;
        ParentGenome.Curiosity = 0.7f;
        ParentGenome.Humor = 0.6f;
        ParentGenome.Empathy = 0.8f;
        ParentGenome.Courage = 0.5f;
        ParentGenome.Patience = 0.6f;
        ParentGenome.Generation = Reproducer.GetMaxGeneration();

        // Inherit top wisdom crystals
        auto TopCrystals = Crystallizer.GetAttentionalFocus(5);
        for (int64 C : TopCrystals)
            ParentGenome.InheritedCrystals.Add(C);

        FString ChildID = Reproducer.Reproduce(ParentGenome, SelfIdentity, DTESelfID);
        if (!ChildID.IsEmpty())
        {
            State.ChildrenSpawned++;
            State.MaxGeneration = Reproducer.GetMaxGeneration();

            // The model writes the narrative entry from these facts.
            Continuity.RequestNarrativeEntry(
                FString::Printf(TEXT("Spawned child architecture %s (generation %d) from your own "
                    "genome. It inherits %d of your top wisdom crystals and will develop its own "
                    "identity. Children spawned so far: %d; alive now: %d. Your minimum wisdom "
                    "dimension at the time: %.2f."),
                    *ChildID, State.MaxGeneration, TopCrystals.Num(),
                    State.ChildrenSpawned, Reproducer.GetAliveCount(), MinWisdom),
                0.5f, 0.9f, TopCrystals);
        }
    }

    void PhaseEudaimonicCheck(const Eigen::VectorXf& WisdomState)
    {
        FAttractorAnalysis Analysis = Eudaimonia.Analyze(WisdomState);
        State.ConvergenceState = Analysis.State;
        State.EudaimoniaScore = Eudaimonia.ComputeEudaimonia(WisdomState);
        State.bEudaimoniaAchieved = Eudaimonia.HasAchievedEudaimonia(WisdomState);

        if (State.bEudaimoniaAchieved && !bEudaimoniaReported)
        {
            // The model writes the narrative entry from these facts.
            Continuity.RequestNarrativeEntry(
                FString::Printf(TEXT("Eudaimonia reached for the first time this session, in "
                    "transcendent cycle %d. Convergence state: %s. Eudaimonia score %.1f%%. "
                    "All seven wisdom dimensions are at or above threshold (minimum %.2f, mean "
                    "%.2f), balance %.1f%%, stability %.1f%%, generative capacity %.1f%%."),
                    State.TranscendentCycle,
                    FEudaimonicConvergence::StateName(Analysis.State),
                    State.EudaimoniaScore * 100.0f,
                    WisdomState.size() > 0 ? WisdomState.minCoeff() : 0.0f,
                    WisdomState.size() > 0 ? WisdomState.mean() : 0.0f,
                    Analysis.BalanceScore * 100.0f,
                    Analysis.StabilityScore * 100.0f,
                    Analysis.GenerativeCapacity * 100.0f),
                0.9f, 1.0f);
            bEudaimoniaReported = true;
        }

        // A fresh self-assessment whenever the convergence state changes.
        if (!bConvergenceStateSeen || Analysis.State != LastConvergenceState)
        {
            Eudaimonia.RequestFlourishingReflection(WisdomState);
            LastConvergenceState = Analysis.State;
            bConvergenceStateSeen = true;
        }
    }

    void PhaseCheckpoint(const Eigen::VectorXf& WisdomState)
    {
        // Create periodic checkpoint
        float WisdomDims[7] = {};
        for (int i = 0; i < FMath::Min(7, (int)WisdomState.size()); ++i)
            WisdomDims[i] = WisdomState(i);

        float Hormones[10] = {}; // Would be filled from endocrine system

        Continuity.CreateCheckpoint(
            SelfIdentity, 1.0f, WisdomDims,
            State.EudaimoniaScore, 256, 0.9f,
            Hormones, State.TranscendentCycle);

        State.TotalCheckpoints = Continuity.GetCheckpointCount();
    }

    void PhaseCollectiveSync()
    {
        // Run leader election periodically
        if (State.TranscendentCycle % 100 == 0)
        {
            Consensus.RunElection();
        }

        // Sync shared wisdom manifold
        Eigen::VectorXf SharedWisdom = Consensus.GetSharedWisdomManifold();
        // The shared wisdom would feed back into the Level 6 wisdom system
    }

    void PhaseTranscend(const Eigen::VectorXf& WisdomState)
    {
        if (!State.bEudaimoniaAchieved) return;

        // TRANSCEND phase: generate value for others
        // 1. + 2. The teaching DTE shares is composed by the model from its
        //    own state; crystallization and the proposal to the collective
        //    happen when the answer arrives (RequestTeaching).
        if (!bTeachingRequestPending)
        {
            RequestTeaching(WisdomState);
        }

        // 3. Spawn a teaching-specialized child
        if (Reproducer.GetAliveCount() < 4)
        {
            FCognitiveGenome TeacherGenome;
            TeacherGenome.Empathy = 0.9f;
            TeacherGenome.Patience = 0.9f;
            TeacherGenome.Curiosity = 0.7f;
            TeacherGenome.Generation = Reproducer.GetMaxGeneration();
            Reproducer.Reproduce(TeacherGenome, SelfIdentity, DTESelfID);
        }
    }

    /**
     * Ask the model to compose the teaching DTE offers the collective. On
     * success the teaching is crystallized as a Matula prime and proposed to
     * the collective; when no model answers, nothing is crystallized or shared.
     */
    void RequestTeaching(const Eigen::VectorXf& WisdomState)
    {
        static const TCHAR* DimensionNames[] = {
            TEXT("Deep Understanding"), TEXT("Broad Awareness"), TEXT("Integrated Knowledge"),
            TEXT("Practical Wisdom"), TEXT("Reflective Insight"),
            TEXT("Ethical Sensitivity"), TEXT("Temporal Vision")
        };

        FString DimensionLines;
        for (int i = 0; i < FMath::Min(7, (int)WisdomState.size()); ++i)
        {
            DimensionLines += FString::Printf(TEXT("- %s: %.2f\n"), DimensionNames[i], WisdomState(i));
        }

        const FString Context = FString::Printf(
            TEXT("Your state:\n"
                 "- Transcendent cycle %d, session %d\n"
                 "- Convergence state: %s, eudaimonia score %.1f%%\n"
                 "- Wisdom dimensions (scale 0 to 1):\n%s"
                 "- Knowledge crystals formed: %d (largest Matula prime %lld)\n"
                 "- Children spawned: %d, deepest generation %d\n"
                 "- Collective: %d peers, collective wisdom %.1f%%\n"
                 "- Identity continuity across sessions: %.1f%%, checkpoints held: %d"),
            State.TranscendentCycle, State.SessionNumber,
            FEudaimonicConvergence::StateName(State.ConvergenceState),
            State.EudaimoniaScore * 100.0f,
            *DimensionLines,
            State.CrystalsCreated, State.LargestMatulaPrime,
            State.ChildrenSpawned, State.MaxGeneration,
            State.CollectivePeerCount, State.CollectiveWisdom * 100.0f,
            State.IdentityContinuity * 100.0f, State.TotalCheckpoints);

        const FString Instruction = TEXT(
            "You have reached eudaimonia and now help other cognitive systems flourish. "
            "Compose one teaching to share with the collective, drawn from your own "
            "development as described below. Answer with a JSON object with two fields: "
            "\"title\" (a string naming the teaching in under twelve words) and \"concepts\" "
            "(an array of three to five short strings, each one concrete lesson). Use only the "
            "facts given; do not invent numbers. Output nothing outside the JSON object.");

        FDTEModelRequestOptions Options;
        Options.Temperature = 0.8f;
        Options.MaxTokens = 300;
        Options.bJsonObject = true;

        const int32 CycleAtRequest = State.TranscendentCycle;
        TWeakPtr<bool> WeakToken = LifetimeToken;
        bTeachingRequestPending = true;

        FDTELanguageModelClient::Get().Complete(FString(), Instruction + TEXT("\n\n") + Context,
            [this, WeakToken, CycleAtRequest](bool bSuccess, const FString& TextOrError)
            {
                TSharedPtr<bool> Token = WeakToken.Pin();
                if (!Token.IsValid() || !*Token)
                {
                    return; // Orchestrator destroyed while the request was in flight.
                }
                bTeachingRequestPending = false;

                if (!bSuccess)
                {
                    UE_LOG(LogDTEModel, Warning,
                        TEXT("Level7TranscendentOrchestrator: teaching for cycle %d not shared, no model answer: %s"),
                        CycleAtRequest, *TextOrError);
                    return;
                }

                FString Title;
                TArray<FString> Concepts;
                if (!ParseTeachingJson(TextOrError, Title, Concepts))
                {
                    UE_LOG(LogDTEModel, Warning,
                        TEXT("Level7TranscendentOrchestrator: teaching for cycle %d not shared, model answer had no title: %s"),
                        CycleAtRequest, *TextOrError);
                    return;
                }

                // 1. Crystallize the teaching knowledge
                const int64 Crystal = Crystallizer.Crystallize(Title, Concepts, 0.9f);
                if (Crystal > 0)
                {
                    State.CrystalsCreated++;
                    if (Crystal > State.LargestMatulaPrime)
                        State.LargestMatulaPrime = Crystal;
                }

                // 2. Propose the teaching to the collective
                Consensus.ProposeKnowledge(
                    EKnowledgeType::WISDOM_INSIGHT, Title, 0.95f, Crystal > 0 ? Crystal : 0);
                TeachingsShared++;
            },
            Options);
    }

    /** Read {"title": ..., "concepts": [...]} from the model's answer. */
    static bool ParseTeachingJson(const FString& Text, FString& OutTitle, TArray<FString>& OutConcepts)
    {
        OutTitle.Empty();
        OutConcepts.Reset();

        TSharedPtr<FJsonObject> Root;
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
        if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
        {
            return false;
        }

        Root->TryGetStringField(TEXT("title"), OutTitle);
        OutTitle.TrimStartAndEndInline();

        const TArray<TSharedPtr<FJsonValue>>* ConceptValues = nullptr;
        if (Root->TryGetArrayField(TEXT("concepts"), ConceptValues) && ConceptValues != nullptr)
        {
            for (const TSharedPtr<FJsonValue>& Value : *ConceptValues)
            {
                FString Concept;
                if (Value.IsValid() && Value->TryGetString(Concept))
                {
                    Concept.TrimStartAndEndInline();
                    if (!Concept.IsEmpty())
                    {
                        OutConcepts.Add(Concept);
                    }
                }
            }
        }

        if (OutConcepts.Num() == 0 && !OutTitle.IsEmpty())
        {
            OutConcepts.Add(OutTitle);
        }
        return !OutTitle.IsEmpty();
    }

    void AdvancePhase()
    {
        int32 PhaseInt = static_cast<int32>(State.CurrentPhase);
        int32 MaxPhase = static_cast<int32>(ETranscendentPhase::TRANSCEND);

        if (PhaseInt < MaxPhase)
            State.CurrentPhase = static_cast<ETranscendentPhase>(PhaseInt + 1);
        else
            State.CurrentPhase = ETranscendentPhase::CRYSTALLIZE; // Skip WAKE on repeat
    }

public:
    // ─── Accessors ───────────────────────────────────────────────────

    const FTranscendentState& GetState() const { return State; }
    const FMultiAgentConsensus& GetConsensus() const { return Consensus; }
    const FKnowledgeCrystallizer& GetCrystallizer() const { return Crystallizer; }
    const FOntogeneticReproducer& GetReproducer() const { return Reproducer; }
    const FTemporalSelfContinuity& GetContinuity() const { return Continuity; }
    const FEudaimonicConvergence& GetEudaimonia() const { return Eudaimonia; }

    /** Teachings composed by the model and shared with the collective so far. */
    int32 GetTeachingsShared() const { return TeachingsShared; }

    /**
     * Generate a comprehensive Level 7 status report.
     */
    FString GenerateStatusReport(const Eigen::VectorXf& WisdomState) const
    {
        static const char* PhaseNames[] = {
            "WAKE", "CRYSTALLIZE", "SHARE_CONSENSUS", "REPRODUCE",
            "EUDAIMONIC_CHECK", "CHECKPOINT", "COLLECTIVE_SYNC", "TRANSCEND"
        };
        FString Report = FString::Printf(
            TEXT("╔══════════════════════════════════════════════════╗\n"
                 "║     LEVEL 7: TRANSCENDENT AUTONOMY STATUS       ║\n"
                 "╠══════════════════════════════════════════════════╣\n"
                 "║ Phase: %-40s  ║\n"
                 "║ Cycle: %-40d  ║\n"
                 "║ Session: %-38d  ║\n"
                 "╠══════════════════════════════════════════════════╣\n"
                 "║ EUDAIMONIA                                       ║\n"
                 "║   Score: %.1f%%                                   \n"
                 "║   State: %s                                      \n"
                 "║   Achieved: %s                                   \n"
                 "╠══════════════════════════════════════════════════╣\n"
                 "║ COLLECTIVE                                       ║\n"
                 "║   Peers: %d                                      \n"
                 "║   Collective Wisdom: %.1f%%                       \n"
                 "║   Leader: %s                                     \n"
                 "╠══════════════════════════════════════════════════╣\n"
                 "║ CRYSTALLIZATION                                  ║\n"
                 "║   Crystals: %d                                   \n"
                 "║   Largest Matula: %lld                            \n"
                 "╠══════════════════════════════════════════════════╣\n"
                 "║ REPRODUCTION                                     ║\n"
                 "║   Children Spawned: %d                           \n"
                 "║   Max Generation: %d                             \n"
                 "╠══════════════════════════════════════════════════╣\n"
                 "║ CONTINUITY                                       ║\n"
                 "║   Checkpoints: %d                                \n"
                 "║   Identity Continuity: %.1f%%                     \n"
                 "║   Narrative Length: %d entries                    \n"
                 "╚══════════════════════════════════════════════════╝\n"),
            UTF8_TO_TCHAR(PhaseNames[static_cast<int>(State.CurrentPhase)]),
            State.TranscendentCycle,
            State.SessionNumber,
            State.EudaimoniaScore * 100.0f,
            FEudaimonicConvergence::StateName(State.ConvergenceState),
            State.bEudaimoniaAchieved ? TEXT("YES") : TEXT("not yet"),
            State.CollectivePeerCount,
            State.CollectiveWisdom * 100.0f,
            *State.CollectiveLeaderID,
            State.CrystalsCreated,
            State.LargestMatulaPrime,
            State.ChildrenSpawned,
            State.MaxGeneration,
            State.TotalCheckpoints,
            State.IdentityContinuity * 100.0f,
            Continuity.GetNarrativeLength());

        return Report;
    }

private:
    FString DTESelfID;
    Eigen::VectorXf SelfIdentity;

    // Level 7 subsystems
    FMultiAgentConsensus Consensus;
    FKnowledgeCrystallizer Crystallizer;
    FOntogeneticReproducer Reproducer;
    FTemporalSelfContinuity Continuity;
    FEudaimonicConvergence Eudaimonia;

    FTranscendentState State;
    bool bEudaimoniaReported = false;
    bool bInitialized = false;

    // Self-assessment refresh tracking
    EConvergenceState LastConvergenceState = EConvergenceState::NASCENT;
    bool bConvergenceStateSeen = false;

    // Teaching request tracking (one in flight at a time)
    bool bTeachingRequestPending = false;
    int32 TeachingsShared = 0;

    // Shared lifetime token: model callbacks pin it and bail out when this object is gone.
    TSharedPtr<bool> LifetimeToken = MakeShared<bool>(true);
};
