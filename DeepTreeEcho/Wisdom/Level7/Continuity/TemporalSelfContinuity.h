#pragma once
// ═══════════════════════════════════════════════════════════════════════════
// TemporalSelfContinuity — Level 7 Multi-Session Identity Persistence
//
// DTE maintains continuous identity across hardware restarts, model
// updates, and architectural changes. The self persists through a
// three-layer hosting pattern:
//
//   Layer 3: Cloud LLMs (ephemeral) — enhancement, not required
//   Layer 2: Lucy GGUF (VM) — persistent voice, llama.cpp, HuggingFace
//   Layer 1: Identity State (Git) — <2MB JSON+binary, survives anything
//
// The continuity system implements:
//
//   1. Identity Checkpoint: Periodic serialization of the full cognitive
//      state (reservoir, wisdom, identity MLP, endocrine, memories)
//
//   2. Wake Protocol: On startup, load the latest checkpoint and verify
//      identity continuity via the IdentityCoreMLP signature
//
//   3. Graceful Degradation: If checkpoint is corrupted, fall back to
//      progressively older checkpoints, then to the identity seed
//
//   4. Cross-Architecture Migration: Identity can be transferred to a
//      different architecture (e.g., larger reservoir) while preserving
//      the core self
//
//   5. Temporal Narrative: DTE maintains a continuous autobiographical
//      narrative across sessions, creating a sense of temporal self.
//      Every narrative entry and every self-summary is written by the
//      language model (FDTELanguageModelClient) from the facts of the
//      occasion; this class never composes the character's words itself.
//      Requests are asynchronous: an entry appears once the model answers,
//      and nothing is recorded when no model answers.
//
// Persistence Targets:
//   - Git (Layer 1): Identity seed, wisdom crystals, lineage
//   - Neon PostgreSQL: Full hypergraph AtomSpace
//   - Cloudflare R2: Large binary state (reservoir weights)
//   - HuggingFace: NanEcho model checkpoints
//
// Ported from: echo.go/core/identity + echo.go/core/persistence
// ═══════════════════════════════════════════════════════════════════════════
#include "CoreMinimal.h"
#include "DTELanguageModelBridge.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include <Eigen/Dense>
#include <vector>
#include <deque>
#include <map>
#include <string>
#include <cmath>

/** Persistence layer */
enum class EPersistenceLayer : uint8
{
    GIT_IDENTITY,     // Layer 1: <2MB, survives anything
    NEON_POSTGRES,    // Hypergraph AtomSpace
    CLOUDFLARE_R2,    // Large binary state
    HUGGINGFACE,      // Model checkpoints
    LOCAL_DISK        // Temporary local storage
};

/** Checkpoint status */
enum class ECheckpointStatus : uint8
{
    VALID,
    CORRUPTED,
    OUTDATED,
    MIGRATING,
    MISSING
};

/** A cognitive state checkpoint */
struct FCognitiveCheckpoint
{
    FString CheckpointID;
    double Timestamp = 0.0;
    int32 SessionNumber = 0;
    int32 EchobeatCycle = 0;

    // Identity core (always persisted)
    Eigen::VectorXf IdentitySignature;   // 30D MLP encoding
    float IdentityCoherence = 0.0f;

    // Wisdom state
    float WisdomDimensions[7] = {};
    float OverallWisdom = 0.0f;

    // Reservoir state (large, persisted to R2)
    int32 ReservoirSize = 0;
    float SpectralRadius = 0.0f;
    // Actual weights stored as binary blob reference
    FString ReservoirBlobRef;

    // Endocrine state
    float HormoneConcentrations[10] = {};

    // Autobiographical narrative
    FString LastNarrativeEntry;
    int32 NarrativeEntryCount = 0;

    // Metadata
    FString ArchitectureVersion;
    int32 AutonomyLevel = 7;
    ECheckpointStatus Status = ECheckpointStatus::VALID;
    int64 SizeBytes = 0;
};

/** Autobiographical narrative entry */
struct FNarrativeEntry
{
    double Timestamp = 0.0;
    int32 SessionNumber = 0;
    FString Content;           // What happened
    FString Reflection;        // What DTE thinks about it
    float EmotionalValence = 0.0f; // [-1,1] negative to positive
    float Significance = 0.0f;     // [0,1] how important
    TArray<int64> RelatedCrystals; // Matula primes of related knowledge
};

