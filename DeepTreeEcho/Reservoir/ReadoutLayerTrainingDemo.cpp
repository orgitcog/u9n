/**
 * Standalone Example: Readout Layer Training System
 * Feature F1.2.4 - Demonstration without Unreal Engine
 * 
 * This example demonstrates the core algorithms of the Readout Layer Training
 * system using standard C++ without Unreal Engine dependencies.
 */

#include <iostream>
#include <vector>
#include <cmath>
#include <random>
#include <chrono>
#include <algorithm>

// Type aliases for compatibility
template<typename T>
using TArray = std::vector<T>;

/**
 * Simple Matrix class for demonstration
 */
class Matrix {
public:
    std::vector<std::vector<float>> data;
    int rows, cols;

    Matrix(int r, int c) : rows(r), cols(c) {
        data.resize(r, std::vector<float>(c, 0.0f));
    }

    float& operator()(int i, int j) { return data[i][j]; }
    const float& operator()(int i, int j) const { return data[i][j]; }

    // Matrix multiplication
    Matrix operator*(const Matrix& other) const {
        if (cols != other.rows) {
            throw std::runtime_error("Matrix dimensions incompatible");
        }
        Matrix result(rows, other.cols);
        for (int i = 0; i < rows; ++i) {
            for (int j = 0; j < other.cols; ++j) {
                float sum = 0.0f;
                for (int k = 0; k < cols; ++k) {
                    sum += data[i][k] * other.data[k][j];
                }
                result(i, j) = sum;
            }
        }
        return result;
    }

    // Transpose
    Matrix transpose() const {
        Matrix result(cols, rows);
        for (int i = 0; i < rows; ++i) {
            for (int j = 0; j < cols; ++j) {
                result(j, i) = data[i][j];
            }
        }
        return result;
    }

    // Add scaled identity
    void addScaledIdentity(float lambda) {
        for (int i = 0; i < std::min(rows, cols); ++i) {
            data[i][i] += lambda;
        }
    }

    // Print matrix
    void print(const std::string& name = "") const {
        if (!name.empty()) std::cout << name << ":\n";
        for (int i = 0; i < std::min(5, rows); ++i) {
            for (int j = 0; j < std::min(5, cols); ++j) {
                std::cout << data[i][j] << " ";
            }
            if (cols > 5) std::cout << "...";
            std::cout << "\n";
        }
        if (rows > 5) std::cout << "...\n";
    }
};

/**
 * Metrics structure
 */
struct Metrics {
    float MSE = 0.0f;
    float RMSE = 0.0f;
    float MAE = 0.0f;
    float RSquared = 0.0f;
    int numSamples = 0;

    void print() const {
        std::cout << "Metrics:\n"
                  << "  MSE: " << MSE << "\n"
                  << "  RMSE: " << RMSE << "\n"
                  << "  MAE: " << MAE << "\n"
                  << "  R²: " << RSquared << "\n"
                  << "  Samples: " << numSamples << "\n";
    }
};

/**
 * Ridge Regression Trainer
 */
class RidgeRegression {
private:
    float lambda;
    Matrix weights;
    bool trained;

public:
    RidgeRegression(float regularization = 1e-6f) 
        : lambda(regularization), weights(1, 1), trained(false) {}

    // Gauss-Jordan matrix inversion
    Matrix invert(const Matrix& A) const {
        int n = A.rows;
        Matrix aug(n, 2 * n);
        
        // Create augmented matrix [A | I]
        for (int i = 0; i < n; ++i) {
            for (int j = 0; j < n; ++j) {
                aug(i, j) = A(i, j);
                aug(i, n + j) = (i == j) ? 1.0f : 0.0f;
            }
        }

        // Forward elimination
        for (int i = 0; i < n; ++i) {
            // Find pivot
            int maxRow = i;
            float maxVal = std::abs(aug(i, i));
            for (int k = i + 1; k < n; ++k) {
                if (std::abs(aug(k, i)) > maxVal) {
                    maxVal = std::abs(aug(k, i));
                    maxRow = k;
                }
            }

            // Swap rows
            if (maxRow != i) {
                std::swap(aug.data[i], aug.data[maxRow]);
            }

            // Scale pivot row
            float pivot = aug(i, i);
            if (std::abs(pivot) < 1e-10f) {
                throw std::runtime_error("Matrix is singular");
            }
            for (int j = 0; j < 2 * n; ++j) {
                aug(i, j) /= pivot;
            }

            // Eliminate column
            for (int k = 0; k < n; ++k) {
                if (k != i) {
                    float factor = aug(k, i);
                    for (int j = 0; j < 2 * n; ++j) {
                        aug(k, j) -= factor * aug(i, j);
                    }
                }
            }
        }

        // Extract inverse
        Matrix inv(n, n);
        for (int i = 0; i < n; ++i) {
            for (int j = 0; j < n; ++j) {
                inv(i, j) = aug(i, n + j);
            }
        }
        return inv;
    }

