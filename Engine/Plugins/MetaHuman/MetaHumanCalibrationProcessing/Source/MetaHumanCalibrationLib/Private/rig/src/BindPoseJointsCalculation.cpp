// Copyright Epic Games, Inc. All Rights Reserved.

#include <rig/BindPoseJointsCalculation.h>
#include <carbon/io/JsonIO.h>
#include <carbon/io/Utils.h>

#include <filesystem>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

enum class CalculationType
{
    Triangle,
    Line,
    Mean
};

enum CalculationPlane
{
    NONE, YZ, XZ
};

Eigen::Vector3f CalculateMean(const std::vector<int>& vIDs, const Eigen::Matrix<float, 3, -1>& meshVertices)
{
    Eigen::Matrix<float, 3, -1> selectedCols = meshVertices(Eigen::all, vIDs);
    return selectedCols.rowwise().mean();
}

Eigen::Vector3f LineInterpolation(const Eigen::Vector3f& v1, const Eigen::Vector3f& v2, float distToV1) { return v1 + distToV1 * (v2 - v1); }

Eigen::Vector3f TriangleBasedCalculation(const Eigen::Vector3f& A, const Eigen::Vector3f& B, float angleDegrees, CalculationPlane calcPlane)
{
    Eigen::Vector2f A2D, B2D;

    switch (calcPlane)
    {
        case CalculationPlane::YZ:
            A2D = Eigen::Vector2f(A.y(), A.z());
            B2D = Eigen::Vector2f(B.y(), B.z());
            break;
        case CalculationPlane::XZ:
            A2D = Eigen::Vector2f(A.x(), A.z());
            B2D = Eigen::Vector2f(B.x(), B.z());
            break;
        case  CalculationPlane::NONE:
            CARBON_CRITICAL("calcPlane input is NONE.");
        default:
            CARBON_CRITICAL("Invalid calcPlane input.");
    }

    Eigen::Vector2f M2D = (A2D + B2D) / 2.0f;

    Eigen::Vector2f baseVector = B2D - A2D;

    Eigen::Vector2f normal(-baseVector.y(), baseVector.x());
    Eigen::Vector2f normalizedNormal = normal.normalized();

    float baseLength = baseVector.norm();

    float angleRadians = angleDegrees * (float)CARBON_PI / 180.0f;

    float height = (baseLength / 2.0f) / std::tan(angleRadians / 2.0f);

    Eigen::Vector2f C2D = M2D + height * normalizedNormal;

    Eigen::Vector3f C = Eigen::Vector3f::Zero();
    switch (calcPlane)
    {
        case YZ:
            C = Eigen::Vector3f(0.0f, C2D.x(), C2D.y());
            break;
        case XZ:
            C = Eigen::Vector3f(C2D.x(), 0.0f, C2D.y());
            break;
        case  CalculationPlane::NONE:
            CARBON_CRITICAL("calcPlane input is NONE.");
    }

    return C;
}

struct VolumetricJointTraits
{
    std::string jointName;
    std::string meshName;
    CalculationType calcType;
    std::vector<int> vIDs;
    float distance = -1.0f;
    float angle = -1.0f;
    CalculationPlane calcPlane = CalculationPlane::NONE;
    std::pair<std::string, std::vector<int>> remainingCoord;
    int start_vID = -1;
};

bool LoadVolumetricJointsJson(const std::string& filepath, std::vector<VolumetricJointTraits>& jointsToCalculate)
{
    jointsToCalculate.clear();
    const bool isValidFile = std::filesystem::exists(filepath);
    std::string jsonString;
    if (!isValidFile)
    {
        return false;
    }
    jsonString = TITAN_NAMESPACE::ReadFile(filepath);

    const TITAN_NAMESPACE::JsonElement json = TITAN_NAMESPACE::ReadJson(jsonString);

    if (json.Contains("joint_correspondence"))
    {
        for (const auto& jointData : json["joint_correspondence"].Array())
        {
            VolumetricJointTraits volJointData;
            if (jointData.Contains("joint_name"))
            {
                volJointData.jointName = jointData["joint_name"].String();
            }
            else
            {
                LOG_ERROR("every input must have joint name.");
                return false;
            }

            if (jointData.Contains("mesh_name"))
            {
                volJointData.meshName = jointData["mesh_name"].String();
            }
            else
            {
                LOG_ERROR("every input must have mesh name.");
                return false;
            }

            if (jointData.Contains("start_vID"))
            {
                volJointData.start_vID = jointData["start_vID"].Get<int>();
            }

            if (jointData.Contains("calc_type"))
            {
                const std::string calcTypeString = jointData["calc_type"].String();
                if (calcTypeString == "triangle")
                {
                    volJointData.calcType = CalculationType::Triangle;
                }
                else if (calcTypeString == "line")
                {
                    volJointData.calcType = CalculationType::Line;
                }
                else if (calcTypeString == "mean")
                {
                    volJointData.calcType = CalculationType::Mean;
                }
                else
                {
                    LOG_ERROR("calc_type not supported");
                    return false;
                }
            }
            else
            {
                LOG_ERROR("every input must have calculation type.");
            }

            if (jointData.Contains("plane"))
            {
                const std::string planeString = jointData["plane"].String();
                if (planeString == "YZ")
                {
                    volJointData.calcPlane = CalculationPlane::YZ;
                }
                else if (planeString == "XZ")
                {
                    volJointData.calcPlane = CalculationPlane::XZ;
                }
                else
                {
                    LOG_ERROR("calc_type not supported");
                    return false;
                }
            }

            if (jointData.Contains("vIDs"))
            {
                const auto vIDs = jointData["vIDs"].Get<std::vector<int>>();
                volJointData.vIDs = vIDs;
            }

            if (jointData.Contains("remaining_coord_string"))
            {
                volJointData.remainingCoord.first = jointData["remaining_coord_string"].String();
            }

            if (jointData.Contains("remaining_coord_array"))
            {
                volJointData.remainingCoord.second = jointData["remaining_coord_array"].Get<std::vector<int>>();
            }

            if (jointData.Contains("angle"))
            {
                volJointData.angle = jointData["angle"].Get<float>();
            }
            if (jointData.Contains("distance"))
            {
                volJointData.distance = jointData["distance"].Get<float>();
            }
            jointsToCalculate.push_back(volJointData);
        }
    }

    return true;
}

