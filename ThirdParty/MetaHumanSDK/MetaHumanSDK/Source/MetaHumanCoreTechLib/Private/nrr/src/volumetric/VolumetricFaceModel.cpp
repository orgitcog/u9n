// Copyright Epic Games, Inc. All Rights Reserved.

#include <carbon/Common.h>
#include <carbon/io/JsonIO.h>
#include <carbon/io/Utils.h>
#include <nls/math/Math.h>
#include <nls/serialization/EigenSerialization.h>
#include <nls/serialization/ObjFileFormat.h>
#include <nrr/volumetric/VolumetricFaceModel.h>

#include <filesystem>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
const std::string VolumetricFaceModel<T>::GetCraniumMeshName() const
{
    return "cranium_with_teeth";
}

template <class T>
const std::string VolumetricFaceModel<T>::GetMandibleMeshName() const
{
    return "mandible_with_teeth";
}

template <class T>
const std::string VolumetricFaceModel<T>::GetFleshMeshName() const
{
    return "flesh";
}

template <class T>
bool VolumetricFaceModel<T>::Load(const std::string& directory)
{
    bool success = true;

    const std::string tetsFilename = directory + "/flesh";
    const std::string skinFilename = directory + "/skin.obj";
    const std::string fleshFilename = directory + "/flesh.obj";
    const std::string craniumFilename = directory + "/cranium_with_teeth.obj";
    const std::string mandibleFilename = directory + "/mandible_with_teeth.obj";
    const std::string teethFilename = directory + "/teeth.obj";
    const std::string embeddingFilename = directory + "/flesh_surface_embedding.json";

    const std::string skinFleshMappingFilename = directory + "/skin_flesh_mapping.json";
    const std::string craniumFleshMappingFilename = directory + "/cranium_with_teeth_flesh_mapping.json";
    const std::string mandibleFleshMappingFilename = directory + "/mandible_with_teeth_flesh_mapping.json";

    auto readMesh = [](const std::string& filename, Mesh<T>& mesh) -> bool {
            if (ObjFileReader<T>().readObj(filename, mesh))
            {
                return true;
            }
            else
            {
                LOG_ERROR("failed to load obj {}", filename);
                return false;
            }
        };

    // load meshes
    success &= readMesh(skinFilename, m_skinMesh);
    success &= readMesh(fleshFilename, m_fleshMesh);
    success &= readMesh(craniumFilename, m_craniumMesh);
    success &= readMesh(mandibleFilename, m_mandibleMesh);
    success &= readMesh(teethFilename, m_teethMesh);

    // load tet mesh
    m_tetMesh.LoadFromNPY(tetsFilename + "_verts.npy", tetsFilename + "_tets.npy");

    // load flesh embedding in tet mesh
    JsonElement embeddingJson = ReadJson(ReadFile(embeddingFilename));
    m_embedding.Deserialize(embeddingJson);

    auto extractBarycentricCoordinatesFromJson = [](const JsonElement& j) {
            std::vector<std::pair<int, BarycentricCoordinates<T>>> out;
            for (const auto& jData : j.Array())
            {
                const int vID = jData[0].Get<int>();
                Eigen::Vector3i indices;
                io::FromJson(jData[1], indices);
                Eigen::Vector3<T> weights;
                io::FromJson(jData[2], weights);
                out.push_back({ vID, BarycentricCoordinates<T>(indices, weights) });
            }
            return out;
        };
    if (embeddingJson.Contains("surface2tet"))
    {
        m_fleshTetMapping = extractBarycentricCoordinatesFromJson(embeddingJson["surface2tet"]);
    }
    if (embeddingJson.Contains("tet2surface"))
    {
        m_tetFleshMapping = extractBarycentricCoordinatesFromJson(embeddingJson["tet2surface"]);
    }

    // load mesh-flesh pairs
    m_skinFleshMapping = ReadJson(ReadFile(skinFleshMappingFilename)).Get<std::vector<std::pair<int, int>>>();
    m_craniumFleshMapping = ReadJson(ReadFile(craniumFleshMappingFilename)).Get<std::vector<std::pair<int, int>>>();
    m_mandibleFleshMapping = ReadJson(ReadFile(mandibleFleshMappingFilename)).Get<std::vector<std::pair<int, int>>>();

    return success;
}

