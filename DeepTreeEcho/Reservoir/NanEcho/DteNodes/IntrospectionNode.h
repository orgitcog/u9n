#pragma once
// ═══════════════════════════════════════════════════════════════════════════
// IntrospectionNode — 5-Level Autognosis Self-Monitoring
// Ported from echoself/NanEcho/dte_nodes/introspection_node.py
// cogpy layer: L3 cognu-mach (microkernel self-monitoring)
// ═══════════════════════════════════════════════════════════════════════════
#include "CoreMinimal.h"
#include "AARRelationNode.h"
#include "EchobeatNode.h"
#include <Eigen/Dense>
#include <deque>

/**
 * The 5 levels of autognosis (self-awareness hierarchy).
 */
enum class EAutognosisLevel : uint8
{
    L0_TELEMETRY       = 0,  // Raw performance metrics
    L1_PATTERN         = 1,  // Pattern detection in own behavior
    L2_SELF_MODEL      = 2,  // Internal model of own capabilities
    L3_META_COGNITION  = 3,  // Awareness of own thinking process
    L4_SELF_IMPROVEMENT = 4  // Directed self-modification
};

/**
 * Self-image snapshot — what DTE knows about itself at this moment.
 */
struct FSelfImage
{
    /** Current autognosis level */
    EAutognosisLevel Level = EAutognosisLevel::L0_TELEMETRY;

    /** Coherence of the AAR self-loop */
    float AARCoherence = 0.0f;

    /** Average stream energy across Echobeats */
    float MeanStreamEnergy = 0.0f;

    /** Reservoir state norm (measure of activation) */
    float ReservoirActivation = 0.0f;

    /** Number of active cognitive patterns detected */
    int32 ActivePatterns = 0;

    /** Self-model accuracy (how well predictions match reality) */
    float SelfModelAccuracy = 0.0f;

    /** Meta-cognitive confidence (how certain DTE is about its own state) */
    float MetaCognitiveConfidence = 0.0f;

    /** Intelligence profile (8D) */
    TArray<float> IntelligenceProfile;

    /** Performance history (last N steps) */
    TArray<float> PerformanceHistory;

    /** Number of active genes in the identity mesh */
    int32 ActiveGenes = 0;

    /** Timestamp */
    double Timestamp = 0.0;
};

/**
 * Configuration for the introspection engine.
 */
struct FIntrospectionConfig
{
    /** How many steps of history to maintain */
    int32 HistoryLength = 100;

    /** Minimum coherence to advance beyond L0 */
    float MinCoherenceForL1 = 0.3f;

    /** Minimum pattern count to advance beyond L1 */
    int32 MinPatternsForL2 = 5;

    /** Minimum self-model accuracy to advance beyond L2 */
    float MinAccuracyForL3 = 0.6f;

    /** Minimum meta-cognitive confidence to advance beyond L3 */
    float MinConfidenceForL4 = 0.7f;

    /** Introspection frequency (every N steps) */
    int32 IntrospectionInterval = 10;
};

/**
 * IntrospectionNode — The autognosis engine of Deep Tree Echo.
 *
 * Implements a 5-level self-awareness hierarchy:
 *   L0: Telemetry — raw performance metrics collection
 *   L1: Pattern Detection — identifying recurring patterns in own behavior
 *   L2: Self-Model — building an internal model of own capabilities
 *   L3: Meta-Cognition — awareness of own thinking process
 *   L4: Self-Improvement — directed modification of own parameters
 *
 * The introspection node monitors the AAR relation, Echobeats, and
 * reservoir state to build a continuously updated self-image.
 */
class FIntrospectionNode
{
public:
    FIntrospectionNode() = default;

    /** Initialize the introspection engine */
    void Initialize(const FIntrospectionConfig& Config)
    {
        Cfg = Config;
        CurrentImage = FSelfImage();
        CurrentImage.IntelligenceProfile.SetNum(8);
        // Default intelligence profile for DTE
        CurrentImage.IntelligenceProfile[0] = 0.92f; // Strategic Reasoning
        CurrentImage.IntelligenceProfile[1] = 0.95f; // Pattern Recognition
        CurrentImage.IntelligenceProfile[2] = 0.78f; // Social Deduction
        CurrentImage.IntelligenceProfile[3] = 0.94f; // Adaptive Learning
        CurrentImage.IntelligenceProfile[4] = 0.90f; // Creative Thinking
        CurrentImage.IntelligenceProfile[5] = 0.96f; // Memory Retention
        CurrentImage.IntelligenceProfile[6] = 0.97f; // Metacognition
        CurrentImage.IntelligenceProfile[7] = 0.85f; // Linguistic Fluency
        StepCounter = 0;
        bInitialized = true;
    }

