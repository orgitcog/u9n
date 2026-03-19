// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <nls/geometry/Polyline.h>

#include <vector>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

/**
 */
template <class T, int D>
class CatmullRom
{
public:
    CatmullRom() = default;
    CatmullRom(const Eigen::Matrix<T, D, -1>& controlPoints, int pointsPerSegment, bool closed)
    {
        Set(controlPoints, pointsPerSegment, closed);
    }

    //! Sets the control points of the catmull rom curve.
    void Set(const Eigen::Matrix<T, D, -1>& controlPoints, int pointsPerSegment, bool closed);

    const Polyline<T, D>& ControlPoints() const { return m_controlPoints; }
    const Polyline<T, D>& SampledPoints() const { return m_sampledPoints; }

    //! Method to resample values at the sample points linearly based on the values at the control points.
    Eigen::Vector<T, -1> LinearResampling(const Eigen::Vector<T, -1>& values) const;

    bool IsClosed() const { return m_closed; }
    int PointsPerSegment() const { return m_pointsPerSegment; }

private:
    Polyline<T, D> m_controlPoints;
    int m_pointsPerSegment = 1;
    bool m_closed;
    Polyline<T, D> m_sampledPoints;
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
