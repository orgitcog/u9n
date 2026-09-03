// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>
#include <rig/RigGeometry.h>


#include <dna/Reader.h>
#include <dna/Writer.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE::rigutils)

template <class T>
std::map<std::string, int> JointNameToIndexMap(const RigGeometry<T>& rigGeometry);

std::map<std::string, int> JointNameToIndexMap(const dna::Reader* reader);

// returns per joint rotation and translation as affine transform.
template <class T>
std::vector<Affine<T, 3, 3>> JointWorldTransforms(const RigGeometry<T>& rigGeometry);

// returns per joint rotation and translation as separate data streams. Rotation is stored as Euler angles, and both rotation and translation are in local
// space.
template <class T>
std::pair<Eigen::Matrix<T, 3, -1>, Eigen::Matrix<T, 3, -1>> CalculateLocalJointRotationAndTranslation(const RigGeometry<T>& rigGeometry,
                                                                                                      const std::vector<Affine<T, 3, 3>>& worldTransforms);

template <class T>
std::vector<Affine<T, 3, 3>> WorldToLocalJointDeltas(const RigGeometry<T>& rigGeometry,
                                                     const std::vector<Affine<T, 3, 3>>& worldJointDeltas);

void AddRBFLayerToDnaStream(dna::Reader* reader, dna::Reader* rbfReader, dna::Writer* writer);

std::vector<int> GetJointsWithVariableSkinning(const dna::Reader* reader, const std::string& regionName);

CARBON_NAMESPACE_END(TITAN_NAMESPACE::rigutils)
