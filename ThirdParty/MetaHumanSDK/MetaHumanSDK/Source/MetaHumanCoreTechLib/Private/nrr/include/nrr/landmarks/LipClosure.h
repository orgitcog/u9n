// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>
#include <nls/geometry/Camera.h>
#include <nls/math/Math.h>
#include <nrr/landmarks/LandmarkConfiguration.h>
#include <nrr/landmarks/LandmarkInstance.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

#ifdef _MSC_VER
// disabling warning about padded structure due to use of Eigen::Vector2f which is padded
#pragma warning(push)
#pragma warning(disable : 4324)
#endif

//! Class to calculate if a point in 2D is closest to the closed or open lips.
template <class T>
class LipClosure
{
public:
    LipClosure() = default;

    bool Valid() const { return m_valid; }
    void Reset() { m_valid = false; }

    void Init(const LandmarkInstance<T, 2>& landmarkInstance)
    {
        static const std::string ptNameLeftMouthCorner = "pt_mouth_corner_l";
        static const std::string ptNameRightMouthCorner = "pt_mouth_corner_r";
        static const std::string ptNameLeftContact = "pt_left_contact";
        static const std::string ptNameRightContact = "pt_right_contact";

        if (landmarkInstance.GetLandmarkConfiguration()->HasLandmark(ptNameLeftContact) &&
            landmarkInstance.GetLandmarkConfiguration()->HasLandmark(ptNameRightContact) &&
            landmarkInstance.GetLandmarkConfiguration()->HasLandmark(ptNameLeftMouthCorner) &&
            landmarkInstance.GetLandmarkConfiguration()->HasLandmark(ptNameRightMouthCorner))
        {
            m_cornerLeft = landmarkInstance.Point(landmarkInstance.GetLandmarkConfiguration()->IndexForLandmark(ptNameLeftMouthCorner));
            m_cornerRight = landmarkInstance.Point(landmarkInstance.GetLandmarkConfiguration()->IndexForLandmark(ptNameRightMouthCorner));
            m_contactLeft = landmarkInstance.Point(landmarkInstance.GetLandmarkConfiguration()->IndexForLandmark(ptNameLeftContact));
            m_contactRight = landmarkInstance.Point(landmarkInstance.GetLandmarkConfiguration()->IndexForLandmark(ptNameRightContact));
            const Eigen::Vector2<T> dir = m_cornerRight - m_cornerLeft;
            const T stepContactLeft = (m_contactLeft - m_cornerLeft).dot(dir.normalized()) / dir.norm();
            const T stepContactRight = (m_contactRight - m_cornerLeft).dot(dir.normalized()) / dir.norm();
            m_globalWeight = std::clamp<T>(T(1) - (stepContactRight - stepContactLeft), T(0), T(1));
            m_valid = true;
        }
        else
        {
            m_valid = false;
        }
    }

    /**
     * @brief Calculates if a point @p pt is closest to closed or open lips
     * @return T(1) if lips are closed, or T(0) if lips are open.
     */
    T ClosureValue(const Eigen::Vector2<T>& pt) const
    {
        if (!m_valid) { return T(0); }

        const Eigen::Vector2<T> dir = m_cornerRight - m_cornerLeft;
        const T stepContactLeft = (m_contactLeft - m_cornerLeft).dot(dir.normalized()) / dir.norm();
        const T stepContactRight = (m_contactRight - m_cornerLeft).dot(dir.normalized()) / dir.norm();
        const T step = (pt - m_cornerLeft).dot(dir.normalized()) / dir.norm();

        if ((step > stepContactLeft) && (step < stepContactRight))
        {
            return T(0);
        }
        else
        {
            return m_globalWeight;
        }
    }

private:
    Eigen::Vector2<T> m_cornerLeft;
    Eigen::Vector2<T> m_cornerRight;
    Eigen::Vector2<T> m_contactLeft;
    Eigen::Vector2<T> m_contactRight;
    bool m_valid = false;
    T m_globalWeight = 0;
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

//! Class to calculate if a point in 3D, when projected into multiple 2D landmarks, whether it projects to open or closed lips.
template <class T>
class LipClosure3D
{
public:
    LipClosure3D() = default;

    void Reset()
    {
        m_lipClosures.clear();
        m_cameras.clear();
    }

    bool Valid() const { return (m_lipClosures.size() > 0); }

    void Add(const LandmarkInstance<T, 2>& landmarkInstance, const Camera<T>& camera)
    {
        LipClosure<T> lipClosure;
        lipClosure.Init(landmarkInstance);
        if (lipClosure.Valid())
        {
            m_lipClosures.emplace_back(std::move(lipClosure));
            m_cameras.push_back(camera);
        }
    }

    /**
     * @brief Calculates if a 3d point @p pt, projected into each image and measured for 2D lip closure.
     * @return T(1) if lips are closed, or T(0) if lips are open.
     */
    T ClosureValue(const Eigen::Vector3<T>& pt) const
    {
        if (m_lipClosures.empty()) { return T(0); }

        T closure = m_lipClosures.front().ClosureValue(m_cameras.front().Project(pt, /*withExtrinsics=*/true));
        for (size_t i = 1; i < m_lipClosures.size(); ++i)
        {
            closure = std::min<T>(closure, m_lipClosures[i].ClosureValue(m_cameras[i].Project(pt, /*withExtrinsics=*/true)));
        }
        return closure;
    }

private:
    std::vector<LipClosure<T>> m_lipClosures;
    std::vector<Camera<T>> m_cameras;
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
