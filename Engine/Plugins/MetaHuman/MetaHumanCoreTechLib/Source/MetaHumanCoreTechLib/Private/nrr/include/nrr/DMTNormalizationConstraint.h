// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <nls/DiffDataMatrix.h>


CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
class DMTNormalizationConstraint
{
public:
    DiffData<T> EvaluateRegionsSumEquals(const DiffData<T>& vec, int numRegions)
    {
        const int numValues = vec.Size();
        const int numCharacters = numValues / numRegions;
        Vector<T> result(numRegions);
        for (int i = 0; i < numRegions; i++)
        {
            result[i] = -1.0;
            for (int j = 0; j < numCharacters; j++)
            {
                result[i] += vec.Value()[j * numRegions + i];
            }
        }
        JacobianConstPtr<T> Jacobian;
        if (vec.HasJacobian() && (vec.Jacobian().NonZeros() > 0))
        {
            std::vector<Eigen::Triplet<T>> triplets;
            triplets.reserve(numRegions * numCharacters);
            for (int i = 0; i < numRegions; ++i)
            {
                for (int j = 0; j < numCharacters; ++j)
                {
                    triplets.push_back(Eigen::Triplet<T>(i, j * numRegions + i, 1));
                }
            }

            SparseMatrix<T> localJacobian(numRegions, numRegions * numCharacters);
            localJacobian.setFromTriplets(triplets.begin(), triplets.end());
            Jacobian = vec.Jacobian().Premultiply(localJacobian);
        }
        return DiffData<T>(std::move(result), Jacobian);
    }
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