    // Train using ridge regression
    Metrics train(const Matrix& X, const Matrix& Y) {
        auto start = std::chrono::high_resolution_clock::now();

        // Compute X^T X
        Matrix XT = X.transpose();
        Matrix XTX = XT * X;

        // Add regularization: X^T X + lambda * I
        XTX.addScaledIdentity(lambda);

        // Compute X^T Y
        Matrix XTY = XT * Y;

        // Solve: W = (X^T X + lambda * I)^(-1) X^T Y
        Matrix XTX_inv = invert(XTX);
        weights = XTX_inv * XTY;

        trained = true;

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        // Compute metrics
        Matrix predictions = predict(X);
        Metrics metrics = computeMetrics(predictions, Y);

        std::cout << "Ridge Regression Training:\n"
                  << "  Lambda: " << lambda << "\n"
                  << "  Training time: " << (duration.count() / 1000.0f) << " ms\n";
        metrics.print();

        return metrics;
    }

    // Predict
    Matrix predict(const Matrix& X) const {
        if (!trained) {
            throw std::runtime_error("Model not trained");
        }
        return X * weights;
    }

    // Compute metrics
    Metrics computeMetrics(const Matrix& pred, const Matrix& target) const {
        Metrics m;
        m.numSamples = pred.rows;

        float sumSqError = 0.0f;
        float sumAbsError = 0.0f;
        float sumTarget = 0.0f;
        float sumSqTarget = 0.0f;

        for (int i = 0; i < pred.rows; ++i) {
            for (int j = 0; j < pred.cols; ++j) {
                float error = target(i, j) - pred(i, j);
                sumSqError += error * error;
                sumAbsError += std::abs(error);
                sumTarget += target(i, j);
                sumSqTarget += target(i, j) * target(i, j);
            }
        }

        int totalElements = pred.rows * pred.cols;
        m.MSE = sumSqError / totalElements;
        m.RMSE = std::sqrt(m.MSE);
        m.MAE = sumAbsError / totalElements;

        // R-squared
        float meanTarget = sumTarget / totalElements;
        float totalSS = 0.0f;
        for (int i = 0; i < pred.rows; ++i) {
            for (int j = 0; j < pred.cols; ++j) {
                float deviation = target(i, j) - meanTarget;
                totalSS += deviation * deviation;
            }
        }

        if (totalSS > 1e-10f) {
            m.RSquared = 1.0f - (sumSqError / totalSS);
        }

        return m;
    }
};

/**
 * RLS (Recursive Least Squares) Trainer
 */
class RLSTrainer {
private:
    Matrix weights;
    Matrix P;  // Covariance matrix
    float alpha;
    float forgettingFactor;
    bool initialized;

public:
    RLSTrainer(float alpha_ = 1e-3f, float forgetting = 1.0f)
        : weights(1, 1), P(1, 1), alpha(alpha_), 
          forgettingFactor(forgetting), initialized(false) {}

    void initialize(int inputDim, int outputDim) {
        weights = Matrix(inputDim, outputDim);
        P = Matrix(inputDim, inputDim);
        
        // Initialize P as scaled identity
        for (int i = 0; i < inputDim; ++i) {
            P(i, i) = 1.0f / alpha;
        }
        
        initialized = true;
    }

