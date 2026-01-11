// Copyright Epic Games, Inc. All Rights Reserved.

#include <nrr/RigAndControlsOptimization.h>

#include <nls/Cost.h>
#include <nls/VectorVariable.h>
#include <nls/solver/GaussNewtonSolver.h>
#include <nls/functions/PointPointConstraintFunction.h>
#include <nls/functions/SubtractFunction.h>
#include <nls/functions/GatherFunction.h>
#include <nls/functions/ScatterFunction.h>
#include <nls/geometry/Mesh.h>
#include <nls/geometry/TriangleBending.h>
#include <nls/geometry/TriangleStrain.h>
#include <nls/geometry/EulerAngles.h>
#include <rig/RigGeometry.h>
#include <rig/RigLogic.h>
#include <nls/serialization/GeometrySerialization.h>
#include <nls/serialization/MeshSerialization.h>
#include <carbon/io/JsonIO.h>


CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
struct MeshSmoothness
{
    Mesh<T> mesh;
    // evaluates strain against the rest state of the rig
    TriangleStrain<T> triangleStrainRest;
    // evaluates bending against the rest state of the rig
    TriangleBending<T> triangleBendingRest;
    // evaluates strain against the target shape
    TriangleStrain<T> triangleStrainTarget;
    // evaluate bending against the target shape
    TriangleBending<T> triangleBendingTarget;
};

template <class T>
struct RigAndControlsOptimization<T>::Private
{
    RigLogic<T> rigLogic;
    RigGeometry<T> rigGeometry;

    // PSD control indices (columns of joint matrix)
    Eigen::VectorX<int> activeControlIndices;

    std::map<std::string, Eigen::Matrix<T, 3, -1>> undeformedGeometry;
    std::map<std::string, std::shared_ptr<MeshSmoothness<T>>> meshSmoothness;

    bool initialized = false;
    bool optimizeJointMatrix = true;
    bool optimizePsdControls = true;

    Eigen::VectorX<T> jointMatrixState;
    Eigen::VectorX<T> psdControlsState;

    // joint deltas after optimization
    Eigen::VectorX<T> jointState;
    SparseMatrix<T> jointMatrixFullState;
};


template <class T>
RigAndControlsOptimization<T>::RigAndControlsOptimization() : m(new Private)
{}

template <class T> RigAndControlsOptimization<T>::~RigAndControlsOptimization() = default;
template <class T> RigAndControlsOptimization<T>::RigAndControlsOptimization(RigAndControlsOptimization&& other) = default;
template <class T> RigAndControlsOptimization<T>& RigAndControlsOptimization<T>::operator=(RigAndControlsOptimization&& other) = default;

template <class T>
void RigAndControlsOptimization<T>::SetRig(dna::BinaryStreamReader* dnaStream)
{
    m->rigGeometry.Init(dnaStream, /*withJointScaling*/ true);
    m->rigLogic.Init(dnaStream, /*withJointScaling*/ true);
    m->initialized = true;
}

template <class T>
void RigAndControlsOptimization<T>::SetMeshes(std::vector<std::string> meshNames)
{
    std::map<std::string, Mesh<T>> rigMeshes;
    for (auto name : meshNames) {
        rigMeshes[name] = m->rigGeometry.GetMesh(name);
    }

    m->meshSmoothness.clear();

    for (const auto& [geometryName, mesh] : rigMeshes)
    {
        std::shared_ptr<MeshSmoothness<T>> meshSmoothnessData = std::make_shared<MeshSmoothness<T>>();

        meshSmoothnessData->mesh = mesh;
        meshSmoothnessData->mesh.Triangulate();
        meshSmoothnessData->triangleStrainRest.SetTopology(meshSmoothnessData->mesh.Triangles());
        meshSmoothnessData->triangleBendingRest.SetTopology(meshSmoothnessData->mesh.Triangles());
        meshSmoothnessData->triangleStrainTarget.SetTopology(meshSmoothnessData->mesh.Triangles());
        meshSmoothnessData->triangleBendingTarget.SetTopology(meshSmoothnessData->mesh.Triangles());
        m->meshSmoothness[geometryName] = meshSmoothnessData;
    }
}

