// Copyright Epic Games, Inc. All Rights Reserved.

#include <carbon/io/Utils.h>
#include <nrr/LoadNeckFalloffMasks.h>


CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)


template<class T>
bool LoadNeckFalloffMasks(const JsonElement& neckFalloffMaskJson, const RigGeometry<T>& rigGeometry, std::vector<std::shared_ptr<VertexWeights<T>>>& headVertexSkinningWeightsMasks)
{
    bool bLoaded = true;
    try
    {
        headVertexSkinningWeightsMasks = std::vector<std::shared_ptr<VertexWeights<T>>>(size_t(rigGeometry.NumLODs()));

        for (int lod = 0; lod < rigGeometry.NumLODs(); ++lod)
        {
            headVertexSkinningWeightsMasks[lod] = std::make_shared<VertexWeights<T>>();
            headVertexSkinningWeightsMasks[lod]->Load(neckFalloffMaskJson, rigGeometry.HeadMeshName(lod), rigGeometry.GetMesh(rigGeometry.HeadMeshIndex(lod)).NumVertices());
        }
    }
    catch (std::exception& e)
    {
        LOG_ERROR("Failed to load neck falloff masks with error: {}", e.what());
        bLoaded = false;
    }

    return bLoaded;
}


template bool LoadNeckFalloffMasks(const JsonElement& neckFalloffMaskJson, const RigGeometry<float>& rigGeometry, std::vector<std::shared_ptr<VertexWeights<float>>>& headVertexSkinningWeightsMasks);
template bool LoadNeckFalloffMasks(const JsonElement& neckFalloffMaskJson, const RigGeometry<double>& rigGeometry, std::vector<std::shared_ptr<VertexWeights<double>>>& headVertexSkinningWeightsMasks);


CARBON_NAMESPACE_END(TITAN_NAMESPACE)
