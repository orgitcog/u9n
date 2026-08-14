// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>
#include <nls/math/Math.h>
#include <nls/geometry/IncrementalPCA.h>

#include <vector>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

namespace PCAHelper
{

/**
 * @brief Calculates the number of modes to keep for a vector of standard deviations, either limited by the maximum variance or the maximum modes.
 *
 * @tparam T                float or double
 * @param stds              The vector of standard deviations which are assumed to be ordered in decreasing order and >= 0.
 * @param maxVariance      The maximum variance to keep.
 * @param maxModes          The maximum number of modes to be kept.
 * @param verbose           Whether to print verbose output of the variances.
 * @return int              The number of modes.
 */
template <class T>
int NumberOfModes(const Eigen::VectorX<T>& stds, const T maxVariance, const int maxModes, bool verbose = false)
{
    Eigen::VectorX<T> accumulatedVariance = Eigen::VectorX<T>::Zero(stds.size());
    T totalVariance = 0;
    for (int i = 0; i < int(stds.size()); i++)
    {
        const T variance = stds[i] * stds[i];
        totalVariance += variance;
        accumulatedVariance[i] = totalVariance;
    }
    int numModes = 1;
    for (int i = 0; i < int(stds.size()); i++)
    {
        const T variance = stds[i] * stds[i];
        if ((accumulatedVariance[i] / totalVariance < maxVariance) && (numModes < int(stds.size())))
        {
            numModes++;
        }
        if (verbose)
        {
            LOG_VERBOSE("std/variance {}: {}/{} => {} ({})", i, stds[i], variance, variance / totalVariance, accumulatedVariance[i] / totalVariance);
        }
    }
    if ((maxModes > 0) && (numModes > maxModes))
    {
        if (verbose)
        {
            LOG_VERBOSE("restricting pca to {} modes, and {} variance.", maxModes, accumulatedVariance[maxModes - 1] / totalVariance);
        }
        numModes = maxModes;
    }
    else
    {
        if (verbose)
        {
            LOG_VERBOSE("use {} out {} modes", numModes, int(stds.size()));
        }
    }

    return numModes;
}

//! return the number of samples
template <class T, int MatOrder>
int NumSamples(const Eigen::Matrix<T, -1, -1, MatOrder>& dataMatrix, const DataOrder dataOrder)
{
    if (dataOrder == DataOrder::ColsAreExamples)
    {
        return (int)dataMatrix.cols();
    }
    else
    {
        return (int)dataMatrix.rows();
    }
}

// mean center the data
template <class T, int MatOrder>
std::pair<Eigen::VectorX<T>, Eigen::Matrix<T, -1, -1, MatOrder>> MeanCenterData(const Eigen::Matrix<T, -1, -1, MatOrder>& dataMatrix, const DataOrder dataOrder)
{
    Eigen::VectorX<T> mean;
    Eigen::Matrix<T, -1, -1, MatOrder> meanCenteredDataMatrix;
    if (dataOrder == DataOrder::ColsAreExamples)
    {
        mean = dataMatrix.rowwise().mean();
        meanCenteredDataMatrix = dataMatrix.colwise() - mean;
    }
    else
    {
        mean = dataMatrix.colwise().mean();
        meanCenteredDataMatrix = dataMatrix.rowwise() - mean.transpose();
    }
    return { std::move(mean), std::move(meanCenteredDataMatrix) };
}

// flip modes to ensure deterministic output
template <class T, int MatOrder>
void MakeModesDeterministic(Eigen::Matrix<T, -1, -1, MatOrder>& modes)
{
    Eigen::Matrix<T, -1, -1, MatOrder> absModes = modes.array().abs();
    for (int c = 0; c < absModes.cols(); c++)
    {
        int i = 0;
        absModes.col(c).maxCoeff(&i);
        if (modes.col(c)(i) < 0)
        {
            modes.col(c) *= -T(1.0f);
        }
    }
}

} // namespace PCAHelper

template <class T, int MatOrder>
struct EigPCA
{
    using scalar_t = T;
    // mean of pca
    Eigen::VectorX<T> mean;
    // modes of PCA as columns scaled by standard deviation of the mode
    Eigen::Matrix<T, -1, -1, MatOrder> modes;