template <class T>
void RigAndControlsOptimization<T>::SetActiveControls(std::vector<std::string> activeControls)
{
    if (!m->initialized) {
        CARBON_CRITICAL("Must initialize using SetRig() before calling SetActiveControls()");
    }
    // TODO: instead of calculating active control indices like this, should implement rigGeometry.ReduceToGuiControls() and then call
    // that function along with rigLogic.ReduceToGuiControls()

    // Find GUI controls that correspond to activeControls by name
    Eigen::VectorX<T> guiControls(m->rigLogic.NumGUIControls());
    guiControls.setZero();
    int numControlsFound = 0;
    for (int i = 0; i < int(guiControls.size()); i++)
    {
        if (std::find(activeControls.begin(), activeControls.end(), m->rigLogic.GuiControlNames()[i]) != activeControls.end())
        {
            guiControls[i] = 1.;
            numControlsFound++;
        }
    }
    if (numControlsFound != static_cast<int>(activeControls.size())) {
        LOG_WARNING("Found {} controls but there were {} in the activeControls list", numControlsFound, activeControls.size());
    }

    DiffData<T> diffGuiControls(guiControls);
    DiffData<T> diffRawControls = m->rigLogic.EvaluateRawControls(diffGuiControls);
    DiffData<T> diffPsd = m->rigLogic.EvaluatePSD(diffRawControls);

    // Only include PSD controls (and respective columns of joint matrix) which are nonzero after evaluating from the GUI controls we care about
    std::vector<int> nonzeroPsdStd;
    for (int i = 0; i < diffPsd.Size(); i++)
    {
        if (abs(diffPsd.Value()[i]) > 1e-10)
        {
            nonzeroPsdStd.push_back(i);
        }
    }
    m->activeControlIndices = Eigen::Map<Vector<int>>(nonzeroPsdStd.data(), nonzeroPsdStd.size());
}

