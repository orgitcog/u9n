// Copyright Epic Games, Inc. All Rights Reserved.

#include <carbon/Common.h>
#include <carbon/geometry/AABBTree.h>
#include <carbon/geometry/KdTree.h>
#include <carbon/io/Utils.h>
#include <nls/Context.h>
#include <nls/Cost.h>
#include <nls/MatrixVariable.h>
#include <nls/VertexOptimization.h>
#include <nls/solver/GaussNewtonSolver.h>
#include <nls/functions/BarycentricCoordinatesFunction.h>
#include <nls/functions/PointSurfaceConstraintFunction.h>
#include <nls/functions/GatherFunction.h>
#include <nls/functions/SubtractFunction.h>
#include <nls/geometry/BarycentricEmbedding.h>
#include <nls/geometry/Mesh.h>
#include <nls/geometry/MeshCorrespondenceSearch.h>
#include <nls/geometry/TetMesh.h>
#include <nls/geometry/TetConstraints.h>
#include <nls/geometry/VertexLaplacian.h>
#include <nls/math/Math.h>
#include <nls/serialization/ObjFileFormat.h>
#include <nrr/EyeballConstraints.h>
#include <nrr/GridDeformation.h>
#include <nrr/TemplateDescription.h>
#include <nrr/VertexWeights.h>
#include <nrr/deformation_models/DeformationModelVertex.h>
#include <nrr/volumetric/VolumetricDeformation.h>
#include <nrr/volumetric/VolumetricFaceModel.h>

#include <string>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
VolumetricFaceModel<T> VolumetricDeformation<T>::MorphVolumetricFaceModelUsingGrid(const VolumetricFaceModel<T>& inputVolModel,
                                                                                   Eigen::Ref<const Eigen::Matrix<T, 3,
                                                                                                                  -1>> srcVertices,
                                                                                   Eigen::Ref<const Eigen::Matrix<T, 3,
                                                                                                                  -1>> targetVertices,
                                                                                   int gridSize)
{
    GridDeformation<T> gridDeformation(gridSize, gridSize, gridSize);
    gridDeformation.Init(srcVertices);
    gridDeformation.Solve(srcVertices, targetVertices, 1.0, 100);

    auto deformMesh = [&](const Eigen::Matrix<T, 3, -1>& vertices)
        {
            Eigen::Matrix<T, 3, -1> output = vertices;
            for (int i = 0; i < (int)output.cols(); ++i)
            {
                output.col(i) += gridDeformation.EvaluateGridPosition(vertices.col(i));
            }
            return output;
        };

    VolumetricFaceModel<T> volModel = inputVolModel;
    volModel.SetSkinMeshVertices(deformMesh(volModel.GetSkinMesh().Vertices()));
    volModel.SetTeethMeshVertices(deformMesh(volModel.GetTeethMesh().Vertices()));
    volModel.SetFleshMeshVertices(deformMesh(volModel.GetFleshMesh().Vertices()));
    volModel.SetCraniumMeshVertices(deformMesh(volModel.GetCraniumMesh().Vertices()));
    volModel.SetMandibleMeshVertices(deformMesh(volModel.GetMandibleMesh().Vertices()));
    volModel.SetTetMeshVertices(deformMesh(volModel.GetTetMesh().Vertices()));
    return volModel;
}

/**
 * Deforms the flesh mesh vertices to coincide with the skin mesh (based on the mapping) and
 * also ensures that the flesh mesh conforms to the input eyes. Note that there
 * is not a correspondence between eyeballs and the flesh mesh, in particular the skin mesh
 * can slide over the eyeballs, and hence using a fixed correspondence would lead to artifacts.
 */
template <class T>
Eigen::Matrix<T, 3, -1> VolumetricDeformation<T>::DeformFleshMeshUsingSkinAndEyes(const VolumetricFaceModel<T>& inputVolModel,
                                                                                  const TemplateDescription& templateDescription,
                                                                                  const Mesh<T>& newSkinMesh,
                                                                                  const Mesh<T>& newEyeLeftMesh,
                                                                                  const Mesh<T>& newEyeRightMesh)
{
    const VertexWeights<T> interfaceVertexWeightsEyeLeft = templateDescription.GetAssetVertexWeights("flesh",
                                                                                                     "eyeball_interface_left").
        template Cast<T>();
    const VertexWeights<T> interfaceVertexWeightsEyeRight = templateDescription.GetAssetVertexWeights("flesh",
                                                                                                      "eyeball_interface_right").
        template Cast<T>();

    Mesh<T> referenceFleshMesh = inputVolModel.GetFleshMesh();
    VertexOptimization<T> problem;
    problem.SetTopology(referenceFleshMesh);
    Eigen::Matrix<T, 3, -1> fleshOffsets = Eigen::Matrix<T, 3, -1>::Zero(3, referenceFleshMesh.NumVertices());
    std::vector<int> fixedIndices;
    for (const auto& [skinIndex, fleshIndex] : inputVolModel.SkinFleshMapping())
    {
        fixedIndices.push_back(fleshIndex);
        fleshOffsets.col(fleshIndex) = newSkinMesh.Vertices().col(skinIndex) - referenceFleshMesh.Vertices().col(fleshIndex);
    }
    EyeballConstraints<T> eyeballConstraintsLeft;
    eyeballConstraintsLeft.SetEyeballMesh(newEyeLeftMesh);
    EyeballConstraints<T> eyeballConstraintsRight;
    eyeballConstraintsRight.SetEyeballMesh(newEyeRightMesh);
    VertexConstraints<T, 1, 1> eyeballVertexConstraints;

    const int iterations = 5;
    const int cgIterations = 250;
    const T eyeballWeight = 1.0;
    for (int iter = 0; iter < iterations; ++iter)
    {
        problem.Clear(fixedIndices);
        problem.Setup(fleshOffsets,
                      /*laplacianRegulariation=*/T(1.0),
                      /*offsetRegularization=*/0,
                      /*updateRegularization=*/0);
        if (iter > 0)
        {
            // only apply eyeball constraints from the second iteration as the vertices may be widely off
            // in the first iteration
            const Eigen::Matrix<T, 3, -1> vertices = referenceFleshMesh.Vertices() + fleshOffsets;
            eyeballVertexConstraints.Clear();
            eyeballConstraintsLeft.SetupEyeballInterfaceConstraints(vertices,
                                                                    interfaceVertexWeightsEyeLeft,
                                                                    eyeballWeight,
                                                                    eyeballVertexConstraints);
            eyeballConstraintsRight.SetupEyeballInterfaceConstraints(vertices,
                                                                     interfaceVertexWeightsEyeRight,
                                                                     eyeballWeight,
                                                                     eyeballVertexConstraints);
            problem.AddConstraints(eyeballVertexConstraints);
        }
        fleshOffsets += problem.Solve(cgIterations);
    }

    return referenceFleshMesh.Vertices() + fleshOffsets;
}

