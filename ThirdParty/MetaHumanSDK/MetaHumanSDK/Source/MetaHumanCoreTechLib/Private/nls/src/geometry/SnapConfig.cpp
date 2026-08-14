// Copyright Epic Games, Inc. All Rights Reserved.

#include <nls/geometry/SnapConfig.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
bool SnapConfig<T>::ReadJson(const JsonElement& element)
{
    if (!element.IsObject())
    {
        LOG_ERROR("snap_config json is not an object");
        return false;
    }

    const auto& snapConfigMap = element.Object();

    if (snapConfigMap.contains("source_mesh") && snapConfigMap.at("source_mesh").IsString())
    {
        sourceMesh = snapConfigMap.at("source_mesh").Get<std::string>();
    }
    else
    {
        LOG_ERROR("Failed to find snap config source_mesh parameter");
        return false;
    }

    if (snapConfigMap.contains("source_verts") && snapConfigMap.at("source_verts").IsArray())
    {
        sourceVertexIndices = snapConfigMap.at("source_verts").Get<std::vector<int>>();
    }
    else
    {
        LOG_ERROR("Failed to find snap config source_verts parameter");
        return false;
    }

    if (snapConfigMap.contains("target_verts") && snapConfigMap.at("target_verts").IsArray())
    {
        targetVertexIndices = snapConfigMap.at("target_verts").Get<std::vector<int>>();

        if (targetVertexIndices.size() != sourceVertexIndices.size())
        {
            LOG_ERROR("source_verts and target_verts must contain the same number of indices");
            return false;
        }
    }
    else
    {
        LOG_ERROR("Failed to find snap config target_verts parameter");
        return false;
    }

    return true;
}


template <class T>
bool SnapConfig<T>::IsValid(const Eigen::Matrix<T, 3, -1>& sourceVertices, const Eigen::Matrix<T, 3, -1>& targetVertices) const
{
    for (const auto& srcVertexInd : sourceVertexIndices)
    {
        if (srcVertexInd < 0 || srcVertexInd >= sourceVertices.cols())
        {
            return false;
        }
    }

    for (const auto& targetVertexInd : targetVertexIndices)
    {
        if (targetVertexInd < 0 || targetVertexInd >= targetVertices.cols())
        {
            return false;
        }
    }

    return true;
}


template <class T>
void SnapConfig<T>::Apply(const Eigen::Matrix<T, 3, -1>& sourceVertices, Eigen::Matrix<T, 3, -1>& targetVertices) const
{
    if (!IsValid(sourceVertices, targetVertices))
    {
        CARBON_CRITICAL("SnapConfig is not compatible with the supplied sourceVertices and TargetVertices");
    }

    for (size_t v = 0; v < sourceVertexIndices.size(); ++v)
    {
        targetVertices.col(targetVertexIndices[v]) = sourceVertices.col(sourceVertexIndices[v]);
    }
}

template <class T>
void SnapConfig<T>::WriteJson(JsonElement& json) const
{
    JsonElement snapConfigJson(JsonElement::JsonType::Object);
    snapConfigJson.Insert("source_mesh", JsonElement(sourceMesh));

    JsonElement sourceVertexIndicesJson(JsonElement::JsonType::Array);
    for (const auto& ind : sourceVertexIndices)
    {
        sourceVertexIndicesJson.Append(JsonElement(ind));
    }
    snapConfigJson.Insert("source_verts", std::move(sourceVertexIndicesJson));

    JsonElement targetVertexIndicesJson(JsonElement::JsonType::Array);
    for (const auto& ind : targetVertexIndices)
    {
        targetVertexIndicesJson.Append(JsonElement(ind));
    }
    snapConfigJson.Insert("target_verts", std::move(targetVertexIndicesJson));

    json.Insert("snap_config", std::move(snapConfigJson));
}

template struct SnapConfig<float>;
template struct SnapConfig<double>;

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
