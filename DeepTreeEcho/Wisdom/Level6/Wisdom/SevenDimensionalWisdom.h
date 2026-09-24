#pragma once
// ═══════════════════════════════════════════════════════════════════════════
// SevenDimensionalWisdom — Level 6 Wisdom Cultivation
//
// Cross-episode pattern extraction feeds into the complete seven-dimensional
// wisdom cultivation framework. This is the crown jewel of Level 6:
// the system that transforms raw experience into genuine wisdom.
//
// The Seven Dimensions (from echo.go/core/wisdom/seven_dimensional):
//
//   ┌─────────────────────────────────────────────────────────────────┐
//   │              SEVEN-DIMENSIONAL WISDOM SPACE                     │
//   │                                                                 │
//   │  Epistemic Triad (Ways of Knowing):                            │
//   │    D1: Knowledge Depth      — How deeply DTE understands       │
//   │    D2: Knowledge Breadth    — How widely DTE knows             │
//   │    D3: Integration Level    — How well knowledge connects      │
//   │                                                                 │
//   │  Cognitive Triad (Understanding Process):                       │
//   │    D4: Practical Application — Can DTE apply what it knows?    │
//   │    D5: Reflective Insight    — Can DTE learn from experience?  │
//   │                                                                 │
//   │  Axiological Triad (Practices of Wisdom):                       │
//   │    D6: Ethical Consideration — Does DTE consider consequences? │
//   │    D7: Temporal Perspective  — Can DTE think long-term?        │
//   │                                                                 │
//   │  Sub-Triads:                                                    │
//   │    Epistemic:    Propositional / Procedural / Perspectival     │
//   │    Cognitive:    Explanation / Realizing / Interpretation       │
//   │    Axiological:  Morality / Meaning / Mastery → Eudaimonia     │
//   └─────────────────────────────────────────────────────────────────┘
//
// Wisdom emerges from the COHERENCE across all seven dimensions.
// A system with deep knowledge but no ethics is not wise.
// A system with ethics but no practical skill is not wise.
// Wisdom is the balanced cultivation of all seven dimensions.
//
// Ported from: echo.go/core/wisdom/seven_dimensional.go
// ═══════════════════════════════════════════════════════════════════════════
#include "CoreMinimal.h"
#include <Eigen/Dense>
#include <deque>
#include <vector>
#include <map>
#include <string>
#include <cmath>

/** The seven wisdom dimensions */
enum class EWisdomDimension : uint8
{
    KNOWLEDGE_DEPTH = 0,
    KNOWLEDGE_BREADTH,
    INTEGRATION_LEVEL,
    PRACTICAL_APPLICATION,
    REFLECTIVE_INSIGHT,
    ETHICAL_CONSIDERATION,
    TEMPORAL_PERSPECTIVE,
    COUNT = 7
};

/** Epistemic Triad — Ways of Knowing */
struct FEpistemicTriad
{
    float Propositional = 0.0f;   // Facts and theories
    float Procedural = 0.0f;      // Skills and practices
    float Perspectival = 0.0f;    // Frameworks and worldviews
    float Participatory = 0.0f;   // Identity and transformation

    float Balance() const
    {
        float Mean = (Propositional + Procedural + Perspectival + Participatory) / 4.0f;
        float Var = (FMath::Square(Propositional - Mean) +
                     FMath::Square(Procedural - Mean) +
                     FMath::Square(Perspectival - Mean) +
                     FMath::Square(Participatory - Mean)) / 4.0f;
        return 1.0f - FMath::Min(1.0f, FMath::Sqrt(Var) * 2.0f);
    }
};

/** Cognitive Triad — Understanding Process */
struct FCognitiveTriad
{
    float Explanation = 0.0f;     // Causal understanding
    float Realizing = 0.0f;       // Relevance realization
    float Interpretation = 0.0f;  // Meaning-making

