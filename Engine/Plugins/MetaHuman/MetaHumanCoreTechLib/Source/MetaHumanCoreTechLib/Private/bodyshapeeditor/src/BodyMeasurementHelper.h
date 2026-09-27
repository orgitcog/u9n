// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <nls/geometry/BarycentricCoordinates.h>
#include <nls/geometry/HalfEdgeMesh.h>
#include <nls/math/Math.h>

#include <string>
#include <vector>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

enum class BarycentricCoordinatesType
{
    Invalid,
    Vertex,
    Edge,
    Face
};

struct BCoordExt
{
    BarycentricCoordinates<float> bc;
    BarycentricCoordinatesType type{BarycentricCoordinatesType::Invalid};
    bool IsValid() const { return type != BarycentricCoordinatesType::Invalid; }
    bool IsVertex() const { return type == BarycentricCoordinatesType::Vertex; }
    bool IsEdge() const { return type == BarycentricCoordinatesType::Edge; }
    bool IsFace() const { return type == BarycentricCoordinatesType::Face; }
};

//! @returns the squared distance of point @p query to the line defined by @p origin and @p direction
static inline float PointToLineDistanceSquared(const Eigen::Vector3f& query, const Eigen::Vector3f& origin, const Eigen::Vector3f& direction)
{
    Eigen::Vector3f linePos = origin + (query - origin).dot(direction) * direction;
    return (query - linePos).squaredNorm();
}

//! @returns the squared distance of point @p query to the segment defined by @p a and @p b
static inline std::pair<float, float> PointToSegmentSquared(const Eigen::Vector3f& query, const Eigen::Vector3f& a, const Eigen::Vector3f& b)
{
    float l = (b - a).squaredNorm();
    if (l > 0)
    {
        float t = std::clamp<float>((query - a).dot(b - a) / l, 0, 1.0f);
        Eigen::Vector3f pos = (1.0f - t) * a + t * b;
        return { t, (query - pos).squaredNorm() };
    }
    else
    {
        return { 0.0f, (query - a).squaredNorm() };
    }
}

//! @returns the closest point on segment @p a and @p b (as coefficient t) and its square distance to the line defined by @p origin and @p direction
static inline std::pair<float, float> SegmentToLineDistanceSquared(const Eigen::Vector3f& a, const Eigen::Vector3f& b, const Eigen::Vector3f& origin, const Eigen::Vector3f& direction)
{
    Eigen::Vector3f delta = b - a;
    if (delta.squaredNorm() < 1e-16f)
    {
        return { 0.5f, PointToLineDistanceSquared(0.5f * (a + b), origin, direction) };
    }
    Eigen::Vector3f n = delta.cross(direction);
    Eigen::Vector3f n2 = direction.cross(n);
    Eigen::Vector3f c1 = a + (origin - a).dot(n2) / (delta.dot(n2)) / delta.norm() * delta;
    float t = (c1 - a).dot(delta) / delta.norm();
    t = std::clamp(t, 0.0f, 1.0f);
    Eigen::Vector3f pos = a + delta * t;
    return { t, PointToLineDistanceSquared(pos, origin, direction) };
}

//! @returns the barycentric coordinates of the point in the triangle @p a, @p b, and @p c that is closest to @p p
static inline Eigen::Vector3f ClosestPointOnTriangle(const Eigen::Vector3f& p, const Eigen::Vector3f& a, const Eigen::Vector3f& b, const Eigen::Vector3f& c)
{
    Eigen::Vector3f e0 = b - a;
    Eigen::Vector3f e1 = c - a;
    Eigen::Vector3f normal = e0.cross(e1);
    if (normal.squaredNorm() < 1e-16f)
    {
        return {0.33334f, 0.33333f, 0.33333f};
    }
    else
    {
        Eigen::Vector3f d = p - a;
        Eigen::Vector3f bary;
        bary[2] = (e0.cross(d).dot(normal)) / normal.squaredNorm();
        bary[1] = (d.cross(e1).dot(normal)) / normal.squaredNorm();
        bary[0] = 1.0f - bary[1] - bary[2];
        bary = bary.array().min(Eigen::Array3f::Ones()).max(Eigen::Array3f::Zero());
        bary /= bary.sum();
        return bary;
    }
}

