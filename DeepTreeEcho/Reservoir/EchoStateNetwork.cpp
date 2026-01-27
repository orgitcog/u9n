/**
 * @file EchoStateNetwork.cpp
 * @brief Feature F1.2.1: Echo State Network Core Implementation
 * 
 * Implements core ESN functionality for Deep Tree Echo cognitive architecture.
 * 
 * @author Deep Tree Echo Team
 * @date January 2026
 */

#include "EchoStateNetwork.h"
#include "Math/UnrealMathUtility.h"
#include <random>

UEchoStateNetwork::UEchoStateNetwork()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.033f; // ~30 Hz
}

void UEchoStateNetwork::BeginPlay()
{
    Super::BeginPlay();
    
    // Auto-initialize if enabled
    if (bAutoUpdate && !State.bInitialized)
    {
        Initialize();
    }
}

void UEchoStateNetwork::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    
    // Placeholder for automatic processing
    // User can override this or use explicit ProcessInput calls
}

// ========================================
// INITIALIZATION
// ========================================

bool UEchoStateNetwork::Initialize()
{
    return InitializeWithConfig(Config);
}

bool UEchoStateNetwork::InitializeWithConfig(const FESNConfig& NewConfig)
{
    Config = NewConfig;
    
    // Validate configuration
    if (Config.ReservoirSize < 10 || Config.InputDim < 1 || Config.OutputDim < 1)
    {
        UE_LOG(LogTemp, Error, TEXT("UEchoStateNetwork: Invalid configuration parameters"));
        return false;
    }
    
    int32 Seed = GenerateSeed();
    
    // Generate sparse reservoir weight matrix W (NxN)
    ReservoirWeights = GenerateSparseRandomMatrix(
        Config.ReservoirSize, 
        Config.ReservoirSize, 
        Config.ReservoirSparsity, 
        Seed
    );
    
    // Scale to target spectral radius
    ScaleToSpectralRadius(ReservoirWeights, Config.SpectralRadius);
    
    // Compute actual spectral radius for verification
    State.ActualSpectralRadius = ComputeSpectralRadius(ReservoirWeights);
    
    // Generate sparse input weight matrix Win (NxM)
    int32 InputCols = Config.bEnableBias ? Config.InputDim + 1 : Config.InputDim;
    InputWeights = GenerateSparseRandomMatrix(
        Config.ReservoirSize, 
        InputCols, 
        Config.InputSparsity, 
        Seed + 1
    );
    
    // Scale input weights
    for (float& Value : InputWeights.Values)
    {
        Value *= Config.InputScaling;
    }
    
    // Initialize bias vector
    if (Config.bEnableBias)
    {
        BiasVector.SetNum(Config.ReservoirSize);
        std::mt19937 gen(Seed + 2);
        std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
        for (int32 i = 0; i < Config.ReservoirSize; ++i)
        {
            BiasVector[i] = dist(gen) * Config.BiasScaling;
        }
    }
    
    // Initialize output weights (will be trained)
    OutputWeights.SetNum(Config.OutputDim * Config.ReservoirSize);
    for (float& W : OutputWeights)
    {
        W = 0.0f;
    }
    
    // Initialize state
    ResetState();
    State.bInitialized = true;
    
    UE_LOG(LogTemp, Display, TEXT("UEchoStateNetwork: Initialized with %d units, spectral radius %.3f (target %.3f)"),
        Config.ReservoirSize, State.ActualSpectralRadius, Config.SpectralRadius);
    
    return true;
}

void UEchoStateNetwork::ResetState()
{
    State.ReservoirState.SetNum(Config.ReservoirSize);
    for (float& x : State.ReservoirState)
    {
        x = 0.0f;
    }
    
    State.LastInput.Empty();
    State.LastOutput.Empty();
    State.Timestep = 0;
}

// ========================================
// FORWARD PASS
// ========================================

