#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "DopaminergicRewardSystem.generated.h"

/**
 * Classification of reward event types
 */
UENUM(BlueprintType)
enum class ERewardEventType : uint8
{
    Primary UMETA(DisplayName = "Primary"),       // Innate reward (food, pleasure)
    Secondary UMETA(DisplayName = "Secondary"),   // Learned/conditioned reward
    Social UMETA(DisplayName = "Social"),         // Social reward (approval, connection)
    Achievement UMETA(DisplayName = "Achievement") // Accomplishment/mastery reward
};

/**
 * VTA (Ventral Tegmental Area) dopamine neuron state.
 * Models the source of mesolimbic dopamine signaling.
 */
USTRUCT(BlueprintType)
struct FVTANeuronState
{
    GENERATED_BODY()

    /** Tonic (baseline) firing rate of VTA neurons (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float TonicFiringRate;

    /** Phasic burst amplitude above tonic baseline (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float PhasicBurstAmplitude;

    /** Phasic dip amplitude below tonic baseline (0-1; stored as positive) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float PhasicDipAmplitude;

    /** Remaining duration of active phasic response (seconds) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float PhasicDuration;

    /** Whether a phasic burst (true) or dip (false) is currently active */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsPhasicBurst;

    /** Rate at which phasic responses decay back to tonic baseline */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float PhasicDecayRate;

    /** Cumulative fatigue from repeated phasic activations (reduces burst magnitude) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float PhasicFatigue;

    FVTANeuronState()
        : TonicFiringRate(0.4f)
        , PhasicBurstAmplitude(0.0f)
        , PhasicDipAmplitude(0.0f)
        , PhasicDuration(0.0f)
        , bIsPhasicBurst(true)
        , PhasicDecayRate(0.8f)
        , PhasicFatigue(0.0f)
    {}
};

/**
 * Reward Prediction Error (RPE) state.
 * Implements Temporal Difference (TD) learning based on Schultz et al.
 */
USTRUCT(BlueprintType)
struct FRewardPredictionState
{
    GENERATED_BODY()

    /** Current predicted value of the environment/situation (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float PredictedValue;

    /** Actual reward received in the last outcome (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ActualReward;

    /** Reward Prediction Error: ActualReward - PredictedValue */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float RPE;

    /** TD learning rate for updating value predictions (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float LearningRate;

    /** Discount factor for future rewards (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float DiscountFactor;

    /** Exponential moving average of recent RPE (measures prediction quality) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float RPERunningMean;

    /** Running variance of RPE (measures prediction uncertainty) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float RPERunningVariance;

    FRewardPredictionState()
        : PredictedValue(0.5f)
        , ActualReward(0.0f)
        , RPE(0.0f)
        , LearningRate(0.1f)
        , DiscountFactor(0.9f)
        , RPERunningMean(0.0f)
        , RPERunningVariance(0.1f)
    {}
};

/**
 * Dopamine receptor subtype state.
 * D1 receptors (direct pathway / Go) vs D2 receptors (indirect pathway / NoGo).
 */
USTRUCT(BlueprintType)
struct FDopamineReceptorState
{
    GENERATED_BODY()

    /** D1 receptor sensitivity (1.0 = normal) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float D1Sensitivity;

    /** D2 receptor sensitivity (1.0 = normal) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float D2Sensitivity;

    /** Rate at which receptors downregulate under sustained high dopamine */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float DownregulationRate;

    /** Rate at which receptor sensitivity recovers when dopamine is low */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float RecoveryRate;

    /** Dopamine level above which receptor downregulation begins */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float DownregulationThreshold;

    /** Dopamine level below which receptor sensitivity recovers */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float RecoveryThreshold;

    /** Net Go signal from direct pathway (approach motivation) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float GoSignal;

    /** Net NoGo signal from indirect pathway (avoidance / suppression) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float NoGoSignal;

    FDopamineReceptorState()
        : D1Sensitivity(1.0f)
        , D2Sensitivity(1.0f)
        , DownregulationRate(0.015f)
        , RecoveryRate(0.008f)
        , DownregulationThreshold(0.75f)
        , RecoveryThreshold(0.35f)
        , GoSignal(0.5f)
        , NoGoSignal(0.5f)
    {}
};

/**
 * Dopamine transporter (DAT) and reuptake kinetics.
 * Controls the clearance of synaptic dopamine.
 */
USTRUCT(BlueprintType)
struct FDopamineTransporterState
{
    GENERATED_BODY()

