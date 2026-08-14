// Copyright Epic Games, Inc. All Rights Reserved.

#include <nrr/landmarks/LandmarkSequence.h>

#include <carbon/io/JsonIO.h>
#include <carbon/io/Utils.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
LandmarkSequence<T>::LandmarkSequence()
{}

template <class T>
bool LandmarkSequence<T>::Load(const std::string& filename, int frameOffset)
{
    const std::string filedata = ReadFile(filename);
    JsonElement json = ReadJson(filedata);

    if (!json.IsObject() || !json.Contains("frames"))
    {
        CARBON_CRITICAL("frames are missing in the landmarks file");
    }

    const int numFrames = int(json["frames"].Size());

    m_landmarkInstances.clear();
    for (int i = 0; i < numFrames; i++)
    {
        std::shared_ptr<LandmarkInstance<T, 2>> landmarkInstance = std::make_shared<LandmarkInstance<T, 2>>();
        if (!landmarkInstance->LoadFromJson(json["frames"][i]))
        {
            LOG_ERROR("failed to load landmarks from json file");
            return false;
        }
        if (frameOffset >= 0)
        {
            m_landmarkInstances[frameOffset + i] = landmarkInstance;
            if (landmarkInstance->FrameNumber() != frameOffset + i)
            {
                LOG_WARNING("frame number does not match: {} vs {}", frameOffset + i, landmarkInstance->FrameNumber());
            }
        }
        else
        {
            m_landmarkInstances[landmarkInstance->FrameNumber()] = landmarkInstance;
        }
    }

    return true;
}

template <class T>
void LandmarkSequence<T>::Save(const std::string& filename, int tabs) const
{
    JsonElement frames(JsonElement::JsonType::Array);
    for (const auto& [_, landmarkInstance] : m_landmarkInstances)
    {
        frames.Append(landmarkInstance->SaveToJson());
    }
    JsonElement meta(JsonElement::JsonType::Object);
    meta.Insert("type", JsonElement("points"));
    meta.Insert("version", JsonElement(1));

    JsonElement json(JsonElement::JsonType::Object);
    json.Insert("metadata", std::move(meta));
    json.Insert("frames", std::move(frames));

    WriteFile(filename, WriteJson(json, tabs));
}

template <class T>
bool LandmarkSequence<T>::HasLandmarks(int frame) const
{
    return (m_landmarkInstances.find(frame) != m_landmarkInstances.end());
}

template <class T>
const LandmarkInstance<T, 2>& LandmarkSequence<T>::Landmarks(int frame) const
{
    auto it = m_landmarkInstances.find(frame);
    if (it != m_landmarkInstances.end())
    {
        return *(it->second);
    }
    else
    {
        CARBON_CRITICAL("no landmarks for frame {}", frame);
    }
}

template <class T>
std::shared_ptr<const LandmarkInstance<T, 2>> LandmarkSequence<T>::LandmarksPtr(int frame) const
{
    auto it = m_landmarkInstances.find(frame);
    if (it != m_landmarkInstances.end())
    {
        return it->second;
    }
    else
    {
        return nullptr;
    }
}

template <class T>
void LandmarkSequence<T>::Undistort(const MetaShapeCamera<T>& camera)
{
    std::map<int, std::shared_ptr<const LandmarkInstance<T, 2>>> newLandmarkInstances;
    for (auto&& [frameNumber, landmarkInstance] : m_landmarkInstances)
    {
        auto newLandmarkInstance = std::make_shared<LandmarkInstance<T, 2>>(*landmarkInstance);
        for (int i = 0; i < newLandmarkInstance->NumLandmarks(); ++i)
        {
            const T confidence = newLandmarkInstance->Confidence()[i];
            const Eigen::Vector2<T> pix = camera.Undistort(newLandmarkInstance->Points().col(i));
            newLandmarkInstance->SetLandmark(i, pix, confidence);
        }
        newLandmarkInstances[frameNumber] = newLandmarkInstance;
    }
    std::swap(newLandmarkInstances, m_landmarkInstances);
}

