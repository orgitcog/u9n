// Copyright Epic Games, Inc. All Rights Reserved.

#include <calib/BundleAdjustmentPerformer.h>
#include <calib/Calibration.h>
#include <calib/Utilities.h>
#include <calib/CoordinateSystemAligner.h>

#include <carbon/Common.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE::calib)

size_t objectPointsCount(Object* object)
{
    size_t count = 0;
    for (size_t i = 0; i < object->getPlaneCount(); i++)
    {
        count += object->getObjectPlane(i)->getLocalPoints().rows();
    }
    return count;
}

size_t objectPointsCount(ObjectPlane* plane) { return plane->getLocalPoints().rows(); }

Eigen::Matrix4<real_t> calculateObjectPlaneTransform(ObjectPlane* plane, const std::vector<Camera*>& cameras, size_t frameNum)
{
    for (size_t k = 0; k < cameras.size(); k++)
    {
        int isVisibleCurrFrame = cameras[k]->isPlaneVisibleOnFrame(*plane, frameNum);
        int isVisibleFirstFrame = cameras[k]->isPlaneVisibleOnFrame(*plane, 0);
        if ((isVisibleCurrFrame != -1) && (isVisibleFirstFrame != -1))
        {
            Eigen::Matrix4<real_t> objectTransform = estimatePatternTransform(cameras[k], plane, 0, frameNum);
            inverseGeometricTransform(objectTransform);

            return objectTransform;
        }
    }
    return Eigen::Matrix4<real_t>::Identity();
}

std::pair<bool, Eigen::MatrixX<real_t>> objectPlanePointsFromCameraFrame(Camera* camera, ObjectPlane* plane, int frameNum)
{
    int isVisible = camera->isPlaneVisibleOnFrame(*plane, frameNum);
    Eigen::MatrixX<real_t> planePoints;
    bool visibility;

    if (isVisible != -1)
    {
        planePoints = camera->getProjectionData()[isVisible]->getProjectionPoints();
        visibility = true;
    }
    else
    {
        size_t planePointsCount = objectPointsCount(plane);
        planePoints = Eigen::MatrixX<real_t>::Zero(planePointsCount, 2);
        visibility = false;
    }

    return std::pair<bool, Eigen::MatrixX<real_t>>(visibility, planePoints);
}

void encodeObject(const std::vector<Camera*>& cameras, Object* object, size_t frameCount, Eigen::MatrixX<real_t>& p3d, std::vector<Eigen::Matrix4<real_t>>& objectTransforms)
{
    for (size_t planeNum = 0; planeNum < object->getPlaneCount(); planeNum++)
    {
        ObjectPlane* currPlane = object->getObjectPlane(planeNum);
        for (size_t frameNum = 0; frameNum < frameCount; frameNum++)
        {
            if ((frameNum == 0) && (planeNum == 0))
            {
                objectTransforms.push_back(Eigen::Matrix4<real_t>::Identity());
                p3d = currPlane->getGlobalPoints();
            }
            else
            {
                Eigen::Matrix4<real_t> transform = calculateObjectPlaneTransform(currPlane, cameras, frameNum);
                objectTransforms.push_back(transform);
            }
        }
    }
}

void decodeObject(Object* object, size_t frameCount, const Eigen::MatrixX<real_t>&/*p3d*/, // TODO is this correct?
                  const std::vector<Eigen::Matrix4<real_t>>& transforms)
{
    const size_t planeCount = object->getPlaneCount();

    for (size_t planeNum = 0; planeNum < planeCount; planeNum++)
    {
        object->getObjectPlane(planeNum)->setNumberOfFrames(frameCount);
        for (size_t frameNum = 0; frameNum < frameCount; frameNum++)
        {
            object->getObjectPlane(planeNum)->setTransform(transforms[frameCount * planeNum + frameNum], frameNum);
        }
    }
}

