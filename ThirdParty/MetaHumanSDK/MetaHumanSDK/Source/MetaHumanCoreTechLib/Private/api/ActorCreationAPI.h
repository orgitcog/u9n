// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Defs.h"

#include <MeshInputData.h>
#include <LandmarkData.h>
#include <OpenCVCamera.h>

#include <map>
#include <memory>
#include <stdint.h>
#include <string>
#include <vector>

namespace dna
{

class Reader;
class Writer;

} // namespace dna

namespace TITAN_API_NAMESPACE
{

enum class FittingMaskType
{
    RIGID,
    NONRIGID,
    FINE,
    EYE_INTERFACE_LEFT,
    EYE_INTERFACE_RIGHT,
    TEETH,
    EYE,
    MOUTH_SOCKET,
    STABILIZATION,
    TEETH_HEAD_COLLISION_INTERFACE
};

enum class ScanMaskType
{
    GLOBAL,
    EYE_FITTING
};

enum class IdentityModelType
{
    FACE,
    COMBINED
};

class TITAN_API ActorCreationAPI
{
public:
    ActorCreationAPI();
    ~ActorCreationAPI();
    ActorCreationAPI(ActorCreationAPI&&) = delete;
    ActorCreationAPI(const ActorCreationAPI&) = delete;
    ActorCreationAPI& operator=(ActorCreationAPI&&) = delete;
    ActorCreationAPI& operator=(const ActorCreationAPI&) = delete;

    /**
     * Initialize actor creation.
     * Note this way of initializing the API is now deprecated.
     *
     * @param[in] InConfigurationDirectory  Directory containing a template_description.json and a dna_description.json file.
     * @returns True if initialization is successful, False otherwise.
     */
    bool Init(const std::string& InConfigurationDirectory);


    /**
     * Initialize actor creation.
     * @param[in] InTemplateDescriptionJson  the flattened template description json
     * @param[in] InIdentityModelJson        the flattened identity model json
     * @returns True if initialization is successful, False otherwise.
     */
    bool Init(const char* InTemplateDescriptionJson, const char* InIdentityModelJson);


    /**
     * Set the depth input data for one frame.
     * @param[in] InLandmarksDataPerCamera  The distorted landmarks for each camera.
     * @param[in] InDepthMaps Depthmap data per (depthmap) camera.
     * @returns True if setting the data was successful.
     *
     * @warning Fails if scan data was set before.
     */
    bool SetDepthInputData(const std::map<std::string, const std::map<std::string, FaceTrackingLandmarkData>>& InLandmarksDataPerCamera,
                           const std::map<std::string, const float*>& InDepthMaps);

    /**
     * Set the scan input data.
     * @param[in] In3dLandmarksData  Landmarks in 3D.
     * @param[in] In2dLandmarksData  The distorted landmarks per camera view.
     * @param[in] InScanData  Input scan data with triangles stored as numTriangles x 3 and vertices as numVertices x 3 in column major format.
     * @param[out] bOutInvalidMeshTopology Set to false if the mesh has a valid topology, true if all vertices on the input mesh are invalid eg due to all disconnected triangles
     * @returns True if setting the data was successful.
     *
     * @warning Fails if depthmap data was set before.
     */
    bool SetScanInputData(const std::map<std::string, const FaceTrackingLandmarkData>& In3dLandmarksData,
                          const std::map<std::string,
                                         const std::map<std::string,
                                                        FaceTrackingLandmarkData>>& In2dLandmarksData,
                          const MeshInputData& InScanData,
                          bool& bOutInvalidMeshTopology);

    /**
     * Set up the cameras for fitting.
     * @param[in] InCameras  Input cameras for landmarks and depth projection.
     * @returns True if successful, false upon any error.
     */
    bool SetCameras(const std::map<std::string, OpenCVCamera>& InCameras);

    /**
     * Set the scan mask type. Method enables specific scan mask creation for eyes fitting or global mesh mask (masking out invalid input points).
     * Note that when generating eyes fitting mask, landmarks are needed for calculation. This means that the landmarks
     * from the camera with label *InCameraName* will be used.
     * @param[in] InCameraName  Input camera name. Note that this camera must already be set through *SetCameras*.
     * @param[in] InScanMaskType  Input scan mask type. Currently supports specific eye mask and global mask.
     * @returns True if successful, false upon any error.
     */
    bool CalculateAndUpdateScanMask(const std::string& InCameraName, ScanMaskType InScanMaskType);

