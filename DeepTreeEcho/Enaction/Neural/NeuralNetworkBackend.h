// NeuralNetworkBackend.h
// Neural Network Backend for Deep Tree Echo
// Provides neural computation primitives for cognitive processing

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NeuralNetworkBackend.generated.h"

/**
 * Activation function type
 */
UENUM(BlueprintType)
enum class EActivationFunction : uint8
{
    Linear      UMETA(DisplayName = "Linear"),
    ReLU        UMETA(DisplayName = "ReLU"),
    Sigmoid     UMETA(DisplayName = "Sigmoid"),
    Tanh        UMETA(DisplayName = "Tanh"),
    Softmax     UMETA(DisplayName = "Softmax"),
    LeakyReLU   UMETA(DisplayName = "Leaky ReLU")
};

/**
 * Neural layer configuration
 */
USTRUCT(BlueprintType)
struct FNeuralLayerConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 InputSize = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 OutputSize = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EActivationFunction Activation = EActivationFunction::ReLU;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float DropoutRate = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bUseBias = true;
};

/**
 * Neural layer state
 */
USTRUCT(BlueprintType)
struct FNeuralLayer
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString LayerID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FNeuralLayerConfig Config;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<float> Weights;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<float> Biases;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<float> LastOutput;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<float> LastGradient;
};

/**
 * Network configuration
 */
USTRUCT(BlueprintType)
struct FNetworkConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString NetworkID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString NetworkName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FNeuralLayerConfig> LayerConfigs;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float LearningRate = 0.001f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Momentum = 0.9f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float WeightDecay = 0.0001f;
};

/**
 * Training result
 */
USTRUCT(BlueprintType)
struct FTrainingResult
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Loss = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Accuracy = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Epoch = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 BatchSize = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float TrainingTime = 0.0f;
};

/**
 * Delegate declarations
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNetworkCreated, const FString&, NetworkID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTrainingComplete, const FTrainingResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInferenceComplete, const FString&, NetworkID, const TArray<float>&, Output);

/**
 * Neural Network Backend Component
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UNeuralNetworkBackend : public UActorComponent
{
    GENERATED_BODY()

public:
    UNeuralNetworkBackend();

protected:
    virtual void BeginPlay() override;

public:
    // ========================================
    // EVENTS
    // ========================================

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnNetworkCreated OnNetworkCreated;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnTrainingComplete OnTrainingComplete;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnInferenceComplete OnInferenceComplete;

    // ========================================
    // NETWORK MANAGEMENT
    // ========================================

    /** Create a neural network */
    UFUNCTION(BlueprintCallable, Category = "Network")
    FString CreateNetwork(const FString& Name, const TArray<int32>& LayerSizes, 
                          EActivationFunction Activation);

    /** Delete a network */
    UFUNCTION(BlueprintCallable, Category = "Network")
    void DeleteNetwork(const FString& NetworkID);

    /** Get network info */
    UFUNCTION(BlueprintPure, Category = "Network")
    FNetworkConfig GetNetworkConfig(const FString& NetworkID) const;

    /** List all networks */
    UFUNCTION(BlueprintPure, Category = "Network")
    TArray<FString> GetAllNetworkIDs() const;

    // ========================================
    // INFERENCE
    // ========================================

    /** Forward pass through network */
    UFUNCTION(BlueprintCallable, Category = "Inference")
    TArray<float> Forward(const FString& NetworkID, const TArray<float>& Input);

    /** Batch forward pass */
    UFUNCTION(BlueprintCallable, Category = "Inference")
    TArray<TArray<float>> BatchForward(const FString& NetworkID, const TArray<TArray<float>>& Inputs);

    // ========================================
    // TRAINING
    // ========================================

    /** Train network on single sample */
    UFUNCTION(BlueprintCallable, Category = "Training")
    float TrainSample(const FString& NetworkID, const TArray<float>& Input, 
                      const TArray<float>& Target);

    /** Train network on batch */
    UFUNCTION(BlueprintCallable, Category = "Training")
    FTrainingResult TrainBatch(const FString& NetworkID, const TArray<TArray<float>>& Inputs, 
                                const TArray<TArray<float>>& Targets);

    /** Set learning rate */
    UFUNCTION(BlueprintCallable, Category = "Training")
    void SetLearningRate(const FString& NetworkID, float Rate);

    // ========================================
    // UTILITIES
    // ========================================

    /** Apply activation function */
    UFUNCTION(BlueprintPure, Category = "Utilities")
    static float ApplyActivation(float Value, EActivationFunction Activation);

    /** Apply activation to array */
    UFUNCTION(BlueprintPure, Category = "Utilities")
    static TArray<float> ApplyActivationArray(const TArray<float>& Values, EActivationFunction Activation);

    /** Compute softmax */
    UFUNCTION(BlueprintPure, Category = "Utilities")
    static TArray<float> Softmax(const TArray<float>& Values);

    /** Compute mean squared error */
    UFUNCTION(BlueprintPure, Category = "Utilities")
    static float MeanSquaredError(const TArray<float>& Predicted, const TArray<float>& Target);

    /** Compute cross entropy loss */
    UFUNCTION(BlueprintPure, Category = "Utilities")
    static float CrossEntropyLoss(const TArray<float>& Predicted, const TArray<float>& Target);

protected:
    // Internal state
    TMap<FString, FNetworkConfig> NetworkConfigs;
    TMap<FString, TArray<FNeuralLayer>> NetworkLayers;

    int32 NetworkIDCounter = 0;
    int32 LayerIDCounter = 0;

    // Internal methods
    void InitializeLayer(FNeuralLayer& Layer, const FNeuralLayerConfig& Config);
    TArray<float> ForwardLayer(FNeuralLayer& Layer, const TArray<float>& Input);
    void BackwardLayer(FNeuralLayer& Layer, const TArray<float>& Gradient, float LearningRate);

    FString GenerateNetworkID();
    FString GenerateLayerID();

    static float ActivationDerivative(float Value, EActivationFunction Activation);
};
