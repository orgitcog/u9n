// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>

#include <carbon/geometry/AABBTree.h>
#include <carbon/geometry/KdTree.h>
#include <nls/geometry/HalfEdgeMesh.h>
#include <nls/geometry/Mesh.h>

#include <memory>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

//! @return { t, dist } i.e. parameter t and distance of a point to the segment. t is clamped to [0, 1].
template <class T>
std::pair<T, T> PointToSegment(const Eigen::Vector3<T>& query, const Eigen::Vector3<T>& a, const Eigen::Vector3<T>& b)
{
    T l = (b - a).squaredNorm();
    if (l > 0)
    {
        T t = std::clamp<T>((query - a).dot(b - a) / l, 0, T(1));
        Eigen::Vector3f pos = (T(1) - t) * a + t * b;
        return { t, (query - pos).norm() };
    }
    else
    {
        return { T(0), (query - a).norm() };
    }
}

//! Calculates the mean value coordinates of point @p query in the polygon spanned by @p vertices
template <class T, int C>
Eigen::Vector<T, C> MeanValueCoordinates(const Eigen::Vector3f& query, const Eigen::Matrix<T, 3, C>& vertices)
{
    auto safeTanHalf = [] (T cosAngle) {
        if (cosAngle >= 1)
        {
            return T(0);
        }
        else if (cosAngle <= -1)
        {
            return T(1e6); // arbitrary high number
        }
        else
        {
            return T(sqrt((T(1) - cosAngle) / (T(1) + cosAngle)));
        }
    };
    
    Eigen::Vector<T, C> weights(vertices.cols());
    Eigen::Vector<T, C> tanHalfs(vertices.cols());
    for (int c = 0; c < C; ++c)
    {
        const Eigen::Vector3<T> v0 = vertices.col(c);
        const Eigen::Vector3<T> v1 = vertices.col((c + 1) % vertices.cols());
        T cosValue = (v0 - query).normalized().dot((v1 - query).normalized());
        tanHalfs[c] = safeTanHalf(cosValue);
    }
    for (int c = 0; c < C; ++c)
    {
        const Eigen::Vector3<T> v0 = vertices.col(c);
        const T dist = (query - v0).norm();
        if (dist == 0)
        {
            // if the distance to the query is zero then we simply choose that vertex
            weights.setZero();
            weights[c] = T(1);
            return weights;
        }
        const T weight = (tanHalfs[c] + tanHalfs[(c + vertices.cols() - 1) % vertices.cols()]) / dist;
        weights[c] = weight;
    }
    return weights / weights.sum();
}

//! Calculates the mean value coordinates of point @p query in the polygon spanned by @p indices and index @p vertices
template <class T>
std::vector<T> MeanValueCoordinates(const Eigen::Vector3f& query, const std::vector<int>& indices, const Eigen::Matrix<T, 3, -1>& vertices)
{
    auto safeTanHalf = [] (T cosAngle) {
        if (cosAngle >= 1)
        {
            return T(0);
        }
        else if (cosAngle <= -1)
        {
            return T(1e6); // arbitrary high number
        }
        else
        {
            return T(sqrt((T(1) - cosAngle) / (T(1) + cosAngle)));
        }
    };
    
    const int C = (int)indices.size();
    std::vector<T> weights(C);
    std::vector<T> tanHalfs(C);
    for (int c = 0; c < C; ++c)
    {
        const Eigen::Vector3<T> v0 = vertices.col(indices[c]);
        const Eigen::Vector3<T> v1 = vertices.col(indices[(c + 1) % C]);
        T cosValue = (v0 - query).normalized().dot((v1 - query).normalized());
        tanHalfs[c] = safeTanHalf(cosValue);
    }
    float sum = 0;
    for (int c = 0; c < C; ++c)
    {
        const Eigen::Vector3<T> v0 = vertices.col(indices[c]);
        const T dist = (query - v0).norm();
        if (dist == 0)
        {
            weights[c] = T(1e9f);
        }
        else
        {
            const T weight = (tanHalfs[c] + tanHalfs[(c + C - 1) % C]) / dist;
            weights[c] = weight;
        }
        sum += weights[c];
    }
    for (int c = 0; c < C; ++c)
    {
        weights[c] /= sum;
    }
    return weights;
}

struct FaceCoord
{
    enum class Type {
        Vertex,
        Edge,
        Face
    };

    std::vector<int> indices;
    std::vector<float> weights;
    bool fixed{};

    FaceCoord() = default;
    FaceCoord(int vID) : indices(1, vID), weights(1, 1.0f), fixed(false) {}

    bool operator!=(const FaceCoord& other) const
    {
        return (indices != other.indices)
            || (weights != other.weights)
            || (fixed != other.fixed)
        ;
    }
    bool operator==(const FaceCoord& other) const
    {
        return !(*this != other);
    }

    bool IsSamePosition(const FaceCoord& other) const
    {
        return (indices == other.indices)
            && (weights == other.weights);
    }

    bool IsValid() const { return (indices.size() > 0) && (indices.size() == weights.size()); }
    bool IsVertex() const { return indices.size() == 1; }
    bool IsEdge() const { return indices.size() == 2; }
    bool IsTriangle() const { return indices.size() == 3; }
    bool IsQuad() const { return indices.size() == 4; }

