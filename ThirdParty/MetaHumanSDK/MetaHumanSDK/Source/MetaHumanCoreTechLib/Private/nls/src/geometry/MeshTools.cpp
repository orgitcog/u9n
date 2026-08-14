// Copyright Epic Games, Inc. All Rights Reserved.

#include <nls/geometry/MeshTools.h>

#include <carbon/utils/Timer.h>
#include <nls/geometry/CatmullRom.h>

#include <queue>
#include <set>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

namespace
{
    void UpdateEdgesMap(std::vector<std::vector<std::pair<int, float>>>& edgesMap, const Eigen::Matrix<float, 3, -1>& vertices)
    {
        if (edgesMap.size() != (size_t)vertices.cols())
        {
            CARBON_CRITICAL("edge map and vertices are not matching");
        }

        for (int vID0 = 0; vID0 < (int)edgesMap.size(); ++vID0) {
            for (auto& [vID1, dist] : edgesMap[vID0]) {
                dist = (vertices.col(vID0) - vertices.col(vID1)).norm();
            }
        }
    }

    std::vector<std::vector<std::pair<int, float>>> CalculateEdgeMap(const Mesh<float>& topology, const Eigen::Matrix<float, 3, -1>& vertices, bool allowDiagonalsInQuads)
    {
        std::vector<std::vector<std::pair<int, float>>> edgesMap;

        // calculate edge map on reference
        edgesMap.clear();
        edgesMap.resize(topology.NumVertices());
        auto addEdge = [&](int vID0, int vID1) {
            if (vID0 > vID1)
            {
                edgesMap[vID0].push_back({vID1, 0.0f});
            }
            else
            {
                edgesMap[vID1].push_back({vID0, 0.0f});
            }
        };

        for (int i = 0; i < topology.NumQuads(); ++i) {
            const int vID0 = topology.Quads()(0, i);
            const int vID1 = topology.Quads()(1, i);
            const int vID2 = topology.Quads()(2, i);
            const int vID3 = topology.Quads()(3, i);
            if (allowDiagonalsInQuads)
            {
                edgesMap[vID0].push_back({vID2, 0.0f});
                edgesMap[vID1].push_back({vID3, 0.0f});
            }
            addEdge(vID0, vID1);
            addEdge(vID1, vID2);
            addEdge(vID2, vID3);
            addEdge(vID3, vID0);
        }
        for (int i = 0; i < topology.NumTriangles(); ++i) {
            const int vID0 = topology.Triangles()(0, i);
            const int vID1 = topology.Triangles()(1, i);
            const int vID2 = topology.Triangles()(2, i);
            addEdge(vID0, vID1);
            addEdge(vID1, vID2);
            addEdge(vID2, vID0);
        }

        for (int vID0 = 0; vID0 < (int)edgesMap.size(); ++vID0)
        {
            for (const auto& [vID1, _] : edgesMap[vID0])
            {
                // vID1 will always be smaller than vID0, so we can push to vID0 map
                edgesMap[vID1].push_back({vID0, 0.0f});
            }
        }

        UpdateEdgesMap(edgesMap, vertices);

        return edgesMap;
    }

    std::vector<FaceCoord> CalculateShortestPath(const Eigen::Matrix<float, 3, -1>& vertices, const FaceCoord& startCoord, const FaceCoord& endCoord, const std::vector<std::vector<std::pair<int, float>>>& edgesMap)
    {
        CARBON_ASSERT(startCoord.IsValid(), "start coord needs to be valid");
        CARBON_ASSERT(endCoord.IsValid(), "end coord needs to be valid");

        const int numVertices = (int)vertices.cols();
        std::vector<float> distances(numVertices, 1e9f);
        std::vector<int> prev(numVertices, -1);

        std::priority_queue<std::pair<float, int>, std::vector<std::pair<float, int>>, std::greater<std::pair<float, int>>> queue;

        for (size_t i = 0; i < startCoord.indices.size(); ++i) {
            const int vID = startCoord.indices[i];
            prev[vID] = vID;
            distances[vID] = (startCoord.Evaluate(vertices) - vertices.col(vID)).norm();
            queue.push({distances[vID], vID});
        }

        int end = endCoord.IsVertex() ? endCoord.indices[0] : endCoord.ClosestVertex(vertices);

        while (queue.size() > 0) {
            auto [currDist, id] = queue.top();
            queue.pop();
            if (id == end)
            {
                break;
            }
            // look at all neighbors and update neighborhood
            for (const auto& [oid, dist] : edgesMap[id]) {
                if (currDist + dist < distances[oid]) {
                    distances[oid] = currDist + dist;
                    prev[oid] = id;
                    queue.push({distances[oid], oid});
                }
            }
        }

        std::vector<FaceCoord> path = {endCoord};
        if (!endCoord.IsVertex())
        {
            path.push_back(FaceCoord(end));
        }
        int currId = path.back().indices[0];

        while (true)
        {
            int prevId = prev[currId];
            if (prevId == currId) break;
            currId = prevId;
            path.push_back(FaceCoord(currId));
        }

        if (!startCoord.IsVertex())
        {
            path.push_back(startCoord);
        }

        std::reverse(path.begin(), path.end());

        return path;
    }
}

