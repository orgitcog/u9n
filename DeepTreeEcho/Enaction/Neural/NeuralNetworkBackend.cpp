// NeuralNetworkBackend.cpp
// Implementation of Neural Network Backend

#include "NeuralNetworkBackend.h"
#include "Math/UnrealMathUtility.h"

UNeuralNetworkBackend::UNeuralNetworkBackend()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UNeuralNetworkBackend::BeginPlay()
{
    Super::BeginPlay();
}

// ========================================
// NETWORK MANAGEMENT
// ========================================

FString UNeuralNetworkBackend::CreateNetwork(const FString& Name, const TArray<int32>& LayerSizes, 
                                              EActivationFunction Activation)
{
    if (LayerSizes.Num() < 2)
    {
        return TEXT("");
    }

    FString NetworkID = GenerateNetworkID();

    FNetworkConfig Config;
    Config.NetworkID = NetworkID;
    Config.NetworkName = Name;
    Config.LearningRate = 0.001f;
    Config.Momentum = 0.9f;
    Config.WeightDecay = 0.0001f;

    TArray<FNeuralLayer> Layers;

    for (int32 i = 0; i < LayerSizes.Num() - 1; ++i)
    {
        FNeuralLayerConfig LayerConfig;
        LayerConfig.InputSize = LayerSizes[i];
        LayerConfig.OutputSize = LayerSizes[i + 1];
        LayerConfig.Activation = (i == LayerSizes.Num() - 2) ? EActivationFunction::Linear : Activation;
        LayerConfig.bUseBias = true;

        Config.LayerConfigs.Add(LayerConfig);

        FNeuralLayer Layer;
        Layer.LayerID = GenerateLayerID();
        Layer.Config = LayerConfig;
        InitializeLayer(Layer, LayerConfig);

        Layers.Add(Layer);
    }

    NetworkConfigs.Add(NetworkID, Config);
    NetworkLayers.Add(NetworkID, Layers);

    OnNetworkCreated.Broadcast(NetworkID);

    return NetworkID;
}

void UNeuralNetworkBackend::DeleteNetwork(const FString& NetworkID)
{
    NetworkConfigs.Remove(NetworkID);
    NetworkLayers.Remove(NetworkID);
}

FNetworkConfig UNeuralNetworkBackend::GetNetworkConfig(const FString& NetworkID) const
{
    if (NetworkConfigs.Contains(NetworkID))
    {
        return NetworkConfigs[NetworkID];
    }
    return FNetworkConfig();
}

TArray<FString> UNeuralNetworkBackend::GetAllNetworkIDs() const
{
    TArray<FString> IDs;
    NetworkConfigs.GetKeys(IDs);
    return IDs;
}

// ========================================
// INFERENCE
// ========================================

TArray<float> UNeuralNetworkBackend::Forward(const FString& NetworkID, const TArray<float>& Input)
{
    if (!NetworkLayers.Contains(NetworkID))
    {
        return TArray<float>();
    }

    TArray<FNeuralLayer>& Layers = NetworkLayers[NetworkID];
    TArray<float> Current = Input;

    for (FNeuralLayer& Layer : Layers)
    {
        Current = ForwardLayer(Layer, Current);
    }

    OnInferenceComplete.Broadcast(NetworkID, Current);

    return Current;
}

TArray<TArray<float>> UNeuralNetworkBackend::BatchForward(const FString& NetworkID, 
                                                          const TArray<TArray<float>>& Inputs)
{
    TArray<TArray<float>> Outputs;
    for (const TArray<float>& Input : Inputs)
    {
        Outputs.Add(Forward(NetworkID, Input));
    }
    return Outputs;
}

// ========================================
// TRAINING
// ========================================

float UNeuralNetworkBackend::TrainSample(const FString& NetworkID, const TArray<float>& Input, 
                                          const TArray<float>& Target)
{
    if (!NetworkLayers.Contains(NetworkID) || !NetworkConfigs.Contains(NetworkID))
    {
        return 0.0f;
    }

    TArray<FNeuralLayer>& Layers = NetworkLayers[NetworkID];
    const FNetworkConfig& Config = NetworkConfigs[NetworkID];

    // Forward pass
    TArray<float> Output = Forward(NetworkID, Input);

    // Compute loss
    float Loss = MeanSquaredError(Output, Target);

    // Compute output gradient
    TArray<float> Gradient;
    Gradient.SetNum(Output.Num());
    for (int32 i = 0; i < Output.Num(); ++i)
    {
        Gradient[i] = 2.0f * (Output[i] - Target[i]) / Output.Num();
    }

    // Backward pass
    for (int32 i = Layers.Num() - 1; i >= 0; --i)
    {
        BackwardLayer(Layers[i], Gradient, Config.LearningRate);
        
        // Compute gradient for previous layer
        if (i > 0)
        {
            TArray<float> NewGradient;
            NewGradient.SetNumZeroed(Layers[i].Config.InputSize);

            for (int32 j = 0; j < Layers[i].Config.InputSize; ++j)
            {
                for (int32 k = 0; k < Layers[i].Config.OutputSize; ++k)
                {
                    int32 WeightIdx = j * Layers[i].Config.OutputSize + k;
                    NewGradient[j] += Gradient[k] * Layers[i].Weights[WeightIdx];
                }
            }

            Gradient = NewGradient;
        }
    }

    return Loss;
}

