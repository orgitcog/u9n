#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "CortisolDynamicsSystem.generated.h"

/**
 * Stress event type classification
 */
UENUM(BlueprintType)
enum class EStressEventType : uint8
{
    Acute UMETA(DisplayName = "Acute"),       // Brief, intense stressor
    Chronic UMETA(DisplayName = "Chronic"),    // Sustained, ongoing stressor
    Traumatic UMETA(DisplayName = "Traumatic") // High-intensity, lasting impact
};

/**
 * HPA (Hypothalamic-Pituitary-Adrenal) axis state
 * Models the CRH -> ACTH -> Cortisol signaling cascade
 */
USTRUCT(BlueprintType)
struct FHPAAxisState
{
    GENERATED_BODY()

    /** Corticotropin-releasing hormone level (hypothalamus output) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CRHLevel;

    /** Adrenocorticotropic hormone level (pituitary output) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ACTHLevel;

    /** Raw cortisol output from adrenal cortex */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CortisolOutput;

    /** Overall axis activation (0 = suppressed, 1 = maximally active) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float AxisActivation;

    /** Negative feedback strength (reduced under chronic stress) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float NegativeFeedbackGain;

    FHPAAxisState()
        : CRHLevel(0.1f)
        , ACTHLevel(0.1f)
        , CortisolOutput(0.0f)
        , AxisActivation(0.0f)
        , NegativeFeedbackGain(1.0f)
    {}
};

/**
 * Circadian rhythm parameters for cortisol secretion
 */
USTRUCT(BlueprintType)
struct FCircadianRhythm
{
    GENERATED_BODY()

    /** Current simulated time of day in hours (0-24) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float TimeOfDay;

    /** Hour of peak cortisol (cortisol awakening response) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float PeakHour;

    /** Amplitude of circadian oscillation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Amplitude;

    /** Baseline cortisol production from circadian drive */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Baseline;

    /** Current circadian contribution to cortisol production */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CurrentContribution;

    FCircadianRhythm()
        : TimeOfDay(8.0f)
        , PeakHour(8.0f)
        , Amplitude(0.15f)
        , Baseline(0.25f)
        , CurrentContribution(0.0f)
    {}
};

/**
 * Active stress event being processed by the system
 */
USTRUCT(BlueprintType)
struct FStressEvent
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Intensity;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Duration;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ElapsedTime;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EStressEventType Type;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bActive;

    FStressEvent()
        : Intensity(0.0f)
        , Duration(0.0f)
        , ElapsedTime(0.0f)
        , Type(EStressEventType::Acute)
        , bActive(false)
    {}

    FStressEvent(float InIntensity, float InDuration, EStressEventType InType)
        : Intensity(InIntensity)
        , Duration(InDuration)
        , ElapsedTime(0.0f)
        , Type(InType)
        , bActive(true)
    {}
};

/**
 * Glucocorticoid receptor state modeling sensitivity/resistance
 */
USTRUCT(BlueprintType)
struct FCortisolReceptorState
{
    GENERATED_BODY()

    /** Receptor sensitivity (1.0 = normal, <1.0 = downregulated/resistant) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Sensitivity;

    /** Rate at which receptors downregulate under high cortisol */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float DownregulationRate;

    /** Rate at which receptors recover when cortisol is low */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float RecoveryRate;

    /** Threshold above which cortisol causes receptor downregulation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float DownregulationThreshold;

    /** Threshold below which receptors begin recovering */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float RecoveryThreshold;

    FCortisolReceptorState()
        : Sensitivity(1.0f)
        , DownregulationRate(0.02f)
        , RecoveryRate(0.01f)
        , DownregulationThreshold(0.7f)
        , RecoveryThreshold(0.4f)
    {}
};

/**
 * Complete cortisol dynamics state snapshot
 */
USTRUCT(BlueprintType)
struct FCortisolDynamicsState
{
    GENERATED_BODY()

    /** Total cortisol level (0-1 range) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float TotalCortisol;

    /** Free (bioactive) cortisol fraction */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float FreeCortisol;

    /** Cortisol bound to corticosteroid-binding globulin */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float BoundCortisol;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FHPAAxisState HPAAxis;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FCircadianRhythm Circadian;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FCortisolReceptorState Receptors;

    /** Accumulated chronic stress load (0 = none, 1 = maximum) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ChronicStressLoad;

    /** Whether the system is currently under chronic stress */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bChronicStressActive;

    /** Chronic stressor intensity when active */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ChronicStressorIntensity;

    FCortisolDynamicsState()
        : TotalCortisol(0.3f)
        , FreeCortisol(0.015f)
        , BoundCortisol(0.285f)
        , ChronicStressLoad(0.0f)
        , bChronicStressActive(false)
        , ChronicStressorIntensity(0.0f)
    {}
};