template <class T>
bool LandmarkSequence<T>::MergeCurves(const std::vector<std::string>& curveNames,
                                      const std::string& newCurveName,
                                      bool ignoreMissingCurves,
                                      bool removeMergedCurves)
{
    bool ok = true;
    std::map<int, std::shared_ptr<const LandmarkInstance<T, 2>>> newLandmarkInstances;
    for (const auto& [frameNumber, landmarkInstance] : m_landmarkInstances)
    {
        auto newLandmarkInstance = std::make_shared<LandmarkInstance<T, 2>>(*landmarkInstance);
        ok &= newLandmarkInstance->MergeCurves(curveNames, newCurveName, ignoreMissingCurves, removeMergedCurves);
        newLandmarkInstances[frameNumber] = newLandmarkInstance;
    }
    std::swap(newLandmarkInstances, m_landmarkInstances);
    return ok;
}

template <class T>
void LandmarkSequence<T>::MergeSequences(const LandmarkSequence& otherSequence)
{
    if (otherSequence.m_landmarkInstances.size() != m_landmarkInstances.size())
    {
        CARBON_CRITICAL("landmark sequences do not have the same size");
    }

    for (const auto& [frame, _] : otherSequence.m_landmarkInstances)
    {
        if (m_landmarkInstances.find(frame) == m_landmarkInstances.end())
        {
            CARBON_CRITICAL("landmarks sequences do not have the same frame numbers");
        }
    }

    // merge instances
    std::map<int, std::shared_ptr<const LandmarkInstance<T, 2>>> newLandmarkInstances;
    for (const auto& [frameNumber, landmarkInstance] : otherSequence.m_landmarkInstances)
    {
        auto it = m_landmarkInstances.find(frameNumber);
        auto newLandmarkInstance = std::make_shared<LandmarkInstance<T, 2>>(*it->second);
        newLandmarkInstance->MergeInstance(*landmarkInstance);
        newLandmarkInstances[frameNumber] = newLandmarkInstance;
    }
    std::swap(newLandmarkInstances, m_landmarkInstances);
}

template <class T>
std::map<int, std::shared_ptr<const LandmarkInstance<T, 2>>> LandmarkSequence<T>::LandmarkInstances() const
{
    std::map<int, std::shared_ptr<const LandmarkInstance<T, 2>>> landmarkInstances;
    for (const auto& [key, value] : m_landmarkInstances)
    {
        landmarkInstances.emplace(key, value);
    }
    return landmarkInstances;
}

template <class T>
void LandmarkSequence<T>::SetLandmarkInstances(const std::map<int, std::shared_ptr<const LandmarkInstance<T, 2>>>& landmarkInstances)
{
    m_landmarkInstances = landmarkInstances;
}

template <class T>
void LandmarkSequence<T>::SetLandmarkInstance(int frame, const std::shared_ptr<const LandmarkInstance<T, 2>>& landmarkInstance)
{
    m_landmarkInstances[frame] = landmarkInstance;
}

template <class T>
std::unique_ptr<LandmarkSequence<T>> LandmarkSequence<T>::Clone() const
{
    std::unique_ptr<LandmarkSequence<T>> result = std::make_unique<LandmarkSequence<T>>();

    for(auto & [frameNumber, lmInstance] : m_landmarkInstances)
    {
        result->m_landmarkInstances[frameNumber] = lmInstance->Clone();
    }

    return result;
}

template <class T>
void LandmarkSequence<T>::OffsetFrameNumbers(int offset)
{
    std::map<int, std::shared_ptr<const LandmarkInstance<T, 2>>> newMap;

    for(auto & [frameNumber, ptr] : m_landmarkInstances)
    {
        auto newInstance = ptr->Clone();
        newInstance->SetFrameNumber(frameNumber + offset);

        newMap[frameNumber + offset] = newInstance;
    }

    m_landmarkInstances = newMap;
}

// explicitly instantiate the landmark sequence classes
template class LandmarkSequence<float>;
template class LandmarkSequence<double>;

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