bool LoadSurfaceJointsMapFromJson(const std::string& filepath, std::map<std::string, int>& jointToVtxId)
{
    if (std::filesystem::exists(filepath))
    {
        jointToVtxId.clear();
        const TITAN_NAMESPACE::JsonElement json = TITAN_NAMESPACE::ReadJson(TITAN_NAMESPACE::ReadFile(filepath));
        if (json.Contains("joint_correspondence"))
        {
            for (const auto& element : json["joint_correspondence"].Array())
            {
                const std::string jointName = element["joint_name"].String();
                const int vID = element["vID"].Get<int>();
                jointToVtxId[jointName] = vID;
            }
        }
    }
    else
    {
        LOG_ERROR("File {} does not exist.", filepath);
        return false;
    }

    return true;
}

struct BindPoseJointsCalculation::Private
{
    std::vector<VolumetricJointTraits> jointsToCalculate;
    std::map<std::string, int> surfaceJointsMapping;
};

BindPoseJointsCalculation::BindPoseJointsCalculation() : m(new Private) {}
BindPoseJointsCalculation::~BindPoseJointsCalculation() = default;
BindPoseJointsCalculation::BindPoseJointsCalculation(BindPoseJointsCalculation&& other) = default;
BindPoseJointsCalculation& BindPoseJointsCalculation::operator=(BindPoseJointsCalculation&& other) = default;

bool BindPoseJointsCalculation::LoadVolumetricConfig(const std::string& filepath) { return LoadVolumetricJointsJson(filepath, m->jointsToCalculate); }

bool BindPoseJointsCalculation::LoadSurfaceConfig(const std::string& filepath) { return LoadSurfaceJointsMapFromJson(filepath, m->surfaceJointsMapping); }

bool BindPoseJointsCalculation::Load(const std::string& surfaceFilepath, const std::string& volumetricFilepath)
{
    bool success = true;
    success &= LoadSurfaceConfig(surfaceFilepath);
    success &= LoadVolumetricConfig(volumetricFilepath);

    return success;
}

bool BindPoseJointsCalculation::VolumetricDataLoaded() const
{
    return !m->jointsToCalculate.empty();
}

bool BindPoseJointsCalculation::SurfaceDataLoaded() const
{
    return !m->surfaceJointsMapping.empty();
}

std::vector<std::string>  BindPoseJointsCalculation::SurfaceJointsList() const
{
    std::vector<std::string> output;

    for (const auto& [jointName, id] : m->surfaceJointsMapping)
    {
        output.push_back(jointName);
    }
    return output;
}

void BindPoseJointsCalculation::Update(Eigen::Matrix<float, 3, -1>& vertices,
                                       const int jointOffset,
                                       const std::map<std::string, std::pair<int, int>>& meshMapping,
                                       const std::map<std::string, int>& jointNameToId) const
{
    UpdateSurface(vertices, jointOffset, jointNameToId);
    UpdateVolumetric(vertices, meshMapping, jointNameToId);
}

void BindPoseJointsCalculation::UpdateSurface(Eigen::Matrix<float, 3, -1>& vertices, const int jointOffset,
                                              const std::map<std::string, int>& jointNameToId) const
{
    for (const auto& [jName, vID] : m->surfaceJointsMapping)
    {
        vertices.col(jointNameToId.at(jName)) = vertices.col(jointOffset + vID);
    }
}

