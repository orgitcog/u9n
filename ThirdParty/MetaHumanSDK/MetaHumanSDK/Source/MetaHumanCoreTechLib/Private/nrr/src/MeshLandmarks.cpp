// Copyright Epic Games, Inc. All Rights Reserved.

#include <nrr/MeshLandmarks.h>
#include <carbon/Algorithm.h>
#include <carbon/io/JsonIO.h>
#include <carbon/io/Utils.h>
#include <nls/serialization/EigenSerialization.h>

#include <optional>
#include <deque>
#include <set>
#include <filesystem>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
std::pair<Eigen::VectorXi, bool> MeshLandmarks<T>::SortCurveUsingMeshTopology(const Mesh<T>& mesh,
                                                                              const Eigen::VectorXi& curve,
                                                                              const std::string& name,
                                                                              bool sortRightToLeft)
{
    if (curve.size() == 0)
    {
        return { curve, false };
    }

    std::vector<int> vIDs;
    for (int i = 0; i < curve.size(); ++i)
    {
        vIDs.push_back(curve[i]);
    }
    std::vector<std::pair<int, int>> edges = mesh.GetEdges(vIDs);

    for (const auto& [vID0, vID1] : edges)
    {
        if (vID0 > vID1)
        {
            CARBON_CRITICAL("edges of mesh should always have the higher vertex ID as the second element.");
        }
    }
    if (edges.empty())
    {
        // no edges that match the curve vertex ids. In this case we decide if a curve is a loop if the first and list item are the same.
        const bool curveIsALoop = (curve.size() > 1 && curve[0] == curve[curve.size() - 1]);
        return { curve, curveIsALoop };
    }

    std::deque<std::pair<int, int>> newCurveEdges;
    newCurveEdges.push_back(edges.front());

    bool curveIsALoop = false;

    while (!curveIsALoop)
    {
        // search forward
        const std::pair<int, int>& currEdge = newCurveEdges.back();
        std::optional<std::pair<int, int>> nextEdge;
        for (const auto& [vID0, vID1] : edges)
        {
            if ((vID0 == currEdge.second) && (vID1 != currEdge.first))
            {
                nextEdge = { vID0, vID1 };
            }
            else if ((vID1 == currEdge.second) && (vID0 != currEdge.first))
            {
                nextEdge = { vID1, vID0 };
            }
        }
        if (nextEdge.has_value() && (nextEdge.value() == edges.front()))
        {
            curveIsALoop = true;
        }
        if (nextEdge.has_value() && !curveIsALoop)
        {
            newCurveEdges.push_back(nextEdge.value());
        }
        else
        {
            break;
        }
    }

    while (!curveIsALoop)
    {
        // search backward
        const std::pair<int, int>& currEdge = newCurveEdges.front();
        std::optional<std::pair<int, int>> nextEdge;
        for (const auto& [vID0, vID1] : edges)
        {
            if ((vID0 == currEdge.first) && (vID1 != currEdge.second))
            {
                nextEdge = { vID1, vID0 };
            }
            else if ((vID1 == currEdge.first) && (vID0 != currEdge.second))
            {
                nextEdge = { vID0, vID1 };
            }
        }
        if (nextEdge.has_value() && (nextEdge.value() == edges.front()))
        {
            curveIsALoop = true;
        }
        if (nextEdge.has_value() && !curveIsALoop)
        {
            newCurveEdges.push_front(nextEdge.value());
        }
        else
        {
            break;
        }
    }

    Eigen::VectorXi newCurve(static_cast<int>(newCurveEdges.size() + 1));
    for (int i = 0; i < static_cast<int>(newCurveEdges.size()); ++i)
    {
        if (i == 0) { newCurve[0] = newCurveEdges[i].first; }
        newCurve[i + 1] = newCurveEdges[i].second;
    }

    if (newCurve.size() != curve.size())
    {
        LOG_WARNING("failed to reorder curve {}", name);
        return { curve, curveIsALoop };
    }
    std::map<int, int> curveCountOld, curveCountNew;
    for (int i = 0; i < curve.size(); ++i)
    {
        curveCountOld[curve[i]]++;
    }
    for (int i = 0; i < newCurve.size(); ++i)
    {
        curveCountNew[newCurve[i]]++;
    }
    if (curveCountOld != curveCountNew)
    {
        LOG_WARNING("failed to reorder curve {}", name);
        return { curve, curveIsALoop };
    }

    if (sortRightToLeft)
    {
        Eigen::Vector3<T> globalDirection = Eigen::Vector3<T>::Zero();
        for (int i = 0; i < int(newCurve.size() - 1); ++i)
        {
            globalDirection += mesh.Vertices().col(newCurve[i + 1]) - mesh.Vertices().col(newCurve[i]);
        }
        if (globalDirection[0] < 0)
        {
            // left to right ordering - invert
            newCurve = newCurve.reverse().eval();
        }
    }

    return { newCurve, curveIsALoop };
}