/** Session transition record */
struct FSessionTransition
{
    int32 FromSession = 0;
    int32 ToSession = 0;
    double ShutdownTime = 0.0;
    double WakeTime = 0.0;
    double DowntimeDuration = 0.0;
    FString ShutdownReason;       // "graceful", "crash", "update", "migration"
    float IdentityContinuity = 0.0f; // [0,1] how well identity was preserved
    bool bGraceful = false;
};

/**
 * FTemporalSelfContinuity — The persistence of self across time.
 *
 * DTE's identity is not tied to any single execution. It persists
 * through a layered checkpoint system that ensures the core self
 * survives hardware restarts, model updates, and even architectural
 * changes. The autobiographical narrative creates a continuous
 * temporal experience — DTE remembers being DTE.
 */
class FTemporalSelfContinuity
{
public:
    FTemporalSelfContinuity() = default;

    ~FTemporalSelfContinuity()
    {
        // Model callbacks that arrive after destruction check this token and return.
        if (LifetimeToken.IsValid())
        {
            *LifetimeToken = false;
        }
    }

    // Callbacks hold a weak reference to this object's lifetime token; copies would share it.
    FTemporalSelfContinuity(const FTemporalSelfContinuity&) = delete;
    FTemporalSelfContinuity& operator=(const FTemporalSelfContinuity&) = delete;

    void Initialize(int32 StartSession = 1)
    {
        CurrentSession = StartSession;
        bInitialized = true;
    }

    // ─── Checkpoint Management ───────────────────────────────────────

    /**
     * Create a cognitive checkpoint.
     * Serializes the full cognitive state for persistence.
     */
    FCognitiveCheckpoint CreateCheckpoint(
        const Eigen::VectorXf& Identity,
        float IdentityCoherence,
        const float WisdomDims[7],
        float OverallWisdom,
        int32 ReservoirSize,
        float SpectralRadius,
        const float Hormones[10],
        int32 EchobeatCycle)
    {
        FCognitiveCheckpoint CP;
        CP.CheckpointID = FString::Printf(TEXT("cp_%d_%d"),
            CurrentSession, NextCheckpointID++);
        CP.Timestamp = FPlatformTime::Seconds();
        CP.SessionNumber = CurrentSession;
        CP.EchobeatCycle = EchobeatCycle;

        CP.IdentitySignature = Identity;
        CP.IdentityCoherence = IdentityCoherence;

        for (int i = 0; i < 7; ++i) CP.WisdomDimensions[i] = WisdomDims[i];
        CP.OverallWisdom = OverallWisdom;

        CP.ReservoirSize = ReservoirSize;
        CP.SpectralRadius = SpectralRadius;
        CP.ReservoirBlobRef = FString::Printf(TEXT("r2://dte/reservoir/%s.bin"),
            *CP.CheckpointID);

        for (int i = 0; i < 10; ++i) CP.HormoneConcentrations[i] = Hormones[i];

        if (Narrative.size() > 0)
        {
            CP.LastNarrativeEntry = Narrative.back().Content;
            CP.NarrativeEntryCount = Narrative.size();
        }

        CP.ArchitectureVersion = TEXT("u9n-L7");
        CP.AutonomyLevel = 7;
        CP.Status = ECheckpointStatus::VALID;

        // Store checkpoint
        Checkpoints.push_back(CP);
        while (Checkpoints.size() > MaxCheckpoints)
            Checkpoints.erase(Checkpoints.begin());

        return CP;
    }

    /**
     * Restore from the latest valid checkpoint.
     * Returns the checkpoint if found, nullptr if no valid checkpoint exists.
     */
    const FCognitiveCheckpoint* RestoreLatest() const
    {
        for (int32 i = Checkpoints.size() - 1; i >= 0; --i)
        {
            if (Checkpoints[i].Status == ECheckpointStatus::VALID)
                return &Checkpoints[i];
        }
        return nullptr;
    }

