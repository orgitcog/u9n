// Copyright Epic Games, Inc.All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/PimplPtr.h"
#include "CameraCalibration.h"
#include "MetaHumanIdentityErrorCode.h"
#include "DNAAsset.h"

struct FFrameTrackingContourData;
struct FTrackingContour3D;

namespace UE
{
    namespace Wrappers
    {

        /**
        * Brief@ FMetaHumanConformer is a wrapper class around core tech lib that provides conformed mesh 
        * i.e. identity fitting.
        * 
        */
        class METAHUMANCORETECHLIB_API FMetaHumanConformer
        {
        public:

            FMetaHumanConformer();

            /**
             * Initialize face fitting.
             * @param[in] InTemplateDescriptionJson  the flattened templated description json
			 * @param[in] InIdentityModelJson		 the flattened identity model json (dna database description)
			 * @param[in] InFittingConfigurationJson the fitting configuration json
			 * @returns True if initialization is successful, False otherwise.
             */
            bool Init(const FString& InTemplateDescriptionJson, const FString& InIdentityModelJson, const FString& InFittingConfigurationJson);

            /**
                * Set the depth input data for one frame.
                * @param[in] InLandmarksDataPerCamera  The distorted landmarks for each camera.
                * @param[in] InDepthMaps Depthmap data per (depthmap) camera.
                * @returns True if setting the data was successful.
                *
                * @warning Fails if scan data was set before.
                */
            bool SetDepthInputData(const TMap<FString, const FFrameTrackingContourData*>& InLandmarksDataPerCamera,
                const TMap<FString, const float*>& InDepthMaps);

            /**
            * Set the scan input data.
            * @param[in] InLandmarks2DData  The distorted 2D landmarks per camera view.
            * @param[in] InLandmarks3DData  The distorted 3D landmarks.
            * @param[in] InTrianlges  Input scan data with triangles stored as numTriangles x 3  in column major format.
			* @param[in] InVertices  Input scan data with vertices as numVertices x 3 in column major format.
			* @param[out] bOutInvalidMeshTopology  bOutInvalidMeshTopology Set to false if the mesh has a valid topology, true if all 
			*			vertices on the input mesh are invalid eg due to all disconnected triangles
			* @returns True if setting the data was successful.
            *
            * @warning Fails if depthmap data was set before.
            */
            bool SetScanInputData(const TSortedMap<FString, const FFrameTrackingContourData*>& InLandmarks2DData,
                const TSortedMap<FString, const FTrackingContour3D*>& InLandmarks3DData,
                const TArray<int32_t>& InTrianlges, const TArray<float>& InVertices, bool& bOutInvalidMeshTopology);

            /**
             * Set up the cameras for fitting.
             * @param[in] InCameras  Input cameras for landmarks and depth projection.
             * @returns True if successful, False upon any error.
             */

            bool SetCameras(const TArray<FCameraCalibration>& InCalibrations);

            /**
             * Fit identity given input data.
			 * @param[out] OutVerticesFace: Vertex positions describing new identity face mesh as numVertices x 3 in column major format.
			 * @param[out] OutVerticesLeftEye: Vertex positions describing new identity left eye mesh as numVertices x 3 in column major format.
			 * @param[out] OutVerticesRightEye: Vertex positions describing new identity right eye mesh as numVertices x 3 in column major format.
			 * @param[out] Stacked rigid transforms, one for each input depth map (this will be 1 for a single mesh) containing 16 float values per transform
			 * @params[out] Stacked scales, one for each input depth map (this will be 1 value for a single mesh)
			 * @params[in] bInFitEyes, a bool if set to true, fit the eye meshes during conformation, if false, don't fit the eye meshes (which will be empty on output)
             * @params[in] InDebuggingDataFolder, a string containing a debugging folder to save to. If not empty, tries to save debugging data including all input data 
			 * (camera calib json, landmarks json, reference images, depth maps as images and objs, and also results of eye fitting as objs if this is enabled.
			 * @returns an error code which described whether the fitting was successful and what went wrong if it failed
             */
			EIdentityErrorCode FitIdentity(TArray<float>& OutVerticesFace, TArray<float>& OutVerticesLeftEye, TArray<float>& OutVerticesRightEye, TArray<float>& OutStackedToScanTransforms, TArray<float>& OutStackedToScanScales, bool bInFitEyes = false, const FString& InDebuggingDataFolder = {});

            /**
             * Update teeth model and position in the rig given input data.
			 * @param[out] Vertex positions describing updated identity face mesh as numVertices x 3 in column major format.
			 * @params[in] InDebuggingDataFolder, a string containing a debugging folder to save to. If not empty, tries to save debugging data including all input data
			 * (camera calib json, landmarks json, reference images, depth maps as images and objs, and also results of teeth fitting as obj.
			 * @returns True if fitting and DNA update was successful.
             */
            bool FitTeeth(TArray<float>& OutVerticesTeeth, const FString& InDebuggingDataFolder = {});