template <class T>
void RigAndControlsOptimization<T>::OptimizeRigAndControls(std::map<std::string, Eigen::Matrix<T, 3, -1>> targetGeometry,
                                                           Eigen::VectorX<T> guiControls,
                                                           std::map<std::string, Vector<T>> vertexWeights,
                                                           T strainWeight,
                                                           T bendingWeight,
                                                           int numIterations)
{
    if (!m->initialized)
    {
        CARBON_CRITICAL("Must initialize using SetRig() before calling OptimizeRigAndControls()");
    }

    // verify that all geometry is mapped and initialize undeformed geometry
    m->undeformedGeometry.clear();
    for (auto&& [name, _] : targetGeometry)
    {
        if (m->rigGeometry.GetMeshIndex(name) == -1)
        {
            CARBON_CRITICAL("target geometry contains {}, but it is not part of the rig", name);
        }
        else
        {
            m->undeformedGeometry[name] = m->rigGeometry.GetMesh(name).Vertices();
        }
    }

    for (auto&& [name, meshSmoothnessData] : m->meshSmoothness)
    {
        meshSmoothnessData->triangleStrainRest.SetRestPose(m->undeformedGeometry[name]);
        meshSmoothnessData->triangleBendingRest.SetRestPose(m->undeformedGeometry[name]);
        meshSmoothnessData->triangleStrainTarget.SetRestPose(targetGeometry[name]);
        meshSmoothnessData->triangleBendingTarget.SetRestPose(targetGeometry[name]);
    }

    // For now, assume rigid is identity
    DiffDataAffine<T, 3, 3> diffRigid;

    GatherFunction<T> gatherFunction;
    ScatterFunction<T> scatterFunction;

    DiffData<T> diffGuiControls(guiControls);
    DiffData<T> diffRawControls = m->rigLogic.EvaluateRawControls(diffGuiControls);
    DiffData<T> diffPsd = m->rigLogic.EvaluatePSD(diffRawControls);
    DiffData<T> diffPsdReduced = gatherFunction.Gather(diffPsd, m->activeControlIndices);
    VectorVariable<T> psdControlsVariable(diffPsdReduced.Value());
    if (!m->optimizePsdControls)
    {
        psdControlsVariable.MakeConstant();
    }

    SparseMatrix<T> jointMatrix = m->rigLogic.JointMatrix(0);
    // ColGather isn't very efficient at the moment
    SparseMatrix<T> jointMatrixReduced = ColGather(jointMatrix, m->activeControlIndices);
    Vector<T> jointMatrixReducedVec = Eigen::Map<Vector<T>>(jointMatrixReduced.valuePtr(), jointMatrixReduced.nonZeros());
    // Dense variable for non-zeros of reduced joint matrix
    VectorVariable<T> jointMatrixVariable (jointMatrixReducedVec);
    if (!m->optimizeJointMatrix)
    {
        jointMatrixVariable.MakeConstant();
    }

    std::function<DiffData<T>(Context<T>*)> evaluationFunction = [&](Context<T>* context) {
        Cost<T> cost;
        for (auto&& [name, deformedVertices] : targetGeometry)
        {
            int meshIndex = m->rigGeometry.GetMeshIndex(name);

            // Scatter dense jointMatrix variable to sparse matrix
            DiffData<T> diffJointMatrixEval = jointMatrixVariable.Evaluate(context);
            DiffDataSparseMatrix<T> diffJointMatrixReduced(std::move(diffJointMatrixEval), jointMatrixReduced);

            // EvaluateJoints using DiffData joint matrix instead of built-in
            DiffData<T> diffPsdReducedEval = psdControlsVariable.Evaluate(context);
            DiffData<T> diffPsdFull = scatterFunction.Scatter(diffPsdReducedEval, diffPsd.Size(), m->activeControlIndices);
            DiffData<T> diffJoints = m->rigLogic.EvaluateJoints(diffPsdReducedEval, diffJointMatrixReduced);
            DiffDataMatrix<T, 3, -1> calculatedDeformedVertices = std::move(m->rigGeometry.EvaluateRigGeometry(diffRigid, diffJoints, diffPsdFull, { meshIndex }).MoveVertices().front());

            auto itMap = vertexWeights.find(name);
            if (itMap == vertexWeights.end())
            {
                LOG_WARNING("No vertexWeights for geometry {}", name);
                continue;
            }
            const auto vertexWeightsForGeo = itMap->second;
            if (vertexWeightsForGeo.size() != calculatedDeformedVertices.Cols())
            {
                CARBON_CRITICAL("vertex weight map size does not match number of vertices");
            }
            {
                DiffData<T> residual = PointPointConstraintFunction<T, 3>::Evaluate(calculatedDeformedVertices,
                                                                                    deformedVertices,
                                                                                    vertexWeightsForGeo,
                                                                                    T(1));
                cost.Add(std::move(residual), T(1));
            }

            auto it = m->meshSmoothness.find(name);
            if (it != m->meshSmoothness.end())
            {
                std::shared_ptr<MeshSmoothness<T>> meshSmoothnessData = it->second;
                T projectiveStrainRest = T(0);
                T greenStrainRest = T(0);
                T quadraticBendingRest = T(0);
                T dihedralBendingRest = T(0);

                T projectiveStrainTarget = T(0);
                T greenStrainTarget = strainWeight;
                T quadraticBendingTarget = T(0);
                T dihedralBendingTarget = bendingWeight;

                if (projectiveStrainRest > 0)
                {
                    cost.Add(meshSmoothnessData->triangleStrainRest.EvaluateProjectiveStrain(calculatedDeformedVertices,
                                                                                             projectiveStrainRest), T(1));
                }
                if (greenStrainRest > 0)
                {
                    cost.Add(meshSmoothnessData->triangleStrainRest.EvaluateGreenStrain(calculatedDeformedVertices,
                                                                                        greenStrainRest), T(1));
                }
                if (quadraticBendingRest > 0)
                {
                    cost.Add(meshSmoothnessData->triangleBendingRest.EvaluateQuadratic(calculatedDeformedVertices,
                                                                                       quadraticBendingRest), T(1));
                }
                if (dihedralBendingRest > 0)
                {
                    cost.Add(meshSmoothnessData->triangleBendingRest.EvaluateDihedral(calculatedDeformedVertices,
                                                                                      dihedralBendingRest), T(1));
                }

                if (projectiveStrainTarget > 0)
                {
                    cost.Add(meshSmoothnessData->triangleStrainTarget.EvaluateProjectiveStrain(calculatedDeformedVertices,
                                                                                               projectiveStrainTarget), T(1));
                }
                if (greenStrainTarget > 0)
                {
                    cost.Add(meshSmoothnessData->triangleStrainTarget.EvaluateGreenStrain(calculatedDeformedVertices,
                                                                                          greenStrainTarget), T(1));
                }
                if (quadraticBendingTarget > 0)
                {
                    cost.Add(meshSmoothnessData->triangleBendingTarget.EvaluateQuadratic(calculatedDeformedVertices,
                                                                                         quadraticBendingTarget), T(1));
                }
                if (dihedralBendingTarget > 0)
                {
                    cost.Add(meshSmoothnessData->triangleBendingTarget.EvaluateDihedral(calculatedDeformedVertices,
                                                                                        dihedralBendingTarget), T(1));
                }
            }
        }
        return cost.CostToDiffData();
    };

    GaussNewtonSolver<T> solver;
    const T startEnergy = evaluationFunction(nullptr).Value().norm();
    if (!solver.Solve(evaluationFunction, numIterations))
    {
        printf("could not solve optimization problem\n");
    }
    const T finalEnergy = evaluationFunction(nullptr).Value().norm();
    printf("energy reduced from %f to %f\n", double(startEnergy), double(finalEnergy));

    // Collect results of optimized variables
    Context<T> context;
    DiffData<T> diffJointMatrixEval = jointMatrixVariable.Evaluate(&context);
    DiffDataSparseMatrix<T> diffJointMatrixReduced(std::move(diffJointMatrixEval), jointMatrixReduced);
    DiffData<T> diffPsdReducedEval = psdControlsVariable.Evaluate(&context);
    DiffData<T> diffJoints = m->rigLogic.EvaluateJoints(diffPsdReducedEval, diffJointMatrixReduced);

    // Store results by scattering back to full form
    m->jointState = diffJoints.Value();
    m->jointMatrixState = diffJointMatrixReduced.Value();

    // Scattering the jointMatrix is more involved because it is row-major and sparse and we need to combine multiple matrices
    // Perhaps put some of this functionality in Math.h?
    SparseMatrix<T> jointMatrixT = jointMatrix.transpose();
    SparseMatrix<T> jointMatrixReducedT = diffJointMatrixReduced.Matrix().transpose();

    std::vector<Eigen::Triplet<T>> triplets;
    triplets.reserve(jointMatrixT.nonZeros());
    int reducedIdx = 0;
    int totalNonzeros = 0;
    for (int k = 0; k < jointMatrixT.outerSize(); ++k) // row: psd index
    {
        if (k == m->activeControlIndices[reducedIdx]) {
            for (typename SparseMatrix<T>::InnerIterator it(jointMatrixReducedT, reducedIdx); it; ++it) // column: joint index
            {
                triplets.emplace_back(k, static_cast<int>(it.col()), it.value());
                totalNonzeros++;
            }
            reducedIdx++;
        }
        else
        {
            for (typename SparseMatrix<T>::InnerIterator it(jointMatrixT, k); it; ++it)
            {
                triplets.emplace_back(static_cast<int>(it.row()), static_cast<int>(it.col()), it.value());
                totalNonzeros++;
            }
        }

    }
    if (reducedIdx != m->activeControlIndices.size() || totalNonzeros != jointMatrix.nonZeros())
    {
        CARBON_CRITICAL("Error in scattering jointMatrix to full size {}={}, {}={}", reducedIdx, m->activeControlIndices.size(), totalNonzeros, jointMatrix.nonZeros());
    }
    SparseMatrix<T> jointMatrixNewT(jointMatrixT.rows(), jointMatrixT.cols());
    jointMatrixNewT.setFromTriplets(triplets.begin(), triplets.end());
    m->jointMatrixFullState = jointMatrixNewT.transpose();

    // Scatter psd controls optimization results as well
    DiffData<T> diffPsdNew = scatterFunction.Scatter(diffPsdReducedEval, diffPsd.Size(), m->activeControlIndices);
    m->psdControlsState = diffPsdNew.Value();

    return;
}

