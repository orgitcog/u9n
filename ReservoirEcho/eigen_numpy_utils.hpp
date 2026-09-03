#ifndef RESERVOIRCPP_EIGEN_NUMPY_UTILS_HPP
#define RESERVOIRCPP_EIGEN_NUMPY_UTILS_HPP

#include <Eigen/Dense>
#include <Eigen/Sparse>
#include <random>
#include <vector>
#include <algorithm>
#include <cmath>
#include <tuple>

namespace reservoircpp {

using MatrixXd = Eigen::MatrixXd;
using VectorXd = Eigen::VectorXd;
using SparseMatrix = Eigen::SparseMatrix<double>;

/**
 * Matrix dot product (equivalent to np.dot)
 */
template<typename DerivedA, typename DerivedB>
auto dot(const Eigen::MatrixBase<DerivedA>& a, const Eigen::MatrixBase<DerivedB>& b) {
    return a * b;
}

/**
 * Pseudo-inverse (equivalent to np.linalg.pinv)
 */
template<typename Derived>
auto pseudoInverse(const Eigen::MatrixBase<Derived>& m, double epsilon=1e-10) {
    Eigen::JacobiSVD<typename Derived::PlainObject> svd(m, Eigen::ComputeThinU | Eigen::ComputeThinV);
    
    const auto& singularValues = svd.singularValues();
    Eigen::Matrix<typename Derived::Scalar, Eigen::Dynamic, 1> singularValuesInv = singularValues;
    
    for (int i = 0; i < singularValues.size(); ++i) {
        if (singularValues(i) > epsilon) {
            singularValuesInv(i) = 1.0 / singularValues(i);
        } else {
            singularValuesInv(i) = 0.0;
        }
    }
    
    return svd.matrixV() * singularValuesInv.asDiagonal() * svd.matrixU().transpose();
}

/**
 * Get eigenvalues (equivalent to np.linalg.eigvals)
 */
template<typename Derived>
auto eigenvalues(const Eigen::MatrixBase<Derived>& m) {
    Eigen::EigenSolver<typename Derived::PlainObject> solver(m);
    return solver.eigenvalues();
}

/**
 * Perform eigenvalue decomposition (equivalent to np.linalg.eig)
 */
template<typename Derived>
auto eigenDecomposition(const Eigen::MatrixBase<Derived>& m) {
    Eigen::EigenSolver<typename Derived::PlainObject> solver(m);
    return std::make_tuple(solver.eigenvalues(), solver.eigenvectors());
}

/**
 * Generate random normal values (equivalent to np.random.randn)
 */
template<typename RNG>
MatrixXd randomNormal(int rows, int cols, RNG& rng) {
    std::normal_distribution<double> dist(0.0, 1.0);
    MatrixXd result(rows, cols);
    for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < cols; ++j) {
            result(i, j) = dist(rng);
        }
    }
    return result;
}

/**
 * Generate a random sparse matrix (equivalent to scipy.sparse.random)
 */
template<typename RNG>
SparseMatrix randomSparse(int rows, int cols, double density, RNG& rng) {
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    SparseMatrix result(rows, cols);
    
    // Approximate number of non-zero elements
    int nnz = static_cast<int>(rows * cols * density);
    
    std::vector<Eigen::Triplet<double>> triplets;
    triplets.reserve(nnz);
    
    for (int k = 0; k < nnz; ++k) {
        int i = std::floor(dist(rng) * rows);
        int j = std::floor(dist(rng) * cols);
        double value = dist(rng);
        
        triplets.push_back(Eigen::Triplet<double>(i, j, value));
    }
    
    result.setFromTriplets(triplets.begin(), triplets.end());
    return result;
}

/**
 * Concatenate matrices vertically (equivalent to np.vstack)
 */
template<typename DerivedA, typename DerivedB>
auto vstack(const Eigen::MatrixBase<DerivedA>& a, const Eigen::MatrixBase<DerivedB>& b) {
    MatrixXd result(a.rows() + b.rows(), a.cols());
    result << a, b;
    return result;
}

/**
 * Concatenate matrices horizontally (equivalent to np.hstack)
 */
template<typename DerivedA, typename DerivedB>
auto hstack(const Eigen::MatrixBase<DerivedA>& a, const Eigen::MatrixBase<DerivedB>& b) {
    MatrixXd result(a.rows(), a.cols() + b.cols());
    result << a, b;
    return result;
}

/**
 * Mean of all elements (equivalent to np.mean)
 */
template<typename Derived>
auto mean(const Eigen::MatrixBase<Derived>& m) {
    return m.mean();
}

/**
 * Slicing operation (partial implementation of Python slicing)
 */
template<typename Derived>
auto slice(const Eigen::MatrixBase<Derived>& m, int start, int end) {
    int rows = end - start;
    return m.block(start, 0, rows, m.cols());
}

/**
 * Reshaping a matrix (equivalent to np.reshape)
 */
template<typename Derived>
auto reshape(const Eigen::MatrixBase<Derived>& m, int rows, int cols) {
    return Eigen::Map<const MatrixXd>(m.data(), rows, cols);
}

} // namespace reservoircpp

#endif // RESERVOIRCPP_EIGEN_NUMPY_UTILS_HPP