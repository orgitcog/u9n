// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Defs.h"

#include <map>
#include <string>
#include <vector>

namespace dna
{

class Reader;
class Writer;

} // namespace dna

namespace TITAN_API_NAMESPACE
{

enum class RefinementMaskType
{
    MOUTH_SOCKET,
    TEETH_PLACEMENT
};

enum class RefinementMeshCorrespondenceType
{
    RIGID,
    DELTA_TRANSFER,
    UV_SPACE_PROJECTION
};

class TITAN_API ActorRefinementAPI
{
public:
    ActorRefinementAPI();
    ~ActorRefinementAPI();
    ActorRefinementAPI(ActorRefinementAPI&&) = delete;
    ActorRefinementAPI(const ActorRefinementAPI&) = delete;
    ActorRefinementAPI& operator=(ActorRefinementAPI&&) = delete;
    ActorRefinementAPI& operator=(const ActorRefinementAPI&) = delete;

    /**
     * Get vertex weights for mask type.
     * @param[out] InVertexWeights weight values
     * @param[in] InMaskType mask type
     * @returns True if was successful.
     */
    bool GetRefinementMask(float* OutVertexWeights, RefinementMaskType InMaskType) const;

    /**
     * Set vertex weights for mask type.
     * @param[in] Num of input weights
     * @param[in] InVertexWeights weight values
     * @param[in] InMaskType mask type
     * @returns True if was successful.
     */
    bool SetRefinementMask(int32_t NumVertices, const float* InVertexWeights, RefinementMaskType InMaskType);

    /**
     * Update teeth model and position in the rig given input data.
     * @param[in] InDnaStream  Input DNA stream.
     * @param[in] InTeethMeshVertexPositions  Input target vertex positions - Teeth mesh vertices - as numVertices x 3 in column major order..
     * @param[out] OutDnaStream  Updated dna stream.
     * @returns True if fitting and DNA update was successful.
     */
    bool UpdateRigWithTeethMeshVertices(dna::Reader* InDnaStream, const float* InTeethMeshVertexPositions, dna::Writer* OutDnaStream);

    /**
     * Update the joints and mesh assets using target head mesh vertex positions.
     * @param[in] InDnaStream  Input DNA stream.
     * @param[in] InHeadMeshVertexPositions  Input target vertex positions - Head mesh vertices - as numVertices x 3 in column major order.
     * @param[out] OutDnaStream  Updated dna stream.
     * @returns True if DNA update was successful.
     */
    bool UpdateRigWithHeadMeshVertices(dna::Reader* InDnaStream,
                                       const float* InHeadMeshVertexPositions,
                                       const float* InTeethMeshVertexPositions,
                                       const float* InEyeLeftMeshVertexPositions,
                                       const float* InEyeRightMeshVertexPositions,
                                       dna::Writer* OutDnaStream);

    /**
     * Update the joints and mesh assets using target vertex positions.
     * @param[in] InDnaStream  Input DNA stream.
     * @param[in] InVertexPositions  Input target vertex positions - as numVertices x 3 in column major order.
     * @param[out] OutDnaStream  Updated dna stream.
     * @param[out] deltaTransferCorrespondanceData  delta transfer mesh correspondance in neutral.
     * @returns True if DNA update was successful.
     */
    bool RefineRig(dna::Reader* InDnaStream, std::map<std::string, const float*> InVertexPositions, dna::Writer* OutDnaStream,
        std::map<std::string, std::tuple<std::string, std::vector<int>, std::vector<std::vector<float>>>>& OutDeltaTransferCorrespondanceData);

    /*
     * Check that the supplied json string for the controls config is valid.
     * @param[in] InControlsConfigJson: the json config defining the set of shapes which help position the teeth without intersecting the mesh
     * @returns true if controls config json is valid, false otherwise
     */
     bool CheckControlsConfig(const char* InControlsConfigJson);


    /**
     * Optimize and update the teeth position in the rig based on the input reference rig.
     * @param[in] InDnaStream  Input DNA stream.
     * @param[in] InRefDnaStream  Input referent rig DNA stream.
     * @param[in] InControlsConfigJson  Contains the Json for the controls config
     * @param[out] OutDnaStream  Updated dna stream.
     * @returns True if DNA update was successful.
     */
    bool RefineTeethPlacement(dna::Reader* InDnaStream,
                              dna::Reader* InRefDnaStream,
                              const char* InControlsConfigJson,
                              dna::Writer* OutDnaStream);

    /**
     * Transform the rig with 4x4 transform matrix.
     * @param[in] InDnaStream  Input DNA stream.
     * @param[in] InTransformMatrix  Input 4x4 transformation matrix in column major order.
     * @param[out] OutDnaStream  Updated dna stream.
     * @returns True if DNA update was successful.
     */
    bool TransformRigOrigin(dna::Reader* InDnaStream, const float* InTransformMatrix, dna::Writer* OutDnaStream);

