#ifndef RESERVOIRCPP_NUMPY_HPP
#define RESERVOIRCPP_NUMPY_HPP

/**
 * @brief NumPy-like array operations for C++
 * 
 * This header provides NumPy-compatible array operations using Eigen
 * for the reservoir computing framework.
 */

#include <Eigen/Dense>
#include <Eigen/Sparse>
#include <vector>
#include <random>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <functional>

namespace reservoircpp {
namespace numpy {

// Type aliases for common array types
using Array1d = Eigen::ArrayXd;
using Array2d = Eigen::ArrayXXd;
using Matrix = Eigen::MatrixXd;
using Vector = Eigen::VectorXd;
using SparseMatrix = Eigen::SparseMatrix<double>;

// Random number generation
class RandomState {
public:
    explicit RandomState(unsigned int seed = std::random_device{}())
        : gen_(seed) {}

    void seed(unsigned int s) { gen_.seed(s); }

    // Uniform distribution [0, 1)
    double rand() {
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        return dist(gen_);
    }

    Array1d rand(Eigen::Index size) {
        Array1d result(size);
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        for (Eigen::Index i = 0; i < size; ++i) {
            result(i) = dist(gen_);
        }
        return result;
    }

    Array2d rand(Eigen::Index rows, Eigen::Index cols) {
        Array2d result(rows, cols);
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        for (Eigen::Index i = 0; i < rows; ++i) {
            for (Eigen::Index j = 0; j < cols; ++j) {
                result(i, j) = dist(gen_);
            }
        }
        return result;
    }

    // Normal distribution
    double randn() {
        std::normal_distribution<double> dist(0.0, 1.0);
        return dist(gen_);
    }

    Array1d randn(Eigen::Index size) {
        Array1d result(size);
        std::normal_distribution<double> dist(0.0, 1.0);
        for (Eigen::Index i = 0; i < size; ++i) {
            result(i) = dist(gen_);
        }
        return result;
    }

    Array2d randn(Eigen::Index rows, Eigen::Index cols) {
        Array2d result(rows, cols);
        std::normal_distribution<double> dist(0.0, 1.0);
        for (Eigen::Index i = 0; i < rows; ++i) {
            for (Eigen::Index j = 0; j < cols; ++j) {
                result(i, j) = dist(gen_);
            }
        }
        return result;
    }

    // Uniform distribution [low, high)
    Array1d uniform(double low, double high, Eigen::Index size) {
        Array1d result(size);
        std::uniform_real_distribution<double> dist(low, high);
        for (Eigen::Index i = 0; i < size; ++i) {
            result(i) = dist(gen_);
        }
        return result;
    }

    // Integer uniform distribution
    int randint(int low, int high) {
        std::uniform_int_distribution<int> dist(low, high - 1);
        return dist(gen_);
    }

    // Choice from array
    template<typename T>
    T choice(const std::vector<T>& arr) {
        std::uniform_int_distribution<std::size_t> dist(0, arr.size() - 1);
        return arr[dist(gen_)];
    }

    // Shuffle
    template<typename T>
    void shuffle(std::vector<T>& arr) {
        std::shuffle(arr.begin(), arr.end(), gen_);
    }

    std::mt19937& generator() { return gen_; }

private:
    std::mt19937 gen_;
};

// Global random state
inline RandomState& default_rng() {
    static RandomState rng;
    return rng;
}

// Array creation functions
inline Array1d zeros(Eigen::Index size) {
    return Array1d::Zero(size);
}

inline Array2d zeros(Eigen::Index rows, Eigen::Index cols) {
    return Array2d::Zero(rows, cols);
}

inline Array1d ones(Eigen::Index size) {
    return Array1d::Ones(size);
}

inline Array2d ones(Eigen::Index rows, Eigen::Index cols) {
    return Array2d::Ones(rows, cols);
}

inline Array1d full(Eigen::Index size, double value) {
    return Array1d::Constant(size, value);
}

inline Array2d full(Eigen::Index rows, Eigen::Index cols, double value) {
    return Array2d::Constant(rows, cols, value);
}

inline Array1d linspace(double start, double stop, Eigen::Index num) {
    return Array1d::LinSpaced(num, start, stop);
}

inline Array1d arange(double start, double stop, double step = 1.0) {
    Eigen::Index num = static_cast<Eigen::Index>(std::ceil((stop - start) / step));
    Array1d result(num);
    for (Eigen::Index i = 0; i < num; ++i) {
        result(i) = start + i * step;
    }
    return result;
}

inline Matrix eye(Eigen::Index n) {
    return Matrix::Identity(n, n);
}

// Mathematical functions
inline Array1d exp(const Array1d& arr) { return arr.exp(); }
inline Array2d exp(const Array2d& arr) { return arr.exp(); }

inline Array1d log(const Array1d& arr) { return arr.log(); }
inline Array2d log(const Array2d& arr) { return arr.log(); }

inline Array1d sqrt(const Array1d& arr) { return arr.sqrt(); }
inline Array2d sqrt(const Array2d& arr) { return arr.sqrt(); }

inline Array1d abs(const Array1d& arr) { return arr.abs(); }
inline Array2d abs(const Array2d& arr) { return arr.abs(); }

inline Array1d tanh(const Array1d& arr) { return arr.tanh(); }
inline Array2d tanh(const Array2d& arr) { return arr.tanh(); }

inline Array1d sin(const Array1d& arr) { return arr.sin(); }
inline Array1d cos(const Array1d& arr) { return arr.cos(); }

// Aggregation functions
inline double sum(const Array1d& arr) { return arr.sum(); }
inline double sum(const Array2d& arr) { return arr.sum(); }

inline double mean(const Array1d& arr) { return arr.mean(); }
inline double mean(const Array2d& arr) { return arr.mean(); }

inline double std_dev(const Array1d& arr) {
    double m = mean(arr);
    return std::sqrt((arr - m).square().mean());
}

inline double max(const Array1d& arr) { return arr.maxCoeff(); }
inline double min(const Array1d& arr) { return arr.minCoeff(); }

// Linear algebra
inline Matrix dot(const Matrix& a, const Matrix& b) { return a * b; }
inline Vector dot(const Matrix& a, const Vector& b) { return a * b; }

inline double norm(const Vector& v) { return v.norm(); }
inline double norm(const Matrix& m, int p = 2) {
    if (p == 2) {
        Eigen::JacobiSVD<Matrix> svd(m);
        return svd.singularValues()(0);
    }
    return m.norm();
}

// Spectral radius
inline double spectral_radius(const Matrix& m) {
    Eigen::EigenSolver<Matrix> solver(m, false);
    return solver.eigenvalues().cwiseAbs().maxCoeff();
}

} // namespace numpy
} // namespace reservoircpp

// Convenience namespace alias
namespace np = reservoircpp::numpy;

#endif // RESERVOIRCPP_NUMPY_HPP
