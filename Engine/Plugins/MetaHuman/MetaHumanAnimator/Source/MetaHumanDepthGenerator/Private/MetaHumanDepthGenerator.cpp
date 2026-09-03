// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanDepthGenerator.h"

#include "CaptureData.h"
#include "TrackingPathUtils.h"
#include "MetaHumanCaptureSource.h"
#include "ImageSequenceUtils.h"
#include "ImgMediaSource.h"
#include "LensFile.h"
#include "Misc/Paths.h"
#include "IAssetTools.h"
#include "AssetToolsModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Features/IModularFeatures.h"
#include "FileHelpers.h"

#include "Models/SphericalLensModel.h"

#include "Pipeline/Pipeline.h"
#include "Nodes/ImageUtilNodes.h"
#include "Nodes/FaceTrackerNode.h"
#include "Nodes/AsyncNode.h"
#include "MetaHumanFaceTrackerInterface.h"

#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "OpenCVHelperLocal.h"

#include "Widgets/SMetaHumanGenerateDepthWindow.h"
#include "FramePathResolver.h"

#include "PackageTools.h"
#include "HAL/FileManager.h"
#include "Internationalization/Regex.h"

#define LOCTEXT_NAMESPACE "MetaHumanDepthGenerator"

DEFINE_LOG_CATEGORY_STATIC(LogMetaHumanDepthGeneration, Log, All);

static constexpr int32 DepthSaveNodeCount = 4;
static constexpr int32 MaxStandardHMCImageSize = 3145728; // Technoprops resolution = 1536 * 2048

namespace UE::MetaHuman::Private
{

int32 GetResizeDepthFactor(EMetaHumanCaptureDepthResolutionType InDepthResolution)
{
	switch (InDepthResolution)
	{
		case EMetaHumanCaptureDepthResolutionType::Half:
			return 2;
		case EMetaHumanCaptureDepthResolutionType::Quarter:
			return 4;
		default:
			break;
	}
	return 1;
}

class FDepthGenerator
{
public:

	struct FParameters
	{
		const FString DepthDirectory;
		bool bShouldCompressDepthFiles;
		EMetaHumanCaptureDepthPrecisionType DepthPrecision;
		EMetaHumanCaptureDepthResolutionType DepthResolution;
		TRange<float> DepthDistance;
	};

	enum EDepthGenerationError
	{
		PipelineError = 0,
		ImageLoadError
	};

	FDepthGenerator(FParameters InParameters);

	TValueOrError<FCameraCalibration, EDepthGenerationError> 
		RunGenerateDepthImagesPipeline(const UFootageCaptureData* InFootageCaptureData, const UCameraCalibration* InCameraCalibration);

private:

