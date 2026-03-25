// Copyright Epic Games, Inc. All Rights Reserved.

#include <nrr/EyelashesGenerator.h>
#include <nls/serialization/AffineSerialization.h>


CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
bool EyelashesGeneratorParams<T>::ReadJson(const JsonElement& element)
{
    if (!element.IsObject())
    {
        LOG_ERROR("params json is not an object");
        return false;
    }

    const auto& paramsMap = element.Object();

    if (paramsMap.contains("roots") && paramsMap.at("roots").IsArray())
    {
        try
        {
            eyelashesRoots = paramsMap.at("roots").Get<std::vector<std::pair<int, T>>>();
        }
        catch (const std::exception& e)
        {
            LOG_ERROR("failed to load roots parameter with error {}", e.what());
            return false;
        }
    }
    else
    {
        LOG_ERROR("Failed to find roots parameter");
        return false;
    }

    return true;
}

template <class T>
bool ToBinaryFile(FILE* pFile, const EyelashesGeneratorParams<T>& params)
{
    bool success = true;
    success &= io::ToBinaryFile(pFile, params.version);
    success &= io::ToBinaryFile(pFile, params.eyelashesRoots.size());

    for (size_t i = 0; i < params.eyelashesRoots.size(); ++i)
    {
        success &= io::ToBinaryFile(pFile, params.eyelashesRoots[i]);
    }
    return success;
}

template <class T>
bool FromBinaryFile(FILE* pFile, EyelashesGeneratorParams<T>& params)
{
    bool success = true;
    int32_t version;
    success &= io::FromBinaryFile(pFile, version);
    if (success && version == 1)
    {
        size_t numEyelashesRoots;
        success &= io::FromBinaryFile(pFile, numEyelashesRoots);
        if (success)
        {
            params.eyelashesRoots.resize(numEyelashesRoots);
            for (size_t i = 0; i < params.eyelashesRoots.size(); ++i)
            {
                success &= io::FromBinaryFile(pFile, params.eyelashesRoots[i]);
            }
        }
    }
    else
    {
        success = false;
    }
    return success;
}

template <class T>
void EyelashesGenerator<T>::SetMeshes(const std::shared_ptr<const Mesh<T>>& driverMesh, const std::shared_ptr<const Mesh<T>>& eyelashesMesh)
{
    m_driverMesh = driverMesh;
    m_eyelashesMesh = eyelashesMesh;
}


template <class T>
bool EyelashesGenerator<T>::Init(const std::shared_ptr<const Mesh<T>>& driverMesh, const std::shared_ptr<const Mesh<T>>& eyelashesMesh, const EyelashesGeneratorParams<T>& params)
{
    if (!driverMesh || !eyelashesMesh)
    {
        CARBON_CRITICAL("driver and eyelashes meshes must be initialized");
    }

    SetMeshes(driverMesh, eyelashesMesh);
    
    return EyelashConnectedVertices<T>::InitializeEyelashMapping(*m_driverMesh, *m_eyelashesMesh, params.eyelashesRoots, m_eyelashesConnectedVertices);
}

template <class T>
void EyelashesGenerator<T>::Apply(const Eigen::Matrix<T, 3, -1>& deformedDriverMeshVertices, Eigen::Matrix<T, 3, -1>& deformedEyelashesMeshVertices) const
{
    if (!m_driverMesh)
    {
        CARBON_CRITICAL("eyelashes generator is not initialized");
    }

    if (deformedDriverMeshVertices.cols() != m_driverMesh->Vertices().cols())
    {
        CARBON_CRITICAL("incorrect number of driver vertices for eyelashes generator");
    }

    EyelashConnectedVertices<T>::ApplyEyelashMapping(*m_driverMesh, deformedDriverMeshVertices, *m_eyelashesMesh, m_eyelashesConnectedVertices, deformedEyelashesMeshVertices);
}

template <class T>
bool ToBinaryFile(FILE* pFile, const EyelashesGenerator<T>& eyelashesGenerator)
{
    bool success = true;
    success &= io::ToBinaryFile(pFile, eyelashesGenerator.m_version);
    success &= io::ToBinaryFile(pFile, eyelashesGenerator.m_driverMesh);
    success &= io::ToBinaryFile(pFile, eyelashesGenerator.m_eyelashesMesh);

    success &= io::ToBinaryFile(pFile, eyelashesGenerator.m_eyelashesConnectedVertices.size());
    for (size_t i = 0; i < eyelashesGenerator.m_eyelashesConnectedVertices.size(); ++i)
    {
        if (!eyelashesGenerator.m_eyelashesConnectedVertices[i])
        {
            return false;
        }
        success &= ToBinaryFile(pFile, *eyelashesGenerator.m_eyelashesConnectedVertices[i]);
    }

    return success;
}

template <class T>
bool FromBinaryFile(FILE* pFile, EyelashesGenerator<T>& eyelashesGenerator)
{
    bool success = true;
    int32_t version;
    success &= io::FromBinaryFile<int32_t>(pFile, version);
    if (success && version == 1)
    {
        std::shared_ptr<Mesh<T>> driverMesh, eyelashesMesh;
        success &= io::FromBinaryFile(pFile, driverMesh);
        eyelashesGenerator.m_driverMesh = driverMesh;
        success &= io::FromBinaryFile(pFile, eyelashesMesh);
        eyelashesGenerator.m_eyelashesMesh = eyelashesMesh;

        size_t numEyelashConnectedVertices;
        success &= io::FromBinaryFile(pFile, numEyelashConnectedVertices);
        if (success)
        {
            eyelashesGenerator.m_eyelashesConnectedVertices.resize(numEyelashConnectedVertices);
            for (size_t i = 0; i < eyelashesGenerator.m_eyelashesConnectedVertices.size(); ++i)
            {
                eyelashesGenerator.m_eyelashesConnectedVertices[i] = std::make_shared<EyelashConnectedVertices<T>>();
                success &= FromBinaryFile(pFile, *eyelashesGenerator.m_eyelashesConnectedVertices[i]);
            }
        }

        EyelashConnectedVertices<T>::Reduce(eyelashesGenerator.m_eyelashesConnectedVertices);
    }
    else
    {
        success = false;
    }
    return success;
}


template class EyelashesGenerator<float>;
template class EyelashesGenerator<double>;

template struct EyelashesGeneratorParams<float>;
template struct EyelashesGeneratorParams<double>;

template bool ToBinaryFile(FILE* pFile, const EyelashesGeneratorParams<float>& params);
template bool ToBinaryFile(FILE* pFile, const EyelashesGeneratorParams<double>& params);

template bool FromBinaryFile(FILE* pFile, EyelashesGeneratorParams<float>& params);
template bool FromBinaryFile(FILE* pFile, EyelashesGeneratorParams<double>& params);

template bool ToBinaryFile(FILE* pFile, const EyelashesGenerator<float>& eyelashesGenerator);
template bool ToBinaryFile(FILE* pFile, const EyelashesGenerator<double>& eyelashesGenerator);

template bool FromBinaryFile(FILE* pFile, EyelashesGenerator<float>& eyelashesGenerator);
template bool FromBinaryFile(FILE* pFile, EyelashesGenerator<double>& eyelashesGenerator);


CARBON_NAMESPACE_END(TITAN_NAMESPACE)
