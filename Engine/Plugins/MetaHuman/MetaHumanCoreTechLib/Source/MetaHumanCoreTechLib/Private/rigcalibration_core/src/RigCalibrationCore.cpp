// Copyright Epic Games, Inc. All Rights Reserved.

#include <rigcalibration/RigCalibrationCore.h>
#include <rigcalibration/RigCalibrationParams.h>
#include <nrr/VertexWeights.h>
#include <carbon/common/Log.h>
#include <carbon/utils/TaskThreadPoolUtils.h>
#include <nls/utils/ConfigurationParameter.h>

#include <numeric>
#include <filesystem>
#include <iostream>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)


void RigCalibrationParams::SetFromConfiguration(const Configuration &config)
{
    if (config.HasParameter("regularization"))
    {
        regularization = config["regularization"].template Value<float>();
    }
    else
    {
        LOG_WARNING("No regularization parameter in config {}", config.Name());
    }
}

Configuration RigCalibrationParams::ToConfiguration() const
{
    Configuration config = { std::string("Rig Calibration Configuration"), { { "regularization", ConfigurationParameter(regularization) } } };

    return config;
}

std::map<std::string, Eigen::VectorXf> RigCalibrationCore::CalibrateExpressionsAndSkinning(
    const std::shared_ptr<ModelData> &data,
    const std::map<std::string, Eigen::VectorXf> &currentParams,
    RigCalibrationParams rigCalibParams,
    bool linearize)
{
    if (currentParams.empty())
    {
        CARBON_CRITICAL("No marked expressions to use.");
    }

    std::map<std::string, Eigen::VectorXf> outputParams;

    // all expressions including neutral
    std::vector<std::string> allExpressionNames = data->GetModelNames();

    const auto &neutralModel = data->GetModel(data->GetNeutralName());
    const auto &skinningModel = data->GetSkinningModel();

    bool dataContainsSkinningModel = !skinningModel.empty() ? true : false;
    int expectedNumOutput = (int)allExpressionNames.size();
    std::map<std::string, std::pair<int, int>> skinningParamsRange;
    if (dataContainsSkinningModel)
    {
        int currentStart = 0;
        for (const auto &[regionName, model] : skinningModel)
        {
            int currentEnd = currentStart + (int)model.modes.cols();
            skinningParamsRange[regionName] = std::make_pair(currentStart, currentEnd);
            currentStart = currentEnd;
        }

        outputParams[data->GetSkinningName()] = Eigen::VectorXf::Zero(currentStart);
        expectedNumOutput++;
    }

    // initialize: for fitted expression take fitted params, for rest take default
    for (auto i = 0; i < (int)allExpressionNames.size(); i++)
    {
        const std::string &expressionName = allExpressionNames[i];
        auto currParamsIt = currentParams.find(expressionName);
        if (currParamsIt == currentParams.end())
        {
            outputParams[expressionName] = data->GetModel(expressionName)->DefaultParameters();
        }
        else
        {
            outputParams[expressionName] = currParamsIt->second;
        }
    }

    const auto &geneCodeMatrices = data->GetRegionGeneCodeMatrices();
    const auto &geneCodeRanges = data->GetRegionGeneCodeExprRanges();

    auto calibrateRegion = [&](int startRegionIt, int endRegionIt)
    {
        for (int r = startRegionIt; r < endRegionIt; ++r)
        {
            std::vector<Eigen::VectorXf> extractedRows;
            std::vector<Eigen::VectorXf> extractedParams;
            std::vector<float> extractedMean;

            const auto &regionName = neutralModel->RegionName(r);
            const auto regionGeneCodeMatrixIt = geneCodeMatrices.find(regionName);
            if (regionGeneCodeMatrixIt == geneCodeMatrices.end())
            {
                CARBON_CRITICAL("No region {} in the character code matrix.", regionName);
            }

            const auto expressionRegionRangeIt = geneCodeRanges.find(regionName);
            if (expressionRegionRangeIt == geneCodeRanges.end())
            {
                CARBON_CRITICAL("No region {} in the character code matrix.", regionName);
            }

            const auto &[regionMean, regionModes] = regionGeneCodeMatrixIt->second;
            const auto &expressionRangesForRegion = expressionRegionRangeIt->second;

            // for fitted expression, extract their parameters for the current region and the corresponding modes and mean from gene code mat
            for (auto i = 0; i < (int)allExpressionNames.size(); i++)
            {
                auto modelName = allExpressionNames[i];

                auto exprIt = currentParams.find(modelName);
                if (exprIt != currentParams.end())
                {
                    const auto params = exprIt->second;

                    const auto &[regionStart, regionEnd] = data->GetModel(modelName)->RegionRanges()[r];
                    const auto regionParams = params.segment(regionStart, regionEnd - regionStart);

                    const auto expressionRangeForRegionIt = expressionRangesForRegion.find(modelName);
                    if (expressionRangeForRegionIt == expressionRangesForRegion.end())
                    {
                        CARBON_CRITICAL("No model {} for region.", modelName);
                    }

                    const auto [start, end] = expressionRangeForRegionIt->second;
                    extractedParams.push_back(regionParams);
                    for (int j = start; j < end; j++)
                    {
                        extractedRows.push_back(regionModes.col(j));
                        extractedMean.push_back(regionMean[j]);
                    }
                }
            }

            const int numRows = (int)extractedRows.size();
            const int numCols = (int)extractedRows[0].size();
            Eigen::MatrixXf A = Eigen::MatrixXf::Zero(numRows, numCols);
            Eigen::VectorXf b = Eigen::VectorXf::Zero(numRows);
            Eigen::VectorXf regMean = Eigen::VectorXf::Zero(numRows);
            for (int i = 0; i < numRows; ++i)
            {
                A.row(i) = extractedRows[i];
                regMean[i] = extractedMean[i];
            }

            int currentStart = 0;
            for (int i = 0; i < (int)extractedParams.size(); ++i)
            {
                int currentParamsSize = (int)extractedParams[i].size();

                b.segment(currentStart, currentParamsSize) = extractedParams[i];
                currentStart += currentParamsSize;
            }

            b -= regMean;

            Eigen::Matrix<float, -1, -1> E = Eigen::Matrix<float, -1, -1>::Identity(A.cols(), A.cols());
            const Eigen::VectorXf x = (A.transpose() * A + rigCalibParams.regularization * E).inverse() * (A.transpose() * b);

            const Eigen::VectorXf reconstruction = regionModes.transpose() * x;
            const Eigen::VectorXf allWeights = regionMean + reconstruction;

            // skinning
            auto skinningIt = std::find(data->GetSkinningModelRegions().begin(), data->GetSkinningModelRegions().end(), regionName);
            if (skinningIt != data->GetSkinningModelRegions().end())
            {
                auto outputParamsIt = outputParams.find(data->GetSkinningName());
                const auto &[start, end] = skinningParamsRange.at(regionName);

                const auto expressionRangeForRegionIt = expressionRangesForRegion.find(data->GetSkinningName());
                if (expressionRangeForRegionIt == expressionRangesForRegion.end())
                {
                    CARBON_CRITICAL("No expression {} for region.", data->GetSkinningName());
                }

                const auto [skinningStart, skinningEnd] = expressionRangeForRegionIt->second;
                Eigen::VectorXf regionParams = allWeights.segment(skinningStart, skinningEnd - skinningStart);

                outputParamsIt->second.segment(start, end - start) = regionParams;
            }

            // expressions including neutral
            for (int i = 0; i < (int)allExpressionNames.size(); ++i)
            {
                const std::string &expressionName = allExpressionNames[i];

                auto exprIt = currentParams.find(expressionName);
                bool expressionNotFitted = exprIt == currentParams.end() ? true : false;

                if (expressionNotFitted || linearize)
                {
                    const auto &[regionStart, regionEnd] = data->GetModel(expressionName)->RegionRanges()[r];
                    const auto expressionRangeForRegionIt = expressionRangesForRegion.find(expressionName);
                    if (expressionRangeForRegionIt == expressionRangesForRegion.end())
                    {
                        CARBON_CRITICAL("No expression {} for region.", expressionName);
                    }

                    const auto [start, end] = expressionRangeForRegionIt->second;
                    Eigen::VectorXf regionParams = allWeights.segment(start, end - start);

                    auto outputParamsIt = outputParams.find(expressionName);
                    if (outputParamsIt == outputParams.end())
                    {
                        CARBON_CRITICAL("Parameters for expression {} not initialized.", expressionName);
                    }

                    outputParamsIt->second.segment(regionStart, regionEnd - regionStart) = regionParams;
                }
            }
        }
    };

    TITAN_NAMESPACE::TaskThreadPoolUtils::RunTaskRangeAndWait(neutralModel->NumRegions(), calibrateRegion);

    if ((int)outputParams.size() != expectedNumOutput)
    {
        CARBON_CRITICAL("Output parameter map does not contain all expressions.");
    }

    return outputParams;
}

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
