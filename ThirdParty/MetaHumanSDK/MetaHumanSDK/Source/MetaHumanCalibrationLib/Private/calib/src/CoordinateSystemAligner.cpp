// Copyright Epic Games, Inc. All Rights Reserved.

#include <calib/CoordinateSystemAligner.h>
#include <calib/Utilities.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE::calib)

std::vector<Camera*> findOverlappingCameras(const std::vector<Camera*>& cameras_left, const std::vector<Camera*>& cameras_right)
{
    std::vector<Camera*> output;

    for (size_t i = 0; i < cameras_left.size(); i++)
    {
        for (size_t j = 0; j < cameras_right.size(); j++)
        {
            if (cameras_left[i] == cameras_right[j])
            {
                output.push_back(cameras_left[i]);
            }
        }
    }

    return output;
}

void subtractOverlappingCameras(std::vector<Camera*>& lhs, const std::vector<Camera*>& rhs)
{
    std::vector<Camera*> output;
    bool ol = false;
    for (size_t i = 0; i < lhs.size(); i++)
    {
        ol = false;
        for (size_t j = 0; j < rhs.size(); j++)
        {
            if (lhs[i] == rhs[j])
            {
                ol = true;
            }
        }
        if (!ol)
        {
            output.push_back(lhs[i]);
        }
    }

    lhs = output;
}

Eigen::Matrix4<real_t> estimatePatternTransform(Camera* referent, ObjectPlane* ref, ObjectPlane* al)
{
    size_t pos_size = referent->getProjectionData().size();
    Eigen::Matrix4<real_t> rel_transform;
    Eigen::MatrixX<real_t> points_ai_2d, points_ref_2d, points_ai_3d, points_ref_3d;

    for (size_t i = 0; i < pos_size; i++)
    {
        if (ref == referent->getProjectionData()[i]->getObjectPlane())
        {
            points_ref_2d = referent->getProjectionData()[i]->getProjectionPoints();
            points_ref_3d = referent->getProjectionData()[i]->getObjectPlane()->getLocalPoints();
        }
        if (al == referent->getProjectionData()[i]->getObjectPlane())
        {
            points_ai_2d = referent->getProjectionData()[i]->getProjectionPoints();
            points_ai_3d = referent->getProjectionData()[i]->getObjectPlane()->getLocalPoints();
        }
    }

    bool transform = estimateRelativePatternTransform(points_ref_2d,
                                                             points_ref_3d,
                                                             points_ai_2d,
                                                             points_ai_3d,
                                                             referent->getCameraModel()->getIntrinsicMatrix(),
                                                             referent->getCameraModel()->getDistortionParams(),
                                                             rel_transform);
    if (!transform)
    {
        LOG_WARNING("Relative pattern transform estimation failed.");
    }

    return rel_transform;
}

Eigen::Matrix4<real_t> estimatePatternTransform(Camera* referent, ObjectPlane* plane, size_t refFrame, size_t alFrame)
{
    size_t pos_size = referent->getProjectionData().size();
    Eigen::Matrix4<real_t> rel_transform;
    Eigen::MatrixX<real_t> points_ai_2d, points_ref_2d, points_ai_3d, points_ref_3d;

    for (size_t i = 0; i < pos_size; i++)
    {
        if ((plane == referent->getProjectionData()[i]->getObjectPlane()) &&
            (refFrame == referent->getProjectionData()[i]->getImage()->getFrameId()))
        {
            points_ref_2d = referent->getProjectionData()[i]->getProjectionPoints();
            points_ref_3d = referent->getProjectionData()[i]->getObjectPlane()->getLocalPoints();
        }
        if ((plane == referent->getProjectionData()[i]->getObjectPlane()) &&
            (alFrame == referent->getProjectionData()[i]->getImage()->getFrameId()))
        {
            points_ai_2d = referent->getProjectionData()[i]->getProjectionPoints();
            points_ai_3d = referent->getProjectionData()[i]->getObjectPlane()->getLocalPoints();
        }
    }

    bool transform = estimateRelativePatternTransform(points_ref_2d,
                                                             points_ref_3d,
                                                             points_ai_2d,
                                                             points_ai_3d,
                                                             referent->getCameraModel()->getIntrinsicMatrix(),
                                                             referent->getCameraModel()->getDistortionParams(),
                                                             rel_transform);

    if (!transform)
    {
        LOG_WARNING("Relative pattern transform estimation failed.");
    }

    return rel_transform;
}

