// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>
#include <nls/DiffDataMatrix.h>
#include <nls/math/Math.h>
#include <nls/geometry/BarycentricCoordinates.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T, int C, typename DISCARD = typename std::enable_if<(C == 3), void>::type>
class LengthConstraintFunction
{
public:
    /**
     * Function to calculate a length constraint measuremed along a vertex line given by indices i_0...i_n:
     * residual(x) = sqrt(wLength) * (sum(norm(i_{j+1} - i_{j})) - target_length)
     */
    static DiffData<T> Evaluate(const DiffDataMatrix<T, C, -1>& v, const std::vector<std::vector<int>>& lines, const Vector<T>& target_lengths, const T wLength)
    {
        const int numConstraints = int(lines.size());

        if (numConstraints != int(target_lengths.size()))
        {
            CARBON_CRITICAL("length constraint: number of lines and target lengths not matching");
        }

        const T sqrtwLength = std::sqrt(wLength);
        Vector<T> residual(numConstraints);

        JacobianConstPtr<T> Jacobian;
        if (v.HasJacobian())
        {
            // calculate residuals + jacobians in a single loop for efficiency
            std::vector<Eigen::Triplet<float>> triplets;
            triplets.reserve(numConstraints * 20 * 6);
            for (int i = 0; i < numConstraints; i++)
            {
                residual[i] = T(0.0);
                for (size_t j = 0; j < lines[i].size() - 1; j++)
                {
                    const int& index0 = lines[i][j];
                    const int& index1 = lines[i][j + 1];
                    const Eigen::Vector3<T> segment = (v.Matrix().col(index1) - v.Matrix().col(index0));
                    const T segmentLength = segment.norm();
                    const T segmentWeight = sqrtwLength / segmentLength;
                    residual[i] += segmentLength;

                    triplets.push_back(Eigen::Triplet<T>(i, C * index0 + 0, segmentWeight * -segment[0]));
                    triplets.push_back(Eigen::Triplet<T>(i, C * index0 + 1, segmentWeight * -segment[1]));
                    triplets.push_back(Eigen::Triplet<T>(i, C * index0 + 2, segmentWeight * -segment[2]));
                    triplets.push_back(Eigen::Triplet<T>(i, C * index1 + 0, segmentWeight * segment[0]));
                    triplets.push_back(Eigen::Triplet<T>(i, C * index1 + 1, segmentWeight * segment[1]));
                    triplets.push_back(Eigen::Triplet<T>(i, C * index1 + 2, segmentWeight * segment[2]));
                }
                residual[i] -= target_lengths[i];
                residual[i] *= sqrtwLength;
            }

            // calculate actual jacobian matrix
            SparseMatrix<T> J(numConstraints, v.Size());
            J.setFromTriplets(triplets.begin(), triplets.end());
            Jacobian = v.Jacobian().Premultiply(J);
        }
        else
        {
            // calculate residuals only
            for (int i = 0; i < numConstraints; i++)
            {
                residual[i] = T(0.0);
                for (size_t j = 0; j < lines[i].size() - 1; j++)
                {
                    const int& index0 = lines[i][j];
                    const int& index1 = lines[i][j + 1];
                    residual[i] += (v.Matrix().col(index1) - v.Matrix().col(index0)).norm();
                }
                residual[i] -= target_lengths[i];
            }
        }

        return DiffData<T>(std::move(residual), Jacobian);
    }

    static DiffData<T> Evaluate(const DiffDataMatrix<T, C, -1>& v, const std::vector<std::vector<BarycentricCoordinates<T, C>>>& lines, const Vector<T>& target_lengths, const T wLength) {
        const int numConstraints = int(lines.size());

        if (numConstraints != int(target_lengths.size())) {
            CARBON_CRITICAL("length constraint: number of lines and target lengths not matching");
        }

        const T sqrtwLength = std::sqrt(wLength);
        Vector<T> residual(numConstraints);

        JacobianConstPtr<T> Jacobian;
        if (v.HasJacobian()) {
            // calculate residuals + jacobians in a single loop for efficiency
            std::vector<Eigen::Triplet<float>> triplets;
            triplets.reserve(numConstraints * 20 * 6);
            for (int i = 0; i < numConstraints; i++) {
                residual[i] = T(0.0);
                for (int j = 0; j < (int)lines[i].size() - 1; j++) {
                    const BarycentricCoordinates<T, C>& b0 = lines[i][j];
                    const BarycentricCoordinates<T, C>& b1 = lines[i][j + 1];
                    const Eigen::Vector3<T> segment = (b1.template Evaluate<C>(v.Matrix()) - b0.template Evaluate<C>(v.Matrix()));
                    const T segmentLength = segment.norm();
                    const T segmentWeight = sqrtwLength / segmentLength;
                    residual[i] += segmentLength;

                    for (int d = 0; d < C; d++) {
                        triplets.push_back(Eigen::Triplet<T>(i, C * b0.Index(d) + 0, segmentWeight * -segment[0] * b0.Weight(d)));
                        triplets.push_back(Eigen::Triplet<T>(i, C * b0.Index(d) + 1, segmentWeight * -segment[1] * b0.Weight(d)));
                        triplets.push_back(Eigen::Triplet<T>(i, C * b0.Index(d) + 2, segmentWeight * -segment[2] * b0.Weight(d)));
                        triplets.push_back(Eigen::Triplet<T>(i, C * b1.Index(d) + 0, segmentWeight * segment[0] * b1.Weight(d)));
                        triplets.push_back(Eigen::Triplet<T>(i, C * b1.Index(d) + 1, segmentWeight * segment[1] * b1.Weight(d)));
                        triplets.push_back(Eigen::Triplet<T>(i, C * b1.Index(d) + 2, segmentWeight * segment[2] * b1.Weight(d)));
                    }
                }
                residual[i] -= target_lengths[i];
                residual[i] *= sqrtwLength;
            }

            // calculate actual jacobian matrix
            SparseMatrix<T> J(numConstraints, v.Size());
            J.setFromTriplets(triplets.begin(), triplets.end());
            Jacobian = v.Jacobian().Premultiply(J);
        }
        else {
            // calculate residuals only
            for (int i = 0; i < numConstraints; i++) {
                residual[i] = T(0.0);
                for (size_t j = 0; j < lines[i].size() - 1; j++) {
                    residual[i] += (lines[i][j+1].template Evaluate<C>(v.Matrix()) - lines[i][j].template Evaluate<C>(v.Matrix())).norm();
                }
                residual[i] -= target_lengths[i];
            }
        }

        return DiffData<T>(std::move(residual), Jacobian);
    }
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