    template <int MatOrderInput>
    void Create(const Eigen::Matrix<T, -1, -1, MatOrderInput>& dataMatrix,
                const DataOrder& dataOrder,
                const T varianceToKeep,
                const int maxModes = 0,
                bool verbose = false)
    {
        Eigen::Matrix<T, -1, -1, MatOrderInput> meanCenteredDataMatrix;
        std::tie(mean, meanCenteredDataMatrix) = PCAHelper::MeanCenterData<T, MatOrderInput>(dataMatrix, dataOrder);
        const int numSamples = PCAHelper::NumSamples<T, MatOrderInput>(dataMatrix, dataOrder);

        meanCenteredDataMatrix /= sqrt(T(numSamples - 1));

        // create AtA or AAt depending on the matrix size
        const int minSize = std::min((int)dataMatrix.rows(), (int)dataMatrix.cols());
        Eigen::Matrix<T, -1, -1> AtA = Eigen::Matrix<T, -1, -1, MatOrderInput>::Zero(minSize, minSize);
        if (meanCenteredDataMatrix.rows() > meanCenteredDataMatrix.cols())
        {
            AtA.template triangularView<Eigen::Lower>() = meanCenteredDataMatrix.transpose() * meanCenteredDataMatrix;
        }
        else
        {
            AtA.template triangularView<Eigen::Lower>() = meanCenteredDataMatrix * meanCenteredDataMatrix.transpose();
        }

        // note that eigen sorts the eigenvalues in increasing order, hence we need to reverse the eigenvalues and eigenvectors
        Eigen::SelfAdjointEigenSolver<Eigen::Matrix<T, -1, -1>> eig(AtA);
        const Eigen::VectorX<T> stds = eig.eigenvalues().reverse().cwiseMax(Eigen::VectorX<T>::Zero(minSize)).cwiseSqrt();
        const int numModes = PCAHelper::NumberOfModes<T>(stds, varianceToKeep, maxModes, verbose);

        if (dataOrder == DataOrder::ColsAreExamples)
        {
            if (meanCenteredDataMatrix.rows() > meanCenteredDataMatrix.cols())
            {
                modes = (meanCenteredDataMatrix * eig.eigenvectors().rightCols(numModes)).rowwise().reverse();
            }
            else
            {
                modes = eig.eigenvectors().rightCols(numModes).rowwise().reverse();
                modes = modes * Eigen::DiagonalMatrix<T, -1>(stds.head(numModes));
            }
        }
        else
        {
            if (meanCenteredDataMatrix.rows() > meanCenteredDataMatrix.cols())
            {
                modes = eig.eigenvectors().rightCols(numModes).rowwise().reverse();
                modes = modes * Eigen::DiagonalMatrix<T, -1>(stds.head(numModes));
            }
            else
            {
                modes = (meanCenteredDataMatrix.transpose() * eig.eigenvectors().rightCols(numModes)).rowwise().reverse();
            }
        }

        PCAHelper::MakeModesDeterministic<T, MatOrder>(modes);
    }
};


template <class T, int MatOrder>
struct SvdPCA
{
    using scalar_t = T;
    // mean of pca
    Eigen::VectorX<T> mean;
    // modes of PCA as columns scaled by standard deviation of the mode
    Eigen::Matrix<T, -1, -1, MatOrder> modes;

    template <int MatOrderInput>
    void Create(const Eigen::Matrix<T, -1, -1, MatOrderInput>& dataMatrix,
                const DataOrder& dataOrder,
                const T varianceToKeep,
                const int maxModes = 0,
                bool verbose = false)
    {
        Eigen::Matrix<T, -1, -1, MatOrderInput> meanCenteredDataMatrix;
        std::tie(mean, meanCenteredDataMatrix) = PCAHelper::MeanCenterData<T, MatOrderInput>(dataMatrix, dataOrder);
        const int numSamples = PCAHelper::NumSamples<T, MatOrderInput>(dataMatrix, dataOrder);

        const Eigen::JacobiSVD<Eigen::Matrix<T, -1, -1>> svd(meanCenteredDataMatrix, Eigen::ComputeThinU | Eigen::ComputeThinV);
        const Eigen::Vector<T, -1> stds = svd.singularValues() / sqrt(T(numSamples - 1));
        const int numModes = PCAHelper::NumberOfModes<T>(stds, varianceToKeep, maxModes, verbose);

        if (dataOrder == DataOrder::ColsAreExamples)
        {
            modes = svd.matrixU().leftCols(numModes);
        }
        else
        {
            modes = svd.matrixV().leftCols(numModes);
        }

        // move std into the modes
        modes = modes * Eigen::DiagonalMatrix<T, -1>(stds.head(numModes));

        PCAHelper::MakeModesDeterministic<T, MatOrder>(modes);
    }
};

