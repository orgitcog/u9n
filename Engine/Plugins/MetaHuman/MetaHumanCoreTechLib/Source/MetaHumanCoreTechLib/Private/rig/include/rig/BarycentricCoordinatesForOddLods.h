// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>
#include <rig/RigGeometry.h>
#include <carbon/io/JsonIO.h>
#include <nls/geometry/BarycentricCoordinates.h> 


CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

/*
* Simple class for representing the barycentric coordinates for the head mesh for odd lods which are calculated relative to the previous lod head mesh
*/
template <class T>
class BarycentricCoordinatesForOddLods
{
public:
    BarycentricCoordinatesForOddLods() = default;

    /**
    * initialise the class from the supplied (head) rigGeometry.
    * @param[in] rigGeometry: the rig geometry for the head rig. 
    */
    void Init(const RigGeometry<T>& headRigGeometry);

    //! Read data from json element
    bool ReadJson(const JsonElement& json);

    //! Write the BarycentricCoordinatesForOddLods to Json
    void WriteJson(JsonElement& json) const;

    /**
     * Check if the confifg is valid for the supplied face rig. Returns true if it is valid, false if not. Checks that a set of barycentric coordinates is
     * available for each odd lod, and that there are the correct number of barycentric coordinates for the current lod head mesh, and that the vertex indices for the bcs are in range
     * for the previous lod head mesh
     */ 
    bool IsValidForRig(const RigGeometry<T>& faceRigGeometry) const;

    /**
    * Get the map of mesh seam snap configs for each head mesh from combined body mesh to head mesh for each lod.
    * Returns a map of (head) lod to a corresponding vector of pairs of boolean and BaryCentric coordinates, for each vertex in the current lod head mesh, how it maps onto
    * the vertices of the previous lod, with the bool set to true to indicate a valid mapping and false to indicate an invalid mapping
    */
    const std::map<int, std::vector<std::pair<bool, BarycentricCoordinates<T>>>>& GetBarycentricCoordinatesForOddLods() const;

private:

    std::map<int, std::vector<std::pair<bool, BarycentricCoordinates<T>>>> m_barycentricCoordinatesForOddLods;
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
