// Copyright Epic Games, Inc. All Rights Reserved.

#include <bodyshapeeditor/BodyMeasurement.h>
#include <carbon/io/JsonIO.h>
#include <carbon/utils/Timer.h>
#include <nls/geometry/BarycentricCoordinates.h>
#include <nls/geometry/CatmullRom.h>
#include <nls/geometry/MeshTools.h>
#include <nls/geometry/Mesh.h>
#include <nls/math/Math.h>

#include "BodyMeasurementHelper.h"

#include <string>
#include <vector>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

BodyMeasurement BodyMeasurement::CreateSemanticMeasurement(const std::string& name, const Eigen::VectorXf& weights)
{
    BodyMeasurement bodyMeasurement;
    bodyMeasurement.m_type = Semantic;
    bodyMeasurement.m_name = name;
    bodyMeasurement.m_weights = weights;
    bodyMeasurement.m_normal = { 0.0f, 0.0f, 0.0f };
    return bodyMeasurement;
}

std::vector<BodyMeasurement> BodyMeasurement::FromJSON(const JsonElement& json, const Eigen::Matrix3Xf& baseVertices)
{
    std::vector<BodyMeasurement> measurements;
    std::vector<std::string> possibleMeasurementKeys = {"contours", "measurements"};
    std::string measurementKey;
    const int numVertices = (int)baseVertices.cols();

    for (const auto& key : possibleMeasurementKeys)
    {
        if (json.Contains(key) && json[key].IsArray())
        {
            measurementKey = key;
        }
    }

    if (!measurementKey.empty())
    {
        for (const auto& item : json[measurementKey].Array())
        {
            if (!item.Contains("type"))
            {
                CARBON_CRITICAL("Invalid json file. Missing \"type\" key in one or more element.");
            }

            if (!item.Contains("name"))
            {
                CARBON_CRITICAL("Invalid json file. Missing \"name\" key in one or more element.");
            }

            BodyMeasurement result;

            const std::string type = item["type"].Get<std::string>();
            if (type == "Circumference")
            {
                result.m_type = BodyMeasurement::Circumference;
            }
            else if (type == "Axis")
            {
                result.m_type = BodyMeasurement::Axis;
            }
            else if (type == "SampledLine")
            {
                result.m_type = BodyMeasurement::SampledLine;
            }
            else if (type == "IndexedLine")
            {
                result.m_type = BodyMeasurement::IndexedLine;
            }
            else if (type == "Semantic")
            {
                result.m_type = BodyMeasurement::Semantic;
            }
            else
            {
                CARBON_CRITICAL("Invalid json file. Unknown \"type\" key in one or more element.");
            }

        	

            result.m_name = GetAlias(item["name"].Get<std::string>());
            if (result.m_type != BodyMeasurement::Semantic)
            {
                result.m_vertexIDs = item["vertex"].Get<std::vector<int>>();
                if (result.m_type == BodyMeasurement::Axis && result.m_vertexIDs.size() != 2)
                {
                    CARBON_CRITICAL("axis measurement {} needs to contain two vertices", result.m_name);
                }
                for (int i = 0; i < (int)result.m_vertexIDs.size(); ++i)
                {
                    if (result.m_vertexIDs[i] >= numVertices)
                    {
                        CARBON_CRITICAL("invalid vertex id for measurement {} ({}): {} ({} of {})", result.m_name, type, result.m_vertexIDs[i], i, result.m_vertexIDs.size());
                    }
                    if (result.m_vertexIDs[i] < 0)
                    {
                        if (result.m_type == BodyMeasurement::Axis)
                        {
                            if (i == 0)
                            {
                                int lowPointIndex{};
                                baseVertices.row(1).minCoeff(&lowPointIndex);
                                result.m_vertexIDs[i] = lowPointIndex;
                            }
                            else
                            {
                                int highPointIndex{};
                                baseVertices.row(1).maxCoeff(&highPointIndex);
                                result.m_vertexIDs[i] = highPointIndex;
                            }
                        }
                        else
                        {
                            CARBON_CRITICAL("invalid vertex id for measurement {} ({}): {} ({} of {})", result.m_name, type, result.m_vertexIDs[i], i, result.m_vertexIDs.size());
                        }
                    }
                }
            }
            else
            {
                const std::vector<float> weights = item["weights"].Get<std::vector<float>>();
                result.m_weights = Eigen::Map<const Eigen::VectorXf>(weights.data(), weights.size());
            }
            std::vector<float> normal{ 0.0f, 0.0f, 0.0f };
            if (item.Contains("normal") && item["normal"].IsArray())
            {
                normal = item["normal"].Get<std::vector<float>>();
            }
            result.m_normal = { normal[0], normal[1], normal[2] };
            result.m_numVertices = numVertices;
            if(item.Contains("minInputValue"))
            {
                result.minInputValue = item["minInputValue"].Value<float>();
            }
            if(item.Contains("maxInputValue"))
            {
                result.maxInputValue = item["maxInputValue"].Value<float>();
            }
            measurements.emplace_back(result);
        }
    }
    return measurements;
}