/**
 * Morphs the volumetric model using a grid deformation model. As boundary counstraints the grid deformation uses
 * the skin mesh @p newSkinMesh, the teeth mesh @p newTeethMesh, as well as the eye meshes.
 * @param[in] inputVolModel  The input volumetric model that is morphed.
 * @param[in] newSkinMesh    The skin mesh that acts as target for the volumetric morph.
 * @param[in] newTeethMesh   The teeth mesh that acts as target for the volumetric morph.
 * @returns the morphed volumetric model were each mesh has been morphed, and skin and possible teeth mesh are replaced by the input meshes.
 * @warning the method does *NOT* update the tet mesh.
 */
template <class T>
VolumetricFaceModel<T> VolumetricDeformation<T>::MorphVolumetricFaceModelUsingGrid(const VolumetricFaceModel<T>& inputVolModel,
                                                                                   const TemplateDescription& templateDescription,
                                                                                   const Mesh<T>& newSkinMesh,
                                                                                   const Mesh<T>& newTeethMesh,
                                                                                   const Mesh<T>& newEyeLeftMesh,
                                                                                   const Mesh<T>& newEyeRightMesh,
                                                                                   int gridSize)
{
    if (inputVolModel.GetSkinMesh().NumVertices() != newSkinMesh.NumVertices())
    {
        CARBON_CRITICAL("skin mesh does not match skin mesh of volumetric face model");
    }

    std::vector<Eigen::Vector3<T>> srcVertices;
    std::vector<Eigen::Vector3<T>> targetVertices;
    // use skin vertices
    for (const auto& [skinIndex, _] : inputVolModel.SkinFleshMapping())
    {
        srcVertices.push_back(inputVolModel.GetSkinMesh().Vertices().col(skinIndex));
        targetVertices.push_back(newSkinMesh.Vertices().col(skinIndex));
    }
    // use teeth vertices
    for (int k = 0; k < inputVolModel.GetTeethMesh().NumVertices(); ++k)
    {
        srcVertices.push_back(inputVolModel.GetTeethMesh().Vertices().col(k));
        targetVertices.push_back(newTeethMesh.Vertices().col(k));
    }
    // deform the flesh vertices to the skin and eyes and use the resulting fitted eye vertices for the grid deformation
    const Eigen::Matrix<T, 3, -1> deformedFleshVertices = DeformFleshMeshUsingSkinAndEyes(inputVolModel,
                                                                                          templateDescription,
                                                                                          newSkinMesh,
                                                                                          newEyeLeftMesh,
                                                                                          newEyeRightMesh);
    const VertexWeights<float> interfaceVertexWeightsEyeLeft = templateDescription.GetAssetVertexWeights("flesh",
                                                                                                         "eyeball_interface_left");
    const VertexWeights<float> interfaceVertexWeightsEyeRight = templateDescription.GetAssetVertexWeights("flesh",
                                                                                                          "eyeball_interface_right");
    for (int vID : interfaceVertexWeightsEyeLeft.NonzeroVertices())
    {
        srcVertices.push_back(inputVolModel.GetFleshMesh().Vertices().col(vID));
        targetVertices.push_back(deformedFleshVertices.col(vID));
    }
    for (int vID : interfaceVertexWeightsEyeRight.NonzeroVertices())
    {
        srcVertices.push_back(inputVolModel.GetFleshMesh().Vertices().col(vID));
        targetVertices.push_back(deformedFleshVertices.col(vID));
    }

    VolumetricFaceModel<T> volModel = MorphVolumetricFaceModelUsingGrid(
        inputVolModel,
        Eigen::Map<const Eigen::Matrix<T, 3, -1>>(&srcVertices[0][0], 3, srcVertices.size()),
        Eigen::Map<const Eigen::Matrix<T, 3, -1>>(&targetVertices[0][0], 3, targetVertices.size()),
        gridSize);
    volModel.SetFleshMeshVertices(deformedFleshVertices);
    volModel.SetSkinMeshVertices(newSkinMesh.Vertices());
    volModel.SetTeethMeshVertices(newTeethMesh.Vertices());

    return volModel;
}

/**
 * Deforms the flesh mesh using the cranium, mandible, skin, and eyes as boundary conditions. This
 * method only deforms the flesh mesh and the tet mesh is not modified. Smoothness of the flesh mesh is
 * based on the @p referenceFleshMesh. The vertices of the flesh mesh that coincide with vertices
 * of cranium, mandible, and skin are directly set to the corresponding vertices of these meshes
 * to have perfect alignment.
 * This method is intended to be used for identities where there is no knowledge on the volume of the flesh.
 * To optimize the mesh of a known identity see @DeformTetsAndFleshMeshUsingCraniumMandibleAndSkinAsBoundaryConditions.
 * @param[in] inputVolModel  The input volumetric model for which the flesh mesh is modified.
 * @param[in] referenceFleshMesh   The reference flesh mesh that is used for smoothness constraints.
 * @returns the volumetric model with the flesh mesh being smoothly deformed.
 * @warning the method does *NOT* update the tet mesh. @see UpdateTetsUsingFleshMesh().
 */
