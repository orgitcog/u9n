// Copyright Epic Games, Inc. All Rights Reserved.

#include <nls/geometry/WrapDeformer.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
bool WrapDeformerParams<T>::ReadJson(const JsonElement& element)
{
    if (!element.IsObject())
    {
        LOG_ERROR("params json is not an object");
        return false;
    }

    const auto& paramsMap = element.Object();

    if (paramsMap.contains("falloff_type") && paramsMap.at("falloff_type").IsString())
    {
        const std::string falloffTypeStr = paramsMap.at("falloff_type").Get<std::string>();
        if (falloffTypeStr == "Volume")
        {
            falloffType = FalloffType::Volume;
        }
        else if (falloffTypeStr == "Surface")
        {
            falloffType = FalloffType::Surface;
        }
        else
        {
            LOG_ERROR("Unrecognized falloff_type parameter {}", falloffTypeStr);
            return false;
        }
    }
    else
    {
        LOG_ERROR("Failed to find falloff_type parameter");
        return false;
    }

    if (paramsMap.contains("exclusive_bind"))
    {
        bExclusiveBind = paramsMap.at("exclusive_bind").Boolean();
    }
    else
    {
        LOG_ERROR("Failed to find exclusive_bind parameter");
        return false;
    }


    if (paramsMap.contains("max_distance") && paramsMap.at("max_distance").IsNumber())
    {
        maxDistance = static_cast<T>(paramsMap.at("max_distance").Get<float>());
    }
    else
    {
        LOG_ERROR("Failed to find max_distance parameter");
        return false;
    }

    if (paramsMap.contains("weight_threshold") && paramsMap.at("weight_threshold").IsNumber())
    {
        weightThreshold = static_cast<T>(paramsMap.at("weight_threshold").Get<float>());
    }
    else
    {
        LOG_ERROR("Failed to find weight_threshold parameter");
        return false;
    }

    if (paramsMap.contains("auto_weight_threshold"))
    {
        bAutoWeightThreshold = paramsMap.at("auto_weight_threshold").Boolean();
    }
    else
    {
        LOG_ERROR("Failed to find auto_weight_threshold parameter");
        return false;
    }

    if (paramsMap.contains("normal_offset") && paramsMap.at("normal_offset").IsNumber())
    {
        normalOffset = static_cast<T>(paramsMap.at("normal_offset").Get<float>());
    }
    else
    {
        LOG_INFO("Optional normal_offset parameter not present");
        normalOffset = 0;
    }
    
    if (paramsMap.contains("wrapped_mesh_vertex_indices_to_apply_to") && paramsMap.at("wrapped_mesh_vertex_indices_to_apply_to").IsArray())
    {   
        std::vector<int> wrappedMeshVertexIndicesToApplyToVec = paramsMap.at("wrapped_mesh_vertex_indices_to_apply_to").Get<std::vector<int>>();
        wrappedMeshVertexIndicesToApplyTo = Eigen::VectorXi(int(wrappedMeshVertexIndicesToApplyToVec.size()));
        for (size_t i = 0; i < wrappedMeshVertexIndicesToApplyToVec.size(); ++i)
        {
            wrappedMeshVertexIndicesToApplyTo(static_cast<int>(i)) = wrappedMeshVertexIndicesToApplyToVec[i];
        }
    }
    else
    {
        LOG_INFO("Optional wrapped_mesh_vertex_indices_to_apply_to parameter not present");
        wrappedMeshVertexIndicesToApplyTo = Eigen::VectorXi();
    }

    return true;
}

template <class T>
bool ToBinaryFile(FILE* pFile, const WrapDeformerParams<T>& params)
{
    bool success = true;
    success &= io::ToBinaryFile(pFile, params.version);
    success &= io::ToBinaryFile(pFile, params.bExclusiveBind);
    success &= io::ToBinaryFile(pFile, static_cast<int>(params.falloffType));
    success &= io::ToBinaryFile(pFile, params.maxDistance);
    success &= io::ToBinaryFile(pFile, params.weightThreshold);
    success &= io::ToBinaryFile(pFile, params.bAutoWeightThreshold);
    success &= io::ToBinaryFile(pFile, params.normalOffset);
    success &= io::ToBinaryFile(pFile, params.wrappedMeshVertexIndicesToApplyTo);
    return success;
}

