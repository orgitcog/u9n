// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <nls/geometry/Mesh.h>
#include <nls/geometry/Affine.h>

#include <string>
#include <vector>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

//! meshIds used only when loading DNA files (0 - head, 1 - teeth, 3 - left eye, 4 - right eye)
template <class T>
std::vector<Mesh<T>> ReadMeshFromObjOrDNA(const std::string& filename, const std::vector<int>& meshIds);

//! meshId used only when loading DNA files (0 - head, 1 - teeth, 3 - left eye, 4 - right eye)
template <class T>
Mesh<T> ReadMeshFromObjOrDNA(const std::string& filename, int meshId);

std::vector<std::string> GetObjsOrDNAsFromDirectory(const std::string& dir, bool objOnly = false);

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
