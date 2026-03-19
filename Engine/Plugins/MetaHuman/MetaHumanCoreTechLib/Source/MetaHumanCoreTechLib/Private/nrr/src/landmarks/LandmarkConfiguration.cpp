// Copyright Epic Games, Inc. All Rights Reserved.

#include <nrr/landmarks/LandmarkConfiguration.h>

#include <carbon/Algorithm.h>

#include <algorithm>
#include <numeric>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)
std::shared_ptr<LandmarkConfiguration> LandmarkConfiguration::Clone() const
{
    //return std::make_shared<LandmarkConfiguration>(m_numPoints, m_landmarkMapping, m_curvesMapping);
    return std::make_shared<LandmarkConfiguration>(*this);
}

bool LandmarkConfiguration::LoadConfiguration(const JsonElement& j)
{
    if (!j.IsObject() || !j.Contains("frames"))
    {
        CARBON_CRITICAL("frames are missing in the landmarks file");
    }

    const int numFrames = int(j["frames"].Size());
    if (numFrames == 0)
    {
        CARBON_CRITICAL("no frames in landmarks file");
    }

    const bool newFormat = j["frames"][0].Contains("points");
    const JsonElement& jFrame = newFormat ? j["frames"][0]["points"] : j["frames"][0];

    m_landmarkMapping.clear();
    m_curvesMapping.clear();
    m_numPoints = 0;
    for (const auto& [landmarkOrCurveName, jLandmarks] : jFrame.Map())
    {
        const int numPointsForLandmark = static_cast<int>(jLandmarks.Size());
        if (numPointsForLandmark == 1)
        {
            m_landmarkMapping[landmarkOrCurveName] = m_numPoints;
        }
        else
        {
            std::vector<int> indices(numPointsForLandmark);
            for (int i = 0; i < numPointsForLandmark; ++i)
            {
                indices[i] = m_numPoints + i;
            }
            m_curvesMapping[landmarkOrCurveName] = indices;
        }
        m_numPoints += numPointsForLandmark;
    }

    return true;
}

void LandmarkConfiguration::ImportIndices(const std::map<std::string, int>& lmIndices,
    const std::map<std::string, std::vector<int>>& curveIndices)
{
    m_landmarkMapping = lmIndices;
    m_curvesMapping = curveIndices;

    m_numPoints = 0;

    for(auto & [lmName, index]:m_landmarkMapping)
    {
        if(index >= m_numPoints)
        {
            m_numPoints = index+1;
        }
    }

    for(auto & [lmName, indices]:m_curvesMapping)
    {
        auto it = std::max_element(indices.begin(), indices.end());

        if(*it >= m_numPoints)
        {
            m_numPoints = *it+1;
        }
    }
}

void LandmarkConfiguration::AddLandmark(const std::string& landmarkName)
{
    if (HasLandmark(landmarkName))
    {
        LOG_INFO("landmark configuration already contains landmark {}", landmarkName);
        return;
    }
    m_landmarkMapping[landmarkName] = m_numPoints;
    m_numPoints++;
}

void LandmarkConfiguration::SetLandmark(const std::string& landmarkName, int idx)
{
    if (HasLandmark(landmarkName))
    {
        CARBON_CRITICAL("landmark configuration already contains landmark {}", landmarkName);
    }
    if (idx >= m_numPoints)
    {
        CARBON_CRITICAL("idx {} is larger than number of points ({})", idx, m_numPoints);
    }
    m_landmarkMapping[landmarkName] = idx;
}


void LandmarkConfiguration::AddCurve(const std::string& curveName, int numPoints)
{
    if (HasCurve(curveName))
    {
        LOG_INFO("landmark configuration already contains curve {}", curveName);
        return;
    }
    std::vector<int> indices(numPoints);
    std::iota(indices.begin(), indices.end(), m_numPoints);
    m_curvesMapping[curveName] = indices;
    m_numPoints += numPoints;
}

