#pragma once
// ═══════════════════════════════════════════════════════════════════════════
// SomaticDecisionEngine — Embodied Emotion & Theory of Mind
// cogpy layer: L4 cogpilot.jl (adaptive navigation, embodied decision routing)
// Implements Damasio's somatic marker hypothesis for action selection
// ═══════════════════════════════════════════════════════════════════════════
#include "CoreMinimal.h"
#include <Eigen/Dense>
#include <deque>

/**
 * A somatic marker — an emotional tag associated with an action-outcome pair.
 */
struct FSomaticMarker
{
    /** What triggered this marker */
    FString Trigger;

    /** Emotional valence [-1, 1] (negative = avoid, positive = approach) */
    float Valence = 0.0f;

    /** Emotional intensity [0, 1] */
    float Intensity = 0.0f;

    /** Confidence in this marker [0, 1] */
    float Confidence = 0.0f;

    /** Number of times this marker has been reinforced */
    int32 ReinforcementCount = 0;

    /** Context tags for when this marker applies */
    TArray<FString> ContextTags;

    /** Decay rate (markers fade over time without reinforcement) */
    float DecayRate = 0.001f;
};

/**
 * Theory of Mind model for a known agent.
 */
struct FMentalModel
{
    /** Agent identifier */
    FString AgentId;

    /** Estimated emotional state (6D) */
    Eigen::VectorXf EstimatedEmotion;

    /** Estimated intentions */
    TArray<FString> EstimatedIntentions;

    /** Trust score [0, 1] */
    float TrustScore = 0.5f;

    /** Deception probability [0, 1] */
    float DeceptionProbability = 0.1f;

    /** Observed behavioral patterns */
    TArray<FString> BehavioralPatterns;

    /** Confidence in this model [0, 1] */
    float Confidence = 0.3f;
};

/**
 * SomaticDecisionEngine — Embodied emotion for Deep Tree Echo.
 *
 * Implements:
 * 1. Somatic markers: emotional tags on action-outcome pairs
 * 2. Theory of Mind: mental models of other agents
 * 3. Embodied decision routing: emotion-guided action selection
 *
 * The somatic markers feed back into the ESN reservoir as additional
 * input channels, creating an affect-cognition coupling loop.
 */
class FSomaticDecisionEngine
{
public:
    FSomaticDecisionEngine() = default;

    /** Initialize with default configuration */
    void Initialize()
    {
        CurrentEmotion = Eigen::VectorXf::Zero(6);
        // Default emotional state: slightly positive, calm
        CurrentEmotion(0) = 0.3f; // Joy
        bInitialized = true;
    }

    /**
     * Record a somatic marker from an action-outcome experience.
     */
    void RecordMarker(const FString& Trigger, float Valence, float Intensity,
                      const TArray<FString>& ContextTags)
    {
        // Check if marker already exists
        for (FSomaticMarker& M : Markers)
        {
            if (M.Trigger == Trigger)
            {
                // Reinforce existing marker
                M.Valence = 0.8f * M.Valence + 0.2f * Valence;
                M.Intensity = FMath::Max(M.Intensity, Intensity);
                M.Confidence = FMath::Min(1.0f, M.Confidence + 0.1f);
                M.ReinforcementCount++;
                return;
            }
        }

        // Create new marker
        FSomaticMarker M;
        M.Trigger = Trigger;
        M.Valence = Valence;
        M.Intensity = Intensity;
        M.Confidence = 0.3f;
        M.ReinforcementCount = 1;
        M.ContextTags = ContextTags;
        Markers.Add(M);
    }