    /** Current transporter activity (1.0 = full capacity) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float TransporterActivity;

    /** Maximum reuptake rate (dopamine cleared per second at full activity) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MaxReuptakeRate;

    /** Michaelis constant — dopamine level at half-maximal reuptake (Km) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float KmReuptake;

    /** Monoamine oxidase (MAO) degradation rate */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MAODegradationRate;

    FDopamineTransporterState()
        : TransporterActivity(1.0f)
        , MaxReuptakeRate(0.4f)
        , KmReuptake(0.3f)
        , MAODegradationRate(0.05f)
    {}
};

/**
 * Active reward event being processed by the system
 */
USTRUCT(BlueprintType)
struct FRewardEvent
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Magnitude;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float PredictedMagnitude;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ERewardEventType Type;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bActive;

    FRewardEvent()
        : Magnitude(0.0f)
        , PredictedMagnitude(0.0f)
        , Type(ERewardEventType::Primary)
        , bActive(false)
    {}

    FRewardEvent(float InMagnitude, float InPredicted, ERewardEventType InType)
        : Magnitude(InMagnitude)
        , PredictedMagnitude(InPredicted)
        , Type(InType)
        , bActive(true)
    {}
};

/**
 * Complete dopaminergic reward system state snapshot
 */
USTRUCT(BlueprintType)
struct FDopaminergicRewardState
{
    GENERATED_BODY()

    /** Nucleus Accumbens extracellular dopamine level (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float NAccDopamine;

    /** Prefrontal Cortex dopamine level (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float PFCDopamine;

    /** Striatal dopamine level (0-1; averaged across dorsal/ventral striatum) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float StriatalDopamine;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVTANeuronState VTANeurons;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FRewardPredictionState RewardPrediction;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FDopamineReceptorState Receptors;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FDopamineTransporterState Transporter;

    /** Cumulative hedonic habituation (reduces sensitivity to repeated rewards) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float HedonicHabituation;

    /** Overall motivational drive (integration of tonic + RPE signals) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MotivationalDrive;

    FDopaminergicRewardState()
        : NAccDopamine(0.5f)
        , PFCDopamine(0.45f)
        , StriatalDopamine(0.5f)
        , HedonicHabituation(0.0f)
        , MotivationalDrive(0.5f)
    {}
};

/**
 * Dopaminergic Reward System (Feature F4.1.2)
 *
 * Biologically-plausible simulation of mesolimbic and mesocortical dopamine
 * pathways. Models:
 *   - VTA dopamine neuron tonic and phasic firing
 *   - Nucleus Accumbens / PFC / striatal dopamine levels
 *   - Reward Prediction Error (RPE) via Temporal Difference learning
 *   - D1/D2 receptor Go/NoGo dynamics
 *   - Dopamine Transporter (DAT) Michaelis–Menten reuptake kinetics
 *   - Hedonic habituation from repeated rewards
 *   - Motivational drive integration
 */
UCLASS(BlueprintType)
class UNREALENGINE_API UDopaminergicRewardSystem : public UObject
{
    GENERATED_BODY()

public:
    UDopaminergicRewardSystem();

    // ===== Lifecycle =====

    UFUNCTION(BlueprintCallable, Category = "Dopamine")
    void Initialize();

    UFUNCTION(BlueprintCallable, Category = "Dopamine")
    void Update(float DeltaTime);

    UFUNCTION(BlueprintCallable, Category = "Dopamine")
    void Reset();

    // ===== Reward Input =====

    /**
     * Register a reward outcome.
     * Computes RPE = ActualReward - PredictedValue and triggers phasic VTA response.
     * Updates value prediction via TD learning.
     *
     * @param ActualReward   Reward actually received (0-1)
     * @param RewardType     Classification of the reward
     */
    UFUNCTION(BlueprintCallable, Category = "Dopamine")
    void RegisterRewardOutcome(float ActualReward,
                               ERewardEventType RewardType = ERewardEventType::Primary);

    /**
     * Register an unexpected (unpredicted) reward — fires a phasic dopamine burst
     * proportional to reward magnitude without updating the value predictor.
     */
    UFUNCTION(BlueprintCallable, Category = "Dopamine")
    void ProcessUnexpectedReward(float Magnitude,
                                 ERewardEventType RewardType = ERewardEventType::Primary);

    /**
     * Register a reward omission (expected reward not received) — fires a
     * phasic dopamine dip proportional to the prediction.
     */
    UFUNCTION(BlueprintCallable, Category = "Dopamine")
    void ProcessRewardOmission();

    /**
     * Simulate anticipatory dopamine rise in response to a reward-predictive cue.
     * Elevates tonic VTA firing ahead of reward delivery.
     *
     * @param CueValue  Learned value of the cue (0-1)
     */
    UFUNCTION(BlueprintCallable, Category = "Dopamine")
    void TriggerCuedAnticipation(float CueValue);

