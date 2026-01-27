#pragma once

/**
 * Liquid State Machine - Spiking Neural Network Reservoir
 * 
 * Implements biologically-inspired spiking neural network reservoir with:
 * - Leaky Integrate-and-Fire (LIF) neuron model
 * - Spike-Timing-Dependent Plasticity (STDP)
 * - Spike encoding/decoding mechanisms
 * - Lateral inhibition for competition
 * - Homeostatic plasticity for stability
 * 
 * Based on:
 * - Maass et al. (2002): "Real-Time Computing Without Stable States"
 * - Natschläger et al. (2002): "The 'Liquid Computer'"
 * - Legenstein & Maass (2007): "Edge of Chaos and Prediction of Computational Performance"
 * 
 * Integration with Deep Tree Echo:
 * - Extends reservoir computing with spiking dynamics
 * - Provides biologically-plausible temporal processing
 * - Supports hierarchical LSM architectures
 */

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LiquidStateMachine.generated.h"

/**
 * LIF Neuron State - Leaky Integrate-and-Fire neuron
 */
USTRUCT(BlueprintType)
struct FLIFNeuronState
{
    GENERATED_BODY()

    /** Neuron ID */
    UPROPERTY(BlueprintReadWrite)
    int32 NeuronID = 0;

    /** Membrane potential (mV) */
    UPROPERTY(BlueprintReadWrite)
    float MembranePotential = -70.0f;

    /** Resting potential (mV) */
    UPROPERTY(BlueprintReadWrite)
    float RestingPotential = -70.0f;

    /** Threshold potential (mV) */
    UPROPERTY(BlueprintReadWrite)
    float ThresholdPotential = -55.0f;

    /** Reset potential (mV) */
    UPROPERTY(BlueprintReadWrite)
    float ResetPotential = -75.0f;

    /** Membrane time constant (ms) */
    UPROPERTY(BlueprintReadWrite)
    float TauMembrane = 20.0f;

    /** Refractory period (ms) */
    UPROPERTY(BlueprintReadWrite)
    float RefractoryPeriod = 2.0f;

    /** Time since last spike (ms) */
    UPROPERTY(BlueprintReadWrite)
    float TimeSinceSpike = 1000.0f;

    /** Is currently spiking */
    UPROPERTY(BlueprintReadWrite)
    bool bIsSpiking = false;

    /** Spike count */
    UPROPERTY(BlueprintReadWrite)
    int32 SpikeCount = 0;

    /** Last spike time (ms) */
    UPROPERTY(BlueprintReadWrite)
    float LastSpikeTime = -1000.0f;
};

/**
 * Synapse State - Connection between neurons with STDP
 */
USTRUCT(BlueprintType)
struct FSynapseState
{
    GENERATED_BODY()

    /** Synapse ID */
    UPROPERTY(BlueprintReadWrite)
    int32 SynapseID = 0;

    /** Pre-synaptic neuron ID */
    UPROPERTY(BlueprintReadWrite)
    int32 PreNeuronID = 0;

    /** Post-synaptic neuron ID */
    UPROPERTY(BlueprintReadWrite)
    int32 PostNeuronID = 0;

    /** Synaptic weight */
    UPROPERTY(BlueprintReadWrite)
    float Weight = 1.0f;

    /** Delay (ms) */
    UPROPERTY(BlueprintReadWrite)
    float Delay = 1.0f;

    /** Is excitatory (true) or inhibitory (false) */
    UPROPERTY(BlueprintReadWrite)
    bool bIsExcitatory = true;

    /** Last pre-synaptic spike time */
    UPROPERTY(BlueprintReadWrite)
    float LastPreSpikeTime = -1000.0f;

    /** Last post-synaptic spike time */
    UPROPERTY(BlueprintReadWrite)
    float LastPostSpikeTime = -1000.0f;

    /** STDP eligibility trace */
    UPROPERTY(BlueprintReadWrite)
    float EligibilityTrace = 0.0f;
};

/**
 * STDP Configuration - Spike-Timing-Dependent Plasticity
 */