void encodeCamera(Camera* camera,
                  Object* object,
                  size_t frameCount,
                  Eigen::Matrix3<real_t>& K,
                  Eigen::VectorX<real_t>& D,
                  Eigen::Matrix4<real_t>& T,
                  std::vector<Eigen::MatrixX<real_t>>& p2d,
                  std::vector<bool>& vis)
{
    K = camera->getCameraModel()->getIntrinsicMatrix();
    D = camera->getCameraModel()->getDistortionParams();
    T = camera->getWorldPosition();

    inverseGeometricTransform(T);

    for (size_t planeNum = 0; planeNum < object->getPlaneCount(); planeNum++)
    {
        for (size_t frameNum = 0; frameNum < frameCount; frameNum++)
        {
            auto points = objectPlanePointsFromCameraFrame(camera, object->getObjectPlane(planeNum), static_cast<int>(frameNum));
            vis.push_back(points.first);
            p2d.push_back(points.second);
        }
    }
}

void encodeCameras(const std::vector<Camera*>& cameras,
                   size_t frameNum,
                   Object* object,
                   std::vector<Eigen::Matrix3<real_t>>& Ks,
                   std::vector<Eigen::VectorX<real_t>>& Ds,
                   std::vector<Eigen::Matrix4<real_t>>& Ts,
                   std::vector<std::vector<Eigen::MatrixX<real_t>>>& p2d,
                   std::vector<std::vector<bool>>& vis)
{
    for (size_t i = 0; i < cameras.size(); i++)
    {
        Eigen::Matrix3<real_t> K;
        Eigen::Matrix4<real_t> T;
        std::vector<Eigen::MatrixX<real_t>> cp2d;
        std::vector<bool> cVis;
        Eigen::VectorX<real_t> D;
        encodeCamera(cameras[i], object, frameNum, K, D, T, cp2d, cVis);

        Ks.push_back(K);
        Ds.push_back(D);
        Ts.push_back(T);
        p2d.push_back(cp2d);
        vis.push_back(cVis);
    }
}

void decodeCamera(Camera* camera, const Eigen::Matrix3<real_t>& K, const Eigen::VectorX<real_t>& D, const Eigen::Matrix4<real_t>& T)
{
    CameraModel* model = camera->getCameraModel();
    Eigen::Matrix4<real_t> camT = T;

    inverseGeometricTransform(camT);
    camera->setWorldPosition(camT);
    model->setIntrinsicMatrix(K);
    model->setDistortionParams(D);
}

void decodeCameras(std::vector<Camera*>& cameras, const std::vector<Eigen::Matrix3<real_t>>& Ks, const std::vector<Eigen::VectorX<real_t>>& Ds,
                   const std::vector<Eigen::Matrix4<real_t>>& Ts)
{
    for (size_t i = 0; i < cameras.size(); i++)
    {
        decodeCamera(cameras[i],
                     Ks[i],
                     Ds[i],
                     Ts[i]);
    }
}

std::optional<real_t> BundleAdjustmentPerformer::bundleAdjustScene(Object* object, std::vector<Camera*>& cameras, BAParams& params) noexcept
{
    std::vector<Eigen::Matrix3<real_t>> Ks;
    std::vector<Eigen::Matrix4<real_t>> Ts;
    std::vector<Eigen::VectorX<real_t>> Ds;
    std::vector<std::vector<Eigen::MatrixX<real_t>>> p2d;
    size_t frameNum = params.frameNum;

    std::vector<std::vector<bool>> vis;
    std::vector<Eigen::Matrix4<real_t>> objectTransforms;
    Eigen::MatrixX<real_t> p3d;

    encodeObject(cameras, object, frameNum, p3d, objectTransforms);
    encodeCameras(cameras, frameNum, object, Ks, Ds, Ts, p2d, vis);

    std::optional<real_t> maybeMse = bundleAdjustment(p3d, objectTransforms, p2d, vis, Ks, Ts, Ds, params);
    if (!maybeMse.has_value())
    {
        LOG_WARNING("Bundle adjustment optimization failed.");
        return std::nullopt;
    }
    decodeObject(object, frameNum, p3d, objectTransforms);
    decodeCameras(cameras, Ks, Ds, Ts);

    return maybeMse;
}

CARBON_NAMESPACE_END(TITAN_NAMESPACE::calib)
