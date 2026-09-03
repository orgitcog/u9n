// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>
#include <vector>
#include <string>
#include <array>
#include <map>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

class RigCalibrationDatabaseDescription
{
public:
    RigCalibrationDatabaseDescription() = default;
    ~RigCalibrationDatabaseDescription() = default;

    bool Load(const std::string &file, bool jointsAndBlends = true);

    const std::vector<std::string>& GetExpressionModelPaths() const { return m_loadedModelPaths; }

    const std::string& GetIdentityModelName() const { return m_loadedIdentityModelName; }

    const std::vector<std::string>& GetExpressionModelNames() const { return m_loadedModelNames; }

    const std::string& GetExpressionModelPath(const int it) const;

    const std::string& GetExpressionBlendshapeModelPath(const int it) const;

    const std::string& GetGeneCodeMatrixFilePath() const { return m_geneCodeMatrixPath; }

    const std::string& GetModelVersionIdentifer() const { return m_modelVersionIdentifier; }

    const std::string& GetRigDefinitionFilePath(bool rbf) const;

    const std::string& GetArchetypeDnaFilePath(bool rbf) const;

    const std::string& GetCalibrationConfigurationFile() const { return m_calibrationConfigurationFile; }

    const std::string& GetNeutralFittingConfigurationFile() const { return m_neutralFittingConfigurationFile; }

    const std::string& GetBlendshapeModelSuffix() const { return m_blendshapeModelSuffix; }

    const std::vector<std::string>& GetNeckDeformationDefaultJointNames() const { return m_neckDeformationDefaultJointNames; }

    const std::string& GetReferenceRdfConfigurationFile() const { return m_neckDeformationRefRdf; }

    const std::string& GetReferenceNeckDeformationDnaFile() const { return m_neckDeformationRefDna; }

    const std::vector<int>& GetModelMeshIds() const { return m_modelMeshIds; }

    const std::vector<int>& GetSkinningMeshIds() const { return m_skinningMeshIds; }

    const std::string& GetSkinningModelFilePath() const { return m_skinningModelPath; }

    const std::string& GetSkinningModelName() const { return m_skinningModelName; }

    const std::string& GetStabilizationModelFilePath() const { return m_stabModelPath; }

private:
    std::vector<std::string> m_loadedModelPaths;
    std::vector<std::string> m_loadedBlendshapeModelPaths;
    std::string m_stabModelPath;
    std::string m_geneCodeMatrixPath;
    std::string m_skinningModelName;
    std::string m_skinningModelPath;
    std::vector<std::string> m_loadedModelNames;
    std::string m_loadedIdentityModelName;
    std::array<std::string, 2> m_archetypeDnaPath;
    std::array<std::string, 2> m_rigDefinitionPath;
    std::string m_calibrationConfigurationFile;
    std::string m_neutralFittingConfigurationFile;
    std::string m_blendshapeModelSuffix = "_bs";
    std::vector<std::string> m_neckDeformationDefaultJointNames = { "neck_01", "neck_02", "head" };
    std::string m_neckDeformationRefRdf;
    std::string m_neckDeformationRefDna;
    std::string m_modelVersionIdentifier;
    std::vector<int> m_modelMeshIds;
    std::vector<int> m_skinningMeshIds;
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
