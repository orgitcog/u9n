// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <nrr/MeshLandmarks.h>
#include <nrr/landmarks/LandmarkInstance.h>
#include <nls/geometry/Camera.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

/**
 * Set up the weights for landmarks in the following way:
 * From the first frame (iterator 0) all landmarks will have weight 1.
 * Landmarks from other frames (iterator 1 to n) will have weight 0 if landmark/curve with same ID is present in frame 0. Otherwise, weight will be 1.
 */
template <class T>
const std::vector<std::map<std::string, T>> MaskLandmarksToAvoidAmbiguity(const std::map<std::string, T>& currentGlobalLandmarkWeights,
                                                                          const std::vector<std::vector<std::pair<LandmarkInstance<T, 2>,
                                                                                                                  Camera<T>>>>& targetLandmarks);

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