USTRUCT(BlueprintType)
struct FSTDPConfig
{
    GENERATED_BODY()

    /** Enable STDP learning */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bEnableSTDP = true;

    /** Learning rate */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float LearningRate = 0.01f;

    /** Time constant for LTP (ms) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float TauPlus = 20.0f;

    /** Time constant for LTD (ms) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float TauMinus = 20.0f;

    /** Maximum weight for LTP */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float APlus = 0.005f;

    /** Maximum weight for LTD */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float AMinus = 0.00525f;

    /** Minimum synaptic weight */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float WeightMin = 0.0f;

    /** Maximum synaptic weight */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float WeightMax = 10.0f;
};

/**
 * Lateral Inhibition Configuration
 */
USTRUCT(BlueprintType)
struct FLateralInhibitionConfig
{
    GENERATED_BODY()

    /** Enable lateral inhibition */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bEnableInhibition = true;

    /** Inhibition radius (neuron indices) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 InhibitionRadius = 5;

    /** Inhibition strength */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float InhibitionStrength = 2.0f;

    /** Winner-take-all mode */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bWinnerTakeAll = false;
};

/**
 * Homeostatic Plasticity Configuration
 */
USTRUCT(BlueprintType)
struct FHomeostaticPlasticityConfig
{
    GENERATED_BODY()

    /** Enable homeostatic plasticity */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bEnableHomeostasis = true;

    /** Target firing rate (Hz) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float TargetFiringRate = 5.0f;

    /** Homeostatic time constant (seconds) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float TauHomeostasis = 10.0f;

    /** Threshold adjustment rate */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ThresholdAdjustmentRate = 0.001f;

    /** Minimum threshold (mV) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ThresholdMin = -60.0f;

    /** Maximum threshold (mV) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ThresholdMax = -50.0f;
};

/**
 * Spike Encoding Type
 */
UENUM(BlueprintType)
enum class ESpikeEncodingType : uint8
{
    /** Rate coding - spike frequency encodes value */
    RateCoding       UMETA(DisplayName = "Rate Coding"),
    
    /** Temporal coding - spike timing encodes value */
    TemporalCoding   UMETA(DisplayName = "Temporal Coding"),
    
    /** Population coding - ensemble encodes value */
    PopulationCoding UMETA(DisplayName = "Population Coding"),
    
    /** Phase coding - spike phase encodes value */
    PhaseCoding      UMETA(DisplayName = "Phase Coding")
};

/**
 * Spike Train - Sequence of spike times
 */
USTRUCT(BlueprintType)
struct FSpikeTrain
{
    GENERATED_BODY()

    /** Neuron ID */
    UPROPERTY(BlueprintReadWrite)
    int32 NeuronID = 0;

    /** Spike times (ms) */
    UPROPERTY(BlueprintReadWrite)
    TArray<float> SpikeTimes;

    /** Encoding type used */
    UPROPERTY(BlueprintReadWrite)
    ESpikeEncodingType EncodingType = ESpikeEncodingType::RateCoding;
};

/**
 * LSM Configuration
 */
USTRUCT(BlueprintType)
struct FLSMConfig
{
    GENERATED_BODY()

    /** Number of neurons in liquid */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "10", ClampMax = "10000"))
    int32 NumNeurons = 200;

    /** Input dimensionality */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1", ClampMax = "1000"))
    int32 InputDimension = 10;

    /** Output dimensionality */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1", ClampMax = "1000"))
    int32 OutputDimension = 5;

    /** Connection probability */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ConnectionProbability = 0.1f;

    /** Excitatory neuron ratio */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ExcitatoryRatio = 0.8f;

    /** Initial weight scale */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float InitialWeightScale = 1.0f;

    /** Simulation timestep (ms) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Timestep = 1.0f;

    /** STDP configuration */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FSTDPConfig STDPConfig;

    /** Lateral inhibition configuration */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLateralInhibitionConfig InhibitionConfig;

    /** Homeostatic plasticity configuration */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FHomeostaticPlasticityConfig HomeostaticConfig;
};