/**
 * @brief Intersects the line defined by @p origin and @p direction with plane defined by @p a, @p b, and @p c and returns whether the line intersects and the barycentric
 *        coordinates of the intersection.
 * @note the returned point is only the closest point on the triangle to the line if and only the line and point intersect (otherwise a vertex or edge may be closer to the line)
 */
static inline std::pair<bool, Eigen::Vector3f> IntersectLineAndTriangle(const Eigen::Vector3f& origin, const Eigen::Vector3f& direction, const Eigen::Vector3f& a, const Eigen::Vector3f& b, const Eigen::Vector3f& c)
{
    Eigen::Vector3f e0 = b - a;
    Eigen::Vector3f e1 = c - a;
    Eigen::Vector3f triangleNormal = e0.cross(e1);
    if (triangleNormal.squaredNorm() < 1e-16f)
    {
        return {false, {0.33334f, 0.33333f, 0.33333f}};
    }

    float denom = triangleNormal.dot(direction);
    if (denom < 1e-16f)
    {
        return {false, {0.33334f, 0.33333f, 0.33333f}};
    }
    float t = (a - origin).dot(triangleNormal) / denom;
    Eigen::Vector3f p = origin + t * direction;

    Eigen::Vector3f d = p - a;
    Eigen::Vector3f bary;
    bary[2] = (e0.cross(d).dot(triangleNormal)) / triangleNormal.squaredNorm();
    bary[1] = (d.cross(e1).dot(triangleNormal)) / triangleNormal.squaredNorm();
    bary[0] = 1.0f - bary[1] - bary[2];

    const bool intersects = (bary.minCoeff() >= 0.0f) && (bary.maxCoeff() <= 1.0f);

    return {intersects, bary};
}