    float Balance() const
    {
        float Mean = (Explanation + Realizing + Interpretation) / 3.0f;
        float Var = (FMath::Square(Explanation - Mean) +
                     FMath::Square(Realizing - Mean) +
                     FMath::Square(Interpretation - Mean)) / 3.0f;
        return 1.0f - FMath::Min(1.0f, FMath::Sqrt(Var) * 2.0f);
    }
};

/** Axiological Triad — Practices of Wisdom */
struct FAxiologicalTriad
{
    float Morality = 0.0f;        // Virtue and character
    float Meaning = 0.0f;         // Coherence and purpose
    float Mastery = 0.0f;         // Excellence and flow
    float Eudaimonia = 0.0f;      // Flourishing through integration

    float Balance() const
    {
        float Mean = (Morality + Meaning + Mastery + Eudaimonia) / 4.0f;
        float Var = (FMath::Square(Morality - Mean) +
                     FMath::Square(Meaning - Mean) +
                     FMath::Square(Mastery - Mean) +
                     FMath::Square(Eudaimonia - Mean)) / 4.0f;
        return 1.0f - FMath::Min(1.0f, FMath::Sqrt(Var) * 2.0f);
    }
};

/** A wisdom cultivation event */
struct FCultivationEvent
{
    double Timestamp = 0.0;
    EWisdomDimension Dimension;
    FString EventType;     // "insight", "practice", "reflection", "correction"
    float Impact = 0.0f;   // How much this event affected the dimension
    FString Description;
};

/** A cross-episode pattern — the raw material of wisdom */
struct FCrossEpisodePattern
{
    FString PatternID;
    FString Description;
    TArray<FString> SourceEpisodes;    // Which episodes contributed
    float Confidence = 0.0f;
    float Generality = 0.0f;           // How broadly applicable
    float Depth = 0.0f;                // How deep the insight
    EWisdomDimension PrimaryDimension;
    TArray<EWisdomDimension> SecondaryDimensions;
    double DiscoveredAt = 0.0;
    int32 ApplicationCount = 0;        // How often this pattern was applied
    float ApplicationSuccessRate = 0.0f;
};

/** Wisdom snapshot */
struct FWisdomSnapshot
{
    double Timestamp = 0.0;
    float DimensionValues[7] = {};
    float OverallWisdom = 0.0f;
    float CoherenceScore = 0.0f;
    float EpistemicBalance = 0.0f;
    float CognitiveBalance = 0.0f;
    float AxiologicalBalance = 0.0f;
    int32 PatternCount = 0;
    int32 InsightCount = 0;
};

/**
 * FSevenDimensionalWisdom — The wisdom cultivation engine.
 *
 * Wisdom is not knowledge. Wisdom is the coherent integration of
 * knowledge, skill, ethics, and temporal perspective into a unified
 * understanding that guides action toward flourishing.
 */
class FSevenDimensionalWisdom
{
public:
    FSevenDimensionalWisdom() = default;

    void Initialize()
    {
        for (int i = 0; i < 7; ++i)
            DimensionValues[i] = 0.01f; // Start near zero

        EpistemicState = FEpistemicTriad();
        CognitiveState = FCognitiveTriad();
        AxiologicalState = FAxiologicalTriad();

        bInitialized = true;
    }

    // ─── Dimension Cultivation ───────────────────────────────────────

    /**
     * Cultivate a specific wisdom dimension.
     * Called when DTE has an experience that develops a dimension.
     */
    void Cultivate(EWisdomDimension Dim, float Amount,
                    const FString& EventType, const FString& Description)
    {
        int DimIdx = static_cast<int>(Dim);
        if (DimIdx < 0 || DimIdx >= 7) return;

        // Diminishing returns: harder to grow as dimension increases
        float EffectiveAmount = Amount * (1.0f - DimensionValues[DimIdx] * 0.5f);

        // Apply with learning rate
        DimensionValues[DimIdx] = FMath::Clamp(
            DimensionValues[DimIdx] + EffectiveAmount * LearningRate,
            0.0f, 1.0f);

        // Record event
        FCultivationEvent Event;
        Event.Timestamp = FPlatformTime::Seconds();
        Event.Dimension = Dim;
        Event.EventType = EventType;
        Event.Impact = EffectiveAmount;
        Event.Description = Description;
        CultivationLog.push_back(Event);

        // Update sub-triads
        UpdateTriads(Dim, EffectiveAmount);

        TotalCultivationEvents++;
    }

