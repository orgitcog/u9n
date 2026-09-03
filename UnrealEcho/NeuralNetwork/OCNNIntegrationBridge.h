#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OCNNIntegrationBridge.generated.h"

/**
 * OCNN (Octree Convolutional Neural Network) Integration Bridge
 * Provides efficient spatial processing for avatar cognitive systems
 * 
 * Based on: Torch nn package with octree spatial convolutions
 * Purpose: Enable hierarchical spatial reasoning and pattern recognition
 */

/**
 * OCNN Layer Types
 */
UENUM(BlueprintType)
enum class EOCNNLayerType : uint8
{
    /** Spatial convolution for 3D data */
    SpatialConvolution UMETA(DisplayName = "Spatial Convolution"),
    
    /** Volumetric convolution for dense 3D data */
    VolumetricConvolution UMETA(DisplayName = "Volumetric Convolution"),
    
    /** Temporal convolution for time-series data */
    TemporalConvolution UMETA(DisplayName = "Temporal Convolution"),
    
    /** Max pooling for spatial reduction */
    MaxPooling UMETA(DisplayName = "Max Pooling"),
    
    /** Linear fully-connected layer */
    Linear UMETA(DisplayName = "Linear"),
    
    /** ReLU activation */
    ReLU UMETA(DisplayName = "ReLU"),
    
    /** Tanh activation */
    Tanh UMETA(DisplayName = "Tanh"),
    
    /** Sigmoid activation */
    Sigmoid UMETA(DisplayName = "Sigmoid"),
    
    /** Batch normalization */
    BatchNormalization UMETA(DisplayName = "Batch Normalization"),
    
    /** Dropout regularization */
    Dropout UMETA(DisplayName = "Dropout")
};

/**
 * OCNN Tensor Data
 * Represents multi-dimensional neural network data
 */
USTRUCT(BlueprintType)
struct FOCNNTensor
{
    GENERATED_BODY()

    /** Tensor dimensions (e.g., [batch, channels, depth, height, width]) */
    UPROPERTY(BlueprintReadWrite)
    TArray<int32> Dimensions;

    /** Flattened tensor data */
    UPROPERTY(BlueprintReadWrite)
    TArray<float> Data;

    /** Tensor name for debugging */
    UPROPERTY(BlueprintReadWrite)
    FString Name;

    FOCNNTensor()
        : Name(TEXT("Unnamed"))
    {
    }

    /** Get total number of elements */
    int32 GetNumElements() const
    {
        int32 Total = 1;
        for (int32 Dim : Dimensions)
        {
            Total *= Dim;
        }
        return Total;
    }

    /** Initialize tensor with dimensions */
    void Initialize(const TArray<int32>& InDimensions)
    {
        Dimensions = InDimensions;
        Data.SetNum(GetNumElements());
        // Initialize with zeros
        for (int32 i = 0; i < Data.Num(); ++i)
        {
            Data[i] = 0.0f;
        }
    }

    /** Get value at indices */
    float GetValue(const TArray<int32>& Indices) const
    {
        int32 FlatIndex = ComputeFlatIndex(Indices);
        if (FlatIndex >= 0 && FlatIndex < Data.Num())
        {
            return Data[FlatIndex];
        }
        return 0.0f;
    }

    /** Set value at indices */
    void SetValue(const TArray<int32>& Indices, float Value)
    {
        int32 FlatIndex = ComputeFlatIndex(Indices);
        if (FlatIndex >= 0 && FlatIndex < Data.Num())
        {
            Data[FlatIndex] = Value;
        }
    }

private:
    /** Compute flat index from multi-dimensional indices */
    int32 ComputeFlatIndex(const TArray<int32>& Indices) const
    {
        if (Indices.Num() != Dimensions.Num())
        {
            return -1;
        }

        int32 FlatIndex = 0;
        int32 Multiplier = 1;

        for (int32 i = Dimensions.Num() - 1; i >= 0; --i)
        {
            if (Indices[i] < 0 || Indices[i] >= Dimensions[i])
            {
                return -1;
            }
            FlatIndex += Indices[i] * Multiplier;
            Multiplier *= Dimensions[i];
        }

        return FlatIndex;
    }
};

/**
 * OCNN Network Configuration
 */