template <class T>
bool FromBinaryFile(FILE* pFile, WrapDeformerParams<T>& params)
{
    bool success = true;
    int32_t version;
    success &= io::FromBinaryFile(pFile, version);
    if (success && version == 2)
    {
        success &= io::FromBinaryFile(pFile, params.bExclusiveBind);
        int falloffType;
        success &= io::FromBinaryFile(pFile, falloffType);
        params.falloffType = static_cast<WrapDeformerParams<T>::FalloffType>(falloffType);
        success &= io::FromBinaryFile(pFile, params.maxDistance);
        success &= io::FromBinaryFile(pFile, params.weightThreshold);
        success &= io::FromBinaryFile(pFile, params.bAutoWeightThreshold);
        success &= io::FromBinaryFile(pFile, params.normalOffset);
        success &= io::FromBinaryFile(pFile, params.wrappedMeshVertexIndicesToApplyTo);
    }
    else if (success && version == 1)
    {
        success &= io::FromBinaryFile(pFile, params.bExclusiveBind);
        int falloffType;
        success &= io::FromBinaryFile(pFile, falloffType);
        params.falloffType = static_cast<WrapDeformerParams<T>::FalloffType>(falloffType);
        success &= io::FromBinaryFile(pFile, params.maxDistance);
        success &= io::FromBinaryFile(pFile, params.weightThreshold);
        success &= io::FromBinaryFile(pFile, params.bAutoWeightThreshold);
        params.normalOffset = 0;
        params.wrappedMeshVertexIndicesToApplyTo = Eigen::VectorXi();
    }
    else
    {
        success = false;
    }
    return success;
}

template <class T>
void WrapDeformer<T>::Init(const std::shared_ptr<const Mesh<T>>& driverMesh, const std::shared_ptr<const Mesh<T>>& wrappedMesh, const WrapDeformerParams<T>& params)
{
    SetMeshes(driverMesh, wrappedMesh);
    Init(params);
}

template <class T>
void WrapDeformer<T>::SetMeshes(const std::shared_ptr<const Mesh<T>>& driverMesh, const std::shared_ptr<const Mesh<T>>& wrappedMesh)
{
    m_driverMesh = driverMesh;
    m_wrappedMesh = wrappedMesh;

    if (driverMesh && driverMesh->NumQuads() > 0)
    {
        CARBON_CRITICAL("Driver mesh must contain triangles only; please re-triangulate it");
    }
}