void BindPoseJointsCalculation::UpdateVolumetric(Eigen::Matrix<float, 3, -1>& vertices,
                                                 const std::map<std::string, std::pair<int, int>>& meshMapping,
                                                 const std::map<std::string, int>& jointNameToId) const
{
    auto calculateRemainingCoord = [&](const VolumetricJointTraits& jointToCalculate, Eigen::Vector3f& result)
        {
            if (jointToCalculate.calcPlane != CalculationPlane::NONE)
            {
                const auto [coordString, coordVec] = jointToCalculate.remainingCoord;
                if (!coordString.empty())
                {
                    int jntId = jointNameToId.at(coordString);
                    Eigen::Vector3f coordinates = vertices.col(jntId);

                    if (jointToCalculate.calcPlane == CalculationPlane::XZ)
                    {
                        result.y() = coordinates.y();
                    }
                    else if (jointToCalculate.calcPlane == CalculationPlane::YZ)
                    {
                        result.x() = coordinates.x();
                    }
                    else
                    {
                        LOG_ERROR("caclulation plane does not exist");
                    }
                }
                else if (!coordVec.empty())
                {
                    Eigen::Vector3f coordinates = Eigen::Vector3f::Zero();
                    const auto& [meshBegin, meshEnd] = meshMapping.at(jointToCalculate.meshName);
                    const auto meshVertices = vertices(Eigen::all, Eigen::seqN(meshBegin, meshEnd - meshBegin));
                    if (coordVec.size() > 2)
                    {
                        coordinates = CalculateMean(coordVec, meshVertices);
                    }
                    else if (coordVec.size() == 2)
                    {
                        const auto v1 = meshVertices.col(coordVec[0]);
                        const auto v2 = meshVertices.col(coordVec[1]);

                        coordinates = LineInterpolation(v1, v2, jointToCalculate.distance);
                    }
                    else if (coordVec.size() == 1)
                    {
                        coordinates = meshVertices.col(coordVec[0]);
                    }
                    else
                    {
                        LOG_ERROR("Coord vec size not supported.");
                    }

                    if (jointToCalculate.calcPlane == CalculationPlane::XZ)
                    {
                        result.y() = coordinates.y();
                    }
                    else if (jointToCalculate.calcPlane == CalculationPlane::YZ)
                    {
                        result.x() = coordinates.x();
                    }
                    else
                    {
                        LOG_ERROR("caclulation plane does not exist");
                    }
                }
            }
        };

    for (const auto& jointToCalculate : m->jointsToCalculate)
    {
        const auto& [meshBegin, meshEnd] = meshMapping.at(jointToCalculate.meshName);
        const auto meshVertices = vertices(Eigen::all, Eigen::seqN(meshBegin, meshEnd - meshBegin));
        if (jointToCalculate.calcType == CalculationType::Mean)
        {
            auto result = CalculateMean(jointToCalculate.vIDs, meshVertices);
            calculateRemainingCoord(jointToCalculate, result);

            int jointId = jointNameToId.at(jointToCalculate.jointName);
            vertices.col(jointId) = result;
        }
        else if (jointToCalculate.calcType == CalculationType::Line)
        {
            if ((jointToCalculate.vIDs.size() != 2) && (jointToCalculate.start_vID < 0))
            {
                CARBON_CRITICAL("Calc type is Line but input contains more than 2 vIDs.");
            }

            if (jointToCalculate.distance < 0)
            {
                CARBON_CRITICAL("Calc type is Line but distance is not set.");
            }

            Eigen::Vector3f result;
            if (jointToCalculate.start_vID >= 0)
            {
                auto vIDs = jointToCalculate.vIDs;
                vIDs.erase(vIDs.begin());
                const Eigen::Vector3f v1 = meshVertices.col(jointToCalculate.vIDs[0]);

                const Eigen::Vector3f v2 = CalculateMean(vIDs, meshVertices);
                result = LineInterpolation(v1, v2, jointToCalculate.distance);
            }
            else
            {
                const Eigen::Vector3f v1 = meshVertices.col(jointToCalculate.vIDs[0]);
                const Eigen::Vector3f v2 = meshVertices.col(jointToCalculate.vIDs[1]);

                result = LineInterpolation(v1, v2, jointToCalculate.distance);
            }

            int jointId = jointNameToId.at(jointToCalculate.jointName);
            vertices.col(jointId) = result;
        }
        else if (jointToCalculate.calcType == CalculationType::Triangle)
        {
            const Eigen::Vector3f v1 = meshVertices.col(jointToCalculate.vIDs[0]);
            const Eigen::Vector3f v2 = meshVertices.col(jointToCalculate.vIDs[1]);
            auto result = TriangleBasedCalculation(v1, v2, jointToCalculate.angle, jointToCalculate.calcPlane);
            calculateRemainingCoord(jointToCalculate, result);
            int jointId = jointNameToId.at(jointToCalculate.jointName);
            vertices.col(jointId) = result;
        }
        else
        {
            CARBON_CRITICAL("Calculation type not supported.");
        }
    }
}

CARBON_NAMESPACE_END(TITAN_NAMESPACE)