TArray<float> UEchoStateNetwork::ProcessInput(const TArray<float>& Input)
{
    if (!State.bInitialized)
    {
        UE_LOG(LogTemp, Warning, TEXT("UEchoStateNetwork: Not initialized, call Initialize() first"));
        return TArray<float>();
    }
    
    if (!ValidateInput(Input))
    {
        UE_LOG(LogTemp, Warning, TEXT("UEchoStateNetwork: Invalid input dimensions"));
        return State.ReservoirState;
    }
    
    // Prepare input with optional bias
    TArray<float> InputWithBias = Input;
    if (Config.bEnableBias)
    {
        InputWithBias.Add(1.0f); // Bias term
    }
    
    // Compute: Win * u(t)
    TArray<float> InputContribution = SparseMatrixVectorMultiply(InputWeights, InputWithBias);
    
    // Compute: W * x(t-1)
    TArray<float> RecurrentContribution = SparseMatrixVectorMultiply(ReservoirWeights, State.ReservoirState);
    
    // Combine: Win*u(t) + W*x(t-1) + bias
    TArray<float> PreActivation = VectorAdd(InputContribution, RecurrentContribution);
    if (Config.bEnableBias && BiasVector.Num() > 0)
    {
        PreActivation = VectorAdd(PreActivation, BiasVector);
    }
    
    // Apply activation function: f(Win*u(t) + W*x(t-1) + bias)
    TArray<float> Activation = ApplyActivation(PreActivation, Config.ActivationFunction);
    
    // Add noise if configured
    if (Config.NoiseLevel > 0.0f)
    {
        for (float& x : Activation)
        {
            x += FMath::FRandRange(-Config.NoiseLevel, Config.NoiseLevel);
        }
    }
    
    // Leaky integrator update: x(t) = (1-α)*x(t-1) + α*f(...)
    for (int32 i = 0; i < Config.ReservoirSize; ++i)
    {
        State.ReservoirState[i] = (1.0f - Config.LeakRate) * State.ReservoirState[i] + 
                                   Config.LeakRate * Activation[i];
    }
    
    State.LastInput = Input;
    State.Timestep++;
    
    return State.ReservoirState;
}

TArray<TArray<float>> UEchoStateNetwork::ProcessSequence(const TArray<TArray<float>>& Inputs)
{
    TArray<TArray<float>> States;
    States.Reserve(Inputs.Num());
    
    for (const TArray<float>& Input : Inputs)
    {
        TArray<float> NewState = ProcessInput(Input);
        States.Add(NewState);
    }
    
    return States;
}

TArray<float> UEchoStateNetwork::GetReservoirState() const
{
    return State.ReservoirState;
}

TArray<float> UEchoStateNetwork::ComputeOutput()
{
    if (!State.bInitialized || OutputWeights.Num() == 0)
    {
        return TArray<float>();
    }
    
    // y(t) = Wout * x(t)
    TArray<float> Output = DenseMatrixVectorMultiply(
        OutputWeights, 
        State.ReservoirState,
        Config.OutputDim,
        Config.ReservoirSize
    );
    
    // Apply output scaling
    Output = VectorScale(Output, Config.OutputScaling);
    
    State.LastOutput = Output;
    return Output;
}

// ========================================
// TRAINING
// ========================================

