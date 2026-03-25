// Copyright Epic Games, Inc.All Rights Reserved.

#include "HMCArchiveIngest.h"
#include "MetaHumanCaptureSourceLog.h"
#include "ImageSequenceUtils.h"
#include "FramePathResolver.h"

#include "Misc/Paths.h"
#include "HAL/FileManager.h"

#include "Nodes/ImageUtilNodes.h"
#include "Nodes/FaceTrackerNode.h"
#include "Nodes/AsyncNode.h"
#include "HAL/IConsoleManager.h"

#define LOCTEXT_NAMESPACE "FootageIngest"

namespace
{
	TAutoConsoleVariable<bool> CVarForceSingleThreadedStereoReconstruction
	{
		TEXT("mh.CaptureSource.ForceSingleThreadedStereoReconstruction"),
		false,
		TEXT("Forces stereo reconstruction to run in a single thread of execution during stero HMC ingest."),
		ECVF_Default
	};
}

FHMCArchiveIngest::FHMCArchiveIngest(const FString& InInputDirectory, 
									 bool bInShouldCompressDepthFiles, 
									 bool bInCopyImagesToProject,
									 TRange<float> InDepthDistance,
									 EMetaHumanCaptureDepthPrecisionType InDepthPrecision,
									 EMetaHumanCaptureDepthResolutionType InDepthResolution)
	: FStereoReconstructionSystemIngest(InInputDirectory, bInShouldCompressDepthFiles, bInCopyImagesToProject, InDepthDistance, InDepthPrecision, InDepthResolution)
{
	CameraCount = 2;
	Type = TEXT("HMC");
}

FHMCArchiveIngest::~FHMCArchiveIngest() = default;

