// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "Features/IModularFeature.h"


class IMetaHumanFaceTrackerInterface
{
public:
	virtual ~IMetaHumanFaceTrackerInterface() = default;

	virtual bool Init(const FString& InTemplateDescriptionJson, const FString& InConfigurationJson,
		const struct FTrackerOpticalFlowConfiguration& InOptFlowConfig, const FString& InPhysicalDeviceLUID) = 0;

	/** Load the DNA file. Returns: True if the DNA file could be loaded. */
	virtual bool LoadDNA(const FString& InDNAFile) = 0;
	
	/** Load the DNA from a UDNAAsset.@returns True if the DNA could be loaded. */
	virtual bool LoadDNA(class UDNAAsset* InDNAAsset) = 0;

	/** Set up the cameras for tracking. Returns: True if successful, False upon any error. */
	virtual bool SetCameras(const TArray<struct FCameraCalibration>& InCalibration) = 0;

	/** Specify the ranges for each camera. @returns True if successful, False upon error. */
	virtual bool SetCameraRanges(const TMap<FString, TPair<float, float>>& InCameraRanges) = 0;

	/**
	 * Reset and set up a new track.
	 * @param[in] InFrameStart  The first frame of the sequence.
	 * @param[in] InFrameEnd  The last (not including) frame of the sequence.
	 * @param[in] InOptFlowConfig  Optical flow configuration for tracking.
	 * @returns True if successful, False upon error.
	 */
	virtual bool ResetTrack(int32 InFrameStart, int32 InFrameEnd, const struct FTrackerOpticalFlowConfiguration& InOptFlowConfig) = 0;

	/**
	 * Specify which cameras are used for stereo reconstruction.
	 * @returns True if successful, False if the cameras have not been set up via SetCameras().
	 */
	virtual bool SetStereoCameraPairs(const TArray<TPair<FString, FString>>& InStereoReconstructionPairs) = 0;

	/**
	 * Set the current input data and performs stereo reconstruction.
	 * @param[in] InImageDataPerCamera      The distorted images per camera (only images that are used for stereo reconstruction are necessary).
	 * @param[in] InLandmarksDataPerCamera  The distorted landmarks for each camera (at least 2 cameras need to have landmarks).
	 * @param[in] InDepthmapDataPerCamera  The distorted depthmaps per depthmap camera
	 * @param[in] InLevel - reconstruction level
	 * @returns True if setting the data was successful.
	 */

	virtual bool SetInputData(const TMap<FString, const unsigned char*>& InImageDataPerCamera,
		const TMap<FString, const struct FFrameTrackingContourData*>& InLandmarksDataPerCamera,
		const TMap<FString, const float*>& InDepthmapDataPerCamera = TMap<FString, const float*>(), int32 InLevel = 0) = 0;

	virtual bool Track(int32 InFrameNumber, const TMap<FString, TPair<TPair<const float*, const float*>, TPair<TPair<const float*, const float*>, TPair<const float*, const float*>>>>& InFlowInfo = {},
				bool bUseFastSolver = false, const FString& InDebuggingDataFolder = {}, bool bSkipPredictiveSolver = false, bool bInSkipPerVertexSolve = true) = 0;

	virtual bool GetTrackingState(int32 InFrameNumber, FTransform& OutHeadPose, TArray<float>& OutHeadPoseRaw, 
				TMap<FString, float>& OutControls, TMap<FString, float>& OutRawControls, TArray<float>& OutFaceMeshVertices,
				TArray<float>& OutTeethMeshVertices, TArray<float>& OutLeftEyeMeshVertices, TArray<float>& OutRightEyeMeshVertices) = 0;

	virtual bool SetPCARig(const TArray<uint8>& InMemoryBuffer) = 0;

	virtual bool AddBrowMeshLandmarks(const FString& InBrowMeshJson) = 0;

	virtual bool TrainSolverModelsSync(const TArray<uint8>& InGlobalTeethPredictiveSolverTrainingDataMemoryBuffer,
		const TArray<uint8>& InPredictiveSolversTrainingDataMemoryBuffer) = 0;

	virtual bool GetPredictiveSolvers(TArray<uint8>& OutMemoryBufferPredictiveSolvers) = 0;

	virtual bool GetGlobalTeethPredictiveSolver(TArray<uint8>& OutMemoryBufferGlobalTeethPredictiveSolver) = 0;

	virtual bool SetPredictiveSolvers(const TArray<uint8>& InMemoryBufferPredictiveSolvers) = 0;

	virtual bool SetGlobalTeethPredictiveSolver(const TArray<uint8>& InMemoryBufferGlobalTeethPredictiveSolver) = 0;

	virtual bool EstimateScale(int32_t InFrameNumber, float& OutScale) = 0;