MeshTools::MeshTools() = default;
MeshTools::~MeshTools() = default;

void MeshTools::Prepare(const std::shared_ptr<const Mesh<float>>& topology, const std::shared_ptr<const Mesh<float>>& mesh)
{
    if (m_topology != topology)
    {
        m_topology = topology;
        m_triTopology.reset();
        m_halfEdgeTopology.reset();
        if (m_topology)
        {
            if (m_topology->NumQuads() > 0)
            {
                auto triTopology = std::make_shared<Mesh<float>>(*m_topology);
                m_triToQuadMapping = triTopology->Triangulate();
                m_triTopology = triTopology;
            }
            else
            {
                m_triTopology = m_topology;
                m_triToQuadMapping.resize(m_triTopology->NumTriangles());
                for (int i = 0; i < (int)m_triToQuadMapping.size(); ++i)
                {
                    m_triToQuadMapping[i] = {i, false};
                }
            }
        }

        m_aabbTree.reset();
        m_kdTree.reset();
        m_edgesMapNoDiagonals.clear();
        m_edgesMapIncludingDiagonals.clear();
    }
    if (m_mesh != mesh)
    {
        m_mesh = mesh;
        m_aabbTree.reset();
        m_kdTree.reset();
        m_requiresEdgesMapNoDiagonalsUpdate = true;
        m_requiresEdgesMapIncludingDiagonalsUpdate = true;
    }
}

void MeshTools::PrepareHalfEdgeTopology()
{
    if (m_topology && !m_halfEdgeTopology)
    {
        m_halfEdgeTopology = std::make_shared<HalfEdgeMesh<float>>(*m_topology);
    }
}

void MeshTools::PrepareKdTree()
{
    if (m_mesh)
    {
        if (!m_kdTree)
        {
            m_kdTree = std::make_shared<KdTree<float>>(m_mesh->Vertices().transpose());
        }
    }
    else
    {
        m_kdTree.reset();
    }
}

void MeshTools::PrepareAabbTree()
{
    if (m_mesh)
    {
        if (!m_aabbTree)
        {
            m_aabbTree = std::make_shared<AABBTree<float>>(m_mesh->Vertices().transpose(), m_triTopology->Triangles().transpose());
        }
    }
    else
    {
        m_aabbTree.reset();
    }
}

void MeshTools::PrepareEdgesMap(bool withDiagonals)
{
    if (withDiagonals)
    {
        if (m_edgesMapIncludingDiagonals.empty())
        {
            m_edgesMapIncludingDiagonals = CalculateEdgeMap(*m_topology, m_mesh->Vertices(), /*allowDiagonalsInQuads=*/true);
        }
        else
        {
            UpdateEdgesMap(m_edgesMapIncludingDiagonals, m_mesh->Vertices());

        }
        m_requiresEdgesMapIncludingDiagonalsUpdate = false;
    }
    else
    {
        if (m_edgesMapNoDiagonals.empty())
        {
            m_edgesMapNoDiagonals = CalculateEdgeMap(*m_topology, m_mesh->Vertices(), /*allowDiagonalsInQuads=*/false);
        }
        else
        {
            UpdateEdgesMap(m_edgesMapNoDiagonals, m_mesh->Vertices());
        }
        m_requiresEdgesMapNoDiagonalsUpdate = false;
    }
}


