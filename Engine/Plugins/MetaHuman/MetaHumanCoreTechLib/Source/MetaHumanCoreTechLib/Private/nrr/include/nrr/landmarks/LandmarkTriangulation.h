// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>
#include <carbon/geometry/AABBTree.h>
#include <nls/geometry/Camera.h>
#include <nls/geometry/BarycentricCoordinates.h>
#include <nls/geometry/Mesh.h>
#include <nls/geometry/MultiCameraTriangulation.h>
#include <nls/geometry/RayTriangleIntersection.h>
#include <nrr/landmarks/LandmarkInstance.h>
#include <nls/geometry/MultiCameraSetup.h>

#include <limits>
#include <map>
#include <set>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

/**
 * Triangulate landmarks in 3D.
 */
template <class T>
std::map<std::string, Eigen::Vector3<T>> TriangulateLandmarks(const MultiCameraSetup<T>& cameraSetup,
                                                              const std::map<std::string, LandmarkInstance<T, 2>>& landmarkInstances)
{
    std::map<std::string, Eigen::Vector3<T>> reconstructedLandmarkPositions;

    if (landmarkInstances.size() > 1)
    {
        std::set<std::string> landmarkNames;
        for (const auto& [_, landmarkInstance] : landmarkInstances)
        {
            if (landmarkInstance.GetLandmarkConfiguration() == nullptr)
            {
                CARBON_CRITICAL("landmark configuration is not set for the landmark instance");
            }
            for (const auto& [landmarkName, _2] : landmarkInstance.GetLandmarkConfiguration()->LandmarkMapping())
            {
                landmarkNames.emplace(landmarkName);
            }
        }

        for (const std::string& landmarkName : landmarkNames)
        {
            std::vector<Eigen::Vector2f> pixels;
            std::vector<T> confidences;

            std::vector<Camera<T>> undistortedLandmarkCameras;

            for (const auto&[cameraName, landmarkInstance] : landmarkInstances)
            {
                if (landmarkInstance.GetLandmarkConfiguration()->HasLandmark(landmarkName))
                {
                    const int landmarkIndex = landmarkInstance.GetLandmarkConfiguration()->IndexForLandmark(landmarkName);
                    if (landmarkInstance.Confidence()[landmarkIndex] > 0)
                    {
                        undistortedLandmarkCameras.push_back(cameraSetup.GetCamera(cameraName)); // converting MetaShapeCamera to Camera implicitly
                        pixels.push_back(landmarkInstance.Points().col(landmarkIndex));
                        confidences.push_back(landmarkInstance.Confidence()[landmarkIndex]);
                    }
                }
            }

            if (pixels.size() > 1)
            {
                MultiCameraTriangulation<T> multiCameraTriangulation;
                multiCameraTriangulation.SetCameras(undistortedLandmarkCameras);
                Eigen::Vector3f pos = multiCameraTriangulation.Triangulate(pixels);
                pos = multiCameraTriangulation.TriangulateNonlinear(pos, pixels, confidences);
                reconstructedLandmarkPositions[landmarkName] = pos;
            }
        }
    }
    else
    {
        CARBON_CRITICAL("at least two cameras with landmarks are required for triangulation");
    }

    return reconstructedLandmarkPositions;
}

/**
 * Triangulate landmarks by intersecting the 2D landmarks with the Mesh
 */
template <class T>
std::map<std::string, Eigen::Vector3<T>> TriangulateLandmarksViaRayCasting(const Camera<T>& camera,
                                                                           const LandmarkInstance<T, 2>& landmarkInstance,
                                                                           const Mesh<T>& mesh)
{
    if (landmarkInstance.GetLandmarkConfiguration() == nullptr)
    {
        CARBON_CRITICAL("landmark configuration is not set for the landmark instance");
    }

    std::map<std::string, Eigen::Vector3<T>> reconstructedLandmarkPositions;

    for (const auto& [landmarkName, landmarkIndex] : landmarkInstance.GetLandmarkConfiguration()->LandmarkMapping())
    {
        const Eigen::Vector3<T> origin = camera.Origin();
        const Eigen::Vector3<T> direction = camera.Unproject(landmarkInstance.Points().col(landmarkIndex), 1.0f, /*withExtrinsics=*/true) - origin;
        T bestAlpha = std::numeric_limits<T>::max();
        Eigen::Vector3<T> bestPos { 0, 0, 0 };
        for (int j = 0; j < mesh.NumTriangles(); ++j)
        {
            const Eigen::Vector3<T> v1 = mesh.Vertices().col(mesh.Triangles()(0, j));
                                                             const Eigen::Vector3<T> v2 = mesh.Vertices().col(mesh.Triangles()(1, j));
                                                                                                              const Eigen::Vector3<T> v3 = mesh.Vertices().col(
                                                                                                                  mesh.Triangles()(2, j));
                                                                                                                  Eigen
                                                                                                                  ::Vector3<T> pos;
                                                                                                                  T
                                                                                                                      alpha;
                                                                                                                  if (
                                                                                                                      RayTriangleIntersection(origin, direction,
                                                                                                                                              v1, v2, v3,
                                                                                                                                              &alpha, &pos))
                {
                    if (alpha < bestAlpha)
                    {
                        bestPos = pos;
                        bestAlpha = alpha;
                    }
                }
        }
        if (bestAlpha < std::numeric_limits<T>::max())
        {
            reconstructedLandmarkPositions[landmarkName] = bestPos;
        }
    }

    return reconstructedLandmarkPositions;
}

