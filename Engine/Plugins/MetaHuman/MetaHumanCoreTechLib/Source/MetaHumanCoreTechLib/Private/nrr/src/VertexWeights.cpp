// Copyright Epic Games, Inc. All Rights Reserved.

#include <nrr/VertexWeights.h>
#include <nls/serialization/EigenSerialization.h>
#include <carbon/io/Utils.h>

#include <filesystem>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
VertexWeights<T>::VertexWeights(int numVertices, T weightForAll)
{
    m_weights = Eigen::VectorX<T>::Constant(numVertices, weightForAll);
    CalculateNonzeroData();
}

template <class T>
VertexWeights<T>::VertexWeights(const JsonElement& json, const std::string& weightsName, int numVertices)
{
    Load(json, weightsName, numVertices);
}

template <class T>
VertexWeights<T>::VertexWeights(const std::string& filename, const std::string& weightsName, int numVertices)
{
    Load(filename, weightsName, numVertices);
}

template <class T>
VertexWeights<T>::VertexWeights(const Eigen::VectorX<T>& weights) : m_weights(weights) { CalculateNonzeroData(); }

template <class T>
void VertexWeights<T>::Save(JsonElement& json, const std::string& weightsName) const
{
    JsonElement arr(JsonElement::JsonType::Array);
    const std::vector<std::pair<int, T>>& nonzeroWeights = NonzeroVerticesAndWeights();
    for (auto&& [vID, weight] : nonzeroWeights)
    {
        JsonElement innerArr(JsonElement::JsonType::Array);
        innerArr.Append(JsonElement(vID));
        innerArr.Append(JsonElement(weight));
        arr.Append(std::move(innerArr));
    }
    json.Insert(weightsName, std::move(arr));
}

template <class T>
void VertexWeights<T>::Load(const JsonElement& json, const std::string& weightsName, int numVertices)
{
    if (json.Contains(weightsName))
    {
        if (!json[weightsName].IsArray())
        {
            CARBON_CRITICAL("vertex mask \"{}\" is not an array", weightsName);
        }
        if ((json[weightsName].Size() > 0) && json[weightsName][0].IsArray())
        {
            m_weights = Eigen::VectorX<T>::Zero(numVertices);
            for (const auto& item : json[weightsName].Array())
            {
                m_weights[item[0].Get<int>()] = item[1].Get<T>();
            }
        }
        else if (json[weightsName].Size() > 0)
        {
            Eigen::VectorX<T> weights;
            io::FromJson(json[weightsName], weights);
            if ((int)weights.size() != numVertices)
            {
                CARBON_CRITICAL("vertex mask {} does not have the right size: {} instead of {}", weightsName, weights.size(), numVertices);
            }
            m_weights = weights;
        }
        else
        {
            m_weights = Eigen::VectorX<T>::Zero(numVertices);
        }
        CalculateNonzeroData();
    }
    else
    {
        CARBON_CRITICAL("no vertex mask data in json with name {}", weightsName);
    }
}

template <class T>
std::vector<T> LoadWeightsFromCSV(const std::string& filename)
{
    FILE* pFile = nullptr;
    #ifdef _MSC_VER
    fopen_s(&pFile, filename.c_str(), "r");
    #else
    pFile = fopen(filename.c_str(), "r");
    #endif
    std::vector<T> weights;
    if (pFile)
    {
        while (true)
        {
            float weight = 0;
            #ifdef _MSC_VER
            if (fscanf_s(pFile, "%f", &weight) != 1) { break; }
            #else
            if (fscanf(pFile, "%f", &weight) != 1) { break; }
            #endif
            weights.push_back(T(weight));
        }
        fclose(pFile);
    }
    return weights;
}

template <class T>
void VertexWeights<T>::Load(const std::string& filename, const std::string& weightsName, int numVertices)
{
    std::filesystem::path path(filename);
    if (path.extension().string() == ".json")
    {
        const std::string weightsData = ReadFile(filename);
        const JsonElement json = ReadJson(weightsData);
        Load(json, weightsName, numVertices);
    }
    else if (path.extension().string() == ".csv")
    {
        const std::string stem = path.stem().string();
        if (stem != weightsName)
        {
            LOG_WARNING("attempting to load a csv mask using a different mask: weight name {} but file is called {}", weightsName, stem);
        }
        const std::vector<T> weights = LoadWeightsFromCSV<T>(path.string());
        if (int(weights.size()) == numVertices)
        {
            m_weights = Eigen::VectorX<T>::Map(weights.data(), weights.size());
            CalculateNonzeroData();
        }
        else
        {
            CARBON_CRITICAL("csv contains vertices of size {} instead of {}", weights.size(), numVertices);
        }
    }
    else
    {
        CARBON_CRITICAL("unsupported file extension for masks: {}", path.extension().string());
    }
}