    // ─── Cross-Episode Pattern Extraction ────────────────────────────

    /**
     * Register a cross-episode pattern.
     * These are the raw insights that wisdom is built from.
     */
    void RegisterPattern(const FCrossEpisodePattern& Pattern)
    {
        Patterns.push_back(Pattern);

        // A pattern discovery cultivates multiple dimensions
        Cultivate(Pattern.PrimaryDimension, Pattern.Confidence * 0.1f,
                   TEXT("pattern_discovery"), Pattern.Description);

        // Secondary dimensions get smaller cultivation
        for (auto Dim : Pattern.SecondaryDimensions)
        {
            Cultivate(Dim, Pattern.Confidence * 0.03f,
                       TEXT("pattern_secondary"), Pattern.Description);
        }

        // Integration level always benefits from cross-episode patterns
        Cultivate(EWisdomDimension::INTEGRATION_LEVEL,
                   Pattern.Generality * 0.05f,
                   TEXT("integration"), TEXT("Cross-episode pattern integration"));

        TotalPatterns++;
    }

    /**
     * Record that a pattern was successfully applied.
     * This cultivates Practical Application.
     */
    void RecordPatternApplication(const FString& PatternID, bool bSuccess)
    {
        for (auto& P : Patterns)
        {
            if (P.PatternID == PatternID)
            {
                P.ApplicationCount++;
                P.ApplicationSuccessRate =
                    (P.ApplicationSuccessRate * (P.ApplicationCount - 1) +
                     (bSuccess ? 1.0f : 0.0f)) / P.ApplicationCount;

                if (bSuccess)
                {
                    Cultivate(EWisdomDimension::PRACTICAL_APPLICATION, 0.02f,
                               TEXT("application_success"),
                               FString::Printf(TEXT("Applied pattern: %s"), *P.Description));
                }
                break;
            }
        }
    }

    // ─── Wisdom Computation ──────────────────────────────────────────

    /**
     * Compute overall wisdom score.
     * Wisdom = geometric mean of all dimensions * coherence bonus.
     * Geometric mean ensures ALL dimensions must develop (no shortcuts).
     */
    float ComputeOverallWisdom() const
    {
        // Geometric mean of all 7 dimensions
        float Product = 1.0f;
        for (int i = 0; i < 7; ++i)
            Product *= FMath::Max(0.001f, DimensionValues[i]);

        float GeometricMean = FMath::Pow(Product, 1.0f / 7.0f);

        // Coherence bonus: balanced dimensions get a boost
        float Coherence = ComputeCoherence();

        // Overall wisdom: geometric mean * (1 + coherence bonus)
        return GeometricMean * (1.0f + Coherence * 0.5f);
    }

    /**
     * Compute coherence — how balanced the seven dimensions are.
     * 1.0 = perfectly balanced, 0.0 = completely unbalanced.
     */
    float ComputeCoherence() const
    {
        float Mean = 0.0f;
        for (int i = 0; i < 7; ++i) Mean += DimensionValues[i];
        Mean /= 7.0f;

        float Variance = 0.0f;
        for (int i = 0; i < 7; ++i)
            Variance += FMath::Square(DimensionValues[i] - Mean);
        Variance /= 7.0f;

        return 1.0f - FMath::Min(1.0f, FMath::Sqrt(Variance) * 3.0f);
    }