template<class T>
Eigen::VectorX<T> RigAndControlsOptimization<T>::GetControls()
{
    return m->psdControlsState;
}

template<class T>
Eigen::VectorX<T> RigAndControlsOptimization<T>::GetJointMatrixNonzeros()
{
    return m->jointMatrixState;
}

template<class T>
Eigen::VectorX<T> RigAndControlsOptimization<T>::GetJointState()
{
    return m->jointState;
}

template <class T>
SparseMatrix<T> RigAndControlsOptimization<T>::GetJointMatrixState()
{
    return m->jointMatrixFullState;
}

//! Sets the degrees of freedom: either optimize controls, joint matrix nonzeros, or both
template<class T>
void RigAndControlsOptimization<T>::SetDegreesOfFreedom(bool jointMatrix, bool controls) {
    m->optimizeJointMatrix = jointMatrix;
    m->optimizePsdControls = controls;
}

/**
 * @brief Setup the rig optimization model
 *
 * @param[in] dnaResource        - DNA resource that is used to setup the joint optimization
 * @param[in] activeControls     - GUI controls to optimize for
 */
template <class T>
void RigAndControlsFitting<T>::Setup(dna::BinaryStreamReader* dnaResource,
                                     std::vector<std::string> activeControls)
{
    m_pDNAResource = dnaResource;

    // Create new to ensure reset
    m_pRigOptimization = std::make_unique<RigAndControlsOptimization<T>>();

    // Init the joint optimization rig
    m_pRigOptimization->SetRig(m_pDNAResource);

    // Set the meshes to be optimized
    // We use only the head mesh on LOD0
    std::vector<std::string> meshNames { "head_lod0_mesh" };
    m_pRigOptimization->SetMeshes(meshNames);

    // Only these controls (and their corresponding PSD columns of joint matrix) can be optimized over
    m_pRigOptimization->SetActiveControls(activeControls);
}