template <class T>
VolumetricFaceModel<T> VolumetricDeformation<T>::DeformFleshMeshUsingCraniumMandibleSkinAndEyesAsBoundaryConditions(const VolumetricFaceModel<T>& inputVolModel,
                                                                                                                    const Mesh<T>& referenceFleshMesh,
                                                                                                                    const TemplateDescription& templateDescription,
                                                                                                                    const Mesh<T>& eyeLeftMesh,
                                                                                                                    const Mesh<T>& eyeRightMesh)
{
    if (inputVolModel.GetFleshMesh().NumVertices() != referenceFleshMesh.NumVertices())
    {
        CARBON_CRITICAL("number of vertices in flesh mesh do not match");
    }

    std::vector<int> fleshVertexIndices;
    std::vector<Eigen::Vector3<T>> initPositions;
    std::vector<Eigen::Vector3<T>> targetPositions;
    for (const auto& [skinIndex, fleshIndex] : inputVolModel.SkinFleshMapping())
    {
        targetPositions.push_back(inputVolModel.GetSkinMesh().Vertices().col(skinIndex));
        initPositions.push_back(referenceFleshMesh.Vertices().col(fleshIndex));
        fleshVertexIndices.push_back(fleshIndex);
    }
    for (const auto& [craniumIndex, fleshIndex] : inputVolModel.CraniumFleshMapping())
    {
        targetPositions.push_back(inputVolModel.GetCraniumMesh().Vertices().col(craniumIndex));
        initPositions.push_back(referenceFleshMesh.Vertices().col(fleshIndex));
        fleshVertexIndices.push_back(fleshIndex);
    }
    for (const auto& [mandibleIndex, fleshIndex] : inputVolModel.MandibleFleshMapping())
    {
        targetPositions.push_back(inputVolModel.GetMandibleMesh().Vertices().col(mandibleIndex));
        initPositions.push_back(referenceFleshMesh.Vertices().col(fleshIndex));
        fleshVertexIndices.push_back(fleshIndex);
    }
    const Eigen::Vector3<T> targetMeanPos = Eigen::Map<const Eigen::Matrix<T, 3, -1>>((const T*)targetPositions.data(),
                                                                                      3,
                                                                                      (int)targetPositions.size()).rowwise().mean();
    const Eigen::Vector3<T> initMeanPose = Eigen::Map<const Eigen::Matrix<T, 3, -1>>((const T*)initPositions.data(),
                                                                                     3,
                                                                                     (int)initPositions.size()).rowwise().mean();

    Eigen::Matrix<T, 3, -1> fleshOffsets = Eigen::Matrix<T, 3, -1>::Zero(3, referenceFleshMesh.NumVertices());
    fleshOffsets.colwise() += targetMeanPos - initMeanPose;
    for (int i = 0; i < (int)fleshVertexIndices.size(); ++i)
    {
        fleshOffsets.col(fleshVertexIndices[i]) = targetPositions[i] - initPositions[i];
    }

    const VertexWeights<T> interfaceVertexWeightsEyeLeft = templateDescription.GetAssetVertexWeights("flesh",
                                                                                                     "eyeball_interface_left").
        template Cast<T>();
    const VertexWeights<T> interfaceVertexWeightsEyeRight = templateDescription.GetAssetVertexWeights("flesh",
                                                                                                      "eyeball_interface_right").
        template Cast<T>();

    EyeballConstraints<T> eyeballConstraintsLeft;
    eyeballConstraintsLeft.SetEyeballMesh(eyeLeftMesh);
    EyeballConstraints<T> eyeballConstraintsRight;
    eyeballConstraintsRight.SetEyeballMesh(eyeRightMesh);
    VertexConstraints<T, 1, 1> eyeballVertexConstraints;

    const int iterations = 5;
    const int cgIterations = 250;
    const T eyeballWeight = 1.0;
    VertexOptimization<T> problem;
    problem.SetTopology(referenceFleshMesh);

    for (int iter = 0; iter < iterations; ++iter)
    {
        problem.Clear(fleshVertexIndices);
        problem.Setup(fleshOffsets,
                      /*laplacianRegulariation=*/T(1.0),
                      /*offsetRegularization=*/0,
                      /*updateRegularization=*/0);
        if (iter > 0)
        {
            const Eigen::Matrix<T, 3, -1> vertices = referenceFleshMesh.Vertices() + fleshOffsets;
            eyeballVertexConstraints.Clear();
            eyeballConstraintsLeft.SetupEyeballInterfaceConstraints(vertices,
                                                                    interfaceVertexWeightsEyeLeft,
                                                                    eyeballWeight,
                                                                    eyeballVertexConstraints);
            eyeballConstraintsRight.SetupEyeballInterfaceConstraints(vertices,
                                                                     interfaceVertexWeightsEyeRight,
                                                                     eyeballWeight,
                                                                     eyeballVertexConstraints);
            problem.AddConstraints(eyeballVertexConstraints);
        }
        fleshOffsets += problem.Solve(cgIterations);
    }

    VolumetricFaceModel<T> volModel = inputVolModel;
    volModel.SetFleshMeshVertices(referenceFleshMesh.Vertices() + fleshOffsets);

    return volModel;
}

//! Class supporting tet optimization
template <class T>
struct TetOptimization
{
    using ScalarType = T;

    int numVertices;
    Eigen::VectorX<T> Jtb;

    std::vector<Eigen::Matrix<T, 3, 3>> diagonalBlocks;
    std::vector<bool> validDiagonal;
    Eigen::SparseMatrix<T, Eigen::RowMajor> smat;
    std::vector<Eigen::Triplet<T>> triplets;

    std::shared_ptr<TaskThreadPool> threadPool;

    int NumUnknowns() const
    {
        return (int)Jtb.size();
    }

    int NumVertices() const
    {
        return numVertices;
    }

    void Init(int n)
    {
        numVertices = n;
        Jtb = Eigen::VectorX<T>::Zero(numVertices * 3);
        diagonalBlocks.resize(numVertices);
        validDiagonal = std::vector<bool>(numVertices, false);
        threadPool = TaskThreadPool::GlobalInstance(/*createIfNotAvailable=*/false);
    }

    void Clear()
    {
        Jtb.setZero();
        std::fill(validDiagonal.begin(), validDiagonal.end(), false);
        triplets.clear();
    }