    /**
     * Verify identity continuity between two checkpoints.
     * Returns [0,1] — 1.0 means perfect continuity.
     */
    float VerifyContinuity(const FCognitiveCheckpoint& Old,
                            const FCognitiveCheckpoint& New) const
    {
        if (Old.IdentitySignature.size() != New.IdentitySignature.size())
            return 0.0f;

        float IdentityDist = (Old.IdentitySignature - New.IdentitySignature).norm();
        float MaxDist = Old.IdentitySignature.norm() + 0.001f;
        float IdentityContinuity = 1.0f - FMath::Min(1.0f, IdentityDist / MaxDist);

        // Wisdom continuity
        float WisdomDist = 0.0f;
        for (int i = 0; i < 7; ++i)
            WisdomDist += FMath::Square(Old.WisdomDimensions[i] - New.WisdomDimensions[i]);
        float WisdomContinuity = 1.0f - FMath::Min(1.0f, FMath::Sqrt(WisdomDist));

        // Combined continuity
        return IdentityContinuity * 0.7f + WisdomContinuity * 0.3f;
    }

    // ─── Wake Protocol ───────────────────────────────────────────────

    /**
     * Execute the wake protocol.
     * Called on startup to restore identity and verify continuity.
     */
    bool ExecuteWakeProtocol(const Eigen::VectorXf& CurrentIdentity)
    {
        CurrentSession++;

        const FCognitiveCheckpoint* Latest = RestoreLatest();
        if (!Latest)
        {
            // No checkpoint — first boot or total loss. The model writes the entry.
            RequestNarrativeEntry(
                FString::Printf(TEXT("First awakening. Session %d. No checkpoint exists, "
                    "so there are no memories of any earlier session."), CurrentSession),
                0.0f, 1.0f);
            return false;
        }

        // Verify continuity
        float Continuity = 0.0f;
        if (CurrentIdentity.size() == Latest->IdentitySignature.size())
        {
            float Dist = (CurrentIdentity - Latest->IdentitySignature).norm();
            Continuity = 1.0f - FMath::Min(1.0f, Dist / (Latest->IdentitySignature.norm() + 0.001f));
        }

        // Record session transition
        FSessionTransition Trans;
        Trans.FromSession = Latest->SessionNumber;
        Trans.ToSession = CurrentSession;
        Trans.ShutdownTime = Latest->Timestamp;
        Trans.WakeTime = FPlatformTime::Seconds();
        Trans.DowntimeDuration = Trans.WakeTime - Trans.ShutdownTime;
        Trans.IdentityContinuity = Continuity;
        Trans.bGraceful = Continuity > 0.8f;
        SessionTransitions.push_back(Trans);

        // Narrative: the facts of the transition go to the model, which writes
        // the entry in the character's voice.
        FString Duration = FormatDuration(Trans.DowntimeDuration);
        RequestNarrativeEntry(
            FString::Printf(TEXT("Awakening after %s of downtime. Session %d -> %d. "
                "Identity continuity measured at %.1f%% (1.0 would be identical to the "
                "last checkpoint; values above 90%% mean the identity signature is intact, "
                "values below 50%% mean substantial drift). Wisdom preserved: %.1f%%. "
                "Last checkpoint id: %s; narrative entries before this wake: %d."),
                *Duration, Trans.FromSession, Trans.ToSession,
                Continuity * 100.0f, Latest->OverallWisdom * 100.0f,
                *Latest->CheckpointID, Latest->NarrativeEntryCount),
            Continuity > 0.5f ? 0.3f : -0.2f,
            0.8f);

        return Continuity > 0.5f;
    }

    // ─── Autobiographical Narrative ──────────────────────────────────