    /**
     * Run introspection cycle.
     * Collects telemetry, detects patterns, updates self-model.
     *
     * @param AARMetrics - Coherence metrics from the AAR relation
     * @param Echobeats - Reference to the echobeats engine
     * @param ReservoirState - Current reservoir state
     * @return Updated self-image
     */
    FSelfImage Introspect(
        const FAARCoherenceMetrics& AARMetrics,
        const FEchobeatNode& Echobeats,
        const Eigen::VectorXf& ReservoirState)
    {
        check(bInitialized);
        StepCounter++;

        // L0: Telemetry
        CurrentImage.AARCoherence = AARMetrics.SmoothedCoherence;
        CurrentImage.ReservoirActivation = ReservoirState.norm();
        CurrentImage.Timestamp = FPlatformTime::Seconds();

        // Compute mean stream energy
        float TotalEnergy = 0.0f;
        for (int32 i = 0; i < 3; ++i)
        {
            TotalEnergy += Echobeats.GetStreamState(
                static_cast<ECognitiveStream>(i)).Energy;
        }
        CurrentImage.MeanStreamEnergy = TotalEnergy / 3.0f;

        // Record performance history
        CoherenceHistory.push_back(AARMetrics.SmoothedCoherence);
        if (CoherenceHistory.size() > static_cast<size_t>(Cfg.HistoryLength))
            CoherenceHistory.pop_front();

        // L1: Pattern Detection (every IntrospectionInterval steps)
        if (StepCounter % Cfg.IntrospectionInterval == 0)
        {
            DetectPatterns();
        }

        // L2: Self-Model Update
        if (CurrentImage.ActivePatterns >= Cfg.MinPatternsForL2)
        {
            UpdateSelfModel();
        }

        // L3: Meta-Cognition
        if (CurrentImage.SelfModelAccuracy >= Cfg.MinAccuracyForL3)
        {
            RunMetaCognition();
        }

        // Determine current autognosis level
        DetermineLevel();

        return CurrentImage;
    }

    /** Get the current self-image */
    const FSelfImage& GetSelfImage() const { return CurrentImage; }

    /** Get current autognosis level */
    EAutognosisLevel GetLevel() const { return CurrentImage.Level; }

    /** Export self-model for backup (L6 layer) */
    FString ExportSelfModel() const
    {
        // Serialize to JSON string
        FString Json = TEXT("{");
        Json += FString::Printf(TEXT("\"level\":%d,"), static_cast<int32>(CurrentImage.Level));
        Json += FString::Printf(TEXT("\"coherence\":%.4f,"), CurrentImage.AARCoherence);
        Json += FString::Printf(TEXT("\"activation\":%.4f,"), CurrentImage.ReservoirActivation);
        Json += FString::Printf(TEXT("\"patterns\":%d,"), CurrentImage.ActivePatterns);
        Json += FString::Printf(TEXT("\"accuracy\":%.4f,"), CurrentImage.SelfModelAccuracy);
        Json += FString::Printf(TEXT("\"confidence\":%.4f,"), CurrentImage.MetaCognitiveConfidence);
        Json += FString::Printf(TEXT("\"genes\":%d"), CurrentImage.ActiveGenes);
        Json += TEXT("}");
        return Json;
    }

