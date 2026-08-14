// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Defs.h"

#include <nls/geometry/BarycentricCoordinates.h>
#include <nls/geometry/Mesh.h>
#include <nls/geometry/Affine.h>

using namespace TITAN_NAMESPACE;

namespace TITAN_API_NAMESPACE
{

class ReferenceAligner
{
public:
    ReferenceAligner(Mesh<float> reference,
                     BarycentricCoordinates<float> fr,
                     BarycentricCoordinates<float> rr,
                     BarycentricCoordinates<float> fl,
                     BarycentricCoordinates<float> rl);

    ~ReferenceAligner() = default;

    std::pair<float, Affine<float, 3, 3>> EstimateScaleAndRigid(const Eigen::Matrix3Xf& vertices) const;

private:
    Mesh<float> m_reference;
    BarycentricCoordinates<float> m_fr;
    BarycentricCoordinates<float> m_rr;
    BarycentricCoordinates<float> m_fl;
    BarycentricCoordinates<float> m_rl;
};

} // namespace TITAN_API_NAMESPACE