            /**
             * Clears previous configuration.
             * @returns True if successful.
             */
            bool ResetInputData();

			/**
			 * Projects brow target landmarks to fitted mesh. Outputs brows projected to mesh as mesh landmarks.
			 * @param[in]  InCameraName  Which camera landmarks to use for projection.
			 * @param[out] OutJsonStream  Serialized json containing all mesh landmarks.
			 * @param[in]  bInConcatenate  Should OutJsonStream contain just projected brow landmarks (false), or to concatenate with all mesh landmarks (true).
			 * @returns True if brow landmarks are generated successfully.
			 */
			bool GenerateBrowMeshLandmarks(const FString& InCameraName, TArray<uint8>& OutJsonStream, bool bInConcatenate = false) const;

			/**
			 * Creates a PCA rig out of input DNA RigLogic rig. 
			 * @param[in] InConfigurationJson  A json string containing the config to convert from DNA to PCA model
			 * @param[in] InDNA  Input DNA containing RigLogic rig.
			 * @param[out] OutPCARigMemoryBuffer  Output block of memory for the PCA rig
			 * @params[in] InDebuggingDataFolder, a string containing a debugging folder to save to. If not empty, tries to save debugging data which in this case is the fitted face mesh as face_fitted.obj
			 * @returns True if calculation of the PCA rig was successful
			 */
			static bool CalculatePcaModelFromDnaRig(const FString& InConfigurationJson, const TArray<uint8>& InDNA, TArray<uint8>& OutPCARigMemoryBuffer, const FString& InDebuggingDataFolder = {});


			/**
			 * Update the teeth source for the conformer from the supplied DNA
			 * @param[in] InDNA  Input DNA.
			 * @returns True if update happened successfully
			 */
			bool UpdateTeethSource(const TArray<uint8>& InDNA);

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
			 * Check that the supplied PCA from DNA rig config is valid.
			 * @param[in] InConfigurationJson  Path to pca from dna configuration file or Json string containing the config.
			 * @param[in] InDNAAsset  An example DNA asset to be used with the config.
			 * @returns True if the config is valid, false otherwise
			 */
			static bool CheckPcaModelFromDnaRigConfig(const FString& InConfigurationJson, UDNAAsset* InDNAAsset);

			/**
			 * Creates a PCA rig out of input DNA RigLogic rig.
			 * @param[in] InConfigurationFilename  Path to pca from dna configuration file.
			 * @param[in] InDNAFilename  Path to input DNA rig file.
			 * @param[out] OutPCARigMemoryBuffer  Output block of memory for the PCA rig
			 * @returns True if calculation of the PCA rig was successful
			 */
			static bool CalculatePcaModelFromDnaRig(const FString& InConfigurationFilename, const FString& InDNAFilename, TArray<uint8>& OutPCARigMemoryBuffer);

			/**
			  * Fit identity given input data.
			  * @param[out] OutVerticesFace  Vertex positions describing new identity as numVertices x 3 in column major format placed in *RIG COORDINATE SPACE*.
			  * @param[out] OutStackedToScanTransforms  Stacked 4x4 transform matrices in column major format.
			  * @param[out] OutStackedToScanScales  Stacked scale values. Scale is not a linear part of ToScanTransform, and needs to be applied after the transformation.
			  * @param[in] InNumIters  Number of iterations of the optimization.
			  * @returns True if fitting was successful.
			  */
			bool FitRigid(TArray<float> &OutVerticesFace, TArray<float>& OutStackedToScanTranform, TArray<float>& OutStackedToScanScale, int32 InIterations);

			/**
			 * Fit expression using PCA rog given input data.
			 * @param[in] InPcaRig  Input DNA -must be PCA rig- to be used as deformable model to fit the expression.
			 * @param[in] InNeutralDNABuffer  the DNA for the neutral pose
			 * @param[out] OutVertexPositions  Vertex positions describing new identity as numVertices x 3 in column major format placed in *RIG COORDINATE SPACE*.
			 * @param[out] OutStackedToScanTransforms  Stacked 4x4 transform matrices in column major format.
			 * @param[out] OutStackedToScanScales  Stacked scale values. Scale is not a linear part of ToScanTransform, and needs to be applied after the transformation.
			 * @params[in] InDebuggingDataFolder, a string containing a debugging folder to save to. If not empty, tries to save debugging data which in this case is the fitted face mesh as face_fitted.obj
			 * @returns True if fitting was successful.
			 */
			bool FitPcaRig(const TArray<uint8>& InPcaRig, const TArray<uint8>& InNeutralDNABuffer, TArray<float>& OutVerticesFace, TArray<float>& OutStackedToScanTranform, TArray<float>& OutStackedToScanScale, const FString& InDebuggingDataFolder = {});