USTRUCT(BlueprintType)
struct FOCNNNetworkConfig
{
    GENERATED_BODY()

    /** Network name */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString NetworkName = TEXT("OCNN_Network");

    /** Input dimensions [channels, depth, height, width] */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<int32> InputDimensions;

    /** Number of output classes/features */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 OutputSize = 10;

    /** Learning rate */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float LearningRate = 0.001f;

    /** Batch size */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 BatchSize = 32;

    /** Use GPU acceleration (if available) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bUseGPU = true;
};

/**
 * OCNN Integration Bridge Component
 * Connects UnrealEngine avatar system with OCNN neural processing
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UNREALECHO_API UOCNNIntegrationBridge : public UActorComponent
{
    GENERATED_BODY()

public:
    UOCNNIntegrationBridge();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Network configuration */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OCNN|Config")
    FOCNNNetworkConfig NetworkConfig;

    /** Enable real-time inference */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OCNN|Config")
    bool bEnableRealTimeInference = true;

    /** Inference frequency (Hz) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OCNN|Config", meta = (ClampMin = "1", ClampMax = "60"))
    int32 InferenceFrequency = 10;

    // ========================================
    // PUBLIC API
    // ========================================

    /** Initialize OCNN network */
    UFUNCTION(BlueprintCallable, Category = "OCNN")
    bool InitializeNetwork(const FOCNNNetworkConfig& Config);

    /** Forward pass through network */
    UFUNCTION(BlueprintCallable, Category = "OCNN")
    FOCNNTensor Forward(const FOCNNTensor& Input);

    /** Process spatial data (3D environment perception) */
    UFUNCTION(BlueprintCallable, Category = "OCNN")
    FOCNNTensor ProcessSpatialData(const TArray<FVector>& SpatialPoints, const TArray<float>& Features);

    /** Process temporal sequence (action history, emotional trajectory) */
    UFUNCTION(BlueprintCallable, Category = "OCNN")
    FOCNNTensor ProcessTemporalSequence(const TArray<FOCNNTensor>& Sequence);

    /** Extract features from volumetric data */
    UFUNCTION(BlueprintCallable, Category = "OCNN")
    FOCNNTensor ExtractVolumetricFeatures(const FOCNNTensor& VolumetricInput);

    /** Train network with labeled data */
    UFUNCTION(BlueprintCallable, Category = "OCNN")
    float TrainStep(const FOCNNTensor& Input, const FOCNNTensor& Target);

    /** Save network weights */
    UFUNCTION(BlueprintCallable, Category = "OCNN")
    bool SaveNetwork(const FString& FilePath);

    /** Load network weights */
    UFUNCTION(BlueprintCallable, Category = "OCNN")
    bool LoadNetwork(const FString& FilePath);

    /** Get network statistics */
    UFUNCTION(BlueprintCallable, Category = "OCNN")
    FString GetNetworkStats() const;

protected:
    virtual void BeginPlay() override;

private:
    // ========================================
    // INTERNAL STATE
    // ========================================

    /** Is network initialized? */
    bool bIsInitialized = false;

    /** Inference timer accumulator */
    float InferenceAccumulator = 0.0f;

    /** Last inference result */
    FOCNNTensor LastInferenceResult;

    /** Network layer stack (simplified representation) */
    TArray<EOCNNLayerType> LayerStack;

    /** Network weights (simplified representation) */
    TMap<FString, FOCNNTensor> NetworkWeights;

    // ========================================
    // INTERNAL METHODS
    // ========================================

    /** Build default network architecture */
    void BuildDefaultArchitecture();

    /** Apply convolution operation */
    FOCNNTensor ApplyConvolution(const FOCNNTensor& Input, const FOCNNTensor& Kernel);

    /** Apply activation function */
    FOCNNTensor ApplyActivation(const FOCNNTensor& Input, EOCNNLayerType ActivationType);

    /** Apply pooling operation */
    FOCNNTensor ApplyPooling(const FOCNNTensor& Input, int32 PoolSize);

    /** Convert spatial points to tensor */
    FOCNNTensor SpatialPointsToTensor(const TArray<FVector>& Points, const TArray<float>& Features);

    /** Compute loss (MSE for now) */
    float ComputeLoss(const FOCNNTensor& Predicted, const FOCNNTensor& Target);
};
