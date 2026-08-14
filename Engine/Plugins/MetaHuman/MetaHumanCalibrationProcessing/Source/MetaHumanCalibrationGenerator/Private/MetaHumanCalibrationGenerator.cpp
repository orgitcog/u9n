// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanCalibrationGenerator.h"

#include "Widgets/SMetaHumanCalibrationGeneratorWindow.h"

#include "ImageSequenceUtils.h"
#include "ImgMediaSource.h"

#include "IImageWrapper.h"
#include "IImageWrapperModule.h"

#include "Modules/ModuleManager.h"

#include "Misc/FileHelper.h"
#include "Misc/ScopedSlowTask.h"

#include "Async/ParallelFor.h"
#include "Async/Monitor.h"

#include "IAssetTools.h"
#include "AssetToolsModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "FileHelpers.h"

#include "Async/Async.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "OutputLogModule.h"

#define LOCTEXT_NAMESPACE "MetaHumanCalibrationGenerator"

DEFINE_LOG_CATEGORY_STATIC(LogMetaHumanCalibrationGenerator, Log, All);

namespace UE::MetaHuman::Private
{

class FCalibrationNotificationManager : public TSharedFromThis<FCalibrationNotificationManager>
{
public:

	void NotificationOnBegin(const FText& InInfoText)
	{
		ExecuteOnGameThread(TEXT("CalibrationNotificationOnBegin"), [This = AsShared(), InInfoText]()
		{
			FNotificationInfo Info(InInfoText);
			Info.bFireAndForget = false;
			Info.ExpireDuration = 1.0f;

			FScopeLock Lock(&This->Mutex);
			checkf(!This->CurrentNotification.IsValid(), TEXT("Missing NotificationOnEnd call"));

			This->CurrentNotification = FSlateNotificationManager::Get().AddNotification(Info);
			if (This->CurrentNotification)
			{
				This->CurrentNotification->SetCompletionState(SNotificationItem::CS_Pending);
			}
		});
	}

	void NotificationOnEnd(bool bIsSuccess)
	{
		ExecuteOnGameThread(TEXT("CalibrationNotificationOnEnd"), [This = AsShared(), bIsSuccess]()
		{
			FScopeLock Lock(&This->Mutex);
			checkf(This->CurrentNotification.IsValid(), TEXT("Missing NotificationOnBegin call"));

			if (!bIsSuccess)
			{
				This->CurrentNotification->SetHyperlink(FSimpleDelegate::CreateLambda([]()
				{
					FOutputLogModule& OutputLogModule = FModuleManager::Get().LoadModuleChecked<FOutputLogModule>("OutputLog");
					OutputLogModule.FocusOutputLog();
				}), LOCTEXT("CalibrationOpenLog", "Open Output Log"));

				This->CurrentNotification->SetExpireDuration(5.0f);
				This->CurrentNotification->SetCompletionState(SNotificationItem::CS_Fail);
			}
			else
			{
				This->CurrentNotification->SetCompletionState(SNotificationItem::CS_Success);
			}

			This->CurrentNotification->ExpireAndFadeout();
			This->CurrentNotification = nullptr;
		});
	}

private:

	FCriticalSection Mutex;
	TSharedPtr<SNotificationItem> CurrentNotification;
};

UCameraCalibration* CreateCameraCalibrationAsset(const FString& InTargetPackagePath, const FString& InDesiredAssetName)
{
	IAssetRegistry& AssetRegistry = FModuleManager::GetModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();

	FString AssetName = InDesiredAssetName;
	FString ObjectPathToCheck = InTargetPackagePath / (AssetName + FString(TEXT(".")) + AssetName);
	FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(ObjectPathToCheck));

	int32 Counter = 1;
	while (AssetData.IsValid())
	{
		AssetName = InDesiredAssetName + TEXT("_") + FString::FromInt(Counter++);
		ObjectPathToCheck = InTargetPackagePath / (AssetName + FString(TEXT(".")) + AssetName);
		AssetData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(ObjectPathToCheck));
	}
	
	return Cast<UCameraCalibration>(AssetTools.CreateAsset(AssetName, InTargetPackagePath, UCameraCalibration::StaticClass(), nullptr));
}

