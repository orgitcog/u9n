// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>
#include <carbon/io/JsonIO.h>
#include <nls/math/Math.h>

#include <string>
#include <vector>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

/**
 * Simple helper class returning the weights per vertex, the indices of all non-zero vertices,
 * or the indices and weights of all non-zero vertices.
 */
template <class T>
class VertexWeights
{
public:
    VertexWeights() = default;
    ~VertexWeights() = default;
    VertexWeights(VertexWeights&&) = default;
    VertexWeights(const VertexWeights&) = default;
    VertexWeights& operator=(VertexWeights&&) = default;
    VertexWeights& operator=(const VertexWeights&) = default;

    //! constructor of weights of size @p numVertices and weights @p weightForAll
    VertexWeights(int numVertices, T weightForAll);

    //! constructor loading weights @p weightsName from json @p json targeting a mask of size @p numVertices
    VertexWeights(const JsonElement& json, const std::string& weightsName, int numVertices);

    //! constructor loading weights @p weightsName from file @p filename (either .json or .csv file) targeting a mask of size @p numVertices
    VertexWeights(const std::string& filename, const std::string& weightsName, int numVertices);

    //! constructor using weight @p weights
    VertexWeights(const Eigen::VectorX<T>& weights);

    int NumVertices() const { return int(m_weights.size()); }

    const Eigen::VectorX<T>& Weights() const { return m_weights; }

    const std::vector<int>& NonzeroVertices() const { return m_nonzeroVertices; }

    const std::vector<std::pair<int, T>>& NonzeroVerticesAndWeights() const { return m_nonzeroVerticesAndWeights; }

    //! Saves the vertex weights to a json structure using name @p weightsName
    void Save(JsonElement& json, const std::string& weightsName) const;

    //! Loads the vertex weights from a json dictionary with key @p weightsName targeting a mask of size @p numVertices
    void Load(const JsonElement& json, const std::string& weightsName, int numVertices);

    //! Loads the vertex weights @p weightsName from a file @p filename (either .json or .csv file) targeting a mask of size @p numVertices
    void Load(const std::string& filename, const std::string& weightsName, int numVertices);

    //! Loads all vertex weights from json file @p json targeting a mask of size @p numVertices
    static std::map<std::string, VertexWeights> LoadAllVertexWeights(const JsonElement& json, int numVertices);

    //! Loads all vertex weights from file or directory @p fileOrDirectory (either .json or directory of .csv files) targeting a mask of size @p numVertices
    static std::map<std::string, VertexWeights> LoadAllVertexWeights(const std::string& fileOrDirectory, int numVertices);

    //! Saves all vertex weights @p vertexWeights to json file @p filename
    static void SaveToJson(const std::string& filename, const std::map<std::string, VertexWeights<T>>& vertexWeights);

    //! Saves all vertex weights @p vertexWeights to directory @p dirname as CSV files
    static void SaveToDirectory(const std::string& dirname, const std::map<std::string, VertexWeights<T>>& vertexWeights);

    //! Inverts the vertex weights i.e. weight = 1 - weight
    void Invert();

    //! @return vertex weights that are inverted i.e. weight = 1- weight
    VertexWeights Inverted() const;

    template <typename S>
    VertexWeights<S> Cast() const
    {
        return VertexWeights<S>(Weights().template cast<S>());
    }

private:
    void CalculateNonzeroData();

private:
    Eigen::VectorX<T> m_weights;
    std::vector<int> m_nonzeroVertices;
    std::vector<std::pair<int, T>> m_nonzeroVerticesAndWeights;
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