    /**
     * Scale the rig with scale parameter and scaling pivot.
     * @param[in] InDnaStream  Input DNA stream.
     * @param[in] InScale  Input scale parameter.
     * @param[in] InScaingPivot Input 3D vector representing scaling pivot point.
     * @param[out] OutDnaStream  Updated dna stream.
     * @returns True if DNA update was successful.
     */
    bool ScaleRig(dna::Reader* InDnaStream, const float InScale, const float* InScalingPivot, dna::Writer* OutDnaStream);

    /**
     * Apply transform and scale to the rig with 4x4 transform, scale parameter and scaling pivot.
     * @param[in] InDnaStream  Input DNA stream.
     * @param[in] InTransformMatrix  Input 4x4 transformation matrix in column major order.
     * @param[in] InScale  Input scale parameter.
     * @param[in] InScaingPivot Input 3D vector representing scaling pivot point.
     * @param[out] OutDnaStream  Updated dna stream.
     * @returns True if DNA update was successful.
     */
    bool ScaleAndTransformRig(dna::Reader* InDnaStream,
                              const float* InTransformMatrix,
                              const float InScale,
                              const float* InScalingPivot,
                              dna::Writer* OutDnaStream);

    /**
     * Applies delta dna to existing DNA and stores result in InOutDna.
     * [in] InDna input DNA stream.
     * [in] InDeltaDna delta dna.
     * [out] OutFinalDna - final DNA.
     * [in] InMask optional mask to multiply delta before applying. Default value should be used.
     *
     * @returns True if operation was successful, False otherwise.
     */
    bool ApplyDNA(dna::Reader* InDna,
                  dna::Reader* InDeltaDna,
                  dna::Writer* OutFinalDna,
                  const std::vector<float>& InMask = {});

    /**
     * Generates delta dna as InToDna - InFromDna and stores result in OutDeltaDna.
     * [in] InFromDna input DNA stream - subtrahend.
     * [in] InToDna input DNA stream - minuend.
     * [out] OutFinalDna - deltaDNA
     * [in] InMask optional mask to multiply delta before applying. Default value should be used.
     *
     * @returns True if operation was successful, False otherwise.
     */
    bool GenerateDeltaDNA(dna::Reader* InFromDna,
                          dna::Reader* InToDna,
                          dna::Writer* OutDeltaDna,
                          const std::vector<float>& InMask = {});

    /**
     * Set the mesh names from the dna file used as main reference for rig refinement.
     * [in] InDrivingMeshNames input vector of mesh names (corresponding to naming convention in dna file)
     *
     * @returns True if operation was successful, False otherwise.
     */
    bool SetDrivingMeshNames(const std::vector<std::string>& InDrivingMeshNames);

    /**
     * Set the joint names from the dna file which can only be transformed with global rigid transformation.
     * [in] InInactiveJointNames input vector of joint names (corresponding to naming convention in dna file)
     *
     * @returns True if operation was successful, False otherwise.
     */
    bool SetInactiveJointNames(const std::vector<std::string>& InInactiveJointNames);

    /**
     * Set the relationship between other meshes in the rig with the main reference meshes.
     * [in] InDrivenMeshNames input map of main mesh names as keys and vector of dependent mesh names (corresponding to naming convention in dna file)
     * [in] InCorrespondanceType type of the mesh correspondance
     *
     * @returns True if operation was successful, False otherwise.
     */
    bool SetMeshCorrespondance(const std::map<std::string, std::vector<std::string>>& InDrivenMeshNames, RefinementMeshCorrespondenceType InCorrespondanceType);

    /**
     * Set the relationship between joints in the rig with the main reference meshes.
     * [in] InDrivenJointNames input map of main mesh names as keys and vector of joint names (corresponding to naming convention in dna file)
     *
     * @returns True if operation was successful, False otherwise.
     */
    bool SetDrivenJointNames(const std::map<std::string, std::vector<std::string>>& InDrivenJointNames);

    /**
     * Set the joint names from the dna file which will be placed based on the sphere optimization.
     * [in] InOptimizationJointNames input vector of joint names (corresponding to naming convention in dna file)
     *
     * @returns True if operation was successful, False otherwise.
     */
    bool SetOptimizationJointNames(const std::vector<std::string>& OptimizationJointNames);

    /**
     * Set the relationship between joints in the rig with the same position and behavior.
     * [in] InDependentJointNames input map of main mesh names as keys and vector of joint names (corresponding to naming convention in dna file)
     *
     * @returns True if operation was successful, False otherwise.
     */
    bool SetDependentJointNames(const std::map<std::string, std::vector<std::string>>& InDependentJointNames);

private:
    struct Private;
    Private* m;
};

} // namespace TITAN_API_NAMESPACE