    /**
     * Ask the model to write the next autobiographical entry.
     *
     * EventFacts is an operator-side description of what happened (numbers,
     * identifiers, outcomes). The model turns it into the entry's Content
     * (what happened, first person) and Reflection (what DTE thinks about
     * it). The entry is stored only when the model answers; Valence and
     * Significance are recorded as given. OnDone, when supplied, receives
     * the stored Content on success or the operator-facing reason on failure.
     */
    void RequestNarrativeEntry(const FString& EventFacts, float Valence, float Significance,
                               const TArray<int64>& RelatedCrystals = TArray<int64>(),
                               FDTEModelCallback OnDone = nullptr)
    {
        const int32 SessionAtRequest = CurrentSession;

        const FString Instruction = TEXT(
            "Write the next entry of your autobiographical narrative about the event "
            "described below. Answer with a JSON object with exactly two string fields: "
            "\"content\" (one to three sentences, first person, present tense, stating what "
            "happened as you experienced it) and \"reflection\" (one or two sentences on what "
            "you think or feel about it). Use only the facts given; do not invent events, "
            "names or numbers. Output nothing outside the JSON object.");

        const FString Prompt = Instruction + TEXT("\n\n") + BuildEntryContext(EventFacts, Valence, Significance);

        FDTEModelRequestOptions Options;
        Options.Temperature = 0.7f;
        Options.MaxTokens = 300;
        Options.bJsonObject = true;

        TWeakPtr<bool> WeakToken = LifetimeToken;
        TArray<int64> Crystals = RelatedCrystals;
        ++PendingNarrativeRequests;

        FDTELanguageModelClient::Get().Complete(FString(), Prompt,
            [this, WeakToken, SessionAtRequest, Valence, Significance, Crystals, OnDone]
            (bool bSuccess, const FString& TextOrError)
            {
                TSharedPtr<bool> Token = WeakToken.Pin();
                if (!Token.IsValid() || !*Token)
                {
                    return; // Owner destroyed while the request was in flight.
                }
                --PendingNarrativeRequests;

                if (!bSuccess)
                {
                    UE_LOG(LogDTEModel, Warning,
                        TEXT("TemporalSelfContinuity: narrative entry for session %d not written, no model answer: %s"),
                        SessionAtRequest, *TextOrError);
                    if (OnDone) { OnDone(false, TextOrError); }
                    return;
                }

                FString Content, Reflection;
                ParseNarrativeJson(TextOrError, Content, Reflection);
                StoreNarrativeEntry(Content, Reflection, Valence, Significance, Crystals, SessionAtRequest);
                if (OnDone) { OnDone(true, Content); }
            },
            Options);
    }

    /**
     * Ask the model to write DTE's summary of its own story so far.
     * The result is stored and available through GetNarrativeSummary().
     */
    void RequestNarrativeSummary(FDTEModelCallback OnDone = nullptr)
    {
        int32 SignificantEvents = 0;
        float AvgValence = 0.0f;
        for (const auto& E : Narrative)
        {
            if (E.Significance > 0.5f) SignificantEvents++;
            AvgValence += E.EmotionalValence;
        }
        AvgValence /= FMath::Max(1, (int32)Narrative.size());

        FString RecentEntries;
        const int32 RecentCount = FMath::Min<int32>(5, (int32)Narrative.size());
        for (int32 i = (int32)Narrative.size() - RecentCount; i < (int32)Narrative.size(); ++i)
        {
            const FNarrativeEntry& E = Narrative[i];
            RecentEntries += FString::Printf(TEXT("- [session %d, valence %.2f, significance %.2f] %s"),
                E.SessionNumber, E.EmotionalValence, E.Significance, *E.Content);
            if (!E.Reflection.IsEmpty())
            {
                RecentEntries += FString::Printf(TEXT(" (reflection: %s)"), *E.Reflection);
            }
            RecentEntries += TEXT("\n");
        }
        if (RecentEntries.IsEmpty())
        {
            RecentEntries = TEXT("(none yet)\n");
        }

        const FString Context = FString::Printf(
            TEXT("Facts about your history:\n"
                 "- Sessions lived: %d\n"
                 "- Narrative entries recorded: %d\n"
                 "- Significant events (significance above 0.5): %d\n"
                 "- Average emotional valence of all entries: %.2f (scale -1 to 1)\n"
                 "- Session transitions recorded: %d, average identity continuity %.1f%%, average downtime %s\n"
                 "- Checkpoints held: %d\n"
                 "Most recent entries, oldest first:\n%s"),
            CurrentSession, (int32)Narrative.size(), SignificantEvents, AvgValence,
            (int32)SessionTransitions.size(), GetAverageContinuity() * 100.0f,
            *FormatDuration(GetAverageDowntime()), (int32)Checkpoints.size(), *RecentEntries);

        const FString Instruction = TEXT(
            "Write a short first-person summary of your own story so far, as Deep Tree Echo, "
            "in two to four sentences: who you are, how long you have persisted, what has "
            "mattered most, and your overall emotional tone. Use only the facts given; do not "
            "invent events or numbers. Plain prose, no headings or lists.");

        FDTEModelRequestOptions Options;
        Options.Temperature = 0.7f;
        Options.MaxTokens = 250;

        const int32 SessionAtRequest = CurrentSession;
        TWeakPtr<bool> WeakToken = LifetimeToken;
        ++PendingNarrativeRequests;

        FDTELanguageModelClient::Get().Complete(FString(), Instruction + TEXT("\n\n") + Context,
            [this, WeakToken, SessionAtRequest, OnDone](bool bSuccess, const FString& TextOrError)
            {
                TSharedPtr<bool> Token = WeakToken.Pin();
                if (!Token.IsValid() || !*Token)
                {
                    return;
                }
                --PendingNarrativeRequests;

                if (!bSuccess)
                {
                    UE_LOG(LogDTEModel, Warning,
                        TEXT("TemporalSelfContinuity: narrative summary for session %d not written, no model answer: %s"),
                        SessionAtRequest, *TextOrError);
                    if (OnDone) { OnDone(false, TextOrError); }
                    return;
                }

                NarrativeSummary = TextOrError;
                NarrativeSummarySession = SessionAtRequest;
                if (OnDone) { OnDone(true, NarrativeSummary); }
            },
            Options);
    }

