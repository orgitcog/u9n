// Copyright Epic Games, Inc. All Rights Reserved.

#include <nls/geometry/HalfEdgeMesh.h>

#include <map>
#include <queue>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
HalfEdgeMesh<T>::HalfEdgeMesh(const Mesh<T>& m) {
    // set vertices
    vertices = m.Vertices();

    // get list of triangles and quads
    const Eigen::Matrix3Xi& ft = m.Triangles();
    const Eigen::Matrix4Xi& fq = m.Quads();

    // helper structure for quick finding oposite edges
    std::map<std::pair<int, int>, int> edgeMap;

    // convert to edges
    const int numFaces = (int)ft.cols() + (int)fq.cols();
    const int numHalfEdges = (int)ft.cols() * 3 + (int)fq.cols() * 4;
    halfEdges.resize(numHalfEdges, { -1, -1, -1, -1, -1, -1 });
    vertexEdge.resize(vertices.cols(), -1);
    faces.resize(numFaces, -1);

    // go over all triangles and quads and convert them to half edges

    int count = 0;
    int fcount = 0;
    // process all triangles
    for (int i = 0; i < ft.cols(); i++) {
        const Eigen::Vector3i& f = ft.col(i);

        const int start = count;
        for (int d = 0; d < 3; d++) {
            // create half edge
            auto& halfEdge = halfEdges[count];
            halfEdge.v0 = f[d];
            halfEdge.v1 = f[(d + 1) % 3];
            halfEdge.next = start + (d + 1) % 3;
            halfEdge.prev = start + (d + 2) % 3;
            halfEdge.face = fcount;

            // find if we already created a dual, if so, add the respective indices
            if (auto it = edgeMap.find({ halfEdge.v1 , halfEdge.v0 }); it != edgeMap.end()) {
                halfEdges[it->second].dual = count;
                halfEdge.dual = it->second;
            }

            edgeMap[{ halfEdge.v0, halfEdge.v1 }] = count;

            // create the face for the first entry
            if (d == 0) {
                faces[fcount] = count;
            }

            // check if we stored the vertex edge yet, if not, save it
            if (vertexEdge[halfEdge.v0] == -1) {
                vertexEdge[halfEdge.v0] = count;
            }

            count++;
        }
        fcount++;
    }

    // process all quads
    for (int i = 0; i < fq.cols(); i++) {
        const Eigen::Vector4i& f = fq.col(i);

        const int start = count;
        for (int d = 0; d < 4; d++) {
            // create half edge
            auto& halfEdge = halfEdges[count];
            halfEdge.v0 = f[d];
            halfEdge.v1 = f[(d + 1) % 4];
            halfEdge.next = start + (d + 1) % 4;
            halfEdge.prev = start + (d + 3) % 4;
            halfEdge.face = fcount;

            // find if we already created a dual, if so, add the respective indices
            if (auto it = edgeMap.find({ halfEdge.v1 , halfEdge.v0 }); it != edgeMap.end()) {
                halfEdges[it->second].dual = count;
                halfEdge.dual = it->second;
            }

            edgeMap[{ halfEdge.v0, halfEdge.v1 }] = count;

            // create the face for the first entry
            if (d == 0) {
                faces[fcount] = count;
            }

            // check if we stored the vertex edge yet, if not, save it
            if (vertexEdge[halfEdge.v0] == -1) {
                vertexEdge[halfEdge.v0] = count;
            }

            count++;
        }
        fcount++;
    }

    // TODO: check for holes in geometry and process them to generate valid dual edges with faces marked as -1
}

template <class T>
std::vector<int> HalfEdgeMesh<T>::GetTopologicalSymmetry(const int referenceEdge) const
{
    std::vector<int> symmetry(vertices.cols(), -1);
    std::vector<int> processed(halfEdges.size(), -1);
    std::vector<int> edgeSymmetry(halfEdges.size(), -1);

    std::queue<int> queue;
    queue.push(referenceEdge);
    edgeSymmetry[referenceEdge] = halfEdges[referenceEdge].dual;
    
    while (!queue.empty()) {
        const int currentEdge = queue.front();
        queue.pop();
        if (processed[currentEdge] >= 0) {
            continue;
        }
        const int symEdge = edgeSymmetry[currentEdge];

        // if there's not symmetric edge or it has already been visited, the mesh
        // is not topologically symmetric, so we return an empty set
        if (symEdge == -1 || processed[symEdge] >= 0) {
            return {};
        }

        const HalfEdge& edge = halfEdges[currentEdge];
        const HalfEdge& edgeSym = halfEdges[symEdge];

        // add vertex symmetries
        symmetry[edge.v0] = edgeSym.v1;
        symmetry[edge.v1] = edgeSym.v0;
        symmetry[edgeSym.v0] = edge.v1;
        symmetry[edgeSym.v1] = edge.v0;

        processed[currentEdge] = 1;
        processed[symEdge] = 1;

        // add next/prev edge pairs as symmetric as well as any dual edges
        edgeSymmetry[edge.next] = edgeSym.prev;
        edgeSymmetry[edgeSym.next] = edge.prev;
        edgeSymmetry[edge.prev] = edgeSym.next;
        edgeSymmetry[edgeSym.prev] = edge.next;

        queue.push(edge.next);
        queue.push(edgeSym.next);
        queue.push(edge.prev);
        queue.push(edgeSym.prev);

        // only process dual edges if they actually exist
        // TODO: this will go away if we actually process holes correctly
        if (edge.dual == -1 || edgeSym.dual == -1) {
            if (edge.dual != edgeSym.dual) {
                // the mesh is not topologically symmetric
                return {};
            }
            else {
                // ignore the dual edges, as both of them are boundaries
            }
        }
        else {
            // process as usual
            edgeSymmetry[edge.dual] = edgeSym.dual;
            edgeSymmetry[edgeSym.dual] = edge.dual;
            queue.push(edge.dual);
            queue.push(edgeSym.dual);
        }
    }

    return symmetry;
}

template struct HalfEdgeMesh<float>;
template struct HalfEdgeMesh<double>;

}