float UEchoStateNetwork::TrainOutputWeights(
    const TArray<TArray<float>>& TrainingInputs,
    const TArray<TArray<float>>& TrainingTargets,
    int32 WashoutSteps)
{
    if (!State.bInitialized)
    {
        UE_LOG(LogTemp, Error, TEXT("UEchoStateNetwork: Cannot train, ESN not initialized"));
        return -1.0f;
    }
    
    if (TrainingInputs.Num() != TrainingTargets.Num() || TrainingInputs.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("UEchoStateNetwork: Training data size mismatch"));
        return -1.0f;
    }
    
    // Reset state before training
    ResetState();
    
    // Collect reservoir states
    TArray<TArray<float>> States;
    States.Reserve(TrainingInputs.Num() - WashoutSteps);
    
    // Run through training inputs
    for (int32 i = 0; i < TrainingInputs.Num(); ++i)
    {
        TArray<float> NewState = ProcessInput(TrainingInputs[i]);
        
        // Skip washout period
        if (i >= WashoutSteps)
        {
            States.Add(NewState);
        }
    }
    
    int32 NumSamples = States.Num();
    if (NumSamples == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("UEchoStateNetwork: No training samples after washout"));
        return -1.0f;
    }
    
    // Build state matrix X (NumSamples x ReservoirSize)
    // Build target matrix Y (NumSamples x OutputDim)
    TArray<float> X;
    TArray<float> Y;
    X.Reserve(NumSamples * Config.ReservoirSize);
    Y.Reserve(NumSamples * Config.OutputDim);
    
    for (int32 i = 0; i < NumSamples; ++i)
    {
        // Add state
        for (float s : States[i])
        {
            X.Add(s);
        }
        
        // Add target
        int32 TargetIdx = i + WashoutSteps;
        if (TargetIdx < TrainingTargets.Num())
        {
            for (float t : TrainingTargets[TargetIdx])
            {
                Y.Add(t);
            }
        }
    }
    
    // Ridge regression: Wout = (X^T*X + λI)^-1 * X^T * Y
    // Note: This is a simplified implementation suitable for small-scale problems
    // For production use with large datasets, consider using:
    // - Iterative solvers (Conjugate Gradient)
    // - QR decomposition
    // - SVD-based pseudoinverse
    // - Online learning algorithms (FORCE, RLS)
    
    // Compute X^T * X
    TArray<float> XTX;
    XTX.SetNum(Config.ReservoirSize * Config.ReservoirSize);
    for (int32 i = 0; i < Config.ReservoirSize; ++i)
    {
        for (int32 j = 0; j < Config.ReservoirSize; ++j)
        {
            float sum = 0.0f;
            for (int32 k = 0; k < NumSamples; ++k)
            {
                sum += X[k * Config.ReservoirSize + i] * X[k * Config.ReservoirSize + j];
            }
            XTX[i * Config.ReservoirSize + j] = sum;
        }
    }
    
    // Add regularization: X^T*X + λI
    for (int32 i = 0; i < Config.ReservoirSize; ++i)
    {
        XTX[i * Config.ReservoirSize + i] += Config.RegularizationLambda;
    }
    
    // Compute X^T * Y
    TArray<float> XTY;
    XTY.SetNum(Config.ReservoirSize * Config.OutputDim);
    for (int32 i = 0; i < Config.ReservoirSize; ++i)
    {
        for (int32 j = 0; j < Config.OutputDim; ++j)
        {
            float sum = 0.0f;
            for (int32 k = 0; k < NumSamples; ++k)
            {
                sum += X[k * Config.ReservoirSize + i] * Y[k * Config.OutputDim + j];
            }
            XTY[i * Config.OutputDim + j] = sum;
        }
    }
    
    // Solve (X^T*X + λI) * Wout = X^T * Y using simple diagonal approximation
    // For better results, use a proper linear solver (Cholesky, LU, or iterative methods)
    // This approximation works reasonably well when λ is relatively large
    OutputWeights.SetNum(Config.ReservoirSize * Config.OutputDim);
    for (int32 i = 0; i < Config.ReservoirSize; ++i)
    {
        float diagVal = XTX[i * Config.ReservoirSize + i];
        if (FMath::Abs(diagVal) > 1e-10f)
        {
            for (int32 j = 0; j < Config.OutputDim; ++j)
            {
                OutputWeights[i * Config.OutputDim + j] = XTY[i * Config.OutputDim + j] / diagVal;
            }
        }
        else
        {
            for (int32 j = 0; j < Config.OutputDim; ++j)
            {
                OutputWeights[i * Config.OutputDim + j] = 0.0f;
            }
        }
    }
    
    // Compute training error (NRMSE)
    ResetState();
    float totalError = 0.0f;
    int32 errorSamples = 0;
    
    for (int32 i = 0; i < TrainingInputs.Num(); ++i)
    {
        ProcessInput(TrainingInputs[i]);
        
        if (i >= WashoutSteps && i < TrainingTargets.Num())
        {
            TArray<float> Output = ComputeOutput();
            const TArray<float>& Target = TrainingTargets[i];
            
            float error = 0.0f;
            for (int32 j = 0; j < FMath::Min(Output.Num(), Target.Num()); ++j)
            {
                float diff = Output[j] - Target[j];
                error += diff * diff;
            }
            totalError += FMath::Sqrt(error);
            errorSamples++;
        }
    }
    
    float NRMSE = errorSamples > 0 ? totalError / errorSamples : 0.0f;
    
    UE_LOG(LogTemp, Display, TEXT("UEchoStateNetwork: Training complete, NRMSE = %.6f"), NRMSE);
    
    return NRMSE;
}

// ========================================
// INTROSPECTION
// ========================================

float UEchoStateNetwork::GetSpectralRadius() const
{
    return State.ActualSpectralRadius;
}

float UEchoStateNetwork::GetReservoirConnectivity() const
{
    if (ReservoirWeights.NonZeros == 0 || ReservoirWeights.Rows == 0 || ReservoirWeights.Cols == 0)
    {
        return 0.0f;
    }
    
    int32 TotalPossible = ReservoirWeights.Rows * ReservoirWeights.Cols;
    return static_cast<float>(ReservoirWeights.NonZeros) / static_cast<float>(TotalPossible);
}

float UEchoStateNetwork::GetAverageActivation() const
{
    if (State.ReservoirState.Num() == 0)
    {
        return 0.0f;
    }
    
    float sum = 0.0f;
    for (float x : State.ReservoirState)
    {
        sum += FMath::Abs(x);
    }
    
    return sum / State.ReservoirState.Num();
}