/**
 * @brief Calculates PCA on the mean centered data matrix @p meanCenteredDataMatrix and returns
 * the modes that keep @p varianceToKeep of the variance.
 * @p meanCenteredDataMatrix  Rows are the data samples, columns the dimensions of the data.
 */
template <class T>
Eigen::Matrix<T, -1, -1> CreatePCAWithMeanCenteredData(const Eigen::Matrix<T, -1, -1>& meanCenteredDataMatrix, const T varianceToKeep, const int maxModes = 0)
{
    const Eigen::JacobiSVD<Eigen::Matrix<T, -1, -1>> svd(meanCenteredDataMatrix, Eigen::ComputeThinU | Eigen::ComputeThinV);
    const Eigen::Vector<T, -1> stds = svd.singularValues() / sqrt(T(meanCenteredDataMatrix.rows() - 1));
    Eigen::Matrix<T, -1, -1> modes = svd.matrixV();
    for (int i = 0; i < stds.size(); i++)
    {
        modes.col(i) *= stds[i];
    }

    const int numModes = PCAHelper::NumberOfModes<T>(stds, varianceToKeep, maxModes);
    return modes.leftCols(numModes);
}

/**
 * @brief Calculates PCA on data matrix @p dataMatrix and returns
 * the modes that keep @p varianceToKeep of the variance.
 * @p dataMatrix  Rows are the data samples, columns the dimensions of the data.
 */
template <class T>
std::pair<Eigen::VectorX<T>, Eigen::Matrix<T, -1, -1>> CreatePCA(const Eigen::Matrix<T, -1, -1>& dataMatrix, const T varianceToKeep, const int maxModes = 0)
{
    const Eigen::VectorX<T> mean = dataMatrix.colwise().mean();
    const Eigen::Matrix<T, -1, -1> meanCenteredDataMatrix = dataMatrix.rowwise() - mean.transpose();
    return { mean, CreatePCAWithMeanCenteredData<T>(meanCenteredDataMatrix, varianceToKeep, maxModes) };
}

/**
 * @brief Helper function for PCA calculation. Input data is vectorized and put as rows into the output matrix. Matrix is zero centered and
 * premultiplied with weights, if stated so.
 *
 * @param[in] meshes - The meshes on which PCA is calculated.
 * @param[in] vertexWeights - The vertices for which PCA is calculated together with a weight per vertex.
 * @param[in] premultiplyWeight   Whether the data is weighted before calculating PCA or after calculating PCA for the region.
 *
 * @returns std::pair<Eigen::VectorX<T>, Eigen::Matrix<T, -1, -1>> where the vector is the mean, and matrix is the data
 */
template <class T, int D>
std::pair<Eigen::VectorX<T>, Eigen::Matrix<T, -1, -1>> PrepareRegionPCAData(const std::vector<Eigen::Matrix<T, D, -1>>& meshes,
    const std::vector<std::pair<int, T>>& vertexWeights,
    const bool premultiplyWeight)
{
    const int numVerticesInRegion = int(vertexWeights.size());
    const int numShapes = int(meshes.size());
    const int numDimensions = int(meshes[0].rows());

    // Create a regionShapeMatrix that combines the data for the region (defined with vertexWeights) from all shapes.
    // There will be one row for each shape, and the columns represent vectorized x, y and z coordinates
    Eigen::Matrix<T, -1, -1> regionShapeMatrix = Eigen::Matrix<T, -1, -1>::Zero(numShapes, numDimensions * numVerticesInRegion);
    for (int j = 0; j < int(vertexWeights.size()); j++)
    {
        const int vertexIndex = vertexWeights[j].first;
        for (int i = 0; i < numShapes; i++)
        {
            for (int k = 0; k < numDimensions; k++)
            {
                regionShapeMatrix(i, numDimensions * j + k) = (meshes[i](k, vertexIndex));
            }
        }
    }

    // zero center the regionShapeMatrix
    const Eigen::VectorX<T> mean = regionShapeMatrix.colwise().mean().transpose();
    regionShapeMatrix.rowwise() -= mean.transpose();

    // Premultiply the data with weights before calculating the PCA models
    if (premultiplyWeight)
    {
        for (int j = 0; j < int(vertexWeights.size()); j++)
        {
            const T weight = vertexWeights[j].second;
            for (int i = 0; i < numShapes; i++)
            {
                for (int k = 0; k < numDimensions; k++)
                {
                    regionShapeMatrix(i, numDimensions * j + k) *= weight;
                }
            }
        }
    }

    return { mean, regionShapeMatrix };
}

