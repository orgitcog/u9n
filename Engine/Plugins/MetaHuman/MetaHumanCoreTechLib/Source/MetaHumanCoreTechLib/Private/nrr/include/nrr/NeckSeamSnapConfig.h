// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>
#include <rig/RigGeometry.h>
#include <nls/geometry/SnapConfig.h>
#include <carbon/io/JsonIO.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

/*
* Simple class for representing the mapping of the neck seam between combined body rig and face rig 
*/
template <class T>
class NeckSeamSnapConfig
{
public:
    NeckSeamSnapConfig() = default;

    /**
    * initialise the neck seam snap config from neckSeamPath json file, for the supplied (head) rigGeometry.
    * This handles the case that there are 8 head lods and only 4 body lods; the neck seam vertices for the 'odd' head lods match exactly. Returns true if loaded successfully, false if not.
    * @param[in] neckSeamPath: filename of json file containing the neck seam vertex indices for the head rig at each lod
    * @param[in] rigGeometry: the rig geometry for the head rig
    * @returns true if successfully loaded, false otherwise
    */
    bool Init(const std::string neckSeamPath, const RigGeometry<T>& rigGeometry);

    //! Read data from json element
    bool ReadJson(const JsonElement& json);

    //! Write the NeckSeamSnapConfig to Json
    void WriteJson(JsonElement& json) const;

    /**
     * Check if the neck seam snap config is valid for the supplied combined body and face rigs. Returns true if it is valid, false if not. Checks that the map
     * keys match the head mesh names for all lods, and that the vertex indices for the snap config are in range, and that all lods of the body are included
     */ 
    bool IsValidForCombinedBodyAndFaceRigs(const RigGeometry<T>& combinedBodyRigGeometry, const RigGeometry<T>& faceRigGeometry) const;

    /**
    * Get the map of mesh seam snap configs for each head mesh from combined body mesh to  head mesh for each lod.
    * Returns a map of (head) mesh name at each lod to the corresponding SnapConfig which maps between src vertex indices 
    * (on the combined body) to target vertex indices (on the head). The int in each pair is the body lod which corresponds to the current head mesh name
    */
    const std::map<std::string, std::pair<int, SnapConfig<T>>>& GetLodNeckSeamSnapConfigs() const;

private:

    std::map<std::string, std::pair<int, SnapConfig<T>>> m_neckBodySnapConfig;
};


CARBON_NAMESPACE_END(TITAN_NAMESPACE)