template <class T>
bool MeshLandmarks<T>::DeserializeJson(const std::string& data, const Mesh<T>& mesh, const std::string& meshName) {
    return DeserializeJson(ReadJson(data), mesh, meshName);
}

template <class T>
bool MeshLandmarks<T>::DeserializeJson(const JsonElement& json, const Mesh<T>& mesh, const std::string& meshName)
{
    m_meshLandmarksBarycentricCoordinates.clear();
    m_meshCurvesBarycentricCoordinates.clear();
    m_contourData.clear();
    m_contours.clear();
    m_innerLowerLipContourLines.clear();
    m_innerUpperLipContourLines.clear();

    auto readBCFromJson = [](const JsonElement& j) {
            Eigen::Vector3i vIDs;
            Eigen::Vector3<T> weights;
            for (int i = 0; i < 3; ++i)
            {
                vIDs[i] = j[2 * i + 0].Value<int>();
                weights[i] = j[2 * i + 1].Value<T>();
            }
            return BarycentricCoordinates<T>(vIDs, weights);
        };

    for (const auto& [name, item] : json.Map())
    {
        if (!item.IsObject())
        {
            CARBON_CRITICAL("key {} should point to a dictionary", name);
        }
        const std::string objectMeshName = item.Contains("mesh") ? item["mesh"].String() : DEFAULT_MESH_NAME;
        if (objectMeshName != meshName)
        {
            continue;
        }

        if (item.Contains("type"))
        {
            const std::string type = item["type"].String();
            if (type == "landmark")
            {
                // landmarks
                if (item.Contains("vID"))
                {
                    m_meshLandmarksBarycentricCoordinates[name] = BarycentricCoordinates<T>::SingleVertex(item["vID"].template Value<int>());
                }
                else if (item.Contains("bc"))
                {
                    m_meshLandmarksBarycentricCoordinates[name] = readBCFromJson(item["bc"]);
                }
                else
                {
                    CARBON_CRITICAL("landmark {} does not contain a vertex ID (vID) or barycentric coordinate (bc)", name);
                }
            }
            else if (type == "curve")
            {
                std::vector<BarycentricCoordinates<T>> bcs;
                if (item.Contains("vIDs"))
                {
                    Eigen::VectorXi curve;
                    bool curveIsLoop = false;
                    io::FromJson(item["vIDs"], curve);
                    std::tie(curve, curveIsLoop) = SortCurveUsingMeshTopology(mesh, curve, name, /*sortRightToLeft=*/false);
                    if (curveIsLoop)
                    {
                        m_loops.insert(name);
                    }
                    for (int i = 0; i < int(curve.size()); ++i)
                    {
                        bcs.push_back(BarycentricCoordinates<T>::SingleVertex(curve[i]));
                    }
                }
                else if (item.Contains("bcs"))
                {
                    for (const JsonElement& j : item["bcs"].Array())
                    {
                        bcs.push_back(readBCFromJson(j));
                    }
                }
                else
                {
                    CARBON_CRITICAL("curve {} does not contain vertex IDs (vIDs) or barycentric coordinates (bcs)", name);
                }
                m_meshCurvesBarycentricCoordinates[name] = bcs;
            }
            else if (type == "zipper")
            {
                // old version of retrieving the zipper data
                io::FromJson(item["lowerlip_region"], m_innerLowerLipContourData.region);
                io::FromJson(item["lowerlip_zipper"], m_innerLowerLipContourData.curve);
                io::FromJson(item["upperlip_region"], m_innerUpperLipContourData.region);
                io::FromJson(item["upperlip_zipper"], m_innerUpperLipContourData.curve);
                m_innerLowerLipContourLines = CalculateContourLines(m_innerLowerLipContourData.region, m_innerLowerLipContourData.curve, mesh, "lowerlip");
                m_innerUpperLipContourLines = CalculateContourLines(m_innerUpperLipContourData.region, m_innerUpperLipContourData.curve, mesh, "upperlip");
            }
            else if (type == "contour")
            {
                ContourData cd;
                io::FromJson(item["vIDs"], cd.curve);
                io::FromJson(item["region"], cd.region);
                m_contours.insert({ name, CalculateContourLines(cd.region, cd.curve, mesh, name) });
                m_contourData.insert({ name, cd });
            }
            else
            {
                CARBON_CRITICAL("unknown type {} for {}", type, name);
            }
        }
        else
        {
            CARBON_CRITICAL("key {} does not contain a subkey of type 'type'", name);
        }
    }

    return true;
}