/**
 * Triangulate landmarks by intersecting the 2D landmarks with the Mesh using AABB tree
 */
template <class T>
std::map<std::string, std::pair<Eigen::Vector3<T>, bool>> TriangulateLandmarksViaAABB(const Camera<T>& camera,
                                                                                      const LandmarkInstance<T, 2>& landmarkInstance,
                                                                                      const Mesh<T>& mesh)
{
    if (landmarkInstance.GetLandmarkConfiguration() == nullptr)
    {
        CARBON_CRITICAL("landmark configuration is not set for the landmark instance");
    }

    const auto aabbTree = std::make_shared<TITAN_NAMESPACE::AABBTree<T>>(mesh.Vertices().transpose(), mesh.Triangles().transpose());
    std::map<std::string, std::pair<Eigen::Vector3<T>, bool>> reconstructedLandmarkPositions;

    for (const auto& [landmarkName, landmarkIndex] : landmarkInstance.GetLandmarkConfiguration()->LandmarkMapping())
    {
        const Eigen::Vector3<T> origin = camera.Origin();
        const Eigen::Vector3<T> direction = camera.Unproject(landmarkInstance.Points().col(landmarkIndex), 1.0f, /*withExtrinsics=*/true) - origin;
        const auto [triangleIndex, barycentricCoordinates, distance] = aabbTree->intersectRay(origin.transpose(), direction.transpose());

        if (triangleIndex >= 0)
        {
            const Eigen::Vector3i triangle = mesh.Triangles().col(triangleIndex);
            const Eigen::Matrix3X<T> vertices = mesh.Vertices();

            BarycentricCoordinates<T> bc(triangle, barycentricCoordinates.transpose());
            reconstructedLandmarkPositions[landmarkName] = std::pair<Eigen::Vector3<T>, bool>(bc.template Evaluate<3>(vertices), true);
        }
        else
        {
            reconstructedLandmarkPositions[landmarkName] = std::pair<Eigen::Vector3<T>, bool>(Eigen::Vector3<T>(T(0), T(0), T(0)), false);
        }
    }

    return reconstructedLandmarkPositions;
}

/**
 * Triangulate landmarks by intersecting the 2D landmarks with the Mesh
 */
template <class T>
std::map<std::string, Eigen::Matrix3X<T>> TriangulateCurvesViaRayCasting(const Camera<T>& camera,
                                                                         const LandmarkInstance<T, 2>& landmarkInstance,
                                                                         const Mesh<T>& mesh)
{
    if (landmarkInstance.GetLandmarkConfiguration() == nullptr)
    {
        CARBON_CRITICAL("landmark configuration is not set for the landmark instance");
    }

    std::map<std::string, Eigen::Matrix3X<T>> reconstructedCurvePointsPositions;

    for (const auto& [curveName, curveIndices] : landmarkInstance.GetLandmarkConfiguration()->CurvesMapping())
    {
        const Eigen::Vector3<T> origin = camera.Origin();
        Eigen::Matrix3X<T> curvePoints = Eigen::Matrix3X<T>(3, curveIndices.size());

        for (int i = 0; i < int(curveIndices.size()); i++)
        {
            const Eigen::Vector3<T> direction = camera.Unproject(landmarkInstance.Points().col(curveIndices[i]), 1.0f, /*withExtrinsics=*/true) - origin;
            T bestAlpha = std::numeric_limits<T>::max();
            Eigen::Vector3<T> bestPos;
            for (int j = 0; j < mesh.NumTriangles(); ++j)
            {
                const Eigen::Vector3<T> v1 = mesh.Vertices().col(mesh.Triangles()(0, j));
                                                                 const Eigen::Vector3<T> v2 = mesh.Vertices().col(mesh.Triangles()(1, j));
                                                                                                                  const Eigen::Vector3<T> v3 =
                                                                                                                      mesh.Vertices().col(mesh.Triangles()(2,
                                                                                                                                                           j));
                                                                                                                                          Eigen
                                                                                                                                          ::Vector3<T> pos;
                                                                                                                                          T
                                                                                                                                              alpha;
                                                                                                                                          if (
                                                                                                                                              RayTriangleIntersection(
                                                                                                                                                  origin,
                                                                                                                                                  direction, v1,
                                                                                                                                                  v2, v3,
                                                                                                                                                  &alpha, &pos))
                    {
                        if (alpha < bestAlpha)
                        {
                            bestPos = pos;
                            bestAlpha = alpha;
                        }
                    }
            }

            if (bestAlpha < std::numeric_limits<T>::max())
            {
                curvePoints.col(i) = bestPos;
            }
        }

        reconstructedCurvePointsPositions[curveName] = curvePoints;
    }

    return reconstructedCurvePointsPositions;
}

