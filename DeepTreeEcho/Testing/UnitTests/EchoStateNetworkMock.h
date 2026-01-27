/**
 * @file EchoStateNetworkMock.h
 * @brief Mock UEchoStateNetwork for unit testing without Unreal Engine
 * 
 * Provides a simplified mock that mimics the UEchoStateNetwork interface
 * for standalone unit testing.
 * 
 * @author Deep Tree Echo Team
 * @date January 2026
 */

#pragma once

#include <vector>
#include <string>
#include <random>
#include <cmath>
#include <algorithm>
#include <chrono>

// Mock configuration struct
struct MockESNConfig {
    int ReservoirSize = 100;
    int InputDim = 10;
    int OutputDim = 1;
    float SpectralRadius = 0.9f;
    float LeakRate = 0.3f;
    float ReservoirSparsity = 0.1f;
    float InputSparsity = 0.5f;
    float InputScaling = 1.0f;
    float OutputScaling = 1.0f;
    float BiasScaling = 0.1f;
    bool bEnableBias = true;
    float NoiseLevel = 0.0f;
    int RandomSeed = 0;
    std::string ActivationFunction = "tanh";
    float RegularizationLambda = 1e-6f;
};

// Mock state struct
struct MockESNState {
    std::vector<float> ReservoirState;
    std::vector<float> LastInput;
    std::vector<float> LastOutput;
    int Timestep = 0;
    bool bInitialized = false;
    float ActualSpectralRadius = 0.0f;
};

// Mock sparse matrix
struct MockSparseMatrix {
    std::vector<float> Values;
    std::vector<int> ColIndices;
    std::vector<int> RowPointers;
    int Rows = 0;
    int Cols = 0;
    int NonZeros = 0;
};

// Mock ESN implementation
class MockEchoStateNetwork {
public:
    MockEchoStateNetwork() = default;

    bool Initialize() {
        return InitializeWithConfig(Config);
    }

    bool InitializeWithConfig(const MockESNConfig& NewConfig) {
        Config = NewConfig;
        
        if (Config.ReservoirSize < 10 || Config.InputDim < 1 || Config.OutputDim < 1) {
            return false;
        }
        
        int Seed = GenerateSeed();
        
        // Generate reservoir weights
        ReservoirWeights = GenerateSparseRandomMatrix(
            Config.ReservoirSize, Config.ReservoirSize, 
            Config.ReservoirSparsity, Seed);
        
        // Scale to spectral radius
        ScaleToSpectralRadius(ReservoirWeights, Config.SpectralRadius);
        State.ActualSpectralRadius = ComputeSpectralRadius(ReservoirWeights);
        
        // Generate input weights
        int InputCols = Config.bEnableBias ? Config.InputDim + 1 : Config.InputDim;
        InputWeights = GenerateSparseRandomMatrix(
            Config.ReservoirSize, InputCols,
            Config.InputSparsity, Seed + 1);
        
        // Scale input weights
        for (float& val : InputWeights.Values) {
            val *= Config.InputScaling;
        }
        
        // Initialize bias
        if (Config.bEnableBias) {
            BiasVector.resize(Config.ReservoirSize);
            std::mt19937 gen(Seed + 2);
            std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
            for (float& b : BiasVector) {
                b = dist(gen) * Config.BiasScaling;
            }
        }
        
        // Initialize output weights
        OutputWeights.resize(Config.OutputDim * Config.ReservoirSize, 0.0f);
        
        ResetState();
        State.bInitialized = true;
        
        return true;
    }

    void ResetState() {
        State.ReservoirState.resize(Config.ReservoirSize, 0.0f);
        State.LastInput.clear();
        State.LastOutput.clear();
        State.Timestep = 0;
    }

    std::vector<float> ProcessInput(const std::vector<float>& Input) {
        if (!State.bInitialized || Input.size() != static_cast<size_t>(Config.InputDim)) {
            return State.ReservoirState;
        }
        
        // Prepare input with bias
        std::vector<float> InputWithBias = Input;
        if (Config.bEnableBias) {
            InputWithBias.push_back(1.0f);
        }
        
        // Win * u(t)
        auto InputContrib = SparseMatVecMul(InputWeights, InputWithBias);
        
        // W * x(t-1)
        auto RecurrentContrib = SparseMatVecMul(ReservoirWeights, State.ReservoirState);
        
        // Combine
        std::vector<float> PreActivation = VectorAdd(InputContrib, RecurrentContrib);
        if (Config.bEnableBias && !BiasVector.empty()) {
            PreActivation = VectorAdd(PreActivation, BiasVector);
        }
        
        // Apply activation
        auto Activation = ApplyActivation(PreActivation, Config.ActivationFunction);
        
        // Add noise
        if (Config.NoiseLevel > 0.0f) {
            std::mt19937 gen(State.Timestep);
            std::uniform_real_distribution<float> noise(-Config.NoiseLevel, Config.NoiseLevel);
            for (float& x : Activation) {
                x += noise(gen);
            }
        }
        
        // Leaky integrator: x(t) = (1-α)*x(t-1) + α*f(...)
        for (size_t i = 0; i < State.ReservoirState.size(); ++i) {
            State.ReservoirState[i] = (1.0f - Config.LeakRate) * State.ReservoirState[i] +
                                      Config.LeakRate * Activation[i];
        }
        
        State.LastInput = Input;
        State.Timestep++;
        
        return State.ReservoirState;
    }