template <class T>
void WrapDeformer<T>::Init(const WrapDeformerParams<T>& params)
{
    if (!m_driverMesh || !m_wrappedMesh)
    {
        CARBON_CRITICAL("driver and wrapped meshes must be initialized in order to Initialize the WrapDeformer");
    }

    m_wrappingParams = params;

    if (m_wrappingParams.falloffType != WrapDeformerParams<T>::FalloffType::Volume)
    {
        CARBON_CRITICAL("Only a FalloffType of Volume is implemented currently; Surface falloff is not allowed");
    }

    // check that params.wrappedMeshVertexIndicesToApplyTo is in range for the wrappedMesh
    for (int i = 0; i < params.wrappedMeshVertexIndicesToApplyTo.size(); ++i)
    {
        if (params.wrappedMeshVertexIndicesToApplyTo(i) >= m_wrappedMesh->NumVertices() || params.wrappedMeshVertexIndicesToApplyTo(i) < 0)
        {
            CARBON_CRITICAL("wrappedMeshVertexIndicesToApplyTo value {} is out of range for the wrappedMesh", params.wrappedMeshVertexIndicesToApplyTo(i));
        }
    }

    AABBTree<T> driverAabbTree(m_driverMesh->Vertices().transpose(), m_driverMesh->Triangles().transpose());
    // currently we calculate correspondence for all mesh vertices, even if only applying to a subset TODO only calculate for subset?
    m_driverMeshCorrespondenceClosestPointData.clear();
    m_driverMeshCorrespondenceClosestPointData.resize(m_wrappedMesh->Vertices().cols(), {});

    // if we are doing exclusive bind, just find the closest triangle, but we still apply the maxDistance criterion
    T maxDistance = m_wrappingParams.maxDistance;

    // special case 0 - use maximum maxDistance
    if (std::fabs(m_wrappingParams.maxDistance) < std::numeric_limits<T>::epsilon())
    {
        maxDistance = std::numeric_limits<T>::max();
    }

    if (m_wrappingParams.bExclusiveBind)
    {
        for (int vID = 0; vID < m_wrappedMesh->Vertices().cols(); ++vID)
        {
            auto [tID, bcWeights, dist] = driverAabbTree.getClosestPoint(m_wrappedMesh->Vertices().col(vID).transpose(), maxDistance);
            if (tID >= 0)
            {
                BarycentricCoordinates<T> bc(m_driverMesh->Triangles().col(tID), bcWeights.transpose());
                m_driverMeshCorrespondenceClosestPointData[vID] = { ClosestPointData<T>(m_driverMesh->Vertices(), m_wrappedMesh->Vertices().col(vID).transpose(), bc, 1) };
            }
            else
            {
                LOG_WARNING("No closest point found in range for vertex {}", vID);
            }
        }
    }
    else
    {
        if (m_wrappingParams.bAutoWeightThreshold)
        {
            // we want to set the weight threshold so that it is the minimum such that every point has at least one
            // nearest point within distance
            // find the largest closest distance across all points to make sure every point is considered
            T maxMinDistanceSq = 0;
            for (int vID = 0; vID < m_wrappedMesh->Vertices().cols(); ++vID)
            {
                auto [tID, bcWeights, distSq] = driverAabbTree.getClosestPoint(m_wrappedMesh->Vertices().col(vID).transpose(), std::numeric_limits<T>::max());
                if (distSq > maxMinDistanceSq)
                {
                    maxMinDistanceSq = distSq;
                }
            }

            maxDistance = std::sqrt(maxMinDistanceSq);
        }

        // calculate the closest points and convert to weights
        // we weight inversely by distance from the point and normalize by the total weight (not sure if this is identical to Maya but gives close results)
        for (int vID = 0; vID < m_wrappedMesh->Vertices().cols(); ++vID)
        {
            auto closestPoints = driverAabbTree.getAllPointsWithinDistance(m_wrappedMesh->Vertices().col(vID).transpose(), maxDistance + std::numeric_limits<T>::epsilon()); // the epsilon is added
            // because with numerical rounding errors the closest point can sometimes fail the <= test in getAllPointsWithinDistance ie no closest point
            if (closestPoints.empty())
            {
                m_driverMeshCorrespondenceClosestPointData[vID] = {}; // no closest point data
            }
            else
            {
                T totalWeight = 0;
                m_driverMeshCorrespondenceClosestPointData[vID].resize(closestPoints.size());
                for (size_t i = 0; i < closestPoints.size(); ++i)
                {
                    BarycentricCoordinates<T> bc(m_driverMesh->Triangles().col(std::get<0>(closestPoints[i])), std::get<1>(closestPoints[i]).transpose());
                    const T dist = std::sqrt(std::get<2>(closestPoints[i]));
                    // special case for dist = 0; we just have one point here and a weight of 1
                    if (dist < std::numeric_limits<T>::epsilon())
                    {
                        m_driverMeshCorrespondenceClosestPointData[vID] = { ClosestPointData<T>(m_driverMesh->Vertices(),
                            m_wrappedMesh->Vertices().col(vID).transpose(), bc, 1.0f) };
                        totalWeight = 1.0f;
                        break;
                    }
                    else
                    {
                        m_driverMeshCorrespondenceClosestPointData[vID][i] = ClosestPointData<T>(m_driverMesh->Vertices(),
                            m_wrappedMesh->Vertices().col(vID).transpose(), bc, 1 / dist);
                        totalWeight += m_driverMeshCorrespondenceClosestPointData[vID][i].weight;
                    }
                }

                // identify any points which are below the weightThreshold
                T newTotalWeight = 0;
                std::vector<size_t> pointsToUse;
                for (size_t i = 0; i < m_driverMeshCorrespondenceClosestPointData[vID].size(); ++i)
                {
                    m_driverMeshCorrespondenceClosestPointData[vID][i].weight /= totalWeight;
                    if (m_driverMeshCorrespondenceClosestPointData[vID][i].weight >= m_wrappingParams.weightThreshold)
                    {
                        newTotalWeight += m_driverMeshCorrespondenceClosestPointData[vID][i].weight;
                        pointsToUse.emplace_back(i);
                    }
                }

                // if we are using auto weight threshold, we don't use the weight threshold so just stick with the original normalized result
                if (!m_wrappingParams.bAutoWeightThreshold)
                {
                    // re-normalize having discarded points
                    std::vector<ClosestPointData<T>> finalNormalizedPoints;
                    for (size_t i = 0; i < pointsToUse.size(); ++i)
                    {
                        ClosestPointData<T> reweightedData = m_driverMeshCorrespondenceClosestPointData[vID][pointsToUse[i]];
                        reweightedData.weight /= newTotalWeight;
                        finalNormalizedPoints.emplace_back(reweightedData);
                    }
                    m_driverMeshCorrespondenceClosestPointData[vID] = finalNormalizedPoints;
                }
            }
        }
    }
}

