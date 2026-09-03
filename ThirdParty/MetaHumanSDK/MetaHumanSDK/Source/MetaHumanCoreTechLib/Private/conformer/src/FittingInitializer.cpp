// Copyright Epic Games, Inc. All Rights Reserved.

#include <conformer/FittingInitializer.h>
#include <nrr/landmarks/LandmarkTriangulation.h>
#include <nls/geometry/Procrustes.h>
#include <carbon/geometry/AABBTree.h> 

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
struct FittingInitializer<T>::Private
{
    //! mesh to project landmarks
    std::vector<std::shared_ptr<const Mesh<T>>> scanMeshes;

    std::vector<Affine<T, 3, 3>> headToScanTransforms;

    std::vector<std::vector<std::pair<LandmarkInstance<T, 2>, Camera<T>>>> targetLandmarks;
};

template <class T>
FittingInitializer<T>::FittingInitializer() : m(new Private) {}
template <class T> FittingInitializer<T>::~FittingInitializer() = default;
template <class T> FittingInitializer<T>::FittingInitializer(FittingInitializer&& other) = default;
template <class T> FittingInitializer<T>& FittingInitializer<T>::operator=(FittingInitializer&& other) = default;

template <class T>
void FittingInitializer<T>::SetTargetLandmarks(const std::vector<std::vector<std::pair<LandmarkInstance<T, 2>, Camera<T>>>>& landmarks) {
    m->targetLandmarks = landmarks;
}

template <class T>
void FittingInitializer<T>::SetTargetMeshes(const std::vector<std::shared_ptr<const Mesh<T>>>& meshes) { m->scanMeshes = meshes; }

template <class T>
void FittingInitializer<T>::SetToScanTransforms(const std::vector<Affine<T, 3, 3>>& transforms) { m->headToScanTransforms = transforms; }

template <class T>
bool FittingInitializer<T>::InitializeFace(std::vector<Affine<T, 3, 3>>& toScanTransform,
                                           std::vector<T>& toScanScale,
                                           const std::map<std::string,
                                                          Eigen::Vector3<T>>& currentMeshLandmarks,
                                           bool withScale) const
{
    CARBON_ASSERT(toScanTransform.size() == m->targetLandmarks.size(), "initialize face inputs mismatch");
    CARBON_ASSERT(toScanTransform.size() == m->scanMeshes.size(), "initialize face inputs mismatch");
    CARBON_ASSERT(toScanTransform.size() == toScanScale.size(), "initialize face inputs mismatch");

    for (int i = 0; i < (int)m->targetLandmarks.size(); ++i)
    {
        // TODO: for now, we use landmarks of the first camera, assuming that it contains proper landmarks
        const auto triangulatedLandmarks = TriangulateLandmarksViaAABB(m->targetLandmarks[i][0].second,
                                                                                  m->targetLandmarks[i][0].first,
                                                                                  *m->scanMeshes[i]);

        std::vector<Eigen::Vector3<T>> srcPts;
        std::vector<Eigen::Vector3<T>> targetPts;
        for (const auto& [landmarkName, srcPt] : currentMeshLandmarks)
        {
            auto targetIt = triangulatedLandmarks.find(landmarkName);
            if (targetIt != triangulatedLandmarks.end())
            {
                if (targetIt->second.second)
                {
                    srcPts.push_back(srcPt);
                    targetPts.push_back(targetIt->second.first);
                }
            }
        }
        if (srcPts.size() < 3)
        {
            LOG_INFO("cannot estimate the initial rigid alignment as there are not sufficient triangulated landmarks (only {})", srcPts.size());
            return false;
        }

        Eigen::Matrix<T, 3, -1> srcPtsMap = Eigen::Map<const Eigen::Matrix3X<T>>((const T*)srcPts.data(), 3, srcPts.size());
        Eigen::Matrix<T, 3, -1> targetPtsMap = Eigen::Map<const Eigen::Matrix3X<T>>((const T*)targetPts.data(), 3, targetPts.size());

        if (withScale)
        {
            const auto [scale, transform] = Procrustes<T, 3>::AlignRigidAndScale(srcPtsMap, targetPtsMap);
            toScanTransform[i] = transform;
            toScanScale[i] = scale;
        }
        else
        {
            toScanTransform[i] = Procrustes<T, 3>::AlignRigid(srcPtsMap, targetPtsMap);
            toScanScale[i] = T(1);
        }
    }

    return true;
}

