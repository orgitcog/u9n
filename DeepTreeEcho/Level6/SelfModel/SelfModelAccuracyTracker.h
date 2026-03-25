#pragma once
// ═══════════════════════════════════════════════════════════════════════════
// SelfModelAccuracyTracker — Level 6 Self-Model Verification
//
// DTE maintains a self-model (its beliefs about its own capabilities,
// personality, and behavior patterns). This tracker measures how well
// that self-model matches actual observed behavior.
//
// Self-Model Dimensions (from echo.go/core/identity):
//   1. Capability Model: "I can do X" vs actually doing X
//   2. Personality Model: "I am Y" vs observed personality traits
//   3. Behavioral Model: "I tend to Z" vs actual behavioral patterns
//   4. Emotional Model: "I feel W" vs endocrine state measurements
//   5. Social Model: "Others see me as V" vs interaction outcomes
//
// Accuracy Metrics:
//   - Prediction Accuracy: How often self-predictions match outcomes
//   - Calibration: How well confidence matches actual success rate
//   - Drift Detection: How much the self-model diverges over time
//   - Blind Spots: Capabilities/traits the model misses entirely
//
// When accuracy drops below threshold, triggers:
//   1. Self-model recalibration (update beliefs)
//   2. Identity checkpoint (save current state)
//   3. Introspection cycle (deep reflection)
//
// Ported from: echo.go/core/identity + core/meta/learner
// ═══════════════════════════════════════════════════════════════════════════
#include "CoreMinimal.h"
#include <Eigen/Dense>
#include <deque>
#include <map>
#include <string>
#include <cmath>

/** Self-model dimension */
enum class ESelfModelDimension : uint8
{
    CAPABILITY,    // What I can do
    PERSONALITY,   // Who I am
    BEHAVIORAL,    // How I act
    EMOTIONAL,     // What I feel
    SOCIAL         // How others see me
};

/** A self-prediction and its outcome */
struct FSelfPrediction
{
    ESelfModelDimension Dimension;
    FString Description;          // "I will succeed at task X"
    float PredictedValue = 0.0f;  // What DTE predicted [0,1]
    float ActualValue = 0.0f;     // What actually happened [0,1]
    float Confidence = 0.0f;      // How confident DTE was [0,1]
    float Error = 0.0f;           // |predicted - actual|
    double Timestamp = 0.0;
    bool bResolved = false;       // Has the outcome been observed?
};

/** Calibration bin for reliability diagram */
struct FCalibrationBin
{
    float BinCenter = 0.0f;       // Center of confidence bin
    float MeanConfidence = 0.0f;  // Mean confidence in this bin
    float MeanAccuracy = 0.0f;    // Mean actual accuracy in this bin
    int32 Count = 0;
    float CalibrationError = 0.0f; // |confidence - accuracy|
};

/** Blind spot — something the self-model misses */
struct FBlindSpot
{
    ESelfModelDimension Dimension;
    FString Description;
    float SeverityScore = 0.0f;   // How bad this blind spot is [0,1]
    int32 OccurrenceCount = 0;
    double FirstDetected = 0.0;
    double LastDetected = 0.0;
    bool bAcknowledged = false;   // Has DTE recognized this blind spot?
};

/** Self-model accuracy snapshot */
struct FSelfModelSnapshot
{
    double Timestamp = 0.0;
    float OverallAccuracy = 0.0f;
    float CapabilityAccuracy = 0.0f;
    float PersonalityAccuracy = 0.0f;
    float BehavioralAccuracy = 0.0f;
    float EmotionalAccuracy = 0.0f;
    float SocialAccuracy = 0.0f;
    float CalibrationScore = 0.0f;  // 1.0 = perfectly calibrated
    float DriftMagnitude = 0.0f;
    int32 BlindSpotCount = 0;
    int32 TotalPredictions = 0;
};

/**
 * SelfModelAccuracyTracker — Monitors self-model fidelity.
 *
 * The key insight: a truly autonomous system must know how well
 * it knows itself. Without this, self-modification is blind.
 */
