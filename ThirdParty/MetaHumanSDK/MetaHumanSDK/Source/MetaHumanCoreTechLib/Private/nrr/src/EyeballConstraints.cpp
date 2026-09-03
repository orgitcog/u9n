// Copyright Epic Games, Inc. All Rights Reserved.

#include <nrr/EyeballConstraints.h>

#include <carbon/geometry/AABBTree.h>
#include <nls/functions/PointPointConstraintFunction.h>
#include <nls/functions/PointSurfaceConstraintFunction.h>
#include <nls/geometry/BarycentricCoordinates.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
struct EyeballConstraints<T>::Private
{
    //! The eyeball mesh
    Mesh<T> eyeballMesh;

    //! The center of the eyeball which is used as center for raycasting to calculate intersection with the eyeball
    Eigen::Vector3<T> eyeballCenter;

    //! The AABB tree for ray-eyeball intersection
    std::unique_ptr<AABBTree<T>> eyeballAABBTree;

    //! Vertices that should lie directly on the eyeball
    VertexWeights<T> interfaceVertexWeights;

    //! Vertices that are affected by the eyeball and are set to have a distance based on a rest pose
    VertexWeights<T> influenceVertexWeights;

    //! Distance of the influence vertices to the eyeball mesh
    std::vector<T> influenceEyeballDistances;

    Configuration config = { std::string("Eyeball Constraints Configuration"), {
                                 //!< how much weight to use on eyeball constraint
                                 { "eyeball", ConfigurationParameter(T(1), T(0), T(1)) },
                                 //!< whether to use closest distance for constraints
                                 { "useClosestEyeballPosition", ConfigurationParameter(false) },
                                 //!< how much to displace the target points along the normal
                                 { "normalDisplacement", ConfigurationParameter(T(0), T(-1000), T(1000)) },
                             } };
};

template <class T>
typename EyeballConstraints<T>::EyeballMatch EyeballConstraints<T>::GetClosestEyeballPosition(const Eigen::Vector3<T>& vertex,
                                                                                              bool useClosestEyeballPosition) const
{
    EyeballMatch match;
    match.tID = -1;
    if (useClosestEyeballPosition)
    {
        const auto [tID, bcWeights, distSq] = m->eyeballAABBTree->getClosestPoint(vertex, std::numeric_limits<T>::max());
        if (tID >= 0)
        {
            match.tID = tID;
            const BarycentricCoordinates<T> bc(m->eyeballMesh.Triangles().col(tID), bcWeights.transpose());
            match.eyeballPosition = bc.template Evaluate<3>(m->eyeballMesh.Vertices());
            match.eyeballNormal = bc.template Evaluate<3>(m->eyeballMesh.VertexNormals()).normalized();
            const bool outside = ((vertex - match.eyeballPosition).dot(match.eyeballNormal) >= 0);
            const T sign = (outside ? T(1) : T(-1));
            const Eigen::Vector3<T> dir = sign * (vertex - match.eyeballPosition).normalized();
            match.dir= dir;
            match.dist = std::sqrt(distSq) * sign;
        }
    }
    else
    {
        const Eigen::Vector3<T> delta = (vertex - m->eyeballCenter);
        const auto [tID, bcWeights, dist] = m->eyeballAABBTree->intersectRay(m->eyeballCenter.transpose(), delta.normalized().transpose());
        if (tID >= 0)
        {
            match.tID = tID;
            const BarycentricCoordinates<T> bc(m->eyeballMesh.Triangles().col(tID), bcWeights.transpose());
            match.eyeballPosition = bc.template Evaluate<3>(m->eyeballMesh.Vertices());
            match.eyeballNormal = bc.template Evaluate<3>(m->eyeballMesh.VertexNormals()).normalized();
            match.dir = delta.normalized();
            match.dist = delta.norm() - dist;
        }
    }
    return match;
}

template <class T>
EyeballConstraints<T>::EyeballConstraints()
    : m(new Private)
{}

template <class T> EyeballConstraints<T>::~EyeballConstraints() = default;
template <class T> EyeballConstraints<T>::EyeballConstraints(EyeballConstraints&&) = default;
template <class T> EyeballConstraints<T>& EyeballConstraints<T>::operator=(EyeballConstraints&&) = default;


template <class T>
const Configuration& EyeballConstraints<T>::GetConfiguration() const
{
    return m->config;
}

template <class T>
void EyeballConstraints<T>::SetConfiguration(const Configuration& config) { m->config.Set(config); }

template <class T>
void EyeballConstraints<T>::SetInfluenceVertices(const VertexWeights<T>& influenceVertexWeights) { m->influenceVertexWeights = influenceVertexWeights; }