TResult<void, FMetaHumanCaptureError> FHMCArchiveIngest::IngestFiles(const FStopToken& InStopToken,
																	 const FMetaHumanTakeInfo& InTakeInfo,
																	 const FCubicTakeInfo& InCubicTakeInfo,
																	 const FCameraContextMap& InCameraContextMap,
																	 const TMap<FString, FCubicCameraInfo>& InTakeCameraCalibrations,
                                                                     FCameraCalibration& OutDepthCameraCalibration)
{
	TArray<FText> Warnings;

	const FString BasePath = TargetIngestBaseDirectory / InTakeInfo.OutputDirectory;

	TArray<TSharedPtr<UE::MetaHuman::Pipeline::FUEImageLoadNode>> Load;

	int32 DepthGenerateNodeCount = 2; // by default the number of depth generation nodes is 2; running 2 frames in parallel
	for (const TPair<FString, FCubicCameraInfo>& Elem : InTakeCameraCalibrations)
	{
		const int32 ImageRes = Elem.Value.Calibration.ImageSize.X * Elem.Value.Calibration.ImageSize.Y;
		if (ImageRes > MaxStandardHMCImageSize)
		{
			DepthGenerateNodeCount = 1; // large images; perform stereo reconstruction one frame at a time otherwise can cause crash if run out of graphics memory
			UE_LOG(LogMetaHumanCaptureSource, Warning, TEXT("Image resolution of %i is larger than the expected maximum size for the MetaHuman plugin (1536 x 2048). Image sequence ingest will be slow and may run out of graphics memory."),
				ImageRes);
			break;
		}
		if (CVarForceSingleThreadedStereoReconstruction.GetValueOnAnyThread())
		{
			DepthGenerateNodeCount = 1; // force single threaded stereo reconstruction
			UE_LOG(LogMetaHumanCaptureSource, Warning, TEXT("CVar mh.CaptureSource.ForceSingleThreadedStereoReconstruction is set; stereo HMC reconstruction will be run in a single thread of execution."));
			break;
		}
	}

	TSharedPtr<UE::MetaHuman::Pipeline::FAsyncNode<UE::MetaHuman::Pipeline::FDepthGenerateNode>> GenerateDepths =
		Pipeline.MakeNode<UE::MetaHuman::Pipeline::FAsyncNode<UE::MetaHuman::Pipeline::FDepthGenerateNode>>(DepthGenerateNodeCount, TEXT("GenerateDepths"));
	TSharedPtr<UE::MetaHuman::Pipeline::FDepthResizeNode> ResizeDepth;
	TSharedPtr<UE::MetaHuman::Pipeline::FDepthQuantizeNode> QuantizeDepth;
	TSharedPtr<UE::MetaHuman::Pipeline::FAsyncNode<UE::MetaHuman::Pipeline::FDepthSaveNode>> SaveDepths =
		Pipeline.MakeNode<UE::MetaHuman::Pipeline::FAsyncNode<UE::MetaHuman::Pipeline::FDepthSaveNode>>(DepthSaveNodeCount, TEXT("SaveDepths"));
	TArray<TSharedPtr<UE::MetaHuman::Pipeline::FCopyImagesNode>> Copy;

	int32 NodeIndex = 0;

	for (const TPair<FString, FCubicCameraInfo>& Elem : InTakeCameraCalibrations)
	{
		FString LoadNodeName = FString::Format(TEXT("Load{0}"), { NodeIndex });
		TSharedPtr<UE::MetaHuman::Pipeline::FUEImageLoadNode> LoadNode = Pipeline.MakeNode<UE::MetaHuman::Pipeline::FUEImageLoadNode>(LoadNodeName);

		UE::MetaHuman::FFrameNumberTransformer FrameNumberTransformer(InCameraContextMap[Elem.Key].FrameOffset);
		LoadNode->FramePathResolver = MakeUnique<UE::MetaHuman::FFramePathResolver>(InCameraContextMap[Elem.Key].FramesPath, MoveTemp(FrameNumberTransformer));

		Load.Add(MoveTemp(LoadNode));

		for (TSharedPtr<UE::MetaHuman::Pipeline::FDepthGenerateNode> GenerateDepth : GenerateDepths->GetNodes())
		{
			GenerateDepth->Calibrations.Add(Elem.Value.Calibration);
		}

		if (bCopyImagesToProject)
		{
			const FString FramePath = BasePath / Elem.Key;

			FString CopyNodeName = FString::Format(TEXT("Copy{0}"), { NodeIndex });
			TSharedPtr<UE::MetaHuman::Pipeline::FCopyImagesNode> CopyNode = Pipeline.MakeNode<UE::MetaHuman::Pipeline::FCopyImagesNode>(CopyNodeName);

			CopyNode->InputFilePath = InCameraContextMap[Elem.Key].FramesPath;
			CopyNode->OutputDirectoryPath = FramePath;
			CopyNode->FrameNumberOffset = InCameraContextMap[Elem.Key].FrameOffset;

			Copy.Add(MoveTemp(CopyNode));
		}

		++NodeIndex;
	}

	for (TSharedPtr<UE::MetaHuman::Pipeline::FDepthGenerateNode> GenerateDepth : GenerateDepths->GetNodes())
	{
		GenerateDepth->DistanceRange = DepthDistance;
	}

	const FString DepthDirectory = BasePath / TEXT("Depth");

	for (TSharedPtr<UE::MetaHuman::Pipeline::FDepthSaveNode> SaveDepthNode : SaveDepths->GetNodes())
	{
		SaveDepthNode->FilePath = DepthDirectory / TEXT("%06d.exr");
		SaveDepthNode->FrameNumberOffset = 1; // Always start from 1 saved frames
		SaveDepthNode->bShouldCompressFiles = bShouldCompressDepthFiles;
	}

	if (bCopyImagesToProject)
	{
		Pipeline.MakeConnection(Load[0], Copy[0]);
		Pipeline.MakeConnection(Load[1], Copy[1]);

		Pipeline.MakeConnection(Copy[0], GenerateDepths, 0, 0);
		Pipeline.MakeConnection(Copy[1], GenerateDepths, 0, 1);
	}
	else
	{
		Pipeline.MakeConnection(Load[0], GenerateDepths, 0, 0);
		Pipeline.MakeConnection(Load[1], GenerateDepths, 0, 1);
	}
	
	TSharedPtr<UE::MetaHuman::Pipeline::FNode> PreviousNode = GenerateDepths;

	if (DepthResolution != EMetaHumanCaptureDepthResolutionType::Full)
	{
		ResizeDepth = Pipeline.MakeNode<UE::MetaHuman::Pipeline::FDepthResizeNode>("Resize");

		switch (DepthResolution)
		{
		case EMetaHumanCaptureDepthResolutionType::Half:
			ResizeDepth->Factor = 2;
			break;

		case EMetaHumanCaptureDepthResolutionType::Quarter:
			ResizeDepth->Factor = 4;
			break;

		default:
			check(false);
		}

		Pipeline.MakeConnection(PreviousNode, ResizeDepth);
		PreviousNode = ResizeDepth;
	}

	if (DepthPrecision != EMetaHumanCaptureDepthPrecisionType::Full)
	{
		QuantizeDepth = Pipeline.MakeNode<UE::MetaHuman::Pipeline::FDepthQuantizeNode>("Quantize");

		switch (DepthPrecision)
		{
		case EMetaHumanCaptureDepthPrecisionType::Eightieth:
			QuantizeDepth->Factor = 80;
			break;

		default:
			check(false);
		}

		Pipeline.MakeConnection(PreviousNode, QuantizeDepth);
		PreviousNode = QuantizeDepth;
	}

	Pipeline.MakeConnection(PreviousNode, SaveDepths);

	int32 TheadsRequiredForParallelProcessing = DepthGenerateNodeCount +
												DepthSaveNodeCount +
												2 * InCubicTakeInfo.CameraMap.Num() + 2; /* 1 thread per camera for image save and load nodes + 2 internal nodes*/
	const bool bShouldRunMultiThreaded = FTaskGraphInterface::Get().GetNumBackgroundThreads() >= TheadsRequiredForParallelProcessing;
	if (!bShouldRunMultiThreaded)
	{
		UE_LOG(LogMetaHumanCaptureSource, Warning, TEXT("Not enough background threads available: required %i, available %i. The HMC ingest pipeline is going to run on a single thread"),
			TheadsRequiredForParallelProcessing, FTaskGraphInterface::Get().GetNumBackgroundThreads());
	}
	TResult<void, FMetaHumanCaptureError> PipelineResult = RunPipeline(InStopToken, InTakeInfo.Id, bShouldRunMultiThreaded);
	if (PipelineResult.IsError())
	{
		return PipelineResult.ClaimError();
	}
	
	OutDepthCameraCalibration = GenerateDepths->GetNodes()[0]->Calibrations[1];

	OutDepthCameraCalibration.CameraId = "Depth";
	OutDepthCameraCalibration.CameraType = FCameraCalibration::Depth;

	if (ResizeDepth.IsValid())
	{
		float OrigImageSize = OutDepthCameraCalibration.ImageSize.X;

		OutDepthCameraCalibration.ImageSize.X /= ResizeDepth->Factor;
		OutDepthCameraCalibration.ImageSize.Y /= ResizeDepth->Factor;
		OutDepthCameraCalibration.PrincipalPoint.X /= ResizeDepth->Factor;
		OutDepthCameraCalibration.PrincipalPoint.Y /= ResizeDepth->Factor;

		float FocalScale = OrigImageSize / OutDepthCameraCalibration.ImageSize.X;

		OutDepthCameraCalibration.FocalLength.X /= FocalScale;
		OutDepthCameraCalibration.FocalLength.Y /= FocalScale;

		if (OutDepthCameraCalibration.ImageSize.X < 640 || OutDepthCameraCalibration.ImageSize.Y < 360) // depth image is orientated on its side 
		{
			FText Warning = 
				FText::Format(LOCTEXT("LowDepthResWarning", "Resized depth image has low resolution {0}x{1}"), OutDepthCameraCalibration.ImageSize.X, OutDepthCameraCalibration.ImageSize.Y);
			UE_LOG(LogMetaHumanCaptureSource, Warning, TEXT("%s"), *Warning.ToString());
			Warnings.Add(MoveTemp(Warning));
		}
	}

	if (!Warnings.IsEmpty())
	{
		FString Message;

		for (const FText& Warning : Warnings)
		{
			Message += LINE_TERMINATOR + Warning.ToString();
		}

		return FMetaHumanCaptureError(EMetaHumanCaptureError::Warning, MoveTemp(Message));
	}

	return ResultOk;
}

#undef LOCTEXT_NAMESPACE