template <class T>
std::map<std::string, VertexWeights<T>> VertexWeights<T>::LoadAllVertexWeights(const JsonElement& json, int numVertices)
{
    std::map<std::string, VertexWeights> vertexWeights;
    for (const auto& [regionName, _] : json.Map())
    {
        vertexWeights.emplace(regionName, VertexWeights(json, regionName, numVertices));
    }
    return vertexWeights;
}

template <class T>
std::map<std::string, VertexWeights<T>> VertexWeights<T>::LoadAllVertexWeights(const std::string& fileOrDirectory, int numVertices)
{
    if (std::filesystem::is_directory(fileOrDirectory))
    {
        // load masks from csv files
        std::map<std::string, VertexWeights<T>> masks;
        std::filesystem::directory_iterator directoryIterator(fileOrDirectory);
        for (const auto& entry : directoryIterator)
        {
            const auto& path = entry.path();
            const std::string stem = path.stem().string();
            const std::string extension = path.extension().string();
            if (extension == ".csv")
            {
                const std::vector<T> weights = LoadWeightsFromCSV<T>(path.string());
                if (numVertices == -1) { numVertices = (int)weights.size(); }
                if (int(weights.size()) == numVertices)
                {
                    masks[stem] = VertexWeights<T>(Eigen::VectorX<T>::Map(weights.data(), weights.size()));
                }
                else
                {
                    CARBON_CRITICAL("csv contains vertices of size {} instead of {}", weights.size(), numVertices);
                }
            }
        }
        return masks;
    }
    else
    {
        // assume it is a json file
        TITAN_NAMESPACE::JsonElement jsonMasks = TITAN_NAMESPACE::ReadJson(ReadFile(fileOrDirectory));
        return LoadAllVertexWeights(jsonMasks, numVertices);
    }
}

template <class T>
void VertexWeights<T>::SaveToJson(const std::string& filename, const std::map<std::string, VertexWeights<T>>& vertexWeights)
{
    TITAN_NAMESPACE::JsonElement json(TITAN_NAMESPACE::JsonElement::JsonType::Object);
    for (const auto&[maskName, maskWeights] : vertexWeights)
    {
        maskWeights.Save(json, maskName);
    }
    WriteFile(filename, TITAN_NAMESPACE::WriteJson(json));
}

template <class T>
void VertexWeights<T>::SaveToDirectory(const std::string& dirname, const std::map<std::string, VertexWeights<T>>& vertexWeights)
{
    if (std::filesystem::exists(dirname))
    {
        if (!std::filesystem::is_directory(dirname))
        {
            CARBON_CRITICAL("{} is not a directory", dirname);
        }
    }
    else
    {
        std::filesystem::create_directories(dirname);
    }

    for (const auto&[maskName, maskWeights] : vertexWeights)
    {
        const std::string filename = dirname + "/" + maskName + ".csv";
        FILE* pFile = nullptr;
        #ifdef _MSC_VER
        fopen_s(&pFile, filename.c_str(), "w");
        #else
        pFile = fopen(filename.c_str(), "w");
        #endif
        if (pFile)
        {
            for (int i = 0; i < maskWeights.NumVertices(); ++i)
            {
                fprintf(pFile, "%f\n", maskWeights.Weights()[i]);
            }
            fclose(pFile);
        }
    }
}

template <class T>
void VertexWeights<T>::CalculateNonzeroData()
{
    m_nonzeroVertices.clear();
    m_nonzeroVerticesAndWeights.clear();
    for (int i = 0; i < NumVertices(); i++)
    {
        if (m_weights[i] != 0)
        {
            m_nonzeroVertices.push_back(i);
            m_nonzeroVerticesAndWeights.push_back({ i, m_weights[i] });
        }
    }
}

template <class T>
void VertexWeights<T>::Invert()
{
    for (int i = 0; i < NumVertices(); i++)
    {
        m_weights[i] = T(1) - m_weights[i];
    }
    CalculateNonzeroData();
}

template <class T>
VertexWeights<T> VertexWeights<T>::Inverted() const
{
    VertexWeights<T> out = *this;
    out.Invert();
    return out;
}

// explicitly instantiate the VertexWeights classes
template class VertexWeights<float>;
template class VertexWeights<double>;

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