static inline BCoordExt ClosestMeshCoordToPoint(const HalfEdgeMesh<float>& heMesh, const Eigen::Matrix<float, 3, -1>& vertices, int startVertexIndex, const Eigen::Vector3f& query)
{
    BCoordExt currCoord{{{startVertexIndex, startVertexIndex, startVertexIndex}, {1.0f, 0.0f, 0.0f}}, BarycentricCoordinatesType::Vertex};
    int currHalfEdge = heMesh.vertexEdge[startVertexIndex];
    bool converged = false;
    float bestDist = (vertices.col(startVertexIndex) - query).squaredNorm();

    while (!converged)
    {
        converged = true;
        BCoordExt nextCoord = currCoord;
        int nextHalfEdge = currHalfEdge;
        if (currCoord.IsVertex())
        {
            if (heMesh.halfEdges[currHalfEdge].v0 != currCoord.bc.Index(0))
            {
                // LOG_ERROR("half edge does not match vertex");
                return currCoord;
            }
            // iterate over vertex edges
            bool isBorderVertex = false;
            int stepHalfEdge = currHalfEdge;
            do
            {
                const int vID0 = heMesh.halfEdges[stepHalfEdge].v0;
                if (vID0 != heMesh.halfEdges[currHalfEdge].v0)
                {
                    // LOG_ERROR("iteration over edge vertex edges failed");
                    return currCoord;
                }
                const int vID1 = heMesh.halfEdges[stepHalfEdge].v1;
                auto [t, dist] = PointToSegmentSquared(query, vertices.col(vID0), vertices.col(vID1));
                if (dist < bestDist)
                {
                    bestDist = dist;
                    if (t <= 0)
                    {
                        // LOG_ERROR("this should not happen - vertex is already closest");
                        return currCoord;
                    }
                    else if (t >= 1)
                    {
                        // select end point of edge
                        nextCoord.bc = {{vID1, vID1, vID1}, {1.0f, 0.0f, 0.0f}};
                        nextCoord.type = BarycentricCoordinatesType::Vertex;
                        converged = false;
                        nextHalfEdge = heMesh.halfEdges[stepHalfEdge].next; // use next half edge as vertex needs to have the root in this vertex
                    }
                    else
                    {
                        nextCoord.bc = {{vID0, vID1, vID1}, {1.0f - t, t, 0.0f}};
                        nextCoord.type = BarycentricCoordinatesType::Edge;
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
            Eigen::Vector2i faceHalfEdges{ currHalfEdge, heMesh.halfEdges[currHalfEdge].dual };
            int iter = 0;
            for (int faceHalfEdge : faceHalfEdges)
            {
                if (faceHalfEdge < 0) continue;

                // get all vertex indices for the face
                Eigen::Vector3i indices;
                int currIdx = 0;
                int stepHalfEdge = faceHalfEdge;
                do
                {
                    indices[currIdx++] = heMesh.halfEdges[stepHalfEdge].v0;
                    stepHalfEdge = heMesh.halfEdges[stepHalfEdge].next;
                }
                while (stepHalfEdge != faceHalfEdge && currIdx < 3);

                BCoordExt faceCoord{{indices, ClosestPointOnTriangle(query, vertices.col(indices[0]), vertices.col(indices[1]), vertices.col(indices[2]))}, BarycentricCoordinatesType::Face};
                float dist = (vertices(Eigen::all, faceCoord.bc.Indices()) * faceCoord.bc.Weights() - query).squaredNorm();
                if (dist < bestDist)
                {
                    bestDist = dist;
                    nextCoord = faceCoord;
                    converged = false;
                    nextHalfEdge = faceHalfEdge;
                }
                iter++;
            }

        }
        else if (currCoord.IsFace()) // it is a face
        {
            // get best distances to edges
            int stepHalfEdge = currHalfEdge;
            do
            {
                const int vID0 = heMesh.halfEdges[stepHalfEdge].v0;
                const int vID1 = heMesh.halfEdges[stepHalfEdge].v1;
                auto [t, dist] = PointToSegmentSquared(query, vertices.col(vID0), vertices.col(vID1));
                if (dist < bestDist)
                {
                    bestDist = dist;
                    if (t <= 0)
                    {
                        // select end point of edge
                        nextCoord.bc = {{vID0, vID0, vID0}, {1.0f, 0.0f, 0.0f}};
                        nextCoord.type = BarycentricCoordinatesType::Vertex;
                        converged = false;
                        nextHalfEdge = stepHalfEdge;
                    }
                    else if (t >= 1)
                    {
                        // select end point of edge
                        nextCoord.bc = {{vID1, vID1, vID1}, {1.0f, 0.0f, 0.0f}};
                        nextCoord.type = BarycentricCoordinatesType::Vertex;
                        converged = false;
                        nextHalfEdge = heMesh.halfEdges[stepHalfEdge].next; // use next half edge as vertex needs to have the root in this vertex
                    }
                    else
                    {
                        nextCoord.bc = {{vID0, vID1, vID1}, {1.0f - t, t, 0.0f}};
                        nextCoord.type = BarycentricCoordinatesType::Edge;
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


static inline BCoordExt ClosestMeshCoordToLine(const HalfEdgeMesh<float>& heMesh, const Eigen::Matrix<float, 3, -1>& vertices, int startVertexIndex, const Eigen::Vector3f& origin, const Eigen::Vector3f& direction)
{
    const Eigen::Vector3f dir = direction.normalized();
    BCoordExt currCoord{{{startVertexIndex, startVertexIndex, startVertexIndex}, {1.0f, 0.0f, 0.0f}}, BarycentricCoordinatesType::Vertex};
    int currHalfEdge = heMesh.vertexEdge[startVertexIndex];
    bool converged = false;
    float bestDist = PointToLineDistanceSquared(vertices.col(startVertexIndex), origin, dir);
    const int maxIterations = 1000;
    int iter = 0;

    while (!converged && iter < maxIterations)
    {
        converged = true;
        BCoordExt nextCoord = currCoord;
        int nextHalfEdge = currHalfEdge;
        iter++;
        if (currCoord.IsVertex())
        {
            if (heMesh.halfEdges[currHalfEdge].v0 != currCoord.bc.Index(0))
            {
                // LOG_ERROR("half edge does not match vertex");
                return currCoord;
            }
            // iterate over vertex edges
            bool isBorderVertex = false;
            int stepHalfEdge = currHalfEdge;
            do
            {
                const int vID0 = heMesh.halfEdges[stepHalfEdge].v0;
                if (vID0 != heMesh.halfEdges[currHalfEdge].v0)
                {
                    // LOG_ERROR("iteration over edge vertex edges failed");
                    return currCoord;
                }
                const int vID1 = heMesh.halfEdges[stepHalfEdge].v1;
                auto [t, dist] = SegmentToLineDistanceSquared(vertices.col(vID0), vertices.col(vID1), origin, dir);
                if (dist < bestDist)
                {
                    bestDist = dist;
                    if (t <= 0)
                    {
                        // LOG_ERROR("this should not happen - vertex is already closest");
                        return currCoord;
                    }
                    else if (t >= 1)
                    {
                        nextCoord.bc = {{vID1, vID1, vID1}, {1.0f, 0.0f, 0.0f}};
                        nextCoord.type = BarycentricCoordinatesType::Vertex;
                        converged = false;
                        nextHalfEdge = heMesh.halfEdges[stepHalfEdge].next; // use next half edge as vertex needs to have the root in this vertex
                    }
                    else
                    {
                        nextCoord.bc = {{vID0, vID1, vID1}, {1.0f - t, t, 0.0f}};
                        nextCoord.type = BarycentricCoordinatesType::Edge;
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
            // first check distance of the point to the edges of neighboring faces
            // and if non is closer then we check the faces themselves (should be closer)
            Eigen::Vector2i faceHalfEdges{ currHalfEdge, heMesh.halfEdges[currHalfEdge].dual };

            bool found = false;                
            for (int faceHalfEdge : faceHalfEdges)
            {
                if (faceHalfEdge < 0) continue;

                // check edges of neighboring faces
                int stepHalfEdge = heMesh.halfEdges[faceHalfEdge].next;
                do
                {
                    const int vID0 = heMesh.halfEdges[stepHalfEdge].v0;
                    const int vID1 = heMesh.halfEdges[stepHalfEdge].v1;
                    auto [t, dist] = SegmentToLineDistanceSquared(vertices.col(vID0), vertices.col(vID1), origin, dir);
                    if (dist < bestDist)
                    {
                        found = true;
                        bestDist = dist;
                        if (t <= 0)
                        {
                            // select end point of edge
                            nextCoord.bc = {{vID0, vID0, vID1}, {1.0f, 0.0f, 0.0f}};
                            nextCoord.type = BarycentricCoordinatesType::Vertex;
                            converged = false;
                            nextHalfEdge = stepHalfEdge;
                        }
                        else if (t >= 1)
                        {
                            // select end point of edge
                            nextCoord.bc = {{vID1, vID1, vID1}, {1.0f, 0.0f, 0.0f}};
                            nextCoord.type = BarycentricCoordinatesType::Vertex;
                            converged = false;
                            nextHalfEdge = heMesh.halfEdges[stepHalfEdge].next;
                        }
                        else
                        {
                            nextCoord.bc = {{vID0, vID1, vID1}, {1.0f - t, t, 0.0f}};
                            nextCoord.type = BarycentricCoordinatesType::Edge;
                            converged = false;
                            nextHalfEdge = stepHalfEdge;
                        }
                    }
                    stepHalfEdge = heMesh.halfEdges[stepHalfEdge].next;
                }
                while (stepHalfEdge != faceHalfEdge);
            }

            if (!found)
            {
                for (int faceHalfEdge : faceHalfEdges)
                {
                    if (faceHalfEdge < 0) continue;

                    // get all vertex indices for the face
                    Eigen::Vector3i indices;
                    Eigen::Vector3i heEdges;
                    int currIdx = 0;
                    int stepHalfEdge = faceHalfEdge;
                    do
                    {
                        indices[currIdx] = heMesh.halfEdges[stepHalfEdge].v0;
                        heEdges[currIdx] = stepHalfEdge;
                        stepHalfEdge = heMesh.halfEdges[stepHalfEdge].next;
                        currIdx++;
                    }
                    while (stepHalfEdge != faceHalfEdge && currIdx < 3);

                    auto [intersect, bc] = IntersectLineAndTriangle(origin, dir, vertices.col(indices[0]), vertices.col(indices[1]), vertices.col(indices[2]));
                    if (intersect)
                    {
                        BCoordExt faceCoord({{indices, bc}, BarycentricCoordinatesType::Face});
                        float dist = PointToLineDistanceSquared(vertices(Eigen::all, faceCoord.bc.Indices()) * faceCoord.bc.Weights(), origin, dir);
                        if (dist < bestDist)
                        {
                            bestDist = dist;
                            nextCoord = faceCoord;
                            // converged
                            nextHalfEdge = faceHalfEdge;
                        }
                    }
                }
            }
        }

        currCoord = nextCoord;
        currHalfEdge = nextHalfEdge;
    }

    return currCoord;
}

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