TArray64<uint8> GetGrayscaleImage(const FString& InFullImagePath)
{
	IImageWrapperModule& ImageWrapperModule = 
		FModuleManager::Get().LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));

	EImageFormat ImageFormat = ImageWrapperModule.GetImageFormatFromExtension(*InFullImagePath);
	if (ImageFormat == EImageFormat::Invalid)
	{
		return TArray64<uint8>();
	}

	TArray<uint8> RawFileData;
	if (!FFileHelper::LoadFileToArray(RawFileData, *InFullImagePath))
	{
		return TArray64<uint8>();
	}

	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(ImageFormat);
	ImageWrapper->SetCompressed(RawFileData.GetData(), RawFileData.Num());

	TArray64<uint8> ImageData;
	static constexpr int32 BitDepth = 8;
	ImageWrapper->GetRaw(ERGBFormat::Gray, BitDepth, ImageData);

	if (ImageData.IsEmpty())
	{
		return TArray64<uint8>();
	}

	return ImageData;
}

void SaveCalibrationProcessCreatedAssets(const FString& InAssetPath)
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

void CreateCalibrationAssetOnGameThread(TStrongObjectPtr<const UMetaHumanCalibrationGenerator> InOwner,
										TStrongObjectPtr<const UMetaHumanCalibrationGeneratorOptions> InOptions,
										TArray<FCameraCalibration> InCameraCalibrations,
										TSharedPtr<FCalibrationNotificationManager> InNotificationManager,
										TStrongObjectPtr<UFootageCaptureData> OutCaptureData)
{
	// UMetaHumanCalibrationGenerated, UFootageCaptureData and FCalibrationNotificationManager are captured here to protect their lifecycle. 
	ExecuteOnGameThread(TEXT("CalibrationAssetCreation"), 
					[Owner = MoveTemp(InOwner),
					Options = MoveTemp(InOptions),
					CaptureData = MoveTemp(OutCaptureData),
					CameraCalibrations = MoveTemp(InCameraCalibrations),
					NotificationManager = MoveTemp(InNotificationManager)]()
	{
		TObjectPtr<UCameraCalibration> CalibrationAsset = 
			UE::MetaHuman::Private::CreateCameraCalibrationAsset(Options->PackagePath.Path, Options->AssetName);
		CalibrationAsset->CameraCalibrations.Reset();
		CalibrationAsset->StereoPairs.Reset();
		CalibrationAsset->ConvertFromTrackerNodeCameraModels(CameraCalibrations, false);

		CaptureData->CameraCalibrations.Add(MoveTemp(CalibrationAsset));

		CaptureData->MarkPackageDirty();

		if (Options->bAutoSaveAssets)
		{
			SaveCalibrationProcessCreatedAssets(Options->PackagePath.Path);
		}
	});
}

}

UMetaHumanCalibrationGenerator::UMetaHumanCalibrationGenerator()
	: StereoCalibrator(MakeUnique<UE::Wrappers::FMetaHumanStereoCalibrator>())
{
}

bool UMetaHumanCalibrationGenerator::Process(UFootageCaptureData* InCaptureData)
{
	TSharedRef<SMetaHumanCalibrationGeneratorWindow> GenerateDepthWindow =
		SNew(SMetaHumanCalibrationGeneratorWindow)
		.CaptureData(InCaptureData);

	TOptional<TStrongObjectPtr<UMetaHumanCalibrationGeneratorOptions>> OptionsOpt = GenerateDepthWindow->ShowModal();

	if (!OptionsOpt.IsSet())
	{
		return false;
	}

	TStrongObjectPtr<UMetaHumanCalibrationGeneratorOptions> Options = MoveTemp(OptionsOpt.GetValue());

	AsyncTask(ENamedThreads::AnyThread,
			  [CaptureData = TStrongObjectPtr<UFootageCaptureData>(InCaptureData),
			  Options = MoveTemp(Options), 
			  This = TStrongObjectPtr<UMetaHumanCalibrationGenerator>(this)]()
		  {
			  This->Process(CaptureData.Get(), Options.Get());
		  });

	return true;
}