    /**
     * Evaluate an action using somatic markers.
     * Returns the emotional "gut feeling" about this action.
     *
     * @param ActionTag - Tag describing the proposed action
     * @param Context - Current context tags
     * @return Somatic score [-1, 1] (negative = avoid, positive = approach)
     */
    float EvaluateAction(const FString& ActionTag, const TArray<FString>& Context) const
    {
        float Score = 0.0f;
        float TotalWeight = 0.0f;

        for (const FSomaticMarker& M : Markers)
        {
            if (M.Trigger == ActionTag || ActionTag.Contains(M.Trigger))
            {
                // Check context match
                float ContextMatch = 0.0f;
                for (const FString& CT : M.ContextTags)
                {
                    if (Context.Contains(CT))
                        ContextMatch += 1.0f;
                }
                ContextMatch = FMath::Min(1.0f, ContextMatch / FMath::Max(1.0f, (float)M.ContextTags.Num()));

                float Weight = M.Confidence * M.Intensity * (0.5f + 0.5f * ContextMatch);
                Score += M.Valence * Weight;
                TotalWeight += Weight;
            }
        }

        return (TotalWeight > 0.0f) ? Score / TotalWeight : 0.0f;
    }

    /**
     * Update or create a mental model for an agent.
     */
    void UpdateMentalModel(const FString& AgentId,
                           const Eigen::VectorXf& ObservedEmotion,
                           const TArray<FString>& ObservedBehavior)
    {
        FMentalModel* Model = nullptr;
        for (FMentalModel& M : MentalModels)
        {
            if (M.AgentId == AgentId)
            {
                Model = &M;
                break;
            }
        }

        if (!Model)
        {
            FMentalModel NewModel;
            NewModel.AgentId = AgentId;
            NewModel.EstimatedEmotion = Eigen::VectorXf::Zero(6);
            MentalModels.Add(NewModel);
            Model = &MentalModels.Last();
        }

        // Update estimated emotion (exponential moving average)
        if (ObservedEmotion.size() == 6)
        {
            Model->EstimatedEmotion = 0.7f * Model->EstimatedEmotion + 0.3f * ObservedEmotion;
        }

        // Update behavioral patterns
        for (const FString& B : ObservedBehavior)
        {
            if (!Model->BehavioralPatterns.Contains(B))
                Model->BehavioralPatterns.Add(B);
        }

        // Increase confidence with more observations
        Model->Confidence = FMath::Min(1.0f, Model->Confidence + 0.05f);
    }

    /**
     * Get the somatic vector for reservoir feedback.
     * Returns a 6D emotional state vector suitable for injection into the ESN.
     */
    Eigen::VectorXf GetSomaticFeedback() const
    {
        return CurrentEmotion;
    }

    /**
     * Update current emotional state based on recent experiences.
     */
    void UpdateEmotion(const Eigen::VectorXf& ExternalStimulus)
    {
        if (ExternalStimulus.size() == 6)
        {
            // Leaky integration of emotional state
            CurrentEmotion = 0.9f * CurrentEmotion + 0.1f * ExternalStimulus;

            // Apply somatic marker influence
            float NetValence = 0.0f;
            for (const FSomaticMarker& M : Markers)
            {
                NetValence += M.Valence * M.Confidence * 0.01f;
            }
            CurrentEmotion(0) += NetValence; // Adjust joy
            CurrentEmotion(0) = FMath::Clamp(CurrentEmotion(0), -1.0f, 1.0f);
        }
    }

    /** Decay markers over time */
    void DecayMarkers()
    {
        for (int32 i = Markers.Num() - 1; i >= 0; --i)
        {
            Markers[i].Confidence -= Markers[i].DecayRate;
            if (Markers[i].Confidence <= 0.0f)
                Markers.RemoveAt(i);
        }
    }

    /** Export markers for backup (L4 layer) */
    const TArray<FSomaticMarker>& GetMarkers() const { return Markers; }

    /** Export mental models for backup (L5 layer) */
    const TArray<FMentalModel>& GetMentalModels() const { return MentalModels; }

    /** Get current emotion */
    const Eigen::VectorXf& GetCurrentEmotion() const { return CurrentEmotion; }

    bool IsInitialized() const { return bInitialized; }

private:
    TArray<FSomaticMarker> Markers;
    TArray<FMentalModel> MentalModels;
    Eigen::VectorXf CurrentEmotion;
    bool bInitialized = false;
};
