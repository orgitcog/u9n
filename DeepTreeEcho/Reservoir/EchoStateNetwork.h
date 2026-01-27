#pragma once

/**
 * @file EchoStateNetwork.h
 * @brief Feature F1.2.1: Echo State Network Core Implementation
 * 
 * Implements core ESN functionality:
 * - Sparse reservoir matrix generation
 * - Spectral radius normalization
 * - Leaky integrator dynamics
 * - Input/output scaling
 * - State harvesting mechanism
 * 
 * @author Deep Tree Echo Team
 * @date January 2026
 * @version 1.0.0-alpha
 */

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EchoStateNetwork.generated.h"

/**
 * @brief ESN Configuration Parameters
 */
USTRUCT(BlueprintType)
struct DEEPTREEECHO_API FESNConfig
{
    GENERATED_BODY()

    /** Number of reservoir units */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESN|Architecture", meta = (ClampMin = "10", ClampMax = "10000"))
    int32 ReservoirSize = 100;

    /** Number of input dimensions */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESN|Architecture", meta = (ClampMin = "1"))
    int32 InputDim = 10;

    /** Number of output dimensions */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESN|Architecture", meta = (ClampMin = "1"))
    int32 OutputDim = 1;

    /** Spectral radius of reservoir weight matrix (controls memory/echo) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESN|Dynamics", meta = (ClampMin = "0.0", ClampMax = "2.0"))
    float SpectralRadius = 0.9f;

    /** Leak rate for leaky integrator neurons (1.0 = no leak, 0.0 = full reset) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESN|Dynamics", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float LeakRate = 0.3f;

    /** Sparsity of reservoir connections (0.1 = 10% connected) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESN|Sparsity", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ReservoirSparsity = 0.1f;

    /** Sparsity of input connections */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESN|Sparsity", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float InputSparsity = 0.5f;

    /** Input scaling factor */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESN|Scaling")
    float InputScaling = 1.0f;

    /** Output scaling factor */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESN|Scaling")
    float OutputScaling = 1.0f;

    /** Bias scaling factor */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESN|Scaling")
    float BiasScaling = 0.1f;

    /** Enable input bias term */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESN|Configuration")
    bool bEnableBias = true;

    /** Noise level for reservoir states */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESN|Noise", meta = (ClampMin = "0.0"))
    float NoiseLevel = 0.0f;

    /** Random seed (0 = random) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESN|Configuration")
    int32 RandomSeed = 0;

    /** Activation function type */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESN|Configuration")
    FString ActivationFunction = TEXT("tanh");

    /** Ridge regression regularization parameter */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESN|Training", meta = (ClampMin = "0.0"))
    float RegularizationLambda = 1e-6f;
};

/**
 * @brief ESN State Information
 */
USTRUCT(BlueprintType)
struct DEEPTREEECHO_API FESNState
{
    GENERATED_BODY()

    /** Current reservoir activation state */
    UPROPERTY(BlueprintReadOnly, Category = "ESN|State")
    TArray<float> ReservoirState;

    /** Last input vector */
    UPROPERTY(BlueprintReadOnly, Category = "ESN|State")
    TArray<float> LastInput;

    /** Last output vector */
    UPROPERTY(BlueprintReadOnly, Category = "ESN|State")
    TArray<float> LastOutput;

    /** Current timestep */
    UPROPERTY(BlueprintReadOnly, Category = "ESN|State")
    int32 Timestep = 0;

    /** Is ESN initialized */
    UPROPERTY(BlueprintReadOnly, Category = "ESN|State")
    bool bInitialized = false;

    /** Actual spectral radius (after normalization) */
    UPROPERTY(BlueprintReadOnly, Category = "ESN|State")
    float ActualSpectralRadius = 0.0f;
};

/**
 * @brief Sparse Matrix Representation (CSR format)
 */
USTRUCT(BlueprintType)
struct DEEPTREEECHO_API FSparseMatrix
{
    GENERATED_BODY()

    /** Non-zero values */
    UPROPERTY()
    TArray<float> Values;

