// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>
#include <nls/geometry/Mesh.h>
#include <nls/geometry/MeshCorrespondenceSearch.h>
#include <nls/math/Math.h>
#include <rig/Rig.h>
#include <nrr/TemplateDescription.h>
#include <nrr/volumetric/VolumetricFaceModel.h>

#include <string>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
class VolumetricDeformation
{
public:
    /**
     * Morphs the @p inputVolModel by optimizing a grid deformation from @p srcVertices to
     * @p targetVertices and apply the grid deformation on all vertices of the @p inputVolModel.
     */
    static VolumetricFaceModel<T> MorphVolumetricFaceModelUsingGrid(const VolumetricFaceModel<T>& inputVolModel,
                                                                    Eigen::Ref<const Eigen::Matrix<T, 3, -1>> srcVertices,
                                                                    Eigen::Ref<const Eigen::Matrix<T, 3, -1>> targetVertices,
                                                                    int gridSize = 32);


    /**
     * Deforms the flesh mesh vertices to coincide with the skin mesh (based on the mapping) and
     * also ensures that the flesh mesh conforms to the input eyes. Note that there
     * is not a correspondence between eyeballs and the flesh mesh, in particular the skin mesh
     * can slide over the eyeballs, and hence using a fixed correspondence would lead to artifacts.
     */
    static Eigen::Matrix<T, 3, -1> DeformFleshMeshUsingSkinAndEyes(const VolumetricFaceModel<T>& inputVolModel,
                                                                   const TemplateDescription& templateDescription,
                                                                   const Mesh<T>& newSkinMesh,
                                                                   const Mesh<T>& newEyeLeftMesh,
                                                                   const Mesh<T>& newEyeRightMesh);

    /**
     * Morphs the volumetric model using a grid deformation model. As boundary counstraints the grid deformation uses
     * the skin mesh @p newSkinMesh, the teeth mesh @p newTeethMesh, as well as the eye meshes.
     * @param[in] inputVolModel  The input volumetric model that is morphed.
     * @param[in] newSkinMesh    The skin mesh that acts as target for the volumetric morph.
     * @param[in] newTeethMesh   The teeth mesh that acts as target for the volumetric morph.
     * @returns the morphed volumetric model were each mesh has been morphed, and skin and possible teeth mesh are replaced by the input meshes.
     * @warning the method does *NOT* update the tet mesh.
     */
    static VolumetricFaceModel<T> MorphVolumetricFaceModelUsingGrid(const VolumetricFaceModel<T>& inputVolModel,
                                                                    const TemplateDescription& templateDescription,
                                                                    const Mesh<T>& newSkinMesh,
                                                                    const Mesh<T>& newTeethMesh,
                                                                    const Mesh<T>& newEyeLeftMesh,
                                                                    const Mesh<T>& newEyeRightMesh,
                                                                    int gridSize = 32);

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
    static VolumetricFaceModel<T> DeformFleshMeshUsingCraniumMandibleSkinAndEyesAsBoundaryConditions(const VolumetricFaceModel<T>& inputVolModel,
                                                                                                     const Mesh<T>& referenceFleshMesh,
                                                                                                     const TemplateDescription& templateDescription,
                                                                                                     const Mesh<T>& eyeLeftMesh,
                                                                                                     const Mesh<T>& eyeRightMesh);

    /**
     * Deform the tets based on the flesh mesh.
     * @param[inout] volModel  The volumetric face model or which the tests are updated.
     * @param[in] referenceVolModel  The reference volumetric model that defines the rest state of the tets
     */
    static void UpdateTetsUsingFleshMesh(VolumetricFaceModel<T>& volModel, const VolumetricFaceModel<T>& referenceVolModel);

    /**
     * TODO: comment
     * @param[inout] volModel  The volumetric model with deformed mandible and skin (cranium should never change). The flesh and tet mesh are updated.
     * @param[in] referenceVolModel  The reference model. Typically the same "identity" as @p volModel but in Neutral position.
     */
    static void DeformTetsAndFleshMeshUsingCraniumMandibleAndSkinAsBoundaryConditions(VolumetricFaceModel<T>& volModel,
                                                                                      const VolumetricFaceModel<T>& referenceVolModel);

    static void ConformCraniumAndMandibleToTeeth(VolumetricFaceModel<T>& volModel, const TemplateDescription& templateDescription);

    static std::pair<typename MeshCorrespondenceSearch<T>::Result,
                     typename MeshCorrespondenceSearch<T>::Result> CraniumAndMandibleToTeethCorrespondences(VolumetricFaceModel<T>& volModel,
                                                                                                            const TemplateDescription& templateDescription);

    static bool CreateVolumetricFaceModelFromRig(const Rig<T>& rig,
                                                 const TemplateDescription& templateDescription,
                                                 const VolumetricFaceModel<T>& referenceModel,
                                                 VolumetricFaceModel<T>& output,
                                                 bool optimizeTets);
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)