std::pair<int, FaceCoord> MeshTools::Select(
    const std::shared_ptr<const Mesh<float>>& topology,
    const std::shared_ptr<const Mesh<float>>& mesh,
    const Eigen::Vector3f& query,
    FaceCoord::Type type,
    float threshold)
{
    std::pair<int, FaceCoord> selection = { -1, FaceCoord() };

    Prepare(topology, mesh);
    PrepareAabbTree();
    if (m_aabbTree)
    {
        const auto [tID, bc, dist] = m_aabbTree->getClosestPoint(query.transpose(), 1.0f);
        if (tID >= 0 && std::sqrt(dist) < threshold)
        {
            if (type == FaceCoord::Type::Vertex)
            {
                int idx;
                bc.maxCoeff(&idx);
                const int vID = m_triTopology->Triangles().col(tID)[idx];
                selection = { vID, FaceCoord(vID) };
            }
            else
            {
                // check if it is a face or triangle
                const std::pair<int, bool> mapping = m_triToQuadMapping[tID];
                if (mapping.second)
                {
                    // it is a quad
                    const int quadID = mapping.first;
                    const Eigen::Vector4i faceIndices = m_topology->Quads().col(quadID);

                    Eigen::Matrix<float, 3, 4> faceVertices;
                    for (int c = 0; c < 4; ++c)
                    {
                        faceVertices.col(c) = mesh->Vertices().col(faceIndices[c]);
                    }

                    if (type == FaceCoord::Type::Edge)
                    {
                        // find closest edge
                        float bestDist = std::numeric_limits<float>::max();
                        for (int eID = 0; eID < 4; ++eID)
                        {
                            Eigen::Vector2i edge(faceIndices[eID], faceIndices[(eID + 1) % faceIndices.size()]);
                            auto [t, edgeDist] = PointToSegment<float>(query, mesh->Vertices().col(edge[0]), mesh->Vertices().col(edge[1]));
                            if (edgeDist < bestDist)
                            {
                                bestDist = edgeDist;
                                selection.second.indices.resize(2);
                                Eigen::Map<Eigen::Vector2i>(selection.second.indices.data()) = edge;
                                selection.second.weights = { 1.0f - t, t };
                            }
                        }
                        return selection;
                    }
                    else
                    {
                        CARBON_ASSERT(type == FaceCoord::Type::Face, "logical error");
                        // estimate barycentric coordinates of quad
                        selection.second.indices.resize(4);
                        Eigen::Map<Eigen::Vector4i>(selection.second.indices.data()) = faceIndices;
                        selection.second.weights.resize(4);
                        Eigen::Map<Eigen::Vector4f>(selection.second.weights.data()) = MeanValueCoordinates<float, 4>(query, faceVertices);
                        selection.first = quadID;
                        return selection;

                    }
                }
                else
                {
                    // it is a triangle
                    const int triID = mapping.first;
                    const Eigen::Vector3f triWeights = bc.transpose();
                    const Eigen::Vector3i faceIndices = m_topology->Triangles().col(triID);

                    if (type == FaceCoord::Type::Edge)
                    {
                        // find closest edge
                        float bestDist = std::numeric_limits<float>::max();
                        for (int eID = 0; eID < 3; ++eID)
                        {
                            Eigen::Vector2i edge(faceIndices[eID], faceIndices[(eID + 1) % faceIndices.size()]);
                            auto [t, edgeDist] = PointToSegment<float>(query, mesh->Vertices().col(edge[0]), mesh->Vertices().col(edge[1]));
                            if (edgeDist < bestDist)
                            {
                                bestDist = edgeDist;
                                selection.second.indices.resize(2);
                                Eigen::Map<Eigen::Vector2i>(selection.second.indices.data()) = edge;
                                selection.second.weights = { 1.0f - t, t };
                            }
                        }
                    }
                    else
                    {
                        CARBON_ASSERT(type == FaceCoord::Type::Face, "logical error");
                        // estimate barycentric coordinates of quad
                        selection.second.indices.resize(3);
                        Eigen::Map<Eigen::Vector3i>(selection.second.indices.data()) = faceIndices;
                        selection.second.weights.resize(3);
                        Eigen::Map<Eigen::Vector3f>(selection.second.weights.data()) = triWeights;
                        selection.first = triID;
                        return selection;
                    }
                }
            }
        }
    }


    return selection;
}

std::vector<FaceCoord> MeshTools::ShortestPath(const std::shared_ptr<const Mesh<float>>& topology, const std::shared_ptr<const Mesh<float>>& mesh, FaceCoord startCoord, FaceCoord endCoord)
{
    Prepare(topology, mesh);
    PrepareEdgesMap(false);
    return CalculateShortestPath(mesh->Vertices(), startCoord, endCoord, m_edgesMapNoDiagonals);
}