void LandmarkConfiguration::AddCurve(const std::string& curveName, const std::string& startLandmark,
    const std::string& endLandmark, int numInbetweenPoints)
{
    if (HasCurve(curveName))
    {
        LOG_INFO("landmark configuration already contains curve {}", curveName);
        return;
    }

    if(!HasLandmark(startLandmark) || !HasLandmark(endLandmark))
    {
        LOG_INFO("landmark configuration does not contain landmarks {} and {}", startLandmark, endLandmark);
        return; 
    }

    std::vector<int> indices(numInbetweenPoints);
    std::iota(indices.begin(), indices.end(), m_numPoints);

    indices.insert(indices.begin(),m_landmarkMapping[startLandmark]);
    indices.push_back(m_landmarkMapping[endLandmark]);

    m_curvesMapping[curveName] = indices;
    m_numPoints += numInbetweenPoints;
}

void LandmarkConfiguration::RemoveLandmark(const std::string& landmarkName)
{
    if (!HasLandmark(landmarkName))
    {
        LOG_INFO("landmark configuration does not contain landmark {}", landmarkName);
        return;
    }
    m_landmarkMapping.erase(m_landmarkMapping.find(landmarkName));
}

void LandmarkConfiguration::RemoveCurve(const std::string& curveName)
{
    if (!HasCurve(curveName))
    {
        LOG_INFO("landmark configuration does not contain curve {}", curveName);
        return;
    }
    m_curvesMapping.erase(m_curvesMapping.find(curveName));
}

void LandmarkConfiguration::RemoveUnwantedCurvesAndLandmarks(    const std::vector<std::string>& validLandmarks,const std::vector<std::string>& validCurves)
{
    std::vector<std::string> curvesToRemove;
    std::vector<std::string> landmarksToRemove;

    for(auto & keyAndValue :m_curvesMapping)
    {
        if( std::find(validCurves.begin(), validCurves.end(), keyAndValue.first) == validCurves.end())
        {
            curvesToRemove.push_back(keyAndValue.first);
        }
    }

    for(auto & keyAndValue :m_landmarkMapping)
    {
        if( std::find(validLandmarks.begin(), validLandmarks.end(), keyAndValue.first) == validLandmarks.end() &&
            !IsLandmarkInAnyCurve(keyAndValue.first, validCurves))
        {
            landmarksToRemove.push_back(keyAndValue.first);
        }
    }

    for(auto & crv: curvesToRemove)
    {
        RemoveCurve(crv);
    }

    for (auto & lm : landmarksToRemove)
    {
        RemoveLandmark(lm);
    }
}

int LandmarkConfiguration::IndexForLandmark(const std::string& landmarkName) const
{
    auto it = m_landmarkMapping.find(landmarkName);
    if (it != m_landmarkMapping.end())
    {
        return it->second;
    }
    else
    {
        CARBON_CRITICAL("no landmark of name {} in configuration", landmarkName);
    }
}

const std::vector<int>& LandmarkConfiguration::IndicesForCurve(const std::string& curveName) const
{
    auto it = m_curvesMapping.find(curveName);
    if (it != m_curvesMapping.end())
    {
        return it->second;
    }
    else
    {
        CARBON_CRITICAL("no curve of name {} in configuration", curveName);
    }
}

