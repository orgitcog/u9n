// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>
#include <rig/RigGeometry.h>


CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)


/**
* A class for calculating a simple mapping for how skinning weights for joints should be propagated from LOD0 to higher LODs. This is intended for a rig where joints for higher lods are represented by a simple
* parent child relationship ie no new joints are introduced at intermediate lods.
* The approach first works out which joints are active for each lod, and for each joint in a higher lod, if that joint is not active, it pushes it up to the first active parent.
* The user can also specify options joints for which joints can be propagated to a combination of the first active parent joint, and the closest active sibling joint. The user can decide the split in
* influence between the parent joint (giving stability) and the sibling joint (giving more specific localized behaviour), and can also apply an optional inverse distance weighting ie if the parent joint is
* further away it gives less influence (and vice versa). Sibling joint propagation works well for joint clusters like fingers (and possibly toes) where some of the digits may be removed at higher lod(s).
*/
template <class T>
class CombinedBodyJointLodMapping
{
public:
    CombinedBodyJointLodMapping() { }

    //| Calculate the joint mapping from RigGeometry as above
    void CalculateMapping(const RigGeometry<T>& rigGeometry);
    
    //! Get the joint mapping for all Lods. For each lod, this returns a mapping for each lod which defines how to take each joint influence for that joint from lod 0 and spread it onto joint(s) in the 
    //! current lod as a weighted sum of values from lod0
    const std::vector<std::map<std::string, std::map<std::string, T>>>& GetJointMapping() const;
 
    //! save the mapping to json
    JsonElement ToJson() const;
    
    //! read the mapping from json
    bool ReadJson(const JsonElement& jointPropagationMapJson);

    //! get whether or not we use distance weighting for sibling propagation
    bool UseDistanceWeightingForSiblingPropagation() const;

    //! set whether we use distance weighting for sibling propagation
    void SetUseDistanceWeightingForSiblingPropagation(bool bUseDistanceWeighting);

    //! get the parent weighting for sibling propagation
    T ParentWeightForSiblingPropagation() const;

    //! set the parent weighting for sibling propagation
    void SetParentWeightForSiblingPropagation(const T& parentWeighting);

    //! get the list of (parent) joints to include in sibling propagation ie all child joints below this joint will use sibling propagation
    const std::vector<std::string>& JointsToIncludeSiblingsInPropagation() const;

    //! set the list of (parent) joints to include in sibling propagation ie all child joints below this joint will use sibling propagation
    void SetJointsToIncludeSiblingsInPropagation(const std::vector<std::string>& jointsToIncludeInSiblingPropagation);

private:
    std::vector<std::map<std::string, std::map<std::string, T>>> m_combinedBodyJointLodMapping;
    std::vector<std::string> m_jointsToIncludeSiblingsInPropagation = { "hand_r", "hand_l" };
    T m_parentWeightForSiblingPropagation = T(0.25f);
    bool m_bUseDistanceWeightingForSiblingPropagation = true;
};


CARBON_NAMESPACE_END(TITAN_NAMESPACE)
