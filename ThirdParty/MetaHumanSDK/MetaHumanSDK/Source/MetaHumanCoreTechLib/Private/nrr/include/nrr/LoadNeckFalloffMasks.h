// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>
#include <rig/RigGeometry.h>
#include <nrr/VertexWeights.h>
#include <carbon/io/JsonIO.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)


/**
* load in a set of neck falloff masks from neckFalloffMaskJsonFilenameh json file, for the supplied (head) rigGeometry, and return as a vector of pts to VertexWeights for each lod
* @param[in] neckFalloffMaskJson: the json element containing the falloff mask for each lod, and each vertex for each lod
* @param[in] rigGeometry: the rig geometry for the head rig
* @param[out] headVertexSkinningWeightsMasks: on completion contains a vector of ptrs to VertexWeights for each lod. The weights are the amount of weight per vertex (0-1) which should be applied to the 
* body skinning weights
* @returns true if successfully loaded, false otherwise
*/
template <class T>
bool LoadNeckFalloffMasks(const JsonElement& neckFalloffMaskJson, const RigGeometry<T>& rigGeometry, std::vector<std::shared_ptr<VertexWeights<T>>>& headVertexSkinningWeightsMasks);


CARBON_NAMESPACE_END(TITAN_NAMESPACE)
