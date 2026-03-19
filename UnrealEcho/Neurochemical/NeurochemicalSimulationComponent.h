#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
class UCortisolDynamicsSystem;

#include "NeurochemicalSimulationComponent.generated.h"

/**
 * Neurochemical types simulating brain chemistry
 */
UENUM(BlueprintType)
enum class ENeurochemicalType : uint8
{
    Dopamine UMETA(DisplayName = "Dopamine"),        // Reward, motivation, pleasure
    Serotonin UMETA(DisplayName = "Serotonin"),      // Mood, happiness, well-being
    Norepinephrine UMETA(DisplayName = "Norepinephrine"), // Alertness, arousal, stress
    Oxytocin UMETA(DisplayName = "Oxytocin"),        // Social bonding, trust, empathy
    Cortisol UMETA(DisplayName = "Cortisol"),        // Stress, anxiety, alertness
    Endorphins UMETA(DisplayName = "Endorphins"),    // Pain relief, euphoria
    GABA UMETA(DisplayName = "GABA"),                // Inhibition, calmness, relaxation
    Glutamate UMETA(DisplayName = "Glutamate"),      // Excitation, learning, memory
    Acetylcholine UMETA(DisplayName = "Acetylcholine") // Attention, learning, arousal
};

/**
 * Neurochemical state data
 */
USTRUCT(BlueprintType)
struct FNeurochemicalState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float DopamineLevel;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SerotoninLevel;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float NorepinephrineLevel;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float OxytocinLevel;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CortisolLevel;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EndorphinsLevel;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float GABALevel;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float GlutamateLevel;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float AcetylcholineLevel;

    FNeurochemicalState()
        : DopamineLevel(0.5f)
        , SerotoninLevel(0.6f)
        , NorepinephrineLevel(0.4f)
        , OxytocinLevel(0.5f)
        , CortisolLevel(0.3f)
        , EndorphinsLevel(0.4f)
        , GABALevel(0.6f)
        , GlutamateLevel(0.5f)
        , AcetylcholineLevel(0.5f)
    {}
};

/**
 * Emotional state derived from neurochemistry
 */
USTRUCT(BlueprintType)
struct FEmotionalChemistry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Happiness; // Serotonin + Dopamine

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Excitement; // Dopamine + Norepinephrine

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Calmness; // GABA + Serotonin

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Anxiety; // Cortisol + Norepinephrine

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Affection; // Oxytocin + Endorphins

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Focus; // Acetylcholine + Glutamate

    FEmotionalChemistry()
        : Happiness(0.5f)
        , Excitement(0.4f)
        , Calmness(0.6f)
        , Anxiety(0.3f)
        , Affection(0.5f)
        , Focus(0.5f)
    {}
};

