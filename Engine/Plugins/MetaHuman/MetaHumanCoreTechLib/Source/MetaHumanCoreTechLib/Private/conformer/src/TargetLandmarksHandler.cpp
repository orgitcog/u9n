// Copyright Epic Games, Inc. All Rights Reserved.

#include <conformer/TargetLandmarksHandler.h>
#include <nls/geometry/Polyline.h>
#include <carbon/Common.h>
#include <string>
#include <map>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
const std::vector<std::map<std::string, T>> MaskLandmarksToAvoidAmbiguity(const std::map<std::string, T>& currentGlobalLandmarkWeights,
                                                                          const std::vector<std::vector<std::pair<LandmarkInstance<T, 2>,
                                                                                                                  Camera<T>>>>& targetLandmarks)
{
    CARBON_ASSERT(targetLandmarks.size() > 0, "target landmarks container is empty.");

    const int numOfObs = static_cast<int>(targetLandmarks.size());
    std::vector<std::map<std::string, T>> outputWeigths(numOfObs);
    // first observation remains the same
    outputWeigths[0] = currentGlobalLandmarkWeights;

    // others use landmarks which are not defined in the first one
    for (size_t i = 1; i < targetLandmarks.size(); i++)
    {
        for (size_t j = 0; j < targetLandmarks[i].size(); j++)
        {
            LandmarkInstance<T, 2> currentLandmarkInstance = targetLandmarks[i][j].first;
            const auto configurationCurrent = currentLandmarkInstance.GetLandmarkConfiguration();

            LandmarkInstance<T, 2> landmarkInstanceObs0 = targetLandmarks[0][j].first;
            const auto configurationObs0 = landmarkInstanceObs0.GetLandmarkConfiguration();

            for (const auto& [name, _] : configurationCurrent->CurvesMapping())
            {
                if (outputWeigths[i].find(name) == outputWeigths[i].end())
                {
                    if (!configurationObs0->HasCurve(name))
                    {
                        outputWeigths[i][name] = 1.0f;
                    }
                    else
                    {
                        outputWeigths[i][name] = 0.0f;
                    }
                }
            }

            for (const auto& [name, _] : configurationCurrent->LandmarkMapping())
            {
                if (outputWeigths[i].find(name) == outputWeigths[i].end())
                {
                    if (!configurationObs0->HasLandmark(name))
                    {
                        outputWeigths[i][name] = 1.0f;
                    }
                    else
                    {
                        outputWeigths[i][name] = 0.0f;
                    }
                }
            }
        }
    }

    return outputWeigths;
}

template const std::vector<std::map<std::string, float>> MaskLandmarksToAvoidAmbiguity(const std::map<std::string, float>& currentGlobalLandmarkWeights,
                                                                                       const std::vector<std::vector<std::pair<LandmarkInstance<float, 2>,
                                                                                                                               Camera<float>>>>& targetLandmarks);

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