    template <int ResidualSize, int NumConstraintVertices>
    void AddConstraints(const VertexConstraintsExt<T, ResidualSize, NumConstraintVertices>& vertexConstraints)
    {
        for (int i = 0; i < vertexConstraints.NumberOfConstraints(); ++i)
        {
            const auto& vIDs = vertexConstraints.VertexIDs()[i];
            const auto& residual = vertexConstraints.Residuals()[i];
            const auto& jac = vertexConstraints.Jacobians()[i];
            for (int e = 0; e < NumConstraintVertices; ++e)
            {
                const int vID = vIDs[e];
                if (validDiagonal[vID])
                {
                    diagonalBlocks[vID] +=
                        jac.block(0, 3 * e, ResidualSize, 3).transpose() * jac.block(0, 3 * e, ResidualSize, 3);
                }
                else
                {
                    diagonalBlocks[vID] = jac.block(0, 3 * e, ResidualSize, 3).transpose() * jac.block(0, 3 * e, ResidualSize, 3);
                    validDiagonal[vID] = true;
                }
                Jtb.segment(3 * vID, 3) += -jac.block(0, 3 * e, ResidualSize, 3).transpose() * residual;
            }

            for (int e1 = 0; e1 < NumConstraintVertices; ++e1)
            {
                for (int e2 = e1 + 1; e2 < NumConstraintVertices; ++e2)
                {
                    const Eigen::Matrix<T, 3, 3> jtj = jac.block(0, 3 * e1, ResidualSize, 3).transpose() * jac.block(0,
                                                                                                                     3 * e2,
                                                                                                                     ResidualSize,
                                                                                                                     3);
                    for (int k1 = 0; k1 < 3; ++k1)
                    {
                        for (int k2 = 0; k2 < 3; ++k2)
                        {
                            triplets.push_back(Eigen::Triplet<T>(3 * vIDs[e1] + k1, 3 * vIDs[e2] + k2, jtj(k1, k2)));
                        }
                    }
                    for (int k1 = 0; k1 < 3; ++k1)
                    {
                        for (int k2 = 0; k2 < 3; ++k2)
                        {
                            triplets.push_back(Eigen::Triplet<T>(3 * vIDs[e2] + k1, 3 * vIDs[e1] + k2, jtj(k2, k1)));
                        }
                    }
                }
            }
        }
    }

    void FinalizeConstraints()
    {
        if (triplets.size() > 0)
        {
            smat.resize(NumUnknowns(), NumUnknowns());
            smat.setFromTriplets(triplets.begin(), triplets.end());
            triplets.clear();
        }
    }

    const Eigen::VectorX<T>& Rhs() const
    {
        return Jtb;
    }

    Eigen::VectorX<T> DiagonalPreconditioner() const
    {
        Eigen::VectorX<T> diag = Eigen::VectorX<T>::Zero(NumUnknowns());

        for (int i = 0; i < NumVertices(); ++i)
        {
            if (validDiagonal[i])
            {
                diag.segment(3 * i, 3) += diagonalBlocks[i].diagonal();
            }
        }

        for (int i = 0; i < (int)diag.size(); ++i)
        {
            if (diag[i] != 0)
            {
                diag[i] = T(1) / diag[i];
            }
        }
        return diag;
    }

    int NumSegments() const
    {
        return 2;
    }

    void MatrixMultiply(Eigen::Ref<Eigen::VectorX<T>> out, int segmentId, Eigen::Ref<const Eigen::VectorX<T>> x) const
    {
        if (segmentId == 0)
        {
            out.setZero();
            for (int i = 0; i < NumVertices(); ++i)
            {
                if (validDiagonal[i])
                {
                    out.segment(3 * i, 3) += diagonalBlocks[i] * x.segment(3 * i, 3);
                }
            }
        }
        else if (segmentId == 1)
        {
            ParallelNoAliasGEMV<T>(out, smat, x, threadPool.get());
        }
    }

    Eigen::Matrix<T, 3, -1> Solve(int cgIterations) const
    {
        ParallelPCG<TetOptimization<T>> solver(threadPool);
        const Eigen::VectorX<T> dx = solver.Solve(cgIterations, *this);
        // all vertices are solved, so we can simply return the result
        return Eigen::Map<const Eigen::Matrix<T, 3, -1>>(dx.data(), 3, dx.size() / 3);
    }
};

/**
 * Deform the tets based on the flesh mesh.
 * @param[inout] volModel  The volumetric face model or which the tests are updated.
 * @param[in] referenceVolModel  The reference volumetric model that defines the rest state of the tets
 */