class FSelfModelAccuracyTracker
{
public:
    FSelfModelAccuracyTracker() = default;

    void Initialize(float AccuracyThreshold = 0.6f)
    {
        MinAccuracyThreshold = AccuracyThreshold;
        bInitialized = true;

        // Initialize calibration bins (10 bins from 0.05 to 0.95)
        for (int32 i = 0; i < 10; ++i)
        {
            FCalibrationBin Bin;
            Bin.BinCenter = 0.05f + i * 0.1f;
            CalibrationBins.push_back(Bin);
        }
    }

    /**
     * Register a self-prediction.
     * Call this when DTE makes a prediction about itself.
     */
    int32 RegisterPrediction(ESelfModelDimension Dimension,
                              const FString& Description,
                              float PredictedValue, float Confidence)
    {
        FSelfPrediction Pred;
        Pred.Dimension = Dimension;
        Pred.Description = Description;
        Pred.PredictedValue = PredictedValue;
        Pred.Confidence = FMath::Clamp(Confidence, 0.0f, 1.0f);
        Pred.Timestamp = FPlatformTime::Seconds();

        int32 ID = Predictions.size();
        Predictions.push_back(Pred);
        TotalPredictions++;

        return ID;
    }

    /**
     * Resolve a prediction with the actual outcome.
     */
    void ResolvePrediction(int32 PredictionID, float ActualValue)
    {
        if (PredictionID < 0 || PredictionID >= (int32)Predictions.size()) return;

        auto& Pred = Predictions[PredictionID];
        Pred.ActualValue = ActualValue;
        Pred.Error = FMath::Abs(Pred.PredictedValue - ActualValue);
        Pred.bResolved = true;

        // Update dimension accuracy
        DimensionErrors[static_cast<int>(Pred.Dimension)].push_back(Pred.Error);

        // Update calibration
        UpdateCalibration(Pred);

        // Check for blind spots
        if (Pred.Error > 0.5f && Pred.Confidence > 0.7f)
        {
            // High confidence + high error = blind spot
            DetectBlindSpot(Pred);
        }

        ResolvedPredictions++;
    }

    /**
     * Compute current self-model accuracy.
     * Returns overall accuracy [0,1] where 1.0 = perfect self-knowledge.
     */
    float ComputeAccuracy() const
    {
        if (ResolvedPredictions == 0) return 1.0f; // No data yet

        float TotalError = 0.0f;
        int32 Count = 0;

        for (const auto& Pred : Predictions)
        {
            if (Pred.bResolved)
            {
                TotalError += Pred.Error;
                Count++;
            }
        }

        return Count > 0 ? 1.0f - (TotalError / Count) : 1.0f;
    }

    /**
     * Compute accuracy for a specific dimension.
     */
    float ComputeDimensionAccuracy(ESelfModelDimension Dim) const
    {
        int DimIdx = static_cast<int>(Dim);
        const auto& Errors = DimensionErrors[DimIdx];
        if (Errors.empty()) return 1.0f;

        float Sum = 0.0f;
        for (float E : Errors) Sum += E;
        return 1.0f - (Sum / Errors.size());
    }

    /**
     * Compute calibration score.
     * Perfect calibration = 1.0 (confidence matches accuracy).
     */
    float ComputeCalibrationScore() const
    {
        float TotalCE = 0.0f;
        int32 NonEmpty = 0;

        for (const auto& Bin : CalibrationBins)
        {
            if (Bin.Count > 0)
            {
                TotalCE += Bin.CalibrationError;
                NonEmpty++;
            }
        }

        return NonEmpty > 0 ? 1.0f - (TotalCE / NonEmpty) : 1.0f;
    }

    /**
     * Detect drift — how much the self-model has diverged recently.
     */
    float ComputeDrift() const
    {
        if (Snapshots.size() < 2) return 0.0f;

        const auto& Recent = Snapshots.back();
        const auto& Previous = Snapshots[Snapshots.size() - 2];

        return FMath::Abs(Recent.OverallAccuracy - Previous.OverallAccuracy);
    }