    Metrics trainOnline(const std::vector<float>& x, const std::vector<float>& y) {
        int inputDim = x.size();
        int outputDim = y.size();

        if (!initialized) {
            initialize(inputDim, outputDim);
        }

        // Compute current prediction
        std::vector<float> pred(outputDim, 0.0f);
        for (int i = 0; i < outputDim; ++i) {
            for (int j = 0; j < inputDim; ++j) {
                pred[i] += weights(j, i) * x[j];
            }
        }

        // Compute error
        std::vector<float> error(outputDim);
        for (int i = 0; i < outputDim; ++i) {
            error[i] = y[i] - pred[i];
        }

        // RLS update: k = P * r / (lambda + r^T * P * r)
        std::vector<float> Pr(inputDim, 0.0f);
        for (int i = 0; i < inputDim; ++i) {
            for (int j = 0; j < inputDim; ++j) {
                Pr[i] += P(i, j) * x[j];
            }
        }

        float rPr = 0.0f;
        for (int i = 0; i < inputDim; ++i) {
            rPr += x[i] * Pr[i];
        }

        std::vector<float> k(inputDim);
        float lambda = forgettingFactor;
        float denom = lambda * (lambda + rPr);
        if (std::abs(denom) > 1e-10f) {
            for (int i = 0; i < inputDim; ++i) {
                k[i] = Pr[i] / denom;
            }
        }

        // Update P: P = (P - k * (P * r)^T) / lambda
        for (int i = 0; i < inputDim; ++i) {
            for (int j = 0; j < inputDim; ++j) {
                P(i, j) = (P(i, j) - k[i] * Pr[j]) / lambda;
            }
        }

        // Update weights: W = W + k * e^T
        for (int i = 0; i < inputDim; ++i) {
            for (int j = 0; j < outputDim; ++j) {
                weights(i, j) += k[i] * error[j];
            }
        }

        // Compute metrics
        Metrics m;
        m.numSamples = 1;
        for (int i = 0; i < outputDim; ++i) {
            m.MSE += error[i] * error[i];
            m.MAE += std::abs(error[i]);
        }
        m.MSE /= outputDim;
        m.MAE /= outputDim;
        m.RMSE = std::sqrt(m.MSE);

        return m;
    }
};

/**
 * Main demonstration
 */
int main() {
    std::cout << "=== Readout Layer Training System Demo ===\n\n";

    // Generate synthetic data: y = 2*x1 + 3*x2 + 1
    int numSamples = 100;
    int inputDim = 2;
    int outputDim = 1;

    Matrix X(numSamples, inputDim);
    Matrix Y(numSamples, outputDim);

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    std::cout << "Generating synthetic data...\n";
    std::cout << "  True function: y = 2*x1 + 3*x2 + 1\n";
    std::cout << "  Samples: " << numSamples << "\n\n";

    for (int i = 0; i < numSamples; ++i) {
        X(i, 0) = dist(rng);
        X(i, 1) = dist(rng);
        Y(i, 0) = 2.0f * X(i, 0) + 3.0f * X(i, 1) + 1.0f;
        
        // Add noise
        Y(i, 0) += 0.01f * (dist(rng) - 0.5f);
    }

    // Demo 1: Ridge Regression
    std::cout << "=== Demo 1: Ridge Regression ===\n";
    RidgeRegression ridge(1e-6f);
    ridge.train(X, Y);
    std::cout << "\n";

    // Demo 2: RLS Online Learning
    std::cout << "=== Demo 2: RLS Online Learning ===\n";
    RLSTrainer rls(1e-3f, 1.0f);
    
    std::cout << "Training online (showing every 10th sample)...\n";
    for (int i = 0; i < numSamples; ++i) {
        std::vector<float> x = {X(i, 0), X(i, 1)};
        std::vector<float> y = {Y(i, 0)};
        
        Metrics m = rls.trainOnline(x, y);
        
        if (i % 10 == 0) {
            std::cout << "  Sample " << i << ": MSE = " << m.MSE << "\n";
        }
    }
    std::cout << "\n";

    // Demo 3: Regularization comparison
    std::cout << "=== Demo 3: Regularization Comparison ===\n";
    std::vector<float> lambdas = {1e-8f, 1e-6f, 1e-4f, 1e-2f, 1.0f};
    
    for (float lambda : lambdas) {
        RidgeRegression ridge_test(lambda);
        Metrics m = ridge_test.train(X, Y);
        std::cout << "Lambda = " << lambda << ": R² = " << m.RSquared << "\n";
    }
    std::cout << "\n";

    std::cout << "=== Demo Complete ===\n";
    return 0;
}