	FParameters Parameters;
};

FDepthGenerator::FDepthGenerator(FParameters InParameters)
	: Parameters(MoveTemp(InParameters))
{
}

TValueOrError<FCameraCalibration, FDepthGenerator::EDepthGenerationError> 
FDepthGenerator::RunGenerateDepthImagesPipeline(const UFootageCaptureData* InFootageCaptureData, const UCameraCalibration* InCameraCalibration)
{
	using namespace UE::MetaHuman::Pipeline;

	// by default the number of depth generation nodes is 2; running 2 frames in parallel
	int32 DepthGenerateNodeCount = 2;

	FIntVector2 ImageDimensions;
	int32 NumImageFrames = 0;
	int32 View = 0;
	FString FullSequencePath = InFootageCaptureData->ImageSequences[View]->GetFullPath();

	if (FImageSequenceUtils::GetImageSequenceInfoFromAsset(InFootageCaptureData->ImageSequences[View], ImageDimensions, NumImageFrames))
	{
		UE_LOG(LogMetaHumanDepthGeneration, Display, TEXT("Detected image resolution %i x %i for first image found at %s"), ImageDimensions.X, ImageDimensions.Y, *FullSequencePath);
	}
	else
	{
		UE_LOG(LogMetaHumanDepthGeneration, Error, TEXT("Could not determine image resolution. Failed to load first image file found at %s. Depth generation aborted."), *FullSequencePath);
		return MakeError(EDepthGenerationError::ImageLoadError);
	}

	const int32 ImageResolution = ImageDimensions.X * ImageDimensions.Y;
	if (ImageResolution > MaxStandardHMCImageSize)
	{
		// large images; perform stereo reconstruction one frame at a time otherwise can cause crash if run out of graphics memory
		DepthGenerateNodeCount = 1;
		UE_LOG(LogMetaHumanDepthGeneration, Warning, TEXT("Image resolution of %i is larger than the expected maximum size for the MetaHuman plugin (1536 x 2048). Image sequence ingest will be slow and may run out of graphics memory."), ImageResolution);
	}

	FPipeline Pipeline;

	TArray<TSharedPtr<FUEImageLoadNode>> Load;
	TSharedPtr<FAsyncNode<FDepthGenerateNode>> GenerateDepths = Pipeline.MakeNode<FAsyncNode<FDepthGenerateNode>>(DepthGenerateNodeCount, TEXT("GenerateDepths"));
	TSharedPtr<FDepthResizeNode> ResizeDepth;
	TSharedPtr<FDepthQuantizeNode> QuantizeDepth;
	TSharedPtr<FAsyncNode<FDepthSaveNode>> SaveDepths = Pipeline.MakeNode<FAsyncNode<FDepthSaveNode>>(DepthSaveNodeCount, TEXT("SaveDepths"));

	TArray<FCameraCalibration> CameraCalibrations;
	TArray<TPair<FString, FString>> StereoReconstructionPairs;

	InCameraCalibration->ConvertToTrackerNodeCameraModels(CameraCalibrations, StereoReconstructionPairs);

	int32 NumFrames = 0;
	for (int32 ViewIndex = 0; ViewIndex < InFootageCaptureData->ImageSequences.Num(); ViewIndex++)
	{
		FString LoadNodeName = FString::Format(TEXT("Load{0}"), { ViewIndex });
		TSharedPtr<FUEImageLoadNode> LoadNode = Pipeline.MakeNode<FUEImageLoadNode>(LoadNodeName);

		FString ImageFilePath;
		int32 FrameOffset = 0;
		
		FTrackingPathUtils::GetTrackingFilePathAndInfo(InFootageCaptureData->ImageSequences[ViewIndex], ImageFilePath, FrameOffset, NumFrames);

		FFrameNumberTransformer FrameNumberTransformer(FrameOffset);
		LoadNode->FramePathResolver = MakeUnique<FFramePathResolver>(ImageFilePath, MoveTemp(FrameNumberTransformer));

		Load.Add(MoveTemp(LoadNode));

		for (const TSharedPtr<FDepthGenerateNode>& GenerateDepth : GenerateDepths->GetNodes())
		{
			GenerateDepth->Calibrations.Add(CameraCalibrations[ViewIndex]);
		}
	}

	for (const TSharedPtr<FDepthGenerateNode>& GenerateDepth : GenerateDepths->GetNodes())
	{
		GenerateDepth->DistanceRange = Parameters.DepthDistance;
	}

	for (const TSharedPtr<FDepthSaveNode>& SaveDepthNode : SaveDepths->GetNodes())
	{
		SaveDepthNode->FilePath = Parameters.DepthDirectory / TEXT("%06d.exr");
		// Always start from 1 saved frames
		SaveDepthNode->FrameNumberOffset = 1;
		SaveDepthNode->bShouldCompressFiles = Parameters.bShouldCompressDepthFiles;
	}

	Pipeline.MakeConnection(Load[0], GenerateDepths, 0, 0);
	Pipeline.MakeConnection(Load[1], GenerateDepths, 0, 1);

	TSharedPtr<FNode> PreviousNode = GenerateDepths;

	if (Parameters.DepthResolution != EMetaHumanCaptureDepthResolutionType::Full)
	{
		ResizeDepth = Pipeline.MakeNode<FDepthResizeNode>("Resize");
		ResizeDepth->Factor = GetResizeDepthFactor(Parameters.DepthResolution);

		Pipeline.MakeConnection(PreviousNode, ResizeDepth);
		PreviousNode = ResizeDepth;
	}

	if (Parameters.DepthPrecision != EMetaHumanCaptureDepthPrecisionType::Full)
	{
		QuantizeDepth = Pipeline.MakeNode<FDepthQuantizeNode>("Quantize");

		switch (Parameters.DepthPrecision)
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

	/* 1 thread per camera for image save and load nodes + 2 internal nodes*/
	const int32 TheadsRequiredForParallelProcessing = 
		DepthGenerateNodeCount + 
		DepthSaveNodeCount +
		2 * InFootageCaptureData->ImageSequences.Num() + 2;

	const bool bShouldRunMultiThreaded = FTaskGraphInterface::Get().GetNumBackgroundThreads() >= TheadsRequiredForParallelProcessing;
	if (!bShouldRunMultiThreaded)
	{
		UE_LOG(LogMetaHumanDepthGeneration, Warning, TEXT("Not enough background threads available: required %i, available %i. The HMC ingest pipeline is going to run on a single thread"),
			TheadsRequiredForParallelProcessing, FTaskGraphInterface::Get().GetNumBackgroundThreads());
	}
	
	FScopedSlowTask DepthGenerationProgress(NumFrames, LOCTEXT("Generating_Depth", "Generating Depth..."));
	DepthGenerationProgress.MakeDialog(true);

	FPipelineRunParameters PipelineRunParameters;
	if (bShouldRunMultiThreaded)
	{
		PipelineRunParameters.SetMode(EPipelineMode::PushSyncNodes);
	}
	else
	{
		PipelineRunParameters.SetMode(EPipelineMode::PushSync);
	}

	PipelineRunParameters.SetRestrictStartingToGameThread(false);

	FFrameComplete OnFrameComplete;
	OnFrameComplete.AddLambda([&DepthGenerationProgress, &Pipeline](TSharedPtr<FPipelineData> InPipelineData) 
	{
		DepthGenerationProgress.EnterProgressFrame(1);
		
		if (DepthGenerationProgress.ShouldCancel())
		{
			Pipeline.Cancel();
		}
	});

	PipelineRunParameters.SetOnFrameComplete(OnFrameComplete);

	TSharedPtr<FPipelineData> PipelineOutput;
	FProcessComplete OnProcessComplete;
	OnProcessComplete.AddLambda([&PipelineOutput](TSharedPtr<FPipelineData> InPipelineData)
	{
		PipelineOutput = InPipelineData;
	});

	PipelineRunParameters.SetOnProcessComplete(OnProcessComplete);

	Pipeline.Run(PipelineRunParameters);

	if (PipelineOutput->GetExitStatus() != EPipelineExitStatus::Ok)
	{
		FString ErrorMessage = PipelineOutput->GetErrorMessage();
		if (PipelineOutput->GetExitStatus() == EPipelineExitStatus::Aborted)
		{
			ErrorMessage = TEXT("Process aborted by the user");
		}

		UE_LOG(LogMetaHumanDepthGeneration, Error, TEXT("Failed to complete depth generation: %s"), *ErrorMessage);
		return MakeError(EDepthGenerationError::PipelineError);
	}

	FCameraCalibration DepthCameraCalibration = GenerateDepths->GetNodes()[0]->Calibrations[1];

	DepthCameraCalibration.CameraId = TEXT("Depth");
	DepthCameraCalibration.CameraType = FCameraCalibration::Depth;

	if (ResizeDepth.IsValid())
	{
		const float OrigImageSize = DepthCameraCalibration.ImageSize.X;

		DepthCameraCalibration.ImageSize /= ResizeDepth->Factor;
		DepthCameraCalibration.PrincipalPoint /= ResizeDepth->Factor;

		const float FocalScale = OrigImageSize / DepthCameraCalibration.ImageSize.X;

		DepthCameraCalibration.FocalLength /= FocalScale;

		if (DepthCameraCalibration.ImageSize.X < 640 || DepthCameraCalibration.ImageSize.Y < 360) // depth image is orientated on its side 
		{
			FText Warning = FText::Format(LOCTEXT("LowDepthResWarning", "Resized depth image has low resolution {0}x{1}"), DepthCameraCalibration.ImageSize.X, DepthCameraCalibration.ImageSize.Y);
			UE_LOG(LogMetaHumanDepthGeneration, Warning, TEXT("%s"), *Warning.ToString());
		}
	}

	DepthCameraCalibration.FocalLengthNormalized = FVector2D::Zero();
	DepthCameraCalibration.PrincipalPointNormalized = FVector2D::Zero();

	return MakeValue(MoveTemp(DepthCameraCalibration));
}

FString CreateUniqueAssetName(const FString& InOriginalName, const FString& InPackagePath)
{
	IAssetRegistry& AssetRegistry = FModuleManager::GetModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();

	FString AssetName = InOriginalName;
	FString ObjectPathToCheck = InPackagePath / (AssetName + FString(TEXT(".")) + AssetName);
	FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(ObjectPathToCheck));

	int32 Counter = 1;
	while (AssetData.IsValid())
	{
		AssetName = InOriginalName + TEXT("_") + FString::FromInt(Counter++);
		ObjectPathToCheck = InPackagePath / (AssetName + FString(TEXT(".")) + AssetName);
		AssetData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(ObjectPathToCheck));
	}