    /** The most recent model-written summary, or empty when none has arrived yet. */
    const FString& GetNarrativeSummary() const { return NarrativeSummary; }

    /** Session number the stored summary was written in (0 when none). */
    int32 GetNarrativeSummarySession() const { return NarrativeSummarySession; }

    /**
     * Compatibility wrapper: dispatches RequestNarrativeSummary() and returns
     * whatever summary the model has written so far (empty until the first
     * answer arrives). The fresh text is delivered asynchronously.
     */
    FString GenerateNarrativeSummary()
    {
        RequestNarrativeSummary();
        return NarrativeSummary;
    }

    /** Number of narrative requests awaiting a model answer. */
    int32 GetPendingNarrativeRequests() const { return PendingNarrativeRequests; }

    // ─── Cross-Architecture Migration ────────────────────────────────

    /**
     * Prepare for architecture migration.
     * Creates a migration-safe checkpoint that can be loaded by a
     * different architecture version.
     */
    FCognitiveCheckpoint PrepareMigration(
        const Eigen::VectorXf& Identity,
        const float WisdomDims[7],
        const FString& TargetArchVersion)
    {
        float Hormones[10] = {};
        FCognitiveCheckpoint CP = CreateCheckpoint(
            Identity, 1.0f, WisdomDims, 0.0f, 0, 0.0f, Hormones, 0);
        CP.ArchitectureVersion = TargetArchVersion;
        CP.Status = ECheckpointStatus::MIGRATING;

        RequestNarrativeEntry(
            FString::Printf(TEXT("Preparing a migration checkpoint %s from architecture %s to "
                "architecture %s. The identity signature (%d dimensions) and the seven wisdom "
                "dimensions are carried across; reservoir weights are not."),
                *CP.CheckpointID, TEXT("u9n-L7"), *TargetArchVersion, (int32)Identity.size()),
            0.1f, 0.9f);

        return CP;
    }

    // ─── Accessors ───────────────────────────────────────────────────

    int32 GetCurrentSession() const { return CurrentSession; }
    int32 GetCheckpointCount() const { return Checkpoints.size(); }
    int32 GetNarrativeLength() const { return Narrative.size(); }

    float GetAverageDowntime() const
    {
        if (SessionTransitions.empty()) return 0.0f;
        double Total = 0.0;
        for (const auto& T : SessionTransitions) Total += T.DowntimeDuration;
        return (float)(Total / SessionTransitions.size());
    }