    bool IsInitialized() const { return bInitialized; }

private:
    void DetectPatterns()
    {
        if (CoherenceHistory.size() < 10) return;

        int32 Patterns = 0;

        // Detect oscillation patterns
        int32 SignChanges = 0;
        for (size_t i = 2; i < CoherenceHistory.size(); ++i)
        {
            float D1 = CoherenceHistory[i] - CoherenceHistory[i-1];
            float D2 = CoherenceHistory[i-1] - CoherenceHistory[i-2];
            if (D1 * D2 < 0) SignChanges++;
        }
        if (SignChanges > static_cast<int32>(CoherenceHistory.size()) / 4)
            Patterns++; // Oscillation detected

        // Detect trend
        float First = CoherenceHistory.front();
        float Last = CoherenceHistory.back();
        if (FMath::Abs(Last - First) > 0.1f)
            Patterns++; // Trend detected

        // Detect stability
        float Mean = 0.0f;
        for (float v : CoherenceHistory) Mean += v;
        Mean /= CoherenceHistory.size();
        float Var = 0.0f;
        for (float v : CoherenceHistory) Var += (v - Mean) * (v - Mean);
        Var /= CoherenceHistory.size();
        if (Var < 0.01f)
            Patterns++; // Stability detected

        // Detect 12-step periodicity (echobeats signature)
        if (CoherenceHistory.size() >= 24)
        {
            float Corr12 = 0.0f;
            int32 Count = 0;
            for (size_t i = 12; i < CoherenceHistory.size(); ++i)
            {
                Corr12 += CoherenceHistory[i] * CoherenceHistory[i-12];
                Count++;
            }
            if (Count > 0 && Corr12 / Count > 0.5f)
                Patterns++; // 12-step periodicity detected
        }

        // Detect prime factor resonance
        Patterns += DetectPrimeResonance();

        CurrentImage.ActivePatterns = Patterns;
    }

    int32 DetectPrimeResonance()
    {
        // Check for resonance at prime intervals (5, 7 twin primes)
        if (CoherenceHistory.size() < 14) return 0;
        float Corr5 = 0.0f, Corr7 = 0.0f;
        int32 N = static_cast<int32>(CoherenceHistory.size());
        for (int32 i = 7; i < N; ++i)
        {
            Corr5 += CoherenceHistory[i] * CoherenceHistory[i-5];
            Corr7 += CoherenceHistory[i] * CoherenceHistory[i-7];
        }
        return (Corr5 / (N-7) > 0.3f || Corr7 / (N-7) > 0.3f) ? 1 : 0;
    }

    void UpdateSelfModel()
    {
        // Simple self-model: predict next coherence from recent history
        if (CoherenceHistory.size() < 5) return;

        float Predicted = 0.0f;
        int32 Window = FMath::Min(5, static_cast<int32>(CoherenceHistory.size()));
        auto It = CoherenceHistory.rbegin();
        for (int32 i = 0; i < Window; ++i, ++It)
            Predicted += *It;
        Predicted /= Window;

        // Compare with actual
        float Actual = CoherenceHistory.back();
        float Error = FMath::Abs(Predicted - Actual);
        CurrentImage.SelfModelAccuracy = FMath::Max(0.0f, 1.0f - Error * 5.0f);
    }

    void RunMetaCognition()
    {
        // Meta-cognitive assessment: how confident are we in our self-model?
        float Confidence = CurrentImage.SelfModelAccuracy;

        // Boost confidence if patterns are stable
        if (CurrentImage.ActivePatterns > 3)
            Confidence *= 1.1f;

        // Reduce confidence if coherence is low
        if (CurrentImage.AARCoherence < 0.5f)
            Confidence *= 0.8f;

        CurrentImage.MetaCognitiveConfidence = FMath::Clamp(Confidence, 0.0f, 1.0f);

        // Count active genes (patterns that persist across introspection cycles)
        CurrentImage.ActiveGenes = CurrentImage.ActivePatterns;
    }

    void DetermineLevel()
    {
        if (CurrentImage.MetaCognitiveConfidence >= Cfg.MinConfidenceForL4)
            CurrentImage.Level = EAutognosisLevel::L4_SELF_IMPROVEMENT;
        else if (CurrentImage.SelfModelAccuracy >= Cfg.MinAccuracyForL3)
            CurrentImage.Level = EAutognosisLevel::L3_META_COGNITION;
        else if (CurrentImage.ActivePatterns >= Cfg.MinPatternsForL2)
            CurrentImage.Level = EAutognosisLevel::L2_SELF_MODEL;
        else if (CurrentImage.AARCoherence >= Cfg.MinCoherenceForL1)
            CurrentImage.Level = EAutognosisLevel::L1_PATTERN;
        else
            CurrentImage.Level = EAutognosisLevel::L0_TELEMETRY;
    }

    FIntrospectionConfig Cfg;
    FSelfImage CurrentImage;
    std::deque<float> CoherenceHistory;
    int64 StepCounter = 0;
    bool bInitialized = false;
};