	return AssetName;
}

FString CreateUniqueFolderName(const FString& InDesiredFolderPath)
{
	IFileManager& FileManager = IFileManager::Get();

	uint32 Counter = 1;
	FString FolderPath = InDesiredFolderPath;

	static constexpr FStringView Delimiter = TEXT("_");
	while (FileManager.DirectoryExists(*FolderPath))
	{
		FolderPath = InDesiredFolderPath + Delimiter + FString::FromInt(Counter++);
	}

	return FolderPath;
}

TObjectPtr<UCameraCalibration> DuplicateReferenceAsset(TObjectPtr<UCameraCalibration> InReferenceAsset, const FString& InPackagePath)
{
	IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();

	FString AssetName = CreateUniqueAssetName(InReferenceAsset->GetName(), InPackagePath);

	// Duplicate the asset.
	return static_cast<UCameraCalibration*>(AssetTools.DuplicateAsset(AssetName, InPackagePath, InReferenceAsset.Get()));
}

bool CreateCalibrationAsset(const FCameraCalibration& InCameraCalibration, const FString& InPackagePath, TObjectPtr<UCameraCalibration> OutCalibrationAsset)
{
	const FString ObjectName = InCameraCalibration.CameraType == FCameraCalibration::Depth ?
		FString::Printf(TEXT("%s_Depth_LensFile"), *OutCalibrationAsset->GetName()) :
		FString::Printf(TEXT("%s_%s_RGB_LensFile"), *OutCalibrationAsset->GetName(), *InCameraCalibration.CameraId);

	const FString PackageName = UPackageTools::SanitizePackageName(InPackagePath + TEXT("/") + ObjectName);
	UPackage* Parent = CreatePackage(*PackageName);

	if (!ensureMsgf(Parent, TEXT("Failed to create parent package for the calibration asset")))
	{
		return false;
	}

	FExtendedLensFile CameraCalibration;
	CameraCalibration.Name = InCameraCalibration.CameraId;
	CameraCalibration.IsDepthCamera = InCameraCalibration.CameraType == FCameraCalibration::Depth;
	CameraCalibration.LensFile = NewObject<ULensFile>(Parent, ULensFile::StaticClass(), *ObjectName, OutCalibrationAsset->GetFlags());

	// These a for a non-FIZ camera.
	const float Focus = 0.0f;
	const float Zoom = 0.0f;

	// LensInfo
	CameraCalibration.LensFile->LensInfo.LensModel = USphericalLensModel::StaticClass();
	CameraCalibration.LensFile->LensInfo.LensModelName = FString::Printf(TEXT("Lens"));

	// leave sensor dimensions with default values and de-normalize using VideoDimensions or DepthDimensions
	CameraCalibration.LensFile->LensInfo.ImageDimensions = FIntPoint(InCameraCalibration.ImageSize.X, InCameraCalibration.ImageSize.Y);

	// FocalLengthInfo
	FFocalLengthInfo FocalLengthInfo;
	if (!InCameraCalibration.FocalLengthNormalized.Equals(FVector2D::Zero()))
	{
		FocalLengthInfo.FxFy = InCameraCalibration.FocalLengthNormalized;
	}
	else
	{
		FocalLengthInfo.FxFy = InCameraCalibration.FocalLength / InCameraCalibration.ImageSize;
	}

	// DistortionInfo
	FDistortionInfo DistortionInfo;
	FSphericalDistortionParameters SphericalParameters;

	SphericalParameters.K1 = InCameraCalibration.K1;
	SphericalParameters.K2 = InCameraCalibration.K2;
	SphericalParameters.P1 = InCameraCalibration.P1;
	SphericalParameters.P2 = InCameraCalibration.P2;
	SphericalParameters.K3 = InCameraCalibration.K3;

	USphericalLensModel::StaticClass()->GetDefaultObject<ULensModel>()->ToArray(
		SphericalParameters,
		DistortionInfo.Parameters
	);

	// ImageCenterInfo
	FImageCenterInfo ImageCenterInfo;
	if (!InCameraCalibration.PrincipalPointNormalized.Equals(FVector2D::Zero()))
	{
		ImageCenterInfo.PrincipalPoint = InCameraCalibration.PrincipalPointNormalized;
	}
	else
	{
		ImageCenterInfo.PrincipalPoint = InCameraCalibration.PrincipalPoint / InCameraCalibration.ImageSize;
	}

	// NodalOffset
	FNodalPointOffset NodalPointOffset;
	FTransform Transform;
	Transform.SetFromMatrix(InCameraCalibration.Transform);
	FOpenCVHelperLocal::ConvertOpenCVToUnreal(Transform);
	NodalPointOffset.LocationOffset = Transform.GetLocation();
	NodalPointOffset.RotationOffset = Transform.GetRotation();

	if (InCameraCalibration.Orientation == EMediaOrientation::CW90 ||
		InCameraCalibration.Orientation == EMediaOrientation::CW270)
	{
		Swap(CameraCalibration.LensFile->LensInfo.ImageDimensions.X, CameraCalibration.LensFile->LensInfo.ImageDimensions.Y);
		Swap(CameraCalibration.LensFile->LensInfo.SensorDimensions.X, CameraCalibration.LensFile->LensInfo.SensorDimensions.Y);
		Swap(FocalLengthInfo.FxFy.X, FocalLengthInfo.FxFy.Y);

		FVector2D UnrotatedPrincipalPoint = ImageCenterInfo.PrincipalPoint;
		ImageCenterInfo.PrincipalPoint.X = 1.0 - UnrotatedPrincipalPoint.Y;
		ImageCenterInfo.PrincipalPoint.Y = UnrotatedPrincipalPoint.X;
	}

	CameraCalibration.LensFile->AddDistortionPoint(Focus, Zoom, DistortionInfo, FocalLengthInfo);
	CameraCalibration.LensFile->AddImageCenterPoint(Focus, Zoom, ImageCenterInfo);
	CameraCalibration.LensFile->AddNodalOffsetPoint(Focus, Zoom, NodalPointOffset);

	CameraCalibration.LensFile->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(CameraCalibration.LensFile);

	// Remove previous depth camera calibration if it exists
	int32 NumberOfRemovals = OutCalibrationAsset->CameraCalibrations.RemoveAll([](const FExtendedLensFile& InCameraLensFile)
	{
		return InCameraLensFile.IsDepthCamera;
	});

	if (NumberOfRemovals != 0)
	{
		UE_LOG(LogMetaHumanDepthGeneration, Warning, TEXT("Removed the previous depth camera calibration data"));
	}

	OutCalibrationAsset->CameraCalibrations.Add(MoveTemp(CameraCalibration));

	return true;
}

