// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Nodes/HyprsenseUtils.h"
#include "Pipeline/Node.h"
#include "Pipeline/PipelineData.h"
#include "UObject/WeakObjectPtr.h"

#include "NNE.h"
#include "NNEModelData.h"
#include "NNERuntimeGPU.h"

class UNeuralNetwork;

namespace UE::MetaHuman::Pipeline
{
	class METAHUMANPIPELINE_API FHyprsenseNodeBase : public FNode, public FHyprsenseUtils
	{
	public:
		enum ErrorCode
		{
			InvalidTracker = 0,
			ModelNotLoaded,
			InvalidIOConfig,
			FailedToTrack
		};

		UENUM()
		enum class ETrackerType : uint8
		{
			FaceTracker = 0	UMETA(DisplayName = "FaceTracker"), 
			FaceDetector	UMETA(DisplayName = "FaceDetector"),
			EyebrowTracker	UMETA(DisplayName = "EyebrowTracker"),
			EyeTracker		UMETA(DisplayName = "EyeTracker"),
			LipsTracker		UMETA(DisplayName = "LipsTracker"),
			LipzipTracker	UMETA(DisplayName = "LipzipTracker"),
			NasoLabialTracker	UMETA(DisplayName = "NasoLabialTracker"),
			ChinTracker		UMETA(DisplayName = "ChinTracker"), 
			TeethTracker	UMETA(DisplayName = "TeethTracker"), 
			LipsNasoNoseTeethTracker	UMETA(DisplayName = "LipsNasoNoseTeethTracker"),
			TeethConfidenceTracker		UMETA(DisplayName = "TeethConfidenceTracker"),
		};

		struct Interval
		{
			//Defines continuous interval of indices [Start,End]
			int32 Start{};
			int32 End{};

			//Any additional indices added to continuous interval above
			TArray<int32> AdditionalIndices{};
		};

		struct NNEModelInfo
		{
			TArray<int32> Inputs{};
			TArray<int32> Outputs{};
		};

		FHyprsenseNodeBase(const FString& InTypeName, const FString& InName);

		virtual bool Start(const TSharedPtr<FPipelineData>& InPipelineData) = 0;
		virtual bool Process(const TSharedPtr<FPipelineData>& InPipelineData) = 0;

		FString GetErrorMessage() const;
		ErrorCode GetErrorCode() const;

	protected:

		enum FacePart
		{
			RightEyeBrow,
			LeftEyeBrow,
			RightEye,
			LeftEye,
			Lips,
			Lipzip,
			NasolabialNose,
			Chin,
			Teeth,
			LipsNasoNoseTeeth,
			TeethConfidence,
			Num = 11
		};

		struct PartPoints
		{
			TArray<float> Points;
		};

		Matrix23f GetTransformFromLandmarkPart(int32 InImageWidth, int32 InImageHeight, int32 InCropBoxSize, const TArray<float>& InLandmarks, float InRotation, bool bFlip, PartType InPartType);
		Matrix23f GetTransformFromLandmarkFacePart(int32 InImageWidth, int32 InImageHeight, int32 InCropBoxSize, const TArray<float>& InLandmarks, FacePart InPartName, float InRotation, bool bFlip, bool isRealtime);

		TArray<float> SelectLandmarksToCrop(const TArray<float>& InLandmarks, const TArray<int32>& InLandmarkIndices, const TArray<int32>& LandmarkRangeIdxNormal, const TArray<int32>& InLandmarkIdxRangeExtra, const TArray<int32>& InLandmarkIdxCenter, const TArray<int32>& InLandmarkIdxCenterExtra, const TArray<int32>& InLandmarkIdxExtra);

		void AddContourToOutput(const TArray<float>& InPoints, const TArray<float>& InConfidences, const TMap<FString, Interval>& InCurveMap, const TMap<FString, int32>& InLandmarkMap, FFrameTrackingContourData& OutResult);

		float GetRotationToUpright(const TArray<float>& InLandmarks);

		Bbox GetInversedBbox(Bbox& InBbox, const Matrix23f& InTransform);
		TArray<float> GetInversedPoints(float* InLandmarks, int32 InNum, const Matrix23f& InTransform);

		void InitTransformLandmark131to159();
		TArray<float> GetLandmark131to159(const TArray<float>& InLandmarks131);
		TArray<int32> Index131to159;

		bool ProcessLandmarks(const FUEImageDataType& Input, bool isRealtime, TArray<PartPoints>& OutDenseTrackerPointsPerModelInversed, PartPoints& OutSparseTrackerPointsInversed, bool bInRunSparseTrackerOnly);