/**
 * @brief Optimize the joints so that the resulting mesh is the closes (w.r.t. given constrints) to the target geometry
 *
 * @note Only the translation and rotaiton of the joints can be optimized, even if there are more degrees of freedom.
 * Deltas for DOFs that are not optimized will be set to zero
 *
 * @param[in] fittingMask       - Mask that selects the vertices that can be moved by the optimization
 * @param[in] params            - Joints optimization parameters
 * @param[in] dofPerJoint       - Number of degrees of freedom for joints. Used to navigate in the output array
 *
 * @return Joint deltas compared to current state of the joints and joint matrix
 */
template <class T>
SparseMatrix<T> RigAndControlsFitting<T>::Optimize(const Eigen::Matrix<T, 3, -1>& targetGeometry,
                                                   const VertexWeights<T>& fittingMask,
                                                   const Eigen::VectorX<T> guiControls,
                                                   const RigAndControlsFittingParams& params)
{
    // Pack the target to a map as this is given to the optimization function
    std::map<std::string, Eigen::Matrix3Xf> target;
    target["head_lod0_mesh"] = targetGeometry;

    std::map<std::string, Eigen::VectorXf> vertexWeights;
    vertexWeights["head_lod0_mesh"] = fittingMask.Weights();

    // Set the optimization parameters and run it
    m_pRigOptimization->SetDegreesOfFreedom(
        /*bool jointMatrix = */!params.m_fixJointMatrix,
        /*bool controls = */!params.m_fixPsdControls);


    m_pRigOptimization->OptimizeRigAndControls(target,
                                                 guiControls,
                                                 vertexWeights,
                                                 params.m_strainWeight,
                                                 params.m_bendingWeight,
                                                 params.m_numIterations);

    // Output is in a form of the updated joint matrix
    return m_pRigOptimization->GetJointMatrixState();
}

// explicitly instantiate the joint rig optimization classes
template class RigAndControlsOptimization<float>;
template class RigAndControlsOptimization<double>;
template class RigAndControlsFitting<float>;

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