/**
 * LSM State - Current state of the liquid
 */
USTRUCT(BlueprintType)
struct FLSMState
{
    GENERATED_BODY()

    /** All neuron states */
    UPROPERTY(BlueprintReadWrite)
    TArray<FLIFNeuronState> Neurons;

    /** All synapse states */
    UPROPERTY(BlueprintReadWrite)
    TArray<FSynapseState> Synapses;

    /** Current simulation time (ms) */
    UPROPERTY(BlueprintReadWrite)
    float CurrentTime = 0.0f;

    /** Recent spike trains */
    UPROPERTY(BlueprintReadWrite)
    TArray<FSpikeTrain> RecentSpikeTrains;

    /** Average firing rate (Hz) */
    UPROPERTY(BlueprintReadWrite)
    float AverageFiringRate = 0.0f;

    /** Total spike count */
    UPROPERTY(BlueprintReadWrite)
    int32 TotalSpikeCount = 0;

    /** Is initialized */
    UPROPERTY(BlueprintReadWrite)
    bool bIsInitialized = false;
};

/**
 * Liquid State Machine Component
 * Implements spiking neural network reservoir with biologically-inspired dynamics
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API ULiquidStateMachine : public UActorComponent
{
    GENERATED_BODY()

public:
    ULiquidStateMachine();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** LSM Configuration */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LSM|Config")
    FLSMConfig Config;

    /** Enable LSM processing */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LSM|Config")
    bool bEnableLSM = true;

    /** Spike encoding type for input */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LSM|Config")
    ESpikeEncodingType InputEncodingType = ESpikeEncodingType::RateCoding;

    // ========================================
    // STATE
    // ========================================

    /** Current LSM state */
    UPROPERTY(BlueprintReadOnly, Category = "LSM|State")
    FLSMState State;

    // ========================================
    // PUBLIC API - INITIALIZATION
    // ========================================

    /** Initialize the liquid state machine */
    UFUNCTION(BlueprintCallable, Category = "LSM")
    void InitializeLSM(const FLSMConfig& InConfig);

    /** Reset the LSM to initial state */
    UFUNCTION(BlueprintCallable, Category = "LSM")
    void ResetLSM();

    // ========================================
    // PUBLIC API - SPIKE ENCODING/DECODING
    // ========================================

    /** Encode continuous values to spike trains */
    UFUNCTION(BlueprintCallable, Category = "LSM|Encoding")
    TArray<FSpikeTrain> EncodeInput(const TArray<float>& Input, float Duration, ESpikeEncodingType EncodingType);

    /** Decode spike trains to continuous values */
    UFUNCTION(BlueprintCallable, Category = "LSM|Decoding")
    TArray<float> DecodeOutput(const TArray<FSpikeTrain>& SpikeTrains, float WindowSize);

    // ========================================
    // PUBLIC API - SIMULATION
    // ========================================

    /** Process input through the liquid */
    UFUNCTION(BlueprintCallable, Category = "LSM|Processing")
    TArray<float> ProcessInput(const TArray<float>& Input, float Duration);

    /** Step simulation forward by one timestep */
    UFUNCTION(BlueprintCallable, Category = "LSM|Simulation")
    void StepSimulation(float DeltaTime);

    /** Inject spike to specific neuron */
    UFUNCTION(BlueprintCallable, Category = "LSM|Simulation")
    void InjectSpike(int32 NeuronID, float SpikeTime);

    /** Inject current to specific neuron */
    UFUNCTION(BlueprintCallable, Category = "LSM|Simulation")
    void InjectCurrent(int32 NeuronID, float Current);

    // ========================================
    // PUBLIC API - PLASTICITY
    // ========================================

    /** Apply STDP learning rule */
    UFUNCTION(BlueprintCallable, Category = "LSM|Plasticity")
    void ApplySTDP();

    /** Apply homeostatic plasticity */
    UFUNCTION(BlueprintCallable, Category = "LSM|Plasticity")
    void ApplyHomeostaticPlasticity();

    /** Apply lateral inhibition */
    UFUNCTION(BlueprintCallable, Category = "LSM|Plasticity")
    void ApplyLateralInhibition();

    // ========================================
    // PUBLIC API - ANALYSIS
    // ========================================

    /** Get current firing rate of neuron */
    UFUNCTION(BlueprintCallable, Category = "LSM|Analysis")
    float GetNeuronFiringRate(int32 NeuronID, float WindowSize) const;

    /** Get liquid state vector (all membrane potentials) */
    UFUNCTION(BlueprintCallable, Category = "LSM|Analysis")
    TArray<float> GetLiquidState() const;

    /** Get spike count in time window */
    UFUNCTION(BlueprintCallable, Category = "LSM|Analysis")
    int32 GetSpikeCount(float StartTime, float EndTime) const;

    /** Get network activity level */
    UFUNCTION(BlueprintCallable, Category = "LSM|Analysis")
    float GetActivityLevel() const;