template <class T>
const VertexWeights<T>& EyeballConstraints<T>::InfluenceVertices() const
{
    return m->influenceVertexWeights;
}

template <class T>
void EyeballConstraints<T>::SetInfluenceEyeballDistances(const std::vector<T>& distances)
{
    if (m->influenceEyeballDistances.size() != distances.size())
    {
        CARBON_CRITICAL("size of distances ({}) does not match number of influence vertices ({})", distances.size(), m->influenceEyeballDistances.size());
    }
    m->influenceEyeballDistances = distances;
}

template <class T>
const std::vector<T>& EyeballConstraints<T>::InfluenceEyeballDistances() const
{
    return m->influenceEyeballDistances;
}

template <class T>
void EyeballConstraints<T>::SetInterfaceVertices(const VertexWeights<T>& interfaceVertexWeights) { m->interfaceVertexWeights = interfaceVertexWeights; }

template <class T>
const VertexWeights<T>& EyeballConstraints<T>::InterfaceVertices() const
{
    return m->interfaceVertexWeights;
}

template <class T>
void EyeballConstraints<T>::SetEyeballMesh(const Mesh<T>& eyeballMesh)
{
    m->eyeballMesh = eyeballMesh;
    m->eyeballMesh.Triangulate();
    m->eyeballMesh.CalculateVertexNormals();
    m->eyeballCenter = m->eyeballMesh.Vertices().rowwise().mean();

    SetEyeballPose(eyeballMesh.Vertices());
}

template <class T>
std::vector<T> EyeballConstraints<T>::GetEyeballDistances(const Eigen::Matrix<T, 3, -1>& vertices) const
{
    std::vector<T> eyeballDistances;
    eyeballDistances.clear();
    eyeballDistances.reserve(vertices.cols());

    const bool useClosestEyeballPosition = m->config["useClosestEyeballPosition"].template Value<bool>();

    for (int i = 0; i < static_cast<int>(vertices.cols()); ++i)
    {
        const Eigen::Vector3<T> vertexPos = vertices.col(i);
        const EyeballMatch match = GetClosestEyeballPosition(vertexPos, useClosestEyeballPosition);
        eyeballDistances.push_back(match.dist);
    }

    return eyeballDistances;
}

template <class T>
std::vector<T> EyeballConstraints<T>::GetEyeballDistances(const Eigen::Matrix<T, 3, -1>& vertices, const std::vector<int>& vIDs) const
{
    std::vector<T> eyeballDistances;
    eyeballDistances.clear();
    eyeballDistances.reserve(vIDs.size());

    const bool useClosestEyeballPosition = m->config["useClosestEyeballPosition"].template Value<bool>();

    for (int i = 0; i < static_cast<int>(vIDs.size()); ++i)
    {
        const Eigen::Vector3<T> vertexPos = vertices.col(vIDs[i]);
        const EyeballMatch match = GetClosestEyeballPosition(vertexPos, useClosestEyeballPosition);
        eyeballDistances.push_back(match.dist);
    }

    return eyeballDistances;
}

template <class T>
void EyeballConstraints<T>::SetRestPose(const Eigen::Matrix<T, 3, -1>& eyeballVertices, const Eigen::Matrix<T, 3, -1>& targetVertices)
{
    SetEyeballPose(eyeballVertices);

    const int numNonzeros = int(m->influenceVertexWeights.NonzeroVertices().size());
    Eigen::Matrix<T, 3, -1> vertices(3, numNonzeros);
    for (int i = 0; i < numNonzeros; ++i)
    {
        vertices.col(i) = targetVertices.col(m->influenceVertexWeights.NonzeroVertices()[i]);
    }
    m->influenceEyeballDistances = GetEyeballDistances(vertices);
}

template <class T>
void EyeballConstraints<T>::SetEyeballPose(const Eigen::Matrix<T, 3, -1>& eyeballVertices)
{
    m->eyeballMesh.SetVertices(eyeballVertices);
    m->eyeballMesh.CalculateVertexNormals();
    m->eyeballAABBTree = std::make_unique<AABBTree<T>>(m->eyeballMesh.Vertices().transpose(), m->eyeballMesh.Triangles().transpose());
    m->eyeballCenter = m->eyeballMesh.Vertices().rowwise().mean();
}

