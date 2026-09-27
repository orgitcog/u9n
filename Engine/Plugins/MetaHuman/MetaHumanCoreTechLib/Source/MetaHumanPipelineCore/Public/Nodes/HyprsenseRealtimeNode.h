// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HyprsenseUtils.h"

#include "Pipeline/Node.h"
#include "Pipeline/PipelineData.h"
#include "HAL/ThreadSafeBool.h"
#include "HyprsenseRealtimeNode.generated.h"

namespace UE::NNE
{
	class IModelInstanceGPU;
}

UENUM()
enum class EHyprsenseRealtimeNodeDebugImage : uint8
{
	None = 0,
	Input UMETA(DisplayName = "Input Video"),
	FaceDetect,
	Headpose,
	Trackers,
	Solver
};

UENUM()
enum class EHyprsenseRealtimeNodeState : uint8
{
	Unknown = 0,
	OK,
	NoFace,
	SubjectTooFar,
};

namespace UE::MetaHuman::Pipeline
{

class METAHUMANPIPELINECORE_API FHyprsenseRealtimeNode : public FNode, public FHyprsenseUtils
{
public:

	FHyprsenseRealtimeNode(const FString& InName);

	virtual bool Start(const TSharedPtr<FPipelineData>& InPipelineData) override;
	virtual bool Process(const TSharedPtr<FPipelineData>& InPipelineData) override;

	bool LoadModels();

	enum ErrorCode
	{
		FailedToInitialize = 0,
		FailedToDetect,
		FailedToTrack,
		FailedToSolve
	};

	void SetDebugImage(EHyprsenseRealtimeNodeDebugImage InDebugImage);
	EHyprsenseRealtimeNodeDebugImage GetDebugImage();

	void SetFocalLength(float InFocalLength);
	float GetFocalLength();

	void SetHeadStabilization(bool bInHeadStabilization);
	bool GetHeadStabilization() const;

private:

	EHyprsenseRealtimeNodeDebugImage DebugImage = EHyprsenseRealtimeNodeDebugImage::None;
	FCriticalSection DebugImageMutex;

	float FocalLength = -1;
	FCriticalSection FocalLengthMutex;

	FThreadSafeBool bHeadStabilization = true;

	TSharedPtr<UE::NNE::IModelInstanceGPU> FaceDetector = nullptr;
	TSharedPtr<UE::NNE::IModelInstanceGPU> Headpose = nullptr;
	TSharedPtr<UE::NNE::IModelInstanceGPU> Solver = nullptr;

	const uint32 HeadposeInputSizeX = 256;
	const uint32 HeadposeInputSizeY = 256;

	const uint32 SolverInputSizeX = 256;
	const uint32 SolverInputSizeY = 512;

	bool bFaceDetected = false;
	TArray<FVector2D> TrackingPoints;
	FVector HeadTranslation = FVector::ZeroVector;

	const float FaceScoreThreshold = 0.5;

	const float LandmarkAwareSmoothingThresholdInCm = 1.5f;
	TArray<FVector2D> PreviousTrackingPoints;
	FTransform PreviousTransform;
	FTransform LandmarkAwareSmooth(const TArray<FVector2D>& InTrackingPoints, const FTransform& InTransform, const float InFocalLength);

	Matrix23f GetTransformFromPoints(const TArray<FVector2D>& InPoints, const FVector2D& InSize, bool bInIsStableBox, Matrix23f& OutTransformInv) const;
};

}