std::vector<std::pair<float, int>> MeshTools::EuclideanDistance(const std::shared_ptr<const Mesh<float>>& topology, const std::shared_ptr<const Mesh<float>>& mesh, const Eigen::Vector3f& query, float radius)
{
    Prepare(topology, mesh);
    PrepareKdTree();
    const std::pair<std::vector<int64_t>, std::vector<float>> pointsInRadius = m_kdTree->getPointsInRadius(query.transpose(), radius);
    std::vector<std::pair<float, int>> out;
    out.reserve(pointsInRadius.first.size());
    for (size_t i = 0; i < pointsInRadius.first.size(); ++i)
    {
        out.push_back({std::sqrt(pointsInRadius.second[i]), (int)pointsInRadius.first[i]});
    }
    return out;
}

std::vector<std::pair<float, int>> MeshTools::GeodesicDistance(const std::shared_ptr<const Mesh<float>>& topology, const std::shared_ptr<const Mesh<float>>& mesh, const FaceCoord& rootCoord, float radius)
{
    if (!topology || !mesh) return {};
    Prepare(topology, mesh);
    PrepareHalfEdgeTopology();
    PrepareEdgesMap(true);

    const int numVertices = mesh->NumVertices();
    std::vector<float> distances(numVertices, -1.0f);

    // queue element
    // first: best distance to contour
    // second: vertex
    typedef std::tuple<float, int> QueueElement;
    std::priority_queue<QueueElement, std::vector<QueueElement>, std::greater<QueueElement>> queue;

    // initiate the geodesic distance for the first point/line/face
    for (size_t i = 0; i < rootCoord.indices.size(); ++i)
    {
        const Eigen::Vector3f& coordPt = rootCoord.Evaluate(mesh->Vertices());

        // set geodesic distance as euclidean distance to initialize geodesic search
        if (rootCoord.weights[i] > 0)
        {
            const int vID = rootCoord.indices[i];
            distances[vID] = (coordPt - mesh->Vertices().col(vID)).norm();
            queue.push({distances[vID], vID});
        }
    }

    while (queue.size() > 0) {
        auto [currDist, vID] = queue.top();
        queue.pop();
        for (const auto& [oid, dist] : m_edgesMapIncludingDiagonals[vID]) {
            float newDist = currDist + dist;
            if (newDist < radius && (distances[oid] < 0 || newDist < distances[oid])) {
                distances[oid] = currDist + dist;
                queue.push({distances[oid], oid});
            }
        }
    }

    std::vector<std::pair<float, int>> result;
    for (int vID = 0; vID < (int)distances.size(); ++vID)
    {
        if (distances[vID] >= 0 && distances[vID] < radius)
        {
            result.push_back({distances[vID], vID});
        }
    }
    return result;
}