/**
 * Neurochemical Simulation Component
 * Simulates brain chemistry and its effects on behavior and emotion
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UNREALENGINE_API UNeurochemicalSimulationComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UNeurochemicalSimulationComponent();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, 
                               FActorComponentTickFunction* ThisTickFunction) override;

    // ===== Neurochemical Management =====

    UFUNCTION(BlueprintCallable, Category = "Neurochemistry")
    void SetNeurochemicalLevel(ENeurochemicalType Type, float Level);

    UFUNCTION(BlueprintCallable, Category = "Neurochemistry")
    float GetNeurochemicalLevel(ENeurochemicalType Type) const;

    UFUNCTION(BlueprintCallable, Category = "Neurochemistry")
    void ModifyNeurochemical(ENeurochemicalType Type, float DeltaLevel);

    UFUNCTION(BlueprintCallable, Category = "Neurochemistry")
    FNeurochemicalState GetCurrentState() const { return CurrentState; }

    // ===== Emotional Chemistry =====

    UFUNCTION(BlueprintCallable, Category = "Neurochemistry")
    FEmotionalChemistry GetEmotionalChemistry() const;

    UFUNCTION(BlueprintCallable, Category = "Neurochemistry")
    void TriggerRewardResponse(float Intensity);

    UFUNCTION(BlueprintCallable, Category = "Neurochemistry")
    void TriggerStressResponse(float Intensity);

    UFUNCTION(BlueprintCallable, Category = "Neurochemistry")
    void TriggerSocialBonding(float Intensity);

    UFUNCTION(BlueprintCallable, Category = "Neurochemistry")
    void TriggerRelaxationResponse(float Intensity);

    // ===== Behavioral Effects =====

    UFUNCTION(BlueprintCallable, Category = "Neurochemistry")
    float GetMotivationLevel() const;

    UFUNCTION(BlueprintCallable, Category = "Neurochemistry")
    float GetEnergyLevel() const;

    UFUNCTION(BlueprintCallable, Category = "Neurochemistry")
    float GetSocialDesire() const;

    UFUNCTION(BlueprintCallable, Category = "Neurochemistry")
    float GetCognitiveClarity() const;

    UFUNCTION(BlueprintCallable, Category = "Neurochemistry")
    float GetEmotionalStability() const;

    // ===== Homeostasis =====

    UFUNCTION(BlueprintCallable, Category = "Neurochemistry")
    void EnableHomeostasis(bool bEnable);

    UFUNCTION(BlueprintCallable, Category = "Neurochemistry")
    void SetHomeostasisRate(float Rate);

    UFUNCTION(BlueprintCallable, Category = "Neurochemistry")
    void ResetToBaseline();

    /** Connect an external cortisol dynamics system to drive cortisol levels */
    UFUNCTION(BlueprintCallable, Category = "Neurochemistry")
    void SetCortisolDynamicsSystem(UCortisolDynamicsSystem* System);

    // ===== Interactions =====

    UFUNCTION(BlueprintCallable, Category = "Neurochemistry")
    void SimulateExercise(float Intensity, float Duration);

    UFUNCTION(BlueprintCallable, Category = "Neurochemistry")
    void SimulateSocialInteraction(float PositivityLevel, float Duration);

    UFUNCTION(BlueprintCallable, Category = "Neurochemistry")
    void SimulateCognitiveTask(float Difficulty, float Duration);

    UFUNCTION(BlueprintCallable, Category = "Neurochemistry")
    void SimulateRestAndRecovery(float Duration);

protected:
    // ===== Internal Methods =====

    void UpdateNeurochemicalDynamics(float DeltaTime);
    void ApplyHomeostasis(float DeltaTime);
    void ProcessNeurochemicalInteractions();
    void UpdateEmotionalState();
    void ApplyNeurochemistryToAvatar();

    float CalculateNeurochemicalDecay(ENeurochemicalType Type, float DeltaTime);
    float CalculateNeurochemicalProduction(ENeurochemicalType Type);

private:
    // ===== Current State =====

    UPROPERTY(EditAnywhere, Category = "Neurochemistry")
    FNeurochemicalState CurrentState;

    UPROPERTY(EditAnywhere, Category = "Neurochemistry")
    FNeurochemicalState BaselineState;

    UPROPERTY(EditAnywhere, Category = "Neurochemistry")
    FEmotionalChemistry CurrentEmotionalChemistry;

    // ===== Configuration =====

    UPROPERTY(EditAnywhere, Category = "Neurochemistry")
    bool bEnableHomeostasis;

    UPROPERTY(EditAnywhere, Category = "Neurochemistry")
    float HomeostasisRate;

    UPROPERTY(EditAnywhere, Category = "Neurochemistry")
    float NeurochemicalDecayRate;

    UPROPERTY(EditAnywhere, Category = "Neurochemistry")
    float InteractionStrength;

    // ===== Decay Rates (per neurochemical) =====

    UPROPERTY(EditAnywhere, Category = "Neurochemistry|Advanced")
    TMap<ENeurochemicalType, float> DecayRates;

    UPROPERTY(EditAnywhere, Category = "Neurochemistry|Advanced")
    TMap<ENeurochemicalType, float> ProductionRates;

    /** Optional reference to external cortisol dynamics system */
    UPROPERTY()
    UCortisolDynamicsSystem* CortisolDynamicsRef;
};