template <class T>
bool VolumetricFaceModel<T>::Save(const std::string& directory, bool forceOverwrite) const
{
    if (std::filesystem::exists(directory))
    {
        if (forceOverwrite)
        {
            std::filesystem::remove_all(directory);
        }
        else
        {
            LOG_ERROR("directory \"{}\" already exists, cannot save flesh model to an existing directory", directory);
            return false;
        }
    }

    const auto parentDirectory = std::filesystem::path(directory).parent_path();
    if (!std::filesystem::exists(parentDirectory))
    {
        LOG_ERROR("Parent directory \"{}\" of output directory needs to exists.", parentDirectory.string());
        return false;
    }
    std::filesystem::create_directories(directory);

    ObjFileWriter<T>().writeObj(GetSkinMesh(), directory + "/skin.obj");
    ObjFileWriter<T>().writeObj(GetFleshMesh(), directory + "/flesh.obj");
    ObjFileWriter<T>().writeObj(GetCraniumMesh(), directory + "/cranium_with_teeth.obj");
    ObjFileWriter<T>().writeObj(GetMandibleMesh(), directory + "/mandible_with_teeth.obj");
    ObjFileWriter<T>().writeObj(GetTeethMesh(), directory + "/teeth.obj");

    GetTetMesh().SaveToNPY(directory + "/flesh_verts.npy", directory + "/flesh_tets.npy");

    JsonElement json(JsonElement::JsonType::Object);
    m_embedding.Serialize(directory + "/flesh_surface_embedding.json");
    m_embedding.Serialize(json);

    auto toJson = [](const std::vector<std::pair<int, BarycentricCoordinates<T>>>& mapping) {
        JsonElement jArray(JsonElement::JsonType::Array);
        for (const auto& [vID, bc] : mapping)
        {
            JsonElement jItem(JsonElement::JsonType::Array);
            jItem.Append(JsonElement(vID));
            jItem.Append(io::ToJson(bc.Indices()));
            jItem.Append(io::ToJson(bc.Weights()));
            jArray.Append(std::move(jItem));
        }
        return jArray;
    };

    json.Insert("surface2tet", toJson(m_fleshTetMapping));
    json.Insert("tet2surface", toJson(m_tetFleshMapping));

    WriteFile(directory + "/flesh_surface_embedding.json", WriteJson(json));

    WriteFile(directory + "/skin_flesh_mapping.json", WriteJson(JsonElement(m_skinFleshMapping)));
    WriteFile(directory + "/cranium_with_teeth_flesh_mapping.json", WriteJson(JsonElement(m_craniumFleshMapping)));
    WriteFile(directory + "/mandible_with_teeth_flesh_mapping.json", WriteJson(JsonElement(m_mandibleFleshMapping)));

    return true;
}

template <class T>
void VolumetricFaceModel<T>::SetSkinMeshVertices(const Eigen::Matrix<T, 3, -1>& skinVertices)
{
    CARBON_ASSERT(static_cast<int>(skinVertices.cols()) == m_skinMesh.NumVertices(),
                  "number of skin vertices is incorrect");
    m_skinMesh.SetVertices(skinVertices);
}

template <class T>
void VolumetricFaceModel<T>::SetFleshMeshVertices(const Eigen::Matrix<T, 3, -1>& fleshVertices)
{
    CARBON_ASSERT(static_cast<int>(fleshVertices.cols()) == m_fleshMesh.NumVertices(),
                  "number of flesh vertices is incorrect");
    m_fleshMesh.SetVertices(fleshVertices);
}

template <class T>
void VolumetricFaceModel<T>::SetCraniumMeshVertices(const Eigen::Matrix<T, 3, -1>& craniumVertices)
{
    CARBON_ASSERT(static_cast<int>(craniumVertices.cols()) == m_craniumMesh.NumVertices(),
                  "number of cranium vertices is incorrect");
    m_craniumMesh.SetVertices(craniumVertices);
}

template <class T>
void VolumetricFaceModel<T>::SetMandibleMeshVertices(const Eigen::Matrix<T, 3, -1>& mandibleVertices)
{
    CARBON_ASSERT(static_cast<int>(mandibleVertices.cols()) == m_mandibleMesh.NumVertices(),
                  "number of mandible vertices is incorrect");
    m_mandibleMesh.SetVertices(mandibleVertices);
}

template <class T>
void VolumetricFaceModel<T>::SetTeethMeshVertices(const Eigen::Matrix<T, 3, -1>& teethVertices)
{
    CARBON_ASSERT(static_cast<int>(teethVertices.cols()) == m_teethMesh.NumVertices(),
                  "number of teeth vertices is incorrect");
    m_teethMesh.SetVertices(teethVertices);
}

template <class T>
void VolumetricFaceModel<T>::SetTetMeshVertices(const Eigen::Matrix<T, 3, -1>& tetVertices)
{
    CARBON_ASSERT(static_cast<int>(tetVertices.cols()) == m_tetMesh.NumVertices(),
                  "number of tet vertices is incorrect");
    m_tetMesh.SetVertices(tetVertices);
}

template <class T>
void VolumetricFaceModel<T>::UpdateFleshMeshVerticesFromSkinCraniumAndMandible()
{
    // copy the skin, cranium, and mandible vertices
    Eigen::Matrix<T, 3, -1> fleshVertices = GetFleshMesh().Vertices();
    for (const auto& [skinIndex, fleshIndex] : SkinFleshMapping())
    {
        fleshVertices.col(fleshIndex) = GetSkinMesh().Vertices().col(skinIndex);
    }
    for (const auto& [craniumIndex, fleshIndex] : CraniumFleshMapping())
    {
        fleshVertices.col(fleshIndex) = GetCraniumMesh().Vertices().col(craniumIndex);
    }
    for (const auto& [mandibleIndex, fleshIndex] : MandibleFleshMapping())
    {
        fleshVertices.col(fleshIndex) = GetMandibleMesh().Vertices().col(mandibleIndex);
    }
    SetFleshMeshVertices(fleshVertices);
}

// explicitly instantiate VolumetricFaceModel for float and double
template class VolumetricFaceModel<float>;
template class VolumetricFaceModel<double>;

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