FaceCoord MeshTools::ClosestFaceCoord(const std::shared_ptr<const Mesh<float>>& topology, const std::shared_ptr<const Mesh<float>>& mesh, const Eigen::Vector3f& query, const FaceCoord& initCoord, std::vector<FaceCoord>* path)
{
    Prepare(topology, mesh);
    PrepareHalfEdgeTopology();

    // always start with a vertex coord to make sure half edge structure and input face coord match
    int k = 0;
    Eigen::Map<const Eigen::VectorXf>(initCoord.weights.data(), (int)initCoord.weights.size()).maxCoeff(&k);
    const int startVertexIndex = initCoord.indices[k];

    const auto& heMesh = *m_halfEdgeTopology;
    const Eigen::Matrix<float, 3, -1>& vertices = mesh->Vertices();

    FaceCoord currCoord(startVertexIndex);
    int currHalfEdge = heMesh.vertexEdge[startVertexIndex];
    bool converged = false;
    float bestDist = (currCoord.Evaluate(vertices) - query).norm();

    if (path) path->clear();

    while (!converged)
    {
        if (path) path->push_back(currCoord);

        converged = true;
        FaceCoord nextCoord = currCoord;
        int nextHalfEdge = currHalfEdge;
        if (currCoord.IsVertex())
        {
            if (heMesh.halfEdges[currHalfEdge].v0 != currCoord.indices[0])
            {
                CARBON_CRITICAL("half edge does not match vertex");
            }
            // iterate over vertex edges
            bool isBorderVertex = false;
            int stepHalfEdge = currHalfEdge;
            do
            {
                const int vID0 = heMesh.halfEdges[stepHalfEdge].v0;
                if (vID0 != heMesh.halfEdges[currHalfEdge].v0)
                {
                    CARBON_CRITICAL("iteration over edge vertex edges failed");
                }
                const int vID1 = heMesh.halfEdges[stepHalfEdge].v1;
                auto [t, dist] = PointToSegment<float>(query, vertices.col(vID0), vertices.col(vID1));
                if (dist < bestDist)
                {
                    if (t <= 0)
                    {
                        CARBON_CRITICAL("this should not happen - vertex is already closest: {} {}", bestDist, dist);
                    }

                    bestDist = dist;
                    if (t <= 0)
                    {
                        CARBON_CRITICAL("this should not happen - vertex is already closest");
                    }
                    else if (t >= 1)
                    {
                        // select end point of edge
                        nextCoord.indices = { vID1 };
                        nextCoord.weights = { 1.0f };
                        converged = false;
                        nextHalfEdge = heMesh.halfEdges[stepHalfEdge].next; // use next half edge as vertex needs to have the root in this vertex
                    }
                    else
                    {
                        nextCoord.indices = { vID0, vID1 };
                        nextCoord.weights = { 1.0f - t, t };
                        converged = false;
                        nextHalfEdge = stepHalfEdge;
                    }
                }
                // get reverse half edge and step to next half edge
                stepHalfEdge = heMesh.halfEdges[stepHalfEdge].dual;
                if (stepHalfEdge < 0)
                {
                    // vertex is a border vertex, either stop or continue by starting into the other direction
                    if (isBorderVertex)
                    {
                        // stop
                        stepHalfEdge = currHalfEdge;
                    }
                    else
                    {
                        isBorderVertex = true;
                        stepHalfEdge = currHalfEdge;
                        // go backwards to find start border edge
                        while (heMesh.halfEdges[heMesh.halfEdges[stepHalfEdge].prev].dual >= 0)
                        {
                            stepHalfEdge = heMesh.halfEdges[heMesh.halfEdges[stepHalfEdge].prev].dual;
                        }
                    }
                }
                else
                {
                    stepHalfEdge = heMesh.halfEdges[stepHalfEdge].next;
                }

            }
            while (stepHalfEdge != currHalfEdge);
        }
        else if (currCoord.IsEdge())
        {
            // check distance of the point to the neighboring faces
            std::vector<int> faceHalfEdges = { currHalfEdge };
            if (heMesh.halfEdges[currHalfEdge].dual >= 0)
            {
                faceHalfEdges.push_back(heMesh.halfEdges[currHalfEdge].dual);
            }
            for (int faceHalfEdge : faceHalfEdges)
            {
                // get all vertex indices for the face
                std::vector<int> indices;
                int stepHalfEdge = faceHalfEdge;
                do
                {
                    indices.push_back(heMesh.halfEdges[stepHalfEdge].v0);
                    stepHalfEdge = heMesh.halfEdges[stepHalfEdge].next;
                }
                while (stepHalfEdge != faceHalfEdge);
                // get optimal coordinates
                FaceCoord faceCoord;
                faceCoord.indices = indices;
                faceCoord.weights = MeanValueCoordinates(query, indices, vertices);
                float dist = (faceCoord.Evaluate(vertices) - query).norm();
                if (dist < bestDist)
                {
                    bestDist = dist;
                    nextCoord = faceCoord;
                    converged = false;
                    nextHalfEdge = faceHalfEdge;
                }
            }

        }
        else if (currCoord.indices.size() > 2) // it is a face
        {
            // get best distances to edges
            int stepHalfEdge = currHalfEdge;
            do
            {
                const int vID0 = heMesh.halfEdges[stepHalfEdge].v0;
                const int vID1 = heMesh.halfEdges[stepHalfEdge].v1;
                auto [t, dist] = PointToSegment<float>(query, vertices.col(vID0), vertices.col(vID1));
                if (dist < bestDist)
                {
                    bestDist = dist;
                    if (t <= 0)
                    {
                        // select end point of edge
                        nextCoord.indices = { vID0 };
                        nextCoord.weights = { 1.0f };
                        converged = false;
                        nextHalfEdge = stepHalfEdge;
                    }
                    else if (t >= 1)
                    {
                        // select end point of edge
                        nextCoord.indices = { vID1 };
                        nextCoord.weights = { 1.0f };
                        converged = false;
                        nextHalfEdge = heMesh.halfEdges[stepHalfEdge].next; // use next half edge as vertex needs to have the root in this vertex
                    }
                    else
                    {
                        nextCoord.indices = { vID0, vID1 };
                        nextCoord.weights = { 1.0f - t, t };
                        converged = false;
                        nextHalfEdge = stepHalfEdge;
                    }
                }
                stepHalfEdge = heMesh.halfEdges[stepHalfEdge].next;
            }
            while (stepHalfEdge != currHalfEdge);
        }

        currCoord = nextCoord;
        currHalfEdge = nextHalfEdge;
    }

    return currCoord;
}