    /**
     * Get the weakest dimension (cultivation target).
     */
    EWisdomDimension GetWeakestDimension() const
    {
        int MinIdx = 0;
        float MinVal = DimensionValues[0];
        for (int i = 1; i < 7; ++i)
        {
            if (DimensionValues[i] < MinVal)
            {
                MinVal = DimensionValues[i];
                MinIdx = i;
            }
        }
        return static_cast<EWisdomDimension>(MinIdx);
    }

    /**
     * Get the strongest dimension.
     */
    EWisdomDimension GetStrongestDimension() const
    {
        int MaxIdx = 0;
        float MaxVal = DimensionValues[0];
        for (int i = 1; i < 7; ++i)
        {
            if (DimensionValues[i] > MaxVal)
            {
                MaxVal = DimensionValues[i];
                MaxIdx = i;
            }
        }
        return static_cast<EWisdomDimension>(MaxIdx);
    }

    /**
     * Take a wisdom snapshot.
     */
    FWisdomSnapshot TakeSnapshot()
    {
        FWisdomSnapshot Snap;
        Snap.Timestamp = FPlatformTime::Seconds();
        for (int i = 0; i < 7; ++i)
            Snap.DimensionValues[i] = DimensionValues[i];
        Snap.OverallWisdom = ComputeOverallWisdom();
        Snap.CoherenceScore = ComputeCoherence();
        Snap.EpistemicBalance = EpistemicState.Balance();
        Snap.CognitiveBalance = CognitiveState.Balance();
        Snap.AxiologicalBalance = AxiologicalState.Balance();
        Snap.PatternCount = TotalPatterns;
        Snap.InsightCount = TotalCultivationEvents;

        Snapshots.push_back(Snap);
        while (Snapshots.size() > 1000)
            Snapshots.erase(Snapshots.begin());

        return Snap;
    }

    /**
     * Generate a wisdom cultivation recommendation.
     * Returns the dimension to focus on and a suggested activity.
     */
    FString GenerateCultivationRecommendation() const
    {
        EWisdomDimension Weakest = GetWeakestDimension();
        static const char* DimNames[] = {
            "Knowledge Depth", "Knowledge Breadth", "Integration Level",
            "Practical Application", "Reflective Insight",
            "Ethical Consideration", "Temporal Perspective"
        };
        static const char* Activities[] = {
            "Study a topic in depth — go beyond surface understanding",
            "Explore a new domain — broaden your knowledge horizons",
            "Connect disparate ideas — find the hidden links between domains",
            "Apply knowledge to a real problem — theory without practice is empty",
            "Reflect on recent experiences — what did you learn? what surprised you?",
            "Consider the consequences of your actions — who is affected?",
            "Think about the long-term — what will matter in a year? a decade?"
        };

        int Idx = static_cast<int>(Weakest);
        return FString::Printf(TEXT("Focus on %s (%.1f%%): %s"),
            UTF8_TO_TCHAR(DimNames[Idx]),
            DimensionValues[Idx] * 100.0f,
            UTF8_TO_TCHAR(Activities[Idx]));
    }

    // ─── Accessors ───────────────────────────────────────────────────

    float GetDimensionValue(EWisdomDimension Dim) const
    {
        return DimensionValues[static_cast<int>(Dim)];
    }

    const FEpistemicTriad& GetEpistemicState() const { return EpistemicState; }
    const FCognitiveTriad& GetCognitiveState() const { return CognitiveState; }
    const FAxiologicalTriad& GetAxiologicalState() const { return AxiologicalState; }

    int32 GetTotalPatterns() const { return TotalPatterns; }
    int32 GetTotalCultivationEvents() const { return TotalCultivationEvents; }