    /**
     * Get regularization for non-rigid fitting.
     * @returns regularization multiplier for non-rigid fitting.
     */
    float GetModelRegularization() const;

    /**
     * Set regularization for non-rigid fitting.
     * @param[in] regularization multiplier
     */
    void SetModelRegularization(float regularization);

    /**
     * Get offset regularization for per-vertex fitting.
     * @returns regularization multiplier for per-vertex fitting.
     */
    float GetPerVertexOffsetRegularization() const;

    /**
     * Set regularization for per-vertex fitting.
     * @param[in] regularization multiplier
     */
    void SetPerVertexOffsetRegularization(float regularization);

    /**
     * Get Laplacian regularization for per-vertex fitting.
     * @returns regularization multiplier for per-vertex fitting.
     */
    float GetPerVertexLaplacianRegularization() const;

    /**
     * Set Laplacian regularization for per-vertex fitting.
     * @param[in] regularization multiplier
     */
    void SetPerVertexLaplacianRegularization(float regularization);

    /**
     * Get ICP minimum distance threshold (used for all types of fitting).
     * @returns threshold value.
     */
    float GetMinimumDistanceThreshold() const;

    /**
     * Set ICP minimum distance threshold (used for all types of fitting).
     * @param[in] threshold value
     */
    void SetMinimumDistanceThreshold(float threshold);

    /**
     * Get the switch option to use / not to use ICP distance threshold.
     * @returns if true, distance threshold will be used in fitting.
     */
    bool GetUseMinimumDistanceThreshold() const;

    /**
     * Set the switch option to use / not to use ICP distance threshold.
     * @param[in] if true, distance treshold will be used to reduce search
     */
    void SetUseMinimumDistanceThreshold(bool useIcpDistanceThreshold);

    /**
     * Get landmarks weight (used for all types of fitting).
     * @returns landmarks weight multiplier for all types of fitting.
     */
    float GetLandmarksWeight() const;

    /**
     * Set landmarks weight (used for all types of fitting).
     * @param[in] weight value
     */
    void SetLandmarksWeight(float weight);

    /**
     * Get is the multi-view landmark masking enabled
     * @returns is multi-view landmark masking enabled
     */
    bool GetAutoMultiViewLandmarkMasking() const;

    /**
     * Set option to use auto multi-view landmark masking
     * @param[in] bool value
     */
    void SetAutoMultiViewLandmarkMasking(bool useLandmarkMask);

    /**
     * Get landmarks weight for inner lips (used for all types of fitting).
     * @returns landmarks weight multiplier for all types of fitting.
     */
    float GetInnerLipsLandmarksWeight() const;

    /**
     * Set landmarks weight (used for all types of fitting).
     * @param[in] weight value
     */
    void SetInnerLipsLandmarksWeight(float weight);

    /**
     * Get collision weight for inner lips (used for all types of fitting).
     * @returns collision weight multiplier for all types of fitting.
     */
    float GetInnerLipsCollisionWeight() const;

    /**
     * Set collision weight (used for all types of fitting).
     * @param[in] collision weight value
     */
    void SetInnerLipsCollisionWeight(float weight);

    /**
     * Get regularization weight for rig logic fit.
     * @returns regularization weight multiplier for rig logic.
     */
    float GetRigLogicL1RegularizationWeight() const;

    /**
     * Set regularization weight for rig logic.
     * @param[in] regularization weight value
     */
    void SetRigLogicL1RegularizationWeight(float weight);

    /**
     * Get vertex weights for mask type.
     * @param[out] InVertexWeights weight values
     * @param[in] InMaskType mask type
     * @returns True if was successful.
     */
    bool GetFittingMask(float* OutVertexWeights, FittingMaskType InMaskType) const;

    /**
     * Set vertex weights for mask type.
     * @param[in] InVertexWeights weight values
     * @param[in] InMaskType mask type
     * @returns True if was successful.
     */
    bool SetFittingMask(float* InVertexWeights, FittingMaskType InMaskType);