    /** Column indices for each value */
    UPROPERTY()
    TArray<int32> ColIndices;

    /** Row pointer array (CSR format) */
    UPROPERTY()
    TArray<int32> RowPointers;

    /** Number of rows */
    UPROPERTY()
    int32 Rows = 0;

    /** Number of columns */
    UPROPERTY()
    int32 Cols = 0;

    /** Number of non-zero entries */
    UPROPERTY()
    int32 NonZeros = 0;
};

/**
 * @brief Echo State Network Component
 * 
 * Core ESN implementation for Deep Tree Echo cognitive architecture.
 * Implements Feature F1.2.1 requirements:
 * - Sparse reservoir matrix generation
 * - Spectral radius normalization
 * - Leaky integrator dynamics
 * - Input/output scaling
 * - State harvesting
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UEchoStateNetwork : public UActorComponent
{
    GENERATED_BODY()

public:
    UEchoStateNetwork();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** ESN configuration parameters */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESN|Config")
    FESNConfig Config;

    /** Enable ESN processing */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESN|Config")
    bool bEnableProcessing = true;

    /** Enable automatic state updates on tick */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ESN|Config")
    bool bAutoUpdate = false;

    // ========================================
    // STATE
    // ========================================

    /** Current ESN state */
    UPROPERTY(BlueprintReadOnly, Category = "ESN|State")
    FESNState State;

    // ========================================
    // PUBLIC API - INITIALIZATION
    // ========================================

    /**
     * @brief Initialize ESN with current configuration
     * 
     * Generates:
     * - Sparse reservoir weight matrix (W) with spectral radius normalization
     * - Sparse input weight matrix (Win) with input scaling
     * - Bias vector (if enabled)
     */
    UFUNCTION(BlueprintCallable, Category = "ESN|Initialize")
    bool Initialize();

    /**
     * @brief Initialize with custom configuration
     */
    UFUNCTION(BlueprintCallable, Category = "ESN|Initialize")
    bool InitializeWithConfig(const FESNConfig& NewConfig);

    /**
     * @brief Reset ESN state to zero
     */
    UFUNCTION(BlueprintCallable, Category = "ESN|Initialize")
    void ResetState();

    // ========================================
    // PUBLIC API - FORWARD PASS
    // ========================================

    /**
     * @brief Process input through ESN and update internal state
     * 
     * Implements leaky integrator dynamics:
     * x(t) = (1-α)*x(t-1) + α*f(Win*u(t) + W*x(t-1) + bias)
     * 
     * @param Input Input vector
     * @return New reservoir state (for state harvesting)
     */
    UFUNCTION(BlueprintCallable, Category = "ESN|Forward")
    TArray<float> ProcessInput(const TArray<float>& Input);

    /**
     * @brief Process multiple inputs in sequence
     * 
     * @param Inputs Array of input vectors
     * @return Array of reservoir states (one per input)
     */
    UFUNCTION(BlueprintCallable, Category = "ESN|Forward")
    TArray<TArray<float>> ProcessSequence(const TArray<TArray<float>>& Inputs);

    /**
     * @brief Get current reservoir state (state harvesting)
     */
    UFUNCTION(BlueprintCallable, Category = "ESN|Forward")
    TArray<float> GetReservoirState() const;

    /**
     * @brief Compute output using trained output weights
     * 
     * y(t) = Wout * x(t)
     */
    UFUNCTION(BlueprintCallable, Category = "ESN|Forward")
    TArray<float> ComputeOutput();

    // ========================================
    // PUBLIC API - TRAINING
    // ========================================

    /**
     * @brief Train output weights using ridge regression
     * 
     * Collects reservoir states for all training inputs,
     * then solves: Wout = (X^T*X + λI)^-1 * X^T * Y
     * 
     * @param TrainingInputs Input sequences
     * @param TrainingTargets Target outputs
     * @param WashoutSteps Number of initial steps to discard
     * @return Training error (NRMSE)
     */
    UFUNCTION(BlueprintCallable, Category = "ESN|Training")
    float TrainOutputWeights(const TArray<TArray<float>>& TrainingInputs, 
                            const TArray<TArray<float>>& TrainingTargets,
                            int32 WashoutSteps = 100);

    // ========================================
    // PUBLIC API - INTROSPECTION
    // ========================================

    /**
     * @brief Get current spectral radius of reservoir
     */
    UFUNCTION(BlueprintCallable, Category = "ESN|Introspection")
    float GetSpectralRadius() const;

    /**
     * @brief Get reservoir connectivity (percentage of non-zero weights)
     */
    UFUNCTION(BlueprintCallable, Category = "ESN|Introspection")
    float GetReservoirConnectivity() const;

    /**
     * @brief Get average activation level
     */
    UFUNCTION(BlueprintCallable, Category = "ESN|Introspection")
    float GetAverageActivation() const;

    /**
     * @brief Get ESN information as string
     */
    UFUNCTION(BlueprintCallable, Category = "ESN|Introspection")
    FString GetESNInfo() const;