template <class T, int D>
std::map<int, int> LandmarkConfiguration::FindDuplicatesAndCreateMap(const Eigen::Matrix<T, D, -1>& pts) const
{
    if (int(pts.cols()) != m_numPoints)
    {
        CARBON_CRITICAL("numter of points does not match the number of points of the landmark configuration");
    }
    // find matching incides for all curves, then merge curves in the configuration by mapping original curve indices to get rid of duplicates
    std::map<std::pair<T, T>, int> pt2index;
    for (const auto& [_, index] : LandmarkMapping())
    {
        pt2index[std::pair<T, T>(pts(0, index), pts(1, index))] = index; // this will remove identical pts
    }
    for (const auto& [_, indices] : CurvesMapping())
    {
        for (const int index : indices)
        {
            pt2index[std::pair<T, T>(pts(0, index), pts(1, index))] = index; // this will remove identical pts
        }
    }
    std::map<int, int> index2index;
    for (const auto& [_, index] : LandmarkMapping())
    {
        index2index[index] = pt2index[std::pair<T, T>(pts(0, index), pts(1, index))];
    }
    for (const auto& [_, indices] : CurvesMapping())
    {
        for (const int index : indices)
        {
            index2index[index] = pt2index[std::pair<T, T>(pts(0, index), pts(1, index))];
        }
    }
    return index2index;
}

/**
 * Concatenates two vectors with undown direction along one matching end point (removing the duplicate).
 * Fails if there is no matching end point or if either vector is empty.
 */
template <class T, int D>
bool ConcatenateVectorsWithMatchingEndPointsAndUnknownDirection(const std::vector<int>& vector1,
                                                                const std::vector<int>& vector2,
                                                                const Eigen::Matrix<T, D, -1>& pts,
                                                                std::vector<int>& mergedVector)
{
    if (vector1.empty() || vector2.empty())
    {
        // no matching of vectors if any is empty
        return false;
    }

    std::vector<int> newMergedVector;
    newMergedVector.reserve(vector1.size() + vector2.size() - 1);
    bool success = false;

    static constexpr T eps = T(1e-9);
    if ((pts.col(vector2.front()) - pts.col(vector1.front())).squaredNorm() < eps)
    {
        newMergedVector.insert(newMergedVector.begin(), vector1.rbegin(), vector1.rend());
        newMergedVector.insert(newMergedVector.end(), vector2.begin() + 1, vector2.end());
        success = true;
    }
    else if ((pts.col(vector2.front()) - pts.col(vector1.back())).squaredNorm() < eps)
    {
        newMergedVector.insert(newMergedVector.begin(), vector1.begin(), vector1.end());
        newMergedVector.insert(newMergedVector.end(), vector2.begin() + 1, vector2.end());
        success = true;
    }
    else if ((pts.col(vector2.back()) - pts.col(vector1.front())).squaredNorm() < eps)
    {
        newMergedVector.insert(newMergedVector.begin(), vector2.begin(), vector2.end());
        newMergedVector.insert(newMergedVector.end(), vector1.begin() + 1, vector1.end());
        success = true;
    }
    else if ((pts.col(vector2.back()) - pts.col(vector1.back())).squaredNorm() < eps)
    {
        newMergedVector.insert(newMergedVector.begin(), vector2.begin(), vector2.end());
        newMergedVector.insert(newMergedVector.end(), vector1.rbegin() + 1, vector1.rend());
        success = true;
    }

    if (success)
    {
        mergedVector.swap(newMergedVector);
    }
    return success;
}