protected:
    virtual void BeginPlay() override;

private:
    // ========================================
    // INTERNAL STATE
    // ========================================

    /** Spike queue for delayed spikes */
    TArray<TPair<float, int32>> SpikeQueue;

    /** Readout layer weights (for decoding) */
    TArray<TArray<float>> ReadoutWeights;

    /** Firing rate history for homeostasis */
    TMap<int32, TArray<float>> FiringRateHistory;

    /** Simulation time accumulator */
    float TimeAccumulator = 0.0f;

    // ========================================
    // INTERNAL METHODS - LIF DYNAMICS
    // ========================================

    /** Update LIF neuron state */
    void UpdateLIFNeuron(FLIFNeuronState& Neuron, float DeltaTime, float InputCurrent);

    /** Check if neuron should spike */
    bool CheckSpikeCondition(const FLIFNeuronState& Neuron) const;

    /** Handle neuron spike event */
    void HandleNeuronSpike(FLIFNeuronState& Neuron, float CurrentTime);

    // ========================================
    // INTERNAL METHODS - SYNAPTIC DYNAMICS
    // ========================================

    /** Compute synaptic input to neuron */
    float ComputeSynapticInput(int32 NeuronID) const;

    /** Update synapse with STDP */
    void UpdateSynapseSTDP(FSynapseState& Synapse, float PreSpikeTime, float PostSpikeTime);

    /** Propagate spike through synapses */
    void PropagateSpikeToSynapses(int32 NeuronID, float SpikeTime);

    // ========================================
    // INTERNAL METHODS - ENCODING/DECODING
    // ========================================

    /** Rate coding: value -> spike frequency */
    FSpikeTrain RateEncode(float Value, float Duration, int32 NeuronID);

    /** Temporal coding: value -> first spike time */
    FSpikeTrain TemporalEncode(float Value, float Duration, int32 NeuronID);

    /** Population coding: value -> ensemble */
    TArray<FSpikeTrain> PopulationEncode(float Value, float Duration, int32 StartNeuronID, int32 PopulationSize);

    /** Decode spikes to rate */
    float RateDecode(const FSpikeTrain& SpikeTrain, float WindowSize) const;

    // ========================================
    // INTERNAL METHODS - NETWORK CONSTRUCTION
    // ========================================

    /** Build initial network topology */
    void BuildNetwork();

    /** Create synaptic connection */
    void CreateSynapse(int32 PreID, int32 PostID, float Weight, float Delay, bool bExcitatory);

    /** Initialize readout layer */
    void InitializeReadoutLayer();

    // ========================================
    // INTERNAL METHODS - UTILITIES
    // ========================================

    /** Generate random connection pattern */
    bool ShouldConnect(int32 PreID, int32 PostID) const;

    /** Compute spatial distance between neurons */
    float GetNeuronDistance(int32 NeuronID1, int32 NeuronID2) const;

    /** Update firing rate statistics */
    void UpdateFiringRateStatistics(float DeltaTime);

    /** Clamp value to range */
    float ClampValue(float Value, float Min, float Max) const;
};