		static inline TArray<float> EmptyConfidences(int32 Size)
		{
			TArray<float> Confidences;
			Confidences.Init(1.0f, Size);
			return Confidences;
		}

		bool CheckTrackers(const TMap<ETrackerType, UE::NNE::FTensorShape>& InputValidationMap, const TMap<ETrackerType, TArray<UE::NNE::FTensorShape>>& OutputValidationMap, const TMap<TSharedPtr<UE::NNE::IModelInstanceGPU>, ETrackerType>& TrackerTypeMap);

		int32 TrackerInputSizeX = 256;
		int32 TrackerInputSizeY = 256;

		TArray<int32> TrackerPartInputSizeX;
		TArray<int32> TrackerPartInputSizeY;

		TSharedPtr<UE::NNE::IModelInstanceGPU> FaceTracker{};
		TSharedPtr<UE::NNE::IModelInstanceGPU> FaceDetector{};
		TSharedPtr<UE::NNE::IModelInstanceGPU> EyebrowTracker{};
		TSharedPtr<UE::NNE::IModelInstanceGPU> EyeTracker{};
		TSharedPtr<UE::NNE::IModelInstanceGPU> LipsTracker{};
		TSharedPtr<UE::NNE::IModelInstanceGPU> LipzipTracker{};
		TSharedPtr<UE::NNE::IModelInstanceGPU> NasolabialNoseTracker{};
		TSharedPtr<UE::NNE::IModelInstanceGPU> ChinTracker{};
		TSharedPtr<UE::NNE::IModelInstanceGPU> TeethTracker{};
		TSharedPtr<UE::NNE::IModelInstanceGPU> LipsNasoNoseTeethTracker{};
		TSharedPtr<UE::NNE::IModelInstanceGPU> TeethConfidenceTracker{};

		TArray<TArray<float>> FaceTrackerOutputData;
		TArray<TArray<float>> FaceDetectorOutputData;
		TArray<TArray<float>> EyebrowTrackerOutputData;
		TArray<TArray<float>> EyeTrackerOutputData;
		TArray<TArray<float>> LipsTrackerOutputData;
		TArray<TArray<float>> LipzipTrackerOutputData;
		TArray<TArray<float>> NasolabialNoseTrackerOutputData;
		TArray<TArray<float>> ChinTrackerOutputData;
		TArray<TArray<float>> TeethTrackerOutputData;
		TArray<TArray<float>> LipsNasoNoseTeethTrackerOutputData;
		TArray<TArray<float>> TeethConfidenceTrackerOutputData;

		FString ErrorMessage{ "Not initialized." };
		ErrorCode EErrorCode{};

		bool bIsInitialized{};
		bool bIsFaceDetected{};

		TArray<TSharedPtr<UE::NNE::IModelInstanceGPU>> NNEModels = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };

		// To make right part of eye/eyebrow to "left-looking" to make an input for partwise tracker.
		const bool ImageFlipPart[FacePart::Num] = { true, false, true , false, false, false, false, false, false, false, false };

		//for separating and merging left/right part
		const bool CombineDataPart[FacePart::Num] = { false, true, false, true, false, false, false, false, false, false, false };

		//if the output of the model is the score rather than points, it doesn't need to be translated to resolution
		const bool isScore[FacePart::Num] = { false, false, false, false, false, false, false, false, false, false, true };

		TArray<bool> ProcessPart;

		const float FaceScoreThreshold = 0.5f;
		const int InvalidMarker = -1;
		bool bIsTrackerSetToGPU = false;
		Matrix23f LastTransform;

		const TMap<FString, Interval> CurveLipMap = { {FString("crv_lip_upper_outer_r"), {24,0}},  {FString("crv_lip_philtrum_r"), {31,24}},
									  {FString("crv_lip_philtrum_l"), {31,38}},    {FString("crv_lip_upper_outer_l"), {38,62}},
									  {FString("crv_lip_lower_outer_l"), {90,62}}, {FString("crv_lip_lower_outer_r"), {90,117,{0}}},//{0} is additional discrete index
									  {FString("crv_lip_upper_inner_r"), { 142,118,{0} }}, { FString("crv_lip_upper_inner_l"), {142,166,{62}}},
									  {FString("crv_lip_lower_inner_l"), { 191,167,{62}}}, { FString("crv_lip_lower_inner_r"), {191,215,{0}}} };