/**
 * Cortisol Dynamics System
 *
 * Biologically-plausible simulation of the HPA axis and cortisol regulation.
 * Models circadian rhythm, acute/chronic stress responses, receptor
 * sensitivity adaptation, and cortisol clearance kinetics.
 */
UCLASS(BlueprintType)
class UNREALENGINE_API UCortisolDynamicsSystem : public UObject
{
    GENERATED_BODY()

public:
    UCortisolDynamicsSystem();

    // ===== Lifecycle =====

    UFUNCTION(BlueprintCallable, Category = "Cortisol")
    void Initialize();

    UFUNCTION(BlueprintCallable, Category = "Cortisol")
    void Update(float DeltaTime);

    UFUNCTION(BlueprintCallable, Category = "Cortisol")
    void Reset();

    // ===== Stress Input =====

    /** Apply an acute stress event with given intensity (0-1) and duration in seconds */
    UFUNCTION(BlueprintCallable, Category = "Cortisol")
    void ApplyStressor(float Intensity, float Duration);

    /** Begin a chronic stress state with given intensity */
    UFUNCTION(BlueprintCallable, Category = "Cortisol")
    void ApplyChronicStressor(float Intensity);

    /** End the current chronic stress state */
    UFUNCTION(BlueprintCallable, Category = "Cortisol")
    void RemoveChronicStressor();

    // ===== Output Queries =====

    /** Get current total cortisol level (0-1) */
    UFUNCTION(BlueprintCallable, Category = "Cortisol")
    float GetCortisolLevel() const;

    /** Get free (bioactive) cortisol fraction */
    UFUNCTION(BlueprintCallable, Category = "Cortisol")
    float GetFreeCortisolLevel() const;

    /** Get cumulative stress load (0-1) */
    UFUNCTION(BlueprintCallable, Category = "Cortisol")
    float GetStressLoad() const;

    /** Get full dynamics state snapshot */
    UFUNCTION(BlueprintCallable, Category = "Cortisol")
    FCortisolDynamicsState GetState() const { return State; }

    UFUNCTION(BlueprintCallable, Category = "Cortisol")
    FHPAAxisState GetHPAAxisState() const { return State.HPAAxis; }

    UFUNCTION(BlueprintCallable, Category = "Cortisol")
    FCortisolReceptorState GetReceptorState() const { return State.Receptors; }

    // ===== Configuration =====

    /** Set simulated time of day (0-24 hours) */
    UFUNCTION(BlueprintCallable, Category = "Cortisol")
    void SetTimeOfDay(float Hours);

    /** Set rate at which simulated time advances (1.0 = real-time, 0 = frozen) */
    UFUNCTION(BlueprintCallable, Category = "Cortisol")
    void SetTimeScale(float Scale);

    /** Get number of currently active stress events */
    UFUNCTION(BlueprintCallable, Category = "Cortisol")
    int32 GetActiveStressEventCount() const;

protected:
    void UpdateHPAAxis(float DeltaTime);
    void UpdateCircadianRhythm(float DeltaTime);
    void UpdateStressEvents(float DeltaTime);
    void UpdateReceptorSensitivity(float DeltaTime);
    void UpdateChronicStress(float DeltaTime);
    float CalculateCortisolProduction(float DeltaTime);
    float CalculateCortisolClearance(float DeltaTime);
    void UpdateFreeAndBoundCortisol();

private:
    UPROPERTY()
    FCortisolDynamicsState State;

    UPROPERTY()
    TArray<FStressEvent> ActiveStressEvents;

    /** Simulated time advancement rate */
    UPROPERTY(EditAnywhere, Category = "Cortisol")
    float TimeScale;

    /** Cortisol half-life scaling factor (game-time seconds) */
    UPROPERTY(EditAnywhere, Category = "Cortisol")
    float ClearanceHalfLife;

    /** CRH -> ACTH coupling strength */
    UPROPERTY(EditAnywhere, Category = "Cortisol")
    float CRHToACTHGain;

    /** ACTH -> Cortisol coupling strength */
    UPROPERTY(EditAnywhere, Category = "Cortisol")
    float ACTHToCortisolGain;

    /** CRH natural decay rate */
    UPROPERTY(EditAnywhere, Category = "Cortisol")
    float CRHDecayRate;

    /** ACTH natural decay rate */
    UPROPERTY(EditAnywhere, Category = "Cortisol")
    float ACTHDecayRate;

    /** Rate at which chronic stress load accumulates */
    UPROPERTY(EditAnywhere, Category = "Cortisol")
    float ChronicStressAccumulationRate;

    /** Rate at which chronic stress load dissipates */
    UPROPERTY(EditAnywhere, Category = "Cortisol")
    float ChronicStressDissipationRate;

    /** Free cortisol fraction under normal conditions (~5%) */
    UPROPERTY(EditAnywhere, Category = "Cortisol")
    float NormalFreeFraction;
};