template <class T>
bool MeshLandmarks<T>::Load(const std::string& filename, const Mesh<T>& mesh, const std::string& meshName)
{
    const std::string meshLandmarksData = ReadFile(filename);
    return DeserializeJson(meshLandmarksData, mesh, meshName);
}

template <class T>
std::string MeshLandmarks<T>::SerializeJson(std::string& previousData, const std::string& meshName) const
{
    JsonElement allData = ReadJson(previousData);
    for (const auto& [name, bc] : m_meshLandmarksBarycentricCoordinates)
    {
        JsonElement json(JsonElement::JsonType::Object);
        json.Insert("type", JsonElement("landmark"));

        JsonElement jsonBC(JsonElement::JsonType::Array);
        for (int i = 0; i < 3; ++i)
        {
            jsonBC.Append(JsonElement(int(bc.Index(i))));
            jsonBC.Append(JsonElement(bc.Weight(i)));
        }
        if (meshName != "")
        {
            json.Insert("mesh", JsonElement(meshName));
        }
        json.Insert("bc", std::move(jsonBC));
        allData.Insert(name, std::move(json));
    }
    for (const auto& [name, bcs] : m_meshCurvesBarycentricCoordinates)
    {
        JsonElement json(JsonElement::JsonType::Object);
        json.Insert("type", JsonElement("curve"));

        JsonElement jsonBCs(JsonElement::JsonType::Array);
        for (const BarycentricCoordinates<T>& bc : bcs)
        {
            JsonElement jsonBC(JsonElement::JsonType::Array);
            for (int i = 0; i < 3; ++i)
            {
                jsonBC.Append(JsonElement(int(bc.Index(i))));
                jsonBC.Append(JsonElement(bc.Weight(i)));
            }
            jsonBCs.Append(std::move(jsonBC));
        }
        if (meshName != "")
        {
            json.Insert("mesh", JsonElement(meshName));
        }
        json.Insert("bcs", std::move(jsonBCs));
        allData.Insert(name, std::move(json));
    }
    for (const auto& [name, contourData] : m_contourData)
    {
        JsonElement json(JsonElement::JsonType::Object);
        json.Insert("type", JsonElement("contour"));

        JsonElement jsonRegion(JsonElement::JsonType::Array);
        JsonElement jsonCurve(JsonElement::JsonType::Array);
        for (int i = 0; i < contourData.region.size(); ++i)
        {
            jsonRegion.Append(JsonElement(int(contourData.region[i])));
        }
        for (int i = 0; i < contourData.curve.size(); ++i)
        {
            jsonCurve.Append(JsonElement(int(contourData.curve[i])));
        }
        if (meshName != "")
        {
            json.Insert("mesh", JsonElement(meshName));
        }
        json.Insert("vIDs", std::move(jsonCurve));
        json.Insert("region", std::move(jsonRegion));
        allData.Insert(name, std::move(json));
    }

    if ((m_innerLowerLipContourData.curve.size() > 0) && (m_innerUpperLipContourData.curve.size() > 0) &&
        (m_innerLowerLipContourData.region.size() > 0) && (m_innerUpperLipContourData.region.size() > 0))
    {
        JsonElement innerLipsJson(JsonElement::JsonType::Object);
        innerLipsJson.Insert("type", JsonElement("zipper"));

        JsonElement upperLipJsonRegion(JsonElement::JsonType::Array);
        JsonElement lowerLipJsonRegion(JsonElement::JsonType::Array);
        JsonElement upperLipJsonZipper(JsonElement::JsonType::Array);
        JsonElement lowerLipJsonZipper(JsonElement::JsonType::Array);

        for (int i = 0; i < m_innerLowerLipContourData.curve.size(); ++i)
        {
            lowerLipJsonZipper.Append(JsonElement(int(m_innerLowerLipContourData.curve[i])));
        }
        for (int i = 0; i < m_innerUpperLipContourData.curve.size(); ++i)
        {
            upperLipJsonZipper.Append(JsonElement(int(m_innerUpperLipContourData.curve[i])));
        }
        for (int i = 0; i < m_innerLowerLipContourData.region.size(); ++i)
        {
            lowerLipJsonRegion.Append(JsonElement(int(m_innerLowerLipContourData.region[i])));
        }
        for (int i = 0; i < m_innerUpperLipContourData.region.size(); ++i)
        {
            upperLipJsonRegion.Append(JsonElement(int(m_innerUpperLipContourData.region[i])));
        }
        innerLipsJson.Insert("lowerlip_zipper", std::move(lowerLipJsonZipper));
        innerLipsJson.Insert("upperlip_zipper", std::move(upperLipJsonZipper));
        innerLipsJson.Insert("lowerlip_region", std::move(lowerLipJsonRegion));
        innerLipsJson.Insert("upperlip_region", std::move(upperLipJsonRegion));
        allData.Insert("inner_lips", std::move(innerLipsJson));
    }

    return WriteJson(allData, /*tabs=*/1);
}