		const TMap<FString, int32> LandmarkLipMap = { {FString("pt_lip_lower_inner_m"), 191},  {FString("pt_lip_lower_outer_m"), 90},
													  {FString("pt_lip_philtrum_r"), 24},    {FString("pt_lip_philtrum_l"), 38},
													  {FString("pt_lip_upper_inner_m"), 142}, {FString("pt_lip_upper_outer_m"), 31},
													  {FString("pt_mouth_corner_r"), 0}, { FString("pt_mouth_corner_l"), 62} };

		const TMap<FString, Interval> CurveNasolabMap = { { FString("crv_nasolabial_r"), {0,24} }, { FString("crv_nasolabial_l"), {25,49} } };

		const TMap<FString, int32> LandmarkNasolabMap = { {FString("pt_naso_upper_r"), 0},  {FString("pt_naso_lower_r"), 24},
														  {FString("pt_naso_upper_l"), 25},    {FString("pt_naso_lower_l"), 49} };

		const TMap<FString, Interval> CurveNoseMap = { { FString("crv_nose_r"), {24,0} }, { FString("crv_nose_l"), {24,48} } };

		const TMap<FString, int32> LandmarkNoseMap = { {FString("pt_nose_r"), 0},  {FString("pt_nose_m"), 24}, {FString("pt_nose_l"), 48} };

		const TMap<FString, Interval> CurveChinMap = { { FString("crv_chin_r"), {24,0} }, { FString("crv_chin_l"), {24,48} } };

		const TMap<FString, int32> LandmarkChinMap = { {FString("pt_chin_r"), 0},  {FString("pt_chin_m"), 24}, {FString("pt_chin_l"), 48} };

		const TMap<FString, Interval> CurveLipzipMap = { };

		const TMap<FString, int32> LandmarkLipzipMap = { {FString("pt_right_contact"), 0},  {FString("pt_left_contact"), 1} };

		const TMap<FString, Interval> CurveTeethMap = { };

		const TMap<FString, int32> LandmarkTeethMap = { {FString("pt_tooth_upper"), 0},  {FString("pt_tooth_lower"), 1},
			{FString("pt_tooth_upper_2"), 2}, {FString("pt_tooth_lower_2"), 3} };

		const TMap<FString, Interval> CurveEyeIrisMap = { {FString("crv_eyelid_upper_r"), {19,0}},  {FString("crv_eyelid_lower_r"), {19,37, {0} }},
														  {FString("crv_iris_r"), {63,38}}, {FString("crv_eyelid_upper_l"), {83,64}},
														  {FString("crv_eyelid_lower_l"), { 83, 101, {64} }}, { FString("crv_iris_l"), {127,102}} };

		const TMap<FString, int32> LandmarkEyeIrisMap = { {FString("pt_eye_corner_inner_r"), 19},  {FString("pt_eye_corner_inner_l"), 83},
														  {FString("pt_eye_corner_outer_r"), 0}, {FString("pt_eye_corner_outer_l"), 64},
														  {FString("pt_iris_top_r"), 38}, { FString("pt_iris_top_l"), 102} };

		const TMap<FString, Interval> CurveBrowMap = { {FString("crv_brow_upper_r"), {24,0}},  {FString("crv_brow_lower_r"), {29,47,{0}}},
													   {FString("crv_brow_intermediate_r"), {24,29}},  {FString("crv_brow_intermediate_l"), {72,77}},
													   {FString("crv_brow_upper_l"), {72,48}}, {FString("crv_brow_lower_l"), {77,95,{48}}} };

		const TMap<FString, int32> LandmarkBrowMap = { {FString("pt_brow_inner_r"), 24}, {FString("pt_brow_inner_l"), 72},
													   {FString("pt_brow_intermediate_r"), 29}, {FString("pt_brow_intermediate_l"), 77},
													   {FString("pt_brow_outer_r"), 0},  {FString("pt_brow_outer_l"), 48} };
		