template <class T>
const WrapDeformerParams<T>& WrapDeformer<T>::Params() const
{
    return m_wrappingParams;
}

template <class T>
void WrapDeformer<T>::GetDriverMeshClosestPointBarycentricCoordinates(std::vector<BarycentricCoordinates<T>>& driverMeshClosestPointBarycentricCoordinates) const
{
    if (!m_driverMesh)
    {
        CARBON_CRITICAL("wrap deformer is not initialized");
    }

    driverMeshClosestPointBarycentricCoordinates.resize(m_driverMeshCorrespondenceClosestPointData.size());
    for (size_t i = 0; i < m_driverMeshCorrespondenceClosestPointData.size(); ++i)
    {
        T highestWeight = 0;
        for (size_t j = 0; j < m_driverMeshCorrespondenceClosestPointData[i].size(); ++j)
        {
            if (m_driverMeshCorrespondenceClosestPointData[i][j].weight >= highestWeight)
            {
                highestWeight = m_driverMeshCorrespondenceClosestPointData[i][j].weight;
                driverMeshClosestPointBarycentricCoordinates[i] = m_driverMeshCorrespondenceClosestPointData[i][j].bcs;
            }
        }
    }
}


template <class T>
void WrapDeformer<T>::Deform(const Eigen::Matrix<T, 3, -1>& deformedDriverMeshVertices, Eigen::Matrix<T, 3, -1>& deformedWrappedMeshVertices) const
{
    if (!m_driverMesh)
    {
        CARBON_CRITICAL("wrap deformer is not initialized");
    }

    if (deformedDriverMeshVertices.cols() != m_driverMesh->Vertices().cols())
    {
        CARBON_CRITICAL("incorrect number of driver vertices for wrap deformer");
    }

    Eigen::Matrix<T, 3, -1> origVertices = deformedWrappedMeshVertices;

    if (deformedWrappedMeshVertices.cols() == 0)
    {
        deformedWrappedMeshVertices = m_wrappedMesh->Vertices();
    }
    else if (deformedWrappedMeshVertices.cols() != m_wrappedMesh->Vertices().cols())
    {
        CARBON_CRITICAL("deformedWrappedMeshVertices must either be empty or the correct size for the wrapped mesh");
    }

    // only apply to the specified vertices
    Eigen::VectorXi wrappedMeshVertexIndicesToApplyTo = m_wrappingParams.wrappedMeshVertexIndicesToApplyTo;
    if (m_wrappingParams.wrappedMeshVertexIndicesToApplyTo.size() == 0)
    {
        wrappedMeshVertexIndicesToApplyTo.resize(deformedWrappedMeshVertices.cols());
        for (int i = 0; i < wrappedMeshVertexIndicesToApplyTo.size(); ++i)
        {
            wrappedMeshVertexIndicesToApplyTo[i] = i;
        }
    }
    for (int j = 0; j < wrappedMeshVertexIndicesToApplyTo.size(); ++j)
    {
        int vID = wrappedMeshVertexIndicesToApplyTo(j);
        // do a weighted sum of the transformed points, or just leave in the same place if no closest point data
        if (m_driverMeshCorrespondenceClosestPointData[vID].size() > 0)
        {
            Eigen::Vector<T, 3> weightedSum = Eigen::Vector<T, 3>::Zero();
            for (size_t i = 0; i < m_driverMeshCorrespondenceClosestPointData[vID].size(); ++i)
            {
                weightedSum += m_driverMeshCorrespondenceClosestPointData[vID][i].CalculateTransformedPoint(deformedDriverMeshVertices, m_wrappingParams.normalOffset) * m_driverMeshCorrespondenceClosestPointData[vID][i].weight;
            }
            deformedWrappedMeshVertices.col(vID) = weightedSum;
        }
    }
}