    std::vector<std::vector<float>> ProcessSequence(const std::vector<std::vector<float>>& Inputs) {
        std::vector<std::vector<float>> States;
        for (const auto& input : Inputs) {
            States.push_back(ProcessInput(input));
        }
        return States;
    }

    std::vector<float> GetReservoirState() const {
        return State.ReservoirState;
    }

    std::vector<float> ComputeOutput() {
        if (!State.bInitialized || OutputWeights.empty()) {
            return std::vector<float>();
        }
        
        auto output = DenseMatVecMul(OutputWeights, State.ReservoirState,
                                     Config.OutputDim, Config.ReservoirSize);
        
        // Scale output
        for (float& y : output) {
            y *= Config.OutputScaling;
        }
        
        State.LastOutput = output;
        return output;
    }

    float TrainOutputWeights(const std::vector<std::vector<float>>& TrainingInputs,
                            const std::vector<std::vector<float>>& TrainingTargets,
                            int WashoutSteps) {
        if (!State.bInitialized || TrainingInputs.size() != TrainingTargets.size()) {
            return -1.0f;
        }
        
        ResetState();
        
        // Collect states
        std::vector<std::vector<float>> States;
        for (size_t i = 0; i < TrainingInputs.size(); ++i) {
            auto state = ProcessInput(TrainingInputs[i]);
            if (static_cast<int>(i) >= WashoutSteps) {
                States.push_back(state);
            }
        }
        
        if (States.empty()) {
            return -1.0f;
        }
        
        int NumSamples = States.size();
        
        // Build X and Y matrices (simplified training)
        // Wout = (X^T * Y) / (N + λ)
        std::vector<float> XTY(Config.ReservoirSize * Config.OutputDim, 0.0f);
        
        for (int i = 0; i < NumSamples; ++i) {
            int targetIdx = i + WashoutSteps;
            if (targetIdx >= static_cast<int>(TrainingTargets.size())) break;
            
            for (int j = 0; j < Config.ReservoirSize; ++j) {
                for (int k = 0; k < Config.OutputDim; ++k) {
                    if (k < static_cast<int>(TrainingTargets[targetIdx].size())) {
                        XTY[j * Config.OutputDim + k] += 
                            States[i][j] * TrainingTargets[targetIdx][k];
                    }
                }
            }
        }
        
        float normalization = NumSamples + Config.RegularizationLambda;
        for (float& w : XTY) {
            w /= normalization;
        }
        
        OutputWeights = XTY;
        
        // Compute error
        ResetState();
        float totalError = 0.0f;
        int errorSamples = 0;
        
        for (size_t i = 0; i < TrainingInputs.size(); ++i) {
            ProcessInput(TrainingInputs[i]);
            if (static_cast<int>(i) >= WashoutSteps && i < TrainingTargets.size()) {
                auto output = ComputeOutput();
                float error = 0.0f;
                for (size_t j = 0; j < std::min(output.size(), TrainingTargets[i].size()); ++j) {
                    float diff = output[j] - TrainingTargets[i][j];
                    error += diff * diff;
                }
                totalError += std::sqrt(error);
                errorSamples++;
            }
        }
        
        return errorSamples > 0 ? totalError / errorSamples : 0.0f;
    }

    float GetSpectralRadius() const {
        return State.ActualSpectralRadius;
    }

    float GetReservoirConnectivity() const {
        if (ReservoirWeights.NonZeros == 0 || 
            ReservoirWeights.Rows == 0 || ReservoirWeights.Cols == 0) {
            return 0.0f;
        }
        int total = ReservoirWeights.Rows * ReservoirWeights.Cols;
        return static_cast<float>(ReservoirWeights.NonZeros) / static_cast<float>(total);
    }

    float GetAverageActivation() const {
        if (State.ReservoirState.empty()) return 0.0f;
        float sum = 0.0f;
        for (float x : State.ReservoirState) {
            sum += std::abs(x);
        }
        return sum / State.ReservoirState.size();
    }

private:
    MockESNConfig Config;
    MockESNState State;
    MockSparseMatrix ReservoirWeights;
    MockSparseMatrix InputWeights;
    std::vector<float> OutputWeights;
    std::vector<float> BiasVector;

    int GenerateSeed() const {
        return Config.RandomSeed > 0 ? Config.RandomSeed : 
               static_cast<int>(std::chrono::system_clock::now().time_since_epoch().count());
    }

