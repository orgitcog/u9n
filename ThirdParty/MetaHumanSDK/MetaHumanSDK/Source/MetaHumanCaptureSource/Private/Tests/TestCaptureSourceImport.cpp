// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "HAL/FileManagerGeneric.h"
#include "MetaHumanCaptureSourceSync.h"
#include "CoreMinimal.h"
#include "ImgMediaSource.h"
#include "UObject/Package.h"


#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FCaptureSourceImportTest, "MetaHuman.FileSize.Capture", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FCaptureSourceImportTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	TArray<FString> Tests;

	Tests.Add("CheckHMCImport");
	Tests.Add("CheckLLFArchiveImport");

	for (const FString& Test : Tests)
	{
		OutBeautifiedNames.Add(Test);
		OutTestCommands.Add(Test);
	}
}

bool FCaptureSourceImportTest::RunTest(const FString& InTestCommand)
{
	bool bIsOK = true;

	const int64 HMCDepthLowerSizeLimit = static_cast<int64>(1.6 * 1024 * 1024);
	const int64 HMCDepthUpeerSizeLimit = static_cast <int64>(2.3 * 1024 * 1024);
	const int64 IOSImageLowerSizeLimit = 250 * 1024;
	const int64 IOSImageUpperSizeLimit = 350 * 1024;
	const int64 IOSDepthLowerSizeLimit = 325 * 1024;
	const int64 IOSDepthUpeerSizeLimit = 375 * 1024;

	FString InputDir = FPaths::ProjectContentDir() / "AutoTestRawData/Footage/GoldDataComparison";
	FString OutputDir = FPaths::ProjectIntermediateDir() / "CaptureImportTest";
	IFileManager& FileManager = FFileManagerGeneric::Get();

	auto FilesWithinSizeLimitLambda = [&FileManager](const TArray<FString>& InFiles, const FString& InPath, const int64 LowerLimit, const int64 UpperLimit) -> bool
	{
		for (const FString& File : InFiles)
		{
			FString FullPath = InPath / File;
			const int64 CurrentSize = FileManager.FileSize(*FullPath);
			if (CurrentSize < LowerLimit || CurrentSize > UpperLimit)
			{
				return false;
			}
		}

		return true;
	};

	if (InTestCommand == TEXT("CheckHMCImport"))
	{
		UMetaHumanCaptureSourceSync* SyncSource = NewObject<UMetaHumanCaptureSourceSync>(GetTransientPackage());

		SyncSource->CaptureSourceType = EMetaHumanCaptureSourceType::HMCArchives;		
		FPropertyChangedEvent CaptureSourceChanged(UMetaHumanCaptureSourceSync::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UMetaHumanCaptureSourceSync, CaptureSourceType)));
		SyncSource->PostEditChangeProperty(CaptureSourceChanged);

		SyncSource->StoragePath = FDirectoryPath(InputDir / "HMC");
		FPropertyChangedEvent StoragePathChanged(UMetaHumanCaptureSourceSync::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UMetaHumanCaptureSourceSync, StoragePath)));
		SyncSource->PostEditChangeProperty(StoragePathChanged);

		FString HMCOutputPath = OutputDir / "HMC";
		if (SyncSource->CanStartup())
		{
			SyncSource->Startup();
			SyncSource->SetTargetPath(HMCOutputPath, "/Game/CaptureTest/HMC_Ingested");

			TArray<FMetaHumanTakeInfo> AllTakes = SyncSource->Refresh();

			UTEST_NOT_EQUAL("Have valid takes for import", AllTakes.Num(), 0);
			TArray<FMetaHumanTake> ImportedTakes = SyncSource->GetTakes({ AllTakes[0].Id });

			UTEST_NOT_EQUAL("Have imported takes", ImportedTakes.Num(), 0);
			TArray<FMetaHumanTakeView> TakeViews = ImportedTakes[0].Views;
			SyncSource->Shutdown();

			bIsOK &= TestEqual("Number of views", TakeViews.Num(), 2);
			if (bIsOK)
			{
				FString VideoAPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectUserDir(), TakeViews[0].Video->GetSequencePath());
				FString VideoBPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectUserDir(), TakeViews[1].Video->GetSequencePath());
				FString RelativeDepthPath = TakeViews[0].Depth->GetSequencePath();
				FString DepthPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectUserDir(), RelativeDepthPath);

				bIsOK &= TestEqual("Depth paths match", RelativeDepthPath, TakeViews[1].Depth->GetSequencePath());

				TArray<FString> VideoAFiles;
				FileManager.FindFiles(VideoAFiles, *VideoAPath, TEXT("PNG"));

				TArray<FString> VideoBFiles;
				FileManager.FindFiles(VideoBFiles, *VideoBPath, TEXT("PNG"));

				TArray<FString> DepthFiles;
				FileManager.FindFiles(DepthFiles, *DepthPath, TEXT("EXR"));

				bIsOK &= TestNotEqual("Found image files", VideoAFiles.Num(), 0);
				bIsOK &= TestEqual("Number of images match", VideoAFiles.Num(), VideoBFiles.Num());
				bIsOK &= TestEqual("Depth and image numbers match", DepthFiles.Num(), VideoAFiles.Num());
				bIsOK &= TestTrue("Depth images within limit", FilesWithinSizeLimitLambda(DepthFiles, DepthPath, HMCDepthLowerSizeLimit, HMCDepthUpeerSizeLimit));
			}

			// Even if some tests are failing, if the import took place we want to clean up the data
			FileManager.DeleteDirectory(*HMCOutputPath, false /*bRequireExists*/, true /*Tree*/);
		}
	}
	else if (InTestCommand == TEXT("CheckLLFArchiveImport"))
	{
		UMetaHumanCaptureSourceSync* SyncSource = NewObject<UMetaHumanCaptureSourceSync>(GetTransientPackage());

		SyncSource->CaptureSourceType = EMetaHumanCaptureSourceType::LiveLinkFaceArchives;
		FPropertyChangedEvent CaptureSourceChanged(UMetaHumanCaptureSourceSync::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UMetaHumanCaptureSourceSync, CaptureSourceType)));
		SyncSource->PostEditChangeProperty(CaptureSourceChanged);

		SyncSource->StoragePath = FDirectoryPath(InputDir / "IOS");
		FPropertyChangedEvent StoragePathChanged(UMetaHumanCaptureSourceSync::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UMetaHumanCaptureSourceSync, StoragePath)));
		SyncSource->PostEditChangeProperty(StoragePathChanged);

		FString IOSOutputPath = OutputDir / "IOS";
		if (SyncSource->CanStartup())
		{
			SyncSource->Startup();
			SyncSource->SetTargetPath(IOSOutputPath, "/Game/CaptureTest/LLFArchive_Ingested");

			TArray<FMetaHumanTakeInfo> AllTakes = SyncSource->Refresh();

			UTEST_NOT_EQUAL("Have valid takes for import", AllTakes.Num(), 0);
			TArray<FMetaHumanTake> ImportedTakes = SyncSource->GetTakes({ AllTakes[0].Id });

			UTEST_NOT_EQUAL("Have imported takes", ImportedTakes.Num(), 0);
			TArray<FMetaHumanTakeView> TakeViews = ImportedTakes[0].Views;
			SyncSource->Shutdown();

			bIsOK &= TestEqual("Number of views", TakeViews.Num(), 1);
			if (bIsOK)
			{
				FString VideoAPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectUserDir(), TakeViews[0].Video->GetSequencePath());
				TArray<FString> VideoAFiles;
				FileManager.FindFiles(VideoAFiles, *VideoAPath, TEXT("JPG"));

				FString DepthPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectUserDir(), TakeViews[0].Depth->GetSequencePath());
				TArray<FString> DepthFiles;
				FileManager.FindFiles(DepthFiles, *DepthPath, TEXT("EXR"));

				bIsOK &= TestNotEqual("Found image files", VideoAFiles.Num(), 0);
				bIsOK &= TestEqual("Depth and image numbers match", DepthFiles.Num(), VideoAFiles.Num());

				bIsOK &= TestTrue("Extracted images within limit", FilesWithinSizeLimitLambda(VideoAFiles, VideoAPath, IOSImageLowerSizeLimit, IOSImageUpperSizeLimit));
				bIsOK &= TestTrue("Depth images within limit", FilesWithinSizeLimitLambda(DepthFiles, DepthPath, IOSDepthLowerSizeLimit, IOSDepthUpeerSizeLimit));
			}

			// Even if some tests are failing, if the import took place we want to clean up the data
			FileManager.DeleteDirectory(*IOSOutputPath, false /*bRequireExists*/, true /*Tree*/);
		}
	}
	else
	{
		bIsOK = false;
	}

	return bIsOK;
}

#endif