    /**
     * Directly set the current value prediction (e.g. for narrative scripting).
     */
    UFUNCTION(BlueprintCallable, Category = "Dopamine")
    void SetValuePrediction(float PredictedValue);

    // ===== Output Queries =====

    /** Nucleus Accumbens dopamine level (0-1); primary reward signal */
    UFUNCTION(BlueprintCallable, Category = "Dopamine")
    float GetNAccDopamine() const;

    /** PFC dopamine level (0-1); modulates working memory and executive control */
    UFUNCTION(BlueprintCallable, Category = "Dopamine")
    float GetPFCDopamine() const;

    /** Current Reward Prediction Error (-1 to +1; 0 = perfectly predicted) */
    UFUNCTION(BlueprintCallable, Category = "Dopamine")
    float GetRewardPredictionError() const;

    /** Tonic VTA firing rate (0-1); baseline motivational drive */
    UFUNCTION(BlueprintCallable, Category = "Dopamine")
    float GetTonicDopamine() const;

    /** Phasic VTA response amplitude (positive = burst, negative = dip) */
    UFUNCTION(BlueprintCallable, Category = "Dopamine")
    float GetPhasicDopamine() const;

    /** Integrated motivational drive combining tonic and phasic signals (0-1) */
    UFUNCTION(BlueprintCallable, Category = "Dopamine")
    float GetMotivationalDrive() const;

    /** Go signal (approach motivation) from D1 pathway (0-1) */
    UFUNCTION(BlueprintCallable, Category = "Dopamine")
    float GetGoSignal() const;

    /** NoGo signal (avoidance suppression) from D2 pathway (0-1) */
    UFUNCTION(BlueprintCallable, Category = "Dopamine")
    float GetNoGoSignal() const;

    /** Degree of hedonic habituation (0 = none, 1 = fully habituated) */
    UFUNCTION(BlueprintCallable, Category = "Dopamine")
    float GetHedonicHabituation() const;

    /** Full state snapshot */
    UFUNCTION(BlueprintCallable, Category = "Dopamine")
    FDopaminergicRewardState GetState() const { return State; }

    UFUNCTION(BlueprintCallable, Category = "Dopamine")
    FVTANeuronState GetVTANeuronState() const { return State.VTANeurons; }

    UFUNCTION(BlueprintCallable, Category = "Dopamine")
    FRewardPredictionState GetRewardPredictionState() const { return State.RewardPrediction; }

    UFUNCTION(BlueprintCallable, Category = "Dopamine")
    FDopamineReceptorState GetReceptorState() const { return State.Receptors; }

    // ===== Configuration =====

    /** Adjust TD learning rate (0-1) */
    UFUNCTION(BlueprintCallable, Category = "Dopamine")
    void SetLearningRate(float LearningRate);

    /** Adjust hedonic habituation dissipation rate */
    UFUNCTION(BlueprintCallable, Category = "Dopamine")
    void SetHabitationDissipationRate(float Rate);

    /** Get number of currently queued reward events */
    UFUNCTION(BlueprintCallable, Category = "Dopamine")
    int32 GetPendingRewardEventCount() const;

protected:
    void UpdateVTANeurons(float DeltaTime);
    void UpdateDopamineLevels(float DeltaTime);
    void UpdateReceptors(float DeltaTime);
    void UpdateHedonicHabituation(float DeltaTime);
    void UpdateMotivationalDrive();
    float CalculateDopamineProduction(float DeltaTime) const;
    float CalculateDopamineClearance(float DeltaTime) const;
    void ApplyRPEToPhasicResponse(float RPE);

private:
    UPROPERTY()
    FDopaminergicRewardState State;

    UPROPERTY()
    TArray<FRewardEvent> PendingRewardEvents;

    /** Dissipation rate of hedonic habituation during low-reward periods */
    UPROPERTY(EditAnywhere, Category = "Dopamine")
    float HabitationDissipationRate;

    /** Rate at which phasic fatigue recovers */
    UPROPERTY(EditAnywhere, Category = "Dopamine")
    float PhasicFatigueRecoveryRate;

    /** Coupling factor from NAcc dopamine to PFC dopamine */
    UPROPERTY(EditAnywhere, Category = "Dopamine")
    float NAccToPFCCoupling;

    /** Coupling factor from NAcc dopamine to striatal dopamine */
    UPROPERTY(EditAnywhere, Category = "Dopamine")
    float NAccToStriatalCoupling;

    /** Tonic VTA target driven by chronic reward history */
    UPROPERTY(EditAnywhere, Category = "Dopamine")
    float TonicSetPoint;
};