    /**
     * Fit identity given input data.
     * @param[out] OutVertexPositions  Vertex positions describing new identity as numVertices x 3 in column major format placed in *RIG COORDINATE SPACE*.
     * @param[out] OutStackedToScanTransforms  Stacked 4x4 transform matrices in column major format.
     * @param[out] OutStackedToScanScales  Stacked scale values. Scale is not a linear part of ToScanTransform, and needs to be applied after the
     * transformation.
     * @param[in] InNumIters  Number of iterations of the optimization.
     * @param[in] InAutoMode  Enable auto mode where optimization will have iterations with decreasing regularization (InNumIters then have no effect).
     * @returns True if fitting was successful.
     */
    bool FitRigid(float* OutVertexPositions,
                  float* OutStackedToScanTransforms,
                  float* OutStackedToScanScales,
                  int32_t InNumIters = 3,
                  const bool InAutoMode = true);

    /**
     * Save debugging data. Saves the debugging data for Actor Creation. This method will save cameras (as .json), 2D landmarks (as .json) and a mesh for each
     * depthmap or scan (as an .obj mesh) into the specified folder.
     * Requires all data to have been set using SetCameras, SetScanInputData or SetDepthInputData before calling. Saving this data should allow a
     * researcher to replicate bugs found in UE. Currently does not support saving of 3D landmarks.
     * @param[in] InDebugDataDirectory  The folder to save the debug data to.
     * @returns True if saving the debugging data was successful.
     */
    bool SaveDebuggingData(const std::string& InDebugDataDirectory);

    /**
     * Fit identity given input data.
     * @param[out] OutVertexPositions  Vertex positions describing new identity as numVertices x 3 in column major format placed in *RIG COORDINATE SPACE*.
     * @param[out] OutStackedToScanTransforms  Stacked 4x4 transform matrices in column major format.
     * @param[out] OutStackedToScanScales  Stacked scale values. Scale is not a linear part of ToScanTransform, and needs to be applied after the
     * transformation.
     * @param[in] InNumIters  Number of iterations of the optimization.
     * @param[in] InAutoMode  Enable auto mode where optimization will have iterations with decreasing regularization (InNumIters then have no effect).
     * @returns True if fitting was successful.
     */
    bool FitNonRigid(float* OutVertexPositions,
                     float* OutStackedToScanTransforms,
                     float* OutStackedToScanScales,
                     int32_t InNumIters = 3,
                     const bool InAutoMode = true);

    /**
     * Fit identity given input data.
     * @param[out] OutVertexPositions  Vertex positions describing new identity as numVertices x 3 in column major format placed in *RIG COORDINATE SPACE*.
     * @param[out] OutStackedToScanTransforms  Stacked 4x4 transform matrices in column major format.
     * @param[out] OutStackedToScanScales  Stacked scale values. Scale is not a linear part of ToScanTransform, and needs to be applied after the
     * transformation.
     * @param[in] InNumIters  Number of iterations of the optimization.
     * @param[in] InDebugDataDirectory  The folder to save the debug data to. If not empty, saves the conformed mesh to the supplied folder as
     * face_conformed.obj
     * @returns True if fitting was successful.
     */
    bool FitPerVertex(float* OutVertexPositions,
                      float* OutStackedToScanTransforms,
                      float* OutStackedToScanScales,
                      int32_t InNumIters = 3,
                      const std::string& InDebugDataDirectory = {});

    /**
     * Fit expression using riglogic given input data.
     * @param[in] InDnaStream  Input DNA -must be RigLogic rig- to be used as deformable model to fit the expression.
     * @param[out] OutVertexPositions  Vertex positions describing new identity as numVertices x 3 in column major format placed in *RIG COORDINATE SPACE*.
     * @param[out] OutStackedToScanTransforms  Stacked 4x4 transform matrices in column major format.
     * @param[out] OutStackedToScanScales  Stacked scale values. Scale is not a linear part of ToScanTransform, and needs to be applied after the
     * transformation.
     * @param[in] InNumIters  Number of iterations of the optimization.
     * @returns True if fitting was successful.
     */
    bool FitRigLogic(dna::Reader* InDnaStream,
                     float* OutVertexPositions,
                     float* OutStackedToScanTransforms,
                     float* OutStackedToScanScales,
                     int32_t InNumIters = 3);