std::vector<float> MeshTools::CalculateLineLength(const std::shared_ptr<const Mesh<float>>& mesh, const std::vector<FaceCoord>& coords) const
{
    std::vector<float> out;
    if (coords.empty() || !mesh) return {};
    out.push_back(0);
    Eigen::Vector3f curr;
    Eigen::Vector3f prev = coords.front().Evaluate(mesh->Vertices());
    for (size_t j = 1; j < coords.size(); ++j)
    {
        curr = coords[j].Evaluate(mesh->Vertices());
        out.push_back((curr - prev).norm() + out.back());
        std::swap(prev, curr);
    }
    return out;
}


std::vector<ContourCorrespondence> MeshTools::CalculateClosestCorrespondences(const std::shared_ptr<const Mesh<float>>& topology, const std::shared_ptr<const Mesh<float>>& mesh, const std::vector<FaceCoord>& coords, float maxRadius)
{
    if (coords.empty()) return {};
    if (!topology || !mesh) return {};

    Prepare(topology, mesh);
    PrepareHalfEdgeTopology();
    PrepareEdgesMap(true);

    const int numVertices = mesh->NumVertices();
    std::vector<ContourCorrespondence> allCorrespondences(numVertices);

    for (int segment = 0; segment < (int)coords.size(); ++segment)
    {
        const auto& coord = coords[segment];

        // calculate closest correspondences to each contour point and add all adjoinging vertices i.e. vertex, edge, or face vertices depending on the coordinate type
        for (size_t i = 0; i < coord.indices.size(); ++i)
        {
            const Eigen::Vector3f& coordPt = coord.Evaluate(mesh->Vertices());

            // set geodesic distance as euclidean distance to initialize geodesic search
            if (coord.weights[i] > 0)
            {
                const int vID = coord.indices[i];
                const float distance = (coordPt - mesh->Vertices().col(vID)).norm();
                if (!allCorrespondences[vID].IsValid() || distance < allCorrespondences[vID].geodesicDistance)
                {
                    allCorrespondences[vID].euclideanDistance = distance;
                    allCorrespondences[vID].geodesicDistance = distance;
                    if (coords.size() > 1 && segment == (int)coords.size() - 1)
                    {
                        allCorrespondences[vID].segment = segment - 1;
                        allCorrespondences[vID].t = 1.0f;
                    }
                    else
                    {
                        allCorrespondences[vID].segment = segment;
                        allCorrespondences[vID].t = 0.0f;
                    }
                }
            }
        }
    }

    // correspondence to contour
    for (int segment = 0; segment < (int)coords.size() - 1; ++segment)
    {
        std::vector<FaceCoord> shortestPath = CalculateShortestPath(mesh->Vertices(), coords[segment], coords[segment + 1], m_edgesMapIncludingDiagonals);
        const Eigen::Vector3f pt1 = coords[segment].Evaluate(mesh->Vertices());
        const Eigen::Vector3f pt2 = coords[segment + 1].Evaluate(mesh->Vertices());

        for (int j = 0; j < (int)shortestPath.size(); ++j)
        {
            // use euclidean distance to line between coords as approximation for geodesic distance
            auto getNextHalfEdge = [](const HalfEdgeMesh<float>& heMesh, int halfEdge) {
                if (heMesh.halfEdges[halfEdge].dual)
                {
                    return heMesh.halfEdges[heMesh.halfEdges[halfEdge].dual].next;
                }
                else
                {
                    // border vertex, so go backwards to find half edge that belongs to a border face
                    while (heMesh.halfEdges[heMesh.halfEdges[halfEdge].prev].dual >= 0)
                    {
                        halfEdge = heMesh.halfEdges[heMesh.halfEdges[halfEdge].prev].dual;
                    }
                    return heMesh.halfEdges[halfEdge].prev;
                }
            };
            // get all faces belonging to the vertex and add the corresponding vertices
            std::vector<int> faces;
            int halfEdge = m_halfEdgeTopology->vertexEdge[shortestPath[j].indices.front()];
            int nextHalfEdge = halfEdge;
            do {
                int stepHalfEdge = nextHalfEdge;
                do {
                    const int vID = m_halfEdgeTopology->halfEdges[stepHalfEdge].v0;
                    const Eigen::Vector3f query = mesh->Vertices().col(vID);
                    const auto [t, dist] = PointToSegment(query, pt1, pt2);
                    if (allCorrespondences[vID].segment < 0 || dist < allCorrespondences[vID].euclideanDistance)
                    {
                        allCorrespondences[vID].euclideanDistance = dist;
                        allCorrespondences[vID].geodesicDistance = dist;
                        allCorrespondences[vID].segment = segment;
                        allCorrespondences[vID].t = t;
                    }
                    stepHalfEdge = m_halfEdgeTopology->halfEdges[stepHalfEdge].next;
                } while (stepHalfEdge != nextHalfEdge);
                nextHalfEdge = getNextHalfEdge(*m_halfEdgeTopology, nextHalfEdge);
            } while (nextHalfEdge != halfEdge);
        }
    }

    // queue element
    // first: best distance to contour
    // second: previous vertex in shortest path calculation
    // third: origin vertex
    typedef std::tuple<float, int, int> QueueElement;
    std::priority_queue<QueueElement, std::vector<QueueElement>, std::greater<QueueElement>> queue;

    for (int vID = 0; vID < numVertices; ++vID)
    {
        if (allCorrespondences[vID].IsValid())
        {
            queue.push({allCorrespondences[vID].geodesicDistance, vID, vID});
        }
    }

    while (queue.size() > 0) {
        auto [currDist, vID, origin] = queue.top();
        queue.pop();
        for (const auto& [oid, dist] : m_edgesMapIncludingDiagonals[vID]) {
            float newDist = currDist + dist;
            if (newDist < maxRadius && (!allCorrespondences[oid].IsValid() || newDist < allCorrespondences[oid].geodesicDistance)) {
                allCorrespondences[oid].geodesicDistance = currDist + dist;
                allCorrespondences[oid].segment = allCorrespondences[origin].segment;
                allCorrespondences[oid].t = allCorrespondences[origin].t;
                queue.push({allCorrespondences[oid].geodesicDistance, oid, origin});
            }
        }
    }

    // update euclidean distances
    for (int vID = 0; vID < numVertices; ++vID)
    {
        auto& correspondence = allCorrespondences[vID];
        if (correspondence.IsValid())
        {
            if (correspondence.t > 0)
            {
                const Eigen::Vector3f pt1 = coords[correspondence.segment].Evaluate(mesh->Vertices());
                const Eigen::Vector3f pt2 = coords[correspondence.segment + 1].Evaluate(mesh->Vertices());
                correspondence.euclideanDistance = (((1.0f - correspondence.t) * pt1 + correspondence.t * pt2) - mesh->Vertices().col(vID)).norm();
            }
            else
            {
                correspondence.euclideanDistance = (coords[correspondence.segment].Evaluate(mesh->Vertices()) - mesh->Vertices().col(vID)).norm();
            }
        }
    }

    return allCorrespondences;
}