/**
 * Triangulate landmarks by intersecting the 2D curves with the Mesh using AABB tree
 */
template <class T>
std::map<std::string, std::pair<Eigen::Matrix3X<T>, std::vector<bool>>> TriangulateCurvesViaAABB(const Camera<T>& camera,
                                                                                                 const LandmarkInstance<T, 2>& landmarkInstance,
                                                                                                 const Mesh<T>& mesh)
{
    if (landmarkInstance.GetLandmarkConfiguration() == nullptr)
    {
        CARBON_CRITICAL("landmark configuration is not set for the landmark instance");
    }

    const auto aabbTree = std::make_shared<TITAN_NAMESPACE::AABBTree<T>>(mesh.Vertices().transpose(), mesh.Triangles().transpose());
    std::map<std::string, std::pair<Eigen::Matrix3X<T>, std::vector<bool>>> reconstructedCurvePointsPositions;

    for (const auto& [curveName, curveIndices] : landmarkInstance.GetLandmarkConfiguration()->CurvesMapping())
    {
        const Eigen::Vector3<T> origin = camera.Origin();
        Eigen::Matrix3X<T> curvePoints = Eigen::Matrix3X<T>(3, curveIndices.size());
        std::vector<bool> intersectionSuccess(curveIndices.size());

        for (int i = 0; i < int(curveIndices.size()); i++)
        {
            const Eigen::Vector3<T> direction = camera.Unproject(landmarkInstance.Points().col(curveIndices[i]), 1.0f, /*withExtrinsics=*/true) - origin;
            const auto [triangleIndex, barycentricCoordinates, distance] = aabbTree->intersectRay(origin.transpose(), direction.transpose());

            if (triangleIndex >= 0)
            {
                const Eigen::Vector3i triangle = mesh.Triangles().col(triangleIndex);
                const Eigen::Matrix3X<T> vertices = mesh.Vertices();

                BarycentricCoordinates<T> bc(triangle, barycentricCoordinates.transpose());
                curvePoints.col(i) = bc.template Evaluate<3>(vertices);
                intersectionSuccess[i] = true;
            }
            else
            {
                curvePoints.col(i) = Eigen::Vector3<T>(T(0), T(0), T(0));
                intersectionSuccess[i] = false;
            }
        }
        reconstructedCurvePointsPositions[curveName] = std::pair<Eigen::Matrix3X<T>, std::vector<bool>>(curvePoints, intersectionSuccess);
    }

    return reconstructedCurvePointsPositions;
}

/**
 * Triangulate landmarks by looking up the position in a depthmap
 */
template <class T>
std::map<std::string, Eigen::Vector3<T>> TriangulateLandmarksViaDepthmap(const Camera<T>& camera,
                                                                         const LandmarkInstance<T, 2>& landmarkInstance,
                                                                         const Camera<T>& depthmapCamera,
                                                                         const Eigen::Matrix<T, 4, -1>& depthAndNormals)
{
    if (landmarkInstance.GetLandmarkConfiguration() == nullptr)
    {
        CARBON_CRITICAL("landmark configuration is not set for the landmark instance");
    }

    if ((camera.Origin() - depthmapCamera.Origin()).norm() > 1e-4)
    {
        CARBON_CRITICAL("camera and depthmap camera are not at the same position");
    }

    std::map<std::string, Eigen::Vector3<T>> reconstructedLandmarkPositions;

    for (const auto& [landmarkName, landmarkIndex] : landmarkInstance.GetLandmarkConfiguration()->LandmarkMapping())
    {
        const Eigen::Vector2<T> pix = depthmapCamera.Project(camera.Unproject(landmarkInstance.Points().col(landmarkIndex), 1.0f, /*withExtrinsics=*/true),
                                                             /*withExtrinsics=*/true);
        const int ix = static_cast<int>(pix[0]);
        const int iy = static_cast<int>(pix[1]);
        if ((ix >= 0) && (ix < depthmapCamera.Width()) && (iy >= 0) && (iy < depthmapCamera.Height()))
        {
            const T depth = depthAndNormals(0, iy * depthmapCamera.Width() + ix);
            if (depth > 0)
            {
                reconstructedLandmarkPositions.emplace(landmarkName, depthmapCamera.Unproject(pix, depth, /*withExtrinsics=*/true));
            }
        }
    }

    return reconstructedLandmarkPositions;
}