    /**
     * Fit expression using PCA rog given input data.
     * @param[in] InDnaStream  Input DNA -must be PCA rig- to be used as deformable model to fit the expression.
     * @param[out] OutVertexPositions  Vertex positions describing new identity as numVertices x 3 in column major format placed in *RIG COORDINATE SPACE*.
     * @param[out] OutStackedToScanTransforms  Stacked 4x4 transform matrices in column major format.
     * @param[out] OutStackedToScanScales  Stacked scale values. Scale is not a linear part of ToScanTransform, and needs to be applied after the
     * transformation.
     * @param[in] InNumIters  Number of iterations of the optimization.
     * @param[in] OptionalInNeutralVertexPositions  Optional referent neutral vertices for auto stabilization procedure.
     * @param[in] InDebugDataDirectory  The folder to save debugging data to for the teeth fitting. If not empty, saves the conformed teeth mesh
     * to the supplied folder as face_fitted.obj
     * @returns True if fitting was successful.
     */
    bool FitPcaRig(dna::Reader* InDnaStream,
                   float* OutVertexPositions,
                   float* OutStackedToScanTransforms,
                   float* OutStackedToScanScales,
                   const float* OptionalInNeutralVertexPositions = nullptr,
                   int32_t InNumIters = 3,
                   const std::string& InDebugDataDirectory = {});

    /**
     * Update teeth model and position in the rig given input data.
     * @param[out] OutVertexPositions  Vertex positions describing updated teeth fitted to the data and in *RIG COORDINATE SPACE*.
     * @param[in] InNumIters  Number of iterations of the optimization.
     * @param[in] InDebugDataDirectory  The folder to save debugging data to for the teeth fitting. If not empty, saves the conformed teeth mesh
     * to the supplied folder as teeth_conformed.obj
     * @returns True if fitting was successful.
     */
    bool FitTeeth(float* OutVertexPositions, int32_t InNumIters = 3, const std::string& InDebugDataDirectory = {});

    /**
     * Update teeth source mesh model. Important if depth regularization for *FitTeeth* is used since it will use the source mesh as reference.
     * @param[in] InVertexPositions  Vertex positions describing teeth shape in *RIG COORDINATE SPACE*.
     * @returns True if fitting was successful.
     */
    bool UpdateTeethSource(const float* InVertexPositions);

    /**
     * Update head source mesh model. Important for ActorTeethUpdateApp so that dna identity is referenced rather than generic template.
     * @param[in] InVertexPositions  Vertex positions describing head shape assuming *RIG COORDINATE SPACE* of actor dna.
     * @returns True if fitting was successful.
     */
    bool UpdateHeadSource(const float* InVertexPositions);

    /*
     * Calculate the offset in *RIG COORDINATE SPACE* to move the teeth a distance InDeltaDistanceFromCamera away from the first (1st) camera.
     * Assumes FitTeeth has been called.
     * @param[out] OutDx delta x for teeth in *RIG COORDINATE SPACE*.
     * @param[out] OutDy delta y for teeth in *RIG COORDINATE SPACE*.
     * @param[out] OutDz delta z for teeth in *RIG COORDINATE SPACE*.
     * @param[in] InDeltaDistanceFromCamera The distance in cm to move the current teeth away from the camera position.
     * @returns True if fitting was successful.
     */
    bool CalcTeethDepthDelta(float InDeltaDistanceFromCamera, float& OutDx, float& OutDy, float& OutDz);

    /**
     * Update eye model and position in the rig given input data.
     * @param[out] OutLeftEyeVertexPositions  Vertex positions describing updated teeth fitted to the data and in *RIG COORDINATE SPACE*.
     * @param[out] OutRightEyeVertexPositions  Vertex positions describing updated teeth fitted to the data and in *RIG COORDINATE SPACE*.
     * @param[in] InSetInterfaceForFaceFitting  If true, this method will also set the result of eyes fitting as eye-to-face interface for FitPerVertex.
     * @param[in] InNumIters  Number of iterations of the optimization.
     * @param[in] InAutoMode  Enable auto mode where optimization will have iterations with decreasing regularization (InNumIters then have no effect).
     * @param[in] InDebugDataDirectory  The folder to save debugging data to for the eye fitting. If not empty, saves the conformed left eye and
     * right eye meshes to the supplied folder as left_eye_conformed.obj and right_eye_conformed.obj
     * @returns True if fitting was successful.
     */
    bool FitEyes(float* OutLeftEyeVertexPositions,
                 float* OutRightEyeVertexPositions,
                 bool InSetInterfaceForFaceFitting = true,
                 int32_t InNumIters = 3,
                 const bool InAutoMode = true,
                 const std::string& InDebugDataDirectory = {});