Contour MeshTools::RefineContour(
    const std::shared_ptr<const Mesh<float>>& topology,
    const std::shared_ptr<const Mesh<float>>& mesh,
    const Contour& contour,
    const ContourRefinementOptions& options)
{
    if (contour.size() < 2) return contour;

    std::vector<Eigen::Vector3f> fixedPoints;
    std::vector<int> fixedPointIndices;
    for (int i = 0; i < (int)contour.size(); ++i)
    {
        if (contour[i].fixed || (i == 0) || (i == (int)contour.size() - 1))
        {
            fixedPoints.push_back(contour[i].Evaluate(mesh->Vertices()));
            fixedPointIndices.push_back(i);
        }
    }
    const bool isClosed = contour.front().IsSamePosition(contour.back()) && fixedPointIndices.size() > 3;

    const Eigen::Map<const Eigen::Matrix<float, 3, -1>> fixedPointsEigen((const float*)fixedPoints.data(), 3, (int)fixedPoints.size() - (isClosed ? 1 : 0));
    CatmullRom<float, 3> spline(fixedPointsEigen, options.segments, isClosed);
    std::vector<float> splineLengths;
    splineLengths.push_back(0);
    for (int i = 1; i < spline.SampledPoints().NumControlPoints(); ++i)
    {
        splineLengths.push_back(splineLengths.back() + (spline.SampledPoints().ControlPoints().col(i) - spline.SampledPoints().ControlPoints().col(i-1)).norm());
    }

    std::vector<FaceCoord> newContour;
    for (int a = 0; a < (int)fixedPointIndices.size() - 1; ++a)
    {
        const int idx1 = fixedPointIndices[a];
        const int idx2 = fixedPointIndices[a + 1];

        newContour.push_back(contour[idx1]);
        auto path = ShortestPath(m_topology, mesh, contour[idx1], contour[idx2]);

        if (path.size() > 1 && !options.edgesOnly)
        {
            const std::vector<float> lineLengths = CalculateLineLength(mesh, path);
            const Eigen::Vector3f startPosition = path.front().Evaluate(mesh->Vertices());
            const Eigen::Vector3f endPosition = path.back().Evaluate(mesh->Vertices());
            for (int j = 1; j < (int)path.size() - 1; ++j)
            {
                // const float t = (path.size() > 1) ? float(i)/(float)(path.size() - 1) : 0;
                const float t = lineLengths[j] / lineLengths.back();
                Eigen::Vector3f pos = (1.0f - t) * startPosition + t * endPosition;

                // get corresponding position on spline
                if (options.useSpline)
                {
                    const int segmentStartIdx = a * options.segments;
                    const float splineSegmentStart = splineLengths[segmentStartIdx];
                    const float splineSegmentEnd = splineLengths[segmentStartIdx + options.segments];
                    for (int k = 0; k < options.segments; ++k)
                    {
                        float ratio1 = (splineLengths[segmentStartIdx + k] - splineSegmentStart) / (splineSegmentEnd - splineSegmentStart);
                        float ratio2 = (splineLengths[segmentStartIdx + k + 1] - splineSegmentStart) /  (splineSegmentEnd - splineSegmentStart);
                        if (t >= ratio1 && t <= ratio2)
                        {
                            pos = spline.SampledPoints().EvaluatePoint(segmentStartIdx + k, (t - ratio1)/(ratio2 - ratio1));
                        }
                    }
                }

                path[j] = ClosestFaceCoord(topology, mesh, pos, path[j]);
                newContour.push_back(path[j]);
                newContour.back().fixed = false;
            }
        }
        else
        {
            // use shortest path
            for (size_t j = 1; j < path.size() - 1; ++j)
            {
                newContour.push_back(path[j]);
                newContour.back().fixed = false;
            }
        }
        if (a == (int)fixedPointIndices.size() - 2)
        {
            newContour.push_back(contour[idx2]);
        }
    }

    return newContour;
}