void SaveDepthProcessCreatedAssets(const FString& InAssetPath)
{
	IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
	IAssetRegistry& AssetRegistry = FModuleManager::GetModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

	TArray<FAssetData> AssetsData;
	AssetRegistry.GetAssetsByPath(FName{ *InAssetPath }, AssetsData, true, false);

	if (AssetsData.IsEmpty())
	{
		return;
	}

	TArray<UPackage*> Packages;
	for (const FAssetData& AssetData : AssetsData)
	{
		UPackage* Package = AssetData.GetAsset()->GetPackage();
		if (!Packages.Contains(Package))
		{
			Packages.Add(Package);
		}
	}

	UEditorLoadingAndSavingUtils::SavePackages(Packages, true);
}

}

bool UMetaHumanDepthGenerator::Process(UFootageCaptureData* InFootageCaptureData)
{
	TSharedRef<SMetaHumanGenerateDepthWindow> GenerateDepthWindow =
		SNew(SMetaHumanGenerateDepthWindow)
		.CaptureData(InFootageCaptureData);

	TOptional<TStrongObjectPtr<UMetaHumanGenerateDepthWindowOptions>> OptionsOpt = GenerateDepthWindow->ShowModal();

	if (!OptionsOpt.IsSet())
	{
		return false;
	}

	TStrongObjectPtr<UMetaHumanGenerateDepthWindowOptions> Options = MoveTemp(OptionsOpt.GetValue());

	return Process(InFootageCaptureData, Options.Get());
}