    /**
     * Retrieve the fitting state
     * @param[out] OutStackedToScanTransforms  Stacked 4x4 transform matrices in column major format.
     * @param[out] OutStackedToScanScales  Stacked scale values. Scale is not a linear part of ToScanTransform, and needs to be applied after the
     * transformation.
     * @param[out] OutFaceMeshVertices     Current state of head mesh vertices; vertices are stored as XYZ triples. It's caller responsibility to allocate the
     * memory to the right size before calling the function.
     * @param[out] OutTeethMeshVertices    Current state of teeth mesh vertices; vertices are stored as XYZ triples. It's caller responsibility to allocate the
     * memory to the right size before calling the function.
     * @param[out] OutLeftEyeMeshVertices  Current state of eye left mesh vertices; vertices are stored as XYZ triples. It's caller responsibility to allocate
     * the memory to the right size before calling the function.
     * @param[out] OutRightEyeMeshVertices Current state of eye right mesh vertices; vertices are stored as XYZ triples. It's caller responsibility to allocate
     * the memory to the right size before calling the function.
     * @returns True if the state could be retrieved, False if there is no valid fitting state.
     */
    bool GetFittingState(float* OutStackedToScanTransforms,
                         float* OutStackedToScanScales,
                         float* OutFaceMeshVertices,
                         float* OutTeethMeshVertices,
                         float* OutLeftEyeMeshVertices,
                         float* OutRightEyeMeshVertices);

    /**
     * Get identity model type derived from configuration files.
     * @param[out] identity model type - can be COMBINED or FACE. If combined, identity model also contains teeth and eye vertices.
     * @returns regularization multiplier for non-rigid fitting.
     */
    bool GetIdentityModelType(IdentityModelType& OutIdentityModelType);

    /**
     * Check that the supplied PCA from DNA rig config is valid.
     * @param[in] InConfigurationFileOrJson  Path to pca from dna configuration file or Json string containing the config.
     * @param[in] InDnaStream  An example DNA asset to be used with the config.
     * @returns true if the config is valid, false otherwise
     */
    static bool CheckPcaModelFromDnaRigConfig(const char* InConfigurationFileOrJson, dna::Reader* InDnaStream);

    /**
     * Creates a PCA rig out of input DNA RigLogic rig. Rig is stored in dna format.
     * @param[in] InConfigurationFileOrJson  Path to pca from dna configuration file, or a Json string containing the config
     * @param[in] InDnaStream  Input DNA stream containing RigLogic rig.
     * @param[out] OutDnaStream  Output DNA stream containing PCA rig.
     * @param[in] InDebugDataDirectory If this is not empty, save the calculated pca rig as file pca_rig.dna
     * @returns True if DNA update was successful.
     */
    static bool CalculatePcaModelFromDnaRig(const char* InConfigurationFileOrJson,
                                            dna::Reader* InDnaStream,
                                            dna::Writer* OutDnaStream,
                                            const std::string& InDebugDataDirectory = "");

    /**
     * Projects brow target landmarks to fitted mesh. Outputs brows projected to mesh as mesh landmarks.
     * @param[in]  InCameraName  Which camera landmarks to use for projection.
     * @param[out] OutJsonStream  Serialized json containing all mesh landmarks.
     * @param[in]  InConcatenate  Should OutJsonStream contain just projected brow landmarks (false), or to concatenate with all mesh landmarks (true).
     * @returns True if brow landmarks are generated successfully.
     */
    bool GenerateBrowMeshLandmarks(const std::string& InCameraName, std::string& OutJsonStream, bool InConcatenate = false) const;

    /**
     * Loads fitting parameters for multiple solvers.
     * @param[in]  InJsonStream  Serialized solver configuration json
     * @returns True if loading json was successful.
     */
    bool LoadFittingConfigurations(const std::string& InJsonStream);

    /**
     * Saves fitting parameters for multiple solvers.
     * @param[out]  OutJsonStream  Serialized json containing solver configuration information
     * @returns True if saving json was successful.
     */
    bool SaveFittingConfigurations(std::string& OutJsonStream) const;

    //! Resets all input data
    bool ResetInputData();

private:
    struct Private;
    Private* m{};
};

} // namespace TITAN_API_NAMESPACE