template <class T>
void VolumetricDeformation<T>::UpdateTetsUsingFleshMesh(VolumetricFaceModel<T>& volModel, const VolumetricFaceModel<T>& referenceVolModel)
{
    const int iterations = 10;

    // MatrixVariable<T, 3, -1> deformedTetVerticesVariable(3, volModel.GetTetMesh().NumVertices());
    // deformedTetVerticesVariable.SetMatrix(volModel.GetTetMesh().Vertices());

    // std::vector<BarycentricCoordinates<T, 4> > barycentricCoordinates = volModel.Embedding().GetBarycentricCoordinates();

    // TetConstraints<T> tetConstraints;
    // tetConstraints.SetTopology(referenceVolModel.GetTetMesh().Tets());
    // tetConstraints.SetRestPose(referenceVolModel.GetTetMesh().Vertices(), /*allowInvertedTets=*/ true);

    // const T strainWeight = 1.0;
    // const Eigen::Matrix<T, 3, -1>& targetVertices = volModel.GetFleshMesh().Vertices();
    // DiffData<T> diffTargetVertices(targetVertices);

    // std::function<DiffData<T>(Context<T>* context)> evaluationFunction = [&](Context<T>* context) -> DiffData<T> {
    // Cost<T> cost;

    // Timer timer;
    // DiffDataMatrix<T, 3, -1> deformedTetVertices = deformedTetVerticesVariable.EvaluateMatrix(context);
    // LOG_INFO("eval ({}): {}", context?"jac":"", timer.Current()); timer.Restart();
    // DiffDataMatrix<T, 3, -1> deformedSurfaceVertices = BarycentricCoordinatesFunction<T, 3, 4>::Evaluate(
    // deformedTetVertices,
    // barycentricCoordinates);
    // LOG_INFO("bc ({}): {}", context?"jac":"", timer.Current()); timer.Restart();
    // cost.Add(deformedSurfaceVertices - diffTargetVertices, T(1));
    // LOG_INFO("diff ({}): {}", context?"jac":"", timer.Current()); timer.Restart();
    // cost.Add(tetConstraints.EvaluateStrain(deformedTetVertices, strainWeight), T(1));
    // LOG_INFO("tets ({}): {}", context?"jac":"", timer.Current()); timer.Restart();

    // auto res = cost.CostToDiffData();
    // return res;
    // };

    // T minVol, avgVol, maxVol;
    // volModel.GetTetMesh().TetVolumeStatistics(minVol, avgVol, maxVol, false);
    // LOG_INFO("pre volume statistics: {} {} {}", minVol, avgVol, maxVol);

    // GaussNewtonSolver<T> solver;
    // Context<T> context;
    // typename GaussNewtonSolver<T>::Settings settings;
    // settings.iterations = iterations;
    // settings.reg = 0;
    // settings.maxLineSearchIterations = 0;
    // settings.cgIterations = 200;
    // LOG_INFO("start energy: {}", evaluationFunction(nullptr).Value().squaredNorm());
    // if (!solver.Solve(evaluationFunction, context, settings)) {
    // CARBON_CRITICAL("could not solve optimization problem\n");
    // }
    // LOG_INFO("final energy: {}", evaluationFunction(nullptr).Value().squaredNorm());

    // volModel.SetTetMeshVertices(deformedTetVerticesVariable.Matrix());

    // volModel.GetTetMesh().TetVolumeStatistics(minVol, avgVol, maxVol, false);
    // LOG_INFO("post volume statistics: {} {} {}", minVol, avgVol, maxVol);

    T minVol, avgVol, maxVol;
    volModel.GetTetMesh().TetVolumeStatistics(minVol, avgVol, maxVol, false);
    LOG_INFO("pre volume statistics: {} {} {}", minVol, avgVol, maxVol);

    TetConstraints<T> tetConstraints;
    tetConstraints.SetTopology(referenceVolModel.GetTetMesh().Tets());
    tetConstraints.SetRestPose(referenceVolModel.GetTetMesh().Vertices(), /*allowInvertedTets=*/true);


    const int cgIterations = 500;
    const T strainWeight = 1.0;
    const Eigen::Matrix<T, 3, -1>& targetVertices = volModel.GetFleshMesh().Vertices();

    // std::vector<BarycentricCoordinates<T, 4> > barycentricCoordinates = volModel.Embedding().GetBarycentricCoordinates();
    // VertexConstraintsExt<T, 3, 4> vertexTargets;
    VertexConstraintsExt<T, 3, 1> vertexTargets;

    VertexConstraintsExt<T, 9, 4> tetStrain;

    Eigen::Matrix<T, 3, -1> deformedTetVertices = volModel.GetTetMesh().Vertices();

    TetOptimization<T> tetOpt;
    tetOpt.Init(referenceVolModel.GetTetMesh().NumVertices());

    if (volModel.TetFleshMapping().empty())
    {
        CARBON_CRITICAL("missing tet to flesh mapping");
    }

    for (int iter = 0; iter < iterations; ++iter)
    {
        tetOpt.Clear();
        vertexTargets.Clear();
        vertexTargets.ResizeToFitAdditionalConstraints((int)volModel.TetFleshMapping().size());
        for (size_t i = 0; i < volModel.TetFleshMapping().size(); ++i)
        {
            const int vID = volModel.TetFleshMapping()[i].first;
            const BarycentricCoordinates<T>& bc = volModel.TetFleshMapping()[i].second;
            const Eigen::Vector3<T> target = bc.template Evaluate<3>(targetVertices);
            vertexTargets.AddConstraint(vID, deformedTetVertices.col(vID) - target, Eigen::Matrix<T, 3, 3>::Identity());
        }

        tetStrain.Clear();
        tetConstraints.SetupStrain(deformedTetVertices, strainWeight, tetStrain);
        tetOpt.AddConstraints(vertexTargets);
        tetOpt.AddConstraints(tetStrain);
        tetOpt.FinalizeConstraints();
        const Eigen::Matrix<T, 3, -1> vertexOffsets = tetOpt.Solve(cgIterations);
        deformedTetVertices += vertexOffsets;
    }

    volModel.SetTetMeshVertices(deformedTetVertices);

    volModel.GetTetMesh().TetVolumeStatistics(minVol, avgVol, maxVol, false);
    LOG_INFO("post volume statistics: {} {} {}", minVol, avgVol, maxVol);
}

/**
 * TODO: comment
 * @param[inout] volModel  The volumetric model with deformed mandible and skin (cranium should never change). The flesh and tet mesh are updated.
 * @param[in] referenceVolModel  The reference model. Typically the same "identity" as @p volModel but in Neutral position.
 */
template <class T>
void VolumetricDeformation<T>::DeformTetsAndFleshMeshUsingCraniumMandibleAndSkinAsBoundaryConditions(VolumetricFaceModel<T>& volModel,
                                                                                                     const VolumetricFaceModel<T>& referenceVolModel)
{
    MatrixVariable<T, 3, -1> deformedTetVerticesVariable(3, volModel.GetTetMesh().NumVertices());
    deformedTetVerticesVariable.SetMatrix(volModel.GetTetMesh().Vertices());

    std::vector<BarycentricCoordinates<T, 4>> barycentricCoordinates;
    std::vector<Eigen::Vector<T, 3>> targetVertices;
    for (const auto& [skinIndex, fleshIndex] : volModel.SkinFleshMapping())
    {
        targetVertices.push_back(volModel.GetSkinMesh().Vertices().col(skinIndex));
        barycentricCoordinates.push_back(volModel.Embedding().GetBarycentricCoordinates()[fleshIndex]);
    }
    for (const auto& [craniumIndex, fleshIndex] : volModel.CraniumFleshMapping())
    {
        targetVertices.push_back(volModel.GetCraniumMesh().Vertices().col(craniumIndex));
        barycentricCoordinates.push_back(volModel.Embedding().GetBarycentricCoordinates()[fleshIndex]);
    }
    for (const auto& [mandibleIndex, fleshIndex] : volModel.MandibleFleshMapping())
    {
        targetVertices.push_back(volModel.GetMandibleMesh().Vertices().col(mandibleIndex));
        barycentricCoordinates.push_back(volModel.Embedding().GetBarycentricCoordinates()[fleshIndex]);
    }

    TetConstraints<T> tetConstraints;
    tetConstraints.SetTopology(referenceVolModel.GetTetMesh().Tets());
    tetConstraints.SetRestPose(referenceVolModel.GetTetMesh().Vertices());

    const T strainWeight = 1.0;
    Eigen::Matrix<T, 3, -1> targetVerticesMatrix = Eigen::Map<const Eigen::Matrix<T, 3, -1>>(&targetVertices[0][0],
                                                                                             3,
                                                                                             static_cast<int>(targetVertices.size()));
    DiffDataMatrix<T, 3, -1> diffTargetVertices(targetVerticesMatrix);

    std::function<DiffData<T>(Context<T>* context)> evaluationFunction = [&](Context<T>* context) -> DiffData<T>
        {
            Cost<T> cost;

            DiffDataMatrix<T, 3, -1> deformedTetVertices = deformedTetVerticesVariable.EvaluateMatrix(context);
            DiffDataMatrix<T, 3, -1> deformedVertices = BarycentricCoordinatesFunction<T, 3, 4>::Evaluate(deformedTetVertices,
                                                                                                          barycentricCoordinates);

            cost.Add(deformedVertices - diffTargetVertices, T(10));
            cost.Add(tetConstraints.EvaluateStrainLinearProjective(deformedTetVertices, strainWeight,
                                                                   TetConstraints<T>::ElasticityModel::Linear), T(1)); //
                                                                                                                       // Corotated),
                                                                                                                       //
                                                                                                                       // T(1));

            return cost.CostToDiffData();
        };

    T minVol, avgVol, maxVol;
    volModel.GetTetMesh().TetVolumeStatistics(minVol, avgVol, maxVol, false);
    LOG_INFO("pre volume statistics: {} {} {}", minVol, avgVol, maxVol);

    GaussNewtonSolver<T> solver;
    LOG_INFO("start energy: {}", evaluationFunction(nullptr).Value().squaredNorm());
    if (!solver.Solve(evaluationFunction, 10))
    {
        CARBON_CRITICAL("could not solve optimization problem\n");
    }
    LOG_INFO("final energy: {}", evaluationFunction(nullptr).Value().squaredNorm());

    volModel.SetTetMeshVertices(deformedTetVerticesVariable.Matrix());

    volModel.GetTetMesh().TetVolumeStatistics(minVol, avgVol, maxVol, false);
    LOG_INFO("pre volume statistics: {} {} {}", minVol, avgVol, maxVol);

    Eigen::Matrix<T, 3, -1> deformedFleshVertices = BarycentricCoordinatesFunction<T, 3, 4>::Evaluate(deformedTetVerticesVariable.EvaluateMatrix(
                                                                                                          nullptr),
                                                                                                      volModel.Embedding().GetBarycentricCoordinates())
        .Matrix();

    volModel.SetFleshMeshVertices(deformedFleshVertices);
    // volModel.UpdateFleshMeshVerticesFromSkinCraniumAndMandible();
}