bool UMetaHumanCalibrationGenerator::Process(UFootageCaptureData* InCaptureData, const UMetaHumanCalibrationGeneratorOptions* InOptions)
{
	TValueOrError<void, FString> OptionsValidity = InOptions->CheckOptionsValidity();
	if (OptionsValidity.HasError())
	{
		UE_LOG(LogMetaHumanCalibrationGenerator, Error, TEXT("Invalid options for stereo calibration process: %s"), *OptionsValidity.GetError());
		return false;
	}

	if (InCaptureData->ImageSequences.Num() != 2)
	{
		UE_LOG(LogMetaHumanCalibrationGenerator, Error, TEXT("Stereo calibration process expects 2 cameras, but found %d"), InCaptureData->ImageSequences.Num());
		return false;
	}

	StereoCalibrator->Init(InOptions->BoardPatternWidth, InOptions->BoardPatternHeight, InOptions->BoardSquareSize);

	const UImgMediaSource* FirstCameraImageSource = InCaptureData->ImageSequences[0];
	const UImgMediaSource* SecondCameraImageSource = InCaptureData->ImageSequences[1];

	FString FirstCameraName = FirstCameraImageSource->GetName();
	FString SecondCameraName = SecondCameraImageSource->GetName();

	FIntVector2 ImageDimensions;
	int32 NumberOfImages = 0;
	FImageSequenceUtils::GetImageSequenceInfoFromAsset(FirstCameraImageSource, ImageDimensions, NumberOfImages);

	UE_LOG(LogMetaHumanCalibrationGenerator, Display, TEXT("Adding %s camera with image size %dx%d"), *FirstCameraName, ImageDimensions.X, ImageDimensions.Y);
	StereoCalibrator->AddCamera(FirstCameraName, ImageDimensions.X, ImageDimensions.Y);

	UE_LOG(LogMetaHumanCalibrationGenerator, Display, TEXT("Adding %s camera with image size %dx%d"), *SecondCameraName, ImageDimensions.X, ImageDimensions.Y);
	StereoCalibrator->AddCamera(SecondCameraName, ImageDimensions.X, ImageDimensions.Y);

	FString FirstCameraImagePath;
	TArray<FString> FirstCameraImageNames;
	FImageSequenceUtils::GetImageSequencePathAndFilesFromAsset(FirstCameraImageSource, FirstCameraImagePath, FirstCameraImageNames);

	FString SecondCameraImagePath;
	TArray<FString> SecondCameraImageNames;
	FImageSequenceUtils::GetImageSequencePathAndFilesFromAsset(SecondCameraImageSource, SecondCameraImagePath, SecondCameraImageNames);

	if (FirstCameraImageNames.Num() != SecondCameraImageNames.Num())
	{
		UE_LOG(LogMetaHumanCalibrationGenerator, Error, 
			   TEXT("Number of frames for stereo camera pair is different:\n%s: %d \n%s: %d)"), 
			   *FirstCameraName, FirstCameraImageNames.Num(), *SecondCameraName, SecondCameraImageNames.Num());
		return false;
	}

	using namespace UE::MetaHuman::Private;
	TSharedPtr<FCalibrationNotificationManager> NotificationManager = MakeShared<FCalibrationNotificationManager>();
	NotificationManager->NotificationOnBegin(LOCTEXT("CalibrationDetectionInProgress", "MetaHumanCalibrationGenerator: Waiting for checkerboard pattern detection..."));

	FDetectedFrames DetectedValidFrames = DetectPatterns(InCaptureData, InOptions);

	static constexpr int32 MinimumRequiredFrames = 3;
	bool bDetectionSuccess = !DetectedValidFrames.IsEmpty() && (DetectedValidFrames.Num() >= MinimumRequiredFrames);
	
	NotificationManager->NotificationOnEnd(bDetectionSuccess);

	if (!bDetectionSuccess)
	{
		UE_LOG(LogMetaHumanCalibrationGenerator, Error, TEXT("Not enough valid frames detected to run calibration (Minimum is %d)"), MinimumRequiredFrames);
		return false;
	}

	NotificationManager->NotificationOnBegin(LOCTEXT("CalibrationInProgress", "MetaHumanCalibrationGenerator: Waiting for calibration..."));
	TArray<FCameraCalibration> CameraCalibrations;
	double OutReprojectionError = 0.0f;
	bool bResult = StereoCalibrator->Calibrate(DetectedValidFrames, CameraCalibrations, OutReprojectionError);

	NotificationManager->NotificationOnEnd(bResult);

	if (!bResult)
	{
		UE_LOG(LogMetaHumanCalibrationGenerator, Error, TEXT("Failed to calibrate the footage"));
		return false;
	}

	UE_LOG(LogMetaHumanCalibrationGenerator, Display, TEXT("Successfully calibrated with reprojection error of %lf"), OutReprojectionError);

	CreateCalibrationAssetOnGameThread(TStrongObjectPtr<const UMetaHumanCalibrationGenerator>(this),
									   TStrongObjectPtr<const UMetaHumanCalibrationGeneratorOptions>(InOptions),
									   MoveTemp(CameraCalibrations),
									   MoveTemp(NotificationManager), 
									   TStrongObjectPtr<UFootageCaptureData>(InCaptureData));

	return true;
}