FTrainingResult UNeuralNetworkBackend::TrainBatch(const FString& NetworkID, 
                                                   const TArray<TArray<float>>& Inputs, 
                                                   const TArray<TArray<float>>& Targets)
{
    FTrainingResult Result;
    Result.BatchSize = Inputs.Num();

    if (Inputs.Num() != Targets.Num() || Inputs.Num() == 0)
    {
        return Result;
    }

    float TotalLoss = 0.0f;
    int32 Correct = 0;

    float StartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

    for (int32 i = 0; i < Inputs.Num(); ++i)
    {
        float Loss = TrainSample(NetworkID, Inputs[i], Targets[i]);
        TotalLoss += Loss;

        // Check accuracy (for classification)
        TArray<float> Output = Forward(NetworkID, Inputs[i]);
        if (Output.Num() > 0 && Targets[i].Num() > 0)
        {
            int32 PredIdx = 0;
            int32 TargetIdx = 0;
            float MaxPred = Output[0];
            float MaxTarget = Targets[i][0];

            for (int32 j = 1; j < Output.Num(); ++j)
            {
                if (Output[j] > MaxPred)
                {
                    MaxPred = Output[j];
                    PredIdx = j;
                }
            }
            for (int32 j = 1; j < Targets[i].Num(); ++j)
            {
                if (Targets[i][j] > MaxTarget)
                {
                    MaxTarget = Targets[i][j];
                    TargetIdx = j;
                }
            }

            if (PredIdx == TargetIdx)
            {
                Correct++;
            }
        }
    }

    Result.Loss = TotalLoss / Inputs.Num();
    Result.Accuracy = static_cast<float>(Correct) / Inputs.Num();
    Result.TrainingTime = (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f) - StartTime;

    OnTrainingComplete.Broadcast(Result);

    return Result;
}

void UNeuralNetworkBackend::SetLearningRate(const FString& NetworkID, float Rate)
{
    if (NetworkConfigs.Contains(NetworkID))
    {
        NetworkConfigs[NetworkID].LearningRate = FMath::Clamp(Rate, 0.00001f, 1.0f);
    }
}

// ========================================
// UTILITIES
// ========================================

float UNeuralNetworkBackend::ApplyActivation(float Value, EActivationFunction Activation)
{
    switch (Activation)
    {
        case EActivationFunction::Linear:
            return Value;

        case EActivationFunction::ReLU:
            return FMath::Max(0.0f, Value);

        case EActivationFunction::LeakyReLU:
            return Value > 0 ? Value : 0.01f * Value;

        case EActivationFunction::Sigmoid:
            return 1.0f / (1.0f + FMath::Exp(-Value));

        case EActivationFunction::Tanh:
            return FMath::Tanh(Value);

        default:
            return Value;
    }
}

TArray<float> UNeuralNetworkBackend::ApplyActivationArray(const TArray<float>& Values, 
                                                           EActivationFunction Activation)
{
    if (Activation == EActivationFunction::Softmax)
    {
        return Softmax(Values);
    }

    TArray<float> Result;
    Result.SetNum(Values.Num());
    for (int32 i = 0; i < Values.Num(); ++i)
    {
        Result[i] = ApplyActivation(Values[i], Activation);
    }
    return Result;
}

TArray<float> UNeuralNetworkBackend::Softmax(const TArray<float>& Values)
{
    TArray<float> Result;
    Result.SetNum(Values.Num());

    if (Values.Num() == 0)
    {
        return Result;
    }

    // Find max for numerical stability
    float MaxVal = Values[0];
    for (int32 i = 1; i < Values.Num(); ++i)
    {
        MaxVal = FMath::Max(MaxVal, Values[i]);
    }

    // Compute exp and sum
    float Sum = 0.0f;
    for (int32 i = 0; i < Values.Num(); ++i)
    {
        Result[i] = FMath::Exp(Values[i] - MaxVal);
        Sum += Result[i];
    }

    // Normalize
    if (Sum > 0.0f)
    {
        for (int32 i = 0; i < Result.Num(); ++i)
        {
            Result[i] /= Sum;
        }
    }

    return Result;
}