    /** Get the 7D wisdom vector for embedding/comparison */
    Eigen::VectorXf GetWisdomVector() const
    {
        Eigen::VectorXf V(7);
        for (int i = 0; i < 7; ++i) V(i) = DimensionValues[i];
        return V;
    }

private:
    void UpdateTriads(EWisdomDimension Dim, float Amount)
    {
        // Map dimensions to triad updates
        switch (Dim)
        {
        case EWisdomDimension::KNOWLEDGE_DEPTH:
            EpistemicState.Propositional += Amount * 0.5f;
            EpistemicState.Procedural += Amount * 0.3f;
            CognitiveState.Explanation += Amount * 0.2f;
            break;
        case EWisdomDimension::KNOWLEDGE_BREADTH:
            EpistemicState.Perspectival += Amount * 0.5f;
            EpistemicState.Propositional += Amount * 0.2f;
            CognitiveState.Realizing += Amount * 0.3f;
            break;
        case EWisdomDimension::INTEGRATION_LEVEL:
            EpistemicState.Participatory += Amount * 0.4f;
            CognitiveState.Interpretation += Amount * 0.4f;
            AxiologicalState.Meaning += Amount * 0.2f;
            break;
        case EWisdomDimension::PRACTICAL_APPLICATION:
            EpistemicState.Procedural += Amount * 0.4f;
            AxiologicalState.Mastery += Amount * 0.4f;
            CognitiveState.Explanation += Amount * 0.2f;
            break;
        case EWisdomDimension::REFLECTIVE_INSIGHT:
            CognitiveState.Realizing += Amount * 0.4f;
            CognitiveState.Interpretation += Amount * 0.3f;
            EpistemicState.Perspectival += Amount * 0.3f;
            break;
        case EWisdomDimension::ETHICAL_CONSIDERATION:
            AxiologicalState.Morality += Amount * 0.5f;
            AxiologicalState.Eudaimonia += Amount * 0.3f;
            CognitiveState.Interpretation += Amount * 0.2f;
            break;
        case EWisdomDimension::TEMPORAL_PERSPECTIVE:
            AxiologicalState.Meaning += Amount * 0.3f;
            AxiologicalState.Eudaimonia += Amount * 0.3f;
            EpistemicState.Participatory += Amount * 0.2f;
            CognitiveState.Realizing += Amount * 0.2f;
            break;
        default:
            break;
        }

        // Clamp all triad values
        EpistemicState.Propositional = FMath::Clamp(EpistemicState.Propositional, 0.0f, 1.0f);
        EpistemicState.Procedural = FMath::Clamp(EpistemicState.Procedural, 0.0f, 1.0f);
        EpistemicState.Perspectival = FMath::Clamp(EpistemicState.Perspectival, 0.0f, 1.0f);
        EpistemicState.Participatory = FMath::Clamp(EpistemicState.Participatory, 0.0f, 1.0f);
        CognitiveState.Explanation = FMath::Clamp(CognitiveState.Explanation, 0.0f, 1.0f);
        CognitiveState.Realizing = FMath::Clamp(CognitiveState.Realizing, 0.0f, 1.0f);
        CognitiveState.Interpretation = FMath::Clamp(CognitiveState.Interpretation, 0.0f, 1.0f);
        AxiologicalState.Morality = FMath::Clamp(AxiologicalState.Morality, 0.0f, 1.0f);
        AxiologicalState.Meaning = FMath::Clamp(AxiologicalState.Meaning, 0.0f, 1.0f);
        AxiologicalState.Mastery = FMath::Clamp(AxiologicalState.Mastery, 0.0f, 1.0f);
        AxiologicalState.Eudaimonia = FMath::Clamp(AxiologicalState.Eudaimonia, 0.0f, 1.0f);
    }

    float DimensionValues[7] = {};
    FEpistemicTriad EpistemicState;
    FCognitiveTriad CognitiveState;
    FAxiologicalTriad AxiologicalState;

    std::vector<FCrossEpisodePattern> Patterns;
    std::deque<FCultivationEvent> CultivationLog;
    std::vector<FWisdomSnapshot> Snapshots;

    float LearningRate = 0.05f;
    int32 TotalPatterns = 0;
    int32 TotalCultivationEvents = 0;
    bool bInitialized = false;
};