Eigen::VectorXf BodyMeasurement::GetBodyMeasurements(const std::vector<BodyMeasurement>& measurements,
                                                     const Eigen::Matrix<float, 3, -1>& mesh,
                                                     const Eigen::VectorXf& RawControls)
{
    Eigen::VectorXf result = Eigen::VectorXf::Zero(measurements.size());

    for (size_t i = 0; i < measurements.size(); i++)
    {
        if ((measurements[i].m_type != Semantic) && (measurements[i].m_numVertices != (int)mesh.cols()))
        {
            CARBON_CRITICAL("mesh does not match measurements");
        }

        const auto& measurement = measurements[i];
        if (measurement.m_type == BodyMeasurement::Axis)
        {
            // axis measurement
            const float lowPoint = mesh(1, measurement.m_vertexIDs[0]);
            const float highPoint = mesh(1, measurement.m_vertexIDs[1]);
            result[i] = highPoint - lowPoint;
        }
        // TODO add actual weights
        else if (measurement.m_type == BodyMeasurement::Semantic)
        {
            CARBON_ASSERT(RawControls.size() >= measurement.m_weights.size(), "measurement.cpp semantic constraint has more weights then raw controls: {} vs {}", RawControls.size(), measurement.m_weights.size());
            result[i] = RawControls.head(measurement.m_weights.size()).dot(measurement.m_weights);
        }
        else
        {
            for (int j = 0; j < static_cast<int>(measurement.m_barycentricCoordinates.size()) - 1; j++)
            {
                result[i] += (measurement.m_barycentricCoordinates[j + 1].Evaluate<3>(mesh) - measurement.m_barycentricCoordinates[j].Evaluate<3>(mesh)).norm();
            }
        }
    }
    return result;
}

Eigen::Matrix3Xf BodyMeasurement::GetMeasurementDebugPoints(const Eigen::Matrix3Xf& vertices) const
{
    (void)vertices;
    return Eigen::Map<const Eigen::Matrix3Xf>((const float*)m_debugPoints.data(), 3, (int)m_debugPoints.size());
}