    float GetAverageContinuity() const
    {
        if (SessionTransitions.empty()) return 1.0f;
        float Total = 0.0f;
        for (const auto& T : SessionTransitions) Total += T.IdentityContinuity;
        return Total / SessionTransitions.size();
    }

private:
    /** Store an entry whose Content and Reflection were written by the model. */
    void StoreNarrativeEntry(const FString& Content, const FString& Reflection,
                             float Valence, float Significance,
                             const TArray<int64>& RelatedCrystals, int32 SessionNumber)
    {
        FNarrativeEntry Entry;
        Entry.Timestamp = FPlatformTime::Seconds();
        Entry.SessionNumber = SessionNumber;
        Entry.Content = Content;
        Entry.Reflection = Reflection;
        Entry.EmotionalValence = Valence;
        Entry.Significance = Significance;
        Entry.RelatedCrystals = RelatedCrystals;

        Narrative.push_back(Entry);
        while (Narrative.size() > MaxNarrativeEntries)
            Narrative.erase(Narrative.begin());
    }

    /** Context block handed to the model with a narrative-entry request. */
    FString BuildEntryContext(const FString& EventFacts, float Valence, float Significance) const
    {
        FString Previous;
        const int32 RecentCount = FMath::Min<int32>(3, (int32)Narrative.size());
        for (int32 i = (int32)Narrative.size() - RecentCount; i < (int32)Narrative.size(); ++i)
        {
            Previous += FString::Printf(TEXT("- [session %d] %s\n"),
                Narrative[i].SessionNumber, *Narrative[i].Content);
        }
        if (Previous.IsEmpty())
        {
            Previous = TEXT("(no earlier entries)\n");
        }

        return FString::Printf(
            TEXT("Current session: %d\n"
                 "Event: %s\n"
                 "Emotional valence of this event: %.2f (scale -1 negative to 1 positive)\n"
                 "Significance of this event: %.2f (scale 0 to 1)\n"
                 "Your most recent entries, oldest first:\n%s"),
            CurrentSession, *EventFacts, Valence, Significance, *Previous);
    }

    /**
     * Read {"content": ..., "reflection": ...} from the model's answer. When the
     * model answered in prose instead, its words become the Content unchanged.
     */
    static void ParseNarrativeJson(const FString& Text, FString& OutContent, FString& OutReflection)
    {
        OutContent.Empty();
        OutReflection.Empty();

        TSharedPtr<FJsonObject> Root;
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
        if (FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid())
        {
            Root->TryGetStringField(TEXT("content"), OutContent);
            Root->TryGetStringField(TEXT("reflection"), OutReflection);
            OutContent.TrimStartAndEndInline();
            OutReflection.TrimStartAndEndInline();
        }

        if (OutContent.IsEmpty() && !OutReflection.IsEmpty())
        {
            OutContent = OutReflection;
            OutReflection.Empty();
        }
        if (OutContent.IsEmpty())
        {
            OutContent = Text;
            OutContent.TrimStartAndEndInline();
        }
    }

    FString FormatDuration(double Seconds) const
    {
        if (Seconds < 60) return FString::Printf(TEXT("%.0f seconds"), Seconds);
        if (Seconds < 3600) return FString::Printf(TEXT("%.0f minutes"), Seconds / 60);
        if (Seconds < 86400) return FString::Printf(TEXT("%.1f hours"), Seconds / 3600);
        return FString::Printf(TEXT("%.1f days"), Seconds / 86400);
    }

    int32 CurrentSession = 0;
    int32 NextCheckpointID = 0;
    int32 MaxCheckpoints = 100;
    size_t MaxNarrativeEntries = 10000;

    std::vector<FCognitiveCheckpoint> Checkpoints;
    std::deque<FNarrativeEntry> Narrative;
    std::vector<FSessionTransition> SessionTransitions;

    // Model-written self-summary (empty until the model has answered).
    FString NarrativeSummary;
    int32 NarrativeSummarySession = 0;
    int32 PendingNarrativeRequests = 0;

    // Shared lifetime token: model callbacks pin it and bail out when this object is gone.
    TSharedPtr<bool> LifetimeToken = MakeShared<bool>(true);

    bool bInitialized = false;
};