template <class T>
bool FittingInitializer<T>::InitializeTeeth(Affine<T, 3, 3>& teethToHeadTransform,
                                            const std::map<std::string, Eigen::Vector3<T>>& currentMeshLandmarks,
                                            int frame) const
{
    // TODO: hardcoded for first camera
    const auto triangulatedLandmarks = TriangulateLandmarksViaAABB(m->targetLandmarks[frame][0].second,
                                                                              m->targetLandmarks[frame][0].first,
                                                                              *m->scanMeshes[frame]);
    const std::string toothLandmarkPrefix = "pt_tooth";
    std::vector<Eigen::Vector3<T>> srcPts;
    std::vector<Eigen::Vector3<T>> targetPts;
    for (const auto& [landmarkName, srcPt] : currentMeshLandmarks)
    {
        if (landmarkName.find(toothLandmarkPrefix) != std::string::npos)
        {
            auto targetIt = triangulatedLandmarks.find(landmarkName);
            if (targetIt != triangulatedLandmarks.end())
            {
                if (targetIt->second.second)
                {
                    srcPts.push_back(srcPt);
                    targetPts.push_back(targetIt->second.first);
                }
            }
        }
    }
    if (srcPts.size() < 1)
    {
        LOG_INFO("cannot initialize teeth rigid since there are no corresponding landmarks");
        return false;
    }

    Eigen::Matrix<T, 3, -1> srcPtsMap = Eigen::Map<const Eigen::Matrix3X<T>>((const T*)srcPts.data(), 3, srcPts.size());
    Eigen::Matrix<T, 3, -1> targetPtsMap = Eigen::Map<const Eigen::Matrix3X<T>>((const T*)targetPts.data(), 3, targetPts.size());

    // get projected data points in the current MH head space
    Eigen::Matrix<T, 3, -1> teethTargetPtsTransformed = m->headToScanTransforms[frame].Inverse().Transform(targetPtsMap);

    // calculate translation - initial eye2head transform to initialize fitting
    Affine<T, 3, 3> newTeethToMeshTransform;
    newTeethToMeshTransform.SetTranslation(teethTargetPtsTransformed.rowwise().mean() - srcPtsMap.rowwise().mean());

    teethToHeadTransform = newTeethToMeshTransform;

    return true;
}