void BodyMeasurement::UpdateBodyMeasurementPoints(std::vector<BodyMeasurement>& measurements,
                                                  const Eigen::Matrix<float, 3, -1>& vertices,
                                                  const Eigen::Matrix<float, 3, -1>& vertexNormals,
                                                  const HalfEdgeMesh<float>& heTopology,
                                                  TaskThreadPool* threadPool,
                                                  bool debug)
{
    int count = 0;
    auto updateMeasurements = [&](int start, int end) {
        for (int ci = start; ci < end; ++ci)
        {
            auto& measurement = measurements[ci];
            const auto previousBarycentricCoordinates = measurement.m_barycentricCoordinates;
            measurement.m_barycentricCoordinates.clear();

            measurement.m_barycentricCoordinates.reserve(measurement.m_vertexIDs.size());
            if (measurement.m_type == BodyMeasurement::Circumference)
            {
                // circumference constraint
                const Eigen::Vector3f& pointOnPlane = vertices.col(measurement.m_vertexIDs[0]);

                for (int j = 0; j < (int)measurement.m_vertexIDs.size(); j++)
                {
                    Eigen::Vector3f res = vertices.col(measurement.m_vertexIDs[j]);
                    float d = (res - pointOnPlane).dot(measurement.m_normal);
                    res = res - d * measurement.m_normal;

                    Eigen::Vector3f offsetDirection = (-1.0f) * (vertexNormals.col(measurement.m_vertexIDs[j]) - (vertexNormals.col(measurement.m_vertexIDs[j]).dot(measurement.m_normal) * measurement.m_normal));

                    count++;
                    const auto& closestFaceCoord = ClosestMeshCoordToLine(heTopology, vertices, measurement.m_vertexIDs[j], res, offsetDirection);
                    measurement.m_barycentricCoordinates.push_back(closestFaceCoord.bc);
                }

                // close the loop
                if (!measurement.m_barycentricCoordinates.empty())
                {
                    measurement.m_barycentricCoordinates.push_back(measurement.m_barycentricCoordinates.front());
                }
            }
            else if (measurement.m_type == BodyMeasurement::IndexedLine)
            {
                // it's a non sampling constraint
                for (size_t i = 0; i < measurement.m_vertexIDs.size(); i++)
                {
                    // TODO bake this we don't need to add em every time
                    measurement.m_barycentricCoordinates.push_back(BarycentricCoordinates<float>({ measurement.m_vertexIDs[i], measurement.m_vertexIDs[i],
                                                                                        measurement.m_vertexIDs[i] },
                                                                                        { 1, 0, 0 }));
                }
            }
            else if (measurement.m_type == BodyMeasurement::SampledLine)
            {
                measurement.m_debugPoints.clear();
                const int numSegments = 16;
                const int raysPerSection = numSegments / (static_cast<int>(measurement.m_vertexIDs.size()) - 1);
                int startIdx = measurement.m_vertexIDs[0];
                measurement.m_barycentricCoordinates.reserve(raysPerSection * (static_cast<int>(measurement.m_vertexIDs.size()) - 1));

                const Eigen::Vector3f direction = (vertices.col(measurement.m_vertexIDs.back()) - vertices.col(measurement.m_vertexIDs.front())).normalized();
                Eigen::Vector3f offsetDirection;
                if (measurement.m_normal.squaredNorm() > 0)
                {
                    offsetDirection = measurement.m_normal.normalized();
                }
                else
                {
                    const Eigen::Vector3f n0 = vertexNormals.col(measurement.m_vertexIDs.front());
                    const Eigen::Vector3f n1 = vertexNormals.col(measurement.m_vertexIDs.back());
                    offsetDirection = (n0 + n1).normalized();
                }

                for (size_t i = 1; i < measurement.m_vertexIDs.size(); i++)
                {
                    const Eigen::Vector3f& o1 = vertices.col(measurement.m_vertexIDs[i - 1]);
                    const Eigen::Vector3f& o2 = vertices.col(measurement.m_vertexIDs[i]);
                    for (int j = 0; j < raysPerSection; j++)
                    {
                        const float t = (float)j / (float)(raysPerSection - 1);
                        const Eigen::Vector3f origin = o1 + t * (o2 - o1);

                        BCoordExt closestFaceCoord = ClosestMeshCoordToLine(heTopology, vertices, startIdx, origin, offsetDirection);

                        if (debug)
                        {
                            Eigen::Vector3f pos = closestFaceCoord.bc.Evaluate<3>(vertices);
                            Eigen::Vector3f newPos = origin + (pos - origin).dot(offsetDirection) * offsetDirection;
                            measurement.m_debugPoints.push_back(origin);
                            measurement.m_debugPoints.push_back(newPos);
                        }

                        startIdx = closestFaceCoord.bc.Index(0);
                        measurement.m_barycentricCoordinates.push_back(closestFaceCoord.bc);
                        count++;
                    }
                }
            }
            measurement.UpdateVisualizationMeasurementPoints(vertices, 5);
        }
    };
    if (threadPool) threadPool->AddTaskRangeAndWait((int)measurements.size(), updateMeasurements);
    else updateMeasurements(0, (int)measurements.size());

}

void BodyMeasurement::UpdateVisualizationMeasurementPoints(const Eigen::Matrix3Xf& vertices, int resampling)
{
    Eigen::Matrix3Xf measurementPoints;

    if (GetType() == BodyMeasurement::Axis)
    {
        measurementPoints = Eigen::Matrix3Xf::Zero(3, 2);
        measurementPoints(1, 0) = vertices(1, m_vertexIDs[0]);
        measurementPoints(1, 1) = vertices(1, m_vertexIDs[1]);
    }
    else
    {
        measurementPoints.resize(3, m_barycentricCoordinates.size());
        for (size_t j = 0; j < m_barycentricCoordinates.size(); j++)
        {
            measurementPoints.col(j) = m_barycentricCoordinates[j].Evaluate<3>(vertices);
        }

        if (GetType() == BodyMeasurement::Circumference && resampling > 0)
        {
            measurementPoints.conservativeResize(Eigen::NoChange, measurementPoints.cols() - 1);
            const Eigen::Vector3f& pointOnPlane = vertices.col(m_vertexIDs[0]);
            auto projectToPlane = [&](const Eigen::Vector3f& point) {
                Eigen::Vector3f n = m_normal;
                float d = (point - pointOnPlane).dot(n);
                return point - d * n;
            };

            Eigen::Matrix3Xf resampledPoints = CatmullRom<float, 3>(measurementPoints, resampling, true).SampledPoints().ControlPoints();
            for (int i = 0; i < (int)resampledPoints.cols(); ++i)
            {
                resampledPoints.col(i) = projectToPlane(resampledPoints.col(i));
            }
            measurementPoints = resampledPoints;
        }
    }

    m_points = measurementPoints;
}

Eigen::Matrix3Xf BodyMeasurement::GetMeasurementPoints() const
{
    return m_points;
}

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