template <class T>
bool ToBinaryFile(FILE* pFile, const WrapDeformer<T>& wrapDeformer)
{
    bool success = true;
    success &= io::ToBinaryFile(pFile, wrapDeformer.m_version);
    success &= io::ToBinaryFile(pFile, wrapDeformer.m_driverMesh);
    success &= io::ToBinaryFile(pFile, wrapDeformer.m_wrappedMesh);
    success &= io::ToBinaryFile(pFile, wrapDeformer.m_driverMeshCorrespondenceClosestPointData);
    success &= ToBinaryFile(pFile, wrapDeformer.m_wrappingParams);
    return success;
}

template <class T>
bool FromBinaryFile(FILE* pFile, WrapDeformer<T>& wrapDeformer)
{
    bool success = true;
    int32_t version;
    success &= io::FromBinaryFile<int32_t>(pFile, version);
    if (success && version == 2)
    {
        std::shared_ptr<Mesh<T>> driverMesh, wrappedMesh;
        success &= io::FromBinaryFile(pFile, driverMesh);
        success &= io::FromBinaryFile(pFile, wrappedMesh);
        wrapDeformer.SetMeshes(driverMesh, wrappedMesh);
        success &= io::FromBinaryFile(pFile, wrapDeformer.m_driverMeshCorrespondenceClosestPointData);
        success &= FromBinaryFile(pFile, wrapDeformer.m_wrappingParams);
    }
    else if (success && version == 1)
    {
        std::shared_ptr<Mesh<T>> driverMesh = std::make_shared<Mesh<T>>();
        success &= io::FromBinaryFile(pFile, *driverMesh);
        wrapDeformer.m_driverMesh = driverMesh;
        std::shared_ptr<Mesh<T>> wrappedMesh = std::make_shared<Mesh<T>>();
        success &= io::FromBinaryFile(pFile, *wrappedMesh);
        wrapDeformer.m_wrappedMesh = wrappedMesh;
        success &= io::FromBinaryFile(pFile, wrapDeformer.m_driverMeshCorrespondenceClosestPointData);
        success &= FromBinaryFile(pFile, wrapDeformer.m_wrappingParams);
    }
    else
    {
        success = false;
    }
    return success;
}


template class WrapDeformer<float>;
template class WrapDeformer<double>;

template struct WrapDeformerParams<float>;
template struct WrapDeformerParams<double>;

template bool ToBinaryFile(FILE* pFile, const WrapDeformerParams<float>& params);
template bool ToBinaryFile(FILE* pFile, const WrapDeformerParams<double>& params);

template bool FromBinaryFile(FILE* pFile, WrapDeformerParams<float>& params);
template bool FromBinaryFile(FILE* pFile, WrapDeformerParams<double>& params);

template bool ToBinaryFile(FILE* pFile, const WrapDeformer<float>& wrapDeformer);
template bool ToBinaryFile(FILE* pFile, const WrapDeformer<double>& wrapDeformer);

template bool FromBinaryFile(FILE* pFile, WrapDeformer<float>& wrapDeformer);
template bool FromBinaryFile(FILE* pFile, WrapDeformer<double>& wrapDeformer);


CARBON_NAMESPACE_END(TITAN_NAMESPACE)