template <class T>
DiffData<T> EyeballConstraints<T>::EvaluateEyeballConstraints(const DiffDataMatrix<T, 3, -1>& targetVertices) const
{
    const T eyeballWeight = m->config["eyeball"].template Value<T>();
    const bool useClosestEyeballPosition = m->config["useClosestEyeballPosition"].template Value<bool>();

    if (eyeballWeight > 0)
    {
        const int numInterfacePts = int(m->interfaceVertexWeights.NonzeroVertices().size());
        const int numInfluencePts = int(m->influenceVertexWeights.NonzeroVertices().size());
        const int numPts = numInterfacePts + numInfluencePts;
        Eigen::Matrix<T, 3, -1> positions(3, numPts);
        Eigen::Matrix<T, 3, -1> normals(3, numPts);
        Eigen::VectorXi vertexIDs(numPts);
        Eigen::VectorX<T> weights(numPts);

        // interface points are located directly on the eyeball
        for (int i = 0; i < numInterfacePts; ++i)
        {
            const auto [vID, weight] = m->interfaceVertexWeights.NonzeroVerticesAndWeights()[i];
            const Eigen::Vector3<T> vertexPos = targetVertices.Matrix().col(vID);
            const EyeballMatch match = GetClosestEyeballPosition(vertexPos, useClosestEyeballPosition);
            positions.col(i) = match.eyeballPosition + m->config["normalDisplacement"].template Value<T>() * match.dir;
            normals.col(i) = match.eyeballNormal;
            vertexIDs[i] = vID;
            weights[i] = weight;
        }

        // influence points are vertices that are affected by the eyeball
        for (int i = 0; i < numInfluencePts; ++i)
        {
            const auto [vID, weight] = m->influenceVertexWeights.NonzeroVerticesAndWeights()[i];
            const Eigen::Vector3<T> vertexPos = targetVertices.Matrix().col(vID);
            const EyeballMatch match = GetClosestEyeballPosition(vertexPos, useClosestEyeballPosition);
            if (match.tID >= 0)
            {
                const Eigen::Vector3<T> targetPos = match.eyeballPosition + (m->influenceEyeballDistances[i] + m->config["normalDisplacement"].template Value<T>()) * match.dir;
                positions.col(numInterfacePts + i) = targetPos;
                normals.col(numInterfacePts + i) = match.eyeballNormal;
                vertexIDs[numInterfacePts + i] = vID;
                weights[numInterfacePts + i] = weight;
            }
            else
            {
                positions.col(numInterfacePts + i).setZero();
                normals.col(numInterfacePts + i).setZero();
                vertexIDs[numInterfacePts + i] = vID;
                weights[numInterfacePts + i] = T(0);
            }
        }

        return PointSurfaceConstraintFunction<T, 3>::Evaluate(targetVertices, vertexIDs, positions, normals, weights, eyeballWeight);
    }
    else
    {
        return DiffData<T>(Vector<T>());
    }
}

template <class T>
void EyeballConstraints<T>::SetupEyeballConstraints(const Eigen::Matrix<T, 3, -1>& vertices, VertexConstraints<T, 1, 1>& vertexConstraints) const
{
    const T eyeballWeight = m->config["eyeball"].template Value<T>();
    SetupEyeballInterfaceConstraints(vertices, m->interfaceVertexWeights, eyeballWeight, vertexConstraints);
    SetupEyeballInfluenceConstraints(vertices, m->influenceVertexWeights, m->influenceEyeballDistances, eyeballWeight, vertexConstraints);
}

template <class T>
void EyeballConstraints<T>::SetupEyeballInterfaceConstraints(const Eigen::Matrix<T, 3, -1>& vertices,
                                                             const VertexWeights<T>& interfaceVertexWeights,
                                                             T eyeballWeight,
                                                             VertexConstraints<T, 1, 1>& vertexConstraints) const
{
    if (eyeballWeight <= 0) { return; }
    const int numInterfacePts = (int)interfaceVertexWeights.NonzeroVertices().size();
    if (numInterfacePts == 0) { return; }

    const bool useClosestEyeballPosition = m->config["useClosestEyeballPosition"].template Value<bool>();
    const T sqrtEyeballWeight = std::sqrt(eyeballWeight);

    vertexConstraints.ResizeToFitAdditionalConstraints(numInterfacePts);

    // interface points are located directly on the eyeball
    for (int i = 0; i < numInterfacePts; ++i)
    {
        const auto [vID, weight] = interfaceVertexWeights.NonzeroVerticesAndWeights()[i];
        const Eigen::Vector3<T> vertexPos = vertices.col(vID);
        const EyeballMatch match = GetClosestEyeballPosition(vertexPos, useClosestEyeballPosition);
        const T totalWeight = sqrtEyeballWeight * weight;
        const T residual = totalWeight * match.eyeballNormal.dot(vertexPos - match.eyeballPosition);
        const Eigen::Matrix<T, 1, 3> drdV = totalWeight * match.eyeballNormal.transpose();
        vertexConstraints.AddConstraint(vID, residual, drdV);
    }
}

