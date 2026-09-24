# Feature F1.2.2: Liquid State Machine - Usage Examples

## Overview

This document provides practical examples for using the `ULiquidStateMachine` component in your Deep Tree Echo projects. The LSM component provides biologically-inspired spiking neural network reservoir computing for temporal pattern recognition, real-time signal processing, and event-based cognitive dynamics.

## Table of Contents

1. [Basic Setup](#basic-setup)
2. [Initialization](#initialization)
3. [Spike Encoding](#spike-encoding)
4. [Processing Input](#processing-input)
5. [Spike Decoding](#spike-decoding)
6. [STDP Learning](#stdp-learning)
7. [Lateral Inhibition](#lateral-inhibition)
8. [Homeostatic Plasticity](#homeostatic-plasticity)
9. [Blueprint Integration](#blueprint-integration)
10. [Advanced Configuration](#advanced-configuration)
11. [Performance Optimization](#performance-optimization)
12. [Troubleshooting](#troubleshooting)

---

## Basic Setup

### Adding LSM to Your Actor (C++)

```cpp
// In your actor header file
UCLASS()
class YOURPROJECT_API AMyActor : public AActor
{
    GENERATED_BODY()

public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
    ULiquidStateMachine* LSM;

    AMyActor()
    {
        LSM = CreateDefaultSubobject<ULiquidStateMachine>(TEXT("LSM"));
    }
};
```

### Adding LSM to Your Actor (Blueprint)

1. Open your Actor Blueprint
2. Click "Add Component" → Search for "Liquid State Machine"
3. Configure properties in the Details panel

---

## Initialization

### Basic Initialization

```cpp
void AMyActor::BeginPlay()
{
    Super::BeginPlay();

    // Configure LSM
    FLSMConfig Config;
    Config.NumNeurons = 200;              // Liquid size
    Config.InputDimension = 10;           // 10-dimensional input
    Config.OutputDimension = 5;           // 5-dimensional output
    Config.ConnectionProbability = 0.1f;  // 10% connectivity
    Config.ExcitatoryRatio = 0.8f;        // 80% excitatory neurons
    Config.InitialWeightScale = 1.0f;     // Weight initialization
    Config.Timestep = 1.0f;               // 1 ms per step

    // Initialize
    LSM->InitializeLSM(Config);

    UE_LOG(LogTemp, Display, TEXT("LSM initialized: %d neurons, %d synapses"),
           LSM->State.Neurons.Num(), LSM->State.Synapses.Num());
}
```

### Automatic Initialization

If `bEnableLSM` is true (default), the component auto-initializes with the `Config` property on `BeginPlay`:

```cpp
// Set defaults in constructor or editor; no explicit call needed
LSM->Config.NumNeurons = 300;
LSM->bEnableLSM = true;  // InitializeLSM called automatically in BeginPlay
```

### Resetting the Liquid

```cpp
// Reset membrane potentials and spike history (keeps topology and weights)
LSM->ResetLSM();
```

---

## Spike Encoding

### Rate Coding (Default)

Values (0–1) map to Poisson spike frequencies (0–100 Hz):

```cpp
TArray<float> Input = {0.8f, 0.2f, 0.5f};
TArray<FSpikeTrain> Trains = LSM->EncodeInput(
    Input,
    100.0f,                          // Duration in ms
    ESpikeEncodingType::RateCoding
);

// Train[0] has ~8 spikes, Train[1] ~2 spikes, Train[2] ~5 spikes (Poisson)
```

### Temporal Coding

Value maps to the time of a single spike within the window (higher value → later spike):

```cpp
TArray<FSpikeTrain> Trains = LSM->EncodeInput(
    Input, 100.0f, ESpikeEncodingType::TemporalCoding);

// Each train has exactly 1 spike at time = value * 100 ms
```

### Population Coding

Value encoded by a Gaussian-tuned ensemble — robust to noise:

```cpp
TArray<FSpikeTrain> Trains = LSM->EncodeInput(
    Input, 100.0f, ESpikeEncodingType::PopulationCoding);

// Each input dimension produces a population of spike trains
// whose rates follow Gaussian tuning curves centered on preferred values
```

### Setting the Default Input Encoding

```cpp
// Used by ProcessInput when encoding type isn't specified
LSM->InputEncodingType = ESpikeEncodingType::RateCoding;
```

---

## Processing Input

### Full Pipeline (Encode → Simulate → Decode)

```cpp
// One-call processing: encodes, injects, simulates for Duration, decodes
TArray<float> Input = {0.5f, 0.9f, 0.1f, 0.3f, 0.7f, 0.0f, 1.0f, 0.4f, 0.6f, 0.2f};
TArray<float> Output = LSM->ProcessInput(Input, 100.0f);  // 100 ms window

// Output has Config.OutputDimension elements (rate-decoded from output neurons)
```

### Manual Step-By-Step Simulation

```cpp
// Encode input
TArray<FSpikeTrain> Trains = LSM->EncodeInput(Input, 100.0f, ESpikeEncodingType::RateCoding);

// Inject spikes into the liquid
for (const FSpikeTrain& Train : Trains)
{
    for (float SpikeTime : Train.SpikeTimes)
    {
        LSM->InjectSpike(Train.NeuronID, SpikeTime);
    }
}

// Step simulation manually (e.g., synced to game tick)
for (int32 Step = 0; Step < 100; ++Step)
{
    LSM->StepSimulation(1.0f);  // 1 ms steps
}

// Read the liquid state (normalized membrane potentials, 0–1)
TArray<float> LiquidState = LSM->GetLiquidState();
```

### Tick-Driven Simulation

The component ticks automatically (`TG_PrePhysics`). With `bEnableLSM = true`, each frame advances the simulation by `DeltaTime` (scaled to ms):

```cpp
// Nothing to do — StepSimulation runs each tick
// Inject current in response to game events:
void AMyActor::OnSensorTriggered(float Intensity)
{
    LSM->InjectCurrent(/*NeuronID=*/0, Intensity * 15.0f);
}
```

---

## Spike Decoding

### Window-Based Rate Decoding

```cpp
// Decode spike trains to normalized rates (0–1)
TArray<float> Values = LSM->DecodeOutput(Trains, 100.0f);  // 100 ms window
```

### Direct Spike Queries

```cpp
// Firing rate of a specific neuron over the last 500 ms
float Rate = LSM->GetNeuronFiringRate(/*NeuronID=*/42, /*WindowSize=*/500.0f);

// Total spikes in an absolute time window
int32 Count = LSM->GetSpikeCount(/*StartTime=*/0.0f, /*EndTime=*/1000.0f);

// Network-wide activity (mean |V − V_rest|)
float Activity = LSM->GetActivityLevel();

// Full liquid state vector for external readout training
TArray<float> StateVector = LSM->GetLiquidState();
```

---

## STDP Learning

### Enabling STDP

```cpp
FLSMConfig Config;
Config.STDPConfig.bEnableSTDP = true;
Config.STDPConfig.LearningRate = 0.01f;   // η
Config.STDPConfig.TauPlus = 20.0f;        // LTP window (ms)
Config.STDPConfig.TauMinus = 20.0f;       // LTD window (ms)
Config.STDPConfig.APlus = 0.005f;         // LTP amplitude
Config.STDPConfig.AMinus = 0.00525f;      // LTD amplitude (slightly larger for stability)
Config.STDPConfig.WeightMin = 0.0f;
Config.STDPConfig.WeightMax = 10.0f;
LSM->InitializeLSM(Config);

// STDP is applied automatically each StepSimulation
```

### Manual STDP Application

```cpp
// Force an STDP pass (e.g., after a burst of activity)
LSM->ApplySTDP();

// Inspect learned weights
for (const FSynapseState& Syn : LSM->State.Synapses)
{
    UE_LOG(LogTemp, Verbose, TEXT("Synapse %d→%d: w=%.3f"),
           Syn.PreNeuronID, Syn.PostNeuronID, Syn.Weight);
}
```

### Temporal Association Learning Example

```cpp
// Train the liquid to associate input spike patterns:
// repeatedly present pattern A, then pattern B, 10 ms apart
for (int32 Trial = 0; Trial < 100; ++Trial)
{
    LSM->InjectSpike(/*PatternANeuron=*/5, LSM->State.CurrentTime + 1.0f);
    LSM->InjectSpike(/*PatternBNeuron=*/17, LSM->State.CurrentTime + 11.0f);
    LSM->StepSimulation(20.0f);
}
// Synapses from A→B potentiate; B→A depress (causal asymmetry)
```

---

## Lateral Inhibition

### Soft Competition (Default)

```cpp
Config.InhibitionConfig.bEnableInhibition = true;
Config.InhibitionConfig.InhibitionRadius = 5;      // Spatial radius on 3D grid
Config.InhibitionConfig.InhibitionStrength = 2.0f; // mV suppression
Config.InhibitionConfig.bWinnerTakeAll = false;
```

### Winner-Take-All (Sparse Coding)

```cpp
Config.InhibitionConfig.bWinnerTakeAll = true;
// Only the most depolarized recently-active neuron survives each step;
// all other active neurons are reset to resting potential
```

### Manual Inhibition Pass

```cpp
LSM->ApplyLateralInhibition();
```

---

## Homeostatic Plasticity

### Firing Rate Regulation

```cpp
Config.HomeostaticConfig.bEnableHomeostasis = true;
Config.HomeostaticConfig.TargetFiringRate = 5.0f;        // Hz
Config.HomeostaticConfig.TauHomeostasis = 10.0f;         // seconds
Config.HomeostaticConfig.ThresholdAdjustmentRate = 0.001f;
Config.HomeostaticConfig.ThresholdMin = -60.0f;          // mV
Config.HomeostaticConfig.ThresholdMax = -50.0f;          // mV
```

Neurons firing above target raise their threshold (harder to spike); silent neurons lower theirs. Applied automatically each step when enabled, or manually:

```cpp
LSM->ApplyHomeostaticPlasticity();
```

### Long-Running Stability Example

```cpp
// Run for simulated minutes without runaway excitation or silence
Config.HomeostaticConfig.bEnableHomeostasis = true;
Config.HomeostaticConfig.TargetFiringRate = 5.0f;
LSM->InitializeLSM(Config);

for (int32 Sec = 0; Sec < 60; ++Sec)
{
    for (int32 Ms = 0; Ms < 1000; ++Ms)
    {
        LSM->StepSimulation(1.0f);
    }
    UE_LOG(LogTemp, Log, TEXT("t=%ds avg rate: %.2f Hz"),
           Sec, LSM->State.AverageFiringRate);  // Converges toward 5 Hz
}
```

---

## Blueprint Integration

All public API functions are `BlueprintCallable`:

1. **Initialize LSM** — drag from component → "Initialize LSM" → configure `FLSMConfig`
2. **Process Input** — "Process Input" node takes a float array + duration, returns float array
3. **Encode/Decode** — "Encode Input" / "Decode Output" with encoding-type enum dropdown
4. **Inject Spike / Current** — event-driven stimulation
5. **Analysis** — "Get Liquid State", "Get Activity Level", "Get Neuron Firing Rate"

Example Blueprint flow:
```
[Event BeginPlay] → [Initialize LSM (Config)]
[Event Tick]      → (automatic via bEnableLSM)
[On Sensor Event] → [Inject Current] → [Get Liquid State] → [Feed to ESN/Readout]
```

---

## Advanced Configuration

### Small Fast Liquid (Real-Time Control)

```cpp
FLSMConfig Config;
Config.NumNeurons = 64;
Config.ConnectionProbability = 0.2f;   // Denser for small networks
Config.Timestep = 0.5f;                // Sub-millisecond resolution
Config.SpikeCurrentAmplitude = 20.0f;  // Stronger drive
```

### Large Liquid (Rich Temporal Memory)

```cpp
FLSMConfig Config;
Config.NumNeurons = 1000;
Config.ConnectionProbability = 0.05f;  // Sparser for large networks
Config.Timestep = 1.0f;
Config.SynapticTimeWindow = 30.0f;     // Longer synaptic memory
Config.SynapticTimeConstant = 8.0f;
```

### Full Plasticity Stack

```cpp
Config.STDPConfig.bEnableSTDP = true;
Config.InhibitionConfig.bEnableInhibition = true;
Config.HomeostaticConfig.bEnableHomeostasis = true;
// STDP shapes weights, inhibition enforces competition,
// homeostasis maintains long-term stability
```

### Integration with Echo State Network (Hybrid Reservoir)

```cpp
// Spiking front-end (LSM) + rate-based reservoir (ESN)
ULiquidStateMachine* LSM = NewObject<ULiquidStateMachine>(this);
UEchoStateNetwork* ESN = NewObject<UEchoStateNetwork>(this);

LSM->InitializeLSM(LSMConfig);

FESNConfig ESNConfig;
ESNConfig.ReservoirSize = 100;
ESNConfig.InputDim = LSMConfig.NumNeurons;  // Liquid state feeds ESN
ESN->InitializeWithConfig(ESNConfig);

// Per frame:
LSM->StepSimulation(DeltaMs);
TArray<float> LiquidState = LSM->GetLiquidState();
TArray<float> ReservoirState = ESN->ProcessInput(LiquidState);
```

---

## Performance Optimization

1. **Reduce neuron count** — Simulation scales O(N²) for construction, O(N+S) per step
2. **Lower connection probability** — Fewer synapses = faster STDP and propagation
3. **Increase timestep** — 2 ms steps halve CPU cost (reduced temporal resolution)
4. **Disable unused plasticity** — Turn off STDP/homeostasis if only inference is needed
5. **Avoid per-tick `ProcessInput`** — Prefer targeted `InjectSpike`/`InjectCurrent` on events
6. **Use rate decoding windows ≥ 50 ms** — Smaller windows increase variance without detail

---

## Troubleshooting

### Network Too Silent (Few/No Spikes)

1. Increase `SpikeCurrentAmplitude` (default 15.0 mV)
2. Increase `InitialWeightScale` or `ConnectionProbability`
3. Verify input encoding produces spikes (check `EncodeInput` output)
4. Check thresholds haven't drifted too high (homeostasis with low target rate)

### Runaway Excitation (Saturated Spiking)

1. Enable homeostatic plasticity with a moderate `TargetFiringRate`
2. Enable lateral inhibition or increase `InhibitionStrength`
3. Lower `ExcitatoryRatio` (e.g., 0.7)
4. Reduce `InitialWeightScale`

### STDP Not Learning

1. Verify `STDPConfig.bEnableSTDP = true`
2. Ensure neurons actually spike (check `State.TotalSpikeCount`)
3. Increase `LearningRate` if changes are too subtle
4. Check `WeightMin`/`WeightMax` aren't clamping all weights

### Poor Temporal Discrimination

1. Increase `NumNeurons` and/or `ConnectionProbability`
2. Tune `TauMembrane` / `SynapticTimeConstant` for the target timescale
3. Enable STDP so recurrent pathways strengthen
4. Consider population coding for richer input representation

### Threshold Oscillations (Unstable Homeostasis)

1. Increase `TauHomeostasis` (slower adaptation)
2. Reduce `ThresholdAdjustmentRate`
3. Verify `TargetFiringRate` is achievable with given input drive

---

## See Also

- **Implementation Summary:** FEATURE_F1.2.2_IMPLEMENTATION_SUMMARY.md
- **Maintenance Agent:** .github/agents/u9ci/F1.2.2.md
- **API Reference:** DeepTreeEcho/Reservoir/LiquidStateMachine.h
- **Unit Tests:** DeepTreeEcho/Testing/UnitTests/LiquidStateMachineTests.cpp
- **Related:** F1.2.1 Echo State Network, F1.2.3 Reservoir Topology Generator, F1.2.4 Readout Layer Training