    /**
     * Take a snapshot of current self-model accuracy.
     */
    FSelfModelSnapshot TakeSnapshot()
    {
        FSelfModelSnapshot Snap;
        Snap.Timestamp = FPlatformTime::Seconds();
        Snap.OverallAccuracy = ComputeAccuracy();
        Snap.CapabilityAccuracy = ComputeDimensionAccuracy(ESelfModelDimension::CAPABILITY);
        Snap.PersonalityAccuracy = ComputeDimensionAccuracy(ESelfModelDimension::PERSONALITY);
        Snap.BehavioralAccuracy = ComputeDimensionAccuracy(ESelfModelDimension::BEHAVIORAL);
        Snap.EmotionalAccuracy = ComputeDimensionAccuracy(ESelfModelDimension::EMOTIONAL);
        Snap.SocialAccuracy = ComputeDimensionAccuracy(ESelfModelDimension::SOCIAL);
        Snap.CalibrationScore = ComputeCalibrationScore();
        Snap.DriftMagnitude = ComputeDrift();
        Snap.BlindSpotCount = BlindSpots.size();
        Snap.TotalPredictions = TotalPredictions;

        Snapshots.push_back(Snap);

        // Keep manageable
        while (Snapshots.size() > 1000)
            Snapshots.erase(Snapshots.begin());

        return Snap;
    }

    /**
     * Check if recalibration is needed.
     */
    bool NeedsRecalibration() const
    {
        return ComputeAccuracy() < MinAccuracyThreshold ||
               ComputeCalibrationScore() < 0.5f ||
               ComputeDrift() > 0.1f;
    }

    /** Get blind spots */
    const std::vector<FBlindSpot>& GetBlindSpots() const { return BlindSpots; }

    /** Get statistics */
    int32 GetTotalPredictions() const { return TotalPredictions; }
    int32 GetResolvedPredictions() const { return ResolvedPredictions; }
    int32 GetBlindSpotCount() const { return BlindSpots.size(); }

private:
    void UpdateCalibration(const FSelfPrediction& Pred)
    {
        // Find the right calibration bin
        int32 BinIdx = FMath::Clamp(
            (int32)(Pred.Confidence * 10.0f), 0, 9);

        auto& Bin = CalibrationBins[BinIdx];
        float Success = Pred.Error < 0.3f ? 1.0f : 0.0f;

        // Running average
        Bin.MeanConfidence = (Bin.MeanConfidence * Bin.Count + Pred.Confidence) / (Bin.Count + 1);
        Bin.MeanAccuracy = (Bin.MeanAccuracy * Bin.Count + Success) / (Bin.Count + 1);
        Bin.Count++;
        Bin.CalibrationError = FMath::Abs(Bin.MeanConfidence - Bin.MeanAccuracy);
    }

    void DetectBlindSpot(const FSelfPrediction& Pred)
    {
        // Check if this is a known blind spot
        for (auto& BS : BlindSpots)
        {
            if (BS.Dimension == Pred.Dimension &&
                BS.Description.Contains(Pred.Description.Left(20)))
            {
                BS.OccurrenceCount++;
                BS.LastDetected = FPlatformTime::Seconds();
                BS.SeverityScore = FMath::Min(1.0f,
                    BS.SeverityScore + 0.1f);
                return;
            }
        }

        // New blind spot
        FBlindSpot BS;
        BS.Dimension = Pred.Dimension;
        BS.Description = Pred.Description;
        BS.SeverityScore = Pred.Error;
        BS.OccurrenceCount = 1;
        BS.FirstDetected = FPlatformTime::Seconds();
        BS.LastDetected = BS.FirstDetected;
        BlindSpots.push_back(BS);
    }

    float MinAccuracyThreshold = 0.6f;

    std::deque<FSelfPrediction> Predictions;
    std::vector<FSelfModelSnapshot> Snapshots;
    std::vector<FBlindSpot> BlindSpots;
    std::vector<FCalibrationBin> CalibrationBins;

    // Per-dimension error history
    std::deque<float> DimensionErrors[5];

    int32 TotalPredictions = 0;
    int32 ResolvedPredictions = 0;
    bool bInitialized = false;
};