bool UMetaHumanDepthGenerator::Process(UFootageCaptureData* InFootageCaptureData, UMetaHumanGenerateDepthWindowOptions* InOptions)
{
	using namespace UE::MetaHuman::Private;

	if (!IModularFeatures::Get().IsModularFeatureAvailable(IFaceTrackerNodeImplFactory::GetModularFeatureName()))
	{
		UE_LOG(LogMetaHumanDepthGeneration, Error, TEXT("Unable to generate depth images. Please make sure Depth Processing plugin is enabled. (Available on Fab)"));
		return false;
	}

	if (!ensureMsgf(InFootageCaptureData, TEXT("Capture Data must exist")))
	{
		return false;
	}

	if (InFootageCaptureData->ImageSequences.Num() != 2)
	{
		UE_LOG(LogMetaHumanDepthGeneration, Error, TEXT("Generating depth images is not possible without the stereo camera pair"));
		return false;
	}

	if (!IsValid(InFootageCaptureData->ImageSequences[0]) || !IsValid(InFootageCaptureData->ImageSequences[1]))
	{
		UE_LOG(LogMetaHumanDepthGeneration, Error, TEXT("Provided image sequences are invalid"));
		return false;
	}

	TObjectPtr<UCameraCalibration> ReferenceCameraCalibration;
	
	if (!InOptions->ReferenceCameraCalibration)
	{
		if (InFootageCaptureData->CameraCalibrations.IsEmpty())
		{
			UE_LOG(LogMetaHumanDepthGeneration, Error, TEXT("Generating depth images is not possible without a Camera Calibration"));
			return false;
		}

		ReferenceCameraCalibration = InFootageCaptureData->CameraCalibrations[0];
	}
	else
	{
		ReferenceCameraCalibration = InOptions->ReferenceCameraCalibration;
	}

	if (!IsValid(ReferenceCameraCalibration))
	{
		UE_LOG(LogMetaHumanDepthGeneration, Error, TEXT("Provided calibration is invalid"));
		return false;
	}
	
	const int32 CameraCalibrationCount = ReferenceCameraCalibration->CameraCalibrations.Num();
	if (CameraCalibrationCount < 2)
	{
		UE_LOG(LogMetaHumanDepthGeneration, Error, TEXT("Depth generation requires one Camera Calibration per camera in the stereo pair. Expecting 2, found %d"), CameraCalibrationCount);
		return false;
	}

	bool bShouldCompressDepthFiles = InOptions->bShouldCompressDepthFiles;
	EMetaHumanCaptureDepthPrecisionType DepthPrecision = InOptions->DepthPrecision;
	EMetaHumanCaptureDepthResolutionType DepthResolution = InOptions->DepthResolution;
	TRange<float> DepthDistance = TRange<float>(InOptions->MinDistance, InOptions->MaxDistance);

	FString DepthDirectory = UE::MetaHuman::Private::CreateUniqueFolderName(InOptions->ImageSequenceRootPath.Path);

	if (FPackagePath PackagePath = FPackagePath::FromLocalPath(InOptions->ImageSequenceRootPath.Path); 
		PackagePath.HasPackageName() && InOptions->bShouldExcludeDepthFilesFromImport)
	{
		DepthDirectory /= UMetaHumanGenerateDepthWindowOptions::ImageSequenceDirectoryName;
	}

	FDepthGenerator::FParameters Parameters =
	{
		.DepthDirectory = DepthDirectory,
		.bShouldCompressDepthFiles = InOptions->bShouldCompressDepthFiles,
		.DepthPrecision = InOptions->DepthPrecision,
		.DepthResolution = InOptions->DepthResolution,
		.DepthDistance = MoveTemp(DepthDistance)
	};

	FDepthGenerator DepthGenerator(MoveTemp(Parameters));

	TValueOrError<FCameraCalibration, FDepthGenerator::EDepthGenerationError> Result =
		DepthGenerator.RunGenerateDepthImagesPipeline(InFootageCaptureData, ReferenceCameraCalibration);

	if (Result.HasError())
	{
		IFileManager::Get().DeleteDirectory(*DepthDirectory, false, true);

		UE_LOG(LogMetaHumanDepthGeneration, Error, TEXT("Failed to generate depth images"));
		return false;
	}

	FCameraCalibration DepthCalibration = Result.StealValue();
	const FString PackagePath = InOptions->PackagePath.Path;
	const FString DepthAssetName = InOptions->AssetName;

	// Each camera has to have a depth image sequence.
	InFootageCaptureData->DepthSequences.Empty();

	IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();

	FString UniqueDepthAssetName = 
		UE::MetaHuman::Private::CreateUniqueAssetName(DepthAssetName, PackagePath);

	UImgMediaSource* DepthImageSequence = Cast<UImgMediaSource>(AssetTools.CreateAsset(UniqueDepthAssetName, PackagePath, UImgMediaSource::StaticClass(), nullptr));
	if (!DepthImageSequence)
	{
		UE_LOG(LogMetaHumanDepthGeneration, Error, TEXT("Unable to create image sequence asset for depth"));
		return false;
	}

	DepthImageSequence->SetTokenizedSequencePath(DepthDirectory);
	// Set the timecode and the frame rate to be the same as video
	DepthImageSequence->FrameRateOverride = InFootageCaptureData->ImageSequences[0]->FrameRateOverride;
	DepthImageSequence->StartTimecode = InFootageCaptureData->ImageSequences[0]->StartTimecode;

	InFootageCaptureData->DepthSequences.Add(DepthImageSequence);
	InFootageCaptureData->DepthSequences.Add(DepthImageSequence);

	TObjectPtr<UCameraCalibration> DuplicatedCameraCalibration = DuplicateReferenceAsset(ReferenceCameraCalibration, PackagePath);

	bool bCreateCalibrationAssetResult = UE::MetaHuman::Private::CreateCalibrationAsset(DepthCalibration, PackagePath, DuplicatedCameraCalibration);
	if (!bCreateCalibrationAssetResult)
	{
		return false;
	}

	InFootageCaptureData->CameraCalibrations.Empty();
	InFootageCaptureData->CameraCalibrations.Add(DuplicatedCameraCalibration);

	InFootageCaptureData->MarkPackageDirty();

	if (InOptions->bAutoSaveAssets)
	{
		SaveDepthProcessCreatedAssets(PackagePath);
	}

	return true;
}

#undef LOCTEXT_NAMESPACE