			/**
			* Update teeth model and position in the rig given input data.
			* @param[in] InDNA  Input DNA.
			* @param[in] InVertices  Input target vertex positions - Teeth mesh vertices - as numVertices x 3 in column major order..
			* @param[out] OutDna  Updated dna.
			* @returns True if fitting and DNA update was successful.
			*/
			bool UpdateRigWithTeethMeshVertices(const TArray<uint8>& InDNA, const TArray<float>& InVertices, TArray<uint8>& OutDNA);

			/**
			 * Set regularization for non-rigid fitting.
			 * @param[in] regularization multiplier
			 * @returns True if successful.
			 */
			bool SetModelRegularization(float InValue);

			/**
			 * Apply the supplied delta DNA to the DNA, and then scale the output about the scaling pivot position, and return in the output buffer.
			 * @param[in] InRawDNABuffer  The input base DNA supplied as a raw block of memory.
			 * @param[in] InRawDeltaDNABuffer  The input delta DNA supplied as a raw block of memory.
			 * @param[out] OutRawCombinedUnscaledDNABuffer  Output block of memory for the combined DNA (but not scaled to metric scale).
			 * @returns True if calculation of the combined DNA rig was successful, false otherwise
			 */
			bool ApplyDeltaDNA(const TArray<uint8>& InRawDNABuffer, const TArray<uint8>& InRawDeltaDNABuffer, TArray<uint8>& OutRawCombinedUnscaledDNABuffer) const;


			/**
			 * Scale the supplied input DNA about the scaling pivot position, and return in the output buffer.
			 * @param[in] InRawDNABuffer  The input  DNA supplied as a raw block of memory.
			 * @param[in] InScale  The scaling factor to apply.
			 * @param[in] InScalingPivot  The input delta DNA supplied as a raw buffer of data.
			 * @param[out] OutRawScaledDNABuffer  Output block of memory for the scaled DNA.
			 * @returns True if calculation of the scaled DNA rig was successful, false otherwise
			 */
			bool ApplyScaleToDNA(const TArray<uint8>& InRawDNABuffer, float InScale, const FVector& InScalingPivot, TArray<uint8>& OutRawScaledDNABuffer) const;


			/**
			 * Apply the supplied rigid transform to the input DNA, and and return in the output buffer.
			 * @param[in] InRawDNABuffer  The input DNA supplied as a raw block of memory.
			 * @param[in] InTransformMatrix  The transform matrix to apply. The transform matrix should be supplied in OpenCV coordinate system.
			 * @param[out] OutTransformedDNABuffer  Output block of memory for the transformed DNA.
			 * @returns True if calculation of the transformed DNA rig was successful, false otherwise
			 */
			bool TransformRigOrigin(const TArray<uint8>& InRawDNABuffer, const FMatrix44f& InTransformMatrix, TArray<uint8>& OutTransformedDNABuffer) const;

			/**
			 * Converts a DNA asset into a byte array.
			 * @param[in] InDNAAsset  DNA asset.
			 * @returns The byte array
			 */

			static TArray<uint8> DNAToBuffer(UDNAAsset* InDNAAsset);

			/*
			* Check that the supplied json string for the controls config is valid.
			* @param[in] InControlsConfigJson: the json config defining the set of shapes which help position the teeth without intersecting the mesh
			* @returns True if controls config json is valid, false otherwise
			*/
			bool CheckControlsConfig(const FString& InControlsConfigJson) const;

			/*
			* Refine the teeth placement provided as output from the autorigging service so that they are in a better starting position
			* @param[in] InControlsConfigJson: the json config defining the set of shapes which help position the teeth without intersecting the mesh
			* @param[in] InRawDNAPlusDeltaDNABuffer: a buffer containing the raw DNA plus delta DNA from the autorigging service (unscaled)
			* @param[in] InRawDNABuffer: a buffer containing the raw DNA returned from the autorigging service (to be used as reference)
			* @param[out] OutRawRefinedDNAPlusDeltaDNABuffer: a buffer containing a refined version of InDNAPlusDeltaDNABuffer with the teeth in a better position
			* @returns True if calculation of DNA with refined teeth placement was successful, false otherwise
			*/
			bool RefineTeethPlacement(const FString& InControlsConfigJson, const TArray<uint8>& InRawDNAPlusDeltaDNABuffer, const TArray<uint8>& InRawDNABuffer, TArray<uint8>& OutRawRefinedDNAPlusDeltaDNABuffer) const;

        private:
            struct Private;
            TPimplPtr<Private> Impl;
            FCriticalSection AccessMutex;
        };
    }
}