UMetaHumanCalibrationGenerator::FDetectedFrames UMetaHumanCalibrationGenerator::DetectPatterns(const UFootageCaptureData* InCaptureData,
																							   const UMetaHumanCalibrationGeneratorOptions* InOptions)
{
	using namespace UE::CaptureManager;

	FString FirstCameraName = InCaptureData->ImageSequences[0]->GetName();
	FString SecondCameraName = InCaptureData->ImageSequences[1]->GetName();

	TArray<FString> FirstCameraImageNames;
	TArray<FString> SecondCameraImageNames;
	FImageSequenceUtils::GetImageSequenceFilesFromPath(InCaptureData->ImageSequences[0]->GetFullPath(), FirstCameraImageNames);
	FImageSequenceUtils::GetImageSequenceFilesFromPath(InCaptureData->ImageSequences[1]->GetFullPath(), SecondCameraImageNames);

	check(FirstCameraImageNames.Num() == SecondCameraImageNames.Num());

	
	using FDetectedCameraFrames = TArray<TMap<FString, TArray<FVector2D>>>;
	FMonitor<FDetectedCameraFrames> ValidFrames;

	static constexpr int32 NumberOfThreads = 10;
	const int32 NumberOfImagesPerThread = FirstCameraImageNames.Num() / NumberOfThreads;
	const int32 TotalNumberOfImages = FirstCameraImageNames.Num();

	ParallelFor(NumberOfThreads, [&](int32 InChunkIndex)
	{
		int32 StartFrameIndex = InChunkIndex * NumberOfImagesPerThread;

		if (StartFrameIndex > TotalNumberOfImages)
		{
			return;
		}

		int32 NumberOfFrames = (StartFrameIndex + NumberOfImagesPerThread) < TotalNumberOfImages ?
			NumberOfImagesPerThread : FMath::Min(TotalNumberOfImages - NumberOfImagesPerThread, 0);

		const int32 Left = TotalNumberOfImages - (StartFrameIndex + NumberOfImagesPerThread);
		if (Left < NumberOfImagesPerThread)
		{
			NumberOfFrames += Left;
		}

		for (int32 FrameIndex = StartFrameIndex; FrameIndex < StartFrameIndex + NumberOfFrames; ++FrameIndex)
		{
			if ((FrameIndex % InOptions->SampleRate) != 0)
			{
				continue;
			}

			FString FirstCameraImagePath = FPaths::ConvertRelativePathToFull(InCaptureData->ImageSequences[0]->GetFullPath(), FirstCameraImageNames[FrameIndex]);
			TArray64<uint8> FirstCameraImage = UE::MetaHuman::Private::GetGrayscaleImage(FirstCameraImagePath);
			if (FirstCameraImage.IsEmpty())
			{
				continue;
			}

			TArray<FVector2D> FirstCameraImageCornerPoints;
			double FirstCameraImageChessboardSharpness = 0.0;

			bool bFirstCameraDetectResult = StereoCalibrator->DetectPattern(FirstCameraName, FirstCameraImage.GetData(), FirstCameraImageCornerPoints, FirstCameraImageChessboardSharpness);

			FString SecondCameraImagePath = FPaths::ConvertRelativePathToFull(InCaptureData->ImageSequences[1]->GetFullPath(), SecondCameraImageNames[FrameIndex]);
			TArray64<uint8> SecondCameraImage = UE::MetaHuman::Private::GetGrayscaleImage(SecondCameraImagePath);
			if (SecondCameraImage.IsEmpty())
			{
				continue;
			}

			TArray<FVector2D> SecondCameraImageCornerPoints;
			double SecondCameraImageChessboardSharpness = 0.0;

			bool bSecondCameraDetectResult = StereoCalibrator->DetectPattern(SecondCameraName, SecondCameraImage.GetData(), SecondCameraImageCornerPoints, SecondCameraImageChessboardSharpness);

			if (bFirstCameraDetectResult && 
				bSecondCameraDetectResult && 
				FirstCameraImageChessboardSharpness < InOptions->SharpnessThreshold &&
				SecondCameraImageChessboardSharpness < InOptions->SharpnessThreshold)
			{
				FMonitor<FDetectedCameraFrames>::FHelper Helper = ValidFrames.Lock();
				int32 Index = Helper->Emplace();
				(*Helper)[Index].Emplace(FirstCameraName, FirstCameraImageCornerPoints);
				(*Helper)[Index].Emplace(SecondCameraName, SecondCameraImageCornerPoints);
			}
		}
	});

	return ValidFrames.Claim();
}

#undef LOCTEXT_NAMESPACE