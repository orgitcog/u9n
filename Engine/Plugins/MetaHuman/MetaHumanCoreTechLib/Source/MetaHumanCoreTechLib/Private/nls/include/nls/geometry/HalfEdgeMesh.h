// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>
#include <carbon/utils/TaskThreadPool.h>
#include <nls/math/Math.h>
#include <nls/geometry/Mesh.h>

#include <vector>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

    /**
     * Half edge mesh class representing meshes with polygonal faces
     *
     */
    template <class T>
    struct HalfEdgeMesh
    {
        // main half edge data structure
        struct HalfEdge
        {
            int v0;         // source vertex index for half-edge
            int v1;         // target vertex index for half-edge
            int next;       // next edge in loop
            int prev;       // previous edge in loop
            int dual;       // dual edge in neighboring face, might be -1 if edge
            int face;       // face index for halfedge
        };

        Eigen::Matrix<T, 3, -1> vertices;

        // list of half edges
        std::vector<HalfEdge> halfEdges;

        // map from vertex id to one of the half-edges originating at that vertex
        std::vector<int> vertexEdge;
        // map from face index to one of the half-edges part of that face
        std::vector<int> faces;

    public:
        HalfEdgeMesh() = default;
        HalfEdgeMesh(const HalfEdgeMesh& o) = default;
        HalfEdgeMesh& operator=(const HalfEdgeMesh& o) = default;

        HalfEdgeMesh(const Mesh<T>& m);

        std::vector<int> GetTopologicalSymmetry(const int referenceEdge) const;
    };

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
