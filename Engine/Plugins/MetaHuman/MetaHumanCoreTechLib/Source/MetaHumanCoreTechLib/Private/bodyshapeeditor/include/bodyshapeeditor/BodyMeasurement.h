// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/common/Defs.h>
#include <carbon/geometry/AABBTree.h>
#include <carbon/io/JsonIO.h>
#include <nls/geometry/BarycentricCoordinates.h>
#include <nls/geometry/Mesh.h>
#include <nls/geometry/HalfEdgeMesh.h>

#include <arrayview/ArrayView.h>
#include <arrayview/StringView.h>

#include <string>
#include <vector>
#include <limits>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

class BodyMeasurement
{
public:
    enum type_t
    {
        Circumference, SampledLine, IndexedLine, Axis, Semantic
    };

    static std::vector<BodyMeasurement> FromJSON(const JsonElement& json, const Eigen::Matrix3Xf& baseVertices);

	
	
    static Eigen::VectorXf GetBodyMeasurements(const std::vector<BodyMeasurement>& measurements,
                                               const Eigen::Matrix<float, 3, -1>& mesh,
                                               const Eigen::VectorXf& RawControls);

    //! Update all measurement points (note that it is the callers responsibility to ensure that meshTools has been prepared with the right topology and half edge data structure
    static void UpdateBodyMeasurementPoints(std::vector<BodyMeasurement>& measurements,
                                            const Eigen::Matrix<float, 3, -1>& vertices,
                                            const Eigen::Matrix<float, 3, -1>& vertexNormals,
                                            const HalfEdgeMesh<float>& heTopology,
                                            TaskThreadPool* threadPool,
                                            bool debug = false);

    static BodyMeasurement CreateSemanticMeasurement(const std::string& name, const Eigen::VectorXf& weights);

    void UpdateVisualizationMeasurementPoints(const Eigen::Matrix3Xf& vertices, int resampling);

    Eigen::Matrix3Xf GetMeasurementPoints() const;
    Eigen::Matrix3Xf GetMeasurementDebugPoints(const Eigen::Matrix3Xf& vertices) const;
    static std::string GetAlias(const std::string& name)
    {
        const std::array<std::array<std::string,2>,1> nameAliases{
            { // Last name is active one
                {"Bust", "Chest"}
            }
        };
        for (const auto& group : nameAliases)
        {
            if (std::find(group.begin(), group.end(), name) != group.end()) {
                return group.back();
            }
        }
        return  name;
    };

    type_t GetType() const { return m_type; }
    const std::string& GetName() const { return m_name; }
    const std::vector<int>& GetVertexIDs() const { return m_vertexIDs; }
    const Eigen::VectorXf& GetWeights() const { return m_weights; }
    const Eigen::Vector3f& GetNormal() const { return m_normal; }
    float GetMaxInputValue() const { return maxInputValue; }
    float GetMinInputValue() const { return minInputValue; }
    const std::vector<BarycentricCoordinates<float>>& GetBarycentricCoordinates() const { return m_barycentricCoordinates; }

	static constexpr float InvalidValue = std::numeric_limits<float>::min();

private:
    type_t m_type;
    std::string m_name;
    std::vector<int> m_vertexIDs;
    Eigen::VectorXf m_weights;
    Eigen::Vector3f m_normal;
    int m_numVertices{};
    std::vector<BarycentricCoordinates<float>> m_barycentricCoordinates;
    std::vector<Eigen::Vector3f> m_debugPoints;
	float minInputValue = InvalidValue; 
	float maxInputValue = InvalidValue; 
    Eigen::Matrix<float, 3, -1> m_points;
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