		const TMap<FString, Interval> CurveSparseTrackerMap = { { FString("crv_sparse_chin_r"), {8,0} }, { FString("crv_sparse_chin_l"), {8, 16} }, 
			{ FString("crv_sparse_brow_upper_r"), {25,17} }, { FString("crv_sparse_brow_lower_r"), {26,33, {17}} }, { FString("crv_sparse_brow_intermediate_r"), {25, 26} },
			{ FString("crv_sparse_brow_upper_l"), {42,34} }, { FString("crv_sparse_brow_lower_l"), {43,50, {34}} }, { FString("crv_sparse_brow_intermediate_l"), {42, 43} }, 
			{ FString("crv_sparse_lip_upper_outer_r"), {100, 94}  }, { FString("crv_sparse_lip_upper_outer_l"), {100, 106}  },
			{ FString("crv_sparse_lip_upper_inner_r"), {121, 118, {94}} }, { FString("crv_sparse_lip_upper_inner_l"), {121, 124, {106}} },
			{ FString("crv_sparse_lip_lower_outer_r"), {112, 117, {94}} }, { FString("crv_sparse_lip_lower_outer_l"), {112, 106} },
			{ FString("crv_sparse_lip_lower_inner_r"), {128, 131, {94}} }, { FString("crv_sparse_lip_lower_inner_l"), {128, 125, {106}} },
			{ FString("crv_sparse_nasolabial_r"), {154, 151, {59}} }, { FString("crv_sparse_nasolabial_l"), {158, 155, {65}} },
			{ FString("crv_sparse_nose_r"), {55, 62} }, { FString("crv_sparse_nose_l"), {69, 62} }, { FString("crv_sparse_nose_m"), {51, 54, {62}} },
			{ FString("crv_sparse_eyelid_upper_r"), {76,70}},  {FString("crv_sparse_eyelid_lower_r"), {76,81, {70} }},
			{ FString("crv_sparse_iris_r"), {133,140}}, {FString("crv_sparse_eyelid_upper_l"), {88,82}},
			{ FString("crv_sparse_eyelid_lower_l"), { 88, 93, {82} }}, { FString("crv_sparse_iris_l"), {142,149}}

		};  
		
		const TMap<FString, int32> LandmarkSparseTrackerMap = { {FString("pt_sparse_chin_r"), 0 }, {FString("pt_sparse_chin_m"), 8 }, {FString("pt_sparse_chin_l"), 16 },
			{ FString("pt_sparse_brow_inner_r"), 25 }, { FString("pt_sparse_brow_inner_l"), 42 },
			{ FString("pt_sparse_brow_intermediate_r"), 26 }, {FString("pt_sparse_brow_intermediate_l"), 43 },
			{ FString("pt_sparse_brow_outer_r"), 17 }, { FString("pt_sparse_brow_outer_l"), 34 },
			{ FString("pt_sparse_eye_corner_inner_r"), 76 },  { FString("pt_sparse_eye_corner_inner_l"), 88 },
			{ FString("pt_sparse_eye_corner_outer_r"), 70 }, { FString("pt_sparse_eye_corner_outer_l"), 82 },
			{ FString("pt_sparse_pupil_r"), 141}, { FString("pt_sparse_pupil_l"), 150 },
			{ FString("pt_sparse_nose_upper_m"), 51 }, { FString("pt_sparse_nose_lower_m"), 62 },
			{ FString("pt_sparse_naso_upper_r"), 59 }, { FString("pt_sparse_naso_lower_r"), 154 },
			{ FString("pt_sparse_naso_upper_l"), 65 }, { FString("pt_sparse_naso_lower_l"), 158 },
			{ FString("pt_sparse_lip_lower_inner_m"), 128 }, { FString("pt_sparse_lip_lower_outer_m"), 112 },
			{ FString("pt_sparse_lip_upper_inner_m"), 121 }, { FString("pt_sparse_lip_upper_outer_m"), 100 },
			{ FString("pt_sparse_mouth_corner_r"), 94 }, { FString("pt_sparse_mouth_corner_l"), 106 },
			{ FString("pt_sparse_mouth_tongue_m"), 132 }
		};

        const TMap <ETrackerType, FString> TrackerNames = {
            { ETrackerType::FaceTracker, TEXT("FaceTracker") },
            { ETrackerType::FaceDetector, TEXT("FaceDetector") },
            { ETrackerType::EyebrowTracker, TEXT("EyebrowTracker") },
            { ETrackerType::EyeTracker, TEXT("EyeTracker") },
            { ETrackerType::LipsTracker, TEXT("LipsTracker") },
            { ETrackerType::LipzipTracker, TEXT("LipzipTracker") },
            { ETrackerType::NasoLabialTracker, TEXT("NasoLabialTracker") },
            { ETrackerType::ChinTracker, TEXT("ChinTracker") },
            { ETrackerType::TeethTracker, TEXT("TeethTracker") },
            { ETrackerType::LipsNasoNoseTeethTracker, TEXT("LipsNasoNoseTeethTracker") },
            { ETrackerType::TeethConfidenceTracker, TEXT("FaceTeethConfidenceTracker") },
        };
	};

}
