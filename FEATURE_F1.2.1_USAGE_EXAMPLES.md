# Feature F1.2.1: Echo State Network - Usage Examples

## Overview

This document provides practical examples for using the `UEchoStateNetwork` component in your Deep Tree Echo projects. The ESN component provides efficient reservoir computing capabilities for temporal pattern recognition, time series prediction, and dynamic state processing.

## Table of Contents

1. [Basic Setup](#basic-setup)
2. [Initialization](#initialization)
3. [Processing Input](#processing-input)
4. [Training Output Weights](#training-output-weights)
5. [Time Series Prediction](#time-series-prediction)
6. [Pattern Recognition](#pattern-recognition)
7. [Blueprint Integration](#blueprint-integration)
8. [Advanced Configuration](#advanced-configuration)
9. [Performance Optimization](#performance-optimization)
10. [Troubleshooting](#troubleshooting)

---

## Basic Setup

### Adding ESN to Your Actor (C++)

```cpp
// In your actor header file
UCLASS()
class YOURPROJECT_API AMyActor : public AActor
{
    GENERATED_BODY()

public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
    UEchoStateNetwork* ESN;

    AMyActor()
    {
        ESN = CreateDefaultSubobject<UEchoStateNetwork>(TEXT("ESN"));
    }
};
```

### Adding ESN to Your Actor (Blueprint)

1. Open your Actor Blueprint
2. Click "Add Component" → Search for "Echo State Network"
3. Configure properties in the Details panel

---

## Initialization

### Basic Initialization

```cpp
void AMyActor::BeginPlay()
{
    Super::BeginPlay();

    // Configure ESN
    FESNConfig Config;
    Config.ReservoirSize = 100;        // 100 neurons
    Config.InputDim = 10;              // 10-dimensional input
    Config.OutputDim = 5;              // 5-dimensional output
    Config.SpectralRadius = 0.9f;      // Stable dynamics
    Config.LeakRate = 0.3f;            // Moderate memory
    Config.ReservoirSparsity = 0.1f;   // 10% connectivity
    Config.RandomSeed = 42;            // Reproducible results

    // Initialize
    if (!ESN->InitializeWithConfig(Config))
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to initialize ESN"));
    }
    else
    {
        UE_LOG(LogTemp, Display, TEXT("ESN initialized: %s"), 
               *ESN->GetESNInfo());
    }
}
```

### Quick Initialization (Default Config)

```cpp
// Use default configuration
if (!ESN->Initialize())
{
    UE_LOG(LogTemp, Error, TEXT("ESN initialization failed"));
}
```

---

## Processing Input

### Single Input

```cpp
void AMyActor::ProcessSensorData(const TArray<float>& SensorReadings)
{
    // Process input through ESN
    TArray<float> ReservoirState = ESN->ProcessInput(SensorReadings);

    // Use reservoir state for further processing
    UE_LOG(LogTemp, Verbose, TEXT("Reservoir state size: %d"), 
           ReservoirState.Num());
    
    // Check average activation
    float AvgActivation = ESN->GetAverageActivation();
    UE_LOG(LogTemp, Verbose, TEXT("Average activation: %.4f"), 
           AvgActivation);
}
```

### Sequence Processing

```cpp
void AMyActor::ProcessSequence(const TArray<TArray<float>>& InputSequence)
{
    // Reset state for clean processing
    ESN->ResetState();

    // Process entire sequence
    TArray<TArray<float>> States = ESN->ProcessSequence(InputSequence);

    UE_LOG(LogTemp, Display, TEXT("Processed %d timesteps"), States.Num());

    // States now contains reservoir state at each timestep
    // Can be used for readout, analysis, or memory consolidation
}
```

---

## Training Output Weights

### Basic Training

```cpp
void AMyActor::TrainESN()
{
    // Prepare training data
    TArray<TArray<float>> TrainingInputs;
    TArray<TArray<float>> TrainingTargets;

    // Example: Learn sine wave from noisy input
    for (int32 i = 0; i < 1000; ++i)
    {
        float t = i * 0.1f;
        
        // Input: time and noisy observation
        TArray<float> Input;
        Input.Add(t);
        Input.Add(FMath::Sin(t) + FMath::FRandRange(-0.1f, 0.1f));
        TrainingInputs.Add(Input);

        // Target: clean sine wave
        TArray<float> Target;
        Target.Add(FMath::Sin(t));
        TrainingTargets.Add(Target);
    }

    // Train output weights
    float Error = ESN->TrainOutputWeights(TrainingInputs, TrainingTargets, 100);

    if (Error >= 0.0f)
    {
        UE_LOG(LogTemp, Display, TEXT("Training complete. NRMSE: %.6f"), Error);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Training failed"));
    }
}
```

### Incremental Training

```cpp
void AMyActor::IncrementalTraining()
{
    // Train on first batch
    TArray<TArray<float>> Batch1Inputs;
    TArray<TArray<float>> Batch1Targets;
    // ... fill batch1 ...
    ESN->TrainOutputWeights(Batch1Inputs, Batch1Targets, 50);

    // Can continue with more data
    TArray<TArray<float>> Batch2Inputs;
    TArray<TArray<float>> Batch2Targets;
    // ... fill batch2 ...
    ESN->TrainOutputWeights(Batch2Inputs, Batch2Targets, 0); // No washout
}
```

---

## Time Series Prediction

### Single-Step Ahead Prediction

```cpp
void AMyActor::PredictNextValue()
{
    // Current observation
    TArray<float> CurrentObservation = GetCurrentSensorReadings();

    // Update ESN state
    ESN->ProcessInput(CurrentObservation);

    // Predict next value
    TArray<float> Prediction = ESN->ComputeOutput();

    UE_LOG(LogTemp, Display, TEXT("Predicted next value: %.4f"), 
           Prediction.Num() > 0 ? Prediction[0] : 0.0f);
}
```

### Multi-Step Prediction

```cpp
TArray<TArray<float>> AMyActor::PredictFuture(int32 NumSteps)
{
    TArray<TArray<float>> Predictions;
    Predictions.Reserve(NumSteps);

    for (int32 step = 0; step < NumSteps; ++step)
    {
        // Get current prediction
        TArray<float> Prediction = ESN->ComputeOutput();
        Predictions.Add(Prediction);

        // Feed prediction back as input (autonomous mode)
        ESN->ProcessInput(Prediction);
    }

    return Predictions;
}
```

---

## Pattern Recognition

### Temporal Pattern Detection

```cpp
void AMyActor::DetectPattern(const TArray<TArray<float>>& Sequence)
{
    // Reset for clean detection
    ESN->ResetState();

    // Process sequence
    ESN->ProcessSequence(Sequence);

    // Analyze final state
    TArray<float> FinalState = ESN->GetReservoirState();

    // Use final state as pattern signature
    float PatternStrength = ESN->GetAverageActivation();

    if (PatternStrength > 0.5f)
    {
        UE_LOG(LogTemp, Display, TEXT("Strong pattern detected: %.4f"), 
               PatternStrength);
    }
}
```

### Pattern Classification

```cpp
int32 AMyActor::ClassifySequence(const TArray<TArray<float>>& Sequence)
{
    // Process sequence
    ESN->ResetState();
    ESN->ProcessSequence(Sequence);

    // Compute classification
    TArray<float> ClassScores = ESN->ComputeOutput();

    // Find max score
    int32 PredictedClass = 0;
    float MaxScore = ClassScores.Num() > 0 ? ClassScores[0] : 0.0f;

    for (int32 i = 1; i < ClassScores.Num(); ++i)
    {
        if (ClassScores[i] > MaxScore)
        {
            MaxScore = ClassScores[i];
            PredictedClass = i;
        }
    }

    UE_LOG(LogTemp, Display, TEXT("Predicted class: %d (score: %.4f)"), 
           PredictedClass, MaxScore);

    return PredictedClass;
}
```

---

## Blueprint Integration

### Blueprint Callable Functions

All main ESN functions are Blueprint-accessible:

```
Initialize() → bool
ProcessInput(Input) → Array<float>
ComputeOutput() → Array<float>
TrainOutputWeights(Inputs, Targets, Washout) → float
GetESNInfo() → String
GetSpectralRadius() → float
GetAverageActivation() → float
ResetState()
```

### Example Blueprint Node Setup

```
[BeginPlay]
  ↓
[ESN Initialize]
  ↓
[Branch] (Success?)
  ↓ True
[Print String] "ESN Ready"
  ↓
[Delay] (0.1s)
  ↓
[Process Sensor Input]
  ↓
[ESN ProcessInput] (Input Array)
  ↓
[ESN ComputeOutput]
  ↓
[Use Output]
```

---

## Advanced Configuration

### Optimizing for Long Memory

```cpp
FESNConfig LongMemoryConfig;
LongMemoryConfig.ReservoirSize = 500;    // Larger reservoir
LongMemoryConfig.SpectralRadius = 0.95f; // Closer to 1.0
LongMemoryConfig.LeakRate = 0.1f;        // Slow leak = long memory
LongMemoryConfig.ReservoirSparsity = 0.05f; // Sparser = longer paths
```

### Optimizing for Fast Dynamics

```cpp
FESNConfig FastDynamicsConfig;
FastDynamicsConfig.ReservoirSize = 100;
FastDynamicsConfig.SpectralRadius = 0.7f; // Lower SR = faster decay
FastDynamicsConfig.LeakRate = 0.8f;       // Fast leak = fast response
FastDynamicsConfig.NoiseLevel = 0.01f;    // Small noise for robustness
```

### Optimizing for Pattern Separation

```cpp
FESNConfig PatternSeparationConfig;
PatternSeparationConfig.ReservoirSize = 1000;  // Large for rich dynamics
PatternSeparationConfig.SpectralRadius = 0.9f;
PatternSeparationConfig.InputScaling = 0.5f;   // Scaled input
PatternSeparationConfig.ReservoirSparsity = 0.1f;
PatternSeparationConfig.ActivationFunction = TEXT("tanh"); // Non-linear
```

---

## Performance Optimization

### Reduce Reservoir Size

```cpp
// For real-time applications, smaller is faster
FESNConfig FastConfig;
FastConfig.ReservoirSize = 50;   // Smaller reservoir
FastConfig.InputDim = 5;         // Fewer inputs
FastConfig.ReservoirSparsity = 0.2f; // More sparse = fewer computations
```

### Batch Processing

```cpp
void AMyActor::BatchProcess()
{
    // Collect inputs over time
    TArray<TArray<float>> InputBatch;
    
    // ... collect inputs ...

    // Process all at once (more efficient)
    TArray<TArray<float>> StateBatch = ESN->ProcessSequence(InputBatch);
}
```

### Reduce Tick Frequency

```cpp
void AMyActor::OptimizeTickRate()
{
    // Don't process every frame
    ESN->PrimaryComponentTick.TickInterval = 0.1f; // 10 Hz instead of 30 Hz
    ESN->bAutoUpdate = false; // Disable auto-update
}
```

---

## Troubleshooting

### Problem: ESN produces NaN values

**Solution:**
- Reduce input scaling: `Config.InputScaling = 0.1f;`
- Reduce spectral radius: `Config.SpectralRadius = 0.7f;`
- Check for invalid inputs (NaN, Inf)

### Problem: ESN forgets too quickly

**Solution:**
- Increase spectral radius: `Config.SpectralRadius = 0.95f;`
- Decrease leak rate: `Config.LeakRate = 0.1f;`
- Increase reservoir size

### Problem: ESN doesn't learn

**Solution:**
- Increase training data size (>1000 samples)
- Increase washout period: `TrainOutputWeights(Inputs, Targets, 200);`
- Check input/target alignment
- Verify inputs are normalized

### Problem: High training error

**Solution:**
- Increase reservoir size: `Config.ReservoirSize = 500;`
- Adjust spectral radius: Try 0.8-0.95
- Increase connectivity: `Config.ReservoirSparsity = 0.15f;`
- Add more training data

### Problem: Slow performance

**Solution:**
- Reduce reservoir size
- Increase sparsity (reduce connectivity)
- Disable auto-update: `ESN->bAutoUpdate = false;`
- Call `ProcessInput()` less frequently

---

## Example: Complete Workflow

```cpp
void AMyActor::CompleteExample()
{
    // 1. Configure
    FESNConfig Config;
    Config.ReservoirSize = 200;
    Config.InputDim = 3;
    Config.OutputDim = 1;
    Config.SpectralRadius = 0.9f;
    Config.LeakRate = 0.3f;
    Config.RandomSeed = 42;

    // 2. Initialize
    if (!ESN->InitializeWithConfig(Config))
    {
        UE_LOG(LogTemp, Error, TEXT("Initialization failed"));
        return;
    }

    // 3. Prepare training data
    TArray<TArray<float>> TrainingInputs;
    TArray<TArray<float>> TrainingTargets;

    for (int32 i = 0; i < 1000; ++i)
    {
        float t = i * 0.01f;
        TArray<float> Input = {
            FMath::Sin(t),
            FMath::Cos(t),
            FMath::Sin(2.0f * t)
        };
        TrainingInputs.Add(Input);

        TArray<float> Target = {FMath::Sin(t + 0.1f)};
        TrainingTargets.Add(Target);
    }

    // 4. Train
    float Error = ESN->TrainOutputWeights(TrainingInputs, TrainingTargets, 100);
    UE_LOG(LogTemp, Display, TEXT("Training error: %.6f"), Error);

    // 5. Test
    ESN->ResetState();
    for (int32 i = 0; i < 10; ++i)
    {
        TArray<float> TestInput = TrainingInputs[i];
        ESN->ProcessInput(TestInput);
        TArray<float> Prediction = ESN->ComputeOutput();
        
        UE_LOG(LogTemp, Display, TEXT("Prediction[%d]: %.4f"), 
               i, Prediction[0]);
    }

    // 6. Get diagnostics
    UE_LOG(LogTemp, Display, TEXT("%s"), *ESN->GetESNInfo());
}
```

---

## See Also

- Feature F1.2.1 Agent Definition: `.github/agents/u9ci/F1.2.1.md`
- API Reference: `DeepTreeEcho/Reservoir/EchoStateNetwork.h`
- Unit Tests: `DeepTreeEcho/Testing/UnitTests/EchoStateNetworkTests.cpp`
- Integration Guide: `RESERVOIRCPP_INTEGRATION_GUIDE.md`