FString UEchoStateNetwork::GetESNInfo() const
{
    return FString::Printf(
        TEXT("ESN Info:\n")
        TEXT("  Units: %d\n")
        TEXT("  Input Dim: %d\n")
        TEXT("  Output Dim: %d\n")
        TEXT("  Spectral Radius: %.3f (target %.3f)\n")
        TEXT("  Leak Rate: %.3f\n")
        TEXT("  Connectivity: %.2f%%\n")
        TEXT("  Avg Activation: %.4f\n")
        TEXT("  Timestep: %d\n")
        TEXT("  Initialized: %s"),
        Config.ReservoirSize,
        Config.InputDim,
        Config.OutputDim,
        State.ActualSpectralRadius,
        Config.SpectralRadius,
        Config.LeakRate,
        GetReservoirConnectivity() * 100.0f,
        GetAverageActivation(),
        State.Timestep,
        State.bInitialized ? TEXT("Yes") : TEXT("No")
    );
}

// ========================================
// MATRIX GENERATION
// ========================================

FSparseMatrix UEchoStateNetwork::GenerateSparseRandomMatrix(
    int32 Rows, int32 Cols, float Sparsity, int32 Seed)
{
    FSparseMatrix Matrix;
    Matrix.Rows = Rows;
    Matrix.Cols = Cols;
    
    std::mt19937 gen(Seed);
    std::normal_distribution<float> normal(0.0f, 1.0f);
    std::uniform_real_distribution<float> uniform(0.0f, 1.0f);
    
    // Build in COO format first, then convert to CSR
    TArray<int32> RowIndices;
    TArray<int32> ColIndices;
    TArray<float> Values;
    
    for (int32 i = 0; i < Rows; ++i)
    {
        for (int32 j = 0; j < Cols; ++j)
        {
            if (uniform(gen) < Sparsity)
            {
                RowIndices.Add(i);
                ColIndices.Add(j);
                Values.Add(normal(gen));
            }
        }
    }
    
    Matrix.NonZeros = Values.Num();
    
    // Convert to CSR format
    Matrix.RowPointers.SetNum(Rows + 1);
    Matrix.ColIndices = ColIndices;
    Matrix.Values = Values;
    
    // Build row pointers
    for (int32& ptr : Matrix.RowPointers)
    {
        ptr = 0;
    }
    
    for (int32 i = 0; i < RowIndices.Num(); ++i)
    {
        Matrix.RowPointers[RowIndices[i] + 1]++;
    }
    
    for (int32 i = 1; i <= Rows; ++i)
    {
        Matrix.RowPointers[i] += Matrix.RowPointers[i - 1];
    }
    
    return Matrix;
}

float UEchoStateNetwork::ComputeSpectralRadius(
    const FSparseMatrix& Matrix, int32 MaxIterations, float Tolerance)
{
    if (Matrix.Rows != Matrix.Cols)
    {
        UE_LOG(LogTemp, Warning, TEXT("UEchoStateNetwork: Cannot compute spectral radius of non-square matrix"));
        return 0.0f;
    }
    
    int32 N = Matrix.Rows;
    
    // Power iteration method
    TArray<float> v;
    v.SetNum(N);
    
    // Initialize with deterministic random vector using FRandomStream
    FRandomStream RandomStream(GenerateSeed() + 999);
    for (float& x : v)
    {
        x = RandomStream.FRandRange(-1.0f, 1.0f);
    }
    
    // Normalize
    float norm = VectorNorm(v);
    if (norm > 0.0f)
    {
        v = VectorScale(v, 1.0f / norm);
    }
    
    float eigenvalue = 0.0f;
    
    for (int32 iter = 0; iter < MaxIterations; ++iter)
    {
        // v_new = A * v
        TArray<float> v_new = SparseMatrixVectorMultiply(Matrix, v);
        
        // Compute eigenvalue estimate
        float lambda = 0.0f;
        for (int32 i = 0; i < N; ++i)
        {
            lambda += v_new[i] * v[i];
        }
        
        // Normalize v_new
        norm = VectorNorm(v_new);
        if (norm > 0.0f)
        {
            v_new = VectorScale(v_new, 1.0f / norm);
        }
        
        // Check convergence
        if (FMath::Abs(lambda - eigenvalue) < Tolerance)
        {
            eigenvalue = lambda;
            break;
        }
        
        eigenvalue = lambda;
        v = v_new;
    }
    
    return FMath::Abs(eigenvalue);
}