// compatibility function checking if the normal is pointing in a similar direction and if the point is valid
template <typename scalar_t, size_t dimension = 3>
struct NormalAndMaskCompatiblityFunction
{
    using data_t = Eigen::Matrix<scalar_t, Eigen::Dynamic, dimension, Eigen::RowMajor>;
    using value_t = Eigen::RowVector<scalar_t, dimension>;
    using mask_t = Eigen::Matrix<scalar_t, Eigen::Dynamic, 1>;

    const Eigen::Ref<const data_t> normals;
    const Eigen::Ref<const mask_t> mask;

    NormalAndMaskCompatiblityFunction(const Eigen::Ref<const data_t>& normals_, const Eigen::Ref<const mask_t>& mask_)
        : normals(normals_)
        , mask(mask_)
    {}

    inline bool isCompatible(const size_t index, const Eigen::Ref<const value_t>& normal, const scalar_t threshold) const
    {
        return (mask[index] > 0) && (normal.dot(normals.row(index)) > threshold);
    }
};

template <class T>
void VolumetricDeformation<T>::ConformCraniumAndMandibleToTeeth(VolumetricFaceModel<T>& volModel, const TemplateDescription& templateDescription)
{
    const VertexWeights<float> craniumTeethCollisionSurfaceWeights = templateDescription.GetAssetVertexWeights(
        "cranium_with_teeth",
        "teeth_collision_surface");
    const VertexWeights<float> mandibleTeethCollisionSurfaceWeights = templateDescription.GetAssetVertexWeights(
        "mandible_with_teeth",
        "teeth_collision_surface");
    const VertexWeights<float> craniumTeethCollisionFixedWeights = templateDescription.GetAssetVertexWeights("cranium_with_teeth",
                                                                                                             "teeth_collision_fixed");
    const VertexWeights<float> mandibleTeethCollisionFixedWeights = templateDescription.GetAssetVertexWeights(
        "mandible_with_teeth",
        "teeth_collision_fixed");
    const VertexWeights<float> lowerTeethWeights = templateDescription.GetAssetVertexWeights("teeth",
                                                                                             "lower_teeth_collision_surface");
    const VertexWeights<float> upperTeethWeights = templateDescription.GetAssetVertexWeights("teeth",
                                                                                             "upper_teeth_collision_surface");

    // create kdtree of teeth with normal compatibility function
    Mesh<T> teethMesh = volModel.GetTeethMesh();
    teethMesh.Triangulate();
    teethMesh.CalculateVertexNormals();
    Mesh<T> craniumMesh = volModel.GetCraniumMesh();
    craniumMesh.Triangulate();
    craniumMesh.CalculateVertexNormals();
    Mesh<T> mandibleMesh = volModel.GetMandibleMesh();
    mandibleMesh.Triangulate();
    mandibleMesh.CalculateVertexNormals();

    auto deformProxy = [](
        Mesh<T>& srcMesh,
        const Mesh<T>& targetMesh,
        const VertexWeights<float>& srcWeights,
        const VertexWeights<float>& targetWeights,
        const VertexWeights<float>& srcFixedWeights)
        {
            const int innerIterations = 5;
            const T normalCompatibilityThreshold = T(0.8);
            const T vertexSmoothness = T(0.5);
            const T deltaSmoothness = T(1);
            const T offsetRegularization = T(0.01);

            const Eigen::VectorX<T> targetWeightsVec = targetWeights.Weights().template cast<T>();

            TITAN_NAMESPACE::KdTree<T, NormalAndMaskCompatiblityFunction<T>> targetKdTree(targetMesh.Vertices().transpose(),
                                                                                       targetMesh.VertexNormals().transpose(),
                                                                                       targetWeightsVec);
            TITAN_NAMESPACE::AABBTree<T> targetAabbTree(targetMesh.Vertices().transpose(), targetMesh.Triangles().transpose());

            DeformationModelVertex<T> defModelVertex;
            VertexLaplacian<T> vertexLaplacian;

            vertexLaplacian.SetRestPose(srcMesh, VertexLaplacian<T>::Type::COTAN);

            defModelVertex.SetMeshTopology(srcMesh);
            defModelVertex.SetRestVertices(srcMesh.Vertices());
            defModelVertex.SetVertexOffsets(Eigen::Matrix<T, 3, -1>::Zero(3, srcMesh.NumVertices()));
            defModelVertex.MakeVerticesConstant(srcFixedWeights.NonzeroVertices());
            auto config = defModelVertex.GetConfiguration();
            config["vertexLaplacian"] = deltaSmoothness;
            config["vertexOffsetRegularization"] = offsetRegularization;
            defModelVertex.SetConfiguration(config);

            std::vector<BarycentricCoordinates<T, 3>> sampleSrcBCs;
            std::vector<T> sampleWeights;
            for (size_t i = 0; i < srcWeights.NonzeroVerticesAndWeights().size(); ++i)
            {
                sampleSrcBCs.push_back(BarycentricCoordinates<T,
                                                              3>::SingleVertex(srcWeights.NonzeroVerticesAndWeights()[i].first));
                sampleWeights.push_back(srcWeights.NonzeroVerticesAndWeights()[i].second);
            }
            for (int i = 0; i < srcMesh.NumTriangles(); ++i)
            {
                const Eigen::Vector3i tIDs = srcMesh.Triangles().col(i);
                const float w0 = srcWeights.Weights()[tIDs[0]];
                const float w1 = srcWeights.Weights()[tIDs[2]];
                const float w2 = srcWeights.Weights()[tIDs[1]];
                if ((w0 >= 0) && (w1 >= 0) && (w2 >= 0))
                {
                    for (const auto& bcs : std::vector<Eigen::Vector3<T>> {
                        { T(0.3333), T(0.3333), T(0.333) },
                        { T(0.5), T(0.5), T(0.0) },
                        { T(0), T(0.5), T(0.5) },
                        { T(0.5), T(0.0), T(0.5) } })
                    {
                        sampleSrcBCs.push_back(BarycentricCoordinates<T, 3>(tIDs, Eigen::Vector3<T>(bcs[0], bcs[1], bcs[2])));
                        sampleWeights.push_back(bcs[0] * w0 + bcs[1] * w1 + bcs[2] * w2);
                    }
                }
            }

            std::vector<BarycentricCoordinates<T, 3>> corrSrcBCs;
            std::vector<Eigen::Vector3<T>> corrTargetVertices;
            std::vector<Eigen::Vector3<T>> corrTargetNormals;
            std::vector<T> corrWeights;

            std::function<DiffData<T>(Context<T>*)> evaluationFunction = [&](Context<T>* context)
                {
                    Cost<T> cost;

                    const DiffDataMatrix<T, 3, -1> vertices = defModelVertex.EvaluateVertices(context);
                    cost.Add(defModelVertex.EvaluateModelConstraints(context), 1.0f);

                    if (context || corrSrcBCs.empty())
                    {
                        srcMesh.SetVertices(vertices.Matrix());
                        srcMesh.CalculateVertexNormals();
                        corrSrcBCs.clear();
                        corrTargetVertices.clear();
                        corrTargetNormals.clear();
                        corrWeights.clear();

                        for (size_t i = 0; i < sampleSrcBCs.size(); ++i)
                        {
                            const Eigen::Vector3<T> vertex = sampleSrcBCs[i].template Evaluate<3>(srcMesh.Vertices());
                            const Eigen::Vector3<T> normal =
                                sampleSrcBCs[i].template Evaluate<3>(srcMesh.VertexNormals()).normalized();
                            const auto [targettID, bcs, absDist] = targetAabbTree.intersectRayBidirectional(vertex.transpose(),
                                                                                                            normal.transpose());
                            if (targettID >= 0)
                            {
                                const Eigen::Vector3i triangle = targetMesh.Triangles().col(targettID);
                                const BarycentricCoordinates<T> bc(triangle, bcs.transpose());
                                const Eigen::Vector3<T> targetVertex = bc.template Evaluate<3>(targetMesh.Vertices());
                                const Eigen::Vector3<T> targetNormal =
                                    bc.template Evaluate<3>(targetMesh.VertexNormals()).normalized();
                                const T targetWeight = bc.template Evaluate<1>(targetWeightsVec)[0];

                                if ((targetWeight > 0) && (targetNormal.dot(normal) > normalCompatibilityThreshold))
                                {
                                    corrSrcBCs.push_back(sampleSrcBCs[i]);
                                    corrTargetVertices.push_back(targetVertex);
                                    corrTargetNormals.push_back(targetNormal);
                                    corrWeights.push_back(sampleWeights[i] * targetWeight);
                                }
                            }
                        }
                    }
                    auto selectedVertices = BarycentricCoordinatesFunction<T, 3>::Evaluate(vertices, corrSrcBCs);
                    cost.Add(PointSurfaceConstraintFunction<T, 3>::Evaluate(selectedVertices, corrTargetVertices.data(),
                                                                            corrTargetNormals.data(),
                                                                            corrWeights.data(), 1.0f),
                             T(1));
                    cost.Add(vertexLaplacian.EvaluateLaplacianOnOffsets(vertices, T(1)), vertexSmoothness);

                    return cost.CostToDiffData();
                };

            GaussNewtonSolver<T> assetSolver;
            const T startEnergy = evaluationFunction(nullptr).Value().squaredNorm();
            if (!assetSolver.Solve(evaluationFunction, innerIterations))
            {
                LOG_WARNING("could not solve optimization problem");
                return;
            }
            const T finalEnergy = evaluationFunction(nullptr).Value().squaredNorm();
            LOG_INFO("deformation energy changed from {} to {}", startEnergy, finalEnergy);
            srcMesh.SetVertices(defModelVertex.EvaluateVertices(nullptr).Matrix());
        };

    deformProxy(craniumMesh, teethMesh, craniumTeethCollisionSurfaceWeights, upperTeethWeights,
                craniumTeethCollisionFixedWeights);
    volModel.SetCraniumMeshVertices(craniumMesh.Vertices());
    deformProxy(mandibleMesh,
                teethMesh,
                mandibleTeethCollisionSurfaceWeights,
                lowerTeethWeights,
                mandibleTeethCollisionFixedWeights);
    volModel.SetMandibleMeshVertices(mandibleMesh.Vertices());
}