template <class T, int D>
Eigen::Matrix<T, -1, -1> PrepareRegionPCAData(const std::vector<Eigen::Matrix<T, D, -1>>& meshes,
    const Eigen::Matrix<T, D, -1>& mean,
    const std::vector<std::pair<int, T>>& vertexWeights,
    const bool premultiplyWeight)
{
    const int numVerticesInRegion = int(vertexWeights.size());
    const int numShapes = int(meshes.size());
    const int numDimensions = int(meshes[0].rows());

    Eigen::Matrix<T, D, -1> regionMean(mean.rows(), vertexWeights.size());

    Eigen::Matrix<T, -1, -1> regionShapeMatrix = Eigen::Matrix<T, -1, -1>::Zero(numShapes, numDimensions * numVerticesInRegion);
    for (int j = 0; j < int(vertexWeights.size()); j++)
    {
        const int vertexIndex = vertexWeights[j].first;
        regionMean.col(j) = mean.col(vertexIndex);
        for (int i = 0; i < numShapes; i++)
        {
            for (int k = 0; k < numDimensions; k++)
            {
                regionShapeMatrix(i, numDimensions * j + k) = (meshes[i](k, vertexIndex));
            }
        }
    }
    // zero center the regionShapeMatrix
    const Eigen::VectorX<T> meanVec =  Eigen::Map<const Eigen::Vector<float, -1>>(regionMean.data(), regionMean.cols() * regionMean.rows());
    regionShapeMatrix.rowwise() -= meanVec.transpose();

    // Premultiply the data with weights before calculating the PCA models
    if (premultiplyWeight)
    {
        for (int j = 0; j < int(vertexWeights.size()); j++)
        {
            const T weight = vertexWeights[j].second;
            for (int i = 0; i < numShapes; i++)
            {
                for (int k = 0; k < numDimensions; k++)
                {
                    regionShapeMatrix(i, numDimensions * j + k) *= weight;
                }
            }
        }
    }

    return regionShapeMatrix;
}


/**
 * @brief Applies PCA for meshes @p meshes and the region as defined by @p vertexWeights.
 *
 * @param[in] meshes - The meshes on which PCA is calculated.
 * @param[in] vertexWeights - The vertices for which PCA is calculated together with a weight per vertex.
 * @param[in] varianceToKeep - The variance to keep for PCA.
 * @param[in] premultiplyWeight - Whether the data is multiplied by weights before calculating PCA or not
 * @param[in] maxModes - The maximum number of modes to keep. If <= 0 then it will calculate it based on @p varianceToKeep
 * 
 * @return std::pair<Eigen::VectorX<T>, Eigen::Matrix<T, -1, -1>>  Returns mean and modes of PCA for just the region. The modes are multiplied by the standard
 * deviation.
 */
template <class T, int D>
std::pair<Eigen::VectorX<T>, Eigen::Matrix<T, -1, -1>> CreatePCA(const std::vector<Eigen::Matrix<T, D, -1>>& meshes,
                                                                 const std::vector<std::pair<int, T>>& vertexWeights,
                                                                 const T varianceToKeep,
                                                                 const bool premultiplyWeight,
                                                                 const int maxModes = 0)
{
    int numModes = maxModes;
    auto [mean, data] = PrepareRegionPCAData(meshes, vertexWeights, premultiplyWeight);
    Eigen::Matrix<T, -1, -1> modes = CreatePCAWithMeanCenteredData(data, varianceToKeep, numModes);

    return { mean, modes };
}

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