    MockSparseMatrix GenerateSparseRandomMatrix(int Rows, int Cols, float Sparsity, int Seed) {
        MockSparseMatrix mat;
        mat.Rows = Rows;
        mat.Cols = Cols;
        
        std::mt19937 gen(Seed);
        std::normal_distribution<float> normal(0.0f, 1.0f);
        std::uniform_real_distribution<float> uniform(0.0f, 1.0f);
        
        std::vector<int> RowIndices;
        std::vector<int> ColIndices;
        std::vector<float> Values;
        
        for (int i = 0; i < Rows; ++i) {
            for (int j = 0; j < Cols; ++j) {
                if (uniform(gen) < Sparsity) {
                    RowIndices.push_back(i);
                    ColIndices.push_back(j);
                    Values.push_back(normal(gen));
                }
            }
        }
        
        mat.NonZeros = Values.size();
        mat.RowPointers.resize(Rows + 1, 0);
        mat.ColIndices = ColIndices;
        mat.Values = Values;
        
        for (int idx : RowIndices) {
            mat.RowPointers[idx + 1]++;
        }
        for (int i = 1; i <= Rows; ++i) {
            mat.RowPointers[i] += mat.RowPointers[i - 1];
        }
        
        return mat;
    }

    float ComputeSpectralRadius(const MockSparseMatrix& mat, int maxIter = 100) {
        if (mat.Rows != mat.Cols) return 0.0f;
        
        std::vector<float> v(mat.Rows);
        for (float& x : v) x = (rand() % 1000) / 1000.0f - 0.5f;
        
        float norm = VectorNorm(v);
        if (norm > 0.0f) {
            for (float& x : v) x /= norm;
        }
        
        float eigenvalue = 0.0f;
        for (int iter = 0; iter < maxIter; ++iter) {
            auto v_new = SparseMatVecMul(mat, v);
            float lambda = 0.0f;
            for (size_t i = 0; i < v.size(); ++i) {
                lambda += v_new[i] * v[i];
            }
            
            norm = VectorNorm(v_new);
            if (norm > 0.0f) {
                for (float& x : v_new) x /= norm;
            }
            
            if (std::abs(lambda - eigenvalue) < 1e-6f) {
                eigenvalue = lambda;
                break;
            }
            
            eigenvalue = lambda;
            v = v_new;
        }
        
        return std::abs(eigenvalue);
    }

    void ScaleToSpectralRadius(MockSparseMatrix& mat, float targetRadius) {
        float currentRadius = ComputeSpectralRadius(mat);
        if (currentRadius > 1e-10f) {
            float scale = targetRadius / currentRadius;
            for (float& val : mat.Values) {
                val *= scale;
            }
        }
    }

    std::vector<float> SparseMatVecMul(const MockSparseMatrix& mat, 
                                       const std::vector<float>& vec) const {
        if (vec.size() != static_cast<size_t>(mat.Cols)) {
            return std::vector<float>();
        }
        
        std::vector<float> result(mat.Rows, 0.0f);
        for (int i = 0; i < mat.Rows; ++i) {
            float sum = 0.0f;
            for (int j = mat.RowPointers[i]; j < mat.RowPointers[i + 1]; ++j) {
                sum += mat.Values[j] * vec[mat.ColIndices[j]];
            }
            result[i] = sum;
        }
        return result;
    }

    std::vector<float> DenseMatVecMul(const std::vector<float>& mat,
                                      const std::vector<float>& vec,
                                      int rows, int cols) const {
        if (vec.size() != static_cast<size_t>(cols)) {
            return std::vector<float>();
        }
        
        std::vector<float> result(rows, 0.0f);
        for (int i = 0; i < rows; ++i) {
            float sum = 0.0f;
            for (int j = 0; j < cols; ++j) {
                sum += mat[i * cols + j] * vec[j];
            }
            result[i] = sum;
        }
        return result;
    }

    std::vector<float> ApplyActivation(const std::vector<float>& vec, 
                                       const std::string& func) const {
        std::vector<float> result;
        result.reserve(vec.size());
        
        for (float x : vec) {
            if (func == "tanh") {
                result.push_back(std::tanh(x));
            } else if (func == "sigmoid") {
                result.push_back(1.0f / (1.0f + std::exp(-x)));
            } else if (func == "relu") {
                result.push_back(std::max(0.0f, x));
            } else {
                result.push_back(std::tanh(x));
            }
        }
        return result;
    }

    std::vector<float> VectorAdd(const std::vector<float>& a, 
                                 const std::vector<float>& b) const {
        if (a.size() != b.size()) return a;
        std::vector<float> result;
        result.reserve(a.size());
        for (size_t i = 0; i < a.size(); ++i) {
            result.push_back(a[i] + b[i]);
        }
        return result;
    }

    float VectorNorm(const std::vector<float>& v) const {
        float sum = 0.0f;
        for (float x : v) sum += x * x;
        return std::sqrt(sum);
    }
};