/**
 * Triangulate raw 2D points by looking up the position in a depthmap.
 */
template <class T>
Eigen::Matrix<T, 3, -1> TriangulatePointsViaDepthmap(const Camera<T>& camera,
                                                     const Eigen::Matrix<T, 2, -1>& points,
                                                     const Camera<T>& depthmapCamera,
                                                     const Eigen::Matrix<T, 4, -1>& depthAndNormals,
                                                     std::vector<bool>& triangulatedPointValidityFlags)
{
    if ((camera.Origin() - depthmapCamera.Origin()).norm() > 1e-4)
    {
        CARBON_CRITICAL("camera and depthmap camera are not at the same position");
    }

    Eigen::Matrix<T, 3, -1> reconstructedPointPositions(3, points.cols());
    triangulatedPointValidityFlags.resize(points.cols());

    for (int i = 0; i < points.cols(); i++)
    {
        const Eigen::Vector2<T> pix = depthmapCamera.Project(camera.Unproject(points.col(i), 1.0f, /*withExtrinsics=*/true), /*withExtrinsics=*/true);
        const int ix = static_cast<int>(pix[0]);
        const int iy = static_cast<int>(pix[1]);
        if ((ix >= 0) && (ix < depthmapCamera.Width()) && (iy >= 0) && (iy < depthmapCamera.Height()))
        {
            const T depth = depthAndNormals(0, iy * depthmapCamera.Width() + ix);
            if (depth > 0)
            {
                reconstructedPointPositions.col(i) = depthmapCamera.Unproject(pix, depth, /*withExtrinsics=*/true);
                triangulatedPointValidityFlags[i] = true;
            }
            else
            {
                triangulatedPointValidityFlags[i] = false;
            }
        }
    }

    return reconstructedPointPositions;
}

/**
 * Triangulate raw 2D points by intersecting the 2D landmarks with the Mesh
 */
template <class T>
Eigen::Matrix<T, 3, -1> TriangulatePointsViaRayCasting(const Camera<T>& camera,
                                                       const Eigen::Matrix<T, 2, -1>& points,
                                                       const Mesh<T>& mesh,
                                                       std::vector<bool>& triangulatedPointValidityFlags)
{
    Eigen::Matrix<T, 3, -1> reconstructedPointPositions(3, points.cols());
    triangulatedPointValidityFlags.resize(points.cols());

    for (int i = 0; i < points.cols(); i++)
    {
        const Eigen::Vector3<T> origin = camera.Origin();
        const Eigen::Vector3<T> direction = camera.Unproject(points.col(i), 1.0f, /*withExtrinsics=*/true) - origin;
        T bestAlpha = std::numeric_limits<T>::max();
        Eigen::Vector3<T> bestPos { 0, 0, 0 };
        for (int j = 0; j < mesh.NumTriangles(); ++j)
        {
            const Eigen::Vector3<T> v1 = mesh.Vertices().col(mesh.Triangles()(0, j));
                                                             const Eigen::Vector3<T> v2 = mesh.Vertices().col(mesh.Triangles()(1, j));
                                                                                                              const Eigen::Vector3<T> v3 = mesh.Vertices().col(
                                                                                                                  mesh.Triangles()(2, j));
                                                                                                                  Eigen
                                                                                                                  ::Vector3<T> pos;
                                                                                                                  T
                                                                                                                      alpha;
                                                                                                                  if (
                                                                                                                      RayTriangleIntersection(origin, direction,
                                                                                                                                              v1, v2, v3,
                                                                                                                                              &alpha, &pos))
                {
                    if (alpha < bestAlpha)
                    {
                        bestPos = pos;
                        bestAlpha = alpha;
                    }
                }
        }
        if (bestAlpha < std::numeric_limits<T>::max())
        {
            reconstructedPointPositions.col(i) = bestPos;
            triangulatedPointValidityFlags[i] = true;
        }
        else
        {
            triangulatedPointValidityFlags[i] = false;
        }
    }

    return reconstructedPointPositions;
}

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