template <class T, int D>
bool LandmarkConfiguration::MergeCurves(const std::vector<std::string>& curveNames,
                                        const std::string& newCurveName,
                                        const Eigen::Matrix<T, D, -1>& pts,
                                        bool ignoreMissingCurves,
                                        bool removeMergedCurves)
{
    std::vector<std::string> curvesToMerge;
    for (const std::string& curveName : curveNames)
    {
        if (HasCurve(curveName))
        {
            curvesToMerge.push_back(curveName);
        }
        else
        {
            if (!ignoreMissingCurves)
            {
                CARBON_CRITICAL("cannot merge curves as curve {} does not exist", curveName);
            }
        }
    }

    if (curvesToMerge.size() < 1)
    {
        // if there is nothing to merge, then return false
        return false;
    }

    if (HasCurve(newCurveName))
    {
        CARBON_CRITICAL("there is a prior curve with name {}", newCurveName);
    }

    if (curvesToMerge.size() == 1)
    {
        // a single curve, just add it with the new name
        m_curvesMapping[newCurveName] = m_curvesMapping[curvesToMerge.front()];
        if (removeMergedCurves)
        {
            RemoveCurve(curvesToMerge.front());
        }
        return true;
    }

    // take the first curve
    std::vector<int> newCurve = m_curvesMapping[curvesToMerge.front()];

    // merge all other curves
    std::set<std::string> toProcess;
    toProcess.insert(curvesToMerge.begin() + 1, curvesToMerge.end());

    while (toProcess.size() > 0)
    {
        bool mergeOk = false;
        for (const std::string& candidate : toProcess)
        {
            // it should be possible merge at least one curve, otherwise the curves are not connected
            if (ConcatenateVectorsWithMatchingEndPointsAndUnknownDirection<T>(newCurve, m_curvesMapping[candidate], pts, newCurve))
            {
                toProcess.erase(toProcess.find(candidate));
                mergeOk = true;
                break;
            }
        }
        if (!mergeOk)
        {
            CARBON_CRITICAL("failure to merge curves - no matching indices");
        }
    }

    m_curvesMapping[newCurveName] = newCurve;

    if (removeMergedCurves)
    {
        for (const std::string& mergeCurveName : curvesToMerge)
        {
            RemoveCurve(mergeCurveName);
        }
    }

    return true;
}

std::vector<std::string> LandmarkConfiguration::CurveNames() const
{
    std::vector<std::string> result;

    for (const auto& [curveName, _] : m_curvesMapping)
    {
        result.push_back(curveName);
    }

    return result;
}

std::vector<std::string> LandmarkConfiguration::LandmarkNames() const
{
    std::vector<std::string> result;

    for (const auto& [lmName, _] : m_landmarkMapping)
    {
        result.push_back(lmName);
    }

    return result;
}

void LandmarkConfiguration::MergeConfiguration(const LandmarkConfiguration& otherConfiguration)
{
    // check that landmark and curve names are unique
    for (const auto& [landmarkName, _] : otherConfiguration.m_landmarkMapping)
    {
        if (HasLandmark(landmarkName) || HasCurve(landmarkName))
        {
            CARBON_CRITICAL("landmark or curve with name {} already exists", landmarkName);
        }
    }

    for (const auto& [curveName, _] : otherConfiguration.m_curvesMapping)
    {
        if (HasLandmark(curveName) || HasCurve(curveName))
        {
            CARBON_CRITICAL("landmark or curve with name {} already exists", curveName);
        }
    }

    // add mapping
    int numPointsOtherConfiguration = otherConfiguration.NumPoints();
    for (const auto& [landmarkName, landmarkIndex] : otherConfiguration.m_landmarkMapping)
    {
        m_landmarkMapping[landmarkName] = landmarkIndex + m_numPoints;
    }

    for (const auto& [curveName, curveIndices] : otherConfiguration.m_curvesMapping)
    {
        std::vector<int> newCurveIndices = curveIndices;
        std::transform(curveIndices.begin(), curveIndices.end(), newCurveIndices.begin(), [&](int id) {
                return id + m_numPoints;
            });
        m_curvesMapping[curveName] = newCurveIndices;
    }

    m_numPoints += numPointsOtherConfiguration;
}