std::vector<int> MeshTools::TopologicalSymmetry(const std::shared_ptr<const Mesh<float>>& topology, const std::shared_ptr<const Mesh<float>>& mesh, const FaceCoord& coord)
{
    Prepare(topology, mesh);
    PrepareHalfEdgeTopology();
    if (m_halfEdgeTopology && coord.IsValid() && coord.IsEdge())
    {
        const int startEdge = m_halfEdgeTopology->vertexEdge[coord.indices[0]];
        int nextEdge = startEdge;
        if (m_halfEdgeTopology->halfEdges[nextEdge].v1 == coord.indices[1])
        {
            return m_halfEdgeTopology->GetTopologicalSymmetry(nextEdge);
        }
        do {
            int dual = m_halfEdgeTopology->halfEdges[nextEdge].dual;
            if (dual >= 0)
            {
                nextEdge = m_halfEdgeTopology->halfEdges[dual].next;
            }
            else
            {
                nextEdge = startEdge;
            }
            if (m_halfEdgeTopology->halfEdges[nextEdge].v1 == coord.indices[1])
            {
                return m_halfEdgeTopology->GetTopologicalSymmetry(nextEdge);
            }
        } while (nextEdge >= 0 && nextEdge != startEdge);
        do {
            int prev = m_halfEdgeTopology->halfEdges[nextEdge].prev;
            int dual = m_halfEdgeTopology->halfEdges[prev].dual;
            if (dual >= 0)
            {
                nextEdge = dual;
            }
            else
            {
                nextEdge = startEdge;
            }
            if (m_halfEdgeTopology->halfEdges[nextEdge].v1 == coord.indices[1])
            {
                return m_halfEdgeTopology->GetTopologicalSymmetry(nextEdge);
            }
        } while (nextEdge >= 0 && nextEdge != startEdge);
    }
    return {};
}


CARBON_NAMESPACE_END(TITAN_NAMESPACE)
