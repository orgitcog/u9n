// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanCameraCalibrationImporterFactory.h"

#include "CameraCalibration.h"
#include "LoadLiveLinkFaceCameraCalibration.h"

#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Editor.h"
#include "EditorFramework/AssetImportData.h"

#define LOCTEXT_NAMESPACE "MetaHuman Camera Calibration Importer"

UMetaHumanCameraCalibrationImporterFactory::UMetaHumanCameraCalibrationImporterFactory(const FObjectInitializer& InObjectInitializer)
	: Super{ InObjectInitializer }
{
	bCreateNew = false;
	bEditAfterNew = false;
	SupportedClass = UCameraCalibration::StaticClass();
	bEditorImport = true;
	bText = true;

	// Multiple formats can be added here to support different calibration formats
	Formats.Add(TEXT("mhaical;MetaHuman Camera Calibration"));
}

FText UMetaHumanCameraCalibrationImporterFactory::GetToolTip() const
{
	return LOCTEXT("MetaHumanCameraCalibrationImporterFactoryDescription", "Camera Calibration importer");
}

bool UMetaHumanCameraCalibrationImporterFactory::FactoryCanImport(const FString& InFileName)
{
	const FString Extension = FPaths::GetExtension(InFileName);
	return Extension.Compare(TEXT("mhaical"), ESearchCase::IgnoreCase) == 0;
}

UObject* UMetaHumanCameraCalibrationImporterFactory::FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags InFlags, const FString& InFileName, const TCHAR* InParams, FFeedbackContext* InWarn, bool& bOutOperationCanceled)
{
	UImportSubsystem* ImportSubsystem = GEditor->GetEditorSubsystem<UImportSubsystem>();
	ImportSubsystem->BroadcastAssetPreImport(this, InClass, InParent, InName, InParams);

	UCameraCalibration* CameraCalibration = LoadLiveLinkFaceCameraCalibration(InClass, InParent, InName, InFlags, InFileName);
	bOutOperationCanceled = false;

	if (CameraCalibration != nullptr &&
		CameraCalibration->AssetImportData != nullptr)
	{
		CameraCalibration->AssetImportData->Update(InFileName);
	}

	ImportSubsystem->BroadcastAssetPostImport(this, CameraCalibration);

	return CameraCalibration;
}

bool UMetaHumanCameraCalibrationImporterFactory::CanReimport(UObject* InObj, TArray<FString>& OutFilenames)
{
	if (UCameraCalibration* CameraCalibration = Cast<UCameraCalibration>(InObj))
	{
		if (CameraCalibration->AssetImportData != nullptr)
		{
			CameraCalibration->AssetImportData->ExtractFilenames(OutFilenames);
		}
		else
		{
			OutFilenames.Add(FString{});
		}

		return true;
	}
	else
	{
		return false;
	}
}

void UMetaHumanCameraCalibrationImporterFactory::SetReimportPaths(UObject* InObj, const TArray<FString>& InNewReimportPaths)
{
	if (InNewReimportPaths.IsEmpty())
	{
		return;
	}

	if (UCameraCalibration* CameraCalibration = Cast<UCameraCalibration>(InObj))
	{
		if (FactoryCanImport(InNewReimportPaths[0]))
		{
			CameraCalibration->AssetImportData->UpdateFilenameOnly(InNewReimportPaths[0]);
		}
	}
}

EReimportResult::Type UMetaHumanCameraCalibrationImporterFactory::Reimport(UObject* InObj)
{
	if (UCameraCalibration* CameraCalibration = Cast<UCameraCalibration>(InObj))
	{
		// Make sure the file is valid and exists
		const FString Filename = CameraCalibration->AssetImportData->GetFirstFilename();
		if (Filename.Len() == 0 || IFileManager::Get().FileSize(*Filename) == INDEX_NONE)
		{
			return EReimportResult::Failed;
		}

		bool bImportCancelled = false;
		if (ImportObject(CameraCalibration->GetClass(), CameraCalibration->GetOuter(), *CameraCalibration->GetName(), RF_Public | RF_Standalone, Filename, nullptr, bImportCancelled))
		{
			CameraCalibration->AssetImportData->Update(Filename);

			if (CameraCalibration->GetOuter())
			{
				CameraCalibration->MarkPackageDirty();
			}

			CameraCalibration->MarkPackageDirty();

			return EReimportResult::Succeeded;
		}
		else
		{
			if (bImportCancelled)
			{
				return EReimportResult::Cancelled;
			}
			else
			{
				return EReimportResult::Failed;
			}
		}
	}

	return EReimportResult::Failed;
}

TObjectPtr<UObject>* UMetaHumanCameraCalibrationImporterFactory::GetFactoryObject() const
{
	return &GCMark;
}

#undef LOCTEXT_NAMESPACE