std::vector<int> LandmarkConfiguration::RemapLandmarksAndCurvesAndCompress(const std::map<int, int>& map)
{
    std::vector<bool> used(NumPoints(), false);
    for (const auto& [_, index] : map)
    {
        used[index] = true;
    }
    std::vector<int> newIndex(NumPoints(), -1);
    int newSize = 0;
    for (int i = 0; i < NumPoints(); ++i)
    {
        if (used[i])
        {
            newIndex[i] = newSize++;
        }
    }

    std::map<std::string, int> newLandmarkMapping;
    std::map<std::string, std::vector<int>> newCurvesMapping;

    for (auto&& [landmarkName, index] : m_landmarkMapping)
    {
        auto it = map.find(index);
        if (it != map.end())
        {
            newLandmarkMapping[landmarkName] = newIndex[it->second];
        }
    }

    for (auto&& [curveName, indices] : m_curvesMapping)
    {
        std::vector<int> newCurveIndices;
        for (int& index : indices)
        {
            auto it = map.find(index);
            if (it != map.end())
            {
                newCurveIndices.push_back(newIndex[map.find(index)->second]);
            }
        }
        if (newCurveIndices.size() > 1)
        {
            newCurvesMapping[curveName] = newCurveIndices;
        }
    }

    std::vector<int> newLandmarksToOldLandmarksMap(newSize);
    for (const auto& [_, index] : map)
    {
        newLandmarksToOldLandmarksMap[newIndex[index]] = index;
    }

    m_numPoints = newSize;
    m_landmarkMapping = std::move(newLandmarkMapping);
    m_curvesMapping = std::move(newCurvesMapping);

    return newLandmarksToOldLandmarksMap;
}

bool LandmarkConfiguration::IsLandmarkInAnyCurve(const std::string & landmarkName, const std::vector<std::string>& validCurveNames)
{
    int lmIndex = m_landmarkMapping[landmarkName];

    for(auto & curveName:validCurveNames)
    {
        if(m_curvesMapping.count(curveName) == 0)
        {
            continue;
        }

        auto curveIndices = IndicesForCurve(curveName);

        if( std::find(curveIndices.begin(), curveIndices.end(), lmIndex) != curveIndices.end())
        {
            return true;
        }
    }

    return false;
}

std::string LandmarkConfiguration::LandmarkNameForPointIndex(int index) const
{
    for(auto & keyAndValue : m_landmarkMapping)
    {
        if(keyAndValue.second == index)
        {
            return keyAndValue.first;
        }
    }

    return "";
}

bool LandmarkConfiguration::DoesConfigMatch(const LandmarkConfiguration & other)const
{
    if(m_numPoints != other.m_numPoints)
    {
        return false;
    }

    for(const auto & [lmName, index]: m_landmarkMapping)
    {
        if(other.m_landmarkMapping.count(lmName) == 0) return false;

        if(other.m_landmarkMapping.at(lmName) != index) return false;
    }

    for(const auto & [curveName, indices] : m_curvesMapping)
    {
        if(other.m_curvesMapping.count(curveName) == 0) return false;

        if(other.m_curvesMapping.at(curveName) != indices) return false;

    }

    return true;
}

template std::map<int, int> LandmarkConfiguration::FindDuplicatesAndCreateMap<float, 2>(const Eigen::Matrix<float, 2, -1>&) const;
template std::map<int, int> LandmarkConfiguration::FindDuplicatesAndCreateMap<double, 2>(const Eigen::Matrix<double, 2, -1>&) const;
template std::map<int, int> LandmarkConfiguration::FindDuplicatesAndCreateMap<float, 3>(const Eigen::Matrix<float, 3, -1>&) const;
template std::map<int, int> LandmarkConfiguration::FindDuplicatesAndCreateMap<double, 3>(const Eigen::Matrix<double, 3, -1>&) const;
template bool LandmarkConfiguration::MergeCurves<float, 2>(const std::vector<std::string>&, const std::string&, const Eigen::Matrix<float, 2, -1>&, bool, bool);
template bool LandmarkConfiguration::MergeCurves<double, 2>(const std::vector<std::string>&, const std::string&, const Eigen::Matrix<double, 2, -1>&, bool,bool);
template bool LandmarkConfiguration::MergeCurves<float, 3>(const std::vector<std::string>&, const std::string&, const Eigen::Matrix<float, 3, -1>&, bool, bool);
template bool LandmarkConfiguration::MergeCurves<double, 3>(const std::vector<std::string>&, const std::string&, const Eigen::Matrix<double, 3, -1>&, bool,bool);
CARBON_NAMESPACE_END(TITAN_NAMESPACE)