	virtual bool CreateFlattenedJsonStringWrapper(const FString& InSolverDefinitionsFile, FString& OutSolverDefinitionsJson) = 0;

	virtual bool LoadPredictiveSolverTrainingDataWrapper(const FString& InGlobalTeethPredictiveSolverDataFilename, const TArray<FString>& InPredictiveSolverDataFilenames,
				TArray<uint8> & OutGlobalTeethPredictiveSolverTrainingDataMemoryBuffer, TArray<uint8> & OutPredictiveSolversTrainingDataMemoryBuffer) = 0;
};

class IDepthGeneratorInterface
{
public:
	virtual ~IDepthGeneratorInterface() = default;

	virtual bool Init(const FString& InPhysicalDeviceLUID = "") = 0;

	virtual bool SetCameras(const TArray<struct FCameraCalibration>& InCalibration) = 0;

	/**
	 * Specify the ranges for each camera.
	 * @returns True if successful, False upon error.
	 */
	virtual bool SetCameraRanges(const TMap<FString, TPair<float, float>>& InCameraRanges) = 0;

	/**
	 * Specify which cameras are used for stereo reconstruction.
	 * @returns True if successful, False if the cameras have not been set up via SetCameras().
	 */
	virtual bool SetStereoCameraPairs(const TArray<TPair<FString, FString>>& InStereoReconstructionPairs) =0;


	/**
	 * Set the current input data and performs stereo reconstruction.
	 * @param[in] InImageDataPerCamera      The distorted images per camera (only images that are used for stereo reconstruction are necessary).
	 * @param[in] InLevel - reconstruction level
	 * @returns True if setting the data was successful.
	 */
	virtual bool SetInputData(const TMap<FString, const unsigned char*>& InImageDataPerCamera, int32 InLevel = 0) = 0;

	/**
	* Returns current Reconstructer state.
	*/
	virtual uint32 GetReconstructerState() = 0;

	/**
	* Gets the depth map data calculated by SetInputData
	*/
	virtual bool GetDepthMap(int32 InStereoPairIndex, int32& OutWidth, int32& OutHeight, const float*& OutDataconst, const float*& OutIntrinsics, const float*& OutExtrinsics) = 0;
};

class IDepthMapDiagnosticsInterface
{
public:
	virtual ~IDepthMapDiagnosticsInterface() = default;

	virtual bool Init(const TArray<struct FCameraCalibration>& InCalibrations) = 0;

	virtual bool CalcDiagnostics(const TMap<FString, const unsigned char*>& InImageDataPerCamera,
				const TMap<FString, const struct FFrameTrackingContourData*>& InLandmarksDataPerCamera,
				const TMap<FString, const float*>& InDepthmapDataPerCamera, TMap<FString, struct FDepthMapDiagnosticsResult> & OutDiagnostics) = 0;
};

class IOpticalFlowInterface
{
public:
	virtual ~IOpticalFlowInterface() = default;

	virtual bool Init(const FString& InConfigurationJson, const FString& InPhysicalDeviceLUID) = 0;
	
	virtual bool SetCameras(const TArray<FCameraCalibration>& InCalibrations) = 0;
	
	virtual bool CalculateFlow(const FString& InCameraName, bool bInUseConfidence, const TArray<float>& InImage0, const TArray<float>& InImage1, TArray<float>& OutFlow,
         TArray<float>& OutConfidence, TArray<float>& OutSourceCamera, TArray<float>& OutTargetCamera) = 0;

	virtual  bool ConvertImageWrapper(const TArray<uint8_t>& InBGRAImageData, int32_t InWidth, int32_t InHeight, bool bIssRGB, TArray<float>& OutGreyImage) = 0;
};

class IFaceTrackerPostProcessingInterface
{
public:
	virtual ~IFaceTrackerPostProcessingInterface() = default;

	/**
 	 * Initialize face tracking post processing class.
 	 * @param[in] InTemplateDescriptionJson: the flattened json for the template_description.json config (containing any none-json embedded objects as base64 strings)
 	 * @param[in] InConfigurationJson: the flattened json for the configuration.json config (containing any none-json embedded objects as base64 strings)
 	 * @returns True if initialization is successful, False otherwise.
 	 */
	virtual bool Init(const FString& InTemplateDescriptionJson, const FString& InConfigurationJson) = 0;
	
	/**
	 * Load the DNA file.
	 *
	 * @param[in] InDNAFile the path to the DNA file  
	 * @param[in] InSolverDefinitionsJson: the string containing the solver definitions as json; a different config will be used for the standard solve than for the hierarchical solve
	 * @returns True if the DNA file could be loaded.
	 */
	virtual bool LoadDNA(const FString& InDNAFile, const FString& InSolverDefinitionsJson) = 0;

