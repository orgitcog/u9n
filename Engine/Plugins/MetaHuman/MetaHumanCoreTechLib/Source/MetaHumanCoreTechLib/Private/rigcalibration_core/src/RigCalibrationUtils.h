// Copyright Epic Games, Inc. All Rights Reserved.

#include <carbon/io/Utils.h>
#include <nls/geometry/BarycentricCoordinates.h>
#include <nls/geometry/AffineVariable.h>
#include <nls/geometry/QuaternionVariable.h>
#include <nls/utils/Configuration.h>
#include <nls/utils/ConfigurationParameter.h>
#include <rig/RigGeometry.h>


CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

/**
 * @brief Converts the blendshape deltas (vector of matrices) into a vector of DiffData
 *
 * @param[in] input - blendshape delta matrices to be converted
 *
 * @return Vector of DiffDataMatrix that were initialized around the blendshape matrices
 */
inline std::pair<std::vector<int>, std::vector<DiffDataMatrix<float, 3, -1>>> BlendshapeDeltasToDiffDataMat(const std::map<std::string, Eigen::Matrix<float, 3,
                                                                                                                                                      -1>> input,
                                                                                                            const std::shared_ptr<RigGeometry<float>>& rigGeometry)
{
    std::pair<std::vector<int>, std::vector<DiffDataMatrix<float, 3, -1>>> output;

    for (const auto& [name, deltas] : input)
    {
        output.second.push_back(DiffDataMatrix<float, 3, -1>(deltas));
        output.first.push_back(rigGeometry->GetMeshIndex(name));
    }

    return output;
}

inline void LoadConfiguration(const std::string& filename, const std::string& configurationNameSuffix, Configuration& configuration)
{
    const std::string configData = ReadFile(filename);
    JsonElement jsonConfig = ReadJson(configData);
    if (jsonConfig.Contains(configuration.Name() + configurationNameSuffix))
    {
        std::vector<std::string> unspecifiedKeys;
        std::vector<std::string> unknownKeys;

        configuration.FromJson(jsonConfig[configuration.Name() + configurationNameSuffix], unspecifiedKeys, unknownKeys);
        for (const std::string& key : unspecifiedKeys)
        {
            LOG_WARNING("config is not specifying {}", key);
        }
        for (const std::string& key : unknownKeys)
        {
            LOG_WARNING("config contains unknown key {}", key);
        }
    }
}

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
