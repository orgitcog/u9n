// Copyright Epic Games, Inc. All Rights Reserved.

#include <conformer/BrowLandmarksGenerator.h>
#include <nls/geometry/Polyline.h>
#include <carbon/Common.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
BrowLandmarksGenerator<T>::BrowLandmarksGenerator() {}

template <class T> BrowLandmarksGenerator<T>::~BrowLandmarksGenerator() = default;

template <class T>
void BrowLandmarksGenerator<T>::Init(const TemplateDescription& templateDesc)
{
    m_topology = templateDesc.Topology();
    m_templateMeshLandmarks = templateDesc.GetMeshLandmarks();
    const SymmetryMapping& symmetryMapping = templateDesc.GetSymmetryMapping();
    m_browMaskL = templateDesc.GetVertexWeights("brow_mask_l").NonzeroVertices();
    for (int vID : m_browMaskL)
    {
        m_browMaskR.push_back(symmetryMapping.Map(vID));
    }
}

template <class T>
void BrowLandmarksGenerator<T>::SetLandmarks(const std::pair<LandmarkInstance<T, 2>, Camera<T>>& landmarks) { m_landmarks = landmarks; }

template <class T>
MeshLandmarks<T> BrowLandmarksGenerator<T>::Generate(const Eigen::Matrix<T, 3, -1>& vertices,
                                                                const Affine<T, 3, 3>& mesh2scanTransform,
                                                                const T mesh2ScanScale,
                                                                bool concatenate)
{
    const std::vector<std::pair<int, int>> browEdgesL = m_topology.GetEdges(m_browMaskL);
    const std::vector<std::pair<int, int>> browEdgesR = m_topology.GetEdges(m_browMaskR);

    const auto& landmarks = m_landmarks.first;
    const auto& camera = m_landmarks.second;
    std::shared_ptr<const LandmarkConfiguration> landmarkConfiguration = landmarks.GetLandmarkConfiguration();

    MeshLandmarks<T> browLandmarks;
    MeshLandmarks<T> allHeadMeshLandmarks = m_templateMeshLandmarks;
    for (bool left : { true, false })
    {
        for (const std::string curveName : { "crv_brow_lower", "crv_brow_upper" })
        {
            const std::string extCurveName = curveName + (left ? "_l" : "_r");
            const std::vector<std::pair<int, int>>& browEdges = left ? browEdgesL : browEdgesR;

            if (landmarkConfiguration->HasCurve(extCurveName))
            {
                const Eigen::Matrix<T, 2, -1> curvePts = landmarks.Points(landmarkConfiguration->IndicesForCurve(extCurveName));
                const Polyline<T, 2> polyline(curvePts);
                std::vector<BarycentricCoordinates<T>> barycentricCoordinates;
                for (const auto& [vID0, vID1] : browEdges)
                {
                    const Eigen::Vector2<T> pix0 = camera.Project(Eigen::Vector3<T>(mesh2ScanScale * mesh2scanTransform.Transform(vertices.col(vID0))),
                                                                  /*withExtrinsics=*/true);
                    const Eigen::Vector2<T> pix1 = camera.Project(Eigen::Vector3<T>(mesh2ScanScale * mesh2scanTransform.Transform(vertices.col(vID1))),
                                                                  /*withExtrinsics=*/true);

                    const std::vector<T> intersections = polyline.FindIntersections(pix0, pix1);
                    if (intersections.size() > 0)
                    {
                        const T alpha = intersections[0];
                        barycentricCoordinates.push_back(BarycentricCoordinates<T>(Eigen::Vector3i(vID0, vID1, vID1),
                                                                                   Eigen::Vector3<T>((1.0f - alpha), alpha, 0)));
                    }
                }
                browLandmarks.AddCurve(extCurveName, barycentricCoordinates);
                if (allHeadMeshLandmarks.HasCurve(extCurveName))
                {
                    LOG_WARNING("Template mesh landmarks already contain curve {}", extCurveName);
                }
                allHeadMeshLandmarks.AddCurve(extCurveName, barycentricCoordinates);
            }
        }
    }

    return concatenate ? allHeadMeshLandmarks : browLandmarks;
}

template class BrowLandmarksGenerator<float>;

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