template <class T>
bool FittingInitializer<T>::InitializeEyes(Affine<T, 3, 3>& leftEyeToHead,
                                           Affine<T, 3, 3>& rightEyeToHead,
                                           const std::map<std::string,
                                                          std::vector<Eigen::Vector3<T>>>& leftCurves,
                                           const std::map<std::string, std::vector<Eigen::Vector3<T>>>& rightCurves,
                                           int frame) const
{
    // TODO: for now, we use landmarks of the first camera, assuming that it contains proper landmarks
    const auto triangulatedCurves =
        TriangulateCurvesViaAABB(m->targetLandmarks[frame][0].second, m->targetLandmarks[frame][0].first, *m->scanMeshes[frame]);

    const auto curveNameLeft = "crv_iris_l";
    const auto curveNameRight = "crv_iris_r";

    if (leftCurves.find(curveNameLeft) == leftCurves.end())
    {
        LOG_INFO("No mesh curve {}.", curveNameLeft);
        return false;
    }
    if (rightCurves.find(curveNameRight) == rightCurves.end())
    {
        LOG_INFO("No mesh curve {}.", curveNameRight);
        return false;
    }

    const auto leftEyeMeshCurve = leftCurves.at(curveNameLeft);
    const auto rightEyeMeshCurve = rightCurves.at(curveNameRight);

    Eigen::Matrix<T, 3, -1> leftEyeMeshPts = Eigen::Map<const Eigen::Matrix3X<T>>((const T*)leftEyeMeshCurve.data(), 3, leftEyeMeshCurve.size());
    Eigen::Matrix<T, 3, -1> rightEyeMeshPts = Eigen::Map<const Eigen::Matrix3X<T>>((const T*)rightEyeMeshCurve.data(), 3, rightEyeMeshCurve.size());

    if (triangulatedCurves.find(curveNameLeft) == triangulatedCurves.end())
    {
        LOG_INFO("No target curve {}.", curveNameLeft);
        return false;
    }
    if (triangulatedCurves.find(curveNameRight) == triangulatedCurves.end())
    {
        LOG_INFO("No target curve {}.", curveNameRight);
        return false;
    }

    const auto [irisLeftTriangulated, irisLeftSuccess] = triangulatedCurves.at(curveNameLeft);
    const auto [irisRightTriangulated, irisRightSuccess] = triangulatedCurves.at(curveNameRight);

    std::vector<Eigen::Vector3f> leftIrisTargets, rightIrisTargets;
    for (int i = 0; i < (int)irisLeftSuccess.size(); ++i)
    {
        if (irisLeftSuccess[i])
        {
            leftIrisTargets.push_back(irisLeftTriangulated.col(i));
        }
    }
    for (int i = 0; i < (int)irisRightSuccess.size(); ++i)
    {
        if (irisRightSuccess[i])
        {
            rightIrisTargets.push_back(irisRightTriangulated.col(i));
        }
    }

    if ((leftIrisTargets.size() < 1) || (rightIrisTargets.size() < 1))
    {
        LOG_INFO("cannot estimate initial translation for eyes as there are no sufficient triangulated landmarks.");
        return false;
    }

    Eigen::Matrix<T, 3, -1> leftEyeTargetPts = Eigen::Map<const Eigen::Matrix3X<T>>((const T*)leftIrisTargets.data(), 3, leftIrisTargets.size());
    Eigen::Matrix<T, 3, -1> rightEyeTargetPts = Eigen::Map<const Eigen::Matrix3X<T>>((const T*)rightIrisTargets.data(), 3, rightIrisTargets.size());

    // get projected data points in the current MH head space
    Eigen::Matrix<T, 3, -1> leftEyeTargetPtsTransformed = m->headToScanTransforms[frame].Inverse().Transform(leftEyeTargetPts);
    Eigen::Matrix<T, 3, -1> rightEyeTargetPtsTransformed = m->headToScanTransforms[frame].Inverse().Transform(rightEyeTargetPts);

    // calculate translation - initial eye2head transform to initialize fitting
    Affine<T, 3, 3> newLeftEyeToHead, newRightEyeToHead;
    newLeftEyeToHead.SetTranslation(leftEyeTargetPtsTransformed.rowwise().mean() - leftEyeMeshPts.rowwise().mean());
    newRightEyeToHead.SetTranslation(rightEyeTargetPtsTransformed.rowwise().mean() - rightEyeMeshPts.rowwise().mean());

    leftEyeToHead = newLeftEyeToHead;
    rightEyeToHead = newRightEyeToHead;

    return true;
}


