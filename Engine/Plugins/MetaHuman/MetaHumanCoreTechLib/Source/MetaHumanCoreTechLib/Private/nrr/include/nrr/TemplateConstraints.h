// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/common/Pimpl.h>
#include <nls/DiffDataMatrix.h>
#include <nls/geometry/BarycentricCoordinates.h>
#include <nls/geometry/Mesh.h>

#include <memory>
#include <vector>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
class TemplateConstraints
{
public:
    struct Data
    {
        std::vector<int> srcIDs;
        std::vector<BarycentricCoordinates<T, 3>> targetTemplateBCs;
        Eigen::Matrix3X<T> templateDeltas;

        void Clear()
        {
            srcIDs.clear();
            targetTemplateBCs.clear();
        }

        //! Evaluate template constraints
        DiffData<T> Evaluate(const DiffDataMatrix<T, 3, -1>& srcVertices,
                             const DiffDataMatrix<T, 3, -1>& targetVertices) const;
    };

public:
    TemplateConstraints() = default;
    ~TemplateConstraints() = default;
    TemplateConstraints(TemplateConstraints&&) = default;
    TemplateConstraints(TemplateConstraints&) = delete;
    TemplateConstraints& operator=(TemplateConstraints&&) = default;
    TemplateConstraints& operator=(const TemplateConstraints&) = delete;

    void SetSourceTopology(const Mesh<T>& mesh, const std::vector<int>& srcMask);

    void SetTargetTopology(const Mesh<T>& mesh, const std::vector<int>& targetMask);

    std::shared_ptr<const Data> SetupCorrespondences(const Eigen::Matrix<T, 3, -1>& srcVertices, const Eigen::Matrix<T, 3, -1>& targetVertices) const;

protected:
    std::vector<int> m_srcMask;
    Eigen::Matrix<int, 3, -1> m_srcTriangles;
    std::vector<int> m_targetMask;
    Eigen::Matrix<int, 3, -1> m_targetTriangles;
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
