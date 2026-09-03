// Copyright Epic Games, Inc. All Rights Reserved.

#include <nls/geometry/GeometryHelpers.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE::geoutils)

std::vector<std::vector<int>> VertexAdjacency(const Eigen::Matrix<int, 3, -1>& Triangles)
{
    const int NumVertices = Triangles.maxCoeff() + 1;
    const int NumTriangles = static_cast<int>(Triangles.cols());
    std::vector<std::vector<int>> adjacency(NumVertices);

    for (int i = 0; i < NumTriangles; i++)
    {
        const int vID0 = Triangles(0, i);
        const int vID1 = Triangles(1, i);
        const int vID2 = Triangles(2, i);

        adjacency[vID0].push_back(vID1);
        adjacency[vID1].push_back(vID2);
        adjacency[vID2].push_back(vID0);

        adjacency[vID1].push_back(vID0);
        adjacency[vID2].push_back(vID1);
        adjacency[vID0].push_back(vID2);
    }

    for(int i = 0; i < NumVertices; i++){
        std::sort(adjacency[i].begin(), adjacency[i].end());
        adjacency[i].erase(std::unique(adjacency[i].begin(), adjacency[i].end()), adjacency[i].end());
    }
    return adjacency;
}


void TriangleAdjacency(const Eigen::Matrix<int, 3, -1>& triangles, Eigen::Matrix<int, 3, -1>& adjacency, Eigen::Matrix<int, 3, -1>& outerTriangleEdges)
{
    adjacency.resize(3, triangles.cols());
    adjacency.setConstant(-1);
    outerTriangleEdges.resize(3, triangles.cols());
    outerTriangleEdges.setConstant(-1);

    typedef std::pair<int, int> HalfEdge;
    typedef std::pair<int, int> EdgeTriangle;
    std::map<HalfEdge, EdgeTriangle> halfEdgeToTriangle;
    for (int i = 0; i < int(triangles.cols()); i++)
    {
        const int vID0 = triangles(0, i);
        const int vID1 = triangles(1, i);
        const int vID2 = triangles(2, i);
        halfEdgeToTriangle[HalfEdge(vID0, vID1)] = EdgeTriangle(0, i);
        halfEdgeToTriangle[HalfEdge(vID1, vID2)] = EdgeTriangle(1, i);
        halfEdgeToTriangle[HalfEdge(vID2, vID0)] = EdgeTriangle(2, i);
    }

    for (auto&& query : halfEdgeToTriangle){
        const int vID0 = query.first.first;
        const int vID1 = query.first.second;

        if (vID0 < vID1)
        {
            auto answer = halfEdgeToTriangle.find(HalfEdge(vID1, vID0));
            if (answer != halfEdgeToTriangle.end())
            {
                adjacency(query.second.first, query.second.second) = answer->second.second;
                adjacency(answer->second.first, answer->second.second) = query.second.second;
                outerTriangleEdges(query.second.first, query.second.second) = answer->second.first;
                outerTriangleEdges(answer->second.first, answer->second.second) = query.second.first;
            }
            else {
                // Robustness to open mesh
                adjacency(query.second.first, query.second.second) = -1;
                outerTriangleEdges(query.second.first, query.second.second) = -1;
            }
        }
    }
}


void QuadAdjacency(
    const Eigen::Matrix<int, 4, -1>& quads,
    Eigen::Matrix<int, 4, -1>& adjacency, 
    Eigen::Matrix<int, 4, -1>& outerQuadEdges)
{
    adjacency.resize(4, quads.cols());
    adjacency.setConstant(-1);
    outerQuadEdges.resize(4, quads.cols());
    outerQuadEdges.setConstant(-1);

    typedef std::pair<int, int> HalfEdge;
    typedef std::pair<int, int> EdgeQuad;
    std::map<HalfEdge, EdgeQuad> halfEdgeToQuad;
    for (int i = 0; i < int(quads.cols()); i++)
    {
        const int vID0 = quads(0, i);
        const int vID1 = quads(1, i);
        const int vID2 = quads(2, i);
        const int vID3 = quads(3, i);
        halfEdgeToQuad[HalfEdge(vID0, vID1)] = EdgeQuad(0, i);
        halfEdgeToQuad[HalfEdge(vID1, vID2)] = EdgeQuad(1, i);
        halfEdgeToQuad[HalfEdge(vID2, vID3)] = EdgeQuad(2, i);
        halfEdgeToQuad[HalfEdge(vID3, vID0)] = EdgeQuad(3, i);
    }

    for (auto&& query : halfEdgeToQuad){
        const int vID0 = query.first.first;
        const int vID1 = query.first.second;

        if (vID0 < vID1)
        {
            auto answer = halfEdgeToQuad.find(HalfEdge(vID1, vID0));
            if (answer != halfEdgeToQuad.end())
            {
                adjacency(query.second.first, query.second.second) = answer->second.second;
                adjacency(answer->second.first, answer->second.second) = query.second.second;
                outerQuadEdges(query.second.first, query.second.second) = answer->second.first;
                outerQuadEdges(answer->second.first, answer->second.second) = query.second.first;
            }
            else {
                // Robustness to open mesh
                adjacency(query.second.first, query.second.second) = -1;
                outerQuadEdges(query.second.first, query.second.second) = -1;
            }
        }
    }
}

std::vector<std::vector<int>> VertexAdjacency(const Eigen::Matrix<int, 4, -1>& Tets)
{
    const int NumVertices = Tets.maxCoeff() + 1;
    const int NumTets = static_cast<int>(Tets.cols());
    std::vector<std::vector<int>> adjacency(NumVertices);

    for (int i = 0; i < NumTets; i++)
    {
        const int vID0 = Tets(0, i);
        const int vID1 = Tets(1, i);
        const int vID2 = Tets(2, i);
        const int vID3 = Tets(3, i);

        adjacency[vID0].push_back(vID1);
        adjacency[vID1].push_back(vID2);
        adjacency[vID2].push_back(vID0);
        adjacency[vID0].push_back(vID3);
        adjacency[vID1].push_back(vID3);
        adjacency[vID2].push_back(vID3);

        adjacency[vID1].push_back(vID0);
        adjacency[vID2].push_back(vID1);
        adjacency[vID0].push_back(vID2);
        adjacency[vID3].push_back(vID0);
        adjacency[vID3].push_back(vID1);
        adjacency[vID3].push_back(vID2);
    }

    for(int i = 0; i < NumVertices; i++){
        std::sort(adjacency[i].begin(), adjacency[i].end());
        adjacency[i].erase(std::unique(adjacency[i].begin(), adjacency[i].end()), adjacency[i].end());
    }
    return adjacency;
}

CARBON_NAMESPACE_END(TITAN_NAMESPACE::geoutils)