	/**
	 * Load the DNA from a UDNAAsset.
	 *
	 * @param[in] InDNAAsset the DNA asset
	 * @param[in] InSolverDefinitionsJson: the string containing the solver definitions as json; a different config will be used for the standard solve than for the hierarchical solve
	 * @returns True if the DNA could be loaded.
	 */
	virtual bool LoadDNA(UDNAAsset* InDNAAsset, const FString& InSolverDefinitionsJson) = 0;

	/*
	 * Set the global teeth predictive solver from a memory buffer (this allows the data to be set from within a UE asset)
	 *
	 * @param[in] InMemoryBufferGlobalTeethSolver the buffer of the solver for the global teeth solve
	 * @returns True if successful
	 */
	virtual bool SetGlobalTeethPredictiveSolver(const TArray<uint8>& InMemoryBufferGlobalTeethSolver) = 0;

	/**
	 * Set up the cameras for tracking.
	 * @param[in] InCalibration An array of camera calibrations. 
	 * @param[in] InCamera The camera to use.
	 * @returns True if successful, False upon any error.
	 */
	virtual bool SetCameras(const TArray<FCameraCalibration>& InCalibration, const FString& InCamera) = 0;

	/*
	 * Convert UI controls to raw controls
	 * @param[in] InGuiControls				The GUI controls
	 * @param[out] OutRawControls			The raw controls
	 * @returns True if successful, False upon any error.
	 */
	virtual bool ConvertUIControlsToRawControls(const TMap<FString, float>& InGuiControls, TMap<FString, float>& OutRawControls) const = 0;

	/*
	 * Save the current debugging state to the specified folder and filename
	 * @param[in] InFrameNumberFirst			The first frame we performed the solve on in the class
	 * @param[in] InNumFramesToSolve			The number of frames to perform the offline global solve on
	 * @param[in] InTrackingData				The tracking data which was used during the solving steps
	 * @param[in] InFilename					The filename to save to
	 * @param[in] InDebuggingDataFolder		The folder to save to; if empty, does not save debugging states data
	 * @returns True if successful, False upon any error.
	 */
	virtual bool SaveDebuggingData(int32 InFrameNumberFirst, int32 InNumFramesToSolve, const TArray<FFrameTrackingContourData>& InTrackingData, const FString& InFilename, const FString& InDebuggingDataFolder) const = 0;

	/**
	 * Set whether to enable or disable the global teeth and eye gaze solves. Note by default global solves are enabled.
	 * @param[in] bInDisableGlobalEyeGazeAndTeethSolves: if set to true, disable the global eye gaze and teeth solves, otherwise apply these
	 */
	virtual void SetDisableGlobalSolves(bool bInDisableGlobalEyeGazeAndTeethSolves) = 0;

	/**
	 * Perform offline solve steps (Eye Gaze correction, Teeth fitting) and prepare for the frame by frame processing
	 * @param[in] InFrameNumberFirst			The first frame to perform the offline global solve on
	 * @param[in] InNumFramesToSolve			The number of frames to perform the offline global solve on
	 * @param[in] InTrackingData				The tracking data which is used during the solving steps
	 * @param[in] InOutFrameData				The frame data, which contains the current animation state and other required data (eg meshes) which 
	 * are used as priors and data for the global solves. On output the AnimationData in InOutFrameData is updated to reflect the results of the global solve steps.
	 * @param[in] InDebuggingDataFolder		The folder to save to; if empty, does not save debugging states data
	 * @returns True if successful, False upon any error.
	 */
	virtual bool OfflineSolvePrepare(int32 InFrameNumberFirst, int32 InNumFramesToSolve, const TArray<FFrameTrackingContourData>& InTrackingData,
		TArray<struct FFrameAnimationData>& InOutFrameData, const FString& InDebuggingDataFolder = {}) const = 0;

	/**
	 * Perform the slow offline solve steps (final solve based upon the corrected eye gaze and teeth)
	 * @param[in] InFrameNumber     			The frame to perform the offline global solve on
	 * @param[in] InFrameNumberFirst			The first frame to perform the offline global solve on
	 * @param[in] InNumFramesToSolve			The number of frames to perform the offline global solve on
	 * @param[in] InTrackingData				The tracking data which is used during the solving steps
	 * @param[in] InOutFrameData				The frame data, which contains the current animation state and other required data (eg meshes) which
	 * are used as priors and data for the global solves. On output the AnimationData in InOutFrameData is updated to reflect the results of the global solve steps.
	 * @returns True if successful, False upon any error.
	 */
	virtual bool OfflineSolveProcessFrame(int32 InFrameNumber, int32 InFrameNumberFirst, int32 InNumFramesToSolve,
		TArray<FFrameAnimationData>& InOutFrameData, TArray<int32>& OutUpdatedFrames) const = 0;
};

