// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <rigcalibration/RigCalibrationDatabaseDescription.h>
#include <nrr/IdentityBlendModel.h>
#include <carbon/common/Pimpl.h>
#include <carbon/utils/TaskThreadPool.h>


#include <future>

#include <string>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

struct SkinningRegionData
{
    Eigen::VectorXi jointMapping;
    Eigen::VectorXi combinedVertexMapping;
    Eigen::MatrixXf mean;
    Eigen::MatrixXf modes;
    std::string name;
};

class ModelData
{
public:
    ModelData();
    ~ModelData();
    ModelData(ModelData&& other) = delete;
    ModelData(const ModelData& other) = delete;
    ModelData& operator=(ModelData&& other) = delete;
    ModelData& operator=(const ModelData& other) = delete;

    bool Load(const RigCalibrationDatabaseDescription &modelHandler, bool loadBlendshapes = true);

    void Set(const std::map<std::string, std::shared_ptr<IdentityBlendModel<float, -1>>>& models,
             const std::string& neutralName,
             const std::string& skinningName,
             const std::map<std::string, std::pair<Eigen::VectorXf, Eigen::MatrixXf>>& geneCode,
             const std::map<std::string, std::map<std::string, std::pair<int, int>>>& geneCodeExprRange,
             const std::map<std::string, SkinningRegionData>& skinningModel,
             const std::shared_ptr<IdentityBlendModel<float>>& stabilizationModel);

    int NumModels() const;

    const std::shared_ptr<IdentityBlendModel<float, -1>> GetModel(const std::string& modelName) const;

    const std::string& GetNeutralName() const;
    const std::string& GetSkinningName() const;
    const std::vector<std::string>& GetModelNames() const;
    const std::string& GetModelVersionIdentifier() const;

    const std::map<std::string, std::pair<Eigen::VectorXf, Eigen::MatrixXf>>& GetRegionGeneCodeMatrices() const;
    const std::map<std::string, std::map<std::string, std::pair<int, int>>>& GetRegionGeneCodeExprRanges() const;
    const std::map<std::string, SkinningRegionData>& GetSkinningModel() const;
    SkinningRegionData GetRegionSkinningModel(const std::string& region) const;
    const std::vector<std::string>& GetSkinningModelRegions() const;

    const std::shared_ptr<IdentityBlendModel<float>>& GetStabilizationModel() const;

    // per region, gene code modes for given expressions
    std::vector<std::pair<Eigen::VectorXf, Eigen::MatrixXf>> GetExpressionGeneCodeModes(const std::string expressionName) const;

private:
    struct Private;
    TITAN_NAMESPACE::Pimpl<Private> m;
};
CARBON_NAMESPACE_END(TITAN_NAMESPACE)