void UEchoStateNetwork::ScaleToSpectralRadius(FSparseMatrix& Matrix, float TargetRadius)
{
    float CurrentRadius = ComputeSpectralRadius(Matrix);
    
    if (CurrentRadius > 1e-10f)
    {
        float Scale = TargetRadius / CurrentRadius;
        for (float& Value : Matrix.Values)
        {
            Value *= Scale;
        }
    }
}

// ========================================
// MATRIX OPERATIONS
// ========================================

TArray<float> UEchoStateNetwork::SparseMatrixVectorMultiply(
    const FSparseMatrix& Matrix, const TArray<float>& Vector) const
{
    if (Vector.Num() != Matrix.Cols)
    {
        UE_LOG(LogTemp, Warning, TEXT("UEchoStateNetwork: Matrix-vector dimension mismatch"));
        return TArray<float>();
    }
    
    TArray<float> Result;
    Result.SetNum(Matrix.Rows);
    
    for (int32 i = 0; i < Matrix.Rows; ++i)
    {
        float sum = 0.0f;
        int32 rowStart = Matrix.RowPointers[i];
        int32 rowEnd = Matrix.RowPointers[i + 1];
        
        for (int32 j = rowStart; j < rowEnd; ++j)
        {
            int32 col = Matrix.ColIndices[j];
            sum += Matrix.Values[j] * Vector[col];
        }
        
        Result[i] = sum;
    }
    
    return Result;
}

TArray<float> UEchoStateNetwork::DenseMatrixVectorMultiply(
    const TArray<float>& Matrix, const TArray<float>& Vector, 
    int32 Rows, int32 Cols) const
{
    if (Vector.Num() != Cols)
    {
        return TArray<float>();
    }
    
    TArray<float> Result;
    Result.SetNum(Rows);
    
    for (int32 i = 0; i < Rows; ++i)
    {
        float sum = 0.0f;
        for (int32 j = 0; j < Cols; ++j)
        {
            sum += Matrix[i * Cols + j] * Vector[j];
        }
        Result[i] = sum;
    }
    
    return Result;
}

// ========================================
// ACTIVATION FUNCTIONS
// ========================================

TArray<float> UEchoStateNetwork::ApplyActivation(
    const TArray<float>& Vector, const FString& FunctionName) const
{
    TArray<float> Result;
    Result.Reserve(Vector.Num());
    
    if (FunctionName == TEXT("tanh"))
    {
        for (float x : Vector)
        {
            Result.Add(Tanh(x));
        }
    }
    else if (FunctionName == TEXT("sigmoid"))
    {
        for (float x : Vector)
        {
            Result.Add(Sigmoid(x));
        }
    }
    else if (FunctionName == TEXT("relu"))
    {
        for (float x : Vector)
        {
            Result.Add(ReLU(x));
        }
    }
    else // Default to tanh
    {
        for (float x : Vector)
        {
            Result.Add(Tanh(x));
        }
    }
    
    return Result;
}

float UEchoStateNetwork::Tanh(float x) const
{
    return FMath::Tanh(x);
}

float UEchoStateNetwork::Sigmoid(float x) const
{
    return 1.0f / (1.0f + FMath::Exp(-x));
}

float UEchoStateNetwork::ReLU(float x) const
{
    return FMath::Max(0.0f, x);
}

// ========================================
// UTILITIES
// ========================================

TArray<float> UEchoStateNetwork::VectorAdd(const TArray<float>& A, const TArray<float>& B) const
{
    if (A.Num() != B.Num())
    {
        UE_LOG(LogTemp, Warning, TEXT("UEchoStateNetwork: Vector dimension mismatch in Add"));
        return A;
    }
    
    TArray<float> Result;
    Result.Reserve(A.Num());
    
    for (int32 i = 0; i < A.Num(); ++i)
    {
        Result.Add(A[i] + B[i]);
    }
    
    return Result;
}

TArray<float> UEchoStateNetwork::VectorScale(const TArray<float>& V, float Scalar) const
{
    TArray<float> Result;
    Result.Reserve(V.Num());
    
    for (float x : V)
    {
        Result.Add(x * Scalar);
    }
    
    return Result;
}

float UEchoStateNetwork::VectorNorm(const TArray<float>& V) const
{
    float sum = 0.0f;
    for (float x : V)
    {
        sum += x * x;
    }
    return FMath::Sqrt(sum);
}

int32 UEchoStateNetwork::GenerateSeed() const
{
    if (Config.RandomSeed > 0)
    {
        return Config.RandomSeed;
    }
    
    // Use system time for random seed
    return static_cast<int32>(FPlatformTime::Cycles());
}

bool UEchoStateNetwork::ValidateInput(const TArray<float>& Input) const
{
    return Input.Num() == Config.InputDim;
}