Eigen::Matrix4<real_t> estimatePatternTransform(std::vector<Camera*>& referent, ObjectPlane* ref, ObjectPlane* al)
{
    std::vector<Eigen::Matrix4<real_t>> transforms;
    Eigen::Matrix4<real_t> transform;

    for (size_t i = 0; i < referent.size(); i++)
    {
        transform = estimatePatternTransform(referent[i], ref, al);
        transforms.push_back(transform);
    }

    /*Matrix4<real_t> maybeTransform =*/averageTransformationMatrices(transforms);
    return averageTransformationMatrices(transforms);
}

std::vector<Camera*> findOverlappingCameras(ObjectPlane* ref, ObjectPlane* al, std::vector<Camera*>& cameras)
{
    std::vector<Camera*> maybe_ref_cameras = CameraManager::groupCamerasByPattern(ref, cameras);
    std::vector<Camera*> maybe_aligned_cameras = CameraManager::groupCamerasByPattern(al, cameras);
    std::vector<Camera*> ref_cameras = maybe_ref_cameras;
    std::vector<Camera*> aligned_cameras = maybe_aligned_cameras;

    return findOverlappingCameras(ref_cameras,
                                  aligned_cameras);
}

std::vector<Camera*> findOverlappingCameras(ObjectPlane* ref, ObjectPlane* al, std::vector<Camera*>& cameras, std::vector<Camera*>& ref_cameras,
                                            std::vector<Camera*>& al_cameras)
{
    std::vector<Camera*> maybe_ref_cameras = CameraManager::groupCamerasByPattern(ref, cameras);
    std::vector<Camera*> maybe_aligned_cameras = CameraManager::groupCamerasByPattern(al, cameras);
    ref_cameras = maybe_ref_cameras;
    al_cameras = maybe_aligned_cameras;

    return findOverlappingCameras(ref_cameras, al_cameras);
}

int getPlaneIdForCamera(ObjectPlane* plane, Camera* camera)
{
    for (size_t i = 0; i < camera->getProjectionData().size(); i++)
    {
        if (plane == camera->getProjectionData()[i]->getObjectPlane())
        {
            return (int)i;
        }
    }
    return -1;
}

void CoordinateSystemAligner::transformCamerasGlobal(ObjectPlane* plane, std::vector<Camera*>& cameras) noexcept
{
    for (size_t i = 0; i < cameras.size(); i++)
    {
        int plane_id = getPlaneIdForCamera(plane, cameras[i]);
        if (plane_id == -1)
        {
            continue;
        }
        Eigen::Matrix4<real_t> pTransform = cameras[i]->getProjectionData()[plane_id]->getTransform();
        inverseGeometricTransform(pTransform);

        Eigen::Matrix4<real_t> world_transform = plane->getTransform() * pTransform;
        cameras[i]->setWorldPosition(world_transform);
    }
}

bool CoordinateSystemAligner::neighborhoodCheck(ObjectPlane* ref, ObjectPlane* al, std::vector<Camera*>& cameras) noexcept
{
    std::vector<Camera*> overlap = findOverlappingCameras(ref, al, cameras);
    if (overlap.empty())
    {
        return false;
    }
    else
    {
        return true;
    }
}

void CoordinateSystemAligner::alignCoordinateSystems(ObjectPlane* ref, ObjectPlane* al, std::vector<Camera*>& cameras) noexcept
{
    std::vector<Camera*> ref_cameras, aligned_cameras;
    std::vector<Camera*> overlap = findOverlappingCameras(ref, al, cameras, ref_cameras, aligned_cameras);
    Eigen::Matrix4<real_t> p2middleman, middleman2orig, p2orig;

    if (overlap.empty())
    {
        for (size_t i = 0; i < aligned_cameras.size(); i++)
        {
            for (size_t j = 0; j < aligned_cameras[i]->getProjectionData().size(); j++)
            {
                if (aligned_cameras[i]->getProjectionData()[j]->getObjectPlane() != al)
                {
                    p2middleman = estimatePatternTransform(aligned_cameras[i],
                                                           al,
                                                           aligned_cameras[i]->getProjectionData()[j]->getObjectPlane());
                    middleman2orig = aligned_cameras[i]->getProjectionData()[j]->getObjectPlane()->getTransform();
                }
            }
        }


        p2orig = middleman2orig * p2middleman;
    }
    else
    {
        p2orig = estimatePatternTransform(overlap[0], ref, al);
    }

    inverseGeometricTransform(p2orig);
    al->setTransform(p2orig);

    subtractOverlappingCameras(aligned_cameras, ref_cameras);
    transformCamerasGlobal(al, aligned_cameras);
}

CARBON_NAMESPACE_END(TITAN_NAMESPACE::calib)