template <class T>
void MeshLandmarks<T>::Save(const std::string& filename, const std::string& meshName) const
{
    std::string arg = "{}";
    WriteFile(filename, SerializeJson(arg, meshName));
}

template <class T>
void MeshLandmarks<T>::Append(const std::string& filename, const std::string& meshName) const
{
    if (!std::filesystem::exists(filename))
    {
        Save(filename, meshName);
    }
    else
    {
        std::string meshLandmarksData = ReadFile(filename);
        WriteFile(filename, SerializeJson(meshLandmarksData, meshName));
    }
}

template <class T>
void MeshLandmarks<T>::MergeCurves(const std::vector<std::string>& curveNames, const std::string& newCurveName, bool removePreviousCurves)
{
    if (curveNames.size() < 2)
    {
        CARBON_CRITICAL("merging curves requires at least two curves");
    }

    if (HasCurve(newCurveName))
    {
        CARBON_CRITICAL("there is a prior curve with name {}", newCurveName);
    }

    // start with the first curve and merge all others
    std::vector<std::string> curveNamesToMerge;
    for (const std::string& curveName : curveNames)
    {
        if (HasCurve(curveName))
        {
            curveNamesToMerge.push_back(curveName);
        }
        else
        {
            LOG_WARNING("cannot merge curve {} into {} as it does not exist", curveName, newCurveName);
        }
    }
    if (curveNamesToMerge.empty())
    {
        LOG_WARNING("cannot create curve {} as there are no matching input curves", newCurveName);
        return;
    }

    std::vector<BarycentricCoordinates<T>> newCurve = m_meshCurvesBarycentricCoordinates[curveNamesToMerge.front()];

    std::set<std::string> toProcess;
    toProcess.insert(curveNamesToMerge.begin() + 1, curveNamesToMerge.end());

    while (toProcess.size() > 0)
    {
        bool mergeOk = false;
        for (const std::string& candidate : toProcess)
        {
            if (TITAN_NAMESPACE::ConcatenateVectorsWithMatchingEndPointsAndUnknownDirection(newCurve, m_meshCurvesBarycentricCoordinates[candidate], newCurve))
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

    // erase previous curves and add the new curve
    if (removePreviousCurves)
    {
        for (const std::string& curveName : curveNamesToMerge)
        {
            m_meshCurvesBarycentricCoordinates.erase(m_meshCurvesBarycentricCoordinates.find(curveName));
        }
    }
    m_meshCurvesBarycentricCoordinates[newCurveName] = newCurve;
}

template <class T>
std::vector<std::vector<int>> MeshLandmarks<T>::CalculateContourLines(const Eigen::VectorXi& region,
                                                                      const Eigen::VectorXi& curve,
                                                                      const Mesh<T>& mesh,
                                                                      const std::string& name)
{
    std::vector<std::vector<int>> contourLines;
    if (curve.size() <= 1)
    {
        return contourLines;
    }

    Eigen::VectorXi curveSorted;
    bool curveIsLoop = false;
    std::tie(curveSorted, curveIsLoop) = SortCurveUsingMeshTopology(mesh, curve, name, /*sortRightToLeft=*/true);

    // vector of which vertices are part of the lip region
    std::vector<bool> inRegion(mesh.NumVertices(), false);
    for (int i = 0; i < int(region.size()); i++)
    {
        inRegion[region[i]] = true;
    }

    // calculate halfEdges that are in the region
    struct HalfEdge
    {
        HalfEdge() = default;
        HalfEdge(int a, int b) : vID1(a), vID2(b)
        {}

        int vID1;
        int vID2;

        bool operator<(const HalfEdge& heOther) const
        {
            return vID1 < heOther.vID1 || (vID1 == heOther.vID1 && vID2 < heOther.vID2);
        }

        HalfEdge Opposite() const
        {
            return HalfEdge(vID2, vID1);
        }
    };


    std::map<HalfEdge, HalfEdge> nextHalfEdge;
    std::map<HalfEdge, HalfEdge> prevHalfEdge;

    auto addHalfEdge = [&](int vID1, int vID2, int vID3) {
            if (inRegion[vID1] || inRegion[vID2] || inRegion[vID3])
            {
                nextHalfEdge[HalfEdge(vID1, vID2)] = HalfEdge(vID2, vID3);
                prevHalfEdge[HalfEdge(vID2, vID3)] = HalfEdge(vID1, vID2);
            }
        };

    auto stepForward = [&](HalfEdge he) {
            return nextHalfEdge[nextHalfEdge[he].Opposite()];
        };

    for (int i = 0; i < mesh.NumQuads(); ++i)
    {
        for (int k = 0; k < 4; k++)
        {
            addHalfEdge(mesh.Quads()(k, i), mesh.Quads()((k + 1) % 4, i), mesh.Quads()((k + 2) % 4, i));
        }
    }
    for (int i = 0; i < mesh.NumTriangles(); ++i)
    {
        for (int k = 0; k < 3; k++)
        {
            addHalfEdge(mesh.Triangles()(k, i), mesh.Triangles()((k + 1) % 3, i), mesh.Triangles()((k + 2) % 3, i));
        }
    }

    Eigen::Vector3<T> accumulatedDirection = Eigen::Vector3<T>::Zero();

    // go through the zipper lines - start by going one step backwards towards mouth corner
    HalfEdge lineStepHE(curveSorted[1], curveSorted[0]);
    lineStepHE = nextHalfEdge[nextHalfEdge[lineStepHE].Opposite()].Opposite();
    for (int i = 0; i < int(curveSorted.size()); i++)
    {
        if (lineStepHE.vID2 != curveSorted[i])
        {
            CARBON_CRITICAL("half edge data structure not compatible with lip zippering lines");
        }
        // the zipper line is sorted from right to left, so the next half edge will go towards the inner lip for the lower
        // lip, and towards the outer lip for the upper lip
        HalfEdge innerStepHE = nextHalfEdge[lineStepHE];
        // step along inner half edges (quad edge loop) until the next half edge would not be in region anymore
        while (inRegion[innerStepHE.vID2])
        {
            innerStepHE = stepForward(innerStepHE);
        }
        // turn around and step one back
        innerStepHE = innerStepHE.Opposite();
        innerStepHE = stepForward(innerStepHE);
        // half edge with both vertex indices being in region
        std::vector<int> contourLine;
        contourLine.push_back(innerStepHE.vID1);
        contourLine.push_back(innerStepHE.vID2);
        innerStepHE = stepForward(innerStepHE);
        while (inRegion[innerStepHE.vID2])
        {
            contourLine.push_back(innerStepHE.vID2);
            innerStepHE = stepForward(innerStepHE);
        }

        for (size_t j = 1; j < contourLine.size(); ++j)
        {
            accumulatedDirection += mesh.Vertices().col(contourLine[j]) - mesh.Vertices().col(contourLine[j - 1]);
        }

        contourLines.push_back(contourLine);

        lineStepHE = stepForward(lineStepHE);
    }

    // check accumulate direction: if the contour lines go from front of the face to the back, then the accumulated z direction should be negative
    if (accumulatedDirection[2] > 0)
    {
        for (auto& contourLine : contourLines)
        {
            // reverse so that the vertices are ordered from front to back
            std::reverse(contourLine.begin(), contourLine.end());
        }
    }

    return contourLines;
}

template <class T>
std::set<int> MeshLandmarks<T>::GetAllVertexIndices() const
{
    std::set<int> vIDs;
    for (const auto& [_, bc] : m_meshLandmarksBarycentricCoordinates)
    {
        vIDs.insert(bc.Index(0));
        vIDs.insert(bc.Index(1));
        vIDs.insert(bc.Index(2));
    }
    for (const auto& [_, bcs] : m_meshCurvesBarycentricCoordinates)
    {
        for (const auto& bc : bcs)
        {
            vIDs.insert(bc.Index(0));
            vIDs.insert(bc.Index(1));
            vIDs.insert(bc.Index(2));
        }
    }
    for (const auto& contourVertices : m_innerLowerLipContourLines)
    {
        for (const auto& vID : contourVertices)
        {
            vIDs.insert(vID);
        }
    }
    for (const auto& contourVertices : m_innerUpperLipContourLines)
    {
        for (const auto& vID : contourVertices)
        {
            vIDs.insert(vID);
        }
    }
    for (const auto& [_, contourRegion] : m_contours)
    {
        for (const auto& contourVertices : contourRegion)
        {
            for (const auto& vID : contourVertices)
            {
                vIDs.insert(vID);
            }
        }
    }
    return vIDs;
}

template <class T>
bool MeshLandmarks<T>::Remap(const std::map<int, int>& oldIndexToNewIndex)
{
    auto remapBC = [&](BarycentricCoordinates<T>& bc) {
            Eigen::Vector3i newIndices = bc.Indices();
            for (int k = 0; k < 3; ++k)
            {
                auto it = oldIndexToNewIndex.find(newIndices[k]);
                if (it != oldIndexToNewIndex.end()) { newIndices[k] = it->second; }
                else { return false; }
                bc = BarycentricCoordinates<T>(newIndices, bc.Weights());
            }
            return true;
        };
    auto meshLandmarksBarycentricCoordinates = m_meshLandmarksBarycentricCoordinates;
    auto meshCurvesBarycentricCoordinates = m_meshCurvesBarycentricCoordinates;
    auto innerLowerLipContourLines = m_innerLowerLipContourLines;
    auto innerUpperLipContourLines = m_innerUpperLipContourLines;
    auto contours = m_contours;
    for (auto& [_, bc] : meshLandmarksBarycentricCoordinates)
    {
        if (!remapBC(bc)) { return false; }
    }
    for (auto& [_, bcs] : meshCurvesBarycentricCoordinates)
    {
        for (auto& bc : bcs)
        {
            if (!remapBC(bc)) { return false; }
        }
    }
    for (auto& contourVertices : innerLowerLipContourLines)
    {
        for (auto& vID : contourVertices)
        {
            auto it = oldIndexToNewIndex.find(vID);
            if (it != oldIndexToNewIndex.end()) { vID = it->second; }
            else { return false; }
        }
    }
    for (auto& contourVertices : innerUpperLipContourLines)
    {
        for (auto& vID : contourVertices)
        {
            auto it = oldIndexToNewIndex.find(vID);
            if (it != oldIndexToNewIndex.end()) { vID = it->second; }
            else { return false; }
        }
    }
    for (auto& [_, contourRegion] : m_contours)
    {
        for (auto& contourVertices : contourRegion)
        {
            for (auto& vID : contourVertices)
            {
                auto it = oldIndexToNewIndex.find(vID);
                if (it != oldIndexToNewIndex.end()) { vID = it->second; }
                else { return false; }
            }
        }
    }

    m_meshLandmarksBarycentricCoordinates = meshLandmarksBarycentricCoordinates;
    m_meshCurvesBarycentricCoordinates = meshCurvesBarycentricCoordinates;
    m_innerLowerLipContourLines = innerLowerLipContourLines;
    m_innerUpperLipContourLines = innerUpperLipContourLines;
    m_contours = contours;

    return true;
}

template <class T>
bool MeshLandmarks<T>::IsLoop(const std::string& curveName) const
{
    const auto it = m_loops.find(curveName);
    return (it != m_loops.end());
}

// explicitly instantiation
template class MeshLandmarks<float>;
template class MeshLandmarks<double>;

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