protected:
    virtual void BeginPlay() override;

private:
    // ========================================
    // INTERNAL STATE - WEIGHT MATRICES
    // ========================================

    /** Reservoir weight matrix W (sparse, NxN) */
    FSparseMatrix ReservoirWeights;

    /** Input weight matrix Win (sparse, NxM) */
    FSparseMatrix InputWeights;

    /** Output weight matrix Wout (dense, KxN) - trained */
    TArray<float> OutputWeights;

    /** Bias vector (Nx1) */
    TArray<float> BiasVector;

    // ========================================
    // INTERNAL METHODS - MATRIX GENERATION
    // ========================================

    /**
     * @brief Generate sparse random matrix with specified sparsity
     * 
     * Uses normal distribution for weights, uniform for sparsity
     */
    FSparseMatrix GenerateSparseRandomMatrix(int32 Rows, int32 Cols, 
                                              float Sparsity, int32 Seed);

    /**
     * @brief Compute spectral radius of matrix using power iteration
     * 
     * Approximates largest eigenvalue magnitude
     */
    float ComputeSpectralRadius(const FSparseMatrix& Matrix, 
                                int32 MaxIterations = 100,
                                float Tolerance = 1e-6f);

    /**
     * @brief Scale matrix to target spectral radius
     */
    void ScaleToSpectralRadius(FSparseMatrix& Matrix, float TargetRadius);

    // ========================================
    // INTERNAL METHODS - MATRIX OPERATIONS
    // ========================================

    /**
     * @brief Sparse matrix-vector multiplication
     */
    TArray<float> SparseMatrixVectorMultiply(const FSparseMatrix& Matrix, 
                                             const TArray<float>& Vector) const;

    /**
     * @brief Dense matrix-vector multiplication
     */
    TArray<float> DenseMatrixVectorMultiply(const TArray<float>& Matrix,
                                            const TArray<float>& Vector,
                                            int32 Rows, int32 Cols) const;

    // ========================================
    // INTERNAL METHODS - ACTIVATION FUNCTIONS
    // ========================================

    /**
     * @brief Apply activation function element-wise
     */
    TArray<float> ApplyActivation(const TArray<float>& Vector, 
                                  const FString& FunctionName) const;

    /**
     * @brief Tanh activation
     */
    float Tanh(float x) const;

    /**
     * @brief Sigmoid activation
     */
    float Sigmoid(float x) const;

    /**
     * @brief ReLU activation
     */
    float ReLU(float x) const;

    // ========================================
    // INTERNAL METHODS - UTILITIES
    // ========================================

    /**
     * @brief Add two vectors element-wise
     */
    TArray<float> VectorAdd(const TArray<float>& A, const TArray<float>& B) const;

    /**
     * @brief Scalar multiplication of vector
     */
    TArray<float> VectorScale(const TArray<float>& V, float Scalar) const;

    /**
     * @brief Compute Euclidean norm of vector
     */
    float VectorNorm(const TArray<float>& V) const;

    /**
     * @brief Generate random seed from configuration or system
     */
    int32 GenerateSeed() const;

    /**
     * @brief Validate input dimensions
     */
    bool ValidateInput(const TArray<float>& Input) const;
};
