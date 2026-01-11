// Copyright Epic Games, Inc. All Rights Reserved.

#include <rig/serialization/DirectoryMeshReader.h>

#include <carbon/Common.h>
#include <nls/geometry/Mesh.h>
#include <rig/RigUtils.h>
#include <rig/RigLogicDNAResource.h>
#include <nls/serialization/ObjFileFormat.h>
#include <nls/geometry/EulerAngles.h>
#include <rig/Rig.h>

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

//! meshIds used only when loading DNA files (0 - head, 1 - teeth, 3 - left eye, 4 - right eye)
template <class T>
std::vector<Mesh<T>> ReadMeshFromObjOrDNA(const std::string& filename, const std::vector<int>& meshIds)
{
    if (filename.substr(filename.size() - 4) == ".obj")
    {
        LOG_INFO("path: {}", filename);
        Mesh<T> mesh;
        if (!ObjFileReader<T>().readObj(filename, mesh))
        {
            CARBON_CRITICAL("failed to read mesh {}", filename);
        }
        return { mesh };
    }
    else if (filename.substr(filename.size() - 4) == ".dna")
    {
        LOG_INFO("path: {}", filename);

        std::shared_ptr<const RigLogicDNAResource> dnaResource = RigLogicDNAResource::LoadDNA(filename, /*retain=*/false);
        if (!dnaResource)
        {
            CARBON_CRITICAL("failed to open dnafile {}", filename);
        }
        std::shared_ptr<RigGeometry<T>> rigGeometry = std::make_shared<RigGeometry<T>>();
        if (!rigGeometry->Init(dnaResource->Stream()))
        {
            CARBON_CRITICAL("failed to load riggeometry from dnafile {}", filename);
        }
        std::vector<Mesh<T>> meshes;
        for (const int meshId : meshIds)
        {
            meshes.push_back(rigGeometry->GetMesh(meshId));
        }
        return meshes;
    }

    CARBON_CRITICAL("file {} is not a valid obj or dna file", filename);
}

template std::vector<Mesh<float>> ReadMeshFromObjOrDNA(const std::string& filename, const std::vector<int>& meshIds);
template std::vector<Mesh<double>> ReadMeshFromObjOrDNA(const std::string& filename, const std::vector<int>& meshIds);

template <class T>
Mesh<T> ReadMeshFromObjOrDNA(const std::string& filename, int meshId) { return ReadMeshFromObjOrDNA<T>(filename, std::vector<int>{ meshId }).front();
}

template Mesh<float> ReadMeshFromObjOrDNA(const std::string& filename, int meshId);
template Mesh<double> ReadMeshFromObjOrDNA(const std::string& filename, int meshId);

std::vector<std::string> GetObjsOrDNAsFromDirectory(const std::string& dir, bool objOnly)
{
    std::vector<std::string> filenames;
    std::filesystem::directory_iterator directoryIterator(dir);
    for (const auto& path : directoryIterator)
    {
        const std::string& filename = path.path().string();
        if (filename.substr(filename.size() - 4) == ".obj")
        {
            filenames.push_back(filename);
        }
        else if ((filename.substr(filename.size() - 4) == ".dna") && !objOnly)
        {
            filenames.push_back(filename);
        }
    }
    std::sort(filenames.begin(), filenames.end());
    return filenames;
}

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