template <class T>
void EyeballConstraints<T>::SetupEyeballInfluenceConstraints(const Eigen::Matrix<T, 3, -1>& vertices,
                                                             const VertexWeights<T>& influenceVertexWeights,
                                                             const std::vector<T>& influenceEyeballDistances,
                                                             T eyeballWeight,
                                                             VertexConstraints<T, 1, 1>& vertexConstraints) const
{
    if (eyeballWeight <= 0) { return; }
    const int numInfluencePts = (int)influenceVertexWeights.NonzeroVertices().size();
    if (numInfluencePts == 0) { return; }

    const bool useClosestEyeballPosition = m->config["useClosestEyeballPosition"].template Value<bool>();
    const T sqrtEyeballWeight = std::sqrt(eyeballWeight);

    vertexConstraints.ResizeToFitAdditionalConstraints(numInfluencePts);

    // influence points are vertices that are affected by the eyeball
    for (int i = 0; i < numInfluencePts; ++i)
    {
        const auto [vID, weight] = influenceVertexWeights.NonzeroVerticesAndWeights()[i];
        const Eigen::Vector3<T> vertexPos = vertices.col(vID);
        const EyeballMatch match = GetClosestEyeballPosition(vertexPos, useClosestEyeballPosition);
        const Eigen::Vector3<T> targetPos = match.eyeballPosition + influenceEyeballDistances[i] * match.dir;
        const T totalWeight = sqrtEyeballWeight * weight;
        const T residual = totalWeight * match.eyeballNormal.dot(vertexPos - targetPos);
        const Eigen::Matrix<T, 1, 3> drdV = totalWeight * match.eyeballNormal.transpose();
        vertexConstraints.AddConstraint(vID, residual, drdV);
    }
}

template <class T>
Eigen::Matrix<T, 3, -1> EyeballConstraints<T>::Project(const Eigen::Matrix<T, 3, -1>& vertices) const
{
    const bool useClosestEyeballPosition = m->config["useClosestEyeballPosition"].template Value<bool>();

    Eigen::Matrix<T, 3, -1> projectedVertices = vertices;
    for (int i = 0; i < static_cast<int>(m->influenceVertexWeights.NonzeroVertices().size()); ++i)
    {
        const int vID = m->influenceVertexWeights.NonzeroVertices()[i];
        const Eigen::Vector3<T> vertexPos = vertices.col(vID);
        const EyeballMatch match = GetClosestEyeballPosition(vertexPos, useClosestEyeballPosition);
        const Eigen::Vector3<T> targetPos = match.eyeballPosition + m->influenceEyeballDistances[i] * match.dir;
        projectedVertices.col(vID) = targetPos;
    }

    // project interface vertices onto the eyeball
    for (int vID : m->interfaceVertexWeights.NonzeroVertices())
    {
        const Eigen::Vector3<T> vertexPos = vertices.col(vID);
        const EyeballMatch match = GetClosestEyeballPosition(vertexPos, useClosestEyeballPosition);
        projectedVertices.col(vID) = match.eyeballPosition;
    }

    return projectedVertices;
}

template <class T>
Eigen::Matrix<T, 3, -1> EyeballConstraints<T>::ProjectOntoEyeball(const Eigen::Matrix<T, 3, -1>& vertices) const
{
    const bool useClosestEyeballPosition = m->config["useClosestEyeballPosition"].template Value<bool>();
    Eigen::Matrix<T, 3, -1> projectedVertices = vertices;
    for (int i = 0; i < static_cast<int>(projectedVertices.cols()); ++i)
    {
        projectedVertices.col(i) = GetClosestEyeballPosition(vertices.col(i), useClosestEyeballPosition).eyeballPosition;
    }
    return projectedVertices;
}

template <class T>
Eigen::Matrix<T, 3, -1> EyeballConstraints<T>::ProjectOntoEyeball(const Eigen::Matrix<T, 3, -1>& vertices, const std::vector<int>& vIDs) const
{
    const bool useClosestEyeballPosition = m->config["useClosestEyeballPosition"].template Value<bool>();
    Eigen::Matrix<T, 3, -1> projectedVertices(3, (int)vIDs.size());
    for (int i = 0; i < static_cast<int>(vIDs.size()); ++i)
    {
        projectedVertices.col(i) = GetClosestEyeballPosition(vertices.col(vIDs[i]), useClosestEyeballPosition).eyeballPosition;
    }
    return projectedVertices;
}

// explicitly instantiate the EyeballConstraints classes
template class EyeballConstraints<float>;
template class EyeballConstraints<double>;

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