template <class T>
std::pair<typename MeshCorrespondenceSearch<T>::Result, typename MeshCorrespondenceSearch<T>::Result>
VolumetricDeformation<T>::CraniumAndMandibleToTeethCorrespondences(VolumetricFaceModel<T>& volModel, const TemplateDescription& templateDescription)
{
    const VertexWeights<float> craniumTeethCollisionSurfaceWeights = templateDescription.GetAssetVertexWeights(
        "cranium_with_teeth",
        "teeth_collision_surface");
    const VertexWeights<float> mandibleTeethCollisionSurfaceWeights = templateDescription.GetAssetVertexWeights(
        "mandible_with_teeth",
        "teeth_collision_surface");
    const VertexWeights<float> lowerTeethWeights = templateDescription.GetAssetVertexWeights("teeth",
                                                                                             "lower_teeth_collision_surface");
    const VertexWeights<float> upperTeethWeights = templateDescription.GetAssetVertexWeights("teeth",
                                                                                             "upper_teeth_collision_surface");

    // create kdtree of teeth with normal compatibility function
    Mesh<T> teethMesh = volModel.GetTeethMesh();
    teethMesh.Triangulate();
    teethMesh.CalculateVertexNormals();
    Mesh<T> craniumMesh = volModel.GetCraniumMesh();
    craniumMesh.Triangulate();
    craniumMesh.CalculateVertexNormals();
    Mesh<T> mandibleMesh = volModel.GetMandibleMesh();
    mandibleMesh.Triangulate();
    mandibleMesh.CalculateVertexNormals();

    auto getCorrespondences =
        [](const Mesh<T>& srcMesh, const Mesh<T>& targetMesh, const VertexWeights<float>& srcWeights,
           const VertexWeights<float>& targetWeights)
        {
            const Eigen::VectorX<T> targetWeightsVec = targetWeights.Weights().template cast<T>();
            TITAN_NAMESPACE::KdTree<T, NormalAndMaskCompatiblityFunction<T>> kdTree(targetMesh.Vertices().transpose(),
                                                                                 targetMesh.VertexNormals().transpose(),
                                                                                 targetWeightsVec);
            TITAN_NAMESPACE::AABBTree<T> targetAabbTree(targetMesh.Vertices().transpose(), targetMesh.Triangles().transpose());

            const T normalCompatibilityThreshold = T(0.8);

            std::vector<int> corrSrcIndices;
            std::vector<Eigen::Vector3<T>> corrTargetVertices;
            std::vector<Eigen::Vector3<T>> corrTargetNormals;
            std::vector<T> corrWeights;

            for (size_t i = 0; i < srcWeights.NonzeroVerticesAndWeights().size(); ++i)
            {
                const auto& [vID, srcWeight] = srcWeights.NonzeroVerticesAndWeights()[i];
                const Eigen::Vector3<T> vertex = srcMesh.Vertices().col(vID);
                const Eigen::Vector3<T> normal = srcMesh.VertexNormals().col(vID);

                const auto [targettID, bcs, absDist] = targetAabbTree.intersectRayBidirectional(vertex.transpose(),
                                                                                                normal.transpose());
                if (targettID >= 0)
                {
                    const Eigen::Vector3i triangle = targetMesh.Triangles().col(targettID);
                    const BarycentricCoordinates<T> bc(triangle, bcs.transpose());
                    const Eigen::Vector3<T> targetVertex = bc.template Evaluate<3>(targetMesh.Vertices());
                    const Eigen::Vector3<T> targetNormal = bc.template Evaluate<3>(targetMesh.VertexNormals()).normalized();
                    const T targetWeight = bc.template Evaluate<1>(targetWeightsVec)[0];
                    if ((targetWeight > 0) && (targetNormal.dot(normal) > normalCompatibilityThreshold))
                    {
                        corrSrcIndices.push_back(vID);
                        corrTargetVertices.push_back(targetVertex);
                        corrTargetNormals.push_back(targetNormal);
                        corrWeights.push_back(srcWeight);
                    }
                }
            }

            typename MeshCorrespondenceSearch<T>::Result corrResults;
            corrResults.srcIndices = Eigen::Map<const Eigen::VectorXi>(corrSrcIndices.data(), corrSrcIndices.size());
            corrResults.targetVertices = Eigen::Map<const Eigen::Matrix<T, 3, -1>>((const T*)corrTargetVertices.data(),
                                                                                   3,
                                                                                   corrTargetVertices.size());
            corrResults.targetNormals = Eigen::Map<const Eigen::Matrix<T, 3, -1>>((const T*)corrTargetNormals.data(),
                                                                                  3,
                                                                                  corrTargetNormals.size());
            corrResults.weights = Eigen::Map<const Eigen::VectorX<T>>(corrWeights.data(), corrWeights.size());
            return corrResults;
        };

    return {
        getCorrespondences(craniumMesh, teethMesh, craniumTeethCollisionSurfaceWeights, upperTeethWeights),
        getCorrespondences(mandibleMesh, teethMesh, mandibleTeethCollisionSurfaceWeights, lowerTeethWeights)
    };
}

