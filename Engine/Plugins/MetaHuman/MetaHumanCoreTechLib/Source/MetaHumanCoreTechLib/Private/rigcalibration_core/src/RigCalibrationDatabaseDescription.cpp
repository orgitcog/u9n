// Copyright Epic Games, Inc. All Rights Reserved.

#include <rigcalibration/RigCalibrationDatabaseDescription.h>
#include <carbon/io/JsonIO.h>
#include <carbon/io/Utils.h>

#include <filesystem>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

const std::string InsertSuffix(const std::string& filename, const std::string& suffix)
{
    size_t dotPosition = filename.find_last_of('.');
    if (dotPosition == std::string::npos) {
        return filename + suffix;
    }
    return filename.substr(0, dotPosition) + suffix + filename.substr(dotPosition);
};

const std::string& RigCalibrationDatabaseDescription::GetExpressionModelPath(const int it) const
{
    return m_loadedModelPaths[it];
}

const std::string& RigCalibrationDatabaseDescription::GetExpressionBlendshapeModelPath(const int it) const
{
    return m_loadedBlendshapeModelPaths[it];
}

bool RigCalibrationDatabaseDescription::Load(const std::string& inputFile, bool jointsAndBlends)
{
    const std::string jsonString = ReadFile(inputFile);
    const JsonElement json = ReadJson(jsonString);
    const std::string dataDescriptionDirectory = std::filesystem::absolute(std::filesystem::path(inputFile)).parent_path().string();

    auto makeAbsolute = [&](const std::string& filename)
        {
            if (std::filesystem::path(filename).is_relative())
            {
                return dataDescriptionDirectory + "/" + filename;
            }
            else
            {
                return filename;
            }
        };

    if (json.Contains("identity_model_name"))
    {
        m_loadedIdentityModelName = json["identity_model_name"].String();
    }
    else
    {
        LOG_ERROR("Pca model database description does not contain identity_model_name.");
        return false;
    }

    if (json.Contains("version_identifier"))
    {
        m_modelVersionIdentifier = json["version_identifier"].String();
    }
    else
    {
        LOG_ERROR("Pca model database description does not contain version_identifier. Please switch to the newer database.");
        return false;
    }

    if (json.Contains("blendshape_model_suffix"))
    {
        m_blendshapeModelSuffix = json["blendshape_model_suffix"].String();
    }

    if (json.Contains("skinning_model_name"))
    {
        m_skinningModelName = json["skinning_model_name"].String();
        if (json.Contains("skinning_model_path"))
        {
            m_skinningModelPath = makeAbsolute(json["skinning_model_path"].String());
        }
        else
        {
            LOG_INFO("Pca model database description does not contain skinning_model_name.");
        }
    }
    else
    {
        LOG_INFO("Pca model database description does not contain skinning_model_name.");
    }

    if (json.Contains("expression_models"))
    {
        const JsonElement &jExpressions = json["expression_models"];
        auto neutralExpressionIt = jExpressions.Map().find(m_loadedIdentityModelName);
        if (neutralExpressionIt != jExpressions.Map().end())
        {
            m_loadedModelNames.push_back(neutralExpressionIt->first);
            m_loadedModelPaths.push_back(makeAbsolute(neutralExpressionIt->second.String()));
            m_loadedBlendshapeModelPaths.push_back({});
        }
        else
        {
            LOG_ERROR("Pca model database description does not contain model for \"{}\".", m_loadedIdentityModelName);
            return false;
        }
        for (auto &&[expressionName, expressionPath] : jExpressions.Map())
        {
            if (expressionName == m_loadedIdentityModelName) continue;
            const auto path = makeAbsolute(expressionPath.String());
            m_loadedModelNames.push_back(expressionName);
            m_loadedModelPaths.push_back(path);
            if (expressionName == m_loadedIdentityModelName)
            {
                m_loadedBlendshapeModelPaths.push_back({});
            }
            else
            {
                m_loadedBlendshapeModelPaths.push_back(InsertSuffix(path, m_blendshapeModelSuffix));
            }
        }
    }
    else
    {
        LOG_ERROR("Pca model database description does not contain expression_models.");
        return false;
    }

    const auto neutralNameIt = std::find(m_loadedModelNames.begin(), m_loadedModelNames.end(), m_loadedIdentityModelName);
    if (neutralNameIt == m_loadedModelNames.end())
    {
        LOG_ERROR("expression_models do not contain {}", m_loadedIdentityModelName);
        return false;
    }

    if (json.Contains("stabilization"))
    {
        m_stabModelPath = makeAbsolute(json["stabilization"].String());
    }

    if (json.Contains("genecode"))
    {
        m_geneCodeMatrixPath = makeAbsolute(json["genecode"].String());
    }
    else
    {
        LOG_ERROR("Pca model database description does not contain genecode.");
        return false;
    }

    if (json.Contains("archetype"))
    {
        if (json["archetype"].IsObject())
        {
            if (json["archetype"].Contains("joints_only") && json["archetype"].Contains("joints_and_blends"))
            {
                if (jointsAndBlends)
                {
                    if (json["archetype"]["joints_and_blends"].IsObject())
                    {
                        m_archetypeDnaPath[0] =  makeAbsolute(json["archetype"]["joints_and_blends"]["with_rbf"].String());
                        m_archetypeDnaPath[1] =  makeAbsolute(json["archetype"]["joints_and_blends"]["without_rbf"].String());
                    }
                    else
                    {
                        m_archetypeDnaPath[0] =  makeAbsolute(json["archetype"]["joints_and_blends"].String());
                        m_archetypeDnaPath[1] =  makeAbsolute(json["archetype"]["joints_and_blends"].String());
                    }
                }
                else
                {
                    if (json["archetype"]["joints_only"].IsObject())
                    {
                        m_archetypeDnaPath[0] =  makeAbsolute(json["archetype"]["joints_only"]["with_rbf"].String());
                        m_archetypeDnaPath[1] =  makeAbsolute(json["archetype"]["joints_only"]["without_rbf"].String());
                    }
                    else
                    {
                        m_archetypeDnaPath[0] =  makeAbsolute(json["archetype"]["joints_only"].String());
                        m_archetypeDnaPath[1] =  makeAbsolute(json["archetype"]["joints_only"].String());
                    }
                }
            }
        }
        else
        {
            m_archetypeDnaPath[0] = makeAbsolute(json["archetype"].String());
            m_archetypeDnaPath[1] = makeAbsolute(json["archetype"].String());
        }
    }
    else
    {
        LOG_ERROR("Pca model database description does not contain archetype.");
        // return false;
    }

    if (json.Contains("mesh_ids"))
    {
        m_modelMeshIds = json["mesh_ids"].Get<std::vector<int>>();
    }
    if (json.Contains("skinning_mesh_ids"))
    {
        m_skinningMeshIds = json["skinning_mesh_ids"].Get<std::vector<int>>();
    }

    if (json.Contains("rdf"))
    {
        if (json["rdf"].IsObject())
        {
            if (json["rdf"].Contains("joints_only") && json["rdf"].Contains("joints_and_blends"))
            {
                if (jointsAndBlends)
                {
                    if (json["rdf"]["joints_and_blends"].IsObject())
                    {
                        m_rigDefinitionPath[0] =  makeAbsolute(json["rdf"]["joints_and_blends"]["with_rbf"].String());
                        m_rigDefinitionPath[1] =  makeAbsolute(json["rdf"]["joints_and_blends"]["without_rbf"].String());
                    }
                    else
                    {
                        m_rigDefinitionPath[0] =  makeAbsolute(json["rdf"]["joints_and_blends"].String());
                        m_rigDefinitionPath[1] =  makeAbsolute(json["rdf"]["joints_and_blends"].String());
                    }
                }
                else
                {
                    if (json["rdf"]["joints_only"].IsObject())
                    {
                        m_rigDefinitionPath[0] =  makeAbsolute(json["rdf"]["joints_only"]["with_rbf"].String());
                        m_rigDefinitionPath[1] =  makeAbsolute(json["rdf"]["joints_only"]["without_rbf"].String());
                    }
                    else
                    {
                        m_rigDefinitionPath[0] =  makeAbsolute(json["rdf"]["joints_only"].String());
                        m_rigDefinitionPath[1] =  makeAbsolute(json["rdf"]["joints_only"].String());
                    }
                }
            }
        }
        else
        {
            m_rigDefinitionPath[0] = makeAbsolute(json["rdf"].String());
            m_rigDefinitionPath[1] = makeAbsolute(json["rdf"].String());
        }
    }
    else
    {
        LOG_ERROR("Pca model database description does not contain rdf.");
        // return false;
    }

    if (json.Contains("calibration_configuration"))
    {
        m_calibrationConfigurationFile = makeAbsolute(json["calibration_configuration"].String());
    }
    else
    {
        LOG_INFO("Pca model database description does not contain calibration_configuration.");
    }

    if (json.Contains("neutral_fitting_configuration"))
    {
        m_neutralFittingConfigurationFile = makeAbsolute(json["neutral_fitting_configuration"].String());
    }
    else
    {
        LOG_INFO("Pca model database description does not contain neutral_fitting_configuration.");
    }

    if (json.Contains("neck_deformations"))
    {
        const JsonElement& jsonNeckDefConfig = json["neck_deformations"];

        std::filesystem::path ref_dna = makeAbsolute(jsonNeckDefConfig["ref_dna"].String());
        if (std::filesystem::exists(ref_dna)) {
            m_neckDeformationRefDna = ref_dna.string();
        }
        else {
            LOG_INFO("Ref dna path doesn't exist or is not valid: {}", ref_dna.string());
        }

        std::filesystem::path ref_rdf = makeAbsolute(jsonNeckDefConfig["ref_rdf"].String());
        if (std::filesystem::exists(ref_rdf))
        {
            m_neckDeformationRefRdf = ref_rdf.string();
        }
        else {
            LOG_INFO("Ref RDF file path doesn't exist or is not valid: {}", ref_rdf.string());
        }

        if (jsonNeckDefConfig["joint_names"].Array().size() != 0) {
            m_neckDeformationDefaultJointNames.clear();

            for (const auto& item : jsonNeckDefConfig["joint_names"].Array()) {
                m_neckDeformationDefaultJointNames.push_back(item.Get<std::string>());
            }
        }
        else {
            LOG_INFO("No joint names found, using default neck_01, neck_02, head");
        }
    }

    return true;
}

const std::string& RigCalibrationDatabaseDescription::GetRigDefinitionFilePath(bool rbf) const
{
    return rbf ? m_rigDefinitionPath[1] : m_rigDefinitionPath[0];
}

const std::string& RigCalibrationDatabaseDescription::GetArchetypeDnaFilePath(bool rbf) const
{
    return rbf ? m_archetypeDnaPath[1] : m_archetypeDnaPath[0];
}

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