class IFaceTrackerPostProcessingFilter
{
public:
	virtual ~IFaceTrackerPostProcessingFilter() = default;

	/**
	 * Initialize face tracking post processing filter class.
	 * @param[in] InTemplateDescriptionJson:  the flattened template description config json string
	 * @param[in] InConfigurationJson: the flattened configuration json string
	 c* @returns True if initialization is successful, False otherwise.
	 */
	virtual bool Init(const FString& InTemplateDescriptionJson, const FString& InConfigurationJson) = 0;
	
	/**
	 * Load the DNA file.
	 *
	 * @param[in] InDNAFile the path to the DNA file  
	 * @param[in] InSolverDefinitions: the json string containing the solver definitions (may be from solver_definitions.json or hierarchical_solver_definitions.json)
	 * @returns True if the DNA file could be loaded.
	 */
	virtual bool LoadDNA(const FString& InDNAFile, const FString& InSolverDefinitions) = 0;

	/**
	 * Load the DNA from a UDNAAsset.
	 *
	 * @param[in] InDNAAsset the DNA asset
	 * @param[in] InSolverDefinitions: the json string containing the solver definitions (may be from solver_definitions.json or hierarchical_solver_definitions.json)
	 * @returns True if the DNA could be loaded.
	 */
	virtual bool LoadDNA(UDNAAsset* InDNAAsset, const FString& InSolverDefinitions) = 0;

	/**
	 * Perform offline filtering
	 * @param[in] InFrameNumberFirst			The first frame to perform the offline filter on
	 * @param[in] InNumFramesToFilter			The number of frames to perform the offline filter on
	 * @param[in] InOutFrameData				The frame data, which contains the current animation state. On output the AnimationData in InOutFrameData is updated to reflect the results of the global solve steps.
	 * @returns True if successful, False upon any error.
	 */
	virtual bool OfflineFilter(int32 InFrameNumberFirst, int32 InNumFramesToFilter, TArray<FFrameAnimationData>& InOutFrameData, const FString& InDebuggingDataFolder = {}) const = 0;
};

class IPredictiveSolverInterface : public IModularFeature
{
public:

	static FName GetModularFeatureName()
	{
		static const FName ModularFeatureName = FName(TEXT("PredictiveSolver"));
		return ModularFeatureName;
	}
	
	virtual ~IPredictiveSolverInterface() = default;
	using SolverProgressFunc = TFunction<void(float)>;

	virtual void TrainPredictiveSolver(std::atomic<bool>& bIsDone, std::atomic<float>& InProgress, SolverProgressFunc InOnProgress, std::atomic<bool>& bInIsCancelled,
		const struct FPredictiveSolversTaskConfig& InConfig, struct FPredictiveSolversResult& OutResult) = 0;

};

class IDepthProcessingMetadataProvider : public IModularFeature
{
public:
	virtual ~IDepthProcessingMetadataProvider() = default;
	
	static FName GetModularFeatureName()
	{
		static const FName ModularFeatureName = FName(TEXT("DepthProcessingPluginMetadata"));
		return ModularFeatureName;
	}
	
	/**
	 * List all GPU devices. Report each one using a locally unique identifier (LUID)
	 * @param[out] OutDeviceLUIDs  List of all device LUIDs.
	 * @returns True if successful, False otherwise.
	 */
	virtual bool ListPhysicalDeviceLUIDs(TArray<FString>& OutDeviceLUIDs) = 0;

	/** Returns the MeshTracker version of the Depth Processing plugin */
	virtual FString GetMeshTrackerVersionString() = 0;
};

class IFaceTrackerNodeImplFactory : public IModularFeature
{
public:
	virtual ~IFaceTrackerNodeImplFactory() = default;

	static FName GetModularFeatureName()
	{
		static const FName ModularFeatureName = FName(TEXT("FaceTrackerNodeFactory"));
		return ModularFeatureName;
	}

	virtual TSharedPtr<IMetaHumanFaceTrackerInterface> CreateFaceTrackerImplementor() = 0;
	virtual TSharedPtr<IDepthMapDiagnosticsInterface> CreateDepthMapImplementor() = 0;
	virtual TSharedPtr<IDepthGeneratorInterface> CreateDepthGeneratorImplementor() = 0;
	virtual TSharedPtr<IOpticalFlowInterface> CreateOpticalFlowImplementor() = 0;
	virtual TSharedPtr<IFaceTrackerPostProcessingInterface> CreateFaceTrackerPostProcessingImplementor() = 0;
	virtual TSharedPtr<IFaceTrackerPostProcessingFilter> CreateFaceTrackerPostProcessingFilterImplementor() = 0;

};