template <class T>
bool VolumetricDeformation<T>::CreateVolumetricFaceModelFromRig(const Rig<T>& rig,
                                                                const TemplateDescription& templateDescription,
                                                                const VolumetricFaceModel<T>& referenceModel,
                                                                VolumetricFaceModel<T>& output,
                                                                bool optimizeTets)
{
    // STEP 1: deform cranium and mandible using a volumetric morph based on skin, teeth, and eyes
    const int lod = 0;
    Mesh<T> newSkinMesh = rig.GetRigGeometry()->GetMesh(rig.GetRigGeometry()->HeadMeshIndex(lod));
    Mesh<T> newTeethMesh = rig.GetRigGeometry()->GetMesh(rig.GetRigGeometry()->TeethMeshIndex(lod));
    Mesh<T> newEyeLeftMesh = rig.GetRigGeometry()->GetMesh(rig.GetRigGeometry()->EyeLeftMeshIndex(lod));
    Mesh<T> newEyeRightMesh = rig.GetRigGeometry()->GetMesh(rig.GetRigGeometry()->EyeRightMeshIndex(lod));

    output = MorphVolumetricFaceModelUsingGrid(referenceModel,
                                               templateDescription,
                                               newSkinMesh,
                                               newTeethMesh,
                                               newEyeLeftMesh,
                                               newEyeRightMesh,
                                               /*gridSize=*/32);

    if (templateDescription.HasAssetVertexWeights("cranium_with_teeth"))
    {
        ConformCraniumAndMandibleToTeeth(output, templateDescription);
    }
    else
    {
        LOG_INFO("skipping conforming cranium and mandible to the teeth");
    }

    // STEP 2: deform the flesh mesh using the cranium, mandible, skin, and eyes as constraints (surface smoothness constraints)
    output = DeformFleshMeshUsingCraniumMandibleSkinAndEyesAsBoundaryConditions(output,
                                                                                referenceModel.GetFleshMesh(),
                                                                                templateDescription,
                                                                                newEyeLeftMesh,
                                                                                newEyeRightMesh);

    // STEP 3: deform the flesh tets based on the flesh mesh
    if (optimizeTets)
    {
        UpdateTetsUsingFleshMesh(output, referenceModel);
    }

    return true;
}

template class VolumetricDeformation<float>;
template class VolumetricDeformation<double>;

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