namespace fitting_tools
{

template <class T>
std::shared_ptr<const CorrespondenceData<T>> FindClosestCorrespondences(const std::shared_ptr<const Mesh<T>>& src,
    const std::shared_ptr<const Mesh<T>>& tgt,
    const VertexWeights<T>& srcWeights)
{
    std::shared_ptr<CorrespondenceData<T>> correspondenceData = std::make_shared<CorrespondenceData<T>>();

    if (!tgt->HasVertexNormals() || tgt->NumTriangles() == 0 || !src->HasVertexNormals())
    {
        return nullptr;
    }

    const auto srcMask = srcWeights.NonzeroVertices();

    TITAN_NAMESPACE::AABBTree<T> targetAabbTree(tgt->Vertices().transpose(), tgt->Triangles().transpose());
    for (int vID : srcMask)
    {
        auto [tID, bcWeights, dist] = targetAabbTree.getClosestPoint(src->Vertices().col(vID).transpose(), T(1e6));
        if (tID >= 0)
        {
            BarycentricCoordinates<T> bc(tgt->Triangles().col(tID), bcWeights.transpose());
            correspondenceData->srcIDs.push_back(vID);
            correspondenceData->targetBCs.push_back(bc);
        }
    }

    return correspondenceData;
}

template <class T>
Eigen::Matrix<T, 3, -1> CorrespondenceData<T>::EvaluateTargetBCs(const Eigen::Matrix<T, 3, -1>& inputPoints) const
{
    int numPoints = (int)targetBCs.size();
    Eigen::Matrix<T, 3, -1> output = Eigen::Matrix<T, 3, -1>::Zero(3, numPoints);

    for (int i = 0; i < numPoints; ++i)
    {
        const auto point = targetBCs[i].template Evaluate<3>(inputPoints);
        output.col(i) = point;
    }

    return output;
}

template <class T>
bool CorrespondenceData<T>::LoadFromJson(const JsonElement& json)
{
    if (json.Contains("metadata")) {
        int numPoints = json["metadata"]["numPoints"].Value<int>();
        targetBCs.resize(numPoints);
    }
    else
    {
        LOG_ERROR("Json element invalid for correspondence data.");
        return false;
    }

    if (json.Contains("sourceIds")) {
        srcIDs = json["sourceIds"].Get<std::vector<int>>();
    }
    else
    {
        LOG_ERROR("Json element invalid for correspondence data.");
        return false;
    }

    if (json.Contains("targetBCsIndices"))
    {
        std::vector<std::tuple<int, int, int>> localTargetBcsIndices = json["targetBCsIndices"].Get<std::vector<std::tuple<int, int, int>>>();
        std::vector<std::tuple<T, T, T>> localTargetBcsWeights = json["targetBCsValues"].Get<std::vector<std::tuple<T, T, T>>>();

        for (int i = 0; i < (int)targetBCs.size(); ++i)
        {
            Eigen::Vector3i vertexIds = Eigen::Vector3i(std::get<0>(localTargetBcsIndices[i]), std::get<1>(localTargetBcsIndices[i]), std::get<2>(localTargetBcsIndices[i]));
            Eigen::Vector3<T> vertexWeights = Eigen::Vector3<T>(std::get<0>(localTargetBcsWeights[i]), std::get<1>(localTargetBcsWeights[i]), std::get<2>(localTargetBcsWeights[i]));
            targetBCs[i] = BarycentricCoordinates<float>(vertexIds, vertexWeights);
        }
    }
    else
    {
        LOG_ERROR("Json element invalid for correspondence data.");
        return false;
    }

    return true;
}

template <class T>
TITAN_NAMESPACE::JsonElement CorrespondenceData<T>::SaveToJson() const
{
    JsonElement json(JsonElement::JsonType::Object);
    JsonElement meta(JsonElement::JsonType::Object);
    meta.Insert("type", JsonElement("correspondences"));
    meta.Insert("numPoints", JsonElement((int)srcIDs.size()));
    json.Insert("metadata", std::move(meta));

    JsonElement sourceIds(JsonElement::JsonType::Array);
    for (int i = 0; i < (int)srcIDs.size(); ++i) {
        sourceIds.Append(JsonElement(srcIDs[i]));
    }

    json.Insert("sourceIds", std::move(sourceIds));

    JsonElement targetBCsIds(JsonElement::JsonType::Array);
    JsonElement targetBCsValues(JsonElement::JsonType::Array);

    for (int i = 0; i < (int)targetBCs.size(); ++i) {
        targetBCsIds.Append(JsonElement(std::tuple<int, int, int>(targetBCs[i].Indices()[0], targetBCs[i].Indices()[1], targetBCs[i].Indices()[2])));
        targetBCsValues.Append(JsonElement(std::tuple<T, T, T>(targetBCs[i].Weights()[0], targetBCs[i].Weights()[1], targetBCs[i].Weights()[2])));
    }

    json.Insert("targetBCsIndices", std::move(targetBCsIds));
    json.Insert("targetBCsValues", std::move(targetBCsValues));

    return json;
}

template std::shared_ptr<const CorrespondenceData<float>> FindClosestCorrespondences(const std::shared_ptr<const Mesh<float>>& src,
                                                                                     const std::shared_ptr<const Mesh<float>>& tgt,
                                                                                     const VertexWeights<float>& srcWeights);
template struct CorrespondenceData<float>;
} // namespace fitting_tools

// explicitly instantiate the fitting initializer classes
template class FittingInitializer<float>;
CARBON_NAMESPACE_END(TITAN_NAMESPACE)