    Eigen::Vector3f Evaluate(const Eigen::Matrix<float, 3, -1>& vertices) const
    {
        if (!IsValid()) return Eigen::Vector3f::Zero();
        Eigen::Vector3f result = vertices.col(indices[0]) * weights[0];
        for (size_t i = 1; i < indices.size(); ++i)
        {
            result += vertices.col(indices[i]) * weights[i];
        }
        return result;
    }

    int ClosestVertex(const Eigen::Matrix<float, 3, -1>& vertices) const
    {
        const Eigen::Vector3f pos = Evaluate(vertices);
        int best = 0;
        float bestDist = (pos - vertices.col(indices[0])).norm();
        for (int k = 1; k < (int)indices.size(); ++k)
        {
            float dist = (pos - vertices.col(indices[k])).norm();
            if (dist < bestDist)
            {
                bestDist = dist;
                best = k;
            }
        }
        return indices[best];
    }
};

typedef std::vector<FaceCoord> Contour;

// Correspondence to a contour, where it is assumed a contour is a vector of FaceCoord
struct ContourCorrespondence {
    int segment{-1};
    float t{};
    float geodesicDistance{};
    float euclideanDistance{};
    bool IsValid() const { return (segment >= 0); }
};


class MeshTools
{
public:
    struct ContourRefinementOptions
    {
        int segments = 5;
        bool edgesOnly = true;
        bool useSpline = true;
    };

public:
    MeshTools();
    ~MeshTools();

    //! Get the mesh selection (vertex, edge, face) based on a query point @p query
    std::pair<int, FaceCoord> Select(const std::shared_ptr<const Mesh<float>>& topology,
                                     const std::shared_ptr<const Mesh<float>>& mesh,
                                     const Eigen::Vector3f& query,
                                     FaceCoord::Type type,
                                     float threshold = 1e-1f);

    //! Calculate the shortest path between @p startVertexID and @p endVertexID
    std::vector<FaceCoord> ShortestPath(const std::shared_ptr<const Mesh<float>>& topology, const std::shared_ptr<const Mesh<float>>& mesh, FaceCoord startCoord, FaceCoord endCoord);

    //! Calculate the approximate geodesic distance for all mesh vertices from @p rootCoord up to @p radius
    std::vector<std::pair<float, int>> GeodesicDistance(const std::shared_ptr<const Mesh<float>>& topology, const std::shared_ptr<const Mesh<float>>& mesh, const FaceCoord& rootCoord, float radius);

    //! Calculate the approximate euclidean distance for all mesh vertices from @p rootCoord up to @p radius
    std::vector<std::pair<float, int>> EuclideanDistance(const std::shared_ptr<const Mesh<float>>& topology, const std::shared_ptr<const Mesh<float>>& mesh, const Eigen::Vector3f& query, float radius);

    //! Find closest facecoord on the mesh to a target position using a walk over the mesh faces
    FaceCoord ClosestFaceCoord(const std::shared_ptr<const Mesh<float>>& topology, const std::shared_ptr<const Mesh<float>>& mesh, const Eigen::Vector3f& query, const FaceCoord& initCoord, std::vector<FaceCoord>* path = nullptr);

    //! Calculate the length of the line
    std::vector<float> CalculateLineLength(const std::shared_ptr<const Mesh<float>>& mesh, const std::vector<FaceCoord>& coords) const;

    //! calculate for every vertex of @p mesh the closest correspondence to the contour defined by @p coords
    std::vector<ContourCorrespondence> CalculateClosestCorrespondences(const std::shared_ptr<const Mesh<float>>& topology, const std::shared_ptr<const Mesh<float>>& mesh, const std::vector<FaceCoord>& coords, float maxRadius);

    //! Refine contour
    Contour RefineContour(const std::shared_ptr<const Mesh<float>>& topology, const std::shared_ptr<const Mesh<float>>& mesh, const Contour& contour, const ContourRefinementOptions& options);

    //! Get topologyical symmetry based on a selected edge
    std::vector<int> TopologicalSymmetry(const std::shared_ptr<const Mesh<float>>& topology, const std::shared_ptr<const Mesh<float>>& mesh, const FaceCoord& coord);

private:
    void Prepare(const std::shared_ptr<const Mesh<float>>& topology, const std::shared_ptr<const Mesh<float>>& mesh);
    void PrepareHalfEdgeTopology();
    void PrepareKdTree();
    void PrepareAabbTree();
    void PrepareEdgesMap(bool withDiagonals);

private:
    std::shared_ptr<const Mesh<float>> m_topology;
    std::shared_ptr<const Mesh<float>> m_triTopology;
    std::shared_ptr<const HalfEdgeMesh<float>> m_halfEdgeTopology;
    std::vector<std::pair<int, bool>> m_triToQuadMapping;
    std::shared_ptr<const Mesh<float>> m_mesh;
    std::shared_ptr<AABBTree<float>> m_aabbTree;
    std::shared_ptr<KdTree<float>> m_kdTree;
    bool m_requiresEdgesMapNoDiagonalsUpdate{};
    std::vector<std::vector<std::pair<int, float>>> m_edgesMapNoDiagonals;
    bool m_requiresEdgesMapIncludingDiagonalsUpdate{};
    std::vector<std::vector<std::pair<int, float>>> m_edgesMapIncludingDiagonals;
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