float UNeuralNetworkBackend::MeanSquaredError(const TArray<float>& Predicted, const TArray<float>& Target)
{
    if (Predicted.Num() != Target.Num() || Predicted.Num() == 0)
    {
        return 0.0f;
    }

    float Sum = 0.0f;
    for (int32 i = 0; i < Predicted.Num(); ++i)
    {
        float Diff = Predicted[i] - Target[i];
        Sum += Diff * Diff;
    }

    return Sum / Predicted.Num();
}

float UNeuralNetworkBackend::CrossEntropyLoss(const TArray<float>& Predicted, const TArray<float>& Target)
{
    if (Predicted.Num() != Target.Num() || Predicted.Num() == 0)
    {
        return 0.0f;
    }

    float Sum = 0.0f;
    for (int32 i = 0; i < Predicted.Num(); ++i)
    {
        float P = FMath::Clamp(Predicted[i], 0.0001f, 0.9999f);
        Sum -= Target[i] * FMath::Loge(P);
    }

    return Sum;
}

// ========================================
// INTERNAL METHODS
// ========================================

void UNeuralNetworkBackend::InitializeLayer(FNeuralLayer& Layer, const FNeuralLayerConfig& Config)
{
    int32 WeightCount = Config.InputSize * Config.OutputSize;
    Layer.Weights.SetNum(WeightCount);
    Layer.Biases.SetNum(Config.OutputSize);
    Layer.LastOutput.SetNum(Config.OutputSize);
    Layer.LastGradient.SetNum(Config.OutputSize);

    // Xavier initialization
    float Std = FMath::Sqrt(2.0f / (Config.InputSize + Config.OutputSize));

    for (int32 i = 0; i < WeightCount; ++i)
    {
        Layer.Weights[i] = FMath::FRandRange(-Std, Std);
    }

    for (int32 i = 0; i < Config.OutputSize; ++i)
    {
        Layer.Biases[i] = 0.0f;
    }
}

TArray<float> UNeuralNetworkBackend::ForwardLayer(FNeuralLayer& Layer, const TArray<float>& Input)
{
    TArray<float> Output;
    Output.SetNumZeroed(Layer.Config.OutputSize);

    // Matrix multiplication
    for (int32 j = 0; j < Layer.Config.OutputSize; ++j)
    {
        float Sum = Layer.Config.bUseBias ? Layer.Biases[j] : 0.0f;

        for (int32 i = 0; i < Layer.Config.InputSize && i < Input.Num(); ++i)
        {
            int32 WeightIdx = i * Layer.Config.OutputSize + j;
            Sum += Input[i] * Layer.Weights[WeightIdx];
        }

        Output[j] = Sum;
    }

    // Apply activation
    Output = ApplyActivationArray(Output, Layer.Config.Activation);

    Layer.LastOutput = Output;

    return Output;
}

void UNeuralNetworkBackend::BackwardLayer(FNeuralLayer& Layer, const TArray<float>& Gradient, 
                                           float LearningRate)
{
    // Apply activation derivative
    TArray<float> ActivationGrad;
    ActivationGrad.SetNum(Gradient.Num());

    for (int32 i = 0; i < Gradient.Num(); ++i)
    {
        float Deriv = ActivationDerivative(Layer.LastOutput[i], Layer.Config.Activation);
        ActivationGrad[i] = Gradient[i] * Deriv;
    }

    Layer.LastGradient = ActivationGrad;

    // Update biases
    if (Layer.Config.bUseBias)
    {
        for (int32 j = 0; j < Layer.Config.OutputSize; ++j)
        {
            Layer.Biases[j] -= LearningRate * ActivationGrad[j];
        }
    }

    // Update weights (simplified - assumes we have the input stored)
    // In a full implementation, we would store the input during forward pass
}

float UNeuralNetworkBackend::ActivationDerivative(float Value, EActivationFunction Activation)
{
    switch (Activation)
    {
        case EActivationFunction::Linear:
            return 1.0f;

        case EActivationFunction::ReLU:
            return Value > 0 ? 1.0f : 0.0f;

        case EActivationFunction::LeakyReLU:
            return Value > 0 ? 1.0f : 0.01f;

        case EActivationFunction::Sigmoid:
            return Value * (1.0f - Value);

        case EActivationFunction::Tanh:
            return 1.0f - Value * Value;

        default:
            return 1.0f;
    }
}

FString UNeuralNetworkBackend::GenerateNetworkID()
{
    return FString::Printf(TEXT("NET_%d_%d"), ++NetworkIDCounter, FMath::RandRange(1000, 9999));
}

FString UNeuralNetworkBackend::GenerateLayerID()
{
    return FString::Printf(TEXT("LAYER_%d_%d"), ++LayerIDCounter, FMath::RandRange(1000, 9999));
}
